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

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRGlyphCache.h>
#include <Tehreer/TRGlyphRun.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

extern "C" {
#include <API/TRComposedLine.h>
#include <API/TRGlyphRun.h>
#include <API/TRTypesetter.h>
#include <Core/AtomicUInt.h>
#include <Layout/LineResolver.h>
}

#include "TestText.h"
#include "TestTypeface.h"

#include "ComposedLineTests.h"

using namespace std;
using namespace Tehreer;

void ComposedLineTests::run() {
    testSimpleLine();
    testGlyphRun();
    testLineOfSubrange();
    testTrailingWhitespace();
    testDistances();
    testIndexOfCodeUnit();
    testEnumerateEdges();
    testPenOffset();
    testPaintAttributesSplitRuns();
    testMixedDirections();
    testRightToLeftParagraph();
    testRunsAreRelativeToTheirRange();
    testClusterCutByLine();
    testJustifiedCopiesAreIndependent();
    testLineMetricsFromTallestRun();
    testBoundingBox();
    testReplacementLines();
    testBlockLine();
    testTruncation();
    testTruncationToken();
    testJustifiedLine();
    testLinesOutliveTypesetter();
    testConcurrentLines();
}

/*
 * In the test font, the glyphs 'a', 'b' and 'c' are 1114, 1149 and 1072 units wide, and every other
 * character, including the space, shows the notdef glyph that is 908 units wide. The units per em
 * are 2048, so a point size of 2048 gives these widths as they are, and an ascent of 1900 and a
 * descent of 500.
 */
constexpr TRFloat EmSize = 2048.0f;
constexpr TRFloat A = 1114.0f;
constexpr TRFloat B = 1149.0f;
constexpr TRFloat C = 1072.0f;
constexpr TRFloat Notdef = 908.0f;

struct Fixture {
    TRTypefaceRef typeface;
    TRMutableTextRef text;
    TRTypesetterRef typesetter;

    explicit Fixture(const u16string &string) {
        typeface = createTestTypeface("Roboto-Regular.abc.ttf");
        text = makeTestText(string, typeface, EmSize);
        typesetter = nullptr;
    }

    TRTypesetterRef make() {
        if (typesetter) {
            TRTypesetterRelease(typesetter);
        }
        typesetter = TRTypesetterCreate(text, nullptr, 0);
        assert(typesetter != nullptr);

        return typesetter;
    }

    TRComposedLineRef line(TRUInteger start, TRUInteger end) {
        if (!typesetter) {
            make();
        }

        TRComposedLineRef result = TRTypesetterCreateSimpleLine(typesetter, { start, end - start });
        assert(result != nullptr);

        return result;
    }

    ~Fixture() {
        if (typesetter) {
            TRTypesetterRelease(typesetter);
        }
        TRTextRelease(text);
        TRTypefaceRelease(typeface);
    }
};

static vector<TRFloat> floats(const TRFloat *pointer, size_t count) {
    return vector<TRFloat>(pointer, pointer + count);
}

static vector<TRUInteger> sizes(const TRUInteger *pointer, size_t count) {
    return vector<TRUInteger>(pointer, pointer + count);
}

