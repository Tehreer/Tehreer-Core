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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <string>

#include <hb.h>

#include <Tehreer/TRFontFile.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>
}

#include "TestPath.h"
#include "TestTypeface.h"

#include "TypefaceTests.h"

using namespace std;
using namespace Tehreer;

void TypefaceTests::run() {
    testStaticTypeface();
    testVariableTypeface();
    testVariationCoordinates();
    testCoordinatesAreCopied();
    testColorTypeface();
    testVariableColorTypeface();
    testNamesAndMetadata();
    testOutlivesFaces();
    testRetainRelease();
    testCreate();
    testCreateInvalid();
    testCreateWithVariation();
    testCreateWithVariationClamping();
    testCreateWithColors();
    testDerivedKeepsOtherTraits();
    testDerivedOutlivesSource();
    testNamesAndMetrics();
    testTableData();
    testGlyphName();
    testGlyphIDs();
    testGlyphAdvance();
    testGlyphPath();
    testBitmapTypeface();
}

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

void TypefaceTests::testStaticTypeface() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    assert(TRTypefaceGetWeight(typeface) == TRWeightRegular);
    assert(TRTypefaceGetWidth(typeface) == TRWidthNormal);
    assert(TRTypefaceGetSlope(typeface) == TRSlopePlain);
    assert(TRTypefaceGetUnitsPerEM(typeface) == 2048);
    assert(TRTypefaceGetAscent(typeface) == 1900);
    assert(TRTypefaceGetDescent(typeface) == 500);
    assert(TRTypefaceGetLeading(typeface) == 0);
    assert(TRTypefaceGetGlyphCount(typeface) == 4);

    assert(TRTypefaceGetVariationAxisCount(typeface) == 0);
    assert(TRTypefaceGetNamedStyleCount(typeface) == 0);
    assert(TRTypefaceGetPaletteEntryCount(typeface) == 0);
    assert(TRTypefaceGetPredefinedPaletteCount(typeface) == 0);
    assert(TRTypefaceGetVariationCoordinatesPtr(typeface) == nullptr);
    assert(TRTypefaceGetAssociatedColorsPtr(typeface) == nullptr);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testVariableTypeface() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf");

    assert(TRTypefaceGetVariationAxisCount(typeface) == 2);
    assert(TRTypefaceGetNamedStyleCount(typeface) == 18);
    assert(TRTypefaceGetVariationAxesPtr(typeface)[0].tag == TRTagMake('w', 'g', 'h', 't'));
    assert(TRTypefaceGetVariationAxesPtr(typeface)[1].tag == TRTagMake('w', 'd', 't', 'h'));
    assert(TRTypefaceGetNamedStylesPtr(typeface)[0].coordinateCount == 2);

    const TRFloat *coordinates = TRTypefaceGetVariationCoordinatesPtr(typeface);
    assert(coordinates != nullptr);
    assert(coordinates[0] == 400.0f);
    assert(coordinates[1] == 100.0f);

    assert(TRTypefaceGetWeight(typeface) == TRWeightRegular);
    assert(TRTypefaceGetWidth(typeface) == TRWidthNormal);
    assert(toString(typeface->subfamilyName) == "Regular");

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testVariationCoordinates() {
    const TRFloat coordinates[] = { 700.0f, 75.0f };
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf", coordinates);

    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[0] == 700.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[1] == 75.0f);
    assert(TRTypefaceGetWeight(typeface) == TRWeightBold);
    assert(TRTypefaceGetWidth(typeface) == TRWidthCondensed);
    assert(toString(typeface->subfamilyName) == "Condensed Bold");

    assert(typeface->rawCoordinates[0] == 700 * 0x10000);
    assert(typeface->rawCoordinates[1] == 75 * 0x10000);

    const TRFloat custom[] = { 450.0f, 90.0f };
    TRTypefaceRef customTypeface = createTestTypeface("Roboto-Variable.abc.ttf", custom);

    assert(TRTypefaceGetWeight(customTypeface) == TRWeightMedium);
    assert(customTypeface->subfamilyName == nullptr);

    TRTypefaceRelease(customTypeface);
    TRTypefaceRelease(typeface);
}

