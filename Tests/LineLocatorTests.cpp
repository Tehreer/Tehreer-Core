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
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRFrameResolver.h>
#include <Tehreer/TRLineLocator.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

#include "TestText.h"
#include "TestTypeface.h"

#include "LineLocatorTests.h"

using namespace std;
using namespace Tehreer;

static TRFloat lineTop(TRComposedLineRef line) {
    return TRComposedLineGetOrigin(line).y - TRComposedLineGetAscent(line);
}

static TRFloat lineBottom(TRComposedLineRef line) {
    return TRComposedLineGetOrigin(line).y + TRComposedLineGetDescent(line)
         + TRComposedLineGetLeading(line);
}

/*
 * In the test font, "abc" is 3335 units wide at a point size of 2048, and the space shows the
 * notdef glyph that is 908 units wide. A frame that is 3500 units wide holds one word per line, and
 * every line is 2400 units tall.
 */
constexpr TRFloat EmSize = 2048.0f;
constexpr TRFloat FrameWidth = 3500.0f;
constexpr TRFloat LineHeight = 2400.0f;
constexpr TRFloat BandHeight = 512.0f;

static TRSize makeSize(TRFloat width, TRFloat height) {
    TRSize size = { width, height };

    return size;
}

static TRRect makeRect(TRFloat x, TRFloat y, TRFloat width, TRFloat height) {
    TRRect rect = { { x, y }, { width, height } };

    return rect;
}

static u16string makeWords(size_t count) {
    u16string string;

    for (size_t i = 0; i < count; i++) {
        string += (i == 0 ? u"abc" : u" abc");
    }

    return string;
}

struct Fixture {
    TRTypefaceRef typeface;
    TRMutableTextRef text;
    TRTypesetterRef typesetter;
    TRFrameResolverRef resolver;
    TRRendererRef renderer;

    explicit Fixture(const u16string &string) {
        typeface = createTestTypeface("Roboto-Regular.abc.ttf");
        text = makeTestText(string, typeface, EmSize);
        typesetter = TRTypesetterCreate(text, nullptr, 0);
        assert(typesetter != nullptr);

        TRSize frameSize = { FrameWidth, 1.0e9f };
        resolver = TRFrameResolverCreate();
        assert(resolver != nullptr);
        TRFrameResolverSetTypesetter(resolver, typesetter);
        TRFrameResolverSetFrameSize(resolver, frameSize);

        renderer = TRRendererCreate();
        assert(renderer != nullptr);
        TRRendererSetTypeface(renderer, typeface);
        TRRendererSetTypeSize(renderer, EmSize);
    }

    TRComposedFrameRef frame() {
        TRComposedFrameRef result = TRFrameResolverCreateFrame(resolver, 0, TRTextGetLength(text));
        assert(result != nullptr);

        return result;
    }

    TRLineLocatorRef locator(TRComposedFrameRef frame) {
        TRLineLocatorRef result = TRLineLocatorCreate(frame, renderer);
        assert(result != nullptr);

        return result;
    }

    ~Fixture() {
        TRRendererRelease(renderer);
        TRFrameResolverRelease(resolver);
        TRTypesetterRelease(typesetter);
        TRTextRelease(text);
        TRTypefaceRelease(typeface);
    }
};

static vector<TRUInteger> collect(TRLineLocatorRef locator, TRRect rect) {
    vector<TRUInteger> lines;

    TRBoolean isEnumerated = TRLineLocatorEnumerateLines(locator, rect,
        [](void *userData, TRUInteger lineIndex, TRBoolean *stop) {
            (void)stop;
            static_cast<vector<TRUInteger> *>(userData)->push_back(lineIndex);
        }, &lines);
    assert(isEnumerated == TRTrue);

    return lines;
}

static vector<TRRect> getBoxes(TRLineLocatorRef locator) {
    vector<TRRect> boxes;

    for (TRUInteger i = 0; i < TRLineLocatorGetLineCount(locator); i++) {
        TRRect box;
        TRBoolean isSet = TRLineLocatorGetLineBox(locator, i, &box);
        assert(isSet == TRTrue);
        boxes.push_back(box);
    }

    return boxes;
}

static TRFloat getBottom(const TRRect &box) {
    return box.origin.y + box.size.height;
}