void ComposedLineTests::testSimpleLine() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);

    TRRange range = TRComposedLineGetCodeUnitRange(line);
    assert(range.index == 0 && range.length == 3);
    assert(TRComposedLineGetParagraphLevel(line) == 0);
    assert(TRComposedLineGetOrigin(line).x == 0.0f && TRComposedLineGetOrigin(line).y == 0.0f);

    assert(TRComposedLineGetAscent(line) == 1900.0f);
    assert(TRComposedLineGetDescent(line) == 500.0f);
    assert(TRComposedLineGetLeading(line) == 0.0f);
    assert(TRComposedLineGetHeight(line) == 2400.0f);
    assert(TRComposedLineGetWidth(line) == A + B + C);
    assert(TRComposedLineGetTrailingWhitespaceExtent(line) == 0.0f);
    assert(!TRComposedLineIsBlock(line));

    /* Without a frame the origin is zero, so the edges follow from the metrics. */
    assert(TRComposedLineGetTop(line) == -1900.0f);
    assert(TRComposedLineGetBottom(line) == 500.0f);
    assert(TRComposedLineGetLeft(line) == 0.0f);
    assert(TRComposedLineGetRight(line) == A + B + C);

    assert(TRComposedLineGetGlyphRunCount(line) == 1);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testGlyphRun() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    TRRange range = TRGlyphRunGetCodeUnitRange(run);
    assert(range.index == 0 && range.length == 3);
    assert(TRGlyphRunGetStartExtraLength(run) == 0 && TRGlyphRunGetEndExtraLength(run) == 0);
    assert(TRGlyphRunGetBidiLevel(run) == 0);
    assert(TRGlyphRunGetWritingDirection(run) == TRWritingDirectionLeftToRight);
    assert(!TRGlyphRunIsBackward(run));
    assert(TRGlyphRunGetTypeface(run) == f.typeface);
    assert(TRGlyphRunGetTypeSize(run) == EmSize);
    assert(TRGlyphRunGetScaleX(run) == 1.0f && TRGlyphRunGetScaleY(run) == 1.0f);
    assert(TRGlyphRunGetReplacement(run) == nullptr);
    assert(TRGlyphRunGetOrigin(run).x == 0.0f);

    assert(TRGlyphRunGetAscent(run) == 1900.0f && TRGlyphRunGetDescent(run) == 500.0f);
    assert(TRGlyphRunGetLeading(run) == 0.0f);
    assert(TRGlyphRunGetWidth(run) == A + B + C);
    assert(TRGlyphRunGetHeight(run) == 2400.0f);

    assert(TRGlyphRunGetGlyphCount(run) == 3);
    const TRGlyphID *ids = TRGlyphRunGetGlyphIDsPtr(run);
    assert(ids[0] == 1 && ids[1] == 2 && ids[2] == 3);
    assert((floats(TRGlyphRunGetGlyphAdvancesPtr(run), 3) == vector<TRFloat>{ A, B, C }));
    for (size_t i = 0; i < 3; i++) {
        assert(TRGlyphRunGetGlyphOffsetsPtr(run)[i].x == 0.0f);
        assert(TRGlyphRunGetGlyphOffsetsPtr(run)[i].y == 0.0f);
    }

    assert(TRGlyphRunGetClusterMapCount(run) == 3);
    assert((sizes(TRGlyphRunGetClusterMapPtr(run), 3) == vector<TRUInteger>{ 0, 1, 2 }));

    for (TRUInteger i = 0; i < 3; i++) {
        assert(TRGlyphRunGetClusterStart(run, i) == i);
        assert(TRGlyphRunGetClusterEnd(run, i) == i + 1);
        assert(TRGlyphRunGetLeadingGlyphIndex(run, i) == i);
        assert(TRGlyphRunGetTrailingGlyphIndex(run, i) == i);
    }

    TRColor color;
    assert(!TRGlyphRunGetForegroundColor(run, &color));
    assert(TRGlyphRunGetUserData(run) == nullptr);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testLineOfSubrange() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(1, 3);

    assert(TRComposedLineGetCodeUnitRange(line).index == 1);
    assert(TRComposedLineGetWidth(line) == B + C);
    assert(TRComposedLineGetGlyphRunCount(line) == 1);

    /* The run has the glyphs of its range only, and its distances start at zero. */
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetCodeUnitRange(run).index == 1 && TRGlyphRunGetCodeUnitRange(run).length == 2);
    assert(TRGlyphRunGetGlyphCount(run) == 2);
    assert(TRGlyphRunGetGlyphIDsPtr(run)[0] == 2 && TRGlyphRunGetGlyphIDsPtr(run)[1] == 3);
    assert((floats(TRGlyphRunGetGlyphAdvancesPtr(run), 2) == vector<TRFloat>{ B, C }));
    assert((sizes(TRGlyphRunGetClusterMapPtr(run), 2) == vector<TRUInteger>{ 0, 1 }));
    assert(TRGlyphRunGetDistance(run, 1) == 0.0f);
    assert(TRGlyphRunGetDistance(run, 2) == B);
    assert(TRGlyphRunGetDistance(run, 3) == B + C);
    assert(TRGlyphRunGetLeadingGlyphIndex(run, 1) == 0);
    assert(TRGlyphRunGetLeadingGlyphIndex(run, 2) == 1);

    TRComposedLineRelease(line);

    line = f.line(0, 1);
    assert(TRComposedLineGetWidth(line) == A);
    TRComposedLineRelease(line);

    line = f.line(2, 3);
    assert(TRComposedLineGetWidth(line) == C);
    TRComposedLineRelease(line);
}

void ComposedLineTests::testTrailingWhitespace() {
    Fixture f(u"ab c  ");

    /* "ab c" is 4 + 908 + 1072 wide, and the two spaces at the end add 2 * 908. */
    TRComposedLineRef line = f.line(0, 6);
    assert(TRComposedLineGetWidth(line) == A + B + Notdef + C + 2 * Notdef);
    assert(TRComposedLineGetTrailingWhitespaceExtent(line) == 2 * Notdef);
    TRComposedLineRelease(line);

    line = f.line(0, 4);
    assert(TRComposedLineGetTrailingWhitespaceExtent(line) == 0.0f);
    TRComposedLineRelease(line);

    line = f.line(0, 3);
    assert(TRComposedLineGetTrailingWhitespaceExtent(line) == Notdef);
    TRComposedLineRelease(line);

    /* A line of nothing but whitespace is all trailing. */
    line = f.line(4, 6);
    assert(TRComposedLineGetTrailingWhitespaceExtent(line) == TRComposedLineGetWidth(line));
    TRComposedLineRelease(line);
}

