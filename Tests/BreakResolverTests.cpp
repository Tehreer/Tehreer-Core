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
#include <string>
#include <vector>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

#include "TestText.h"
#include "TestTypeface.h"

#include "BreakResolverTests.h"

using namespace std;
using namespace Tehreer;

void BreakResolverTests::run() {
    testForwardLineBreaks();
    testForwardTrailingWhitespace();
    testForwardCharacterBreaks();
    testForwardAtLeastOneCharacter();
    testForwardSubranges();
    testForwardStopsAtParagraph();
    testBackwardLineBreaks();
    testBackwardCharacterBreaks();
    testBackwardAtLeastOneCharacter();
    testBackwardStopsAtParagraph();
    testSurrogatePairsAreNotCut();
    testMixedSizes();
    testBlocksEndLinesBeforeThem();
    testBlockLines();
    testWalkingForwardCoversText();
    testWalkingBackwardCoversText();
}

/*
 * In the test font, the glyphs 'a', 'b' and 'c' are 1114, 1149 and 1072 units wide, and every other
 * character, including the space, shows the notdef glyph that is 908 units wide. A point size of
 * 2048 gives these widths as they are. The text of most tests is "ab c ab", whose characters are
 * 1114, 1149, 908, 1072, 908, 1114 and 1149 units wide, and whose line breaks are after the
 * spaces, at 3 and 5.
 */
constexpr TRFloat EmSize = 2048.0f;

struct Fixture {
    TRTypefaceRef typeface;
    TRMutableTextRef text;
    TRTypesetterRef typesetter;

    explicit Fixture(const u16string &string) {
        typeface = createTestTypeface("Roboto-Regular.abc.ttf");
        text = makeTestText(string, typeface, EmSize);
        typesetter = TRTypesetterCreate(text, nullptr, 0);
        assert(typesetter != nullptr);
    }

    ~Fixture() {
        TRTypesetterRelease(typesetter);
        TRTextRelease(text);
        TRTypefaceRelease(typeface);
    }

    TRUInteger forward(TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode mode = TRBreakModeLine) {
        return TRTypesetterSuggestForwardBreak(typesetter, { start, end - start }, extent, mode);
    }

    TRUInteger backward(TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode mode = TRBreakModeLine) {
        return TRTypesetterSuggestBackwardBreak(typesetter, { start, end - start }, extent, mode);
    }
};

void BreakResolverTests::testForwardLineBreaks() {
    Fixture f(u"ab c ab");

    assert(f.forward(0, 7, 100000.0f) == 7);
    assert(f.forward(0, 7, 7414.0f) == 7);

    /* The last word does not fit, so the line ends before it. */
    assert(f.forward(0, 7, 7000.0f) == 5);
    assert(f.forward(0, 7, 5151.0f) == 5);

    /* Only the first word fits. */
    assert(f.forward(0, 7, 3171.0f) == 3);
    assert(f.forward(0, 7, 3000.0f) == 3);
}

void BreakResolverTests::testForwardTrailingWhitespace() {
    Fixture f(u"ab c ab");

    /* The space at the end of a word is not measured if it is what makes the line too wide. */
    assert(f.forward(0, 7, 5000.0f) == 5);
    assert(f.forward(0, 7, 4243.0f) == 5);
    assert(f.forward(0, 7, 4242.0f) == 3);
    assert(f.forward(0, 7, 2263.0f) == 3);

    /* One unit less and the word does not fit, so a line break is not possible. */
    assert(f.forward(0, 7, 2262.0f) == 1);
}

void BreakResolverTests::testForwardCharacterBreaks() {
    Fixture f(u"ab c ab");

    assert(f.forward(0, 7, 100000.0f, TRBreakModeCharacter) == 7);

    /* "ab" fits, and the space after it is not counted if it is the one that overflows. */
    assert(f.forward(0, 7, 2300.0f, TRBreakModeCharacter) == 3);
    assert(f.forward(0, 7, 2262.0f, TRBreakModeCharacter) == 1);
    assert(f.forward(0, 7, 1200.0f, TRBreakModeCharacter) == 1);
    assert(f.forward(0, 7, 3500.0f, TRBreakModeCharacter) == 3);
    assert(f.forward(0, 7, 4500.0f, TRBreakModeCharacter) == 5);
}

