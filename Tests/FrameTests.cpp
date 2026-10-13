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
#include <iostream>
#include <string>
#include <vector>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRFrameResolver.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

#include "TestText.h"
#include "TestTypeface.h"

#include "FrameTests.h"

using namespace std;
using namespace Tehreer;

static TRSize makeSize(TRFloat width, TRFloat height)
{
    TRSize size = { width, height };

    return size;
}

void FrameTests::run() {
    testSingleLine();
    testFitting();
    testWrapping();
    testFrameRange();
    testHeightLimit();
    testMaxLines();
    testTruncation();
    testTruncationAcrossParagraphs();
    testAlignments();
    testIndents();
    testParagraphSpacing();
    testLineHeights();
    testJustification();
    testRightToLeft();
    testIndexOfLine();
    testHitTest();
    testHitTestRightToLeft();
    testHitTestMixedDirections();
    testHitTestClusters();
    testHitTestTruncatedLine();
    testSelection();
    testRangesAreChecked();
    testFramesOutliveResolver();
}

/*
 * In the test font, the glyphs 'a', 'b' and 'c' are 1114, 1149 and 1072 units wide, and every other
 * character, including the space, shows the notdef glyph that is 908 units wide. At a point size of
 * 2048 these are the widths as they are, with an ascent of 1900 and a descent of 500.
 */
constexpr TRFloat EmSize = 2048.0f;
constexpr TRFloat A = 1114.0f;
constexpr TRFloat B = 1149.0f;
constexpr TRFloat C = 1072.0f;
constexpr TRFloat Notdef = 908.0f;
constexpr TRFloat Ascent = 1900.0f;
constexpr TRFloat Height = 2400.0f;

static bool isNear(TRFloat a, TRFloat b) {
    return fabsf(a - b) < 0.01f;
}

struct Fixture {
    TRTypefaceRef typeface;
    TRMutableTextRef text;
    TRTypesetterRef typesetter;
    TRFrameResolverRef resolver;

    explicit Fixture(const u16string &string) {
        typeface = createTestTypeface("Roboto-Regular.abc.ttf");
        text = makeTestText(string, typeface, EmSize);
        typesetter = TRTypesetterCreate(text, nullptr, 0);
        assert(typesetter != nullptr);

        resolver = TRFrameResolverCreate();
        assert(resolver != nullptr);
        TRFrameResolverSetTypesetter(resolver, typesetter);
    }

    TRComposedFrameRef frame(TRUInteger start, TRUInteger end) {
        TRComposedFrameRef result = TRFrameResolverCreateFrame(resolver, start, end - start);
        assert(result != nullptr);

        return result;
    }

    TRComposedFrameRef frame() {
        return frame(0, TRTextGetLength(text));
    }

    ~Fixture() {
        TRFrameResolverRelease(resolver);
        TRTypesetterRelease(typesetter);
        TRTextRelease(text);
        TRTypefaceRelease(typeface);
    }
};

static void setParagraphCount(TRMutableTextRef text, TRAttributeType type, TRUInteger value) {
    TRAttribute attribute = {};
    attribute.type = type;
    attribute.value.firstIndentLineCount = value;
    TRTextSetAttribute(text, 0, TRTextGetLength(text), &attribute);
}

static void setAlignment(TRMutableTextRef text, TRTextAlignment alignment) {
    TRAttribute attribute = {};
    attribute.type = TRAttributeTextAlignment;
    attribute.value.textAlignment = alignment;
    TRTextSetAttribute(text, 0, TRTextGetLength(text), &attribute);
}

static void setParagraph(TRMutableTextRef text, TRAttributeType type, TRFloat value) {
    setTestFloat(text, 0, TRTextGetLength(text), type, value);
}

