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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H

#include <API/TRBase.h>
#include "Utilities.h"

#define FixedMake(n) ((n) * 0x10000)

TR_INTERNAL TRBoolean SFNTIsEnglishLanguage(TRUInt16 platformID, TRUInt16 languageID)
{
    switch (platformID) {
    case SFNTPlatformIDWindows:
        switch (languageID) {
        case SFNTWinLangIDEnAU:
        case SFNTWinLangIDEnBZ:
        case SFNTWinLangIDEnCA:
        case SFNTWinLangIDEnHK:
        case SFNTWinLangIDEnIN:
        case SFNTWinLangIDEnIE:
        case SFNTWinLangIDEnJM:
        case SFNTWinLangIDEnMY:
        case SFNTWinLangIDEnNZ:
        case SFNTWinLangIDEnPH:
        case SFNTWinLangIDEnSG:
        case SFNTWinLangIDEnZA:
        case SFNTWinLangIDEnTT:
        case SFNTWinLangIDEnAE:
        case SFNTWinLangIDEnGB:
        case SFNTWinLangIDEnUS:
        case SFNTWinLangIDEnZW:
            return TRTrue;
        }
        break;

    case SFNTPlatformIDMacintosh:
        /* Macintosh English is language 0 */
        return (languageID == 0);
    }

    return TRFalse;
}

TR_INTERNAL SFNTEncoding SFNTGetNameEncoding(TRUInt16 platformID, TRUInt16 encodingID)
{
    SFNTEncoding encoding = SFNTEncodingUnknown;

    switch (platformID) {
    case SFNTPlatformIDUnicode:
        encoding = SFNTEncodingUTF16BE;
        break;

    case SFNTPlatformIDMacintosh:
        /* The other Macintosh encodings are legacy scripts, which are not supported. */
        if (encodingID == 0) {
            encoding = SFNTEncodingMacRoman;
        }
        break;

    case SFNTPlatformIDWindows:
        switch (encodingID) {
        case 0:
        case 1:
        case 10:
            encoding = SFNTEncodingUTF16BE;
            break;

        default:
            /* All other Windows encodings are legacy codepages */
            break;
        }
    }

    return encoding;
}

TR_INTERNAL TRWeight GetWeightFromValue(TRUInt16 value)
{
    const TRWeight weights[] = {
        TRWeightThin, TRWeightExtraLight, TRWeightLight, TRWeightRegular, TRWeightMedium,
        TRWeightSemiBold, TRWeightBold, TRWeightExtraBold, TRWeightExtraHeavy
    };
    TRInteger index = (TRInteger)(((TRFloat)value / 100.0) - 0.5);

    /**
     * | wght       | mapped group |
     * | ---------- | ------------ |
     * | < 100      | thin         |
     * | [100, 150) | thin         |
     * | [150, 250) | extraLight   |
     * | [250, 350) | light        |
     * | [350, 450) | regular      |
     * | [450, 550) | medium       |
     * | [550, 650) | semiBold     |
     * | [650, 750) | bold         |
     * | [750, 850) | extraBold    |
     * | ≥ 850      | extraHeavy   |
     */
    if (index < 0) {
        index = 0;
    } else if (index > 8) {
        index = 8;
    }

    return weights[index];
}

TR_INTERNAL TRWeight GetWeightFromWGHTCoordinate(TRFloat coordinate)
{
    TRUInt16 value;

    if (coordinate < 1) {
        value = 1;
    } else if (coordinate > 1000) {
        value = 1000;
    } else {
        value = (TRUInt16)coordinate;
    }

    return GetWeightFromValue(value);
}

TR_INTERNAL TRWidth GetWidthFromValue(TRUInt16 value)
{
    const TRWidth widths[] = {
        TRWidthUltraCondensed, TRWidthExtraCondensed, TRWidthCondensed, TRWidthSemiCondensed,
        TRWidthNormal, TRWidthSemiExpanded, TRWidthExpanded, TRWidthExtraExpanded,
        TRWidthUltraExpanded
    };
    TRInteger index = (TRInteger)(value - 1);

    if (index < 0) {
        index = 0;
    } else if (index > 8) {
        index = 8;
    }

    return widths[index];
}

TR_INTERNAL TRWidth GetWidthFromWDTHCoordinate(TRFloat coordinate)
{
    TRUInt16 value;

    /**
     * | wdth         | mapped group    |
     * | ------------ | --------------- |
     * | < 50         | ultra condensed |
     * | [50, 62.5)   | ultra condensed |
     * | [62.5, 75)   | extra condensed |
     * | [75, 87.5)   | condensed       |
     * | [87.5, 100)  | semi condensed  |
     * | [100, 112.5) | normal          |
     * | [112.5, 125) | semi expanded   |
     * | [125, 150)   | expanded        |
     * | [150, 175)   | extra expanded  |
     * | [175, 200)   | ultra expanded  |
     * | ≥ 200        | ultra expanded  |
     */
    if (coordinate < 50) {
        value = 1;
    } else if (coordinate < 125) {
        value = (TRUInt16)(((coordinate - 50) / 12.5) + 1);
    } else if (coordinate < 200) {
        value = (TRUInt16)(((coordinate - 125) / 25) + 7);
    } else {
        value = 9;
    }

    return GetWidthFromValue(value);
}

