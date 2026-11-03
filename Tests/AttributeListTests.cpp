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
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRTypesetter.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRReplacement.h>
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
}

#include "TestTypeface.h"

#include "AttributeListTests.h"

using namespace std;
using namespace Tehreer;

void AttributeListTests::run() {
    testNoAttributes();
    testTypeSizeAttribute();
    testRunLengths();
    testRemoveAttribute();
    testOverwriteAttribute();
    testMultipleAttributes();
    testTypefaceAttribute();
    testTypefaceRetainBalance();
    testUnknownAttributeType();
    testListOutlivesText();
    testRunAttributeValues();
    testParagraphAttributeValues();
    testParagraphAttributesCoverParagraphs();
    testEqualParagraphAttributesMerge();
    testReplacementAttribute();
    testReplacementRetainBalance();
    testForegroundColorAttribute();
    testDecorationAttributes();
    testUserDataAttribute();
}

static TRAttribute makeTypeSize(TRFloat size) {
    TRAttribute attribute = {};
    attribute.type = TRAttributeTypeSize;
    attribute.value.typeSize = size;

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

static TRMutableTypesetterRef createText() {
    TRMutableTypesetterRef text = TRTypesetterCreateMutable(TRStringEncodingUTF8, nullptr, 0);
    TRTypesetterAppendCodeUnits(text, "abcdef", 6);

    return text;
}

static size_t retainCount(TRTypefaceRef typeface) {
    return AtomicUIntLoad(&typeface->_base.retainCount);
}

void AttributeListTests::testNoAttributes() {
    TRMutableTypesetterRef text = createText();
    TRUInteger length = 0;

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);

    assert(list != nullptr);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 6);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
}

void AttributeListTests::testTypeSizeAttribute() {
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeSize(12.5f);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 1, 3, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 2, &length);

    assert(TRAttributeListGetCount(list) == 1);
    assert(readItem(list, 0).type == TRAttributeTypeSize);
    assert(readItem(list, 0).value.typeSize == 12.5f);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
}

void AttributeListTests::testRunLengths() {
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeSize(10.0f);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 1, 3, &attribute);

    TRAttributeListRef before = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(before) == 0);
    assert(length == 1);
    TRAttributeListRelease(before);

    TRAttributeListRef inside = TRTypesetterCopyAttributes(text, 1, &length);
    assert(TRAttributeListGetCount(inside) == 1);
    assert(length == 3);
    TRAttributeListRelease(inside);

    TRAttributeListRef after = TRTypesetterCopyAttributes(text, 4, &length);
    assert(TRAttributeListGetCount(after) == 0);
    assert(length == 2);
    TRAttributeListRelease(after);

    TRTypesetterRelease(text);
}

void AttributeListTests::testRemoveAttribute() {
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeSize(10.0f);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 6, &attribute);
    TRTypesetterRemoveAttribute(text, 2, 2, TRAttributeTypeSize);

    TRAttributeListRef first = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(first) == 1);
    assert(length == 2);
    TRAttributeListRelease(first);

    TRAttributeListRef removed = TRTypesetterCopyAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(removed) == 0);
    assert(length == 2);
    TRAttributeListRelease(removed);

    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeTypeSize);

    TRAttributeListRef cleared = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(cleared) == 0);
    assert(length == 6);
    TRAttributeListRelease(cleared);

    TRTypesetterRelease(text);
}

void AttributeListTests::testOverwriteAttribute() {
    TRMutableTypesetterRef text = createText();
    TRAttribute smallSize = makeTypeSize(8.0f);
    TRAttribute large = makeTypeSize(20.0f);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 6, &smallSize);
    TRTypesetterSetAttribute(text, 2, 2, &large);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 3, &length);
    assert(readItem(list, 0).value.typeSize == 20.0f);
    assert(length == 1);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 4, &length);
    assert(readItem(list, 0).value.typeSize == 8.0f);
    assert(length == 2);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

