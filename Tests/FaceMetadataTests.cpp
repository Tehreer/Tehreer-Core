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
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <Core/Mutex.h>
#include <Font/FaceMetadata.h>
#include <Graphics/FreeType.h>
}

#include "TestFonts.h"

#include "FaceMetadataTests.h"

using namespace std;
using namespace Tehreer;

void FaceMetadataTests::run() {
    testStaticFace();
    testNamelessFace();
    testFaceWithoutOS2Table();
    testOutOfRangeWidthClass();
    testVariableFace();
    testAxisFlags();
    testNamedStyles();
    testVariableColorFace();
    testPalettes();
    testPaletteFlags();
    testOversizedPalettes();
    testIndependentOfFace();
    testRetainRelease();
}

class DefaultFace {
public:
    explicit DefaultFace(const char *fontName) {
        FreeTypeRef freetype = FreeTypeGetDefault();

        MutexLock(&freetype->mutex);
        FT_Error error = FT_New_Face(freetype->library, testFontPath(fontName).c_str(), 0, &m_face);
        MutexUnlock(&freetype->mutex);

        assert(error == FT_Err_Ok);
    }

    DefaultFace(const uint8_t *data, size_t size) {
        FreeTypeRef freetype = FreeTypeGetDefault();

        MutexLock(&freetype->mutex);
        FT_Error error = FT_New_Memory_Face(freetype->library, data, static_cast<FT_Long>(size), 0, &m_face);
        MutexUnlock(&freetype->mutex);

        assert(error == FT_Err_Ok);
    }

    ~DefaultFace() {
        if (m_face) {
            release();
        }
    }

    FT_Face get() const { return m_face; }

    void release() {
        FreeTypeRef freetype = FreeTypeGetDefault();

        MutexLock(&freetype->mutex);
        FT_Done_Face(m_face);
        MutexUnlock(&freetype->mutex);

        m_face = nullptr;
    }

private:
    FT_Face m_face = nullptr;
};

static string toString(const TRStringView *view) {
    assert(view != nullptr);
    assert(view->encoding == TRStringEncodingUTF16);

    auto *codeUnits = static_cast<const uint16_t *>(view->buffer);
    string result;

    for (size_t i = 0; i < view->length; i++) {
        assert(codeUnits[i] < 0x80);
        result.push_back(static_cast<char>(codeUnits[i]));
    }

    return result;
}