void ComposedLineTests::testDistances() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);

    assert(TRComposedLineGetDistance(line, 0) == 0.0f);
    assert(TRComposedLineGetDistance(line, 1) == A);
    assert(TRComposedLineGetDistance(line, 2) == A + B);
    /* The end of the line is the whole width. */
    assert(TRComposedLineGetDistance(line, 3) == A + B + C);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testIndexOfCodeUnit() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);

    /* The closest boundary is found, and the ends are the limits. */
    assert(TRComposedLineGetIndexOfCodeUnit(line, -100.0f) == 0);
    assert(TRComposedLineGetIndexOfCodeUnit(line, 0.0f) == 0);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A / 2.0f - 1.0f) == 0);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A / 2.0f + 1.0f) == 1);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A + 10.0f) == 1);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A + B - 10.0f) == 2);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A + B + C - 1.0f) == 3);
    assert(TRComposedLineGetIndexOfCodeUnit(line, 100000.0f) == 3);

    TRComposedLineRelease(line);
}

namespace {

struct Edges {
    vector<pair<TRFloat, TRFloat>> parts;
};

void collectEdge(void *userData, TRFloat left, TRFloat right) {
    static_cast<Edges *>(userData)->parts.push_back({ left, right });
}

}

void ComposedLineTests::testEnumerateEdges() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);

    Edges edges;
    TRComposedLineEnumerateEdges(line, { 1, 2 }, collectEdge, &edges);
    assert(edges.parts.size() == 1);
    assert(edges.parts[0].first == A && edges.parts[0].second == A + B + C);

    /* The range is clamped to the line, and an empty one has nothing. */
    edges.parts.clear();
    TRComposedLineEnumerateEdges(line, { 0, 100 }, collectEdge, &edges);
    assert(edges.parts.size() == 1);
    assert(edges.parts[0].first == 0.0f && edges.parts[0].second == A + B + C);

    edges.parts.clear();
    TRComposedLineEnumerateEdges(line, { 1, 0 }, collectEdge, &edges);
    TRComposedLineEnumerateEdges(line, { 5, 3 }, collectEdge, &edges);
    assert(edges.parts.empty());

    TRComposedLineRelease(line);
}

