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
#include <string>
#include <vector>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

extern "C" {
#include <API/TRTypesetter.h>
#include <Core/AtomicUInt.h>
#include <Layout/TextRun.h>
}

#include "TestText.h"
#include "TestTypeface.h"

#include "TypesetterTests.h"

using namespace std;
using namespace Tehreer;

void TypesetterTests::run() {
    testCreateInvalid();
    testDefaultAttributes();
    testSingleRun();
    testEmptyText();
    testRunsFollowShapingAttributes();
    testPaintAttributesDoNotSplitRuns();
    testScaleAndBaselineOffset();
    testBidirectionalRuns();
    testParagraphs();
    testRightToLeftParagraph();
    testReplacementRuns();
    testBlockReplacements();
    testTextIsCopied();
    testFindRunsAndParagraphs();
    testMeasureRange();
    testRunQueries();
    testEncodings();
    testUnknownScriptsAndNotdef();
}

/*
 * In the test font, the glyphs 'a', 'b' and 'c' are 1114, 1149 and 1072 units wide, and every other
 * character shows the notdef glyph that is 908 units wide. The units per em are 2048, so a point
 * size of 2048 gives these widths as they are.
 */
constexpr TRFloat EmSize = 2048.0f;
constexpr TRFloat A = 1114.0f;
constexpr TRFloat B = 1149.0f;
constexpr TRFloat C = 1072.0f;
constexpr TRFloat Notdef = 908.0f;

static TypesetterRef create(TRTextRef text, const TRAttribute *defaults = nullptr, TRUInteger count = 0) {
    return const_cast<TypesetterRef>(TRTypesetterCreate(text, defaults, count));
}

static vector<TRFloat> advancesOf(TextRunRef run) {
    return vector<TRFloat>(run->glyphAdvances, run->glyphAdvances + run->glyphCount);
}