void AttributeListTests::testMultipleAttributes() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTypesetterRef text = createText();
    TRAttribute typeSize = makeTypeSize(16.0f);
    TRAttribute typefaceAttribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 4, &typeSize);
    TRTypesetterSetAttribute(text, 2, 4, &typefaceAttribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(list) == 2);
    assert(length == 2);

    bool hasTypeSize = false;
    bool hasTypeface = false;

    for (TRUInteger i = 0; i < 2; i++) {
        TRAttribute item = readItem(list, i);

        if (item.type == TRAttributeTypeSize) {
            hasTypeSize = (item.value.typeSize == 16.0f);
        } else if (item.type == TRAttributeTypeface) {
            hasTypeface = (item.value.typeface == typeface);
        }
    }

    assert(hasTypeSize);
    assert(hasTypeface);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
    TRTypefaceRelease(typeface);
}

void AttributeListTests::testTypefaceAttribute() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 6, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 6);
    assert(readItem(list, 0).type == TRAttributeTypeface);
    assert(readItem(list, 0).value.typeface == typeface);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
    TRTypefaceRelease(typeface);
}

void AttributeListTests::testTypefaceRetainBalance() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    assert(retainCount(typeface) == 1);

    TRTypesetterSetAttribute(text, 0, 6, &attribute);
    size_t storedCount = retainCount(typeface);
    assert(storedCount > 1);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(retainCount(typeface) == storedCount + 1);

    TRAttributeListRelease(list);
    assert(retainCount(typeface) == storedCount);

    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeTypeface);
    assert(retainCount(typeface) <= storedCount);

    TRTypesetterSetAttribute(text, 0, 3, &attribute);
    TRTypesetterSetAttribute(text, 3, 3, &attribute);
    TRTypesetterRelease(text);
    assert(retainCount(typeface) == 1);

    TRTypefaceRelease(typeface);
}

void AttributeListTests::testUnknownAttributeType() {
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeSize(10.0f);
    TRUInteger length = 0;

    attribute.type = 99;
    TRTypesetterSetAttribute(text, 0, 6, &attribute);
    TRTypesetterRemoveAttribute(text, 0, 6, 99);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 6);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
}

void AttributeListTests::testListOutlivesText() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = makeTypeface(typeface);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 6, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    TRTypesetterRelease(text);

    assert(TRAttributeListGetCount(list) == 1);
    assert(readItem(list, 0).value.typeface == typeface);
    assert(retainCount(typeface) > 1);

    TRAttributeListRelease(list);
    assert(retainCount(typeface) == 1);

    TRTypefaceRelease(typeface);
}

static TRMutableTypesetterRef createParagraphText() {
    TRMutableTypesetterRef text = TRTypesetterCreateMutable(TRStringEncodingUTF8, nullptr, 0);
    TRTypesetterAppendCodeUnits(text, "ab\ncd\nef", 8);

    return text;
}

static TRAttribute makeFloat(TRAttributeType type, TRFloat value) {
    TRAttribute attribute = {};
    attribute.type = type;

    switch (type) {
    case TRAttributeScaleX: attribute.value.scaleX = value; break;
    case TRAttributeScaleY: attribute.value.scaleY = value; break;
    case TRAttributeBaselineOffset: attribute.value.baselineOffset = value; break;
    case TRAttributeObliqueness: attribute.value.obliqueness = value; break;
    case TRAttributeFirstLineHeadIndent: attribute.value.firstLineHeadIndent = value; break;
    case TRAttributeHeadIndent: attribute.value.headIndent = value; break;
    case TRAttributeTailIndent: attribute.value.tailIndent = value; break;
    case TRAttributeParagraphSpacingBefore: attribute.value.paragraphSpacingBefore = value; break;
    case TRAttributeParagraphSpacing: attribute.value.paragraphSpacing = value; break;
    case TRAttributeLineHeightMultiple: attribute.value.lineHeightMultiple = value; break;
    case TRAttributeMinimumLineHeight: attribute.value.minimumLineHeight = value; break;
    case TRAttributeMaximumLineHeight: attribute.value.maximumLineHeight = value; break;
    case TRAttributeLineSpacing: attribute.value.lineSpacing = value; break;
    default: assert(false);
    }

    return attribute;
}