void TypefaceTests::testCoordinatesAreCopied() {
    TRFloat coordinates[] = { 700.0f, 100.0f };
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf", coordinates);

    coordinates[0] = 100.0f;
    coordinates[1] = 75.0f;

    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[0] == 700.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[1] == 100.0f);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testColorTypeface() {
    TRTypefaceRef typeface = createTestTypeface("COLRv0.extents.ttf");

    assert(TRTypefaceGetPaletteEntryCount(typeface) == 5);
    assert(TRTypefaceGetPredefinedPaletteCount(typeface) == 2);

    const TRColor *colors = TRTypefaceGetAssociatedColorsPtr(typeface);
    const TRPredefinedPalette &palette = TRTypefaceGetPredefinedPalettesPtr(typeface)[0];
    assert(colors != nullptr);

    for (size_t i = 0; i < 5; i++) {
        assert(colors[i] == palette.colorsPtr[i]);

        const FT_Color &raw = typeface->rawColors[i];
        assert(TRColorMake(raw.alpha, raw.red, raw.green, raw.blue) == colors[i]);
    }

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testVariableColorTypeface() {
    TRTypefaceRef typeface = createTestTypeface("RocherColorGX.abc.ttf");

    assert(TRTypefaceGetVariationAxisCount(typeface) == 2);
    assert(TRTypefaceGetPaletteEntryCount(typeface) == 4);
    assert(TRTypefaceGetPredefinedPaletteCount(typeface) == 11);
    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[0] == 100.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(typeface)[1] == 100.0f);
    assert(toString(typeface->subfamilyName) == "Regular");

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testNamesAndMetadata() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    assert(toString(typeface->familyName) == "Roboto");
    assert(toString(typeface->subfamilyName) == "Regular");
    assert(typeface->underlinePosition == -200);
    assert(typeface->underlineThickness == 100);

    TRTypefaceRelease(typeface);

    TRTypefaceRef nameless = createTestTypeface("COLRv0.extents.ttf");
    assert(nameless->familyName == nullptr);
    assert(nameless->subfamilyName == nullptr);

    TRTypefaceRelease(nameless);
}

void TypefaceTests::testOutlivesFaces() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf");

    assert(TRTypefaceGetGlyphCount(typeface) == 4);
    assert(TRTypefaceGetVariationAxesPtr(typeface)[0].tag == TRTagMake('w', 'g', 'h', 't'));
    assert(RenderableFaceGetCodePointGlyphID(typeface->renderableFace, 'a') == 1);
    assert(hb_face_get_glyph_count(typeface->shapableFace->hbFace) == 4);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testRetainRelease() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    assert(TRTypefaceRetain(typeface) == typeface);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 2);

    TRTypefaceRelease(typeface);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 1);
    assert(TRTypefaceGetGlyphCount(typeface) == 4);

    TRTypefaceRelease(typeface);
}

static TRTypefaceRef createTypeface(const char *fontName, TRUInteger faceIndex = 0) {
    return createTestTypeface(fontName, nullptr, faceIndex);
}

void TypefaceTests::testCreate() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.abc.ttf");
    assert(typeface != nullptr);

    assert(TRTypefaceGetWeight(typeface) == TRWeightRegular);
    assert(TRTypefaceGetUnitsPerEM(typeface) == 2048);
    assert(TRTypefaceGetGlyphCount(typeface) == 4);
    assert(toString(TRTypefaceGetFamilyName(typeface)) == "Roboto");
    assert(toString(TRTypefaceGetSubfamilyName(typeface)) == "Regular");

    /* The font file is released by the helper, but the typeface keeps what it needs. */
    assert(TRTypefaceGetGlyphID(typeface, 'a') == 1);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testCreateInvalid() {
    /* A typeface comes from a font file, which is not made from what is not a font. */
    assert(TRFontFileCreateFromPath(nullptr) == nullptr);
    assert(TRFontFileCreateFromPath("/no/such/font.ttf") == nullptr);
    assert(TRFontFileCreateFromMemory(nullptr, 10) == nullptr);

    const char garbage[] = "not a font";
    assert(TRFontFileCreateFromMemory(garbage, sizeof(garbage)) == nullptr);
}