void ComposedLineTests::testPenOffset() {
    Fixture f(u"ab c ");
    TRComposedLineRef line = f.line(0, 5);

    /* The width without the trailing whitespace is 4243 - 908 + 908 = 3171 + 1072. */
    TRFloat visible = A + B + Notdef + C;
    assert(TRComposedLineGetWidth(line) == visible + Notdef);
    assert(TRComposedLineGetPenOffset(line, 0.0f, 10000.0f) == 0.0f);
    assert(TRComposedLineGetPenOffset(line, 1.0f, 10000.0f) == 10000.0f - visible);
    assert(TRComposedLineGetPenOffset(line, 0.5f, 10000.0f) == (10000.0f - visible) * 0.5f);

    TRComposedLineRelease(line);

    /* In a right-to-left paragraph the whitespace is at the left of the line, so it moves back. */
    Fixture rtl(u"\u05E9\u05DC ");
    line = rtl.line(0, 3);
    assert(TRComposedLineGetParagraphLevel(line) == 1);
    TRFloat extent = TRComposedLineGetWidth(line);
    TRFloat trailing = TRComposedLineGetTrailingWhitespaceExtent(line);
    assert(trailing == Notdef);
    assert(TRComposedLineGetPenOffset(line, 1.0f, 5000.0f) == (5000.0f - (extent - trailing)) - trailing);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testPaintAttributesSplitRuns() {
    Fixture f(u"abc");
    int first = 1;
    int second = 2;

    TRAttribute color = {};
    color.type = TRAttributeForegroundColor;
    color.value.foregroundColor = TRColorMake(0xFF, 0x11, 0x22, 0x33);
    TRTextSetAttribute(f.text, 1, 1, &color);

    TRAttribute userData = {};
    userData.type = TRAttributeUserData;
    userData.value.userData = &first;
    TRTextSetAttribute(f.text, 0, 2, &userData);
    userData.value.userData = &second;
    TRTextSetAttribute(f.text, 2, 1, &userData);

    TRComposedLineRef line = f.line(0, 3);

    /* There is a run for each part that is painted alike, though the text is shaped as a whole. */
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    TRGlyphRunRef plain = TRComposedLineGetGlyphRun(line, 0);
    TRGlyphRunRef colored = TRComposedLineGetGlyphRun(line, 1);
    TRGlyphRunRef last = TRComposedLineGetGlyphRun(line, 2);

    assert(TRGlyphRunGetCodeUnitRange(plain).index == 0 && TRGlyphRunGetCodeUnitRange(plain).length == 1);
    assert(TRGlyphRunGetCodeUnitRange(colored).index == 1);
    assert(TRGlyphRunGetCodeUnitRange(last).index == 2);

    TRColor value = 0;
    assert(!TRGlyphRunGetForegroundColor(plain, &value));
    assert(TRGlyphRunGetForegroundColor(colored, &value) && value == TRColorMake(0xFF, 0x11, 0x22, 0x33));
    assert(!TRGlyphRunGetForegroundColor(last, &value));

    assert(TRGlyphRunGetUserData(plain) == &first);
    assert(TRGlyphRunGetUserData(colored) == &first);
    assert(TRGlyphRunGetUserData(last) == &second);

    /* Each run starts where the one before it ends, and the line is as wide as all of them. */
    assert(TRGlyphRunGetOrigin(plain).x == 0.0f);
    assert(TRGlyphRunGetOrigin(colored).x == A);
    assert(TRGlyphRunGetOrigin(last).x == A + B);
    assert(TRComposedLineGetWidth(line) == A + B + C);

    /* The glyphs of each run are its own. */
    assert(TRGlyphRunGetGlyphIDsPtr(colored)[0] == 2);
    assert(TRGlyphRunGetGlyphIDsPtr(last)[0] == 3);

    TRComposedLineRelease(line);

    /* A line that covers a part of the paint spans only has the parts of them. */
    line = f.line(1, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 2);
    assert(TRGlyphRunGetCodeUnitRange(TRComposedLineGetGlyphRun(line, 0)).index == 1);
    TRComposedLineRelease(line);
}

void ComposedLineTests::testMixedDirections() {
    Fixture f(u"ab \u05E9\u05DC\u05D5\u05DD cd");
    TRComposedLineRef line = f.line(0, 10);
    size_t count = TRComposedLineGetGlyphRunCount(line);

    assert(count == 4);
    assert(TRComposedLineGetParagraphLevel(line) == 0);

    /* A single right-to-left word keeps its place in a left-to-right line. */
    const size_t starts[] = { 0, 3, 7, 8 };
    const size_t ends[] = { 3, 7, 8, 10 };
    TRFloat origin = 0.0f;

    for (size_t i = 0; i < count; i++) {
        TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, i);
        TRRange range = TRGlyphRunGetCodeUnitRange(run);

        assert(range.index == starts[i] && range.index + range.length == ends[i]);
        assert(TRGlyphRunGetOrigin(run).x == origin);
        origin += TRGlyphRunGetWidth(run);
    }

    assert(TRComposedLineGetWidth(line) == origin);
    assert(TRGlyphRunGetBidiLevel(TRComposedLineGetGlyphRun(line, 1)) == 1);
    assert(TRGlyphRunGetWritingDirection(TRComposedLineGetGlyphRun(line, 1)) == TRWritingDirectionRightToLeft);

    /* Distances inside of the right-to-left word grow from its right edge to the left. */
    TRGlyphRunRef hebrew = TRComposedLineGetGlyphRun(line, 1);
    assert(TRGlyphRunGetDistance(hebrew, 3) == 4 * Notdef);
    assert(TRGlyphRunGetDistance(hebrew, 4) == 3 * Notdef);
    assert(TRGlyphRunGetDistance(hebrew, 7) == 0.0f);

    /*
     * The line positions follow the visual order. A boundary between two runs is that of the run
     * that it starts, so the one after the Hebrew word is at the start of the run that follows
     * it, which is on the far side of the word from where the word starts.
     */
    TRFloat latinWidth = A + B + Notdef;
    assert(TRComposedLineGetDistance(line, 3) == latinWidth + 4 * Notdef);
    assert(TRComposedLineGetDistance(line, 5) == latinWidth + 2 * Notdef);
    assert(TRComposedLineGetDistance(line, 7) == latinWidth + 4 * Notdef);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testRightToLeftParagraph() {
    Fixture f(u"\u05E9\u05DC ab");
    TRComposedLineRef line = f.line(0, 5);

    assert(TRComposedLineGetParagraphLevel(line) == 1);

    /*
     * The Hebrew letters and the space after them are one run at the level of the paragraph, and the
     * Latin letters are embedded at the level above it, so they are on the left.
     */
    assert(TRComposedLineGetGlyphRunCount(line) == 2);
    TRGlyphRunRef latin = TRComposedLineGetGlyphRun(line, 0);
    TRGlyphRunRef hebrew = TRComposedLineGetGlyphRun(line, 1);

    assert(TRGlyphRunGetCodeUnitRange(latin).index == 3 && TRGlyphRunGetCodeUnitRange(latin).length == 2);
    assert(TRGlyphRunGetBidiLevel(latin) == 2);
    assert(TRGlyphRunGetCodeUnitRange(hebrew).index == 0 && TRGlyphRunGetCodeUnitRange(hebrew).length == 3);
    assert(TRGlyphRunGetBidiLevel(hebrew) == 1);

    assert(TRGlyphRunGetOrigin(latin).x == 0.0f);
    assert(TRGlyphRunGetOrigin(hebrew).x == A + B);
    assert(TRComposedLineGetWidth(line) == A + B + 3 * Notdef);

    /* The start of the text is at the right end of the line. */
    assert(TRComposedLineGetDistance(line, 0) == TRComposedLineGetWidth(line));
    assert(TRComposedLineGetDistance(line, 1) == A + B + 2 * Notdef);
    assert(TRComposedLineGetDistance(line, 2) == A + B + Notdef);
    assert(TRComposedLineGetDistance(line, 3) == 0.0f);
    assert(TRComposedLineGetDistance(line, 4) == A);

    /*
     * A position past the ends of a run gives the last or the first index of the run in the order
     * of its code units, in either direction, as it does on the platforms.
     */
    assert(TRComposedLineGetIndexOfCodeUnit(line, TRComposedLineGetWidth(line) + 50.0f) == 3);
    assert(TRComposedLineGetIndexOfCodeUnit(line, -50.0f) == 0);
    assert(TRComposedLineGetIndexOfCodeUnit(line, A + B + 100.0f) == 3);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testRunsAreRelativeToTheirRange() {
    Fixture f(u"abcabc");
    setTestFloat(f.text, 3, 3, TRAttributeScaleX, 2.0f);
    TRComposedLineRef line = f.line(2, 5);

    /* The range goes across two text runs, which are shaped alike except for the scale. */
    assert(TRComposedLineGetGlyphRunCount(line) == 2);

    TRGlyphRunRef first = TRComposedLineGetGlyphRun(line, 0);
    TRGlyphRunRef second = TRComposedLineGetGlyphRun(line, 1);

    assert(TRGlyphRunGetGlyphCount(first) == 1 && TRGlyphRunGetGlyphIDsPtr(first)[0] == 3);
    assert(TRGlyphRunGetGlyphCount(second) == 2);
    assert(TRGlyphRunGetScaleX(first) == 1.0f && TRGlyphRunGetScaleX(second) == 2.0f);
    assert(TRGlyphRunGetWidth(first) == C);
    assert(TRGlyphRunGetWidth(second) == (A + B) * 2.0f);
    assert(TRGlyphRunGetOrigin(second).x == C);
    assert(TRComposedLineGetWidth(line) == C + (A + B) * 2.0f);
    assert(TRGlyphRunGetClusterStart(second, 4) == 4);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testClusterCutByLine() {
    /* The emoji is a cluster of two code units, and a line is cut in the middle of it. */
    Fixture f(u"a\U0001F600" "b");
    TRComposedLineRef line = f.line(2, 4);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    assert(TRGlyphRunGetCodeUnitRange(run).index == 2 && TRGlyphRunGetCodeUnitRange(run).length == 2);
    assert(TRGlyphRunGetStartExtraLength(run) == 1);
    assert(TRGlyphRunGetEndExtraLength(run) == 0);

    /* It has the glyph of the cluster, which is shared with the other side of the cut. */
    assert(TRGlyphRunGetGlyphCount(run) == 2);
    assert(TRGlyphRunGetGlyphIDsPtr(run)[0] == 0 && TRGlyphRunGetGlyphIDsPtr(run)[1] == 2);

    /* The cluster map and the caret edges start at the first code unit of the cluster. */
    assert(TRGlyphRunGetClusterMapCount(run) == 3);
    assert((sizes(TRGlyphRunGetClusterMapPtr(run), 3) == vector<TRUInteger>{ 0, 0, 1 }));

    assert(TRGlyphRunGetClusterStart(run, 2) == 1);
    assert(TRGlyphRunGetClusterEnd(run, 2) == 3);
    assert(TRGlyphRunGetClusterStart(run, 3) == 3);

    /* Distances are measured from the cut, so those inside the cluster are negative or zero. */
    assert(TRGlyphRunGetDistance(run, 2) == 0.0f);
    assert(TRGlyphRunGetDistance(run, 3) == Notdef / 2.0f);
    assert(TRGlyphRunGetDistance(run, 4) == Notdef / 2.0f + B);
    assert(TRGlyphRunGetWidth(run) == Notdef / 2.0f + B);

    TRComposedLineRelease(line);

    /* A line that ends in the middle has extra code units at its end instead. */
    line = f.line(0, 2);
    run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 0 && TRGlyphRunGetEndExtraLength(run) == 1);
    assert(TRGlyphRunGetClusterMapCount(run) == 3);
    assert(TRGlyphRunGetGlyphCount(run) == 2);
    assert(TRGlyphRunGetWidth(run) == A + Notdef / 2.0f);
    TRComposedLineRelease(line);
}

void ComposedLineTests::testJustifiedCopiesAreIndependent() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    GlyphRunRef run = (GlyphRunRef)TRComposedLineGetGlyphRun(line, 0);

    const TRFloat wider[] = { A + 10.0f, B + 20.0f, C + 30.0f };
    GlyphRunRef justified = GlyphRunCreateJustified(run, wider);
    assert(justified != nullptr);

    /* The justified run has its own advances and edges, and shares the glyphs. */
    assert(GlyphRunGetAdvances(justified) == justified->justifiedAdvances);
    assert((floats(TRGlyphRunGetGlyphAdvancesPtr(justified), 3) == vector<TRFloat>{ A + 10, B + 20, C + 30 }));
    assert(TRGlyphRunGetGlyphIDsPtr(justified) == TRGlyphRunGetGlyphIDsPtr(run));
    assert(TRGlyphRunGetWidth(justified) == A + B + C + 60.0f);
    assert(TRGlyphRunGetDistance(justified, 1) == A + 10.0f);
    assert(TRGlyphRunGetDistance(justified, 2) == A + B + 30.0f);

    /* The original is not changed. */
    assert(TRGlyphRunGetWidth(run) == A + B + C);
    assert(TRGlyphRunGetGlyphAdvancesPtr(run)[0] == A);

    /* A copy of a justified run is justified too. */
    GlyphRunRef copy = GlyphRunCreateCopy(justified);
    assert(TRGlyphRunGetWidth(copy) == A + B + C + 60.0f);
    assert(GlyphRunGetAdvances(copy) == copy->justifiedAdvances);
    assert(copy->justifiedAdvances != justified->justifiedAdvances);

    TRGlyphRunRelease(copy);
    TRGlyphRunRelease(justified);
    TRComposedLineRelease(line);
}

void ComposedLineTests::testLineMetricsFromTallestRun() {
    Fixture f(u"abc");
    setTestFloat(f.text, 1, 1, TRAttributePointSize, 4096.0f);
    TRComposedLineRef line = f.line(0, 3);

    /* The line takes the greatest ascent and descent of its runs. */
    assert(TRComposedLineGetGlyphRunCount(line) == 3);
    assert(TRComposedLineGetAscent(line) == 3800.0f);
    assert(TRComposedLineGetDescent(line) == 1000.0f);
    assert(TRGlyphRunGetAscent(TRComposedLineGetGlyphRun(line, 0)) == 1900.0f);
    assert(TRGlyphRunGetAscent(TRComposedLineGetGlyphRun(line, 1)) == 3800.0f);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testBoundingBox() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);

    TRRendererRef renderer = TRRendererCreate();
    TRGlyphCacheRef cache = TRGlyphCacheCreate(1024 * 1024);
    TRRendererSetGlyphCache(renderer, cache);

    TRRect box = TRComposedLineGetBoundingBox(line, renderer);
    assert(box.size.width > 0.0f && box.size.height > 0.0f);

    /* The glyphs are above the baseline, and the box is to the right of the start of the line. */
    assert(box.origin.y < 0.0f);
    assert(box.origin.x + box.size.width <= TRComposedLineGetWidth(line) + 4.0f);

    /* The renderer was set up for the run, and the box is the same for a run alone. */
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    TRRect runBox = TRGlyphRunGetBoundingBox(run, { 0, 3 }, renderer);
    assert(runBox.origin.x == box.origin.x && runBox.size.width == box.size.width);

    /* A box of a part of the glyphs is smaller. */
    TRRect partial = TRGlyphRunGetBoundingBox(run, { 0, 1 }, renderer);
    assert(partial.size.width < runBox.size.width);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRComposedLineRelease(line);
}