static TRFloat readFloat(const TRAttribute &attribute) {
    switch (attribute.type) {
    case TRAttributeScaleX: return attribute.value.scaleX;
    case TRAttributeScaleY: return attribute.value.scaleY;
    case TRAttributeBaselineOffset: return attribute.value.baselineOffset;
    case TRAttributeObliqueness: return attribute.value.obliqueness;
    case TRAttributeFirstLineHeadIndent: return attribute.value.firstLineHeadIndent;
    case TRAttributeHeadIndent: return attribute.value.headIndent;
    case TRAttributeTailIndent: return attribute.value.tailIndent;
    case TRAttributeParagraphSpacingBefore: return attribute.value.paragraphSpacingBefore;
    case TRAttributeParagraphSpacing: return attribute.value.paragraphSpacing;
    case TRAttributeLineHeightMultiple: return attribute.value.lineHeightMultiple;
    case TRAttributeMinimumLineHeight: return attribute.value.minimumLineHeight;
    case TRAttributeMaximumLineHeight: return attribute.value.maximumLineHeight;
    case TRAttributeLineSpacing: return attribute.value.lineSpacing;
    default: assert(false);
    }

    return 0.0f;
}

void AttributeListTests::testRunAttributeValues() {
    const TRAttributeType types[] = {
        TRAttributeScaleX, TRAttributeScaleY, TRAttributeBaselineOffset, TRAttributeObliqueness
    };

    for (TRAttributeType type : types) {
        TRMutableTypesetterRef text = createText();
        TRAttribute attribute = makeFloat(type, 1.5f);
        TRUInteger length = 0;

        TRTypesetterSetAttribute(text, 1, 3, &attribute);

        TRAttributeListRef list = TRTypesetterCopyAttributes(text, 1, &length);
        assert(TRAttributeListGetCount(list) == 1);
        assert(length == 3);
        assert(readItem(list, 0).type == type);
        assert(readFloat(readItem(list, 0)) == 1.5f);
        TRAttributeListRelease(list);

        /* Outside of the range there is nothing, as these attributes apply to the exact range. */
        list = TRTypesetterCopyAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 0);
        assert(length == 1);
        TRAttributeListRelease(list);

        TRTypesetterRemoveAttribute(text, 0, 6, type);
        list = TRTypesetterCopyAttributes(text, 2, &length);
        assert(TRAttributeListGetCount(list) == 0);
        TRAttributeListRelease(list);

        TRTypesetterRelease(text);
    }

    /* Each of them has an identity of its own, so they do not replace each other. */
    TRMutableTypesetterRef text = createText();
    for (TRAttributeType type : types) {
        TRAttribute attribute = makeFloat(type, 2.0f);
        TRTypesetterSetAttribute(text, 0, 6, &attribute);
    }

    TRUInteger length = 0;
    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 4);
    assert(length == 6);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

