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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <SFNT/Utilities.h>
}

#include "TestFace.h"

#include "SFNTUtilitiesTests.h"

using namespace std;
using namespace Tehreer;

void SFNTUtilitiesTests::run() {
    testIsEnglishLanguage();
    testGetNameEncoding();
    testWeightFromValue();
    testWeightFromWGHTCoordinate();
    testWidthFromValue();
    testWidthFromWDTHCoordinate();
    testSlopeFromCoordinates();
    testNameStringToStringView();
    testNameStringToStringViewOddLength();
    testNameStringToStringViewInvalid();
    testMacRomanNames();
    testSearchEnglishName();
    testSearchEnglishNameMissing();
    testSearchEnglishNameMacintosh();
    testSearchFamilyAndSubfamilyName();
    testSearchFullName();
}

static NameString makeNameString(const vector<uint8_t> &bytes, SFNTEncoding encoding) {
    return NameString{ bytes.data(), bytes.size(), encoding };
}

void SFNTUtilitiesTests::testIsEnglishLanguage() {
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDWindows, SFNTWinLangIDEnUS) == TRTrue);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDWindows, SFNTWinLangIDEnGB) == TRTrue);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDWindows, SFNTWinLangIDEnZW) == TRTrue);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDWindows, 0x040C) == TRFalse);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDMacintosh, 0) == TRTrue);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDMacintosh, 1) == TRFalse);
    assert(SFNTIsEnglishLanguage(SFNTPlatformIDUnicode, 0) == TRFalse);
    assert(SFNTIsEnglishLanguage(2, 0) == TRFalse);
}

void SFNTUtilitiesTests::testGetNameEncoding() {
    assert(SFNTGetNameEncoding(SFNTPlatformIDUnicode, 3) == SFNTEncodingUTF16BE);
    assert(SFNTGetNameEncoding(SFNTPlatformIDWindows, 0) == SFNTEncodingUTF16BE);
    assert(SFNTGetNameEncoding(SFNTPlatformIDWindows, 1) == SFNTEncodingUTF16BE);
    assert(SFNTGetNameEncoding(SFNTPlatformIDWindows, 10) == SFNTEncodingUTF16BE);
    assert(SFNTGetNameEncoding(SFNTPlatformIDWindows, 2) == SFNTEncodingUnknown);
    assert(SFNTGetNameEncoding(SFNTPlatformIDMacintosh, 0) == SFNTEncodingMacRoman);
    assert(SFNTGetNameEncoding(SFNTPlatformIDMacintosh, 1) == SFNTEncodingUnknown);
    assert(SFNTGetNameEncoding(2, 0) == SFNTEncodingUnknown);
}

void SFNTUtilitiesTests::testWeightFromValue() {
    assert(GetWeightFromValue(0) == TRWeightThin);
    assert(GetWeightFromValue(100) == TRWeightThin);
    assert(GetWeightFromValue(149) == TRWeightThin);
    assert(GetWeightFromValue(150) == TRWeightExtraLight);
    assert(GetWeightFromValue(249) == TRWeightExtraLight);
    assert(GetWeightFromValue(250) == TRWeightLight);
    assert(GetWeightFromValue(349) == TRWeightLight);
    assert(GetWeightFromValue(350) == TRWeightRegular);
    assert(GetWeightFromValue(449) == TRWeightRegular);
    assert(GetWeightFromValue(450) == TRWeightMedium);
    assert(GetWeightFromValue(549) == TRWeightMedium);
    assert(GetWeightFromValue(550) == TRWeightSemiBold);
    assert(GetWeightFromValue(649) == TRWeightSemiBold);
    assert(GetWeightFromValue(650) == TRWeightBold);
    assert(GetWeightFromValue(749) == TRWeightBold);
    assert(GetWeightFromValue(750) == TRWeightExtraBold);
    assert(GetWeightFromValue(849) == TRWeightExtraBold);
    assert(GetWeightFromValue(850) == TRWeightExtraHeavy);
    assert(GetWeightFromValue(1000) == TRWeightExtraHeavy);
    assert(GetWeightFromValue(65535) == TRWeightExtraHeavy);
}

