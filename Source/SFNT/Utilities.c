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
        /* These are legacy single byte encodings, not supported */
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

TR_INTERNAL TRBoolean NameStringToStringView(NameString *nameString, TRStringView *stringView)
{
    if (nameString) {
        Data bytes = nameString->bytes;
        TRStringEncoding encoding = nameString->encoding;
        TRUInteger length = nameString->length / 2;

        if (bytes && encoding == SFNTEncodingUTF16BE && length > 0) {
            TRUInt16 *codeUnits = (TRUInt16 *)stringView->buffer;
            TRUInteger index;

            for (index = 0; index < length; index++) {
                codeUnits[index] = Data_BigUInt16(bytes, index * 2);
            }

            stringView->length = length;
            stringView->encoding = TRStringEncodingUTF16;

            return TRTrue;
        }
    }

    return TRFalse;
}
