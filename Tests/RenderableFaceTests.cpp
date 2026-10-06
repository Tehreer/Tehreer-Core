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
#include <thread>
#include <vector>

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <Tehreer/TRFontFile.h>
#include <Tehreer/TRString.h>

extern "C" {
#include <Core/Allocator.h>
#include <Font/FaceMetadata.h>
#include <Graphics/GlyphBitmap.h>
#include <Graphics/RenderableFace.h>
#include <SFNT/Utilities.h>
}

#include "TestFonts.h"

#include "RenderableFaceTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 200;

void RenderableFaceTests::run() {
    testCreate();
    testCreateInvalidIndex();
    testGlyphIDs();
    testCopyTable();
    testSearchEnglishName();
    testDescription();
    testVariationDescription();
    testMetrics();
    testGlyphAdvance();
    testVariationGlyphAdvance();
    testRasterizeGlyph();
    testRasterizeColorGlyph();
    testRasterizeInvalidGlyph();
    testConcurrentAccess();
    testConcurrentVariations();
    testRetainRelease();
    testGlyphType();
}

class FontFileHolder {
public:
    explicit FontFileHolder(const char *fontName) {
        m_path = testFontPath(fontName);
        m_fontFile = TRFontFileCreateFromPath(m_path.c_str());
        assert(m_fontFile != nullptr);
    }

    ~FontFileHolder() {
        TRFontFileRelease(m_fontFile);
    }

    TRFontFileRef get() const { return m_fontFile; }

private:
    string m_path;
    TRFontFileRef m_fontFile = nullptr;
};

static string toString(const TRStringView *view) {
    assert(view != nullptr);
    assert(view->encoding == TRStringEncodingUTF16);

    auto *codeUnits = static_cast<const uint16_t *>(view->buffer);
    string result;

    for (size_t i = 0; i < view->length; i++) {
        result.push_back(static_cast<char>(codeUnits[i]));
    }

    return result;
}

static FontParams makeParams(FT_Fixed *coordinates, FT_UInt coordinateCount, FT_Color *colors = nullptr,
    FT_UInt colorCount = 0) {
    FontParams params = {};

    params.coordinatesPtr = coordinates;
    params.coordinateCount = coordinateCount;
    params.colorsPtr = colors;
    params.colorCount = colorCount;
    params.pixelWidth = 32 * 64;
    params.pixelHeight = 32 * 64;
    params.transform = { 0x10000, 0, 0, 0x10000 };

    return params;
}

static FT_Fixed fixed(int value) {
    return static_cast<FT_Fixed>(value) * 0x10000;
}

void RenderableFaceTests::testCreate() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);

    assert(face != nullptr);
    assert(face->faceIndex == 0);
    assert(face->glyphCount == 4);
    assert(face->metadata != nullptr);
    assert(toString(face->metadata->familyName) == "Roboto");

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testCreateInvalidIndex() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");

    assert(RenderableFaceCreate(fontFile.get(), 5) == nullptr);
}

