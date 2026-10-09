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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRShapingResult.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRShapingEngine.h>
#include <Core/AtomicUInt.h>
}

#include "TestTypeface.h"

#include "ShapingEngineTests.h"

using namespace std;
using namespace Tehreer;

void ShapingEngineTests::run() {
    testDefaults();
    testScriptDefaultDirection();
    testInvalidArguments();
    testEmptyText();
    testShapeLatin();
    testEncodings();
    testTypeSize();
    testMissingGlyph();
    testRightToLeft();
    testBackwardOrder();
    testSurrogatePair();
    testCaretEdges();
    testCaretStops();
    testOpenTypeFeatures();
    testVariationTypeface();
    testTypefaceIsRetained();
    testResultOutlivesEngine();
    testEngineIsReusable();
    testConcurrentShaping();
}

/* The units per em of the test font is 2048, so shaping at that size gives font units. */
constexpr TRFloat EmSize = 2048.0f;

static TRShapingEngineRef createEngine(TRTypefaceRef typeface, TRFloat typeSize = EmSize) {
    TRShapingEngineRef engine = TRShapingEngineCreate();
    assert(engine != nullptr);

    TRShapingEngineSetTypeface(engine, typeface);
    TRShapingEngineSetTypeSize(engine, typeSize);

    return engine;
}

static TRShapingResultRef shape16(TRShapingEngineRef engine, const char16_t *text, size_t length) {
    return TRShapingEngineCreateShapingResult(engine, text, length, TRStringEncodingUTF16);
}

static vector<TRGlyphID> glyphIDs(TRShapingResultRef result) {
    const TRGlyphID *ptr = TRShapingResultGetGlyphIDsPtr(result);
    return vector<TRGlyphID>(ptr, ptr + TRShapingResultGetGlyphCount(result));
}

static vector<TRFloat> glyphAdvances(TRShapingResultRef result) {
    const TRFloat *ptr = TRShapingResultGetGlyphAdvancesPtr(result);
    return vector<TRFloat>(ptr, ptr + TRShapingResultGetGlyphCount(result));
}

static vector<TRUInteger> clusterMap(TRShapingResultRef result) {
    const TRUInteger *ptr = TRShapingResultGetClusterMapPtr(result);
    return vector<TRUInteger>(ptr, ptr + TRShapingResultGetCodeUnitCount(result));
}

static vector<TRFloat> caretEdges(TRShapingResultRef result, const vector<TRBoolean> *stops = nullptr) {
    vector<TRFloat> edges(TRShapingResultGetCodeUnitCount(result) + 1, -1.0f);
    TRShapingResultGetCaretEdges(result, stops ? stops->data() : nullptr, edges.data());

    return edges;
}

void ShapingEngineTests::testDefaults() {
    TRShapingEngineRef engine = TRShapingEngineCreate();

    assert(engine != nullptr);
    assert(engine->typeface == nullptr);
    assert(engine->typeSize == 16.0f);
    assert(engine->scriptTag == TRTagMake('D', 'F', 'L', 'T'));
    assert(engine->languageTag == TRTagMake('d', 'f', 'l', 't'));
    assert(engine->writingDirection == TRWritingDirectionLeftToRight);
    assert(engine->shapingOrder == TRShapingOrderForward);
    assert(engine->featureCount == 0);

    TRShapingEngineSetTypeSize(engine, 30.0f);
    TRShapingEngineSetScriptTag(engine, TRTagMake('a', 'r', 'a', 'b'));
    TRShapingEngineSetLanguageTag(engine, TRTagMake('U', 'R', 'D', ' '));
    TRShapingEngineSetWritingDirection(engine, TRWritingDirectionRightToLeft);
    TRShapingEngineSetShapingOrder(engine, TRShapingOrderBackward);

    assert(engine->typeSize == 30.0f);
    assert(engine->scriptTag == TRTagMake('a', 'r', 'a', 'b'));
    assert(engine->languageTag == TRTagMake('U', 'R', 'D', ' '));
    assert(engine->writingDirection == TRWritingDirectionRightToLeft);
    assert(engine->shapingOrder == TRShapingOrderBackward);

    TRShapingEngineRelease(engine);
}

