// Copyright © 2026 CCP ehf.

#if !_WIN32

#include <BlueUtf8Posix.h>

TEST( BlueUtf8ToWideTest, AsciiHasNoTrailingCharacters )
{
	const std::wstring result = BlueUtf8ToWide( std::string( "/home/user/.local/share" ) );
	EXPECT_EQ( result, L"/home/user/.local/share" );
	EXPECT_EQ( result.size(), 23u );
}

TEST( BlueUtf8ToWideTest, EmptyString )
{
	EXPECT_EQ( BlueUtf8ToWide( std::string() ), L"" );
	EXPECT_EQ( BlueUtf8ToWide( static_cast<const char*>( nullptr ) ), L"" );
}

TEST( BlueUtf8ToWideTest, MultiByteCharacters )
{
	// "þæð 日本" followed by U+1F680 (4 byte UTF-8, a single UTF-32 wchar_t)
	const std::wstring result = BlueUtf8ToWide( std::string( "\xc3\xbe\xc3\xa6\xc3\xb0 \xe6\x97\xa5\xe6\x9c\xac\xf0\x9f\x9a\x80" ) );
	EXPECT_EQ( result, std::wstring( L"þæð 日本" ) + wchar_t( 0x1F680 ) );
}

TEST( BlueUtf8ToWideTest, InvalidSequenceIsReplaced )
{
	EXPECT_EQ( BlueUtf8ToWide( std::string( "a\xff" "b" ) ), L"a\xFFFD" L"b" );
}

TEST( BlueUtf8ToWideTest, TruncatedSequenceIsReplaced )
{
	EXPECT_EQ( BlueUtf8ToWide( std::string( "ab\xe6\x97" ) ), L"ab\xFFFD\xFFFD" );
}

TEST( BlueUtf8ToWideTest, RoundTripsThroughWideToUTF8 )
{
	const std::string original = "Clipboard \xc3\xbe \xe6\x97\xa5\xe6\x9c\xac";
	EXPECT_EQ( WideToUTF8( BlueUtf8ToWide( original ) ), original );
}

#endif