void BreakResolverTests::testForwardAtLeastOneCharacter() {
    Fixture f(u"ab c ab");

    /* A line takes one character even if it is wider than the extent. */
    assert(f.forward(0, 7, 100.0f) == 1);
    assert(f.forward(0, 7, 0.0f) == 1);
    assert(f.forward(0, 7, -5.0f) == 1);
    assert(f.forward(0, 7, 100.0f, TRBreakModeCharacter) == 1);
    assert(f.forward(3, 7, 100.0f) == 4);
    assert(f.forward(6, 7, 100.0f) == 7);
}

void BreakResolverTests::testForwardSubranges() {
    Fixture f(u"ab c ab");

    assert(f.forward(3, 7, 3000.0f) == 5);
    assert(f.forward(3, 7, 100000.0f) == 7);
    assert(f.forward(5, 7, 100000.0f) == 7);
    assert(f.forward(1, 7, 2000.0f) == 3);

    /* The end of the range is respected. */
    assert(f.forward(0, 5, 100000.0f) == 5);
    assert(f.forward(0, 4, 100000.0f) == 4);
    assert(f.forward(0, 3, 100000.0f) == 3);
    assert(f.forward(0, 1, 100000.0f) == 1);
}

void BreakResolverTests::testForwardStopsAtParagraph() {
    Fixture f(u"ab c\nab");

    /* The paragraphs are "ab c\n" and "ab", and a line cannot go across them. */
    assert(f.forward(0, 7, 100000.0f) == 5);
    assert(f.forward(5, 7, 100000.0f) == 7);
    assert(f.forward(0, 7, 100000.0f, TRBreakModeCharacter) == 5);
}

void BreakResolverTests::testBackwardLineBreaks() {
    Fixture f(u"ab c ab");

    assert(f.backward(0, 7, 100000.0f) == 0);
    assert(f.backward(0, 7, 7414.0f) == 0);

    /* The line from the end takes as many words as fit. */
    assert(f.backward(0, 7, 5000.0f) == 3);
    assert(f.backward(0, 7, 4243.0f) == 3);
    assert(f.backward(0, 7, 2263.0f) == 5);
    assert(f.backward(0, 7, 6506.0f) == 0);
}

void BreakResolverTests::testBackwardCharacterBreaks() {
    Fixture f(u"ab c ab");

    assert(f.backward(0, 7, 100000.0f, TRBreakModeCharacter) == 0);
    assert(f.backward(0, 7, 3000.0f, TRBreakModeCharacter) == 4);
    assert(f.backward(0, 7, 2300.0f, TRBreakModeCharacter) == 4);
    assert(f.backward(0, 7, 1200.0f, TRBreakModeCharacter) == 6);
}

void BreakResolverTests::testBackwardAtLeastOneCharacter() {
    Fixture f(u"ab c ab");

    assert(f.backward(0, 7, 100.0f) == 6);
    assert(f.backward(0, 7, 2262.0f) == 6);
    assert(f.backward(0, 7, 0.0f) == 6);
    assert(f.backward(0, 7, 100.0f, TRBreakModeCharacter) == 6);
    assert(f.backward(0, 4, 100.0f) == 3);
    assert(f.backward(0, 1, 100.0f) == 0);
}

void BreakResolverTests::testBackwardStopsAtParagraph() {
    Fixture f(u"ab\ncd");

    /* The paragraphs are "ab\n" and "cd", and a line cannot go across them. */
    assert(f.backward(0, 5, 100000.0f) == 3);
    assert(f.backward(0, 3, 100000.0f) == 0);
    assert(f.backward(0, 5, 100000.0f, TRBreakModeCharacter) == 3);
}

void BreakResolverTests::testSurrogatePairsAreNotCut() {
    /* The emoji takes two code units, and is not in the font, so it is as wide as the notdef glyph. */
    Fixture f(u"a\U0001F600" "b");

    assert(f.forward(1, 4, 100.0f) == 3);
    assert(f.forward(1, 4, 100.0f, TRBreakModeCharacter) == 3);
    assert(f.backward(0, 3, 100.0f) == 1);
    assert(f.backward(0, 3, 100.0f, TRBreakModeCharacter) == 1);

    /* Only one of the characters fits next to the 'a'. */
    assert(f.forward(0, 4, 2100.0f, TRBreakModeCharacter) == 3);
    assert(f.forward(0, 4, 2000.0f, TRBreakModeCharacter) == 1);
}