static bool intersects(const TRRect &a, const TRRect &b) {
    return a.origin.x < b.origin.x + b.size.width && b.origin.x < a.origin.x + a.size.width
        && a.origin.y < getBottom(b) && b.origin.y < getBottom(a);
}

/* Finds the lines the slow way, by looking at every box. */
static vector<TRUInteger> bruteForce(const vector<TRRect> &boxes, TRRect rect) {
    vector<TRUInteger> lines;

    for (TRUInteger i = 0; i < boxes.size(); i++) {
        if (intersects(boxes[i], rect)) {
            lines.push_back(i);
        }
    }

    return lines;
}

static bool contains(const vector<TRUInteger> &lines, TRUInteger lineIndex) {
    return find(lines.begin(), lines.end(), lineIndex) != lines.end();
}

static void assertSameLines(TRLineLocatorRef locator, const vector<TRRect> &boxes, TRRect rect) {
    vector<TRUInteger> found = collect(locator, rect);
    vector<TRUInteger> expected = bruteForce(boxes, rect);

    /* No line is passed twice. */
    sort(found.begin(), found.end());
    assert(adjacent_find(found.begin(), found.end()) == found.end());

    assert(found == expected);
}

/* Walks over rects of several sizes and places, and checks each against the slow way. */
static void assertAllRects(TRLineLocatorRef locator, TRFloat height) {
    vector<TRRect> boxes = getBoxes(locator);
    const TRFloat heights[] = { 0.0f, 1.0f, 100.0f, 512.0f, 513.0f, 3000.0f, 20000.0f };
    const TRFloat widths[] = { 0.0f, 1.0f, 100.0f, 3500.0f, 5000.0f };

    for (TRFloat h : heights) {
        for (TRFloat w : widths) {
            for (TRFloat y = -1000.0f; y < height + 1000.0f; y += 337.0f) {
                assertSameLines(locator, boxes, makeRect(-200.0f, y, w + 200.0f, h));
                assertSameLines(locator, boxes, makeRect(1700.0f, y, w, h));
            }
        }
    }
}

void LineLocatorTests::run() {
    testSingleLine();
    testManyLines();
    testBoundaries();
    testRectsOutside();
    testOverlappingBoxes();
    testFirstLineIndexBelow();
    testStop();
    testInvalidArguments();
    testInfiniteRects();
    testBandLimit();
    testNegativeBoxes();
    testConcurrentQueries();
    testReferenceCounting();
}