void AttributeListTests::testParagraphAttributeValues() {
    const TRAttributeType types[] = {
        TRAttributeFirstLineHeadIndent, TRAttributeHeadIndent, TRAttributeTailIndent,
        TRAttributeParagraphSpacingBefore, TRAttributeParagraphSpacing,
        TRAttributeLineHeightMultiple, TRAttributeMinimumLineHeight,
        TRAttributeMaximumLineHeight, TRAttributeLineSpacing
    };

    for (TRAttributeType type : types) {
        TRMutableTypesetterRef text = createParagraphText();
        TRAttribute attribute = makeFloat(type, -4.25f);
        TRUInteger length = 0;

        TRTypesetterSetAttribute(text, 0, 2, &attribute);

        TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 1);
        assert(readItem(list, 0).type == type);
        assert(readFloat(readItem(list, 0)) == -4.25f);
        TRAttributeListRelease(list);

        TRTypesetterRemoveAttribute(text, 0, 8, type);
        list = TRTypesetterCopyAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 0);
        TRAttributeListRelease(list);

        TRTypesetterRelease(text);
    }

    /* The alignment and the line count are not floating-point values. */
    TRMutableTypesetterRef text = createParagraphText();
    TRAttribute alignment = {};
    alignment.type = TRAttributeTextAlignment;
    alignment.value.textAlignment = TRTextAlignmentTrailing;

    TRAttribute lineCount = {};
    lineCount.type = TRAttributeFirstIndentLineCount;
    lineCount.value.firstIndentLineCount = 3;

    TRTypesetterSetAttribute(text, 0, 2, &alignment);
    TRTypesetterSetAttribute(text, 0, 2, &lineCount);

    TRUInteger length = 0;
    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 2);

    bool hasAlignment = false;
    bool hasLineCount = false;
    for (TRUInteger i = 0; i < 2; i++) {
        TRAttribute item = readItem(list, i);

        if (item.type == TRAttributeTextAlignment) {
            assert(item.value.textAlignment == TRTextAlignmentTrailing);
            hasAlignment = true;
        } else if (item.type == TRAttributeFirstIndentLineCount) {
            assert(item.value.firstIndentLineCount == 3);
            hasLineCount = true;
        }
    }
    assert(hasAlignment && hasLineCount);

    TRAttributeListRelease(list);
    TRTypesetterRelease(text);
}

void AttributeListTests::testParagraphAttributesCoverParagraphs() {
    TRMutableTypesetterRef text = createParagraphText();
    TRAttribute attribute = makeFloat(TRAttributeHeadIndent, 12.0f);
    TRUInteger length = 0;

    /* A range inside of the second paragraph covers the whole paragraph, including its newline. */
    TRTypesetterSetAttribute(text, 4, 1, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 3);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 3, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 3);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 5, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(readFloat(readItem(list, 0)) == 12.0f);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 6, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 2);
    TRAttributeListRelease(list);

    /* A range across two paragraphs covers both of them. */
    TRAttribute spacing = makeFloat(TRAttributeParagraphSpacing, 6.0f);
    TRTypesetterSetAttribute(text, 2, 3, &spacing);

    list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 3);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 3, &length);
    assert(TRAttributeListGetCount(list) == 2);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 6, &length);
    assert(TRAttributeListGetCount(list) == 0);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