void TypesetterTests::testCreateInvalid() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    assert(TRTypesetterCreate(nullptr, nullptr, 0) == nullptr);

    /* Text with no typeface at all cannot be shaped. */
    TRMutableTextRef noTypeface = makeTestText(u"abc");
    assert(TRTypesetterCreate(noTypeface, nullptr, 0) == nullptr);

    /* One part of it without a typeface is enough to fail. */
    TRMutableTextRef partial = makeTestText(u"abc");
    TRAttribute attribute = {};
    attribute.type = TRAttributeTypeface;
    attribute.value.typeface = typeface;
    TRTextSetAttribute(partial, 0, 2, &attribute);
    assert(TRTypesetterCreate(partial, nullptr, 0) == nullptr);

    /* The default attributes can supply it. */
    assert(TRTypesetterCreate(partial, &attribute, 1) != nullptr);

    TRTextRelease(partial);
    TRTextRelease(noTypeface);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testDefaultAttributes() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abc");

    TRAttribute defaults[2] = {};
    defaults[0].type = TRAttributeTypeface;
    defaults[0].value.typeface = typeface;
    defaults[1].type = TRAttributePointSize;
    defaults[1].value.pointSize = EmSize;

    TypesetterRef typesetter = create(text, defaults, 2);
    assert(typesetter != nullptr);
    assert(typesetter->runCount == 1);
    assert(typesetter->runs[0]->typeSize == EmSize);
    assert((advancesOf(typesetter->runs[0]) == vector<TRFloat>{ A, B, C }));
    TRTypesetterRelease(typesetter);

    /* Without a size, it is 16, and what the text sets wins over the defaults. */
    defaults[1].value.pointSize = 0.0f;
    TRTextRelease(text);
    text = makeTestText(u"abc");
    typesetter = create(text, defaults, 1);
    assert(typesetter->runs[0]->typeSize == 16.0f);
    TRTypesetterRelease(typesetter);

    setTestFloat(text, 1, 2, TRAttributePointSize, 100.0f);
    TRAttribute onlyTypeface = defaults[0];
    typesetter = create(text, &onlyTypeface, 1);
    assert(typesetter->runCount == 2);
    assert(typesetter->runs[0]->typeSize == 16.0f);
    assert(typesetter->runs[1]->typeSize == 100.0f);
    TRTypesetterRelease(typesetter);

    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testSingleRun() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abc", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    assert(typesetter != nullptr);
    assert(TRTypesetterGetCodeUnitCount(typesetter) == 3);

    assert(typesetter->paragraphCount == 1);
    assert(typesetter->paragraphs[0].start == 0 && typesetter->paragraphs[0].end == 3);
    assert(typesetter->paragraphs[0].baseLevel == 0);

    assert(typesetter->runCount == 1);
    TextRunRef run = typesetter->runs[0];
    assert(run->kind == TextRunKindIntrinsic);
    assert(run->codeUnitStart == 0 && run->codeUnitEnd == 3);
    assert(run->bidiLevel == 0 && !run->isBackward);
    assert(run->writingDirection == TRWritingDirectionLeftToRight);
    assert(run->typeface == typeface && run->typeSize == EmSize);

    /* The metrics of the typeface at the size of the em square are the ones of its units. */
    assert(run->ascent == 1900.0f && run->descent == 500.0f && run->leading == 0.0f);

    assert(run->glyphCount == 3);
    assert((vector<TRGlyphID>(run->glyphIDs, run->glyphIDs + 3) == vector<TRGlyphID>{ 1, 2, 3 }));
    assert((advancesOf(run) == vector<TRFloat>{ A, B, C }));
    assert((vector<TRUInteger>(run->clusterMap, run->clusterMap + 3) == vector<TRUInteger>{ 0, 1, 2 }));
    assert((vector<TRFloat>(run->caretEdges, run->caretEdges + 4) == vector<TRFloat>{ 0, A, A + B, A + B + C }));
    assert(TextRunGetWidth(run) == A + B + C);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testEmptyText() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    assert(typesetter != nullptr);
    assert(TRTypesetterGetCodeUnitCount(typesetter) == 0);
    assert(typesetter->paragraphCount == 0 && typesetter->runCount == 0);
    assert(TypesetterFindRun(typesetter, 0) == TRInvalidIndex);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testRunsFollowShapingAttributes() {
    TRTypefaceRef regular = createTestTypeface("Roboto-Regular.abc.ttf");
    TRTypefaceRef other = createTestTypeface("Roboto-Variable.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abcabcabc", regular, EmSize);

    /* A size, a scale, an offset and an obliqueness each have their own runs. */
    setTestFloat(text, 3, 1, TRAttributePointSize, 1024.0f);
    setTestFloat(text, 4, 1, TRAttributeScaleX, 2.0f);
    setTestFloat(text, 5, 1, TRAttributeScaleY, 2.0f);
    setTestFloat(text, 6, 1, TRAttributeBaselineOffset, 5.0f);
    setTestFloat(text, 7, 1, TRAttributeObliqueness, 0.2f);

    TRAttribute typefaceAttribute = {};
    typefaceAttribute.type = TRAttributeTypeface;
    typefaceAttribute.value.typeface = other;
    TRTextSetAttribute(text, 8, 1, &typefaceAttribute);

    TypesetterRef typesetter = create(text);
    assert(typesetter != nullptr);
    assert(typesetter->runCount == 7);

    const size_t starts[] = { 0, 3, 4, 5, 6, 7, 8 };
    for (size_t i = 0; i < 7; i++) {
        assert(typesetter->runs[i]->codeUnitStart == starts[i]);
    }
    assert(typesetter->runs[6]->typeface == other);
    assert(typesetter->runs[1]->typeSize == 1024.0f);

    TRTypesetterRelease(typesetter);

    /* Equal values on neighbors are shaped together, even if they were set one by one. */
    TRMutableTextRef merged = makeTestText(u"abcabc", regular, EmSize);
    setTestFloat(merged, 0, 2, TRAttributeScaleX, 1.5f);
    setTestFloat(merged, 2, 2, TRAttributeScaleX, 1.5f);
    setTestFloat(merged, 4, 2, TRAttributeScaleX, 1.5f);

    typesetter = create(merged);
    assert(typesetter->runCount == 1);
    assert(typesetter->runs[0]->scaleX == 1.5f);
    TRTypesetterRelease(typesetter);

    /* A value that is the same as the default does not split either. */
    TRMutableTextRef same = makeTestText(u"abcabc", regular, EmSize);
    setTestFloat(same, 2, 2, TRAttributeScaleX, 1.0f);
    setTestFloat(same, 4, 1, TRAttributePointSize, EmSize);

    typesetter = create(same);
    assert(typesetter->runCount == 1);
    TRTypesetterRelease(typesetter);

    TRTextRelease(same);
    TRTextRelease(merged);
    TRTextRelease(text);
    TRTypefaceRelease(other);
    TRTypefaceRelease(regular);
}

