// Copyright © 2021 CCP ehf.

#include "StdAfx.h"
#include "BlueClipboard.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <vector>

/*
	Linux has no system clipboard API: the clipboard belongs to the display server (Wayland or X11)
	and talking to it directly would mean linking against a windowing library. Instead we use the
	standard command line clipboard tools, which every desktop distribution packages:

		Wayland: wl-copy / wl-paste   (package wl-clipboard)
		X11:     xclip or xsel

	Text is exchanged as UTF-8; the wide string overloads convert to and from UTF-32 wchar_t.
*/

namespace
{
// A clipboard owner that never answers must not hang the caller
constexpr int TOOL_TIMEOUT_MS = 2000;

// Exit status used by posix_spawn's child when the program could not be executed
constexpr int EXIT_NOT_FOUND = 127;

using Command = std::vector<const char*>;

enum class ToolResult
{
	Ok,         // Tool ran and succeeded
	Failed,     // Tool ran but reported an error, e.g. no text on the clipboard
	NotFound,   // Tool is not installed
};

std::vector<Command> GetPasteCommands()
{
	std::vector<Command> commands;
	if( std::getenv( "WAYLAND_DISPLAY" ) )
	{
		commands.push_back( { "wl-paste", "--no-newline", "--type", "text", nullptr } );
	}
	if( std::getenv( "DISPLAY" ) )
	{
		commands.push_back( { "xclip", "-selection", "clipboard", "-out", nullptr } );
		commands.push_back( { "xsel", "--clipboard", "--output", nullptr } );
	}
	return commands;
}

std::vector<Command> GetCopyCommands()
{
	std::vector<Command> commands;
	if( std::getenv( "WAYLAND_DISPLAY" ) )
	{
		commands.push_back( { "wl-copy", "--type", "text/plain;charset=utf-8", nullptr } );
	}
	if( std::getenv( "DISPLAY" ) )
	{
		commands.push_back( { "xclip", "-selection", "clipboard", "-in", nullptr } );
		commands.push_back( { "xsel", "--clipboard", "--input", nullptr } );
	}
	return commands;
}

ToolResult WaitForTool( pid_t pid, std::chrono::steady_clock::time_point deadline )
{
	int status = 0;
	while( true )
	{
		const pid_t result = waitpid( pid, &status, WNOHANG );
		if( result == pid )
		{
			break;
		}
		if( result < 0 && errno != EINTR )
		{
			return ToolResult::Failed;
		}
		if( std::chrono::steady_clock::now() >= deadline )
		{
			kill( pid, SIGKILL );
			waitpid( pid, &status, 0 );
			return ToolResult::Failed;
		}
		usleep( 1000 );
	}

	if( !WIFEXITED( status ) )
	{
		return ToolResult::Failed;
	}
	if( WEXITSTATUS( status ) == EXIT_NOT_FOUND )
	{
		return ToolResult::NotFound;
	}
	return WEXITSTATUS( status ) == 0 ? ToolResult::Ok : ToolResult::Failed;
}

/*
	Runs a clipboard tool, optionally writing `input` to its stdin and/or capturing its stdout.
	posix_spawn is used rather than fork so this is safe in a multi-threaded process.
*/
ToolResult RunTool( const Command& command, const std::string* input, std::string* output )
{
	int inPipe[2] = { -1, -1 };
	int outPipe[2] = { -1, -1 };
	if( ( input && pipe2( inPipe, O_CLOEXEC ) != 0 ) || ( output && pipe2( outPipe, O_CLOEXEC ) != 0 ) )
	{
		for( int fd : { inPipe[0], inPipe[1], outPipe[0], outPipe[1] } )
		{
			if( fd >= 0 )
			{
				close( fd );
			}
		}
		return ToolResult::Failed;
	}

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init( &actions );
	if( input )
	{
		posix_spawn_file_actions_adddup2( &actions, inPipe[0], STDIN_FILENO );
	}
	else
	{
		posix_spawn_file_actions_addopen( &actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0 );
	}
	if( output )
	{
		posix_spawn_file_actions_adddup2( &actions, outPipe[1], STDOUT_FILENO );
	}
	else
	{
		// wl-copy and xclip leave a background process serving the clipboard; it must not
		// inherit anything we wait on.
		posix_spawn_file_actions_addopen( &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0 );
	}
	posix_spawn_file_actions_addopen( &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0 );

	pid_t pid = 0;
	const int spawnResult = posix_spawnp( &pid, command[0], &actions, nullptr, const_cast<char* const*>( command.data() ), environ );
	posix_spawn_file_actions_destroy( &actions );

	if( input )
	{
		close( inPipe[0] );
	}
	if( output )
	{
		close( outPipe[1] );
	}

	if( spawnResult != 0 )
	{
		if( input )
		{
			close( inPipe[1] );
		}
		if( output )
		{
			close( outPipe[0] );
		}
		return spawnResult == ENOENT ? ToolResult::NotFound : ToolResult::Failed;
	}

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( TOOL_TIMEOUT_MS );

	if( input )
	{
		// Clipboard text is small enough that a blocking write cannot deadlock against the
		// tool, which reads all of its input before doing anything else.
		const char* data = input->data();
		size_t remaining = input->size();
		while( remaining > 0 )
		{
			const ssize_t written = write( inPipe[1], data, remaining );
			if( written < 0 )
			{
				if( errno == EINTR )
				{
					continue;
				}
				break;
			}
			data += written;
			remaining -= size_t( written );
		}
		close( inPipe[1] );
	}

	if( output )
	{
		char buffer[4096];
		while( true )
		{
			const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>( deadline - std::chrono::steady_clock::now() ).count();
			if( remainingMs <= 0 )
			{
				break;
			}
			pollfd pfd = { outPipe[0], POLLIN, 0 };
			const int ready = poll( &pfd, 1, int( remainingMs ) );
			if( ready < 0 && errno == EINTR )
			{
				continue;
			}
			if( ready <= 0 )
			{
				break;
			}
			const ssize_t bytes = read( outPipe[0], buffer, sizeof( buffer ) );
			if( bytes < 0 && errno == EINTR )
			{
				continue;
			}
			if( bytes <= 0 )
			{
				break;
			}
			output->append( buffer, size_t( bytes ) );
		}
		close( outPipe[0] );
	}

	return WaitForTool( pid, deadline );
}

BlueClipboard::OperationResult GetUtf8( std::string& data )
{
	auto commands = GetPasteCommands();
	for( const auto& command : commands )
	{
		std::string text;
		switch( RunTool( command, nullptr, &text ) )
		{
		case ToolResult::Ok:
			data = std::move( text );
			return BlueClipboard::CLIPBOARD_OK;
		case ToolResult::Failed:
			// The tool works but the clipboard holds nothing representable as text, matching
			// what Windows and macOS report for an empty or non-text clipboard.
			return BlueClipboard::CLIPBOARD_INCOMPATIBLE_FORMAT;
		case ToolResult::NotFound:
			break;
		}
	}
	return BlueClipboard::CLIPBOARD_FAILURE;
}

BlueClipboard::OperationResult SetUtf8( const std::string& data )
{
	auto commands = GetCopyCommands();
	for( const auto& command : commands )
	{
		switch( RunTool( command, &data, nullptr ) )
		{
		case ToolResult::Ok:
			return BlueClipboard::CLIPBOARD_OK;
		case ToolResult::Failed:
			return BlueClipboard::CLIPBOARD_FAILURE;
		case ToolResult::NotFound:
			break;
		}
	}
	return BlueClipboard::CLIPBOARD_FAILURE;
}
}

BlueClipboard::OperationResult BlueClipboard::GetData( std::string& data ) const
{
	return GetUtf8( data );
}

BlueClipboard::OperationResult BlueClipboard::GetData( std::wstring& data ) const
{
	std::string utf8;
	auto result = GetUtf8( utf8 );
	if( result == CLIPBOARD_OK )
	{
		// Widens byte by byte, so this is correct for ASCII only.
		// TODO: Use UTF8ToWide once carbon-core's Linux implementation no longer pads its result with NULs.
		data = std::wstring( utf8.begin(), utf8.end() );
	}
	return result;
}

BlueClipboard::OperationResult BlueClipboard::SetData( const std::string& data )
{
	return SetUtf8( data );
}

BlueClipboard::OperationResult BlueClipboard::SetData( const std::wstring& data )
{
	return SetUtf8( WideToUTF8( data ) );
}