void ShapingEngineTests::testScriptDefaultDirection() {
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('a', 'r', 'a', 'b')) == TRWritingDirectionRightToLeft);
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('h', 'e', 'b', 'r')) == TRWritingDirectionRightToLeft);
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('t', 'h', 'a', 'a')) == TRWritingDirectionRightToLeft);
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('l', 'a', 't', 'n')) == TRWritingDirectionLeftToRight);
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('D', 'F', 'L', 'T')) == TRWritingDirectionLeftToRight);
    assert(TRShapingEngineGetScriptDefaultDirection(TRTagMake('d', 'e', 'v', '2')) == TRWritingDirectionLeftToRight);
}

void ShapingEngineTests::testInvalidArguments() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef noTypeface = TRShapingEngineCreate();
    TRShapingEngineRef engine = createEngine(typeface);

    /* There is nothing to shape with. */
    assert(shape16(noTypeface, u"abc", 3) == nullptr);

    assert(TRShapingEngineCreateShapingResult(engine, nullptr, 3, TRStringEncodingUTF16) == nullptr);
    assert(TRShapingEngineCreateShapingResult(engine, u"abc", 3, 3) == nullptr);
    assert(TRShapingEngineCreateShapingResult(engine, u"abc", 3, 0xFFFF) == nullptr);
    assert(TRShapingEngineCreateShapingResult(engine, u"abc", static_cast<TRUInteger>(INT32_MAX) + 1,
        TRStringEncodingUTF16) == nullptr);

    /* The typeface can be removed again. */
    TRShapingEngineSetTypeface(engine, nullptr);
    assert(shape16(engine, u"abc", 3) == nullptr);

    TRShapingEngineRelease(engine);
    TRShapingEngineRelease(noTypeface);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testEmptyText() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = TRShapingEngineCreateShapingResult(engine, nullptr, 0, TRStringEncodingUTF16);
    assert(result != nullptr);
    assert(TRShapingResultGetCodeUnitCount(result) == 0);
    assert(TRShapingResultGetGlyphCount(result) == 0);

    vector<TRFloat> edges = caretEdges(result);
    assert(edges.size() == 1 && edges[0] == 0.0f);

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testShapeLatin() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert(result != nullptr);

    assert(TRShapingResultGetCodeUnitCount(result) == 3);
    assert(TRShapingResultGetGlyphCount(result) == 3);
    assert(!TRShapingResultIsBackward(result));
    assert(!TRShapingResultIsRTL(result));

    assert((glyphIDs(result) == vector<TRGlyphID>{ 1, 2, 3 }));
    assert((glyphAdvances(result) == vector<TRFloat>{ 1114.0f, 1149.0f, 1072.0f }));
    assert((clusterMap(result) == vector<TRUInteger>{ 0, 1, 2 }));

    const TRPoint *offsets = TRShapingResultGetGlyphOffsetsPtr(result);
    for (size_t i = 0; i < 3; i++) {
        assert(offsets[i].x == 0.0f && offsets[i].y == 0.0f);
    }

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testEncodings() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef utf8 = TRShapingEngineCreateShapingResult(engine, "cab", 3, TRStringEncodingUTF8);
    TRShapingResultRef utf16 = shape16(engine, u"cab", 3);
    TRShapingResultRef utf32 = TRShapingEngineCreateShapingResult(engine, U"cab", 3, TRStringEncodingUTF32);

    assert((glyphIDs(utf8) == vector<TRGlyphID>{ 3, 1, 2 }));
    assert(glyphIDs(utf16) == glyphIDs(utf8));
    assert(glyphIDs(utf32) == glyphIDs(utf8));
    assert(glyphAdvances(utf16) == glyphAdvances(utf8));
    assert(glyphAdvances(utf32) == glyphAdvances(utf8));
    assert((clusterMap(utf8) == vector<TRUInteger>{ 0, 1, 2 }));
    assert(clusterMap(utf32) == clusterMap(utf8));

    /* The code unit count follows the encoding: two bytes for one character in UTF-8. */
    TRShapingResultRef multibyte = TRShapingEngineCreateShapingResult(engine, "a\xC3\xA9" "b", 4, TRStringEncodingUTF8);
    assert(TRShapingResultGetCodeUnitCount(multibyte) == 4);
    assert((glyphIDs(multibyte) == vector<TRGlyphID>{ 1, 0, 2 }));
    assert((clusterMap(multibyte) == vector<TRUInteger>{ 0, 1, 1, 2 }));

    TRShapingResultRelease(multibyte);
    TRShapingResultRelease(utf32);
    TRShapingResultRelease(utf16);
    TRShapingResultRelease(utf8);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testTypeSize() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    TRShapingEngineRef half = createEngine(typeface, 1024.0f);
    TRShapingResultRef result = shape16(half, u"abc", 3);
    assert((glyphAdvances(result) == vector<TRFloat>{ 557.0f, 574.5f, 536.0f }));
    TRShapingResultRelease(result);

    /* Advances are not rounded to whole pixels. */
    TRShapingEngineRef small = createEngine(typeface, 10.0f);
    result = shape16(small, u"a", 1);
    TRFloat advance = glyphAdvances(result)[0];
    assert(fabsf(advance - 1114.0f * 10.0f / 2048.0f) < 1e-4f);
    TRShapingResultRelease(result);

    /* A size that is not positive collapses the glyphs. */
    TRShapingEngineSetTypeSize(small, 0.0f);
    result = shape16(small, u"abc", 3);
    assert(TRShapingResultGetGlyphCount(result) == 3);
    for (TRFloat value : glyphAdvances(result)) {
        assert(value == 0.0f);
    }
    TRShapingResultRelease(result);

    TRShapingEngineSetTypeSize(small, -12.0f);
    result = shape16(small, u"abc", 3);
    for (TRFloat value : glyphAdvances(result)) {
        assert(value == 0.0f);
    }
    TRShapingResultRelease(result);

    TRShapingEngineRelease(small);
    TRShapingEngineRelease(half);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testMissingGlyph() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = shape16(engine, u"azb", 3);
    assert((glyphIDs(result) == vector<TRGlyphID>{ 1, 0, 2 }));
    assert(glyphAdvances(result)[1] == 908.0f);

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testRightToLeft() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);
    TRShapingEngineSetWritingDirection(engine, TRWritingDirectionRightToLeft);

    TRShapingResultRef result = shape16(engine, u"abc", 3);

    /* The glyphs flow with the writing direction, which is the order of the code units here. */
    assert(TRShapingResultIsRTL(result));
    assert(!TRShapingResultIsBackward(result));
    assert((glyphIDs(result) == vector<TRGlyphID>{ 1, 2, 3 }));
    assert((glyphAdvances(result) == vector<TRFloat>{ 1114.0f, 1149.0f, 1072.0f }));
    assert((clusterMap(result) == vector<TRUInteger>{ 0, 1, 2 }));

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testBackwardOrder() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);
    TRShapingEngineSetShapingOrder(engine, TRShapingOrderBackward);

    /* Left-to-right text that is shaped backward flows right to left against its code units. */
    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert(TRShapingResultIsBackward(result));
    assert(TRShapingResultIsRTL(result));
    assert((glyphIDs(result) == vector<TRGlyphID>{ 3, 2, 1 }));
    assert((glyphAdvances(result) == vector<TRFloat>{ 1072.0f, 1149.0f, 1114.0f }));
    assert((clusterMap(result) == vector<TRUInteger>{ 2, 1, 0 }));
    TRShapingResultRelease(result);

    /* With right-to-left direction it is the other way round. */
    TRShapingEngineSetWritingDirection(engine, TRWritingDirectionRightToLeft);
    result = shape16(engine, u"abc", 3);
    assert(TRShapingResultIsBackward(result));
    assert(!TRShapingResultIsRTL(result));
    assert((glyphIDs(result) == vector<TRGlyphID>{ 3, 2, 1 }));
    assert((clusterMap(result) == vector<TRUInteger>{ 2, 1, 0 }));
    TRShapingResultRelease(result);

    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testSurrogatePair() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    /* One character takes two code units, and maps to a single glyph. */
    TRShapingResultRef result = shape16(engine, u"a\U0001F600" "b", 4);
    assert(TRShapingResultGetCodeUnitCount(result) == 4);
    assert((glyphIDs(result) == vector<TRGlyphID>{ 1, 0, 2 }));
    assert((clusterMap(result) == vector<TRUInteger>{ 0, 1, 1, 2 }));

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testCaretEdges() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert((caretEdges(result) == vector<TRFloat>{ 0.0f, 1114.0f, 2263.0f, 3335.0f }));
    TRShapingResultRelease(result);

    /* Right-to-left edges grow toward the first code unit. */
    TRShapingEngineSetWritingDirection(engine, TRWritingDirectionRightToLeft);
    result = shape16(engine, u"abc", 3);
    assert((caretEdges(result) == vector<TRFloat>{ 3335.0f, 2221.0f, 1072.0f, 0.0f }));
    TRShapingResultRelease(result);

    /* The surrogate pair shares the advance of its glyph. */
    TRShapingEngineSetWritingDirection(engine, TRWritingDirectionLeftToRight);
    result = shape16(engine, u"a\U0001F600" "b", 4);
    assert((caretEdges(result) == vector<TRFloat>{ 0.0f, 1114.0f, 1114.0f + 454.0f, 1114.0f + 908.0f, 1114.0f + 908.0f + 1149.0f }));
    TRShapingResultRelease(result);

    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testCaretStops() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = shape16(engine, u"a\U0001F600" "b", 4);

    /* Allowing a stop at every code unit changes nothing. */
    vector<TRBoolean> all(4, TRTrue);
    assert(caretEdges(result, &all) == caretEdges(result));

    /* The expected edges were produced by the original Swift algorithm for the same input. */
    vector<TRBoolean> noMiddle = { TRTrue, TRTrue, TRFalse, TRTrue };
    assert((caretEdges(result, &noMiddle) == vector<TRFloat>{ 0.0f, 1114.0f, 2142.5f, 2142.5f, 3171.0f }));

    vector<TRBoolean> noSecond = { TRTrue, TRFalse, TRTrue, TRTrue };
    assert((caretEdges(result, &noSecond) == vector<TRFloat>{ 0.0f, 1114.0f, 1114.0f, 2022.0f, 3171.0f }));

    TRShapingResultRelease(result);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testOpenTypeFeatures() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef plain = shape16(engine, u"abc", 3);

    TROpenTypeFeature features[] = {
        { TRTagMake('l', 'i', 'g', 'a'), 0 },
        { TRTagMake('k', 'e', 'r', 'n'), 0 }
    };
    assert(TRShapingEngineSetOpenTypeFeatures(engine, features, 2));
    assert(engine->featureCount == 2);

    /* The features are copied. */
    features[0].tag = 0;
    features[1].value = 7;
    assert(engine->features[0].tag == TRTagMake('l', 'i', 'g', 'a'));
    assert(engine->features[1].value == 0);

    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert(result != nullptr);
    assert(glyphIDs(result) == glyphIDs(plain));
    TRShapingResultRelease(result);

    /* Replacing and removing them. */
    TROpenTypeFeature one[] = { { TRTagMake('k', 'e', 'r', 'n'), 1 } };
    assert(TRShapingEngineSetOpenTypeFeatures(engine, one, 1));
    assert(engine->featureCount == 1);
    assert(engine->features[0].value == 1);

    assert(TRShapingEngineSetOpenTypeFeatures(engine, nullptr, 0));
    assert(engine->featureCount == 0);
    assert(engine->features == nullptr);

    result = shape16(engine, u"abc", 3);
    assert(glyphAdvances(result) == glyphAdvances(plain));
    TRShapingResultRelease(result);

    TRShapingResultRelease(plain);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testVariationTypeface() {
    const TRFloat thin[] = { 100.0f, 100.0f };
    const TRFloat black[] = { 900.0f, 100.0f };
    TRTypefaceRef regularFace = createTestTypeface("Roboto-Variable.abc.ttf");
    TRTypefaceRef thinFace = createTestTypeface("Roboto-Variable.abc.ttf", thin);
    TRTypefaceRef blackFace = createTestTypeface("Roboto-Variable.abc.ttf", black);

    /* The derived typefaces of the public API shape the same way. */
    TRTypefaceRef derivedFace = TRTypefaceCreateWithVariation(regularFace, black, 2);

    TRShapingEngineRef engine = createEngine(regularFace);

    TRShapingResultRef regular = shape16(engine, u"abc", 3);
    assert(glyphAdvances(regular)[1] == 1150.0f);

    TRShapingEngineSetTypeface(engine, thinFace);
    TRShapingResultRef thinResult = shape16(engine, u"abc", 3);

    TRShapingEngineSetTypeface(engine, blackFace);
    TRShapingResultRef blackResult = shape16(engine, u"abc", 3);

    TRShapingEngineSetTypeface(engine, derivedFace);
    TRShapingResultRef derivedResult = shape16(engine, u"abc", 3);

    assert(glyphAdvances(thinResult) != glyphAdvances(regular));
    assert(glyphAdvances(blackResult) != glyphAdvances(regular));
    assert(glyphAdvances(thinResult) != glyphAdvances(blackResult));
    assert(glyphAdvances(derivedResult) == glyphAdvances(blackResult));
    assert(glyphIDs(derivedResult) == glyphIDs(regular));

    /* The advances agree with the ones the typeface reports for the same coordinates. */
    for (size_t i = 0; i < 3; i++) {
        TRGlyphID glyph = glyphIDs(blackResult)[i];
        assert(fabsf(glyphAdvances(blackResult)[i] - TRTypefaceGetGlyphAdvance(blackFace, glyph, EmSize, TRFalse)) < 1e-3f);
    }

    TRShapingResultRelease(derivedResult);
    TRShapingResultRelease(blackResult);
    TRShapingResultRelease(thinResult);
    TRShapingResultRelease(regular);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(derivedFace);
    TRTypefaceRelease(blackFace);
    TRTypefaceRelease(thinFace);
    TRTypefaceRelease(regularFace);
}