void TypesetterTests::testPaintAttributesDoNotSplitRuns() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abcabc", typeface, EmSize);
    int first = 1;
    int second = 2;

    TRAttribute color = {};
    color.type = TRAttributeForegroundColor;
    color.value.foregroundColor = TRColorMake(0xFF, 0xFF, 0x00, 0x00);
    TRTextSetAttribute(text, 1, 2, &color);

    TRAttribute userData = {};
    userData.type = TRAttributeUserData;
    userData.value.userData = &first;
    TRTextSetAttribute(text, 0, 3, &userData);
    userData.value.userData = &second;
    TRTextSetAttribute(text, 3, 3, &userData);

    /* The text is shaped as a whole, since colors and user data do not change glyphs. */
    TypesetterRef typesetter = create(text);
    assert(typesetter->runCount == 1);
    assert(typesetter->runs[0]->codeUnitStart == 0 && typesetter->runs[0]->codeUnitEnd == 6);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testScaleAndBaselineOffset() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"ab", typeface, EmSize);
    setTestFloat(text, 0, 1, TRAttributeScaleX, 2.0f);
    setTestFloat(text, 0, 1, TRAttributeScaleY, 0.5f);
    setTestFloat(text, 1, 1, TRAttributeBaselineOffset, 7.0f);

    TypesetterRef typesetter = create(text);
    assert(typesetter->runCount == 2);

    TextRunRef scaled = typesetter->runs[0];
    assert(scaled->glyphAdvances[0] == A * 2.0f);
    assert(scaled->glyphOffsets[0].x == 0.0f && scaled->glyphOffsets[0].y == 0.0f);
    /* The vertical scale applies to the metrics. */
    assert(scaled->ascent == 950.0f && scaled->descent == 250.0f);
    assert(scaled->scaleX == 2.0f && scaled->scaleY == 0.5f);
    assert(scaled->caretEdges[1] == A * 2.0f);

    /* The baseline offset raises the glyphs. */
    TextRunRef raised = typesetter->runs[1];
    assert(raised->glyphAdvances[0] == B);
    assert(raised->glyphOffsets[0].y == 7.0f);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testBidirectionalRuns() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    /* "ab " then four Hebrew letters, then " cd". */
    TRMutableTextRef text = makeTestText(u"ab \u05E9\u05DC\u05D5\u05DD cd", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    assert(typesetter->paragraphCount == 1);
    assert(typesetter->paragraphs[0].baseLevel == 0);
    assert(typesetter->runCount == 4);

    TextRunRef latin = typesetter->runs[0];
    assert(latin->codeUnitStart == 0 && latin->codeUnitEnd == 3 && latin->bidiLevel == 0);
    assert(!latin->isBackward && latin->writingDirection == TRWritingDirectionLeftToRight);

    /* Hebrew is shaped right to left at an odd level, which flows with the direction. */
    TextRunRef hebrew = typesetter->runs[1];
    assert(hebrew->codeUnitStart == 3 && hebrew->codeUnitEnd == 7 && hebrew->bidiLevel == 1);
    assert(!hebrew->isBackward && hebrew->writingDirection == TRWritingDirectionRightToLeft);
    assert(TextRunIsRTL(hebrew));
    assert(hebrew->glyphCount == 4);

    /* The runs cover the text without gaps. */
    assert(typesetter->runs[2]->codeUnitStart == 7);
    assert(typesetter->runs[3]->codeUnitEnd == 10);
    for (size_t i = 1; i < typesetter->runCount; i++) {
        assert(typesetter->runs[i]->codeUnitStart == typesetter->runs[i - 1]->codeUnitEnd);
    }

    /* A run at an even level whose script goes right to left is shaped backward. */
    TextRunRef space = typesetter->runs[2];
    assert(space->bidiLevel == 0);
    assert(space->isBackward == (space->writingDirection == TRWritingDirectionRightToLeft));

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testParagraphs() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"ab\ncd\n\nef", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    /* A newline belongs to the paragraph that it ends, and runs do not cross paragraphs. */
    assert(typesetter->paragraphCount == 4);
    const size_t starts[] = { 0, 3, 6, 7 };
    const size_t ends[] = { 3, 6, 7, 9 };
    for (size_t i = 0; i < 4; i++) {
        assert(typesetter->paragraphs[i].start == starts[i]);
        assert(typesetter->paragraphs[i].end == ends[i]);
    }

    assert(typesetter->runCount == 4);
    for (size_t i = 0; i < 4; i++) {
        assert(typesetter->runs[i]->codeUnitStart == starts[i]);
        assert(typesetter->runs[i]->codeUnitEnd == ends[i]);
    }

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testRightToLeftParagraph() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"\u05E9\u05DC ab\nx", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    /* The base level comes from the first strong character of each paragraph. */
    assert(typesetter->paragraphCount == 2);
    assert(typesetter->paragraphs[0].baseLevel == 1);
    assert(typesetter->paragraphs[1].baseLevel == 0);

    /* The Latin letters are embedded at the level above the base. */
    TextRunRef latin = typesetter->runs[1];
    assert(latin->codeUnitStart == 3 && latin->codeUnitEnd == 5 && latin->bidiLevel == 2);
    assert(!latin->isBackward);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

namespace {

struct ReplacementState {
    TRFloat width = 0.0f;
    int roomCalls = 0;
    TRFloat lastLayoutWidth = -1.0f;
};

void computeRoom(void *userData, TRFloat layoutWidth, TRReplacementRoom *room) {
    auto *state = static_cast<ReplacementState *>(userData);
    state->roomCalls++;
    state->lastLayoutWidth = layoutWidth;

    room->ascent = 30.0f;
    room->descent = 6.0f;
    room->extent = state->width;
}

TRReplacementRef createReplacement(ReplacementState *state, bool isBlock, TRFloat leading = 0.0f) {
    TRReplacementCallbacks callbacks = { computeRoom, nullptr };
    return TRReplacementCreate(&callbacks, state, leading, isBlock);
}

}

void TypesetterTests::testReplacementRuns() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    ReplacementState state;
    state.width = 50.0f;
    TRReplacementRef replacement = createReplacement(&state, false, 4.0f);

    TRMutableTextRef text = makeTestText(u"a\uFFFCb", typeface, EmSize);
    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTextSetAttribute(text, 1, 1, &attribute);

    TypesetterRef typesetter = create(text);
    assert(typesetter->runCount == 3);
    assert(typesetter->blockCount == 0);

    TextRunRef run = typesetter->runs[1];
    assert(run->kind == TextRunKindReplacement);
    assert(run->codeUnitStart == 1 && run->codeUnitEnd == 2);
    assert(run->replacement == replacement);

    /* The room is asked for when the text is typeset, without any layout width. */
    assert(state.roomCalls == 1 && state.lastLayoutWidth == 0.0f);
    assert(run->ascent == 30.0f && run->descent == 6.0f && run->leading == 4.0f);
    assert(run->extent == 50.0f && TextRunGetWidth(run) == 50.0f);

    /* It has a single glyph, which is a space as wide as the replacement. */
    assert(run->glyphCount == 1);
    assert(run->glyphIDs[0] == TRTypefaceGetGlyphID(typeface, 0x20));
    assert(run->glyphAdvances[0] == 50.0f);
    assert(run->clusterMap[0] == 0);

    /* Its clusters and glyphs are the whole run. */
    assert(TextRunGetClusterStart(run, 1) == 1 && TextRunGetClusterEnd(run, 1) == 2);
    assert(TextRunGetLeadingGlyphIndex(run, 1) == 0 && TextRunGetTrailingGlyphIndex(run, 1) == 0);
    TRUInteger glyphStart, glyphEnd;
    TextRunGetGlyphRange(run, 1, 2, &glyphStart, &glyphEnd);
    assert(glyphStart == 0 && glyphEnd == 1);

    /* The extent is at the end of the run in its direction. */
    assert(run->caretEdges[0] == 0.0f && run->caretEdges[1] == 50.0f);

    /* For another layout width, the run takes the room that is asked for. */
    state.width = 80.0f;
    TextRunRef forFrame = TextRunCreateForLayoutWidth(run, 120.0f);
    assert(state.lastLayoutWidth == 120.0f);
    assert(forFrame->extent == 80.0f && forFrame->leading == 0.0f);
    assert(forFrame->replacement == replacement);
    assert(run->extent == 50.0f);
    TextRunRelease(forFrame);

    /* A run that is not a replacement is returned as it is. */
    TextRunRef same = TextRunCreateForLayoutWidth(typesetter->runs[0], 120.0f);
    assert(same == typesetter->runs[0]);
    TextRunRelease(same);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRReplacementRelease(replacement);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testBlockReplacements() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    ReplacementState state;
    TRReplacementRef inlineOne = createReplacement(&state, false);
    TRReplacementRef block = createReplacement(&state, true);

    TRMutableTextRef text = makeTestText(u"a\uFFFCb\uFFFCc", typeface, EmSize);
    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = inlineOne;
    TRTextSetAttribute(text, 1, 1, &attribute);
    attribute.value.replacement = block;
    TRTextSetAttribute(text, 3, 1, &attribute);

    TypesetterRef typesetter = create(text);
    assert(typesetter->runCount == 5);
    assert(typesetter->blockCount == 1);
    assert(TextRunIsBlock(typesetter->blocks[0]));
    assert(typesetter->blocks[0] == typesetter->runs[3]);
    assert(!TextRunIsBlock(typesetter->runs[1]));
    assert(!TextRunIsBlock(typesetter->runs[0]));

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRReplacementRelease(block);
    TRReplacementRelease(inlineOne);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testTextIsCopied() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abc", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    /* Changing the text afterwards does not change what was typeset. */
    TRTextReplaceCodeUnits(text, 0, 3, u"cba cba", 7);
    assert(TRTextGetLength(text) == 7);
    assert(TRTypesetterGetCodeUnitCount(typesetter) == 3);
    assert(typesetter->runCount == 1);
    assert(typesetter->runs[0]->glyphIDs[0] == 1);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testFindRunsAndParagraphs() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"ab\ncde\nf", typeface, EmSize);
    setTestFloat(text, 4, 1, TRAttributePointSize, 1024.0f);
    TypesetterRef typesetter = create(text);

    /* Paragraphs are [0, 3), [3, 7) and [7, 8), and the second one has three runs. */
    assert(typesetter->paragraphCount == 3);
    const size_t paragraphOf[] = { 0, 0, 0, 1, 1, 1, 1, 2 };
    for (size_t i = 0; i < 8; i++) {
        assert(TypesetterFindParagraph(typesetter, i) == paragraphOf[i]);
    }
    assert(TypesetterFindParagraph(typesetter, 8) == TRInvalidIndex);

    assert(typesetter->runCount == 5);
    const size_t runOf[] = { 0, 0, 0, 1, 2, 3, 3, 4 };
    for (size_t i = 0; i < 8; i++) {
        assert(TypesetterFindRun(typesetter, i) == runOf[i]);
    }
    assert(TypesetterFindRun(typesetter, 8) == TRInvalidIndex);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testMeasureRange() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abcabc", typeface, EmSize);
    setTestFloat(text, 3, 3, TRAttributePointSize, 1024.0f);
    TypesetterRef typesetter = create(text);

    assert(typesetter->runCount == 2);
    assert(TypesetterMeasureRange(typesetter, 0, 0) == 0.0f);
    assert(TypesetterMeasureRange(typesetter, 0, 1) == A);
    assert(TypesetterMeasureRange(typesetter, 1, 3) == B + C);
    assert(TypesetterMeasureRange(typesetter, 0, 3) == A + B + C);
    assert(TypesetterMeasureRange(typesetter, 3, 6) == (A + B + C) / 2.0f);

    /* A range across two runs adds up what each of them has in it. */
    assert(TypesetterMeasureRange(typesetter, 2, 5) == C + (A + B) / 2.0f);
    assert(TypesetterMeasureRange(typesetter, 0, 6) == (A + B + C) * 1.5f);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testRunQueries() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = makeTestText(u"abc", typeface, EmSize);
    TypesetterRef typesetter = create(text);
    TextRunRef run = typesetter->runs[0];

    /* One glyph for each code unit, so each cluster is a single code unit. */
    for (size_t i = 0; i < 3; i++) {
        assert(TextRunGetClusterStart(run, i) == i);
        assert(TextRunGetClusterEnd(run, i) == i + 1);
        assert(TextRunGetLeadingGlyphIndex(run, i) == i);
        assert(TextRunGetTrailingGlyphIndex(run, i) == i);
        assert(TextRunGetCaretEdge(run, i) == (i == 0 ? 0.0f : (i == 1 ? A : A + B)));
    }
    assert(TextRunGetCaretEdge(run, 3) == A + B + C);

    TRUInteger glyphStart, glyphEnd;
    TextRunGetGlyphRange(run, 1, 3, &glyphStart, &glyphEnd);
    assert(glyphStart == 1 && glyphEnd == 3);
    TextRunGetGlyphRange(run, 0, 3, &glyphStart, &glyphEnd);
    assert(glyphStart == 0 && glyphEnd == 3);

    assert(TextRunGetDistance(run, 1, 3) == B + C);
    assert(TextRunGetCaretBoundary(run, 1, 3) == A);

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testEncodings() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    TRAttribute attributes[2] = {};
    attributes[0].type = TRAttributeTypeface;
    attributes[0].value.typeface = typeface;
    attributes[1].type = TRAttributePointSize;
    attributes[1].value.pointSize = EmSize;

    /* The same text in each encoding is shaped alike, and counts its own code units. */
    TRTextRef utf8 = TRTextCreate("ab\xC3\xA9" "c", 5, TRStringEncodingUTF8);
    TRTextRef utf16 = TRTextCreate(u"ab\u00E9" "c", 4, TRStringEncodingUTF16);
    TRTextRef utf32 = TRTextCreate(U"ab\u00E9" "c", 4, TRStringEncodingUTF32);

    TypesetterRef byBytes = create(utf8, attributes, 2);
    TypesetterRef byUnits = create(utf16, attributes, 2);
    TypesetterRef byWords = create(utf32, attributes, 2);

    assert(TRTypesetterGetCodeUnitCount(byBytes) == 5);
    assert(TRTypesetterGetCodeUnitCount(byUnits) == 4);
    assert(TRTypesetterGetCodeUnitCount(byWords) == 4);

    const vector<TRFloat> expected = { A, B, Notdef, C };
    assert(advancesOf(byBytes->runs[0]) == expected);
    assert(advancesOf(byUnits->runs[0]) == expected);
    assert(advancesOf(byWords->runs[0]) == expected);

    /* Each code unit of the bytes has an entry in the cluster map. */
    assert((vector<TRUInteger>(byBytes->runs[0]->clusterMap, byBytes->runs[0]->clusterMap + 5)
            == vector<TRUInteger>{ 0, 1, 2, 2, 3 }));

    TRTypesetterRelease(byWords);
    TRTypesetterRelease(byUnits);
    TRTypesetterRelease(byBytes);
    TRTextRelease(utf32);
    TRTextRelease(utf16);
    TRTextRelease(utf8);
    TRTypefaceRelease(typeface);
}

void TypesetterTests::testUnknownScriptsAndNotdef() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");

    /* Characters of scripts that the font has nothing for still get runs and metrics. */
    TRMutableTextRef text = makeTestText(u"\u0627\u0644\u0639\u0631\u0628\u064A \u0939\u093F\u0928\u094D\u0926\u0940 \u4E2D\u6587", typeface, EmSize);
    TypesetterRef typesetter = create(text);

    assert(typesetter != nullptr);
    assert(typesetter->runCount >= 3);
    for (size_t i = 0; i < typesetter->runCount; i++) {
        TextRunRef run = typesetter->runs[i];
        assert(run->glyphCount > 0);
        assert(TextRunGetWidth(run) > 0.0f);
        assert(run->codeUnitEnd > run->codeUnitStart);
    }

    /* The runs cover the text exactly once. */
    assert(typesetter->runs[0]->codeUnitStart == 0);
    assert(typesetter->runs[typesetter->runCount - 1]->codeUnitEnd == TRTextGetLength(text));

    TRTypesetterRelease(typesetter);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    TypesetterTests tests;
    tests.run();

    return 0;
}

#endif