void TypefaceTests::testCreateWithVariation() {
    TRTypefaceRef base = createTypeface("Roboto-Variable.abc.ttf");

    const TRFloat coordinates[] = { 700.0f, 75.0f };
    TRTypefaceRef derived = TRTypefaceCreateWithVariation(base, coordinates, 2);
    assert(derived != nullptr);
    assert(derived != base);

    assert(TRTypefaceGetVariationCoordinatesPtr(derived)[0] == 700.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(derived)[1] == 75.0f);
    assert(TRTypefaceGetWeight(derived) == TRWeightBold);
    assert(TRTypefaceGetWidth(derived) == TRWidthCondensed);
    assert(toString(TRTypefaceGetSubfamilyName(derived)) == "Condensed Bold");

    /* The source is not affected. */
    assert(TRTypefaceGetVariationCoordinatesPtr(base)[0] == 400.0f);
    assert(TRTypefaceGetWeight(base) == TRWeightRegular);

    /* Missing coordinates take the defaults of their axes. */
    TRTypefaceRef partial = TRTypefaceCreateWithVariation(base, coordinates, 1);
    assert(TRTypefaceGetVariationCoordinatesPtr(partial)[0] == 700.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(partial)[1] == 100.0f);

    TRTypefaceRef defaults = TRTypefaceCreateWithVariation(base, nullptr, 0);
    assert(TRTypefaceGetVariationCoordinatesPtr(defaults)[0] == 400.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(defaults)[1] == 100.0f);

    /* The coordinates are copied. */
    TRFloat mutableCoordinates[] = { 300.0f, 80.0f };
    TRTypefaceRef copied = TRTypefaceCreateWithVariation(base, mutableCoordinates, 2);
    mutableCoordinates[0] = 900.0f;
    assert(TRTypefaceGetVariationCoordinatesPtr(copied)[0] == 300.0f);

    /* A derived typeface can be derived again. */
    const TRFloat other[] = { 200.0f, 100.0f };
    TRTypefaceRef second = TRTypefaceCreateWithVariation(derived, other, 2);
    assert(second != nullptr);
    assert(TRTypefaceGetVariationCoordinatesPtr(second)[0] == 200.0f);

    TRTypefaceRelease(second);
    TRTypefaceRelease(copied);
    TRTypefaceRelease(defaults);
    TRTypefaceRelease(partial);
    TRTypefaceRelease(derived);
    TRTypefaceRelease(base);

    /* A static typeface cannot be varied. */
    TRTypefaceRef fixed = createTypeface("Roboto-Regular.abc.ttf");
    assert(TRTypefaceCreateWithVariation(fixed, coordinates, 2) == nullptr);
    TRTypefaceRelease(fixed);
}

void TypefaceTests::testCreateWithVariationClamping() {
    TRTypefaceRef base = createTypeface("Roboto-Variable.abc.ttf");
    const TRVariationAxis *axes = TRTypefaceGetVariationAxesPtr(base);

    const TRFloat tooLow[] = { axes[0].minValue - 500.0f, axes[1].minValue - 500.0f };
    TRTypefaceRef low = TRTypefaceCreateWithVariation(base, tooLow, 2);
    assert(TRTypefaceGetVariationCoordinatesPtr(low)[0] == axes[0].minValue);
    assert(TRTypefaceGetVariationCoordinatesPtr(low)[1] == axes[1].minValue);
    assert(low->rawCoordinates[0] == static_cast<FT_Fixed>(axes[0].minValue * 65536.0f));

    const TRFloat tooHigh[] = { axes[0].maxValue + 500.0f, axes[1].maxValue + 500.0f };
    TRTypefaceRef high = TRTypefaceCreateWithVariation(base, tooHigh, 2);
    assert(TRTypefaceGetVariationCoordinatesPtr(high)[0] == axes[0].maxValue);
    assert(TRTypefaceGetVariationCoordinatesPtr(high)[1] == axes[1].maxValue);
    assert(TRTypefaceGetWeight(high) == TRWeightExtraHeavy);

    TRTypefaceRelease(high);
    TRTypefaceRelease(low);
    TRTypefaceRelease(base);
}

