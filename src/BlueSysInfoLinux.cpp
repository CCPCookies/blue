// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueSysInfo.h"
#include "BlueUtf8Posix.h"
#include "pdm.h"

#include <sstream>
#include <fstream>
#include <filesystem>

#include <netdb.h>
#include <unistd.h>

namespace
{
const int64_t s_startTime = TimeNow();

std::string GetHomeDirectory()
{
	const char* home = std::getenv( "HOME" );
	return home ? std::string( home ) : std::string();
}

/*
	Looks up XDG_DOCUMENTS_DIR. It is normally not an environment variable but an entry in
	$XDG_CONFIG_HOME/user-dirs.dirs (written by xdg-user-dirs), e.g.
		XDG_DOCUMENTS_DIR="$HOME/Documents"
	An environment variable of the same name takes precedence. Returns an empty string if not set.
*/
std::string GetXdgDocumentsDirectory( const std::string& home )
{
	if( const char* fromEnvironment = std::getenv( "XDG_DOCUMENTS_DIR" ) )
	{
		return fromEnvironment;
	}

	std::string configHome;
	if( const char* xdgConfigHome = std::getenv( "XDG_CONFIG_HOME" ) )
	{
		configHome = xdgConfigHome;
	}
	else if( !home.empty() )
	{
		configHome = home + "/.config";
	}
	else
	{
		return {};
	}

	std::ifstream userDirs( configHome + "/user-dirs.dirs" );
	std::string line;
	const std::string key = "XDG_DOCUMENTS_DIR=";
	while( std::getline( userDirs, line ) )
	{
		if( line.compare( 0, key.size(), key ) != 0 )
		{
			continue;
		}

		std::string value = line.substr( key.size() );
		if( value.size() >= 2 && value.front() == '"' && value.back() == '"' )
		{
			value = value.substr( 1, value.size() - 2 );
		}

		const std::string homeToken = "$HOME";
		if( value.compare( 0, homeToken.size(), homeToken ) == 0 )
		{
			value = home + value.substr( homeToken.size() );
		}
		return value;
	}
	return {};
}

/*
	Returns the host name as reported by the OS, which may or may not be fully qualified.
*/
std::string GetHostName()
{
	char buffer[HOST_NAME_MAX + 1] = {};
	if( gethostname( buffer, sizeof( buffer ) - 1 ) != 0 )
	{
		return {};
	}
	return buffer;
}

/*
	Returns the fully qualified host name if one can be determined, otherwise the plain host name.
*/
std::string GetFullyQualifiedHostName()
{
	std::string host = GetHostName();
	if( host.empty() || host.find( '.' ) != std::string::npos )
	{
		return host;
	}

	addrinfo hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_CANONNAME;
	addrinfo* info = nullptr;
	if( getaddrinfo( host.c_str(), nullptr, &hints, &info ) == 0 && info )
	{
		if( info->ai_canonname )
		{
			host = info->ai_canonname;
		}
		freeaddrinfo( info );
	}
	return host;
}

/*
	Reads an integer field such as "cpu family" from /proc/cpuinfo. Not every architecture
	reports every field (ARM has no "cpu family"), so missing fields return the fallback.
*/
int ReadCpuInfoField( const std::string& field, int fallback )
{
	std::ifstream cpuInfo( "/proc/cpuinfo" );
	std::string line;
	while( std::getline( cpuInfo, line ) )
	{
		auto colon = line.find( ':' );
		if( colon == std::string::npos )
		{
			continue;
		}
		std::string key = line.substr( 0, colon );
		key.erase( key.find_last_not_of( " \t" ) + 1 );
		if( key == field )
		{
			try
			{
				return std::stoi( line.substr( colon + 1 ) );
			}
			catch( const std::exception& )
			{
				return fallback;
			}
		}
	}
	return fallback;
}
}

std::wstring BlueSysInfo::GetUserDocumentsDirectory() const
{
	const std::string home = GetHomeDirectory();

	std::string path = GetXdgDocumentsDirectory( home );
	if( path.empty() && !home.empty() )
	{
		path = home + "/Documents";
	}

	std::error_code error;
	if( !path.empty() && std::filesystem::is_directory( path, error ) )
	{
		return BlueUtf8ToWide( path );
	}

	// Desktop-less systems (servers, containers, CI) often have no Documents folder. Callers expect an
	// existing, writable directory, so fall back to the home directory as xdg-user-dirs itself does.
	return BlueUtf8ToWide( home );
}

std::wstring BlueSysInfo::GetSharedApplicationDataDirectory() const
{
	const char* xdgDataHome = std::getenv( "XDG_DATA_HOME" );

	std::string sharedApplicationDataPath;

	if( xdgDataHome )
	{
		sharedApplicationDataPath = xdgDataHome;
	}
	else
	{
		// Fallback to ~/.local/share if XDG not found
		const char* home = std::getenv( "HOME" );
		if( !home )
		{
			return L"";
		}
		sharedApplicationDataPath = std::string( home ) + "/.local/share";
	}
	return BlueUtf8ToWide( sharedApplicationDataPath ); // Byte-wise widening would mangle non-ASCII paths
}

std::wstring BlueSysInfo::GetUserApplicationDataDirectory() const
{
	return GetSharedApplicationDataDirectory();
}

std::wstring BlueSysInfo::GetSharedFontsDirectory() const
{
	return L"/usr/share/fonts";
}

std::wstring BlueSysInfo::GetSystemFontsDirectory() const
{
	return GetSharedFontsDirectory();
}

uint32_t BlueSysInfo::GetProcessBitCount() const
{
	return CcpGetProcessBitCount();
}

uint32_t BlueSysInfo::GetSystemBitCount() const
{
	return 64;
}