void AttributeListTests::testEqualParagraphAttributesMerge() {
    TRMutableTypesetterRef text = createParagraphText();
    TRAttribute attribute = makeFloat(TRAttributeLineSpacing, 3.0f);
    TRUInteger length = 0;

    TRTypesetterSetAttribute(text, 0, 1, &attribute);
    TRTypesetterSetAttribute(text, 3, 1, &attribute);
    TRTypesetterSetAttribute(text, 6, 1, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 8);
    TRAttributeListRelease(list);

    TRAttribute other = makeFloat(TRAttributeLineSpacing, 4.0f);
    TRTypesetterSetAttribute(text, 3, 1, &other);

    list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(length == 3);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

namespace {

struct ReplacementState {
    int roomCalls = 0;
    int finalizeCalls = 0;
    TRFloat lastWidth = -1.0f;
};

void computeTestRoom(void *userData, TRFloat layoutWidth, TRReplacementRoom *room) {
    auto *state = static_cast<ReplacementState *>(userData);
    state->roomCalls++;
    state->lastWidth = layoutWidth;

    room->ascent = 10.0f;
    room->descent = 2.0f;
    room->extent = layoutWidth + 5.0f;
}

void finalizeTestReplacement(void *userData) {
    static_cast<ReplacementState *>(userData)->finalizeCalls++;
}

TRReplacementRef createReplacement(ReplacementState *state, bool isBlock = false) {
    TRReplacementCallbacks callbacks = { computeTestRoom, finalizeTestReplacement };
    return TRReplacementCreate(&callbacks, state,
        isBlock ? TRReplacementKindBlock : TRReplacementKindInline);
}

}

void AttributeListTests::testReplacementAttribute() {
    ReplacementState state;
    TRReplacementRef replacement = createReplacement(&state);
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = {};
    TRUInteger length = 0;

    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTypesetterSetAttribute(text, 2, 1, &attribute);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 1);
    assert(readItem(list, 0).type == TRAttributeReplacement);
    assert(readItem(list, 0).value.replacement == replacement);

    /* The replacement that is read back is still usable. */
    TRReplacementRoom room;
    TRReplacementComputeRoom(readItem(list, 0).value.replacement, 20.0f, &room);
    assert(room.extent == 25.0f);

    TRAttributeListRelease(list);

    /* The same replacement on adjacent characters is one run, a different one is not. */
    TRTypesetterSetAttribute(text, 3, 1, &attribute);
    list = TRTypesetterCopyAttributes(text, 2, &length);
    assert(length == 2);
    TRAttributeListRelease(list);

    ReplacementState otherState;
    TRReplacementRef other = createReplacement(&otherState);
    attribute.value.replacement = other;
    TRTypesetterSetAttribute(text, 4, 1, &attribute);

    list = TRTypesetterCopyAttributes(text, 3, &length);
    assert(length == 1);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
    TRReplacementRelease(other);
    TRReplacementRelease(replacement);

    assert(state.finalizeCalls == 1);
    assert(otherState.finalizeCalls == 1);
}

void AttributeListTests::testReplacementRetainBalance() {
    ReplacementState state;
    TRReplacementRef replacement = createReplacement(&state);
    TRMutableTypesetterRef text = createText();
    TRAttribute attribute = {};
    TRUInteger length = 0;

    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 1);

    /* The text holds on to the replacement, however many references it takes for that. */
    TRTypesetterSetAttribute(text, 0, 2, &attribute);
    size_t storedCount = AtomicUIntLoad(&replacement->_base.retainCount);
    assert(storedCount > 1);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == storedCount + 1);
    TRAttributeListRelease(list);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == storedCount);

    /* The text may keep unused values around, but never takes more references by removing. */
    TRTypesetterRemoveAttribute(text, 0, 2, TRAttributeReplacement);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) <= storedCount);

    /* The text lets go of everything when it is destroyed. */
    TRTypesetterSetAttribute(text, 0, 2, &attribute);
    TRTypesetterSetAttribute(text, 3, 2, &attribute);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) > 1);
    assert(state.finalizeCalls == 0);

    TRTypesetterRelease(text);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 1);
    assert(state.finalizeCalls == 0);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 1);
}

void AttributeListTests::testForegroundColorAttribute() {
    TRMutableTypesetterRef text = createText();
    TRAttribute red = {};
    TRUInteger length = 0;

    red.type = TRAttributeForegroundColor;
    red.value.foregroundColor = TRColorMake(0xFF, 0xFF, 0x00, 0x00);

    TRTypesetterSetAttribute(text, 1, 3, &red);

    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 1, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 3);
    assert(readItem(list, 0).type == TRAttributeForegroundColor);
    assert(readItem(list, 0).value.foregroundColor == TRColorMake(0xFF, 0xFF, 0x00, 0x00));
    TRAttributeListRelease(list);

    /* Equal colors of neighbors are one run, and different ones are not. */
    TRTypesetterSetAttribute(text, 4, 1, &red);
    list = TRTypesetterCopyAttributes(text, 1, &length);
    assert(length == 4);
    TRAttributeListRelease(list);

    TRAttribute blue = red;
    blue.value.foregroundColor = TRColorMake(0xFF, 0x00, 0x00, 0xFF);
    TRTypesetterSetAttribute(text, 4, 1, &blue);
    list = TRTypesetterCopyAttributes(text, 1, &length);
    assert(length == 3);
    TRAttributeListRelease(list);

    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeForegroundColor);
    list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0 && length == 6);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