void TypefaceTests::testCreateWithColors() {
    TRTypefaceRef base = createTypeface("COLRv0.extents.ttf");

    const TRColor colors[] = {
        TRColorMake(0xFF, 0xFF, 0x00, 0x00),
        TRColorMake(0x80, 0x00, 0xFF, 0x00)
    };
    TRTypefaceRef derived = TRTypefaceCreateWithColors(base, colors, 2);
    assert(derived != nullptr);

    const TRColor *associated = TRTypefaceGetAssociatedColorsPtr(derived);
    assert(associated[0] == colors[0]);
    assert(associated[1] == colors[1]);
    /* Missing colors are opaque black. */
    for (size_t i = 2; i < TRTypefaceGetPaletteEntryCount(derived); i++) {
        assert(associated[i] == TRColorMake(0xFF, 0x00, 0x00, 0x00));
    }

    assert(derived->rawColors[0].red == 0xFF);
    assert(derived->rawColors[1].alpha == 0x80);
    assert(derived->rawColors[1].green == 0xFF);

    /* The source keeps its palette. */
    assert(TRTypefaceGetAssociatedColorsPtr(base)[0]
           == TRTypefaceGetPredefinedPalettesPtr(base)[0].colorsPtr[0]);

    TRTypefaceRef defaults = TRTypefaceCreateWithColors(base, nullptr, 0);
    assert(TRTypefaceGetAssociatedColorsPtr(defaults)[0] == TRColorMake(0xFF, 0x00, 0x00, 0x00));

    TRTypefaceRelease(defaults);
    TRTypefaceRelease(derived);
    TRTypefaceRelease(base);

    /* A typeface without a palette has nothing to color. */
    TRTypefaceRef plain = createTypeface("Roboto-Regular.abc.ttf");
    assert(TRTypefaceCreateWithColors(plain, colors, 2) == nullptr);
    TRTypefaceRelease(plain);
}

void TypefaceTests::testDerivedKeepsOtherTraits() {
    TRTypefaceRef base = createTypeface("RocherColorGX.abc.ttf");

    const TRColor colors[] = {
        TRColorMake(0xFF, 0x11, 0x22, 0x33), TRColorMake(0xFF, 0x44, 0x55, 0x66),
        TRColorMake(0xFF, 0x77, 0x88, 0x99), TRColorMake(0xFF, 0xAA, 0xBB, 0xCC)
    };
    TRTypefaceRef colored = TRTypefaceCreateWithColors(base, colors, 4);

    /* A variation instance of a colored typeface keeps the colors. */
    const TRFloat coordinates[] = { 50.0f, 60.0f };
    TRTypefaceRef both = TRTypefaceCreateWithVariation(colored, coordinates, 2);
    assert(TRTypefaceGetVariationCoordinatesPtr(both)[0] == 50.0f);
    for (size_t i = 0; i < 4; i++) {
        assert(TRTypefaceGetAssociatedColorsPtr(both)[i] == colors[i]);
    }

    /* A color instance of a varied typeface keeps the coordinates. */
    TRTypefaceRef varied = TRTypefaceCreateWithVariation(base, coordinates, 2);
    TRTypefaceRef variedColored = TRTypefaceCreateWithColors(varied, colors, 4);
    assert(TRTypefaceGetVariationCoordinatesPtr(variedColored)[0] == 50.0f);
    assert(TRTypefaceGetVariationCoordinatesPtr(variedColored)[1] == 60.0f);
    assert(TRTypefaceGetAssociatedColorsPtr(variedColored)[3] == colors[3]);

    TRTypefaceRelease(variedColored);
    TRTypefaceRelease(varied);
    TRTypefaceRelease(both);
    TRTypefaceRelease(colored);
    TRTypefaceRelease(base);
}

void TypefaceTests::testDerivedOutlivesSource() {
    TRTypefaceRef base = createTypeface("Roboto-Variable.abc.ttf");
    const TRFloat coordinates[] = { 700.0f, 100.0f };
    TRTypefaceRef derived = TRTypefaceCreateWithVariation(base, coordinates, 2);

    TRTypefaceRelease(base);

    assert(TRTypefaceGetGlyphID(derived, 'b') == 2);
    assert(toString(TRTypefaceGetFamilyName(derived)) == "Roboto");
    assert(TRTypefaceGetGlyphAdvance(derived, 2, 2048.0f, TRFalse) > 0.0f);

    TRTypefaceRelease(derived);
}