void FrameTests::testSingleLine() {
    Fixture f(u"abc");
    TRComposedFrameRef frame = f.frame();

    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRRange range = { TRComposedFrameGetCodeUnitStart(frame), TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame) };
    assert(range.index == 0 && range.length == 3);

    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(TRComposedLineGetOrigin(line).x == 0.0f);
    assert(TRComposedLineGetOrigin(line).y == Ascent);
    assert(TRComposedLineGetTop(line) == 0.0f);
    assert(TRComposedLineGetBottom(line) == Height);

    /* A frame without a size is as big as it can be. */
    assert(TRComposedFrameGetWidth(frame) > 1.0e30f);
    assert(TRComposedFrameGetHeight(frame) > 1.0e30f);

    TRComposedFrameRelease(frame);
}

void FrameTests::testFitting() {
    Fixture f(u"abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(10000.0f, 10000.0f));
    TRFrameResolverSetFitsHorizontally(f.resolver, TRTrue);
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetWidth(frame) == A + B + C);
    assert(TRComposedFrameGetHeight(frame) == Height);
    TRComposedFrameRelease(frame);

    /* Lines are moved with the frame that shrinks, as their alignment says. */
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentRight);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testWrapping() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 100000.0f));
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    TRRange first = { TRComposedLineGetCodeUnitStart(TRComposedFrameGetLine(frame, 0)), TRComposedLineGetCodeUnitEnd(TRComposedFrameGetLine(frame, 0)) - TRComposedLineGetCodeUnitStart(TRComposedFrameGetLine(frame, 0)) };
    TRRange second = { TRComposedLineGetCodeUnitStart(TRComposedFrameGetLine(frame, 1)), TRComposedLineGetCodeUnitEnd(TRComposedFrameGetLine(frame, 1)) - TRComposedLineGetCodeUnitStart(TRComposedFrameGetLine(frame, 1)) };
    assert(first.index == 0 && first.length == 4);
    assert(second.index == 4 && second.length == 3);

    assert(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).y == Height + Ascent);
    assert(TRComposedFrameGetHeight(frame) == 2.0f * Height);
    assert(TRComposedFrameGetWidth(frame) == 3500.0f);

    TRComposedFrameRelease(frame);
}

void FrameTests::testFrameRange() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 100000.0f));

    TRComposedFrameRef frame = f.frame(4, 7);
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRRange range = { TRComposedFrameGetCodeUnitStart(frame), TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame) };
    assert(range.index == 4 && range.length == 3);
    TRComposedFrameRelease(frame);
}

void FrameTests::testHeightLimit() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 3000.0f));

    /* The second line would end at 4800. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRRange range = { TRComposedFrameGetCodeUnitStart(frame), TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame) };
    assert(range.index == 0 && range.length == 4);
    TRComposedFrameRelease(frame);

    /* There is a line even if the frame is smaller than it. */
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 0.0f));
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRComposedFrameRelease(frame);
}

void FrameTests::testMaxLines() {
    Fixture f(u"abc abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 100000.0f));

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 3);
    TRComposedFrameRelease(frame);

    TRFrameResolverSetMaxLines(f.resolver, 2);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    TRRange range = { TRComposedFrameGetCodeUnitStart(frame), TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame) };
    assert(range.index == 0 && range.length == 8);
    TRComposedFrameRelease(frame);
}

void FrameTests::testTruncation() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, Height));
    TRFrameResolverSetTruncationMode(f.resolver, TRBreakModeCharacter);

    /* Without truncation, the frame shows what fits. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 4);
    TRComposedFrameRelease(frame);

    /* The last line is cut so that the three dots fit after 'ab'. */
    TRFrameResolverSetTruncationPlace(f.resolver, TRTruncationPlaceEnd);
    frame = f.frame();
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 4);
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTruncationEnabled(f.resolver, TRTrue);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 7);

    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetWidth(line), A + B + 3.0f * Notdef));
    assert((TRComposedLineGetCodeUnitEnd(line) - TRComposedLineGetCodeUnitStart(line)) == 2);
    TRComposedFrameRelease(frame);

    /* A line that shows the token is not justified, as it would lose it. */
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);
    frame = f.frame();
    assert(isNear(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), A + B + 3.0f * Notdef));
    assert(TRComposedLineIsTruncated(TRComposedFrameGetLine(frame, 0)));
    TRComposedFrameRelease(frame);
    TRFrameResolverSetJustificationEnabled(f.resolver, TRFalse);

    /* There is nothing to cut if everything fits. */
    TRFrameResolverSetFrameSize(f.resolver, makeSize(10000.0f, Height));
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert(isNear(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), 2.0f * (A + B + C) + Notdef));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTruncationEnabled(f.resolver, TRFalse);
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, Height));
    frame = f.frame();
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 4);
    TRComposedFrameRelease(frame);
}