void RenderableFaceTests::testGlyphIDs() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);

    assert(RenderableFaceGetCodePointGlyphID(face, 'a') == 1);
    assert(RenderableFaceGetCodePointGlyphID(face, 'b') == 2);
    assert(RenderableFaceGetCodePointGlyphID(face, 'c') == 3);
    assert(RenderableFaceGetCodePointGlyphID(face, 'd') == 0);
    assert(RenderableFaceGetCodePointGlyphID(face, 0x1F600) == 0);
    assert(RenderableFaceGetVariantGlyphID(face, 'a', 0xFE00) == 0);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testCopyTable() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    void *buffer = nullptr;
    TRUInteger size = 0;

    RenderableFaceCopyTable(face, TRTagMake('h', 'e', 'a', 'd'), &buffer, &size);

    assert(buffer != nullptr);
    assert(size == 54);

    auto *bytes = static_cast<uint8_t *>(buffer);
    const uint8_t version[] = { 0x00, 0x01, 0x00, 0x00 };
    const uint8_t magic[] = { 0x5F, 0x0F, 0x3C, 0xF5 };
    assert(memcmp(bytes, version, 4) == 0);
    assert(memcmp(bytes + 12, magic, 4) == 0);

    AllocatorDeallocateBlock(buffer);

    buffer = &size;
    size = 99;
    RenderableFaceCopyTable(face, TRTagMake('Z', 'Z', 'Z', 'Z'), &buffer, &size);
    assert(buffer == nullptr);
    assert(size == 0);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testSearchEnglishName() {
    FontFileHolder fontFile("Roboto-Variable.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    NameString nameString;

    assert(RenderableFaceSearchEnglishName(face, SFNTNameIDFontFamily, &nameString) == TRTrue);
    assert(nameString.length == 12);
    assert(nameString.encoding == SFNTEncodingUTF16BE);

    assert(RenderableFaceSearchEnglishName(face, 999, &nameString) == TRFalse);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testDescription() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    const TRStringView *subfamilyName = nullptr;
    FaceDescription description = {};

    RenderableFaceGetDescription(face, nullptr, &subfamilyName, &description);

    assert(toString(subfamilyName) == "Regular");
    assert(description.weight == TRWeightRegular);
    assert(description.width == TRWidthNormal);
    assert(description.slope == TRSlopePlain);

    RenderableFaceGetDescription(face, nullptr, nullptr, nullptr);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testVariationDescription() {
    FontFileHolder fontFile("Roboto-Variable.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    const TRStringView *subfamilyName = nullptr;
    FaceDescription description = {};

    TRFloat bold[] = { 700.0f, 100.0f };
    RenderableFaceGetDescription(face, bold, &subfamilyName, &description);
    assert(toString(subfamilyName) == "Bold");
    assert(description.weight == TRWeightBold);
    assert(description.width == TRWidthNormal);

    TRFloat condensedLight[] = { 300.0f, 75.0f };
    RenderableFaceGetDescription(face, condensedLight, &subfamilyName, &description);
    assert(toString(subfamilyName) == "Condensed Light");
    assert(description.weight == TRWeightLight);
    assert(description.width == TRWidthCondensed);

    TRFloat custom[] = { 450.0f, 80.0f };
    subfamilyName = reinterpret_cast<const TRStringView *>(&description);
    RenderableFaceGetDescription(face, custom, &subfamilyName, &description);
    assert(subfamilyName == nullptr);
    assert(description.weight == TRWeightMedium);
    assert(description.width == TRWidthCondensed);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testMetrics() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    FaceMetrics metrics = {};

    RenderableFaceGetMetrics(face, nullptr, &metrics);

    assert(metrics.unitsPerEM == 2048);
    assert(metrics.ascent == 1900);
    assert(metrics.descent == 500);
    assert(metrics.leading == 0);
    assert(metrics.underlinePosition == -200);
    assert(metrics.underlineThickness == 100);

    RenderableFaceGetMetrics(face, nullptr, nullptr);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testGlyphAdvance() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    FontParams params = makeParams(nullptr, 0);

    assert(RenderableFaceGetGlyphAdvance(face, &params, 1) == 1114);
    assert(RenderableFaceGetGlyphAdvance(face, &params, 2) == 1149);
    assert(RenderableFaceGetGlyphAdvance(face, &params, 3) == 1072);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testVariationGlyphAdvance() {
    FontFileHolder fontFile("Roboto-Variable.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);

    FT_Fixed regular[] = { fixed(400), fixed(100) };
    FT_Fixed thin[] = { fixed(100), fixed(100) };
    FT_Fixed black[] = { fixed(900), fixed(100) };

    FontParams regularParams = makeParams(regular, 2);
    FontParams thinParams = makeParams(thin, 2);
    FontParams blackParams = makeParams(black, 2);

    TRInt32 regularAdvance = RenderableFaceGetGlyphAdvance(face, &regularParams, 2);
    TRInt32 thinAdvance = RenderableFaceGetGlyphAdvance(face, &thinParams, 2);
    TRInt32 blackAdvance = RenderableFaceGetGlyphAdvance(face, &blackParams, 2);

    assert(regularAdvance == 1150);
    assert(thinAdvance != regularAdvance);
    assert(blackAdvance != regularAdvance);
    assert(RenderableFaceGetGlyphAdvance(face, &regularParams, 2) == regularAdvance);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testRasterizeGlyph() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    FontParams params = makeParams(nullptr, 0);
    FT_Color foreground = { 0, 0, 0, 255 };

    GlyphBitmapRef bitmap = RenderableFaceRasterizeGlyph(face, &params, 1, foreground);

    assert(bitmap != nullptr);
    assert(bitmap->format == BitmapFormatAlpha);
    assert(bitmap->width == 15);
    assert(bitmap->height == 17);
    assert(bitmap->left == 1);
    assert(bitmap->top == 17);

    bool hasInk = false;
    for (size_t i = 0; i < bitmap->width * bitmap->height; i++) {
        hasInk = hasInk || bitmap->buffer[i] > 0;
    }
    assert(hasInk);

    GlyphBitmapDestroy(bitmap);

    params.transform = { 0x20000, 0, 0, 0x20000 };
    GlyphBitmapRef scaled = RenderableFaceRasterizeGlyph(face, &params, 1, foreground);
    assert(scaled != nullptr);
    assert(scaled->width > 25);
    assert(scaled->height > 30);

    GlyphBitmapDestroy(scaled);
    RenderableFaceRelease(face);
}

void RenderableFaceTests::testRasterizeColorGlyph() {
    FontFileHolder fontFile("COLRv0.extents.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    FT_Color red = { 0, 0, 255, 255 };
    FT_Color colors[] = { red, red, red, red, red };
    FontParams params = makeParams(nullptr, 0, colors, 5);

    GlyphBitmapRef bitmap = RenderableFaceRasterizeGlyph(face, &params, 13, red);

    assert(bitmap != nullptr);
    assert(bitmap->format == BitmapFormatARGB);
    assert(bitmap->width == 30);
    assert(bitmap->height == 31);

    bool hasInk = false;
    for (size_t i = 0; i < bitmap->width * bitmap->height; i++) {
        const uint8_t *pixel = bitmap->buffer + i * 4;

        hasInk = hasInk || pixel[0] > 0;
        assert(pixel[1] == pixel[0]);
        assert(pixel[2] == 0);
        assert(pixel[3] == 0);
    }
    assert(hasInk);

    GlyphBitmapDestroy(bitmap);
    RenderableFaceRelease(face);
}

void RenderableFaceTests::testRasterizeInvalidGlyph() {
    FontFileHolder fontFile("COLRv0.extents.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    FontParams params = makeParams(nullptr, 0);
    FT_Color foreground = { 0, 0, 0, 255 };

    assert(RenderableFaceRasterizeGlyph(face, &params, 0, foreground) == nullptr);
    assert(RenderableFaceRasterizeGlyph(face, &params, 999, foreground) == nullptr);

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testConcurrentAccess() {
    FontFileHolder fontFile("Roboto-Variable.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([face]() {
            FT_Fixed coordinates[] = { fixed(400), fixed(100) };
            FontParams params = makeParams(coordinates, 2);
            FT_Color foreground = { 0, 0, 0, 255 };

            for (size_t j = 0; j < Iterations; j++) {
                assert(RenderableFaceGetCodePointGlyphID(face, 'a') == 1);
                assert(RenderableFaceGetGlyphAdvance(face, &params, 1) == 1114);

                GlyphBitmapRef bitmap = RenderableFaceRasterizeGlyph(face, &params, 1, foreground);
                assert(bitmap != nullptr);
                GlyphBitmapDestroy(bitmap);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testConcurrentVariations() {
    FontFileHolder fontFile("Roboto-Variable.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);

    FT_Fixed thin[] = { fixed(100), fixed(100) };
    FT_Fixed black[] = { fixed(900), fixed(100) };
    FontParams thinParams = makeParams(thin, 2);
    FontParams blackParams = makeParams(black, 2);

    TRInt32 thinAdvance = RenderableFaceGetGlyphAdvance(face, &thinParams, 2);
    TRInt32 blackAdvance = RenderableFaceGetGlyphAdvance(face, &blackParams, 2);
    assert(thinAdvance != blackAdvance);

    vector<thread> threads;

    for (size_t i = 0; i < NumThreads * 2; i++) {
        threads.emplace_back([&, i]() {
            const FontParams &params = (i % 2 == 0 ? thinParams : blackParams);
            TRInt32 expected = (i % 2 == 0 ? thinAdvance : blackAdvance);

            for (size_t j = 0; j < Iterations; j++) {
                assert(RenderableFaceGetGlyphAdvance(face, &params, 2) == expected);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    RenderableFaceRelease(face);
}

void RenderableFaceTests::testGlyphType() {
    /* A font without colors only has mask glyphs. */
    FontFileHolder plain("Roboto-Regular.abc.ttf");
    RenderableFaceRef plainFace = RenderableFaceCreate(plain.get(), 0);
    for (TRGlyphID glyph = 0; glyph < 4; glyph++) {
        assert(RenderableFaceGetGlyphType(plainFace, glyph) == GlyphTypeMask);
    }
    assert(RenderableFaceGetGlyphType(plainFace, 100) == GlyphTypeMask);
    RenderableFaceRelease(plainFace);

    /* Only the glyph with color layers is a color glyph. */
    FontFileHolder colored("COLRv0.extents.ttf");
    RenderableFaceRef coloredFace = RenderableFaceCreate(colored.get(), 0);
    assert(RenderableFaceGetGlyphType(coloredFace, 1) == GlyphTypeMask);
    assert(RenderableFaceGetGlyphType(coloredFace, 13) == GlyphTypeColor);
    RenderableFaceRelease(coloredFace);

    FontFileHolder variableColor("RocherColorGX.abc.ttf");
    RenderableFaceRef variableFace = RenderableFaceCreate(variableColor.get(), 0);
    assert(RenderableFaceGetGlyphType(variableFace, 0) == GlyphTypeColor);
    assert(RenderableFaceGetGlyphType(variableFace, 4) == GlyphTypeMask);
    RenderableFaceRelease(variableFace);
}

void RenderableFaceTests::testRetainRelease() {
    FontFileHolder fontFile("Roboto-Regular.abc.ttf");
    RenderableFaceRef face = RenderableFaceCreate(fontFile.get(), 0);

    assert(RenderableFaceRetain(face) == face);
    RenderableFaceRelease(face);
    assert(RenderableFaceGetCodePointGlyphID(face, 'a') == 1);

    RenderableFaceRelease(face);
}

#ifdef STANDALONE_TESTING

int main() {
    RenderableFaceTests tests;
    tests.run();

    return 0;
}

#endif