void TypefaceTests::testNamesAndMetrics() {
    TRTypefaceRef typeface = createTypeface("Roboto-Variable.abc.ttf");

    /* A variable font is named after the style that its coordinates match. */
    assert(toString(TRTypefaceGetFullName(typeface)) == "Roboto Regular");

    const TRFloat thinCoordinates[] = { 100.0f, 100.0f };
    TRTypefaceRef thin = TRTypefaceCreateWithVariation(typeface, thinCoordinates, 2);
    assert(toString(TRTypefaceGetSubfamilyName(thin)) == "Thin");
    assert(toString(TRTypefaceGetFullName(thin)) == "Roboto Thin");
    TRTypefaceRelease(thin);

    /* It has no full name if they match no style. */
    const TRFloat oddCoordinates[] = { 123.0f, 100.0f };
    TRTypefaceRef odd = TRTypefaceCreateWithVariation(typeface, oddCoordinates, 2);
    assert(TRTypefaceGetSubfamilyName(odd) == nullptr);
    assert(TRTypefaceGetFullName(odd) == nullptr);
    TRTypefaceRelease(odd);

    TRRect box = TRTypefaceGetBoundingBox(typeface);
    assert(box.origin.x == -1510.0f);
    assert(box.origin.y == -555.0f);
    assert(box.size.width == 3863.0f);
    assert(box.size.height == 2718.0f);

    assert(TRTypefaceGetUnderlinePosition(typeface) == -200);
    assert(TRTypefaceGetUnderlineThickness(typeface) == 100);
    assert(TRTypefaceGetStrikeoutPosition(typeface) == 512);
    assert(TRTypefaceGetStrikeoutThickness(typeface) == 102);

    TRTypefaceRelease(typeface);

    /* No OS/2 table means no strikeout. */
    TRTypefaceRef noOS2 = createTypeface("varc-6868.ttf");
    assert(TRTypefaceGetStrikeoutPosition(noOS2) == 0);
    assert(TRTypefaceGetStrikeoutThickness(noOS2) == 0);
    TRTypefaceRelease(noOS2);

    /* Names of the Macintosh platform are in Mac Roman. */
    TRTypefaceRef macNames = createTypeface("nameID.dup.expected.ttf");
    assert(toString(TRTypefaceGetFamilyName(macNames)) == "Roboto");
    TRTypefaceRelease(macNames);
}

