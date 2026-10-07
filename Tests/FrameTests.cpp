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

void FrameTests::run() {
    testSingleLine();
    testFitting();
    testWrapping();
    testFrameRange();
    testHeightLimit();
    testMaxLines();
    testTruncation();
    testAlignments();
    testIndents();
    testParagraphSpacing();
    testLineHeights();
    testJustification();
    testRightToLeft();
    testIndexOfLine();
    testSelection();
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

static bool near(TRFloat a, TRFloat b) {
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
        TRComposedFrameRef result = TRFrameResolverCreateFrame(resolver, { start, end - start });
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
    TRRange range = TRComposedFrameGetCodeUnitRange(frame);
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
    TRFrameResolverSetFrameSize(f.resolver, 10000.0f, 10000.0f);
    TRFrameResolverSetFitsHorizontally(f.resolver, TRTrue);
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetWidth(frame) == A + B + C);
    assert(TRComposedFrameGetHeight(frame) == Height);
    TRComposedFrameRelease(frame);

    /* Lines are moved with the frame that shrinks, as their alignment says. */
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentRight);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testWrapping() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 100000.0f);
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    TRRange first = TRComposedLineGetCodeUnitRange(TRComposedFrameGetLine(frame, 0));
    TRRange second = TRComposedLineGetCodeUnitRange(TRComposedFrameGetLine(frame, 1));
    assert(first.index == 0 && first.length == 4);
    assert(second.index == 4 && second.length == 3);

    assert(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).y == Height + Ascent);
    assert(TRComposedFrameGetHeight(frame) == 2.0f * Height);
    assert(TRComposedFrameGetWidth(frame) == 3500.0f);

    TRComposedFrameRelease(frame);
}

void FrameTests::testFrameRange() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 100000.0f);

    TRComposedFrameRef frame = f.frame(4, 7);
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRRange range = TRComposedFrameGetCodeUnitRange(frame);
    assert(range.index == 4 && range.length == 3);
    TRComposedFrameRelease(frame);
}

void FrameTests::testHeightLimit() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 3000.0f);

    /* The second line would end at 4800. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRRange range = TRComposedFrameGetCodeUnitRange(frame);
    assert(range.index == 0 && range.length == 4);
    TRComposedFrameRelease(frame);

    /* There is a line even if the frame is smaller than it. */
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 0.0f);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    TRComposedFrameRelease(frame);
}

void FrameTests::testMaxLines() {
    Fixture f(u"abc abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 100000.0f);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 3);
    TRComposedFrameRelease(frame);

    TRFrameResolverSetMaxLines(f.resolver, 2);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    TRRange range = TRComposedFrameGetCodeUnitRange(frame);
    assert(range.index == 0 && range.length == 8);
    TRComposedFrameRelease(frame);
}

void FrameTests::testTruncation() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 6000.0f, Height);
    TRFrameResolverSetTruncationMode(f.resolver, TRBreakModeCharacter);

    /* Without truncation, the frame shows what fits. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert(TRComposedFrameGetCodeUnitRange(frame).length == 4);
    TRComposedFrameRelease(frame);

    /* The last line is cut so that the three dots fit after 'ab'. */
    TRFrameResolverSetTruncationPlace(f.resolver, TRTruncationPlaceEnd);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert(TRComposedFrameGetCodeUnitRange(frame).length == 7);

    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetWidth(line), A + B + 3.0f * Notdef));
    assert(TRComposedLineGetCodeUnitRange(line).length == 2);
    TRComposedFrameRelease(frame);

    /* A line that shows the token is not justified, as it would lose it. */
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);
    frame = f.frame();
    assert(near(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), A + B + 3.0f * Notdef));
    assert(TRComposedLineIsTruncated(TRComposedFrameGetLine(frame, 0)));
    TRComposedFrameRelease(frame);
    TRFrameResolverSetJustificationEnabled(f.resolver, TRFalse);

    /* There is nothing to cut if everything fits. */
    TRFrameResolverSetFrameSize(f.resolver, 10000.0f, Height);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 1);
    assert(near(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), 2.0f * (A + B + C) + Notdef));
    TRComposedFrameRelease(frame);

    TRFrameResolverDisableTruncation(f.resolver);
    TRFrameResolverSetFrameSize(f.resolver, 6000.0f, Height);
    frame = f.frame();
    assert(TRComposedFrameGetCodeUnitRange(frame).length == 4);
    TRComposedFrameRelease(frame);
}

