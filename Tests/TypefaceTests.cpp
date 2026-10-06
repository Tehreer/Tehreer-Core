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

#include <hb.h>

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>
}

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

#ifdef STANDALONE_TESTING

int main() {
    TypefaceTests tests;
    tests.run();

    return 0;
}

#endif