void FrameTests::testTruncationAcrossParagraphs() {
    Fixture f(u"abc abc\nabc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, Height));
    TRFrameResolverSetTruncationMode(f.resolver, TRBreakModeCharacter);
    TRFrameResolverSetTruncationPlace(f.resolver, TRTruncationPlaceEnd);
    TRFrameResolverSetTruncationEnabled(f.resolver, TRTrue);

    /* The frame fills up in the first paragraph, but it still covers the whole range. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert(TRComposedLineIsTruncated(TRComposedFrameGetLine(frame, 0)));
    assert(TRComposedFrameGetCodeUnitStart(frame) == 0);
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 15);
    TRComposedFrameRelease(frame);

    /* A range in the middle of the text ends where it is asked to. */
    frame = f.frame(2, 12);
    assert(TRComposedFrameGetCodeUnitStart(frame) == 2);
    assert((TRComposedFrameGetCodeUnitEnd(frame) - TRComposedFrameGetCodeUnitStart(frame)) == 10);
    TRComposedFrameRelease(frame);
}

void FrameTests::testAlignments() {
    Fixture f(u"abc");
    const TRFloat width = A + B + C;
    TRFrameResolverSetFrameSize(f.resolver, makeSize(5000.0f, 5000.0f));

    TRComposedFrameRef frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentTrailing);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 5000.0f - width));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentCenter);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, (5000.0f - width) / 2.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetVerticalAlignment(f.resolver, TRVerticalAlignmentCenter);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent + (5000.0f - Height) / 2.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetVerticalAlignment(f.resolver, TRVerticalAlignmentBottom);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent + 5000.0f - Height));
    TRComposedFrameRelease(frame);

    /* The alignment of a paragraph wins over the one of the resolver. */
    setAlignment(f.text, TRTextAlignmentRight);
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);
    TRFrameResolverSetTypesetter(f.resolver, f.typesetter);
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentLeft);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 5000.0f - width));
    TRComposedFrameRelease(frame);
}

static void useText(Fixture &f) {
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);
    assert(f.typesetter != nullptr);
    TRFrameResolverSetTypesetter(f.resolver, f.typesetter);
}