void AttributeListTests::testDecorationAttributes() {
    TRMutableTypesetterRef text = createText();
    TRUInteger length = 0;

    TRAttribute background = {};
    background.type = TRAttributeBackgroundColor;
    background.value.backgroundColor = TRColorMake(0xFF, 0x10, 0x20, 0x30);
    TRTypesetterSetAttribute(text, 0, 6, &background);

    TRAttribute underline = {};
    underline.type = TRAttributeUnderline;
    underline.value.underline = TRTrue;
    TRTypesetterSetAttribute(text, 0, 3, &underline);
    TRTypesetterSetAttribute(text, 3, 3, &underline);

    TRAttribute strikethrough = {};
    strikethrough.type = TRAttributeStrikethrough;
    strikethrough.value.strikethrough = TRTrue;
    TRTypesetterSetAttribute(text, 2, 2, &strikethrough);

    TRAttribute decoration = {};
    decoration.type = TRAttributeDecorationColor;
    decoration.value.decorationColor = TRColorMake(0xFF, 0x40, 0x50, 0x60);
    TRTypesetterSetAttribute(text, 2, 2, &decoration);

    /* Equal values of neighbors are one run, and the attributes of a range are all kept. */
    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 2);
    assert(length == 2);
    assert(readItem(list, 0).type == TRAttributeBackgroundColor);
    assert(readItem(list, 0).value.backgroundColor == TRColorMake(0xFF, 0x10, 0x20, 0x30));
    assert(readItem(list, 1).type == TRAttributeUnderline);
    assert(readItem(list, 1).value.underline == TRTrue);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(list) == 4);
    assert(length == 2);
    TRAttributeListRelease(list);

    list = TRTypesetterCopyAttributes(text, 4, &length);
    assert(TRAttributeListGetCount(list) == 2);
    assert(length == 2);
    TRAttributeListRelease(list);

    /* A different value is not the same run, and an attribute can be removed on its own. */
    underline.value.underline = TRFalse;
    TRTypesetterSetAttribute(text, 4, 2, &underline);
    list = TRTypesetterCopyAttributes(text, 4, &length);
    assert(length == 2);
    assert(readItem(list, 1).value.underline == TRFalse);
    TRAttributeListRelease(list);

    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeUnderline);
    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeStrikethrough);
    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeDecorationColor);
    TRTypesetterRemoveAttribute(text, 0, 6, TRAttributeBackgroundColor);
    list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0 && length == 6);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

void AttributeListTests::testUserDataAttribute() {
    TRMutableTypesetterRef text = createText();
    int first = 1;
    int second = 2;
    TRUInteger length = 0;

    TRAttribute attribute = {};
    attribute.type = TRAttributeUserData;
    attribute.value.userData = &first;

    TRTypesetterSetAttribute(text, 0, 3, &attribute);
    TRTypesetterSetAttribute(text, 3, 3, &attribute);

    /* The same pointer on neighbors is one run. */
    TRAttributeListRef list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(readItem(list, 0).value.userData == &first);
    assert(length == 6);
    TRAttributeListRelease(list);

    /* Another pointer splits it, even if what it points to is equal. */
    int copyOfFirst = 1;
    attribute.value.userData = &copyOfFirst;
    TRTypesetterSetAttribute(text, 3, 3, &attribute);
    list = TRTypesetterCopyAttributes(text, 0, &length);
    assert(length == 3);
    TRAttributeListRelease(list);

    attribute.value.userData = &second;
    TRTypesetterSetAttribute(text, 3, 3, &attribute);
    list = TRTypesetterCopyAttributes(text, 3, &length);
    assert(readItem(list, 0).value.userData == &second);
    assert(length == 3);
    TRAttributeListRelease(list);

    TRTypesetterRelease(text);
}

#ifdef STANDALONE_TESTING

int main() {
    AttributeListTests tests;
    tests.run();

    return 0;
}

#endif
