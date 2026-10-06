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
#include <string>

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
    testVariableFace();
    testNamedStyles();
    testVariableColorFace();
    testPalettes();
    testPaletteFlags();
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

    const TRVariationAxis &width = metadata->variationAxesPtr[1];
    assert(width.tag == TRTagMake('w', 'd', 't', 'h'));
    assert(toString(width.name) == "Width");
    assert(width.minValue == 75.0f);
    assert(width.defaultValue == 100.0f);
    assert(width.maxValue == 100.0f);

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

    assert(metadata->namedStyleCount == 5);
    assert(metadata->namedStylesPtr[0].subfamilyName == nullptr);
    assert(metadata->namedStylesPtr[0].postScriptName == nullptr);
    assert(metadata->namedStylesPtr[0].coordinatesPtr[0] == 100.0f);
    assert(metadata->namedStylesPtr[0].coordinatesPtr[1] == 50.0f);

    const TRNamedStyle &regular = metadata->namedStylesPtr[4];
    assert(toString(regular.subfamilyName) == "Regular");
    assert(toString(regular.postScriptName) == "RocherColor-Regular");

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