void SFNTUtilitiesTests::testWeightFromWGHTCoordinate() {
    assert(GetWeightFromWGHTCoordinate(-10.0f) == TRWeightThin);
    assert(GetWeightFromWGHTCoordinate(0.0f) == TRWeightThin);
    assert(GetWeightFromWGHTCoordinate(100.0f) == TRWeightThin);
    assert(GetWeightFromWGHTCoordinate(349.9f) == TRWeightLight);
    assert(GetWeightFromWGHTCoordinate(350.0f) == TRWeightRegular);
    assert(GetWeightFromWGHTCoordinate(400.0f) == TRWeightRegular);
    assert(GetWeightFromWGHTCoordinate(700.0f) == TRWeightBold);
    assert(GetWeightFromWGHTCoordinate(1000.0f) == TRWeightExtraHeavy);
    assert(GetWeightFromWGHTCoordinate(5000.0f) == TRWeightExtraHeavy);
}

void SFNTUtilitiesTests::testWidthFromValue() {
    assert(GetWidthFromValue(0) == TRWidthUltraCondensed);
    assert(GetWidthFromValue(1) == TRWidthUltraCondensed);
    assert(GetWidthFromValue(2) == TRWidthExtraCondensed);
    assert(GetWidthFromValue(3) == TRWidthCondensed);
    assert(GetWidthFromValue(4) == TRWidthSemiCondensed);
    assert(GetWidthFromValue(5) == TRWidthNormal);
    assert(GetWidthFromValue(6) == TRWidthSemiExpanded);
    assert(GetWidthFromValue(7) == TRWidthExpanded);
    assert(GetWidthFromValue(8) == TRWidthExtraExpanded);
    assert(GetWidthFromValue(9) == TRWidthUltraExpanded);
    assert(GetWidthFromValue(10) == TRWidthUltraExpanded);
    assert(GetWidthFromValue(65535) == TRWidthUltraExpanded);
}

void SFNTUtilitiesTests::testWidthFromWDTHCoordinate() {
    assert(GetWidthFromWDTHCoordinate(0.0f) == TRWidthUltraCondensed);
    assert(GetWidthFromWDTHCoordinate(49.9f) == TRWidthUltraCondensed);
    assert(GetWidthFromWDTHCoordinate(50.0f) == TRWidthUltraCondensed);
    assert(GetWidthFromWDTHCoordinate(62.5f) == TRWidthExtraCondensed);
    assert(GetWidthFromWDTHCoordinate(75.0f) == TRWidthCondensed);
    assert(GetWidthFromWDTHCoordinate(87.5f) == TRWidthSemiCondensed);
    assert(GetWidthFromWDTHCoordinate(99.9f) == TRWidthSemiCondensed);
    assert(GetWidthFromWDTHCoordinate(100.0f) == TRWidthNormal);
    assert(GetWidthFromWDTHCoordinate(112.5f) == TRWidthSemiExpanded);
    assert(GetWidthFromWDTHCoordinate(124.9f) == TRWidthSemiExpanded);
    assert(GetWidthFromWDTHCoordinate(125.0f) == TRWidthExpanded);
    assert(GetWidthFromWDTHCoordinate(149.9f) == TRWidthExpanded);
    assert(GetWidthFromWDTHCoordinate(150.0f) == TRWidthExtraExpanded);
    assert(GetWidthFromWDTHCoordinate(175.0f) == TRWidthUltraExpanded);
    assert(GetWidthFromWDTHCoordinate(200.0f) == TRWidthUltraExpanded);
    assert(GetWidthFromWDTHCoordinate(1000.0f) == TRWidthUltraExpanded);
}

