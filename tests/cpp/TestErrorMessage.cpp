// Copyright © 2026 CCP ehf.

#if __linux__

#include <errormessage.h>

#include <cstdlib>
#include <map>
#include <string>

namespace
{
const char* const LOCALE_VARIABLES[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };

// Clears the locale environment for each test and restores it afterwards
class LinuxLanguageCodeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		for( const char* name : LOCALE_VARIABLES )
		{
			if( const char* value = std::getenv( name ) )
			{
				m_saved[name] = value;
			}
			unsetenv( name );
		}
	}

	void TearDown() override
	{
		for( const char* name : LOCALE_VARIABLES )
		{
			auto saved = m_saved.find( name );
			if( saved != m_saved.end() )
			{
				setenv( name, saved->second.c_str(), 1 );
			}
			else
			{
				unsetenv( name );
			}
		}
	}

private:
	std::map<std::string, std::string> m_saved;
};
}

TEST_F( LinuxLanguageCodeTest, DefaultsToEnglishWithoutLocale )
{
	EXPECT_EQ( GetOSLanguageCode(), "en" );
}

TEST_F( LinuxLanguageCodeTest, ReadsLang )
{
	setenv( "LANG", "fr_FR.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "fr" );
}

TEST_F( LinuxLanguageCodeTest, LcAllOverridesLang )
{
	setenv( "LANG", "fr_FR.UTF-8", 1 );
	setenv( "LC_ALL", "de_DE.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "de" );
}

TEST_F( LinuxLanguageCodeTest, LcMessagesOverridesLang )
{
	setenv( "LANG", "en_US.UTF-8", 1 );
	setenv( "LC_MESSAGES", "ja_JP.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "ja" );
}

TEST_F( LinuxLanguageCodeTest, LanguageListUsesFirstSupportedEntry )
{
	setenv( "LANGUAGE", "is:ru:en", 1 );
	setenv( "LANG", "de_DE.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "ru" );
}

TEST_F( LinuxLanguageCodeTest, HandlesModifiersAndCase )
{
	setenv( "LANG", "ES_es@euro", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "es" );
}

TEST_F( LinuxLanguageCodeTest, CLocaleIsEnglish )
{
	setenv( "LC_ALL", "C", 1 );
	setenv( "LANG", "ko_KR.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "en" );
}

TEST_F( LinuxLanguageCodeTest, UnsupportedLanguageIsEnglish )
{
	setenv( "LANG", "is_IS.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "en" );
}

TEST_F( LinuxLanguageCodeTest, EmptyVariableIsIgnored )
{
	setenv( "LC_ALL", "", 1 );
	setenv( "LANG", "zh_CN.UTF-8", 1 );
	EXPECT_EQ( GetOSLanguageCode(), "zh" );
}

TEST( LinuxErrorMessageEncodingTest, CodePagesMatchOtherPlatforms )
{
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "en" ), "CP1252" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "fr" ), "CP1252" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "ja" ), "CP932" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "ko" ), "CP949" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "ru" ), "CP866" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "zh" ), "CP936" );
	EXPECT_STREQ( GetIconvCodePageForLanguageCode( "xx" ), "CP1252" );
}

TEST( LinuxErrorMessageEncodingTest, ConvertsWindows1252ToUtf8 )
{
	// "höher" as stored in the German string table
	EXPECT_EQ( ConvertCodePageToUtf8( "h\xf6her", "CP1252" ), "h\xc3\xb6her" );
}

TEST( LinuxErrorMessageEncodingTest, ConvertsCp866ToUtf8 )
{
	// Cyrillic capital "Т" (0x92 in CP866) is U+0422
	EXPECT_EQ( ConvertCodePageToUtf8( "\x92", "CP866" ), "\xd0\xa2" );
}

TEST( LinuxErrorMessageEncodingTest, AsciiIsUnchanged )
{
	EXPECT_EQ( ConvertCodePageToUtf8( "Verification Failure", "CP932" ), "Verification Failure" );
}

TEST( LinuxErrorMessageEncodingTest, UnknownCodePageReturnsInput )
{
	EXPECT_EQ( ConvertCodePageToUtf8( "h\xf6her", "NOT-A-CODEPAGE" ), "h\xf6her" );
}

TEST( LinuxErrorMessageEncodingTest, NullTextIsEmpty )
{
	EXPECT_EQ( ConvertCodePageToUtf8( nullptr, "CP1252" ), "" );
}

#endif