void LineLocatorTests::testSingleLine() {
    Fixture f(u"abc");
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);

    assert(TRLineLocatorGetLineCount(locator) == 1);

    /* The box spans the frame, and the line, with integral edges. */
    TRRect box = {};
    assert(TRLineLocatorGetLineBox(locator, 0, &box) == TRTrue);
    assert(box.origin.x <= 0.0f);
    assert(box.origin.y <= 0.0f);
    assert(box.origin.x + box.size.width >= FrameWidth);
    assert(getBottom(box) >= LineHeight);
    assert(box.origin.x == floorf(box.origin.x) && box.origin.y == floorf(box.origin.y));
    assert(box.size.width == floorf(box.size.width) && box.size.height == floorf(box.size.height));

    /* Any rect inside of the box finds the line. */
    vector<TRUInteger> lines = collect(locator, makeRect(10.0f, 10.0f, 10.0f, 10.0f));
    assert(lines.size() == 1 && lines[0] == 0);

    assert(TRLineLocatorGetFirstLineIndexBelow(locator, 0.0f) == 0);
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, getBottom(box)) == TRInvalidIndex);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testManyLines() {
    constexpr size_t Count = 60;

    Fixture f(makeWords(Count));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);

    /* The lines are 144000 units tall together, which are more than 280 bands. */
    assert(TRComposedFrameGetLineCount(frame) == Count);
    assert(TRLineLocatorGetLineCount(locator) == Count);

    vector<TRRect> boxes = getBoxes(locator);
    for (TRUInteger i = 0; i < Count; i++) {
        TRComposedLineRef line = TRComposedFrameGetLine(frame, i);
        assert(boxes[i].origin.y <= lineTop(line));
        assert(getBottom(boxes[i]) >= lineBottom(line));
    }

    /* A rect in the middle of a line finds that line only, in whichever band it is. */
    for (TRUInteger i = 0; i < Count; i++) {
        TRComposedLineRef line = TRComposedFrameGetLine(frame, i);
        TRFloat middle = (lineTop(line) + lineBottom(line)) / 2.0f;
        vector<TRUInteger> lines = collect(locator, makeRect(100.0f, middle, 50.0f, 1.0f));
        assert(lines.size() == 1 && lines[0] == i);
    }

    /* A rect that covers everything finds every line once, in the order of the frame. */
    vector<TRUInteger> all = collect(locator, makeRect(-10.0f, -10.0f, 1.0e9f, 1.0e9f));
    assert(all.size() == Count);
    for (TRUInteger i = 0; i < Count; i++) {
        assert(all[i] == i);
    }

    /* A rect that spans several bands finds the lines of them. */
    vector<TRUInteger> some = collect(locator, makeRect(0.0f, 5000.0f, 100.0f, 5000.0f));
    assert(some.size() >= 2 && some.size() <= 4);
    assert(is_sorted(some.begin(), some.end()));

    assertAllRects(locator, Count * LineHeight);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testBoundaries() {
    Fixture f(makeWords(8));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);

    for (TRUInteger i = 0; i < boxes.size(); i++) {
        TRFloat top = boxes[i].origin.y;
        TRFloat bottom = getBottom(boxes[i]);
        TRFloat left = boxes[i].origin.x;
        TRFloat right = left + boxes[i].size.width;

        /* A rect that only touches the box is not found, one that overlaps by a unit is. */
        assert(!contains(collect(locator, makeRect(left, bottom, 10.0f, 10.0f)), i));
        assert(contains(collect(locator, makeRect(left, bottom - 1.0f, 10.0f, 10.0f)), i));

        assert(!contains(collect(locator, makeRect(left, top - 10.0f, 10.0f, 10.0f)), i));
        assert(contains(collect(locator, makeRect(left, top - 10.0f, 10.0f, 11.0f)), i));

        assert(!contains(collect(locator, makeRect(right, top, 10.0f, 10.0f)), i));
        assert(contains(collect(locator, makeRect(right - 1.0f, top, 10.0f, 10.0f)), i));

        assert(!contains(collect(locator, makeRect(left - 10.0f, top, 10.0f, 10.0f)), i));
        assert(contains(collect(locator, makeRect(left - 10.0f, top, 11.0f, 10.0f)), i));
    }

    /* The rects on the edges of the bands agree with the slow way. */
    for (TRFloat edge = 0.0f; edge < 8 * LineHeight; edge += BandHeight) {
        assertSameLines(locator, boxes, makeRect(0.0f, edge, 10.0f, 0.0f));
        assertSameLines(locator, boxes, makeRect(0.0f, edge - 1.0f, 10.0f, 2.0f));
        assertSameLines(locator, boxes, makeRect(0.0f, edge, 10.0f, BandHeight));
        assertSameLines(locator, boxes, makeRect(0.0f, edge - BandHeight, 10.0f, BandHeight));
    }

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testRectsOutside() {
    Fixture f(makeWords(20));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);
    TRFloat bottom = getBottom(boxes.back());
    TRFloat inf = numeric_limits<TRFloat>::infinity();

    assert(collect(locator, makeRect(0.0f, -5000.0f, 3000.0f, 100.0f)).empty());
    assert(collect(locator, makeRect(0.0f, -100000.0f, 3000.0f, 100.0f)).empty());
    assert(collect(locator, makeRect(0.0f, bottom, 3000.0f, 100.0f)).empty());
    assert(collect(locator, makeRect(0.0f, bottom + 1000000.0f, 3000.0f, 100.0f)).empty());
    assert(collect(locator, makeRect(FrameWidth + 1000.0f, 0.0f, 100.0f, 100000.0f)).empty());
    assert(collect(locator, makeRect(-5000.0f, 0.0f, 100.0f, 100000.0f)).empty());

    /* Rects that are infinitely tall still work. */
    assert(collect(locator, makeRect(0.0f, 0.0f, 100.0f, inf)).size() == 20);
    assert(collect(locator, makeRect(0.0f, bottom, 100.0f, inf)).empty());
    assert(collect(locator, makeRect(0.0f, -inf, 100.0f, 0.0f)).empty());

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testOverlappingBoxes() {
    Fixture f(makeWords(40));
    TRFrameResolverSetLineHeightMultiplier(f.resolver, 0.3f);
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);

    /* The ink of the glyphs is taller than the lines, so the boxes cover more than their lines. */
    size_t overlapCount = 0;
    for (TRUInteger i = 0; i < boxes.size(); i++) {
        TRComposedLineRef line = TRComposedFrameGetLine(frame, i);
        assert(boxes[i].origin.y < lineTop(line)
            || getBottom(boxes[i]) > lineBottom(line));

        if (i + 1 < boxes.size() && intersects(boxes[i], boxes[i + 1])) {
            overlapCount++;
        }
    }
    assert(overlapCount > 0);

    /* A rect finds every line that its box touches, not only the lines it is within. */
    for (TRUInteger i = 0; i < boxes.size(); i++) {
        TRFloat middle = boxes[i].origin.y + boxes[i].size.height / 2.0f;
        assert(contains(collect(locator, makeRect(0.0f, middle, 100.0f, 1.0f)), i));
    }

    assertAllRects(locator, getBottom(boxes.back()));

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testFirstLineIndexBelow() {
    Fixture f(makeWords(30));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);

    /* The boxes are one below the other, so the first line that reaches below a y is its own. */
    for (TRUInteger i = 0; i < boxes.size(); i++) {
        TRFloat bottom = getBottom(boxes[i]);
        TRUInteger next = (i + 1 < boxes.size() ? i + 1 : TRInvalidIndex);

        assert(TRLineLocatorGetFirstLineIndexBelow(locator, bottom - 1.0f) == i);
        assert(TRLineLocatorGetFirstLineIndexBelow(locator, bottom - 0.5f) == i);
        assert(TRLineLocatorGetFirstLineIndexBelow(locator, bottom) == next);
    }

    TRFloat last = getBottom(boxes.back());
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, -1.0e9f) == 0);
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, -1.0f) == 0);
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, last) == TRInvalidIndex);
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, 1.0e9f) == TRInvalidIndex);
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, numeric_limits<TRFloat>::quiet_NaN())
        == TRInvalidIndex);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);

    /* With overlapping boxes, the line that is found reaches below the y, and none is missed. */
    Fixture g(makeWords(40));
    TRFrameResolverSetLineHeightMultiplier(g.resolver, 0.3f);
    frame = g.frame();
    locator = g.locator(frame);
    boxes = getBoxes(locator);

    for (TRFloat y = -100.0f; y < 40 * LineHeight; y += 123.0f) {
        TRUInteger found = TRLineLocatorGetFirstLineIndexBelow(locator, y);
        bool hasBelow = false;

        for (const TRRect &box : boxes) {
            hasBelow = hasBelow || (getBottom(box) > y);
        }

        if (hasBelow) {
            assert(found < boxes.size());
            assert(getBottom(boxes[found]) > y);
        } else {
            assert(found == TRInvalidIndex);
        }
    }

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testStop() {
    Fixture f(makeWords(10));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);

    struct Counter {
        size_t calls;
        size_t stopAt;
    } counter = { 0, 3 };

    TRBoolean isEnumerated = TRLineLocatorEnumerateLines(locator,
        makeRect(-10.0f, -10.0f, 1.0e6f, 1.0e6f),
        [](void *userData, TRUInteger lineIndex, TRBoolean *stop) {
            Counter *counter = static_cast<Counter *>(userData);
            assert(*stop == TRFalse);
            assert(lineIndex == counter->calls);

            counter->calls += 1;
            if (counter->calls == counter->stopAt) {
                *stop = TRTrue;
            }
        }, &counter);

    assert(isEnumerated == TRTrue);
    assert(counter.calls == 3);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testInvalidArguments() {
    Fixture f(makeWords(3));
    TRComposedFrameRef frame = f.frame();

    assert(TRLineLocatorCreate(nullptr, f.renderer) == nullptr);
    assert(TRLineLocatorCreate(frame, nullptr) == nullptr);

    TRLineLocatorRef locator = f.locator(frame);
    size_t calls = 0;
    auto count = [](void *userData, TRUInteger, TRBoolean *) {
        *static_cast<size_t *>(userData) += 1;
    };
    TRFloat nan = numeric_limits<TRFloat>::quiet_NaN();
    TRFloat inf = numeric_limits<TRFloat>::infinity();

    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, 100, 100), nullptr, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, -1, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, 100, -1), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(nan, 0, 100, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, nan, 100, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, nan, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, 100, nan), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(nan, 0, inf, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, nan, 100, inf), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, -inf, 100), count, &calls) == TRFalse);
    assert(TRLineLocatorEnumerateLines(locator, makeRect(0, 0, 100, -inf), count, &calls) == TRFalse);
    assert(calls == 0);

    TRRect box = makeRect(7, 8, 9, 10);
    assert(TRLineLocatorGetLineBox(locator, 3, &box) == TRFalse);
    assert(TRLineLocatorGetLineBox(locator, TRInvalidIndex, &box) == TRFalse);
    assert(box.origin.x == 7 && box.origin.y == 8 && box.size.width == 9 && box.size.height == 10);
    assert(TRLineLocatorGetLineBox(locator, 0, nullptr) == TRFalse);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testInfiniteRects() {
    Fixture f(makeWords(30));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);
    TRFloat inf = numeric_limits<TRFloat>::infinity();
    vector<TRUInteger> all;

    for (TRUInteger i = 0; i < boxes.size(); i++) {
        all.push_back(i);
    }

    /* Everything is asked with an infinite origin and size. */
    assert(collect(locator, makeRect(-inf, -inf, inf, inf)) == all);
    assert(collect(locator, makeRect(0.0f, -inf, 100.0f, inf)) == all);
    assert(collect(locator, makeRect(-inf, 0.0f, inf, getBottom(boxes.back()))) == all);
    assert(collect(locator, makeRect(0.0f, 0.0f, inf, inf)) == all);

    /* An infinite size reaches only downward from the origin. */
    TRFloat y = boxes[10].origin.y + 1.0f;
    assertSameLines(locator, boxes, makeRect(0.0f, y, 100.0f, 1.0e30f));
    assert(collect(locator, makeRect(0.0f, y, 100.0f, inf)) == bruteForce(boxes, makeRect(0.0f, y, 100.0f, 1.0e30f)));
    assert(collect(locator, makeRect(-inf, y, inf, 100.0f)) == bruteForce(boxes, makeRect(-1.0e30f, y, 2.0e30f, 100.0f)));
    assert(collect(locator, makeRect(0.0f, y, 100.0f, inf)).front() != 0);
    assert(collect(locator, makeRect(0.0f, getBottom(boxes.back()), 100.0f, inf)).empty());

    /* An infinite origin is outside of the frame. */
    assert(collect(locator, makeRect(0.0f, inf, 100.0f, inf)).empty());
    assert(collect(locator, makeRect(0.0f, -inf, 100.0f, 100.0f)).empty());
    assert(collect(locator, makeRect(inf, 0.0f, inf, inf)).empty());

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testBandLimit() {
    /* The bands are limited to 2^22, which are 2^31 units. */
    {
        Fixture f(makeWords(3));
        TRFrameResolverSetFrameSize(f.resolver, makeSize(FrameWidth, 1.0e14f));
        TRFrameResolverSetExtraLineSpacing(f.resolver, 1.0e12f);
        TRComposedFrameRef frame = f.frame();

        assert(TRLineLocatorCreate(frame, f.renderer) == nullptr);

        TRComposedFrameRelease(frame);
    }

    {
        Fixture f(makeWords(3));
        TRFrameResolverSetFrameSize(f.resolver, makeSize(FrameWidth, 1.0e14f));
        TRFrameResolverSetExtraLineSpacing(f.resolver, 1.0e11f);
        TRComposedFrameRef frame = f.frame();

        assert(TRLineLocatorCreate(frame, f.renderer) == nullptr);

        TRComposedFrameRelease(frame);
    }

    /* Three lines in a frame that is 1.95 billion units tall still fit, in 3.8 million bands. */
    {
        Fixture f(makeWords(3));
        TRFrameResolverSetFrameSize(f.resolver, makeSize(FrameWidth, 1.0e14f));
        TRFrameResolverSetExtraLineSpacing(f.resolver, 6.5e8f);
        TRComposedFrameRef frame = f.frame();
        TRLineLocatorRef locator = f.locator(frame);
        vector<TRRect> boxes = getBoxes(locator);

        assert(boxes.size() == 3);
        assert(getBottom(boxes.back()) > 1.9e9f && getBottom(boxes.back()) < 2.1e9f);
        assert(collect(locator, makeRect(0.0f, 0.0f, 10.0f, 10.0f)) == vector<TRUInteger>({ 0 }));
        assertSameLines(locator, boxes, makeRect(0.0f, 6.0e8f, 10.0f, 1.0e9f));
        assertSameLines(locator, boxes, makeRect(0.0f, 1.5e9f, 10.0f, 1.0e9f));
        assert(TRLineLocatorGetFirstLineIndexBelow(locator, 5.0e8f) == 0);
        assert(TRLineLocatorGetFirstLineIndexBelow(locator, 1.0e9f) == 1);

        TRLineLocatorRelease(locator);
        TRComposedFrameRelease(frame);
    }

    /* A tall frame of many lines, ten million units, works. */
    {
        Fixture f(makeWords(4200));
        TRComposedFrameRef frame = f.frame();
        TRLineLocatorRef locator = f.locator(frame);
        vector<TRRect> boxes = getBoxes(locator);

        assert(getBottom(boxes.back()) > 1.0e7f);
        assertSameLines(locator, boxes, makeRect(0.0f, 5.0e6f, 3500.0f, 3000.0f));

        TRLineLocatorRelease(locator);
        TRComposedFrameRelease(frame);
    }
}