void SFNTUtilitiesTests::testSlopeFromCoordinates() {
    assert(GetSlopeFromITALCoordinate(0.0f) == TRSlopePlain);
    assert(GetSlopeFromITALCoordinate(0.99f) == TRSlopePlain);
    assert(GetSlopeFromITALCoordinate(1.0f) == TRSlopeItalic);

    assert(GetSlopeFromSLNTCoordinate(0.0f) == TRSlopePlain);
    assert(GetSlopeFromSLNTCoordinate(-12.0f) == TRSlopeOblique);
    assert(GetSlopeFromSLNTCoordinate(12.0f) == TRSlopeOblique);
}

void SFNTUtilitiesTests::testNameStringToStringView() {
    const vector<uint8_t> bytes = { 0x00, 0x52, 0x06, 0x2F, 0x00, 0x6F };
    NameString nameString = makeNameString(bytes, SFNTEncodingUTF16BE);
    uint16_t buffer[3] = { 0 };
    TRStringView view = { buffer, 0, TRStringEncodingUTF8 };

    assert(NameStringToStringView(&nameString, &view) == TRTrue);
    assert(view.length == 3);
    assert(view.encoding == TRStringEncodingUTF16);
    assert(buffer[0] == 0x0052);
    assert(buffer[1] == 0x062F);
    assert(buffer[2] == 0x006F);
}