TR_INTERNAL TRSlope GetSlopeFromITALCoordinate(TRFloat coordinate)
{
    return (coordinate >= 1.0 ? TRSlopeItalic : TRSlopePlain);
}

TR_INTERNAL TRSlope GetSlopeFromSLNTCoordinate(TRFloat coordinate)
{
    return (coordinate != 0.0 ? TRSlopeOblique : TRSlopePlain);
}

TR_INTERNAL TRBoolean SearchEnglishName(FT_Face ftFace, TRUInt16 nameID, NameString *nameString)
{
    TRUInteger candidate = TRInvalidIndex;
    TRBoolean nameFound = TRFalse;
    TRUInteger recordCount;
    TRUInteger index;

    recordCount = FT_Get_Sfnt_Name_Count(ftFace);

    for (index = 0; index < recordCount; index++) {
        FT_SfntName current;
        FT_Error error;

        error = FT_Get_Sfnt_Name(ftFace, index, &current);
        if (error != FT_Err_Ok) {
            continue;
        }

        if (current.name_id != nameID) {
            continue;
        }

        if (!SFNTIsEnglishLanguage(current.platform_id, current.language_id)) {
            continue;
        }

        /* Best match: Windows English US */
        if (current.platform_id == SFNTPlatformIDWindows &&
            current.language_id == SFNTWinLangIDEnUS)
        {
            candidate = index;
            break;
        }

        /* Secondary match: first English or Macintosh English */
        if (candidate == TRInvalidIndex || current.platform_id == SFNTPlatformIDMacintosh) {
            candidate = index;
        }
    }

    if (candidate != TRInvalidIndex) {
        FT_SfntName sfntName;

        if (FT_Get_Sfnt_Name(ftFace, candidate, &sfntName) == FT_Err_Ok) {
            nameString->bytes = sfntName.string;
            nameString->length = sfntName.string_len;
            nameString->encoding = SFNTGetNameEncoding(sfntName.platform_id, sfntName.encoding_id);

            nameFound = TRTrue;
        }
    }

    if (!nameFound) {
        nameString->encoding = SFNTEncodingUnknown;
        nameString->bytes = NULL;
        nameString->length = 0;
    }

    return nameFound;
}

TR_INTERNAL TRBoolean SearchFamilyName(FT_Face ftFace, const TT_OS2 *os2Table, NameString *nameString)
{
    TRBoolean nameFound = TRFalse;

    if (os2Table && (os2Table->fsSelection & FSSelectionWWS)) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDWWSFamily, nameString);
    }
    if (!nameFound) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDTypographicFamily, nameString);
    }
    if (!nameFound) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDFontFamily, nameString);
    }

    return nameFound;
}

TR_INTERNAL TRBoolean SearchSubfamilyName(FT_Face ftFace, const TT_OS2 *os2Table, NameString *nameString)
{
    TRBoolean nameFound = TRFalse;

    if (os2Table && (os2Table->fsSelection & FSSelectionWWS)) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDWWSSubfamily, nameString);
    }
    if (!nameFound) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDTypographicSubfamily, nameString);
    }
    if (!nameFound) {
        nameFound = SearchEnglishName(ftFace, SFNTNameIDFontSubfamily, nameString);
    }

    return nameFound;
}

TR_INTERNAL TRBoolean SearchFullName(FT_Face ftFace, NameString *nameString)
{
    return SearchEnglishName(ftFace, SFNTNameIDFullName, nameString);
}

/* The Unicode values of the bytes from 0x80 to 0xFF of Mac OS Roman. */
static const TRUInt16 MacRomanHighHalf[128] = {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
};

TR_INTERNAL TRUInteger NameStringGetCapacity(const NameString *nameString)
{
    /* A Mac Roman name has a code unit for each byte, while UTF-16 has a unit for two bytes. */
    return (nameString->encoding == SFNTEncodingMacRoman
            ? nameString->length * 2
            : nameString->length);
}

TR_INTERNAL TRBoolean NameStringToStringView(NameString *nameString, TRStringView *stringView)
{
    if (nameString) {
        Data bytes = nameString->bytes;
        TRStringEncoding encoding = nameString->encoding;

        if (bytes && encoding == SFNTEncodingUTF16BE) {
            TRUInteger length = nameString->length / 2;
            TRUInt16 *codeUnits = (TRUInt16 *)stringView->buffer;
            TRUInteger index;

            if (length > 0) {
                for (index = 0; index < length; index++) {
                    codeUnits[index] = Data_BigUInt16(bytes, index * 2);
                }

                stringView->length = length;
                stringView->encoding = TRStringEncodingUTF16;

                return TRTrue;
            }
        } else if (bytes && encoding == SFNTEncodingMacRoman) {
            TRUInteger length = nameString->length;
            TRUInt16 *codeUnits = (TRUInt16 *)stringView->buffer;
            TRUInteger index;

            if (length > 0) {
                for (index = 0; index < length; index++) {
                    TRUInt8 byte = bytes[index];
                    codeUnits[index] = (byte < 0x80 ? byte : MacRomanHighHalf[byte - 0x80]);
                }

                stringView->length = length;
                stringView->encoding = TRStringEncodingUTF16;

                return TRTrue;
            }
        }
    }

    return TRFalse;
}