void TypefaceTests::testTableData() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.abc.ttf");
    const TRTag head = TRTagMake('h', 'e', 'a', 'd');

    /* The head table is 54 bytes, and begins with the version 1.0. */
    assert(TRTypefaceGetTableSize(typeface, head) == 54);

    uint8_t buffer[54] = { 0 };
    assert(TRTypefaceGetTableData(typeface, head, 0, buffer, sizeof(buffer)) == 54);
    assert(buffer[0] == 0 && buffer[1] == 1 && buffer[2] == 0 && buffer[3] == 0);

    /* A table that does not fit is cut short, and the rest of the buffer is left alone. */
    uint8_t small[6] = { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE };
    assert(TRTypefaceGetTableData(typeface, head, 0, small, 4) == 4);
    assert(small[1] == 1 && small[4] == 0xEE && small[5] == 0xEE);

    /* A part from an offset is the same as that part of the whole table. */
    uint8_t part[8] = { 0 };
    assert(TRTypefaceGetTableData(typeface, head, 12, part, sizeof(part)) == 8);
    assert(memcmp(part, buffer + 12, sizeof(part)) == 0);

    /* The part is limited by the end of the table, not by FreeType reading past it. */
    uint8_t tail[16] = { 0 };
    assert(TRTypefaceGetTableData(typeface, head, 50, tail, sizeof(tail)) == 4);
    assert(memcmp(tail, buffer + 50, 4) == 0);

    /* Nothing is copied from the end of the table or past it, or without a buffer. */
    assert(TRTypefaceGetTableData(typeface, head, 54, tail, sizeof(tail)) == 0);
    assert(TRTypefaceGetTableData(typeface, head, 1000, tail, sizeof(tail)) == 0);
    assert(TRTypefaceGetTableData(typeface, head, 0, nullptr, 10) == 0);
    assert(TRTypefaceGetTableData(typeface, head, 0, tail, 0) == 0);

    const TRTag missing = TRTagMake('Z', 'Z', 'Z', 'Z');
    assert(TRTypefaceGetTableSize(typeface, missing) == 0);
    assert(TRTypefaceGetTableData(typeface, missing, 0, buffer, sizeof(buffer)) == 0);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testGlyphName() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.names.ttf");
    char buffer[32];

    /* The names come from the post table of the font. */
    assert(TRTypefaceGetGlyphName(typeface, 1, buffer, sizeof(buffer)) == 1);
    assert(string(buffer) == "a");
    assert(TRTypefaceGetGlyphName(typeface, 3, buffer, sizeof(buffer)) == 1);
    assert(string(buffer) == "c");
    assert(TRTypefaceGetGlyphName(typeface, 0, buffer, sizeof(buffer)) == 7);
    assert(string(buffer) == ".notdef");

    /* A name that does not fit is cut short, and still ends with the terminator. */
    assert(TRTypefaceGetGlyphName(typeface, 0, buffer, 4) == 3);
    assert(string(buffer) == ".no");

    /* A glyph that the font does not have gets no name. */
    buffer[0] = 'x';
    assert(TRTypefaceGetGlyphName(typeface, 100, buffer, sizeof(buffer)) == 0);
    assert(buffer[0] == '\0');
    assert(TRTypefaceGetGlyphName(typeface, 1, buffer, 0) == 0);

    TRTypefaceRelease(typeface);

    /* The post table of the other fonts has no names. */
    typeface = createTypeface("Roboto-Regular.abc.ttf");
    buffer[0] = 'x';
    assert(TRTypefaceGetGlyphName(typeface, 1, buffer, sizeof(buffer)) == 0);
    assert(buffer[0] == '\0');

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testGlyphIDs() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.abc.ttf");

    assert(TRTypefaceGetGlyphID(typeface, 'a') == 1);
    assert(TRTypefaceGetGlyphID(typeface, 'b') == 2);
    assert(TRTypefaceGetGlyphID(typeface, 'c') == 3);
    assert(TRTypefaceGetGlyphID(typeface, 'z') == 0);
    assert(TRTypefaceGetGlyphID(typeface, 0x1F600) == 0);
    assert(TRTypefaceGetGlyphID(typeface, 0x10FFFF) == 0);

    /* The font has no variation sequences. */
    assert(TRTypefaceGetVariantGlyphID(typeface, 'a', 0xFE0F) == 0);

    TRTypefaceRelease(typeface);
}

void TypefaceTests::testGlyphAdvance() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.abc.ttf");

    /* At the size of the em square the advance is the one in font units. */
    assert(TRTypefaceGetGlyphAdvance(typeface, 1, 2048.0f, TRFalse) == 1114.0f);
    assert(TRTypefaceGetGlyphAdvance(typeface, 2, 2048.0f, TRFalse) == 1149.0f);
    assert(TRTypefaceGetGlyphAdvance(typeface, 3, 2048.0f, TRFalse) == 1072.0f);
    assert(TRTypefaceGetGlyphAdvance(typeface, 1, 1024.0f, TRFalse) == 557.0f);
    assert(TRTypefaceGetGlyphAdvance(typeface, 1, 0.0f, TRFalse) == 0.0f);

    /* It scales linearly and does not round to pixels. */
    TRFloat small = TRTypefaceGetGlyphAdvance(typeface, 1, 10.0f, TRFalse);
    assert(small > 5.43f && small < 5.44f);

    /* Vertical advance of a font without vertical metrics is still well defined. */
    assert(TRTypefaceGetGlyphAdvance(typeface, 1, 2048.0f, TRTrue) >= 0.0f);

    TRTypefaceRelease(typeface);

    /* It follows the variation coordinates. */
    TRTypefaceRef variable = createTypeface("Roboto-Variable.abc.ttf");
    const TRFloat thin[] = { 100.0f, 100.0f };
    const TRFloat black[] = { 900.0f, 100.0f };
    TRTypefaceRef thinFace = TRTypefaceCreateWithVariation(variable, thin, 2);
    TRTypefaceRef blackFace = TRTypefaceCreateWithVariation(variable, black, 2);

    TRFloat regularAdvance = TRTypefaceGetGlyphAdvance(variable, 2, 2048.0f, TRFalse);
    assert(regularAdvance == 1150.0f);
    assert(TRTypefaceGetGlyphAdvance(thinFace, 2, 2048.0f, TRFalse) != regularAdvance);
    assert(TRTypefaceGetGlyphAdvance(blackFace, 2, 2048.0f, TRFalse) != regularAdvance);
    /* Asking the source again is not affected by the derived typefaces. */
    assert(TRTypefaceGetGlyphAdvance(variable, 2, 2048.0f, TRFalse) == regularAdvance);

    TRTypefaceRelease(blackFace);
    TRTypefaceRelease(thinFace);
    TRTypefaceRelease(variable);
}