namespace {

void computeRoom(void *userData, TRFloat layoutWidth, TRReplacementRoom *room) {
    room->ascent = 300.0f;
    room->descent = 100.0f;
    room->extent = *static_cast<TRFloat *>(userData) + (layoutWidth > 0.0f ? layoutWidth : 0.0f);
}

TRReplacementRef createReplacement(TRFloat *width, bool isBlock, TRFloat leading = 25.0f) {
    TRReplacementCallbacks callbacks = { computeRoom, nullptr };
    return TRReplacementCreate(&callbacks, width, leading, isBlock);
}

}

void ComposedLineTests::testReplacementLines() {
    Fixture f(u"a\uFFFCb");
    TRFloat width = 500.0f;
    TRReplacementRef replacement = createReplacement(&width, false);

    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTextSetAttribute(f.text, 1, 1, &attribute);

    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 1);
    assert(TRGlyphRunGetReplacement(run) == replacement);
    assert(TRGlyphRunGetWidth(run) == 500.0f);
    assert(TRGlyphRunGetAscent(run) == 300.0f && TRGlyphRunGetDescent(run) == 100.0f);
    assert(TRGlyphRunGetLeading(run) == 25.0f);
    assert(TRGlyphRunGetGlyphCount(run) == 1);
    assert(TRGlyphRunGetOrigin(run).x == A);

    /* The line is as wide as all of it, and as tall as its tallest run. */
    assert(TRComposedLineGetWidth(line) == A + 500.0f + B);
    assert(TRComposedLineGetAscent(line) == 1900.0f);
    assert(TRComposedLineGetLeading(line) == 25.0f);
    assert(TRComposedLineGetDistance(line, 2) == A + 500.0f);

    /* Its box is that of its room. */
    TRRendererRef renderer = TRRendererCreate();
    TRRect box = TRGlyphRunGetBoundingBox(run, { 0, 1 }, renderer);
    assert(box.size.width == 500.0f && box.size.height == 425.0f);
    TRRendererRelease(renderer);

    TRComposedLineRelease(line);
    TRReplacementRelease(replacement);
}