void SFNTUtilitiesTests::testNameStringToStringViewOddLength() {
    const vector<uint8_t> bytes = { 0x00, 0x52, 0x00, 0x6F, 0x00 };
    NameString nameString = makeNameString(bytes, SFNTEncodingUTF16BE);
    uint16_t buffer[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
    TRStringView view = { buffer, 0, TRStringEncodingUTF8 };

    assert(NameStringToStringView(&nameString, &view) == TRTrue);
    assert(view.length == 2);
    assert(buffer[2] == 0xFFFF);
}

void SFNTUtilitiesTests::testMacRomanNames() {
    /* The bytes are 'C', 'a', 'f', then e acute, en dash, euro, Apple logo, and caron. */
    const vector<uint8_t> bytes = { 0x43, 0x61, 0x66, 0x8E, 0xD0, 0xDB, 0xF0, 0xFF };
    NameString nameString = makeNameString(bytes, SFNTEncodingMacRoman);
    uint16_t buffer[8] = { 0 };
    TRStringView view = { buffer, 0, TRStringEncodingUTF8 };

    /* Each byte takes a code unit, which is two bytes. */
    assert(NameStringGetCapacity(&nameString) == 16);
    assert(NameStringToStringView(&nameString, &view) == TRTrue);
    assert(view.length == 8);
    assert(view.encoding == TRStringEncodingUTF16);

    const uint16_t expected[8] = { 0x0043, 0x0061, 0x0066, 0x00E9, 0x2013, 0x20AC, 0xF8FF, 0x02C7 };
    for (size_t index = 0; index < 8; index++) {
        assert(buffer[index] == expected[index]);
    }

    /* A name in UTF-16 takes as many bytes as it has. */
    const vector<uint8_t> wide = { 0x00, 0x52, 0x00, 0x6F };
    NameString wideName = makeNameString(wide, SFNTEncodingUTF16BE);
    assert(NameStringGetCapacity(&wideName) == 4);
}

void SFNTUtilitiesTests::testNameStringToStringViewInvalid() {
    const vector<uint8_t> bytes = { 0x00, 0x52 };
    uint16_t buffer[1] = { 0 };
    TRStringView view = { buffer, 7, TRStringEncodingUTF8 };

    NameString unknownEncoding = makeNameString(bytes, SFNTEncodingUnknown);
    assert(NameStringToStringView(&unknownEncoding, &view) == TRFalse);

    NameString noBytes = { nullptr, 2, SFNTEncodingUTF16BE };
    assert(NameStringToStringView(&noBytes, &view) == TRFalse);

    NameString noLength = { bytes.data(), 0, SFNTEncodingUTF16BE };
    assert(NameStringToStringView(&noLength, &view) == TRFalse);

    NameString singleByte = makeNameString({ 0x00 }, SFNTEncodingUTF16BE);
    assert(NameStringToStringView(&singleByte, &view) == TRFalse);

    assert(NameStringToStringView(nullptr, &view) == TRFalse);
    assert(view.length == 7);
    assert(view.encoding == TRStringEncodingUTF8);
}

static string toString(const NameString &nameString) {
    string result;

    for (size_t i = 0; i + 1 < nameString.length; i += 2) {
        assert(nameString.bytes[i] == 0);
        result.push_back(static_cast<char>(nameString.bytes[i + 1]));
    }

    return result;
}

void SFNTUtilitiesTests::testSearchEnglishName() {
    TestFace face("Roboto-Variable.abc.ttf");
    NameString nameString;

    assert(SearchEnglishName(face.get(), SFNTNameIDFontFamily, &nameString) == TRTrue);
    assert(nameString.encoding == SFNTEncodingUTF16BE);
    assert(toString(nameString) == "Roboto");

    assert(SearchEnglishName(face.get(), 256, &nameString) == TRTrue);
    assert(toString(nameString) == "Weight");

    assert(SearchEnglishName(face.get(), 341, &nameString) == TRTrue);
    assert(toString(nameString) == "RobotoRoman-CondensedBlack");
}

void SFNTUtilitiesTests::testSearchEnglishNameMissing() {
    TestFace face("Roboto-Regular.abc.ttf");
    const vector<uint8_t> bytes = { 0x00, 0x52 };
    NameString nameString = makeNameString(bytes, SFNTEncodingUTF16BE);

    assert(SearchEnglishName(face.get(), 999, &nameString) == TRFalse);
    assert(nameString.bytes == nullptr);
    assert(nameString.length == 0);
    assert(nameString.encoding == SFNTEncodingUnknown);

    TestFace nameless("COLRv0.extents.ttf");
    assert(SearchEnglishName(nameless.get(), SFNTNameIDFontFamily, &nameString) == TRFalse);
}

void SFNTUtilitiesTests::testSearchEnglishNameMacintosh() {
    TestFace face("nameID.dup.expected.ttf");
    NameString nameString;

    assert(SearchEnglishName(face.get(), SFNTNameIDFontFamily, &nameString) == TRTrue);
    assert(nameString.encoding == SFNTEncodingMacRoman);
    assert(nameString.length == 6);
    assert(memcmp(nameString.bytes, "Roboto", 6) == 0);

    uint16_t buffer[8];
    TRStringView view = { buffer, 0, TRStringEncodingUTF8 };
    assert(NameStringToStringView(&nameString, &view) == TRTrue);
    assert(view.length == 6);
    assert(buffer[0] == 'R' && buffer[5] == 'o');
}

void SFNTUtilitiesTests::testSearchFamilyAndSubfamilyName() {
    TestFace face("Roboto-Regular.abc.ttf");
    const TT_OS2 *os2Table = static_cast<TT_OS2 *>(FT_Get_Sfnt_Table(face.get(), FT_SFNT_OS2));
    NameString nameString;

    assert(SearchFamilyName(face.get(), os2Table, &nameString) == TRTrue);
    assert(toString(nameString) == "Roboto");

    assert(SearchSubfamilyName(face.get(), os2Table, &nameString) == TRTrue);
    assert(toString(nameString) == "Regular");

    assert(SearchFamilyName(face.get(), nullptr, &nameString) == TRTrue);
    assert(toString(nameString) == "Roboto");

    TestFace nameless("COLRv0.extents.ttf");
    assert(SearchFamilyName(nameless.get(), nullptr, &nameString) == TRFalse);
    assert(SearchSubfamilyName(nameless.get(), nullptr, &nameString) == TRFalse);
}

void SFNTUtilitiesTests::testSearchFullName() {
    TestFace variable("Roboto-Variable.abc.ttf");
    NameString nameString;

    assert(SearchFullName(variable.get(), &nameString) == TRTrue);
    assert(toString(nameString) == "Roboto");

    TestFace regular("Roboto-Regular.abc.ttf");
    assert(SearchFullName(regular.get(), &nameString) == TRFalse);
}

#ifdef STANDALONE_TESTING

int main() {
    SFNTUtilitiesTests tests;
    tests.run();

    return 0;
}

#endif