uint64_t BlueSysInfo::GetProcessStartTime() const
{
	return s_startTime;
}

std::string BlueSysInfo::GetMachineUuid() const
{
	std::ifstream machineId( "/etc/machine-id" );

	if( !machineId.is_open() )
	{
		return "";
	}

	std::string machineUuid = "";

	std::getline( machineId, machineUuid );

	machineId.close();

	return machineUuid;
}

std::wstring BlueSysInfo::GetMachineName() const
{
	// Short host name, matching GetComputerNameW on Windows and the macOS implementation.
	std::string host = GetHostName();
	host = host.substr( 0, host.find( '.' ) );
	return BlueUtf8ToWide( host );
}

std::wstring BlueSysInfo::GetDomainName() const
{
	// First label of the DNS domain (e.g. "corp" for "machine.corp.example.com"), matching
	// DnsHostnameToComputerNameW on Windows and the macOS implementation. Empty if the host
	// has no DNS domain.
	const std::string host = GetFullyQualifiedHostName();
	const auto firstDot = host.find( '.' );
	if( firstDot == std::string::npos )
	{
		return {};
	}
	const std::string domain = host.substr( firstDot + 1 );
	return BlueUtf8ToWide( domain.substr( 0, domain.find( '.' ) ) );
}

BlueSysInfoCpu::BlueSysInfoCpu() :
	m_extensions( PDM::GetCPUInfo().extensions )
{
	auto pdmCpu = PDM::GetCPUInfo();
	m_mHz = pdmCpu.frequency;
	m_brand = pdmCpu.brand;
	m_logicalCpuCount = pdmCpu.logicalCoreCount;
	m_bitCount = static_cast<uint32_t>( pdmCpu.bitness );
	// PDM does not report the CPU family, so read it from the kernel (x86 only; 0 elsewhere).
	m_family = static_cast<uint32_t>( ReadCpuInfoField( "cpu family", 0 ) );

	std::string vendor = pdmCpu.vendor;
	int model = pdmCpu.model;
	int stepping = pdmCpu.stepping;
	m_revision = static_cast<uint32_t>( ( model << 8 ) | stepping ); // Same encoding as macOS

	switch( pdmCpu.architecture )
	{
	case PDM::CPUArchitecture::UNKNOWN: {
		m_architecture = "UNKNOWN";
		break;
	}

	case PDM::CPUArchitecture::X86: {
		m_architecture = "x86";
		break;
	}

	case PDM::CPUArchitecture::X86_64: {
		m_architecture = "Intel64";
		break;
	}

	case PDM::CPUArchitecture::ARM: {
		m_architecture = "ARM";
		break;
	}

	case PDM::CPUArchitecture::ARM64: {
		m_architecture = "ARM64";
		break;
	}

	default:
		m_architecture = "UNKNOWN";
	}

	std::stringstream identifierStream;

	identifierStream
		<< m_architecture
		<< " Family" << m_family
		<< " Model" << model
		<< " Stepping" << stepping
		<< " , " << vendor;

	m_identifier = identifierStream.str();
}


BlueSysInfoOs::BlueSysInfoOs()
{
	m_platform = LINUX;
	m_suite = DESKTOP;

	std::ifstream osReleaseInfoFile( "/etc/os-release" );

	if( !osReleaseInfoFile.is_open() )
	{
		return;
	}

	std::string lineEntry;
	while( std::getline( osReleaseInfoFile, lineEntry ) )
	{
		auto position = lineEntry.find( "=" );
		auto key = lineEntry.substr( 0, position );
		auto value = lineEntry.substr( position + 1 );

		if( key == "VERSION" )
		{
			auto semanticVersionSize = value.find( " " );
			std::string semanticVersionString = value.substr( 1, semanticVersionSize - 1 ); // Offset by 1 to ignore leading "

			std::stringstream ss;

			ss << semanticVersionString;

			std::vector<std::string> version;

			std::string token;
			while( std::getline( ss, token, '.' ) )
			{
				version.push_back( token );
			}

			if( version.empty() )
			{
				// Failed to retrieve version info from file
				osReleaseInfoFile.close();

				return;
			}

			m_majorVersion = atoi( version.at( 0 ).c_str() );
			m_minorVersion = version.size() > 1 ? atoi( version.at( 1 ).c_str() ) : 0;
			m_buildNumber = version.size() > 2 ? atoi( version.at( 2 ).c_str() ) : 0;

			osReleaseInfoFile.close();

			return;
		}
	}

	osReleaseInfoFile.close();
}

BlueSysInfoMemory::BlueSysInfoMemory()
{
	CcpProcessMemoryInfo info;
	if( CcpGetProcessMemoryInfo( info ) )
	{
		m_workingSet = uint64_t( info.workingSetSize );
		m_pageFile = uint64_t( info.pageFileUsage );
	}

	m_totalPhysical = 0;
	m_availablePhysical = 0;

	std::ifstream memInfoFile( "/proc/meminfo" );

	if( !memInfoFile.is_open() )
	{
		return;
	}

	std::string lineEntry;
	while( std::getline( memInfoFile, lineEntry ) )
	{
		std::stringstream ss;
		ss << lineEntry;

		std::string entryId;
		ss >> entryId;

		if( entryId == "MemTotal:" )
		{
			uint64_t totalPhysicalKb;
			ss >> totalPhysicalKb;
			m_totalPhysical = totalPhysicalKb * 1024;
		}
		else if( entryId == "MemAvailable:" )
		{
			uint64_t availablePhysicalKb;
			ss >> availablePhysicalKb;
			m_availablePhysical = availablePhysicalKb * 1024;
		}
	}

	memInfoFile.close();
}