void FrameTests::testAlignments() {
    Fixture f(u"abc");
    const TRFloat width = A + B + C;
    TRFrameResolverSetFrameSize(f.resolver, 5000.0f, 5000.0f);

    TRComposedFrameRef frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentTrailing);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 5000.0f - width));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentCenter);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, (5000.0f - width) / 2.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetVerticalAlignment(f.resolver, TRVerticalAlignmentCenter);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent + (5000.0f - Height) / 2.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetVerticalAlignment(f.resolver, TRVerticalAlignmentBottom);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent + 5000.0f - Height));
    TRComposedFrameRelease(frame);

    /* The alignment of a paragraph wins over the one of the resolver. */
    setAlignment(f.text, TRTextAlignmentRight);
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);
    TRFrameResolverSetTypesetter(f.resolver, f.typesetter);
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentLeft);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 5000.0f - width));
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
    TRFrameResolverSetFrameSize(f.resolver, 3700.0f, 100000.0f);
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 100.0f);
    setParagraph(f.text, TRAttributeHeadIndent, 300.0f);
    useText(f);

    /* The first line has 3600 for text, and the second has 3400. */
    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 100.0f));
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 300.0f));
    TRComposedFrameRelease(frame);

    /* Two lines use the first line indent. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 2);
    useText(f);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 100.0f));
    TRComposedFrameRelease(frame);

    /* With none, all of them use the head indent. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 0);
    useText(f);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 300.0f));
    TRComposedFrameRelease(frame);

    /* A tail indent that is not positive is a margin at the trailing edge. */
    setParagraphCount(f.text, TRAttributeFirstIndentLineCount, 1);
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 0.0f);
    setParagraph(f.text, TRAttributeHeadIndent, 0.0f);
    setParagraph(f.text, TRAttributeTailIndent, -200.0f);
    TRFrameResolverSetFrameSize(f.resolver, 3600.0f, 100000.0f);
    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentTrailing);
    useText(f);
    frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).x, 3400.0f - (A + B + C)));
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
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y, Ascent));
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 1)).y, Height + 150.0f + Ascent));
    assert(near(TRComposedFrameGetHeight(frame), 2.0f * Height + 150.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testLineHeights() {
    Fixture f(u"abc");
    TRFrameResolverSetFitsVertically(f.resolver, TRTrue);

    TRFrameResolverSetLineHeightMultiplier(f.resolver, 2.0f);
    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetAscent(line), Ascent + Height / 2.0f));
    assert(near(TRComposedLineGetDescent(line), 500.0f + Height / 2.0f));
    assert(near(TRComposedFrameGetHeight(frame), 2.0f * Height));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetLineHeightMultiplier(f.resolver, 1.0f);
    TRFrameResolverSetExtraLineSpacing(f.resolver, 100.0f);
    frame = f.frame();
    line = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetLeading(line), 100.0f));
    assert(near(TRComposedFrameGetHeight(frame), Height + 100.0f));
    TRComposedFrameRelease(frame);

    /* The styles of the paragraphs: multiple, minimum, maximum, and line spacing. */
    TRFrameResolverSetExtraLineSpacing(f.resolver, 0.0f);
    setParagraph(f.text, TRAttributeLineHeightMultiple, 1.5f);
    setParagraph(f.text, TRAttributeLineSpacing, 10.0f);
    useText(f);
    frame = f.frame();
    line = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetAscent(line), Ascent + Height / 2.0f));
    assert(near(TRComposedLineGetLeading(line), 10.0f));
    TRComposedFrameRelease(frame);

    setParagraph(f.text, TRAttributeLineHeightMultiple, 0.0f);
    setParagraph(f.text, TRAttributeLineSpacing, 0.0f);
    setParagraph(f.text, TRAttributeMinimumLineHeight, 3000.0f);
    useText(f);
    frame = f.frame();
    assert(near(TRComposedFrameGetHeight(frame), 3000.0f));
    TRComposedFrameRelease(frame);

    setParagraph(f.text, TRAttributeMinimumLineHeight, 0.0f);
    setParagraph(f.text, TRAttributeMaximumLineHeight, 2000.0f);
    useText(f);
    frame = f.frame();
    assert(near(TRComposedFrameGetHeight(frame), 2000.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testJustification() {
    Fixture f(u"ab ab ab");
    TRFrameResolverSetFrameSize(f.resolver, 6000.0f, 100000.0f);
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    /* The first line takes the width, and the last line of the paragraph stays as it is. */
    TRComposedLineRef first = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 6000.0f));
    assert(near(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 1)), A + B));
    assert(near(TRComposedLineGetOrigin(first).y, Ascent));
    TRComposedFrameRelease(frame);

    /* The margins of the paragraph are left out of the width that the line takes. */
    TRFrameResolverSetJustificationEnabled(f.resolver, TRTrue);
    TRFrameResolverSetFrameSize(f.resolver, 6100.0f, 100000.0f);
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 100.0f);
    useText(f);
    frame = f.frame();
    first = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetOrigin(first).x, 100.0f));
    assert(near(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 6000.0f));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetJustificationEnabled(f.resolver, TRFalse);
    TRFrameResolverSetFrameSize(f.resolver, 6000.0f, 100000.0f);
    setParagraph(f.text, TRAttributeFirstLineHeadIndent, 0.0f);
    useText(f);
    frame = f.frame();
    first = TRComposedFrameGetLine(frame, 0);
    assert(near(TRComposedLineGetWidth(first) - TRComposedLineGetTrailingWhitespaceExtent(first), 2.0f * (A + B) + Notdef));
    TRComposedFrameRelease(frame);
}

