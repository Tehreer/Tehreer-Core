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
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>

extern "C" {
#include <Core/AtomicUInt.h>
#include <Text/BreakClassifier.h>
}

#include "BreakClassifierTests.h"

using namespace std;
using namespace Tehreer;

void BreakClassifierTests::run() {
    testCreateInvalid();
    testEmptyText();
    testLineBreaks();
    testMandatoryBreaks();
    testGraphemeBreaks();
    testSurrogatePairs();
    testEncodingsAgree();
    testForwardSearch();
    testBackwardSearch();
    testSearchesVisitEveryBreak();
    testRetainRelease();
    testConcurrentQueries();
}

static BreakClassifierRef create16(const char16_t *text, size_t length) {
    return BreakClassifierCreate(text, length, TRStringEncodingUTF16);
}

/* Lists the indexes that have a break, from the first code unit to the end of the text. */
static vector<size_t> breaksOf(BreakClassifierRef classifier, BreakType type) {
    vector<size_t> indexes;

    for (size_t i = 0; i <= classifier->length; i++) {
        if (BreakClassifierHasBreak(classifier, type, i)) {
            indexes.push_back(i);
        }
    }

    return indexes;
}

void BreakClassifierTests::testCreateInvalid() {
    assert(BreakClassifierCreate(u"abc", 3, 3) == nullptr);
    assert(BreakClassifierCreate(u"abc", 3, 0xFFFF) == nullptr);
    assert(BreakClassifierCreate(nullptr, 3, TRStringEncodingUTF16) == nullptr);
}

void BreakClassifierTests::testEmptyText() {
    BreakClassifierRef classifier = BreakClassifierCreate(nullptr, 0, TRStringEncodingUTF16);

    assert(classifier != nullptr);
    assert(classifier->length == 0);
    assert(!BreakClassifierHasBreak(classifier, BreakTypeLine, 0));
    assert(!BreakClassifierHasBreak(classifier, BreakTypeGrapheme, 0));

    BreakClassifierRelease(classifier);
}

/*
 * The expected breaks of these tests follow the Unicode line and grapheme cluster rules, with the
 * values coming from libunibreak.
 */
void BreakClassifierTests::testLineBreaks() {
    BreakClassifierRef classifier = create16(u"Hello world", 11);

    /* The only opportunity is after the space; the end of the text is not reported. */
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 6 }));
    assert(!BreakClassifierHasBreak(classifier, BreakTypeLine, 0));
    assert(!BreakClassifierHasBreak(classifier, BreakTypeLine, 5));
    assert(BreakClassifierHasBreak(classifier, BreakTypeLine, 6));
    BreakClassifierRelease(classifier);

    /* A hyphen allows a break after it. */
    classifier = create16(u"well-known text", 15);
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 5, 11 }));
    BreakClassifierRelease(classifier);

    /* Arabic words are separated by spaces too. */
    classifier = create16(u"\u0627\u0631\u062F\u0648 \u0632\u0628\u0627\u0646", 9);
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 5 }));
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testMandatoryBreaks() {
    BreakClassifierRef classifier = create16(u"ab\ncd", 5);
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 3 }));
    BreakClassifierRelease(classifier);

    /* A carriage return and a line feed are one separator. */
    classifier = create16(u"a\r\nb", 4);
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 3 }));
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 1, 3, 4 }));
    BreakClassifierRelease(classifier);

    /* A text that ends with a separator has a break at its end. */
    classifier = create16(u"ab\n", 3);
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 3 }));
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testGraphemeBreaks() {
    BreakClassifierRef classifier = create16(u"abc", 3);
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 1, 2, 3 }));
    BreakClassifierRelease(classifier);

    /* A combining mark stays with its base. */
    classifier = create16(u"e\u0301x", 3);
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 2, 3 }));
    BreakClassifierRelease(classifier);

    /* A Hangul syllable written with conjoining jamo is a single cluster. */
    classifier = create16(u"\u1100\u1161\u11A8x", 4);
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 3, 4 }));
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testSurrogatePairs() {
    /* Neither kind of break falls between the two surrogates, at index 2. */
    BreakClassifierRef classifier = create16(u"a\U0001F600" "b", 4);
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 1, 3, 4 }));
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 1, 3 }));
    BreakClassifierRelease(classifier);

    /* An emoji sequence joined by zero width joiners is one cluster of eight code units. */
    classifier = create16(u"\U0001F468\u200D\U0001F469\u200D\U0001F467 x", 10);
    assert((breaksOf(classifier, BreakTypeGrapheme) == vector<size_t>{ 8, 9, 10 }));
    assert((breaksOf(classifier, BreakTypeLine) == vector<size_t>{ 9 }));
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testEncodingsAgree() {
    /* The same text has the same breaks in each encoding, at the indexes of its code units. */
    BreakClassifierRef utf8 = BreakClassifierCreate("a\xF0\x9F\x98\x80" "b", 6, TRStringEncodingUTF8);
    BreakClassifierRef utf16 = create16(u"a\U0001F600" "b", 4);
    BreakClassifierRef utf32 = BreakClassifierCreate(U"a\U0001F600" "b", 3, TRStringEncodingUTF32);

    assert((breaksOf(utf8, BreakTypeGrapheme) == vector<size_t>{ 1, 5, 6 }));
    assert((breaksOf(utf16, BreakTypeGrapheme) == vector<size_t>{ 1, 3, 4 }));
    assert((breaksOf(utf32, BreakTypeGrapheme) == vector<size_t>{ 1, 2, 3 }));

    assert((breaksOf(utf8, BreakTypeLine) == vector<size_t>{ 1, 5 }));
    assert((breaksOf(utf16, BreakTypeLine) == vector<size_t>{ 1, 3 }));
    assert((breaksOf(utf32, BreakTypeLine) == vector<size_t>{ 1, 2 }));

    /* The bytes inside of a UTF-8 sequence have no breaks. */
    for (size_t i = 2; i <= 4; i++) {
        assert(!BreakClassifierHasBreak(utf8, BreakTypeGrapheme, i));
        assert(!BreakClassifierHasBreak(utf8, BreakTypeLine, i));
    }

    BreakClassifierRelease(utf32);
    BreakClassifierRelease(utf16);
    BreakClassifierRelease(utf8);
}

