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
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>

extern "C" {
#include <Layout/TextBuffer.h>
}

#include "TextBufferTests.h"

using namespace std;
using namespace Tehreer;

void TextBufferTests::run() {
    testIsWhitespace();
    testDecode();
    testCodeUnits();
    testLeadingWhitespaceEnd();
    testTrailingWhitespaceStart();
    testFindWhitespace();
    testNonBmpAndMultibyteWhitespace();
}

static TextBuffer make16(const u16string &s) {
    return { s.data(), s.size(), TRStringEncodingUTF16 };
}

void TextBufferTests::testIsWhitespace() {
    /* The White_Space property of Unicode. */
    const uint32_t spaces[] = {
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20, 0x85, 0xA0, 0x1680, 0x2000, 0x2005, 0x200A,
        0x2028, 0x2029, 0x202F, 0x205F, 0x3000
    };
    for (uint32_t codePoint : spaces) {
        assert(TextBufferIsWhitespace(codePoint));
    }

    /* Neighbors that are not, such as the zero width space and the controls around the tab. */
    const uint32_t others[] = {
        0x00, 0x08, 0x0E, 0x1F, 0x21, 0x41, 0x84, 0x86, 0xA1, 0x167F, 0x1681, 0x1FFF, 0x200B,
        0x200C, 0x2027, 0x202A, 0x202E, 0x2030, 0x205E, 0x2060, 0x2FFF, 0x3001, 0xFEFF, 0x1F600,
        0x10FFFF, 0xFFFFFFFF
    };
    for (uint32_t codePoint : others) {
        assert(!TextBufferIsWhitespace(codePoint));
    }
}

void TextBufferTests::testDecode() {
    const u16string text = u"a\U0001F600" "b\xD800" "c";
    TextBuffer buffer = make16(text);

    TRUInteger index = 0;
    assert(TextBufferDecodeNext(&buffer, &index) == 'a' && index == 1);
    assert(TextBufferDecodeNext(&buffer, &index) == 0x1F600 && index == 3);
    assert(TextBufferDecodeNext(&buffer, &index) == 'b' && index == 4);
    /* A surrogate without its pair is a faulty code point of one code unit. */
    assert(TextBufferDecodeNext(&buffer, &index) == 0xFFFD && index == 5);
    assert(TextBufferDecodeNext(&buffer, &index) == 'c' && index == 6);

    assert(TextBufferGetCodePoint(&buffer, 1) == 0x1F600);
    assert(TextBufferGetCodePoint(&buffer, 0) == 'a');

    const string utf8 = "a\xC3\xA9\xF0\x9F\x98\x80" "b";
    TextBuffer bytes = { utf8.data(), utf8.size(), TRStringEncodingUTF8 };
    index = 0;
    assert(TextBufferDecodeNext(&bytes, &index) == 'a' && index == 1);
    assert(TextBufferDecodeNext(&bytes, &index) == 0xE9 && index == 3);
    assert(TextBufferDecodeNext(&bytes, &index) == 0x1F600 && index == 7);
    assert(TextBufferDecodeNext(&bytes, &index) == 'b' && index == 8);

    const u32string utf32 = U"a\U0001F600" "b";
    TextBuffer words = { utf32.data(), utf32.size(), TRStringEncodingUTF32 };
    index = 0;
    assert(TextBufferDecodeNext(&words, &index) == 'a' && index == 1);
    assert(TextBufferDecodeNext(&words, &index) == 0x1F600 && index == 2);
    assert(TextBufferGetCodePoint(&words, 2) == 'b');
}

void TextBufferTests::testCodeUnits() {
    const u16string text = u"a\U0001F600";
    TextBuffer buffer = make16(text);
    assert(TextBufferGetCodeUnit(&buffer, 0) == 'a');
    assert(TextBufferGetCodeUnit(&buffer, 1) == 0xD83D);
    assert(TextBufferGetCodeUnit(&buffer, 2) == 0xDE00);

    const string utf8 = "a\xC3\xA9";
    TextBuffer bytes = { utf8.data(), utf8.size(), TRStringEncodingUTF8 };
    assert(TextBufferGetCodeUnit(&bytes, 1) == 0xC3);
    assert(TextBufferGetCodeUnit(&bytes, 2) == 0xA9);

    const u32string utf32 = U"\U0001F600";
    TextBuffer words = { utf32.data(), utf32.size(), TRStringEncodingUTF32 };
    assert(TextBufferGetCodeUnit(&words, 0) == 0x1F600);
}