void FrameTests::testIndents() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3700.0f, 100000.0f));
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 100.0f);
    setParagraph(f.text, TRAttributeHeadIndent, 300.0f);
    useText(f);

    /* The first line has 3600 for text, and the second has 3400. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 100.0f));
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 300.0f));
    TRComposedFrameRelease(frame);

    /* Two lines use the first line indent. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 2);
    useText(f);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 100.0f));
    TRComposedFrameRelease(frame);

    /* With none, all of them use the head indent. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 0);
    useText(f);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 300.0f));
    TRComposedFrameRelease(frame);

    /* A tail indent that is not positive is a margin at the trailing edge. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 1);
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 0.0f);
    setParagraph(f.text, TRAttributeHeadIndent, 0.0f);
    setParagraph(f.text, TRAttributeTailIndent, -200.0f);
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3600.0f, 100000.0f));
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentTrailing);
    useText(f);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 3400.0f - (A + B + C)));
    TRComposedFrameRelease(frame);
}

void FrameTests::testParagraphSpacing() {
    Fixture f(u"abc\nabc");
    setParagraph(f.text, TRAttributeParagraphSpacing, 100.0f);
    setParagraph(f.text, TRAttributeParagraphSpacingBefore, 50.0f);
    useText(f);
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    /* The spacing before the first paragraph and after the last one does not count. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent));
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).y, Height + 150.0f + Ascent));
    assert(isNear(TRComposedFrameGetHeight(frame), 2.0f * Height + 150.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testLineHeights() {
    Fixture f(u"abc");
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRFrameResolverSetLineHeightMultiplier(f.resolver, 2.0f);
    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetAscent(line), Ascent + Height / 2.0f));
    assert(isNear(TRComposedLineGetDescent(line), 500.0f + Height / 2.0f));
    assert(isNear(TRComposedFrameGetHeight(frame), 2.0f * Height));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetLineHeightMultiplier(f.resolver, 1.0f);
    TRFrameResolverSetExtraLineSpacing(f.resolver, 100.0f);
    frame = f.frame();
    line = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetLeading(line), 100.0f));
    assert(isNear(TRComposedFrameGetHeight(frame), Height + 100.0f));
    TRComposedFrameRelease(frame);

    /* The styles of the paragraphs: multiple, minimum, maximum, and line spacing. */
    TRFrameResolverSetExtraLineSpacing(f.resolver, 0.0f);
    setParagraph(f.text, TRAttributeLineHeightMultiple, 1.5f);
    setParagraph(f.text, TRAttributeLineSpacing, 10.0f);
    useText(f);
    frame = f.frame();
    line = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetAscent(line), Ascent + Height / 2.0f));
    assert(isNear(TRComposedLineGetLeading(line), 10.0f));
    TRComposedFrameRelease(frame);

    setParagraph(f.text, TRAttributeLineHeightMultiple, 0.0f);
    setParagraph(f.text, TRAttributeLineSpacing, 0.0f);
    setParagraph(f.text, TRAttributeMinimumLineHeight, 3000.0f);
    useText(f);
    frame = f.frame();
    assert(isNear(TRComposedFrameGetHeight(frame), 3000.0f));
    TRComposedFrameRelease(frame);

    setParagraph(f.text, TRAttributeMinimumLineHeight, 0.0f);
    setParagraph(f.text, TRAttributeMaximumLineHeight, 2000.0f);
    useText(f);
    frame = f.frame();
    assert(isNear(TRComposedFrameGetHeight(frame), 2000.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testJustification() {
    Fixture f(u"ab ab ab");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, 100000.0f));
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    /* The first line takes the width, and the last line of the paragraph stays as it is. */
    TRComposedLineRef first = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 6000.0f));
    assert(isNear(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 1)), A + B));
    assert(isNear(TRComposedLineGetOrigin(first).y, Ascent));
    TRComposedFrameRelease(frame);

    /* The margins of the paragraph are left out of the width that the line takes. */
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6100.0f, 100000.0f));
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 100.0f);
    useText(f);
    frame = f.frame();
    first = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetOrigin(first).x, 100.0f));
    assert(isNear(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 6000.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetJustificationEnabled(f.resolver, TRFalse);
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, 100000.0f));
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 0.0f);
    useText(f);
    frame = f.frame();
    first = TRComposedFrameGetLine(frame, 0);
    assert(isNear(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 2.0f * (A + B) + Notdef));
    TRComposedFrameRelease(frame);
}

void FrameTests::testRightToLeft() {
    Fixture f(u"שלום");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(5000.0f, 5000.0f));

    /* Leading is the right edge in a right-to-left paragraph. */
    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(TRComposedLineGetParagraphLevel(line) == 1);
    assert(isNear(TRComposedLineGetOrigin(line).x, 5000.0f - 4.0f * Notdef));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentLeft);
    frame = f.frame();
    assert(isNear(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testIndexOfLine() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 100000.0f));

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 0) == 0);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 3) == 0);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 4) == 1);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 6) == 1);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 7) == TRInvalidIndex);

    assert(TRComposedFrameGetIndexOfLineAtPosition(frame, { 10.0f, 100.0f }) == 0);
    assert(TRComposedFrameGetIndexOfLineAtPosition(frame, { 10.0f, 3000.0f }) == 1);
    assert(TRComposedFrameGetIndexOfLineAtPosition(frame, { 10.0f, -50.0f }) == 1);
    assert(TRComposedFrameGetIndexOfLineAtPosition(frame, { 10.0f, 99999.0f }) == 1);

    TRComposedFrameRelease(frame);
}