void ComposedLineTests::testBlockLine() {
    Fixture f(u"\uFFFC\n");
    TRFloat width = 0.0f;
    TRReplacementRef block = createReplacement(&width, true, 0.0f);

    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = block;
    TRTextSetAttribute(f.text, 0, 1, &attribute);

    TRComposedLineRef line = f.line(0, 2);

    /* The line of a block has the metrics of the block, not those of the newline after it. */
    assert(TRComposedLineIsBlock(line));
    assert(TRComposedLineGetAscent(line) == 300.0f);
    assert(TRComposedLineGetDescent(line) == 100.0f);
    assert(TRComposedLineGetLeading(line) == 0.0f);

    TRComposedLineRelease(line);

    /* A line that is made for a frame gets the room that the width of the frame gives. */
    line = LineResolverCreateSimpleLine((TypesetterRef)f.typesetter, 0, 2, TRTrue, 120.0f);
    assert(TRComposedLineGetWidth(line) >= 120.0f);
    assert(TRGlyphRunGetWidth(TRComposedLineGetGlyphRun(line, 0)) == 120.0f);
    TRComposedLineRelease(line);

    TRReplacementRelease(block);
}

static TRComposedLineRef makeToken(TRTypesetterRef typesetter, TRRange range, TRTruncationPlace place,
    const char16_t *token) {
    TRComposedLineRef line = TRTypesetterCreateTruncationToken(typesetter, range, place, token,
        std::char_traits<char16_t>::length(token), TRStringEncodingUTF16);
    assert(line != nullptr);

    return line;
}