void TextBufferTests::testLeadingWhitespaceEnd() {
    const u16string text = u"  ab  c ";
    TextBuffer buffer = make16(text);

    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 0, 8) == 2);
    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 1, 8) == 2);
    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 2, 8) == 2);
    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 4, 8) == 6);
    /* The range limits the search, and an empty range has nothing in it. */
    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 0, 1) == 1);
    assert(TextBufferGetLeadingWhitespaceEnd(&buffer, 3, 3) == 3);

    const u16string blanks = u" \t\n ";
    TextBuffer all = make16(blanks);
    assert(TextBufferGetLeadingWhitespaceEnd(&all, 0, 4) == 4);
}

void TextBufferTests::testTrailingWhitespaceStart() {
    const u16string text = u"  ab  c ";
    TextBuffer buffer = make16(text);

    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 0, 8) == 7);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 0, 7) == 7);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 0, 6) == 4);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 4, 6) == 4);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 0, 2) == 0);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 1, 2) == 1);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 3, 3) == 3);

    const u16string blanks = u" \t\n ";
    TextBuffer all = make16(blanks);
    assert(TextBufferGetTrailingWhitespaceStart(&all, 0, 4) == 0);
    assert(TextBufferGetTrailingWhitespaceStart(&all, 2, 4) == 2);
}

void TextBufferTests::testFindWhitespace() {
    const u16string text = u"ab cd  e";
    TextBuffer buffer = make16(text);

    assert(TextBufferFindWhitespace(&buffer, 0, 8) == 2);
    assert(TextBufferFindWhitespace(&buffer, 3, 8) == 5);
    assert(TextBufferFindWhitespace(&buffer, 5, 8) == 5);
    assert(TextBufferFindWhitespace(&buffer, 7, 8) == 8);
    assert(TextBufferFindWhitespace(&buffer, 0, 2) == 2);
    assert(TextBufferFindWhitespace(&buffer, 0, 0) == 0);
}

void TextBufferTests::testNonBmpAndMultibyteWhitespace() {
    /* A no-break space is two bytes in UTF-8, and an emoji is a surrogate pair in UTF-16. */
    const string utf8 = "a\xC2\xA0\xC2\xA0" "b\xF0\x9F\x98\x80\xE3\x80\x80";
    TextBuffer bytes = { utf8.data(), utf8.size(), TRStringEncodingUTF8 };

    assert(TextBufferFindWhitespace(&bytes, 0, bytes.length) == 1);
    assert(TextBufferGetLeadingWhitespaceEnd(&bytes, 1, bytes.length) == 5);
    /* The ideographic space at the end is three bytes. */
    assert(TextBufferGetTrailingWhitespaceStart(&bytes, 0, bytes.length) == bytes.length - 3);
    assert(TextBufferGetTrailingWhitespaceStart(&bytes, 0, bytes.length - 3) == bytes.length - 3);

    const u16string text = u"a\U0001F600\u3000\u3000";
    TextBuffer buffer = make16(text);
    assert(TextBufferGetTrailingWhitespaceStart(&buffer, 0, buffer.length) == 3);

    /* A text that ends with a pair is not cut in the middle of it. */
    const u16string emoji = u"a\U0001F600";
    TextBuffer tail = make16(emoji);
    assert(TextBufferGetTrailingWhitespaceStart(&tail, 0, tail.length) == 3);

    const u32string utf32 = U"ab\u00A0\u2003";
    TextBuffer words = { utf32.data(), utf32.size(), TRStringEncodingUTF32 };
    assert(TextBufferGetTrailingWhitespaceStart(&words, 0, 4) == 2);
    assert(TextBufferFindWhitespace(&words, 0, 4) == 2);
}

#ifdef STANDALONE_TESTING

int main() {
    TextBufferTests tests;
    tests.run();

    return 0;
}

#endif