static void assertHit(TRComposedFrameRef frame, TRFloat x, TRFloat y, TRUInteger clusterIndex,
    TRUInteger clusterLength, TRHitSide side, bool isInside) {
    TRPoint position = { x, y };
    TRHitTestResult hit = TRComposedFrameHitTest(frame, position);

    assert(hit.clusterIndex == clusterIndex);
    assert(hit.clusterLength == clusterLength);
    assert(hit.side == side);
    assert(hit.isInside == (isInside ? TRTrue : TRFalse));

    /* The boundary is the edge of the cluster on the side of the position. */
    assert(hit.codeUnitIndex == (side == TRHitSideLeading ? clusterIndex : clusterIndex + clusterLength));
    assert(TRComposedFrameGetCodeUnitIndexAtPosition(frame, position) == hit.codeUnitIndex);
}

void FrameTests::testHitTest() {
    Fixture f(u"abc");
    TRComposedFrameRef frame = f.frame();

    /* The halves of a cluster give the boundaries before and after it. */
    assertHit(frame, 500.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, 900.0f, 100.0f, 0, 1, TRHitSideTrailing, true);
    assertHit(frame, A + 500.0f, 100.0f, 1, 1, TRHitSideLeading, true);
    assertHit(frame, A + B + C - 10.0f, 2300.0f, 2, 1, TRHitSideTrailing, true);

    /* A position beside the text is nearest to the first or the last cluster, and is not on it. */
    assertHit(frame, -50.0f, 100.0f, 0, 1, TRHitSideLeading, false);
    assertHit(frame, A + B + C + 50.0f, 100.0f, 2, 1, TRHitSideTrailing, false);

    /* A position above or below the only line is in the line for x, but not on the cluster. */
    assertHit(frame, 500.0f, -100.0f, 0, 1, TRHitSideLeading, false);
    assertHit(frame, A + 900.0f, Height + 100.0f, 1, 1, TRHitSideTrailing, false);
    assertHit(frame, -500.0f, -100.0f, 0, 1, TRHitSideLeading, false);
    assertHit(frame, 90000.0f, 90000.0f, 2, 1, TRHitSideTrailing, false);
    TRComposedFrameRelease(frame);

    /* Above the first line and below the last one pick the lines at the ends. */
    Fixture g(u"abc abc");
    TRFrameResolverSetFrameSize(g.resolver, makeSize(3500.0f, 100000.0f));
    frame = g.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    assertHit(frame, 500.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, 500.0f, Height + 100.0f, 4, 1, TRHitSideLeading, true);
    assertHit(frame, A + 900.0f, -5000.0f, 1, 1, TRHitSideTrailing, false);
    assertHit(frame, A + 900.0f, 99999.0f, 5, 1, TRHitSideTrailing, false);
    assertHit(frame, 99999.0f, 99999.0f, 6, 1, TRHitSideTrailing, false);

    /* The result of a position at the end of the first line is the end of that line. */
    TRPoint end = { 4100.0f, 100.0f };
    assert(TRComposedFrameGetCodeUnitIndexAtPosition(frame, end) == 4);
    TRComposedFrameRelease(frame);
}