void ComposedLineTests::testTruncation() {
    Fixture f(u"abcabc");
    TRTypesetterRef typesetter = f.make();
    const TRRange all = { 0, 6 };
    const TRFloat extent = 3500.0f;

    /* The token is 'c', so 2428 units are left for the text. */
    TRComposedLineRef token = makeToken(typesetter, all, TRTruncationPlaceEnd, u"c");
    assert(TRComposedLineGetWidth(token) == C);

    TRComposedLineRef end = TRTypesetterCreateTruncatedLine(typesetter, all, extent,
        TRBreakModeCharacter, TRTruncationPlaceEnd, token);
    assert(end != nullptr);
    assert(TRComposedLineGetWidth(end) == A + B + C);
    assert(TRComposedLineGetCodeUnitRange(end).index == 0);
    assert(TRComposedLineGetCodeUnitRange(end).length == 2);
    assert(TRComposedLineGetGlyphRunCount(end) == 2);

    TRComposedLineRef start = TRTypesetterCreateTruncatedLine(typesetter, all, extent,
        TRBreakModeCharacter, TRTruncationPlaceStart, token);
    assert(start != nullptr);
    assert(TRComposedLineGetWidth(start) == C + B + C);
    assert(TRComposedLineGetCodeUnitRange(start).index == 4);
    assert(TRComposedLineGetCodeUnitRange(start).length == 2);
    assert(TRComposedLineGetGlyphRunCount(start) == 2);

    TRComposedLineRef middle = TRTypesetterCreateTruncatedLine(typesetter, all, extent,
        TRBreakModeCharacter, TRTruncationPlaceMiddle, token);
    assert(middle != nullptr);
    assert(TRComposedLineGetWidth(middle) == A + C + C);
    assert(TRComposedLineGetCodeUnitRange(middle).index == 0);
    assert(TRComposedLineGetCodeUnitRange(middle).length == 6);
    assert(TRComposedLineGetGlyphRunCount(middle) == 3);

    /* Nothing is cut out if the text fits. */
    TRComposedLineRef fits = TRTypesetterCreateTruncatedLine(typesetter, all, 10000.0f,
        TRBreakModeCharacter, TRTruncationPlaceEnd, token);
    assert(fits != nullptr);
    assert(TRComposedLineGetWidth(fits) == 2.0f * (A + B + C));
    assert(TRComposedLineGetGlyphRunCount(fits) == 1);

    TRComposedLineRelease(fits);
    TRComposedLineRelease(middle);
    TRComposedLineRelease(start);
    TRComposedLineRelease(end);
    TRComposedLineRelease(token);
}