static void assertPathWellFormed(const vector<PathEvent> &events) {
    assert(!events.empty());
    assert(events.front().kind == PathEvent::Move);
    assert(events.back().kind == PathEvent::Close);

    /* Every contour starts with a move and ends with a close. */
    bool isOpen = false;
    for (const PathEvent &event : events) {
        if (event.kind == PathEvent::Move) {
            assert(!isOpen);
            isOpen = true;
        } else if (event.kind == PathEvent::Close) {
            assert(isOpen);
            isOpen = false;
        } else {
            assert(isOpen);
        }
    }
    assert(!isOpen);
}

static vector<TRPoint> allPoints(const vector<PathEvent> &events) {
    vector<TRPoint> points;

    for (const PathEvent &event : events) {
        points.insert(points.end(), event.points.begin(), event.points.end());
    }

    return points;
}

void TypefaceTests::testGlyphPath() {
    TRTypefaceRef typeface = createTypeface("Roboto-Regular.abc.ttf");
    TRRect box = TRTypefaceGetBoundingBox(typeface);

    TRPathRef path = TRTypefaceCreateGlyphPath(typeface, 1, 2048.0f);
    assert(path != nullptr);

    vector<PathEvent> events = enumeratePath(path);
    assertPathWellFormed(events);

    /* The y axis points downward, so the glyph sits above the baseline at negative y. */
    vector<TRPoint> points = allPoints(events);
    TRFloat minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (const TRPoint &point : points) {
        minX = min(minX, point.x);
        maxX = max(maxX, point.x);
        minY = min(minY, point.y);
        maxY = max(maxY, point.y);
    }
    assert(minX >= box.origin.x - 8.0f && maxX <= box.origin.x + box.size.width + 8.0f);
    assert(minY >= -(box.origin.y + box.size.height) - 8.0f);
    assert(maxY <= -box.origin.y + 8.0f);
    assert(minY < 0.0f);

    /* The first point is exact: no negative zero leaks out. */
    assert(events[0].points[0].x == 808.0f);
    assert(events[0].points[0].y == 0.0f && !signbit(events[0].points[0].y));

    /* Half the size, half the coordinates. */
    TRPathRef half = TRTypefaceCreateGlyphPath(typeface, 1, 1024.0f);
    vector<TRPoint> halfPoints = allPoints(enumeratePath(half));
    assert(halfPoints.size() == points.size());
    for (size_t i = 0; i < points.size(); i++) {
        assert(fabsf(halfPoints[i].x - points[i].x / 2.0f) < 2.0f);
        assert(fabsf(halfPoints[i].y - points[i].y / 2.0f) < 2.0f);
    }

    /* The transform is applied when the path is enumerated. */
    TRAffineTransform transform = { 2.0f, 0.0f, 0.0f, 3.0f, 10.0f, 20.0f };
    vector<TRPoint> moved = allPoints(enumeratePath(path, &transform));
    assert(moved.size() == points.size());
    for (size_t i = 0; i < points.size(); i++) {
        assert(moved[i].x == points[i].x * 2.0f + 10.0f);
        assert(moved[i].y == points[i].y * 3.0f + 20.0f);
    }

    TRPathRelease(half);
    TRPathRelease(path);

    /* The missing glyph has an outline, and out of range glyphs have none. */
    TRPathRef notdef = TRTypefaceCreateGlyphPath(typeface, 0, 2048.0f);
    assert(notdef != nullptr);
    TRPathRelease(notdef);
    assert(TRTypefaceCreateGlyphPath(typeface, 100, 2048.0f) == nullptr);

    /* A zero or negative size has no outline. */
    assert(TRTypefaceCreateGlyphPath(typeface, 1, 0.0f) == nullptr);
    assert(TRTypefaceCreateGlyphPath(typeface, 1, -5.0f) == nullptr);

    TRTypefaceRelease(typeface);

    /* A variation instance has a different outline. */
    TRTypefaceRef variable = createTypeface("Roboto-Variable.abc.ttf");
    const TRFloat black[] = { 900.0f, 100.0f };
    TRTypefaceRef blackFace = TRTypefaceCreateWithVariation(variable, black, 2);

    TRPathRef regularPath = TRTypefaceCreateGlyphPath(variable, 1, 2048.0f);
    TRPathRef blackPath = TRTypefaceCreateGlyphPath(blackFace, 1, 2048.0f);
    assert(regularPath != nullptr && blackPath != nullptr);

    vector<TRPoint> regularPoints = allPoints(enumeratePath(regularPath));
    vector<TRPoint> blackPoints = allPoints(enumeratePath(blackPath));
    bool isDifferent = regularPoints.size() != blackPoints.size();
    for (size_t i = 0; !isDifferent && i < regularPoints.size(); i++) {
        isDifferent = (regularPoints[i].x != blackPoints[i].x
                       || regularPoints[i].y != blackPoints[i].y);
    }
    assert(isDifferent);

    TRPathRelease(blackPath);
    TRPathRelease(regularPath);
    TRTypefaceRelease(blackFace);
    TRTypefaceRelease(variable);
}