void FrameTests::testHitTestRightToLeft() {
    Fixture f(u"\u05E9\u05DC\u05D5\u05DD");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(5000.0f, 5000.0f));

    TRComposedFrameRef frame = f.frame();
    TRFloat left = 5000.0f - 4.0f * Notdef;

    /* The first character is at the right, and its leading side is the right half of it. */
    assertHit(frame, 4950.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, 4200.0f, 100.0f, 0, 1, TRHitSideTrailing, true);
    assertHit(frame, left + 3.0f * Notdef - 100.0f, 100.0f, 1, 1, TRHitSideLeading, true);

    /* The last character is at the left, and its trailing side is the left half of it. */
    assertHit(frame, left + 10.0f, 100.0f, 3, 1, TRHitSideTrailing, true);
    assertHit(frame, left + Notdef - 10.0f, 100.0f, 3, 1, TRHitSideLeading, true);

    /* Beside the text, the nearest edge is the end at the left and the start at the right. */
    assertHit(frame, 0.0f, 100.0f, 3, 1, TRHitSideTrailing, false);
    assertHit(frame, 5000.0f + 400.0f, 100.0f, 0, 1, TRHitSideLeading, false);
    assertHit(frame, 4950.0f, 4000.0f, 0, 1, TRHitSideLeading, false);
    TRComposedFrameRelease(frame);
}

void FrameTests::testHitTestMixedDirections() {
    /* The latin run takes the space, and the hebrew run follows it with its first letter at right. */
    Fixture f(u"abc \u05E9\u05DC\u05D5\u05DD");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(20000.0f, 5000.0f));

    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(TRComposedLineGetParagraphLevel(line) == 0);
    assert(TRComposedLineGetGlyphRunCount(line) == 2);

    TRFloat hebrew = A + B + C + Notdef;
    assertHit(frame, 100.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, A + B + C + 800.0f, 100.0f, 3, 1, TRHitSideTrailing, true);

    /* The right end of the hebrew run is where it starts. */
    assertHit(frame, hebrew + 4.0f * Notdef - 100.0f, 100.0f, 4, 1, TRHitSideLeading, true);
    assertHit(frame, hebrew + 4.0f * Notdef - 800.0f, 100.0f, 4, 1, TRHitSideTrailing, true);

    /* The characters of the hebrew run grow toward the left. */
    assertHit(frame, hebrew + Notdef + 100.0f, 100.0f, 6, 1, TRHitSideTrailing, true);
    assertHit(frame, hebrew + 10.0f, 100.0f, 7, 1, TRHitSideTrailing, true);
    assertHit(frame, hebrew + Notdef - 10.0f, 100.0f, 7, 1, TRHitSideLeading, true);

    /* Beyond the end of the line, the caret is at the start of the last run on the screen. */
    assertHit(frame, hebrew + 4.0f * Notdef + 500.0f, 100.0f, 4, 1, TRHitSideLeading, false);
    TRComposedFrameRelease(frame);
}

void FrameTests::testHitTestClusters() {
    /* A surrogate pair is a single cluster, so the caret cannot be put between its units. */
    Fixture f(u"a\xD83D\xDE00" u"b");
    TRComposedFrameRef frame = f.frame();

    assertHit(frame, 100.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, A + 100.0f, 100.0f, 1, 2, TRHitSideLeading, true);
    assertHit(frame, A + Notdef / 2.0f + 1.0f, 100.0f, 1, 2, TRHitSideTrailing, true);
    assertHit(frame, A + Notdef - 100.0f, 100.0f, 1, 2, TRHitSideTrailing, true);
    assertHit(frame, A + Notdef + 100.0f, 100.0f, 3, 1, TRHitSideLeading, true);

    TRPoint inside = { A + Notdef / 2.0f - 1.0f, 100.0f };
    assert(TRComposedFrameGetCodeUnitIndexAtPosition(frame, inside) == 1);
    inside.x += 2.0f;
    assert(TRComposedFrameGetCodeUnitIndexAtPosition(frame, inside) == 3);

    TRComposedFrameRelease(frame);
}

