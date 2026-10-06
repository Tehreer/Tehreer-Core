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

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
}

#include "TestTypeface.h"

#include "AttributeListTests.h"

using namespace std;
using namespace Tehreer;

void AttributeListTests::run() {
    testNoAttributes();
    testPointSizeAttribute();
    testRunLengths();
    testRemoveAttribute();
    testOverwriteAttribute();
    testMultipleAttributes();
    testTypefaceAttribute();
    testTypefaceRetainBalance();
    testUnknownAttributeType();
    testListOutlivesText();
}

static TRAttribute makePointSize(TRFloat size) {
    TRAttribute attribute = {};
    attribute.type = TRAttributePointSize;
    attribute.value.pointSize = size;

    return attribute;
}

static TRAttribute makeTypeface(TRTypefaceRef typeface) {
    TRAttribute attribute = {};
    attribute.type = TRAttributeTypeface;
    attribute.value.typeface = typeface;

    return attribute;
}

static TRAttribute readItem(TRAttributeListRef list, TRUInteger index) {
    return *TRAttributeListGetItem(list, index);
}

static TRMutableTextRef createText() {
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF8);
    TRTextAppendCodeUnits(text, "abcdef", 6);

    return text;
}

static size_t retainCount(TRTypefaceRef typeface) {
    return AtomicUIntLoad(&typeface->_base.retainCount);
}

void AttributeListTests::testNoAttributes() {
    TRMutableTextRef text = createText();
    TRUInteger length = 0;

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);

    assert(list != nullptr);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 6);

    SBAttributeListRelease(list);
    TRTextRelease(text);
}

void AttributeListTests::testPointSizeAttribute() {
    TRMutableTextRef text = createText();
    TRAttribute attribute = makePointSize(12.5f);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 1, 3, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 2, &length);

    assert(TRAttributeListGetCount(list) == 1);
    assert(readItem(list, 0).type == TRAttributePointSize);
    assert(readItem(list, 0).value.pointSize == 12.5f);

    SBAttributeListRelease(list);
    TRTextRelease(text);
}

void AttributeListTests::testRunLengths() {
    TRMutableTextRef text = createText();
    TRAttribute attribute = makePointSize(10.0f);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 1, 3, &attribute);

    TRAttributeListRef before = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(before) == 0);
    assert(length == 1);
    SBAttributeListRelease(before);

    TRAttributeListRef inside = TRTextGetAttributes(text, 1, &length);
    assert(TRAttributeListGetCount(inside) == 1);
    assert(length == 3);
    SBAttributeListRelease(inside);

    TRAttributeListRef after = TRTextGetAttributes(text, 4, &length);
    assert(TRAttributeListGetCount(after) == 0);
    assert(length == 2);
    SBAttributeListRelease(after);

    TRTextRelease(text);
}

void AttributeListTests::testRemoveAttribute() {
    TRMutableTextRef text = createText();
    TRAttribute attribute = makePointSize(10.0f);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 6, &attribute);
    TRTextRemoveAttribute(text, 2, 2, TRAttributePointSize);

    TRAttributeListRef first = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(first) == 1);
    assert(length == 2);
    SBAttributeListRelease(first);

    TRAttributeListRef removed = TRTextGetAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(removed) == 0);
    assert(length == 2);
    SBAttributeListRelease(removed);

    TRTextRemoveAttribute(text, 0, 6, TRAttributePointSize);

    TRAttributeListRef cleared = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(cleared) == 0);
    assert(length == 6);
    SBAttributeListRelease(cleared);

    TRTextRelease(text);
}

void AttributeListTests::testOverwriteAttribute() {
    TRMutableTextRef text = createText();
    TRAttribute small = makePointSize(8.0f);
    TRAttribute large = makePointSize(20.0f);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 6, &small);
    TRTextSetAttribute(text, 2, 2, &large);

    TRAttributeListRef list = TRTextGetAttributes(text, 3, &length);
    assert(readItem(list, 0).value.pointSize == 20.0f);
    assert(length == 1);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 4, &length);
    assert(readItem(list, 0).value.pointSize == 8.0f);
    assert(length == 2);
    SBAttributeListRelease(list);

    TRTextRelease(text);
}

void AttributeListTests::testMultipleAttributes() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = createText();
    TRAttribute pointSize = makePointSize(16.0f);
    TRAttribute typefaceAttribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 4, &pointSize);
    TRTextSetAttribute(text, 2, 4, &typefaceAttribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(list) == 2);
    assert(length == 2);

    bool hasPointSize = false;
    bool hasTypeface = false;

    for (TRUInteger i = 0; i < 2; i++) {
        TRAttribute item = readItem(list, i);

        if (item.type == TRAttributePointSize) {
            hasPointSize = (item.value.pointSize == 16.0f);
        } else if (item.type == TRAttributeTypeface) {
            hasTypeface = (item.value.typeface == typeface);
        }
    }

    assert(hasPointSize);
    assert(hasTypeface);

    SBAttributeListRelease(list);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void AttributeListTests::testTypefaceAttribute() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 6, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 6);
    assert(readItem(list, 0).type == TRAttributeTypeface);
    assert(readItem(list, 0).value.typeface == typeface);

    SBAttributeListRelease(list);
    TRTextRelease(text);
    TRTypefaceRelease(typeface);
}

void AttributeListTests::testTypefaceRetainBalance() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    assert(retainCount(typeface) == 1);

    TRTextSetAttribute(text, 0, 6, &attribute);
    size_t storedCount = retainCount(typeface);
    assert(storedCount > 1);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(retainCount(typeface) == storedCount + 1);

    SBAttributeListRelease(list);
    assert(retainCount(typeface) == storedCount);

    TRTextRemoveAttribute(text, 0, 6, TRAttributeTypeface);
    assert(retainCount(typeface) <= storedCount);

    TRTextSetAttribute(text, 0, 3, &attribute);
    TRTextSetAttribute(text, 3, 3, &attribute);
    TRTextRelease(text);
    assert(retainCount(typeface) == 1);

    TRTypefaceRelease(typeface);
}

void AttributeListTests::testUnknownAttributeType() {
    TRMutableTextRef text = createText();
    TRAttribute attribute = makePointSize(10.0f);
    TRUInteger length = 0;

    attribute.type = 99;
    TRTextSetAttribute(text, 0, 6, &attribute);
    TRTextRemoveAttribute(text, 0, 6, 99);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 6);

    SBAttributeListRelease(list);
    TRTextRelease(text);
}

void AttributeListTests::testListOutlivesText() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTextRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 6, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    TRTextRelease(text);

    assert(TRAttributeListGetCount(list) == 1);
    assert(readItem(list, 0).value.typeface == typeface);
    assert(retainCount(typeface) > 1);

    SBAttributeListRelease(list);
    assert(retainCount(typeface) == 1);

    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    AttributeListTests tests;
    tests.run();

    return 0;
}

#endif
