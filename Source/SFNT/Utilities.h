/*
 * Copyright (C) 2026 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _TEHREER__SFNT__UTILITIES_H
#define _TEHREER__SFNT__UTILITIES_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>
#include <API/TRBase.h>
#include <Core/Data.h>

enum {
    SFNTNameIDFontFamily           = 1,
    SFNTNameIDFontSubfamily        = 2,
    SFNTNameIDFullName             = 4,
    SFNTNameIDTypographicFamily    = 16,
    SFNTNameIDTypographicSubfamily = 17,
    SFNTNameIDWWSFamily            = 21,
    SFNTNameIDWWSSubfamily         = 22
};

enum {
    SFNTPlatformIDUnicode   = 0,
    SFNTPlatformIDMacintosh = 1,
    SFNTPlatformIDWindows   = 3
};

enum {
    /* Reference: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-lcid/70feba9f-294e-491e-b6eb-56532684c37f */
    SFNTWinLangIDEnAU = 0x0C09, /* en-AU */
    SFNTWinLangIDEnBZ = 0x2809, /* en-BZ */
    SFNTWinLangIDEnCA = 0x1009, /* en-CA */
    SFNTWinLangIDEnHK = 0x3C09, /* en-HK */
    SFNTWinLangIDEnIN = 0x4009, /* en-IN */
    SFNTWinLangIDEnIE = 0x1809, /* en-IE */
    SFNTWinLangIDEnJM = 0x2009, /* en-JM */
    SFNTWinLangIDEnMY = 0x4409, /* en-MY */
    SFNTWinLangIDEnNZ = 0x1409, /* en-NZ */
    SFNTWinLangIDEnPH = 0x3409, /* en-PH */
    SFNTWinLangIDEnSG = 0x4809, /* en-SG */
    SFNTWinLangIDEnZA = 0x1C09, /* en-ZA */
    SFNTWinLangIDEnTT = 0x2C09, /* en-TT */
    SFNTWinLangIDEnAE = 0x4C09, /* en-AE */
    SFNTWinLangIDEnGB = 0x0809, /* en-GB */
    SFNTWinLangIDEnUS = 0x0409, /* en-US */
    SFNTWinLangIDEnZW = 0x3009  /* en-ZW */
};

enum {
    SFNTEncodingUnknown = 0,
    SFNTEncodingUTF16BE = 1
};
typedef TRUInt16 SFNTEncoding;

enum MacStyle {
    MacStyleBold      = 1 << 0,
    MacStyleItalic    = 1 << 1,
    MacStyleCondensed = 1 << 5,
    MacStyleExtended  = 1 << 6
};

enum {
    FSSelectionItalic  = 1 << 0,
    FSSelectionWWS     = 1 << 8,
    FSSelectionOblique = 1 << 9
};

typedef struct _NameString {
    Data bytes;
    TRUInteger length;
    SFNTEncoding encoding;
} NameString;

TR_INTERNAL TRBoolean SFNTIsEnglishLanguage(TRUInt16 platformID, TRUInt16 languageID);
TR_INTERNAL SFNTEncoding SFNTGetNameEncoding(TRUInt16 platformID, TRUInt16 encodingID);

TR_INTERNAL TRWeight GetWeightFromValue(TRUInt16 value);
TR_INTERNAL TRWeight GetWeightFromWGHTCoordinate(TRFloat coordinate);

TR_INTERNAL TRWidth GetWidthFromValue(TRUInt16 value);
TR_INTERNAL TRWidth GetWidthFromWDTHCoordinate(TRFloat coordinate);

TR_INTERNAL TRSlope GetSlopeFromITALCoordinate(TRFloat coordinate);
TR_INTERNAL TRSlope GetSlopeFromSLNTCoordinate(TRFloat coordinate);

TR_INTERNAL TRBoolean SearchEnglishName(FT_Face ftFace, TRUInt16 nameID, NameString *nameString);
TR_INTERNAL TRBoolean SearchFamilyName(FT_Face ftFace, const TT_OS2 *os2Table, NameString *nameString);
TR_INTERNAL TRBoolean SearchSubfamilyName(FT_Face ftFace, const TT_OS2 *os2Table, NameString *nameString);
TR_INTERNAL TRBoolean SearchFullName(FT_Face ftFace, NameString *nameString);

TR_INTERNAL TRBoolean NameStringToStringView(NameString *nameString, TRStringView *stringView);

#endif