void BreakResolverTests::testMixedSizes() {
    Fixture f(u"ab c ab");

    /* The second half of the text is half as big, so it takes half the extent. */
    setTestFloat(f.text, 4, 3, TRAttributePointSize, 1024.0f);
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);

    /* The segments are 3171, then 1072 + 454 for "c " and 557 + 574.5 for "ab". */
    assert(f.forward(0, 7, 100000.0f) == 7);
    assert(f.forward(0, 7, 5829.0f) == 7);
    assert(f.forward(0, 7, 5828.0f) == 5);
    assert(f.forward(0, 7, 5000.0f) == 5);
    assert(f.forward(0, 7, 4600.0f) == 5);
    assert(f.forward(0, 7, 4400.0f) == 5);
    assert(f.forward(0, 7, 4200.0f) == 3);
}

namespace {

void computeRoom(void *, TRFloat layoutWidth, TRReplacementRoom *room) {
    room->ascent = 20.0f;
    room->descent = 5.0f;
    room->extent = layoutWidth;
}

TRReplacementRef createBlock() {
    TRReplacementCallbacks callbacks = { computeRoom, nullptr };
    return TRReplacementCreate(&callbacks, nullptr, 0.0f, TRTrue);
}

void setReplacement(TRMutableTextRef text, size_t index, TRReplacementRef replacement) {
    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTextSetAttribute(text, index, 1, &attribute);
}

}

void BreakResolverTests::testBlocksEndLinesBeforeThem() {
    TRReplacementRef block = createBlock();
    Fixture f(u"ab\uFFFC cd");
    setReplacement(f.text, 2, block);
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);

    /* A line that has text before the block ends right before it, whatever the extent. */
    assert(f.forward(0, 6, 100000.0f) == 2);
    assert(f.forward(0, 6, 100.0f) == 1);
    assert(f.forward(1, 6, 100000.0f) == 2);
    assert(f.forward(0, 2, 100000.0f) == 2);

    TRTextRelease(f.text);
    f.text = makeTestText(u"x");
    TRReplacementRelease(block);
}

void BreakResolverTests::testBlockLines() {
    TRReplacementRef block = createBlock();
    Fixture f(u"ab\uFFFC  cd");
    setReplacement(f.text, 2, block);
    TRTypesetterRelease(f.typesetter);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);

    /* A block at the start of a line is the line, together with the whitespace that follows it. */
    assert(f.forward(2, 7, 100000.0f) == 5);
    assert(f.forward(2, 7, 0.0f) == 5);
    assert(f.forward(2, 4, 100000.0f) == 4);

    /* The line of a block also ends at the newline that ends its paragraph. */
    TRTextRelease(f.text);
    TRTypesetterRelease(f.typesetter);
    f.text = makeTestText(u"\uFFFC \n x", f.typeface, EmSize);
    setReplacement(f.text, 0, block);
    f.typesetter = TRTypesetterCreate(f.text, nullptr, 0);
    assert(f.forward(0, 5, 100000.0f) == 3);

    TRReplacementRelease(block);
}

void BreakResolverTests::testWalkingForwardCoversText() {
    Fixture f(u"ab c ab ca b ab\nc ab c");
    const TRUInteger length = 21;

    for (TRFloat extent : { 1000.0f, 3000.0f, 4500.0f, 8000.0f }) {
        for (TRBreakMode mode : { TRBreakModeLine, TRBreakModeCharacter }) {
            TRUInteger start = 0;
            size_t lines = 0;

            /* Every suggestion makes progress, and the lines add up to the whole text. */
            while (start < length) {
                TRUInteger end = f.forward(start, length, extent, mode);
                assert(end > start && end <= length);

                start = end;
                lines++;
                assert(lines <= length);
            }

            assert(start == length);
        }
    }
}

void BreakResolverTests::testWalkingBackwardCoversText() {
    Fixture f(u"ab c ab ca b ab\nc ab c");
    const TRUInteger length = 21;

    for (TRFloat extent : { 1000.0f, 3000.0f, 4500.0f, 8000.0f }) {
        for (TRBreakMode mode : { TRBreakModeLine, TRBreakModeCharacter }) {
            TRUInteger end = length;
            size_t lines = 0;

            while (end > 0) {
                TRUInteger start = f.backward(0, end, extent, mode);
                assert(start < end);

                end = start;
                lines++;
                assert(lines <= length);
            }
        }
    }
}

#ifdef STANDALONE_TESTING

int main() {
    BreakResolverTests tests;
    tests.run();

    return 0;
}

#endif