void FrameTests::testRightToLeft() {
    Fixture f(u"שלום");
    TRFrameResolverSetFrameSize(f.resolver, 5000.0f, 5000.0f);

    /* Leading is the right edge in a right-to-left paragraph. */
    TRComposedFrameRef frame = f.frame();
    TRComposedLineRef line = TRComposedFrameGetLine(frame, 0);
    assert(TRComposedLineGetParagraphLevel(line) == 1);
    assert(near(TRComposedLineGetOrigin(line).x, 5000.0f - 4.0f * Notdef));
    TRComposedFrameRelease(frame);

    TRFrameResolverSetTextAlignment(f.resolver, TRTextAlignmentLeft);
    frame = f.frame();
    assert(near(TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).x, 0.0f));
    TRComposedFrameRelease(frame);
}

void FrameTests::testIndexOfLine() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 100000.0f);

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

static void collectRect(void *userData, TRRect rect) {
    static_cast<vector<TRRect> *>(userData)->push_back(rect);
}

void FrameTests::testSelection() {
    Fixture f(u"abc abc");
    TRFrameResolverSetFrameSize(f.resolver, 4300.0f, 100000.0f);

    TRComposedFrameRef frame = f.frame();
    assert(TRComposedFrameGetLineCount(frame) == 2);

    /* A range in a line is covered by the parts of the line. */
    vector<TRRect> rects;
    TRComposedFrameEnumerateSelection(frame, { 1, 1 }, collectRect, &rects);
    assert(rects.size() == 1);
    assert(near(rects[0].origin.x, A) && near(rects[0].size.width, B));
    assert(near(rects[0].origin.y, 0.0f) && near(rects[0].size.height, Height));

    /* A range over two lines has the paddings in between. */
    rects.clear();
    TRComposedFrameEnumerateSelection(frame, { 2, 4 }, collectRect, &rects);
    assert(rects.size() == 4);

    assert(near(rects[0].origin.x, A + B) && near(rects[0].size.width, C + Notdef));
    assert(near(rects[1].origin.x, A + B + C + Notdef));
    assert(near(rects[1].origin.x + rects[1].size.width, 4300.0f));
    assert(near(rects[2].origin.x, 0.0f) && near(rects[2].size.width, 0.0f));
    assert(near(rects[2].origin.y, Height));
    assert(near(rects[3].origin.x, 0.0f) && near(rects[3].size.width, A + B));
    assert(near(rects[3].origin.y, Height) && near(rects[3].size.height, Height));

    TRComposedFrameRelease(frame);

    /* A line in the middle is covered completely. */
    Fixture g(u"abc abc abc");
    TRFrameResolverSetFrameSize(g.resolver, 4300.0f, 100000.0f);
    frame = g.frame();
    assert(TRComposedFrameGetLineCount(frame) == 3);
    rects.clear();
    TRComposedFrameEnumerateSelection(frame, { 0, 11 }, collectRect, &rects);
    bool hasMid = false;
    for (const TRRect &rect : rects) {
        if (near(rect.origin.y, Height) && near(rect.size.width, 4300.0f)) {
            hasMid = true;
        }
    }
    assert(hasMid);
    TRComposedFrameRelease(frame);
}

void FrameTests::testFramesOutliveResolver() {
    TRComposedFrameRef frame;
    {
        Fixture f(u"abc abc");
        TRFrameResolverSetFrameSize(f.resolver, 3500.0f, 100000.0f);
        frame = f.frame();
    }

    assert(TRComposedFrameGetLineCount(frame) == 2);
    assert(near(TRComposedLineGetWidth(TRComposedFrameGetLine(frame, 0)), A + B + C + Notdef));

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
