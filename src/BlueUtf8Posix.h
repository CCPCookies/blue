// Copyright © 2026 CCP ehf.

#pragma once

#include <string>

#if !_WIN32
#include <iconv.h>
#endif

/*
	Decodes UTF-8 into a wide string (UTF-16 on Windows, UTF-32 elsewhere).

	On POSIX carbon-core's UTF8ToWide is not used: it sizes its result in bytes rather than wchar_t
	elements and never trims it, so every result carries trailing NUL characters (4x the length of
	the input). Invalid sequences are replaced with U+FFFD rather than failing the whole conversion.
*/
inline std::wstring BlueUtf8ToWide( const std::string& utf8 )
{
#if _WIN32
	return UTF8ToWide( utf8 );
#else
	std::wstring result;
	iconv_t converter = iconv_open( "WCHAR_T", "UTF-8" );
	if( converter == reinterpret_cast<iconv_t>( -1 ) )
	{
		return std::wstring( utf8.begin(), utf8.end() );
	}

	std::string input = utf8;
	char* in = input.data();
	size_t inLeft = input.size();

	// Every UTF-8 byte produces at most one wchar_t
	result.resize( input.size() );
	char* out = reinterpret_cast<char*>( result.data() );
	size_t outLeft = result.size() * sizeof( wchar_t );

	while( inLeft > 0 )
	{
		if( iconv( converter, &in, &inLeft, &out, &outLeft ) != static_cast<size_t>( -1 ) )
		{
			break;
		}
		if( outLeft < sizeof( wchar_t ) )
		{
			break; // Cannot happen given the sizing above, but never write past the buffer
		}
		// Invalid or truncated sequence: emit a replacement character and skip one byte
		*reinterpret_cast<wchar_t*>( out ) = L'\xFFFD';
		out += sizeof( wchar_t );
		outLeft -= sizeof( wchar_t );
		++in;
		--inLeft;
	}
	iconv_close( converter );

	result.resize( ( result.size() * sizeof( wchar_t ) - outLeft ) / sizeof( wchar_t ) );
	return result;
#endif
}

inline std::wstring BlueUtf8ToWide( const char* utf8 )
{
	return BlueUtf8ToWide( std::string( utf8 ? utf8 : "" ) );
}
