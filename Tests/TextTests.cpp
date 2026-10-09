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
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRSheenBidi.h>
#include <Tehreer/TRText.h>

extern "C" {
#include <API/TRText.h>
#include <Core/AtomicUInt.h>
#include <Text/AttributeRegistry.h>
}

#include "TextTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 1000;

void TextTests::run() {
    testCreateUTF8();
    testCreateUTF16();
    testCreateUTF32();
    testCreateEmpty();
    testGetCodeUnitsRange();
    testRangesAreChecked();
    testSheenBidiText();
    testCreateCopy();
    testCreateMutable();
    testAppendInsertDelete();
    testSetAndReplace();
    testBatchEditing();
    testCreateMutableCopy();
    testRetainRelease();
    testConcurrentReads();
}

static string readUTF8(TRTextRef text) {
    string result(TRTextGetLength(text), '\0');
    TRTextGetCodeUnits(text, 0, result.size(), &result[0]);

    return result;
}

static TRMutableTextRef createMutableText(const char *string) {
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF8);
    assert(text != nullptr);

    TRTextAppendCodeUnits(text, string, strlen(string));

    return text;
}

void TextTests::testCreateUTF8() {
    const char string[] = "Hello \xD9\x85";
    TRTextRef text = TRTextCreate(string, strlen(string), TRStringEncodingUTF8);

    assert(text != nullptr);
    assert(TRTextGetEncoding(text) == TRStringEncodingUTF8);
    assert(TRTextGetLength(text) == 8);
    assert(readUTF8(text) == string);
    assert(text->isMutable == TRFalse);

    TRTextRelease(text);
}

void TextTests::testCreateUTF16() {
    const u16string string = u"H\u00E9llo \U0001F600";
    TRTextRef text = TRTextCreate(string.data(), string.size(), TRStringEncodingUTF16);

    assert(text != nullptr);
    assert(TRTextGetEncoding(text) == TRStringEncodingUTF16);
    assert(TRTextGetLength(text) == 8);

    u16string copy(string.size(), 0);
    TRTextGetCodeUnits(text, 0, copy.size(), &copy[0]);
    assert(copy == string);

    TRTextRelease(text);
}

void TextTests::testCreateUTF32() {
    const u32string string = U"abc\U0001F600";
    TRTextRef text = TRTextCreate(string.data(), string.size(), TRStringEncodingUTF32);

    assert(text != nullptr);
    assert(TRTextGetEncoding(text) == TRStringEncodingUTF32);
    assert(TRTextGetLength(text) == 4);

    u32string copy(string.size(), 0);
    TRTextGetCodeUnits(text, 0, copy.size(), &copy[0]);
    assert(copy == string);

    TRTextRelease(text);
}

void TextTests::testCreateEmpty() {
    TRTextRef text = TRTextCreate("", 0, TRStringEncodingUTF8);

    assert(text != nullptr);
    assert(TRTextGetLength(text) == 0);

    TRTextRelease(text);
}

void TextTests::testGetCodeUnitsRange() {
    const char string[] = "abcdefgh";
    TRTextRef text = TRTextCreate(string, 8, TRStringEncodingUTF8);
    char buffer[4] = { 0 };

    TRTextGetCodeUnits(text, 2, 3, buffer);
    assert(memcmp(buffer, "cde", 3) == 0);

    TRTextGetCodeUnits(text, 7, 1, buffer);
    assert(buffer[0] == 'h');

    TRTextRelease(text);
}

