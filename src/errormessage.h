// Copyright © 2021 CCP ehf.

#ifndef _ERRORMESSAGE_H_
#define _ERRORMESSAGE_H_


#include <string>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define IDS_VERIFYFAIL_M 106
#define IDS_VERIFYFAIL_C 107
#define IDS_INVALIDWINDOWS 108
#define IDS_VERIFYFAIL_M1 109
#define IDS_VERIFYFAIL_NOTFOUND 110
#define IDS_VERIFYFAIL_UNKNOWNFOUND 111
#define IDS_VERIFYFAIL_INCORRECTCRC 112
#define IDS_INVALIDMACOS 113
#define IDS_INVALIDOS 114
#define IDS_INVALIDLINUX 115

// Exposed so invalid os dialogs can be requested externally to blue
extern "C" BLUEIMPORT void BlueShowInvalidOSVersionError();

// Translate an error message to an appropriate language for the user - returns the original string on failure.
std::string TranslateErrorMessage( const std::string& original, unsigned messageID );

void DisplayErrorMessageBox( const char* title, const char* message );

// Language code ("en", "de", ...) derived from the user's OS settings.
std::string GetOSLanguageCode();

#if __linux__
// Shows an error dialog for UTF-8 text using zenity or kdialog when a desktop session is
// available. Always logs the message and writes it to stderr.
void DisplayUtf8ErrorMessageBox( const char* title, const char* message );

// iconv name of the code page the string table for the given language code is stored in.
const char* GetIconvCodePageForLanguageCode( const std::string& languageCode );

// Converts text in the given iconv code page to UTF-8. Returns the input unchanged on failure.
std::string ConvertCodePageToUtf8( const char* text, const char* codePage );
#endif

#endif // _ERRORMESSAGE_H_
