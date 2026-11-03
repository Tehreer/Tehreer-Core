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
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypesetter.h>

#include "TypesetterBoundaryTests.h"
#include "Utilities/TestText.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 200;

void TypesetterBoundaryTests::run() {
    testGraphemes();
    testWords();
    testParagraphs();
    testEdges();
    testRanges();
    testEditsAreSeen();
    testEditsAcrossParagraphs();
    testEncodingsAgree();
    testConcurrentSearches();
}

static TRUInteger next(TRTypesetterRef text, TRUInteger index, TRBoundaryType type) {
    return TRTypesetterFindBoundary(text, index, type, TRBoundaryDirectionForward);
}

static TRUInteger previous(TRTypesetterRef text, TRUInteger index, TRBoundaryType type) {
    return TRTypesetterFindBoundary(text, index, type, TRBoundaryDirectionBackward);
}

void TypesetterBoundaryTests::testGraphemes() {
    /* A letter with a combining mark, a surrogate pair, and a carriage return with a line feed. */
    TRMutableTypesetterRef text = makeTestText(u"a\u0301\U0001F600\r\nb");

    assert(next(text, 0, TRBoundaryTypeGrapheme) == 2);
    assert(next(text, 1, TRBoundaryTypeGrapheme) == 2);
    assert(next(text, 2, TRBoundaryTypeGrapheme) == 4);
    assert(next(text, 3, TRBoundaryTypeGrapheme) == 4);
    assert(next(text, 4, TRBoundaryTypeGrapheme) == 6);
    assert(next(text, 6, TRBoundaryTypeGrapheme) == 7);

    assert(previous(text, 7, TRBoundaryTypeGrapheme) == 6);
    assert(previous(text, 6, TRBoundaryTypeGrapheme) == 4);
    assert(previous(text, 5, TRBoundaryTypeGrapheme) == 4);
    assert(previous(text, 4, TRBoundaryTypeGrapheme) == 2);
    assert(previous(text, 2, TRBoundaryTypeGrapheme) == 0);

    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testWords() {
    TRMutableTypesetterRef text = makeTestText(u"Hello, world");

    assert(next(text, 0, TRBoundaryTypeWord) == 5);
    assert(next(text, 3, TRBoundaryTypeWord) == 5);
    assert(next(text, 5, TRBoundaryTypeWord) == 6);
    assert(next(text, 6, TRBoundaryTypeWord) == 7);
    assert(next(text, 7, TRBoundaryTypeWord) == 12);

    assert(previous(text, 12, TRBoundaryTypeWord) == 7);
    assert(previous(text, 9, TRBoundaryTypeWord) == 7);
    assert(previous(text, 7, TRBoundaryTypeWord) == 6);
    assert(previous(text, 5, TRBoundaryTypeWord) == 0);

    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testParagraphs() {
    /* A line feed, and a carriage return with a line feed, each end their paragraph. */
    TRMutableTypesetterRef text = makeTestText(u"ab\ncd\r\nef");

    assert(next(text, 0, TRBoundaryTypeParagraph) == 3);
    assert(next(text, 2, TRBoundaryTypeParagraph) == 3);
    assert(next(text, 3, TRBoundaryTypeParagraph) == 7);
    assert(next(text, 6, TRBoundaryTypeParagraph) == 7);
    assert(next(text, 7, TRBoundaryTypeParagraph) == 9);

    assert(previous(text, 9, TRBoundaryTypeParagraph) == 7);
    assert(previous(text, 8, TRBoundaryTypeParagraph) == 7);
    assert(previous(text, 7, TRBoundaryTypeParagraph) == 3);
    assert(previous(text, 4, TRBoundaryTypeParagraph) == 3);
    assert(previous(text, 3, TRBoundaryTypeParagraph) == 0);

    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testEdges() {
    TRMutableTypesetterRef text = makeTestText(u"abc");

    /* There is nothing after the end, nor before the start. */
    assert(next(text, 3, TRBoundaryTypeGrapheme) == TRInvalidIndex);
    assert(previous(text, 0, TRBoundaryTypeGrapheme) == TRInvalidIndex);
    assert(next(text, 3, TRBoundaryTypeParagraph) == TRInvalidIndex);
    assert(previous(text, 0, TRBoundaryTypeParagraph) == TRInvalidIndex);

    /* The index, the type and the direction are checked. */
    assert(next(text, 4, TRBoundaryTypeWord) == TRInvalidIndex);
    assert(previous(text, 4, TRBoundaryTypeWord) == TRInvalidIndex);
    assert(next(text, 0, 3) == TRInvalidIndex);
    assert(TRTypesetterFindBoundary(text, 0, TRBoundaryTypeWord, 2) == TRInvalidIndex);

    /* A text that is being edited has no valid analysis. */
    TRTypesetterBeginEditing(text);
    assert(next(text, 0, TRBoundaryTypeWord) == TRInvalidIndex);
    assert(next(text, 0, TRBoundaryTypeParagraph) == TRInvalidIndex);
    TRTypesetterEndEditing(text);
    assert(next(text, 0, TRBoundaryTypeWord) == 3);

    TRTypesetterRelease(text);

    TRMutableTypesetterRef empty = makeTestText(u"");
    assert(next(empty, 0, TRBoundaryTypeGrapheme) == TRInvalidIndex);
    assert(previous(empty, 0, TRBoundaryTypeGrapheme) == TRInvalidIndex);
    TRTypesetterRelease(empty);
}

void TypesetterBoundaryTests::testRanges() {
    TRMutableTypesetterRef text = makeTestText(u"Hello, world\nbye");

    TRRange range = TRTypesetterGetBoundaryRange(text, 1, TRBoundaryTypeWord);
    assert(range.index == 0 && range.length == 5);

    /* A boundary belongs to the segment that starts at it. */
    range = TRTypesetterGetBoundaryRange(text, 5, TRBoundaryTypeWord);
    assert(range.index == 5 && range.length == 1);

    range = TRTypesetterGetBoundaryRange(text, 0, TRBoundaryTypeGrapheme);
    assert(range.index == 0 && range.length == 1);

    /* A paragraph includes its separator. */
    range = TRTypesetterGetBoundaryRange(text, 12, TRBoundaryTypeParagraph);
    assert(range.index == 0 && range.length == 13);

    range = TRTypesetterGetBoundaryRange(text, 14, TRBoundaryTypeParagraph);
    assert(range.index == 13 && range.length == 3);

    /* The end of the text has an empty segment. */
    range = TRTypesetterGetBoundaryRange(text, 16, TRBoundaryTypeWord);
    assert(range.index == 16 && range.length == 0);

    range = TRTypesetterGetBoundaryRange(text, 17, TRBoundaryTypeWord);
    assert(range.index == TRInvalidIndex && range.length == 0);

    range = TRTypesetterGetBoundaryRange(text, 0, 3);
    assert(range.index == TRInvalidIndex && range.length == 0);

    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testEditsAreSeen() {
    TRMutableTypesetterRef text = makeTestText(u"ab cd");
    assert(next(text, 0, TRBoundaryTypeWord) == 2);

    /* The cached analysis is dropped by an insertion, a deletion, and a replacement. */
    TRTypesetterInsertCodeUnits(text, 2, u"xyz", 3);
    assert(next(text, 0, TRBoundaryTypeWord) == 5);

    TRTypesetterDeleteCodeUnits(text, 0, 3);
    assert(next(text, 0, TRBoundaryTypeWord) == 2);

    TRTypesetterReplaceCodeUnits(text, 0, 2, u"a b c", 5);
    assert(next(text, 0, TRBoundaryTypeWord) == 1);

    TRTypesetterSetCodeUnits(text, u"one two", 7);
    assert(next(text, 0, TRBoundaryTypeWord) == 3);

    TRTypesetterAppendCodeUnits(text, u"three", 5);
    assert(next(text, 8, TRBoundaryTypeWord) == 12);

    /* A copy has its own analysis. */
    TRTypesetterRef copy = TRTypesetterCreateCopy(text);
    TRTypesetterDeleteCodeUnits(text, 0, 4);
    assert(next(copy, 0, TRBoundaryTypeWord) == 3);
    assert(next(text, 0, TRBoundaryTypeWord) == 8);

    TRTypesetterRelease(copy);
    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testEditsAcrossParagraphs() {
    TRMutableTypesetterRef text = makeTestText(u"ab cd\nef gh");

    /* No word crosses a paragraph separator, and the breaks of both paragraphs are known. */
    assert(next(text, 3, TRBoundaryTypeWord) == 5);
    assert(next(text, 5, TRBoundaryTypeWord) == 6);
    assert(next(text, 6, TRBoundaryTypeWord) == 8);
    assert(previous(text, 8, TRBoundaryTypeWord) == 6);
    assert(previous(text, 6, TRBoundaryTypeWord) == 5);

    /* Deleting the separator merges the paragraphs, so the words next to it become one. */
    TRTypesetterDeleteCodeUnits(text, 5, 1);
    assert(next(text, 3, TRBoundaryTypeWord) == 7);
    assert(previous(text, 7, TRBoundaryTypeWord) == 3);

    /* Inserting a separator splits a paragraph, so the breaks are renewed for both parts. */
    TRTypesetterInsertCodeUnits(text, 5, u"\n", 1);
    assert(next(text, 3, TRBoundaryTypeWord) == 5);
    assert(next(text, 6, TRBoundaryTypeWord) == 8);

    /* An edit inside of one paragraph leaves the others as they are. */
    TRTypesetterInsertCodeUnits(text, 0, u"xy", 2);
    assert(next(text, 0, TRBoundaryTypeWord) == 4);
    assert(next(text, 8, TRBoundaryTypeWord) == 10);

    TRTypesetterRelease(text);
}

void TypesetterBoundaryTests::testEncodingsAgree() {
    const char *utf8 = "a\xCC\x81" "b c";
    TRTypesetterRef text8 = TRTypesetterCreate(utf8, 5, TRStringEncodingUTF8, nullptr, 0);
    assert(next(text8, 0, TRBoundaryTypeGrapheme) == 3);
    assert(next(text8, 0, TRBoundaryTypeWord) == 4);

    const char32_t utf32[] = { U'a', 0x0301, U'b', U' ', U'c' };
    TRTypesetterRef text32 = TRTypesetterCreate(utf32, 5, TRStringEncodingUTF32, nullptr, 0);
    assert(next(text32, 0, TRBoundaryTypeGrapheme) == 2);
    assert(next(text32, 0, TRBoundaryTypeWord) == 3);

    TRTypesetterRelease(text32);
    TRTypesetterRelease(text8);
}

void TypesetterBoundaryTests::testConcurrentSearches() {
    /* The first searches of an immutable text race to publish its analysis. */
    for (size_t round = 0; round < Iterations; round++) {
        TRTypesetterRef text = TRTypesetterCreate(u"Hello, world", 12, TRStringEncodingUTF16, nullptr, 0);
        vector<thread> threads;

        for (size_t i = 0; i < NumThreads; i++) {
            threads.emplace_back([text] {
                assert(next(text, 0, TRBoundaryTypeWord) == 5);
                assert(previous(text, 12, TRBoundaryTypeWord) == 7);
                assert(next(text, 0, TRBoundaryTypeGrapheme) == 1);
            });
        }
        for (thread &t : threads) {
            t.join();
        }

        TRTypesetterRelease(text);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    TypesetterBoundaryTests tests;
    tests.run();

    return 0;
}

#endif