void TextTests::testRangesAreChecked() {
    const TRUInteger max = TRInvalidIndex;
    const char string[] = "abcdefgh";
    TRTextRef text = TRTextCreate(string, 8, TRStringEncodingUTF8);
    char buffer[16];

    /* Code units are only copied from a range that is within the text. */
    memset(buffer, '.', sizeof(buffer));
    assert(TRTextGetCodeUnits(text, 6, 2, buffer));
    assert(memcmp(buffer, "gh.", 3) == 0);
    assert(TRTextGetCodeUnits(text, 8, 0, buffer));
    assert(!TRTextGetCodeUnits(text, 6, 10, buffer));
    assert(!TRTextGetCodeUnits(text, 6, max, buffer));
    assert(!TRTextGetCodeUnits(text, 8, 3, buffer));
    assert(!TRTextGetCodeUnits(text, 100, 3, buffer));
    assert(!TRTextGetCodeUnits(text, max, max, buffer));
    assert(!TRTextGetCodeUnits(text, 0, 8, nullptr));
    assert(memcmp(buffer, "gh.", 3) == 0);

    /* The attributes of a position that is not in the text are not there. */
    TRUInteger length = 99;
    assert(TRTextCopyAttributes(text, 8, &length) == nullptr && length == 0);
    assert(TRTextCopyAttributes(text, max, nullptr) == nullptr);
    TRTextRelease(text);

    TRMutableTextRef mutableText = TRTextCreateMutable(TRStringEncodingUTF8);
    assert(TRTextAppendCodeUnits(mutableText, "abcd", 4));

    /* An index can be the end of the text to insert there, but not past it. */
    assert(TRTextInsertCodeUnits(mutableText, 4, "ef", 2));
    assert(TRTextGetLength(mutableText) == 6);
    assert(!TRTextInsertCodeUnits(mutableText, 7, "XX", 2));
    assert(!TRTextInsertCodeUnits(mutableText, max, "XX", 2));
    assert(TRTextGetLength(mutableText) == 6);
    TRTextGetCodeUnits(mutableText, 0, 6, buffer);
    assert(memcmp(buffer, "abcdef", 6) == 0);

    /* A range that is not in the text is not deleted, not even a part of it. */
    assert(!TRTextDeleteCodeUnits(mutableText, 4, max));
    assert(!TRTextDeleteCodeUnits(mutableText, 4, 3));
    assert(!TRTextDeleteCodeUnits(mutableText, 10, 2));
    assert(TRTextGetLength(mutableText) == 6);
    assert(TRTextDeleteCodeUnits(mutableText, 4, 2));
    assert(TRTextDeleteCodeUnits(mutableText, 2, 0));
    assert(TRTextGetLength(mutableText) == 4);

    assert(!TRTextReplaceCodeUnits(mutableText, 3, 50, "XY", 2));
    assert(!TRTextReplaceCodeUnits(mutableText, 99, 5, "Z", 1));
    assert(TRTextGetLength(mutableText) == 4);
    assert(TRTextReplaceCodeUnits(mutableText, 3, 1, "XY", 2));
    assert(TRTextGetLength(mutableText) == 5);
    TRTextGetCodeUnits(mutableText, 0, 5, buffer);
    assert(memcmp(buffer, "abcXY", 5) == 0);

    /* Replacing with nothing deletes the range. */
    assert(TRTextReplaceCodeUnits(mutableText, 0, 2, nullptr, 0));
    assert(TRTextGetLength(mutableText) == 3);

    /* Missing code units change nothing. */
    assert(!TRTextInsertCodeUnits(mutableText, 1, nullptr, 3));
    assert(!TRTextAppendCodeUnits(mutableText, nullptr, 3));
    assert(!TRTextReplaceCodeUnits(mutableText, 0, 1, nullptr, 3));
    assert(!TRTextSetCodeUnits(mutableText, nullptr, 3));
    assert(TRTextGetLength(mutableText) == 3);

    /* An attribute is only set on a range that is in the text, with a type that is known. */
    TRAttribute attribute = {};
    attribute.type = TRAttributeTypeSize;
    attribute.value.typeSize = 12.0f;
    assert(!TRTextSetAttribute(mutableText, 1, max, &attribute));
    assert(!TRTextSetAttribute(mutableText, 1, 3, &attribute));
    assert(!TRTextSetAttribute(mutableText, 99, 2, &attribute));
    assert(!TRTextSetAttribute(mutableText, 0, 3, nullptr));
    assert(TRTextSetAttribute(mutableText, 0, 0, &attribute));

    TRAttribute unknown = {};
    unknown.type = 1000;
    assert(!TRTextSetAttribute(mutableText, 0, 3, &unknown));

    assert(TRTextSetAttribute(mutableText, 1, 2, &attribute));
    TRAttributeListRef list = TRTextCopyAttributes(mutableText, 0, &length);
    assert(TRAttributeListGetCount(list) == 0 && length == 1);
    TRAttributeListRelease(list);

    list = TRTextCopyAttributes(mutableText, 1, &length);
    assert(TRAttributeListGetCount(list) == 1 && length == 2);

    /* Items that are not in a list are not given. */
    assert(TRAttributeListGetItem(list, 0) != nullptr);
    assert(TRAttributeListGetItem(list, 1) == nullptr);
    assert(TRAttributeListGetItem(list, max) == nullptr);
    TRAttributeListRelease(list);

    assert(!TRTextRemoveAttribute(mutableText, 2, max, TRAttributeTypeSize));
    assert(!TRTextRemoveAttribute(mutableText, 50, 5, TRAttributeTypeSize));
    assert(!TRTextRemoveAttribute(mutableText, 0, 3, 1000));
    assert(TRTextRemoveAttribute(mutableText, 2, 1, TRAttributeTypeSize));
    list = TRTextCopyAttributes(mutableText, 1, &length);
    assert(TRAttributeListGetCount(list) == 1 && length == 1);
    TRAttributeListRelease(list);

    /* Text that is not given cannot be made. */
    assert(TRTextCreate(nullptr, 3, TRStringEncodingUTF8) == nullptr);

    TRTextRelease(mutableText);
}