void LineLocatorTests::testNegativeBoxes() {
    /* A line height far smaller than the ink puts the ink of line 0 above the top of the frame. */
    Fixture f(makeWords(20));
    TRFrameResolverSetLineHeightMultiplier(f.resolver, 0.1f);
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);

    assert(boxes[0].origin.y < 0.0f);

    assertAllRects(locator, getBottom(boxes.back()));
    assert(contains(collect(locator, makeRect(0.0f, boxes[0].origin.y, 10.0f, 1.0f)), 0));
    assert(TRLineLocatorGetFirstLineIndexBelow(locator, boxes[0].origin.y - 1.0f) == 0);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testConcurrentQueries() {
    constexpr size_t NumThreads = 8;
    constexpr size_t Rounds = 200;

    Fixture f(makeWords(300));
    TRFrameResolverSetLineHeightMultiplier(f.resolver, 0.5f);
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);
    vector<TRRect> boxes = getBoxes(locator);
    TRFloat height = getBottom(boxes.back());

    vector<TRRect> rects;
    vector<vector<TRUInteger>> expected;
    vector<TRUInteger> expectedBelow;
    for (TRFloat y = -500.0f; y < height; y += 977.0f) {
        TRRect rect = makeRect(0.0f, y, 3500.0f, 1500.0f);
        rects.push_back(rect);
        expected.push_back(collect(locator, rect));
        expectedBelow.push_back(TRLineLocatorGetFirstLineIndexBelow(locator, y));
    }

    atomic<size_t> mismatches(0);
    vector<thread> threads;

    for (size_t t = 0; t < NumThreads; t++) {
        threads.emplace_back([&]() {
            for (size_t r = 0; r < Rounds; r++) {
                for (size_t i = 0; i < rects.size(); i++) {
                    TRUInteger below = TRLineLocatorGetFirstLineIndexBelow(locator, rects[i].origin.y);

                    if (collect(locator, rects[i]) != expected[i]) {
                        mismatches++;
                    }
                    if (below != expectedBelow[i]) {
                        mismatches++;
                    }
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(mismatches == 0);

    TRLineLocatorRelease(locator);
    TRComposedFrameRelease(frame);
}

void LineLocatorTests::testReferenceCounting() {
    Fixture f(makeWords(5));
    TRComposedFrameRef frame = f.frame();
    TRLineLocatorRef locator = f.locator(frame);

    /* The locator does not need the frame after it is made. */
    TRComposedFrameRelease(frame);

    assert(TRLineLocatorRetain(locator) == locator);
    TRLineLocatorRelease(locator);

    assert(TRLineLocatorGetLineCount(locator) == 5);
    assert(collect(locator, makeRect(0.0f, 0.0f, 10.0f, 10.0f)).size() == 1);

    TRLineLocatorRelease(locator);
}

#ifdef STANDALONE_TESTING

int main() {
    LineLocatorTests tests;
    tests.run();

    return 0;
}

#endif