void TypefaceTests::testBitmapTypeface() {
    /* An outline font is scalable, and has no strikes. */
    TRTypefaceRef scalable = createTypeface("Roboto-Regular.abc.ttf");
    assert(TRTypefaceIsScalable(scalable));
    assert(TRTypefaceGetBitmapStrikeCount(scalable) == 0);
    TRTypefaceRelease(scalable);

    /* A font with only bitmaps can still be used, with its metrics taken from its tables. */
    TRTypefaceRef typeface = createTypeface("NotoColorEmoji-CBDT.flags.ttf");
    assert(!TRTypefaceIsScalable(typeface));
    assert(TRTypefaceGetBitmapStrikeCount(typeface) == 1);

    const TRBitmapStrike *strike = TRTypefaceGetBitmapStrikesPtr(typeface);
    assert(strike != nullptr);
    assert(strike->pixelWidth == 109.0f && strike->pixelHeight == 109.0f);

    assert(TRTypefaceGetUnitsPerEM(typeface) == 2048);
    assert(TRTypefaceGetAscent(typeface) > 0);
    assert(TRTypefaceGetDescent(typeface) > 0);
    assert(TRTypefaceGetGlyphCount(typeface) == 18);

    TRRect box = TRTypefaceGetBoundingBox(typeface);
    assert(box.size.width > 0.0f && box.size.height > 0.0f);

    /* The advance is in the units of the font, so it follows the size without a strike. */
    TRFloat small = TRTypefaceGetGlyphAdvance(typeface, 3, 32.0f, TRFalse);
    TRFloat big = TRTypefaceGetGlyphAdvance(typeface, 3, 64.0f, TRFalse);
    assert(small > 0.0f);
    assert(fabsf(big - 2.0f * small) < 0.01f);

    /* There are no outlines to make paths from. */
    assert(TRTypefaceCreateGlyphPath(typeface, 3, 32.0f) == nullptr);

    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    TypefaceTests tests;
    tests.run();

    return 0;
}

#endif