void TextTests::testSheenBidiText() {
    TRTextRef text = TRTextCreate("abc", 3, TRStringEncodingUTF8);
    SBTextRef sbText = TRTextGetSheenBidiText(text);

    assert(sbText != nullptr);
    assert(SBTextGetLength(sbText) == 3);
    assert(SBTextGetEncoding(sbText) == SBStringEncodingUTF8);
    SBAttributeRegistryRef registry = SBTextGetAttributeRegistry(sbText);
    assert(SBAttributeRegistryGetAttributeID(registry, "Typeface")
        == AttributeRegistryGetAttributeID(TRAttributeTypeface));

    TRTextRelease(text);
}

void TextTests::testCreateCopy() {
    TRTextRef text = TRTextCreate("copy me", 7, TRStringEncodingUTF8);
    TRTextRef copy = TRTextCreateCopy(text);

    assert(copy != nullptr);
    assert(copy != text);
    assert(copy->isMutable == TRFalse);
    assert(TRTextGetEncoding(copy) == TRStringEncodingUTF8);
    assert(readUTF8(copy) == "copy me");

    TRTextRelease(text);
    assert(readUTF8(copy) == "copy me");

    TRTextRelease(copy);
}

void TextTests::testCreateMutable() {
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF16);

    assert(text != nullptr);
    assert(text->isMutable == TRTrue);
    assert(text->isEditing == TRFalse);
    assert(TRTextGetEncoding(text) == TRStringEncodingUTF16);
    assert(TRTextGetLength(text) == 0);

    TRTextRelease(text);
}

void TextTests::testAppendInsertDelete() {
    TRMutableTextRef text = createMutableText("abc");
    assert(readUTF8(text) == "abc");

    TRTextAppendCodeUnits(text, "def", 3);
    assert(readUTF8(text) == "abcdef");

    TRTextInsertCodeUnits(text, 3, "XY", 2);
    assert(readUTF8(text) == "abcXYdef");

    TRTextDeleteCodeUnits(text, 3, 2);
    assert(readUTF8(text) == "abcdef");

    TRTextDeleteCodeUnits(text, 0, 6);
    assert(TRTextGetLength(text) == 0);

    TRTextRelease(text);
}

void TextTests::testSetAndReplace() {
    TRMutableTextRef text = createMutableText("hello");

    TRTextSetCodeUnits(text, "world!", 6);
    assert(readUTF8(text) == "world!");

    TRTextReplaceCodeUnits(text, 1, 4, "ar", 2);
    assert(readUTF8(text) == "war!");

    TRTextReplaceCodeUnits(text, 0, 0, ">> ", 3);
    assert(readUTF8(text) == ">> war!");

    TRTextRelease(text);
}

void TextTests::testBatchEditing() {
    TRMutableTextRef text = createMutableText("abc");

    TRTextBeginEditing(text);
    assert(text->isEditing == TRTrue);

    TRTextAppendCodeUnits(text, "def", 3);
    TRTextInsertCodeUnits(text, 0, ">", 1);
    TRTextEndEditing(text);

    assert(text->isEditing == TRFalse);
    assert(readUTF8(text) == ">abcdef");

    TRTextRelease(text);
}

void TextTests::testCreateMutableCopy() {
    TRTextRef original = TRTextCreate("origin", 6, TRStringEncodingUTF8);
    TRMutableTextRef copy = TRTextCreateMutableCopy(original);

    assert(copy != nullptr);
    assert(copy->isMutable == TRTrue);
    assert(readUTF8(copy) == "origin");

    TRTextAppendCodeUnits(copy, "!", 1);
    assert(readUTF8(copy) == "origin!");
    assert(readUTF8(original) == "origin");

    TRMutableTextRef nested = TRTextCreateMutableCopy(copy);
    TRTextDeleteCodeUnits(nested, 0, 3);
    assert(readUTF8(nested) == "gin!");
    assert(readUTF8(copy) == "origin!");

    TRTextRelease(nested);
    TRTextRelease(copy);
    TRTextRelease(original);
}

void TextTests::testRetainRelease() {
    TRTextRef text = TRTextCreate("abc", 3, TRStringEncodingUTF8);

    assert(TRTextRetain(text) == text);
    assert(AtomicUIntLoad(&text->_base.retainCount) == 2);

    TRTextRelease(text);
    assert(AtomicUIntLoad(&text->_base.retainCount) == 1);
    assert(readUTF8(text) == "abc");

    TRTextRelease(text);
}

void TextTests::testConcurrentReads() {
    const char string[] = "concurrent reads";
    TRTextRef text = TRTextCreate(string, strlen(string), TRStringEncodingUTF8);
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([text, &string]() {
            for (size_t j = 0; j < Iterations; j++) {
                TRTextRef retained = TRTextRetain(text);
                assert(TRTextGetLength(retained) == strlen(string));
                assert(readUTF8(retained) == string);

                TRTextRelease(retained);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(AtomicUIntLoad(&text->_base.retainCount) == 1);

    TRTextRelease(text);
}

#ifdef STANDALONE_TESTING

int main() {
    TextTests tests;
    tests.run();

    return 0;
}

#endif