void BreakClassifierTests::testForwardSearch() {
    BreakClassifierRef classifier = create16(u"well-known text", 15);

    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 0, 15) == 5);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 5, 15) == 11);
    /* Nothing is left after the last break, so the search ends where its range does. */
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 11, 15) == 15);

    /* A break at the end of the range counts, and one past it does not. */
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 0, 5) == 5);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 0, 4) == 4);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 2, 3) == 3);

    /* Graphemes break everywhere here, so the search moves one code unit at a time. */
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeGrapheme, 0, 15) == 1);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeGrapheme, 7, 15) == 8);

    BreakClassifierRelease(classifier);

    /* A search starting inside a cluster finds the end of it. */
    classifier = create16(u"e\u0301x", 3);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeGrapheme, 0, 3) == 2);
    assert(BreakClassifierGetForwardBreak(classifier, BreakTypeGrapheme, 1, 3) == 2);
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testBackwardSearch() {
    BreakClassifierRef classifier = create16(u"well-known text", 15);

    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 15, 0) == 11);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 11, 0) == 5);
    /* Nothing is left before the first break, so the search ends where its range does. */
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 5, 0) == 0);

    /* A break at the start of the range counts, and one before it does not. */
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 15, 11) == 11);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 15, 12) == 12);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 12, 11) == 11);

    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeGrapheme, 15, 0) == 14);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeGrapheme, 8, 0) == 7);

    BreakClassifierRelease(classifier);

    classifier = create16(u"e\u0301x", 3);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeGrapheme, 3, 0) == 2);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeGrapheme, 2, 0) == 0);
    assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeGrapheme, 1, 0) == 0);
    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testSearchesVisitEveryBreak() {
    const char16_t text[] = u"Hello wor\u0301ld, a\U0001F600 b-c d\nend";
    const size_t length = sizeof(text) / sizeof(text[0]) - 1;
    BreakClassifierRef classifier = create16(text, length);

    for (BreakType type : { BreakTypeLine, BreakTypeGrapheme }) {
        vector<size_t> expected;
        for (size_t i = 1; i < length; i++) {
            if (BreakClassifierHasBreak(classifier, type, i)) {
                expected.push_back(i);
            }
        }
        expected.push_back(length);

        /* Walking forward collects every break, then the end of the range. */
        vector<size_t> forward;
        size_t index = 0;
        while (index < length) {
            index = BreakClassifierGetForwardBreak(classifier, type, index, length);
            forward.push_back(index);
        }
        assert(forward == expected);

        /* Walking backward collects the same ones, and ends at the start of the range. */
        vector<size_t> backward;
        index = length;
        while (index > 0) {
            index = BreakClassifierGetBackwardBreak(classifier, type, index, 0);
            backward.push_back(index);
        }
        assert(backward.back() == 0);
        backward.pop_back();

        vector<size_t> inner(expected.begin(), expected.end() - 1);
        vector<size_t> reversed(inner.rbegin(), inner.rend());
        assert(backward == reversed);
    }

    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testRetainRelease() {
    BreakClassifierRef classifier = create16(u"a b", 3);

    assert(BreakClassifierRetain(classifier) == classifier);
    assert(AtomicUIntLoad(&classifier->_base.retainCount) == 2);

    BreakClassifierRelease(classifier);
    assert(AtomicUIntLoad(&classifier->_base.retainCount) == 1);
    assert(BreakClassifierHasBreak(classifier, BreakTypeLine, 2));

    BreakClassifierRelease(classifier);
}

void BreakClassifierTests::testConcurrentQueries() {
    BreakClassifierRef classifier = create16(u"The quick brown fox jumps over the lazy dog", 43);
    vector<size_t> expected = breaksOf(classifier, BreakTypeLine);

    vector<thread> threads;
    for (size_t i = 0; i < 8; i++) {
        threads.emplace_back([classifier, &expected]() {
            for (size_t j = 0; j < 500; j++) {
                assert(breaksOf(classifier, BreakTypeLine) == expected);
                assert(BreakClassifierGetForwardBreak(classifier, BreakTypeLine, 0, 43) == expected[0]);
                assert(BreakClassifierGetBackwardBreak(classifier, BreakTypeLine, 43, 0) == expected.back());
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    BreakClassifierRelease(classifier);
}

#ifdef STANDALONE_TESTING

int main() {
    BreakClassifierTests tests;
    tests.run();

    return 0;
}

#endif