void ComposedLineTests::testTruncationToken() {
    Fixture f(u"abc");
    TRTypesetterRef typesetter = f.make();

    /* The default token has three dots, as the test font lacks the ellipsis character. */
    TRComposedLineRef token = TRTypesetterCreateTruncationToken(typesetter, { 0, 3 },
        TRTruncationPlaceEnd, nullptr, 0, TRStringEncodingUTF16);
    assert(token != nullptr);
    assert(TRComposedLineGetWidth(token) == 3.0f * Notdef);
    TRComposedLineRelease(token);

    token = makeToken(typesetter, { 0, 3 }, TRTruncationPlaceEnd, u"bb");
    assert(TRComposedLineGetWidth(token) == 2.0f * B);
    TRComposedLineRelease(token);
}

void ComposedLineTests::testJustifiedLine() {
    Fixture f(u"a b ");
    TRTypesetterRef typesetter = f.make();
    const TRFloat natural = A + Notdef + B;

    TRComposedLineRef full = TRTypesetterCreateJustifiedLine(typesetter, { 0, 3 }, 1.0f, 4000.0f);
    assert(full != nullptr);
    assert(std::fabs(TRComposedLineGetWidth(full) - 4000.0f) < 0.01f);

    /* Only the space is widened. */
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(full, 0);
    assert(TRGlyphRunGetGlyphAdvancesPtr(run)[0] == A);
    assert(std::fabs(TRGlyphRunGetGlyphAdvancesPtr(run)[1] - (Notdef + 4000.0f - natural)) < 0.01f);
    assert(TRGlyphRunGetGlyphAdvancesPtr(run)[2] == B);

    TRComposedLineRef half = TRTypesetterCreateJustifiedLine(typesetter, { 0, 3 }, 0.5f, 4000.0f);
    assert(half != nullptr);
    assert(std::fabs(TRComposedLineGetWidth(half) - (natural + (4000.0f - natural) / 2.0f)) < 0.01f);

    /* A trailing space is not counted for the justification. */
    TRComposedLineRef trailing = TRTypesetterCreateJustifiedLine(typesetter, { 0, 4 }, 1.0f, 4000.0f);
    assert(trailing != nullptr);
    assert(std::fabs(TRComposedLineGetWidth(trailing) - 4000.0f - Notdef) < 0.01f);

    /* Without inner spaces, the line stays as it is. */
    TRComposedLineRef none = TRTypesetterCreateJustifiedLine(typesetter, { 0, 1 }, 1.0f, 4000.0f);
    assert(none != nullptr);
    assert(TRComposedLineGetWidth(none) == A);

    TRComposedLineRelease(none);
    TRComposedLineRelease(trailing);
    TRComposedLineRelease(half);
    TRComposedLineRelease(full);
}

void ComposedLineTests::testLinesOutliveTypesetter() {
    TRComposedLineRef line;

    {
        Fixture f(u"abc");
        line = f.line(0, 3);
    }

    /* The line keeps the shaped text that it needs, and the typeface. */
    assert(TRComposedLineGetWidth(line) == A + B + C);
    assert(TRGlyphRunGetGlyphIDsPtr(TRComposedLineGetGlyphRun(line, 0))[1] == 2);
    assert(TRGlyphRunGetTypeface(TRComposedLineGetGlyphRun(line, 0)) != nullptr);
    assert(TRGlyphRunGetDistance(TRComposedLineGetGlyphRun(line, 0), 2) == A + B);

    TRComposedLineRelease(line);
}

void ComposedLineTests::testConcurrentLines() {
    Fixture f(u"ab c ab ca b");
    f.make();
    atomic<int> failures{0};
    vector<thread> threads;

    /* A typesetter is shared, and each thread makes the lines and uses them. */
    for (size_t t = 0; t < 8; t++) {
        threads.emplace_back([&]() {
            for (size_t i = 0; i < 100; i++) {
                TRComposedLineRef line = TRTypesetterCreateSimpleLine(f.typesetter, { 1, 9 });

                if (!line || TRComposedLineGetCodeUnitRange(line).length != 9
                        || TRComposedLineGetDistance(line, 10) <= TRComposedLineGetDistance(line, 2)) {
                    failures++;
                }

                if (line) {
                    TRComposedLineRelease(line);
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(failures == 0);
}

#ifdef STANDALONE_TESTING

int main() {
    ComposedLineTests tests;
    tests.run();

    return 0;
}

#endif