void ShapingEngineTests::testTypefaceIsRetained() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRTypefaceRef other = createTestTypeface("Roboto-Variable.abc.ttf");
    TRShapingEngineRef engine = TRShapingEngineCreate();

    TRShapingEngineSetTypeface(engine, typeface);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 2);

    /* Setting the same one again does not unbalance the count. */
    TRShapingEngineSetTypeface(engine, typeface);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 2);

    TRShapingEngineSetTypeface(engine, other);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 1);
    assert(AtomicUIntLoad(&other->_base.retainCount) == 2);

    /* The engine keeps working after the caller lets go of the typeface. */
    TRTypefaceRelease(other);
    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert(TRShapingResultGetGlyphCount(result) == 3);
    TRShapingResultRelease(result);

    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testResultOutlivesEngine() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef result = shape16(engine, u"abc", 3);
    assert(TRShapingResultRetain(result) == result);

    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
    TRShapingResultRelease(result);

    assert((glyphIDs(result) == vector<TRGlyphID>{ 1, 2, 3 }));
    assert((caretEdges(result) == vector<TRFloat>{ 0.0f, 1114.0f, 2263.0f, 3335.0f }));

    TRShapingResultRelease(result);
}

void ShapingEngineTests::testEngineIsReusable() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRShapingEngineRef engine = createEngine(typeface);

    TRShapingResultRef first = shape16(engine, u"abc", 3);
    TRShapingResultRef second = shape16(engine, u"cb", 2);
    TRShapingResultRef third = shape16(engine, u"abc", 3);

    /* Earlier results are not touched by later shaping. */
    assert((glyphIDs(first) == vector<TRGlyphID>{ 1, 2, 3 }));
    assert((glyphIDs(second) == vector<TRGlyphID>{ 3, 2 }));
    assert(glyphIDs(third) == glyphIDs(first));
    assert(first != third);

    TRShapingResultRelease(third);
    TRShapingResultRelease(second);
    TRShapingResultRelease(first);
    TRShapingEngineRelease(engine);
    TRTypefaceRelease(typeface);
}

void ShapingEngineTests::testConcurrentShaping() {
    const TRFloat coordinates[] = { 700.0f, 100.0f };
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf", coordinates);

    TRShapingEngineRef reference = createEngine(typeface);
    TRShapingResultRef expected = shape16(reference, u"abcabc", 6);

    vector<thread> threads;
    for (size_t i = 0; i < 8; i++) {
        threads.emplace_back([typeface, expected]() {
            TRShapingEngineRef engine = createEngine(typeface);

            for (size_t j = 0; j < 200; j++) {
                TRShapingResultRef result = shape16(engine, u"abcabc", 6);

                assert(glyphIDs(result) == glyphIDs(expected));
                assert(glyphAdvances(result) == glyphAdvances(expected));
                assert(caretEdges(result) == caretEdges(expected));

                TRShapingResultRelease(result);
            }

            TRShapingEngineRelease(engine);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    TRShapingResultRelease(expected);
    TRShapingEngineRelease(reference);
    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    ShapingEngineTests tests;
    tests.run();

    return 0;
}

#endif