void FrameTests::testHitTestTruncatedLine() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(6000.0f, Height));
    TRFrameResolverSetTruncationMode(f.resolver, TRBreakModeCharacter);
    TRFrameResolverSetTruncationPlace(f.resolver, TRTruncationPlaceEnd);
    TRFrameResolverSetTruncationEnabled(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(TRComposedLineIsTruncated(line));

    /* The text before the token is hit as usual. */
    assertHit(frame, 100.0f, 100.0f, 0, 1, TRHitSideLeading, true);
    assertHit(frame, A + 900.0f, 100.0f, 1, 1, TRHitSideTrailing, true);

    /* The token is on the line, so a position in it is on a cluster too. */
    TRPoint inToken = { A + B + 100.0f, 100.0f };
    TRHitTestResult hit = TRComposedFrameHitTest(frame, inToken);
    assert(hit.isInside && hit.clusterLength == 1);

    inToken.x = A + B + 3.0f * Notdef - 100.0f;
    hit = TRComposedFrameHitTest(frame, inToken);
    assert(hit.isInside && hit.clusterLength == 1);

    /* Beyond the token, the nearest cluster is its last one, and the position is not on it. */
    inToken.x = 90000.0f;
    hit = TRComposedFrameHitTest(frame, inToken);
    assert(!hit.isInside && hit.side == TRHitSideTrailing && hit.clusterLength == 1);
    assert(TRComposedFrameGetCodeUnitIndexAtPosition(frame, inToken) == hit.codeUnitIndex);
    TRComposedFrameRelease(frame);
}

static void collectRect(void *userData, TRRect rect, TRBoolean *) {
    static_cast<vector<TRRect> *>(userData)->push_back(rect);
}

void FrameTests::testSelection() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(4300.0f, 100000.0f));

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    /* A range in a line is covered by the parts of the line. */
    vector<TRRect> rects;
    TRComposedFrameEnumerateSelection(frame, 1, 1, collectRect, &rects);
    assert(rects.size() == 1);
    assert(isNear(rects[0].origin.x, A) && isNear(rects[0].size.width, B));
    assert(isNear(rects[0].origin.y, 0.0f) && isNear(rects[0].size.height, Height));

    /* A range over two lines has the paddings in between. */
    rects.clear();
    TRComposedFrameEnumerateSelection(frame, 2, 4, collectRect, &rects);
    assert(rects.size() == 4);

    assert(isNear(rects[0].origin.x, A + B) && isNear(rects[0].size.width, C + Notdef));
    assert(isNear(rects[1].origin.x, A + B + C + Notdef));
    assert(isNear(rects[1].origin.x + rects[1].size.width, 4300.0f));
    assert(isNear(rects[2].origin.x, 0.0f) && isNear(rects[2].size.width, 0.0f));
    assert(isNear(rects[2].origin.y, Height));
    assert(isNear(rects[3].origin.x, 0.0f) && isNear(rects[3].size.width, A + B));
    assert(isNear(rects[3].origin.y, Height) && isNear(rects[3].size.height, Height));

    TRComposedFrameRelease(frame);

    /* A line in the middle is covered completely. */
    Fixture g(u"abc abc abc");
    TRFrameResolverSetFrameSize(g.resolver, makeSize(4300.0f, 100000.0f));
    frame = g.frame();
    assert(TRComposedFrameGetLineCount(frame) == 3);
    rects.clear();
    TRComposedFrameEnumerateSelection(frame, 0, 11, collectRect, &rects);
    bool hasMid = false;
    for (const TRRect &rect : rects) {
        if (isNear(rect.origin.y, Height) && isNear(rect.size.width, 4300.0f)) {
            hasMid = true;
        }
    }
    assert(hasMid);
    TRComposedFrameRelease(frame);
}

namespace {

void stopAfterFirstRect(void *userData, TRRect, TRBoolean *stop) {
    (*static_cast<size_t *>(userData))++;
    *stop = TRTrue;
}

}