void FaceMetadataTests::testStaticFace() {
    DefaultFace face("Roboto-Regular.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(toString(metadata->familyName) == "Roboto");
    assert(toString(metadata->subfamilyName) == "Regular");
    assert(metadata->weight == TRWeightRegular);
    assert(metadata->width == TRWidthNormal);
    assert(metadata->slope == TRSlopePlain);

    assert(metadata->variationAxesPtr == nullptr);
    assert(metadata->variationAxisCount == 0);
    assert(metadata->namedStylesPtr == nullptr);
    assert(metadata->namedStyleCount == 0);
    assert(metadata->paletteEntriesPtr == nullptr);
    assert(metadata->paletteEntryCount == 0);
    assert(metadata->predefinedPalettesPtr == nullptr);
    assert(metadata->predefinedPaletteCount == 0);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testNamelessFace() {
    DefaultFace face("COLRv0.extents.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->familyName == nullptr);
    assert(metadata->subfamilyName == nullptr);
    assert(metadata->weight == TRWeightRegular);
    assert(metadata->width == TRWidthNormal);
    assert(metadata->slope == TRSlopePlain);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testFaceWithoutOS2Table() {
    DefaultFace face("varc-6868.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->weight == TRWeightRegular);
    assert(metadata->width == TRWidthNormal);
    assert(metadata->slope == TRSlopePlain);

    FaceMetadataRelease(metadata);
}

static size_t findTable(const vector<uint8_t> &data, const char *tag) {
    size_t tableCount = (data[4] << 8) | data[5];

    for (size_t i = 0; i < tableCount; i++) {
        size_t record = 12 + (i * 16);

        if (memcmp(&data[record], tag, 4) == 0) {
            return (static_cast<size_t>(data[record + 8]) << 24)
                 | (static_cast<size_t>(data[record + 9]) << 16)
                 | (static_cast<size_t>(data[record + 10]) << 8)
                 | static_cast<size_t>(data[record + 11]);
        }
    }

    return 0;
}

static TRWidth readWidthWithClass(uint16_t widthClass) {
    ifstream stream(testFontPath("Roboto-Regular.abc.ttf"), ios::binary);
    vector<uint8_t> data((istreambuf_iterator<char>(stream)), istreambuf_iterator<char>());
    size_t os2 = findTable(data, "OS/2");
    assert(os2 != 0);

    /* usWidthClass is at offset 6 of the OS/2 table. */
    data[os2 + 6] = static_cast<uint8_t>(widthClass >> 8);
    data[os2 + 7] = static_cast<uint8_t>(widthClass);

    DefaultFace face(data.data(), data.size());
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());
    assert(metadata != nullptr);

    TRWidth width = metadata->width;
    FaceMetadataRelease(metadata);

    return width;
}

void FaceMetadataTests::testOutOfRangeWidthClass() {
    assert(readWidthWithClass(0) == TRWidthUltraCondensed);
    assert(readWidthWithClass(1) == TRWidthUltraCondensed);
    assert(readWidthWithClass(5) == TRWidthNormal);
    assert(readWidthWithClass(9) == TRWidthUltraExpanded);
    assert(readWidthWithClass(10) == TRWidthUltraExpanded);
    assert(readWidthWithClass(65535) == TRWidthUltraExpanded);
}

void FaceMetadataTests::testVariableFace() {
    DefaultFace face("Roboto-Variable.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->variationAxisCount == 2);

    const TRVariationAxis &weight = metadata->variationAxesPtr[0];
    assert(weight.tag == TRTagMake('w', 'g', 'h', 't'));
    assert(toString(weight.name) == "Weight");
    assert(weight.minValue == 100.0f);
    assert(weight.defaultValue == 400.0f);
    assert(weight.maxValue == 900.0f);
    assert(weight.flags == 0);

    const TRVariationAxis &width = metadata->variationAxesPtr[1];
    assert(width.tag == TRTagMake('w', 'd', 't', 'h'));
    assert(toString(width.name) == "Width");
    assert(width.minValue == 75.0f);
    assert(width.defaultValue == 100.0f);
    assert(width.maxValue == 100.0f);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testAxisFlags() {
    DefaultFace face("Roboto-Variable.hidden.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->variationAxisCount == 2);

    /* Only the first axis is hidden. */
    assert(metadata->variationAxesPtr[0].flags == TRVariationAxisFlagHidden);
    assert(metadata->variationAxesPtr[1].flags == 0);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testNamedStyles() {
    DefaultFace face("Roboto-Variable.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->namedStyleCount == 18);

    const TRNamedStyle &thin = metadata->namedStylesPtr[0];
    assert(toString(thin.subfamilyName) == "Thin");
    assert(toString(thin.postScriptName) == "RobotoRoman-Thin");
    assert(thin.coordinateCount == 2);
    assert(thin.coordinatesPtr[0] == 100.0f);
    assert(thin.coordinatesPtr[1] == 100.0f);

    const TRNamedStyle &condensedBlack = metadata->namedStylesPtr[17];
    assert(toString(condensedBlack.subfamilyName) == "Condensed Black");
    assert(toString(condensedBlack.postScriptName) == "RobotoRoman-CondensedBlack");
    assert(condensedBlack.coordinatesPtr[0] == 900.0f);
    assert(condensedBlack.coordinatesPtr[1] == 75.0f);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testVariableColorFace() {
    DefaultFace face("RocherColorGX.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->variationAxisCount == 2);
    assert(metadata->variationAxesPtr[0].tag == TRTagMake('B', 'V', 'E', 'L'));
    assert(toString(metadata->variationAxesPtr[0].name) == "Bevel");
    assert(metadata->variationAxesPtr[1].tag == TRTagMake('S', 'H', 'D', 'W'));
    assert(metadata->variationAxesPtr[1].name == nullptr);

    /* The font has four instances, but none for its default coordinates, which come first. */
    assert(metadata->namedStyleCount == 5);

    const TRNamedStyle &regular = metadata->namedStylesPtr[0];
    assert(toString(regular.subfamilyName) == "Regular");
    assert(regular.postScriptName == nullptr);
    assert(regular.coordinatesPtr[0] == 100.0f);
    assert(regular.coordinatesPtr[1] == 100.0f);

    assert(metadata->namedStylesPtr[1].subfamilyName == nullptr);
    assert(metadata->namedStylesPtr[1].postScriptName == nullptr);
    assert(metadata->namedStylesPtr[1].coordinatesPtr[0] == 100.0f);
    assert(metadata->namedStylesPtr[1].coordinatesPtr[1] == 50.0f);

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testPalettes() {
    DefaultFace face("RocherColorGX.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    FT_Palette_Data paletteData;
    assert(FT_Palette_Data_Get(face.get(), &paletteData) == FT_Err_Ok);

    assert(metadata != nullptr);
    assert(metadata->paletteEntryCount == 4);
    assert(metadata->predefinedPaletteCount == 11);
    assert(metadata->paletteEntryCount == paletteData.num_palette_entries);
    assert(metadata->predefinedPaletteCount == paletteData.num_palettes);

    for (size_t i = 0; i < metadata->paletteEntryCount; i++) {
        assert(metadata->paletteEntriesPtr[i].name == nullptr);
    }

    for (size_t i = 0; i < metadata->predefinedPaletteCount; i++) {
        const TRPredefinedPalette &palette = metadata->predefinedPalettesPtr[i];
        FT_Color *colors = nullptr;

        assert(FT_Palette_Select(face.get(), static_cast<FT_UShort>(i), &colors) == FT_Err_Ok);
        assert(palette.name == nullptr);
        assert(palette.colorCount == 4);

        for (size_t j = 0; j < palette.colorCount; j++) {
            TRColor expected = TRColorMake(colors[j].alpha, colors[j].red, colors[j].green,
                colors[j].blue);
            assert(palette.colorsPtr[j] == expected);
        }
    }

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testPaletteFlags() {
    DefaultFace face("COLRv0.extents.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(metadata != nullptr);
    assert(metadata->paletteEntryCount == 5);
    assert(metadata->predefinedPaletteCount == 2);
    assert(metadata->predefinedPalettesPtr[0].flags == 0);
    assert(metadata->predefinedPalettesPtr[1].flags == 1);

    FaceMetadataRelease(metadata);
}

static void writeUInt16(vector<uint8_t> &data, size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value >> 8);
    data[offset + 1] = static_cast<uint8_t>(value);
}

static void writeUInt32(vector<uint8_t> &data, size_t offset, uint32_t value) {
    writeUInt16(data, offset, value >> 16);
    writeUInt16(data, offset + 2, value & 0xFFFF);
}

/*
 * Replaces the CPAL table of a font with a valid version 0 table that has the given number of
 * palettes and entries per palette. Every palette refers to the same colors, so the table is much
 * smaller than the palettes that it claims.
 */
static vector<uint8_t> makeFontWithPalettes(const char *fontName, uint16_t paletteCount,
    uint16_t entryCount) {
    ifstream stream(testFontPath(fontName), ios::binary);
    vector<uint8_t> data((istreambuf_iterator<char>(stream)), istreambuf_iterator<char>());
    size_t tableCount = (data[4] << 8) | data[5];
    size_t record = 0;

    for (size_t i = 0; i < tableCount && record == 0; i++) {
        if (memcmp(&data[12 + (i * 16)], "CPAL", 4) == 0) {
            record = 12 + (i * 16);
        }
    }
    assert(record != 0);

    const uint32_t colorsOffset = 12 + (paletteCount * 2U);
    const uint32_t tableSize = colorsOffset + (entryCount * 4U);
    const size_t tableOffset = (data.size() + 3) / 4 * 4;

    data.resize(tableOffset + tableSize, 0);
    writeUInt16(data, tableOffset + 2, entryCount);
    writeUInt16(data, tableOffset + 4, paletteCount);
    writeUInt16(data, tableOffset + 6, entryCount);
    writeUInt32(data, tableOffset + 8, colorsOffset);

    writeUInt32(data, record + 8, static_cast<uint32_t>(tableOffset));
    writeUInt32(data, record + 12, tableSize);

    return data;
}

void FaceMetadataTests::testOversizedPalettes() {
    FreeTypeRef freetype = FreeTypeGetDefault();
    vector<uint8_t> data = makeFontWithPalettes("RocherColorGX.abc.ttf", 65535, 65535);
    FT_Face ftFace = nullptr;

    MutexLock(&freetype->mutex);
    FT_Error error = FT_New_Memory_Face(freetype->library, data.data(),
        static_cast<FT_Long>(data.size()), 0, &ftFace);
    MutexUnlock(&freetype->mutex);
    assert(error == FT_Err_Ok);

    /* The palettes would need gigabytes of colors, which the table does not hold. */
    FaceMetadataRef metadata = FaceMetadataCreate(ftFace);
    assert(metadata != nullptr);
    assert(metadata->predefinedPaletteCount == 0);
    assert(metadata->paletteEntryCount == 0);

    FaceMetadataRelease(metadata);

    MutexLock(&freetype->mutex);
    FT_Done_Face(ftFace);
    MutexUnlock(&freetype->mutex);
}

void FaceMetadataTests::testIndependentOfFace() {
    DefaultFace face("Roboto-Variable.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());
    face.release();

    assert(metadata != nullptr);
    assert(toString(metadata->familyName) == "Roboto");
    assert(toString(metadata->variationAxesPtr[0].name) == "Weight");
    assert(toString(metadata->namedStylesPtr[3].subfamilyName) == "Regular");
    assert(toString(metadata->namedStylesPtr[3].postScriptName) == "RobotoRoman-Regular");

    FaceMetadataRelease(metadata);
}

void FaceMetadataTests::testRetainRelease() {
    DefaultFace face("Roboto-Regular.abc.ttf");
    FaceMetadataRef metadata = FaceMetadataCreate(face.get());

    assert(FaceMetadataRetain(metadata) == metadata);
    FaceMetadataRelease(metadata);
    assert(toString(metadata->familyName) == "Roboto");

    FaceMetadataRelease(metadata);
}

#ifdef STANDALONE_TESTING

int main() {
    FaceMetadataTests tests;
    tests.run();

    return 0;
}

#endif