void FrameTests::testRangesAreChecked() {
    const TRUInteger max = TRInvalidIndex;
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, makeSize(4300.0f, 100000.0f));

    /* A range that is within the text is the frame, up to the end of the text. */
    TRComposedFrameRef frame = TRFrameResolverCreateFrame(f.resolver, 4, 3);
    assert(frame != nullptr);
    assert(TRComposedFrameGetCodeUnitStart(frame) == 4);
    assert(TRComposedFrameGetCodeUnitEnd(frame) == 7);
    TRComposedFrameRelease(frame);

    frame = TRFrameResolverCreateFrame(f.resolver, 0, 7);
    assert(frame != nullptr);

    /* There is no line past the last one, and no code unit that is not in the frame has a line. */
    assert(TRComposedFrameGetLine(frame, 2) == nullptr);
    assert(TRComposedFrameGetLine(frame, max) == nullptr);
    assert(TRComposedFrameGetLine(frame, 1) != nullptr);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 7) == TRInvalidIndex);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, max) == TRInvalidIndex);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 99) == TRInvalidIndex);
    assert(TRComposedFrameGetIndexOfLineForCodeUnit(frame, 0) == 0);

    /* A selection is only of a range that is within the frame. */
    vector<TRRect> expected;
    assert(TRComposedFrameEnumerateSelection(frame, 2, 4, collectRect, &expected));
    assert(!expected.empty());

    vector<TRRect> everything;
    assert(TRComposedFrameEnumerateSelection(frame, 0, 7, collectRect, &everything));
    assert(!everything.empty());

    vector<TRRect> nothing;
    assert(!TRComposedFrameEnumerateSelection(frame, 0, 8, collectRect, &nothing));
    assert(!TRComposedFrameEnumerateSelection(frame, 0, max, collectRect, &nothing));
    assert(!TRComposedFrameEnumerateSelection(frame, 7, 3, collectRect, &nothing));
    assert(!TRComposedFrameEnumerateSelection(frame, 99, 3, collectRect, &nothing));
    assert(!TRComposedFrameEnumerateSelection(frame, max, max, collectRect, &nothing));
    assert(nothing.empty());

    /* An empty range is within the frame, and selects nothing. */
    assert(TRComposedFrameEnumerateSelection(frame, 3, 0, collectRect, &nothing));
    assert(nothing.empty());

    /* The enumeration stops when the function says so, even across the lines of a range. */
    size_t stopped = 0;
    assert(TRComposedFrameEnumerateSelection(frame, 2, 4, stopAfterFirstRect, &stopped));
    assert(stopped == 1);
    TRComposedFrameRelease(frame);

    /* There is no frame for a range that is empty or not within the text, or without text. */
    assert(TRFrameResolverCreateFrame(f.resolver, 0, 8) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, 7, 3) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, 99, 3) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, 3, 0) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, 0, max) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, max, max) == nullptr);

    TRFrameResolverRef bare = TRFrameResolverCreate();
    assert(TRFrameResolverCreateFrame(bare, 0, 3) == nullptr);
    TRFrameResolverRelease(bare);

    /* An empty text has nothing to lay out either. */
    TRMutableTextRef empty = makeTestText(u"", f.typeface, EmSize);
    TRTypesetterRef emptyTypesetter = TRTypesetterCreate(empty, nullptr, 0);
    assert(emptyTypesetter != nullptr);
    TRFrameResolverSetTypesetter(f.resolver, emptyTypesetter);
    assert(TRFrameResolverCreateFrame(f.resolver, 0, 5) == nullptr);
    assert(TRFrameResolverCreateFrame(f.resolver, 0, 0) == nullptr);
    TRTypesetterRelease(emptyTypesetter);
    TRTextRelease(empty);
}

void FrameTests::testFramesOutliveResolver() {
    TRComposedFrameRef frame;
    {
        Fixture f(u"abc abc");
        TRFrameResolverSetFrameSize(f.resolver, makeSize(3500.0f, 100000.0f));
        frame = f.frame();
    }

    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(isNear(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), A + B + C + Notdef));

    TRComposedFrameRetain(frame);
    TRComposedFrameRelease(frame);
    TRComposedFrameRelease(frame);
}

#ifdef STANDALONE_TESTING

int main() {
    FrameTests tests;
    tests.run();

    return 0;
}

#endif
