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
#include <Tehreer/TRText.h>
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
    testPointSizeAttribute();
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

static TRMutableTextRef createParagraphText() {
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF8);
    TRTextAppendCodeUnits(text, "ab\ncd\nef", 8);

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
        TRMutableTextRef text = createText();
        TRAttribute attribute = makeFloat(type, 1.5f);
        TRUInteger length = 0;

        TRTextSetAttribute(text, 1, 3, &attribute);

        TRAttributeListRef list = TRTextGetAttributes(text, 1, &length);
        assert(TRAttributeListGetCount(list) == 1);
        assert(length == 3);
        assert(readItem(list, 0).type == type);
        assert(readFloat(readItem(list, 0)) == 1.5f);
        SBAttributeListRelease(list);

        /* Outside of the range there is nothing, as these attributes apply to the exact range. */
        list = TRTextGetAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 0);
        assert(length == 1);
        SBAttributeListRelease(list);

        TRTextRemoveAttribute(text, 0, 6, type);
        list = TRTextGetAttributes(text, 2, &length);
        assert(TRAttributeListGetCount(list) == 0);
        SBAttributeListRelease(list);

        TRTextRelease(text);
    }

    /* Each of them has an identity of its own, so they do not replace each other. */
    TRMutableTextRef text = createText();
    for (TRAttributeType type : types) {
        TRAttribute attribute = makeFloat(type, 2.0f);
        TRTextSetAttribute(text, 0, 6, &attribute);
    }

    TRUInteger length = 0;
    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 4);
    assert(length == 6);
    SBAttributeListRelease(list);

    TRTextRelease(text);
}

void AttributeListTests::testParagraphAttributeValues() {
    const TRAttributeType types[] = {
        TRAttributeFirstLineHeadIndent, TRAttributeHeadIndent, TRAttributeTailIndent,
        TRAttributeParagraphSpacingBefore, TRAttributeParagraphSpacing,
        TRAttributeLineHeightMultiple, TRAttributeMinimumLineHeight,
        TRAttributeMaximumLineHeight, TRAttributeLineSpacing
    };

    for (TRAttributeType type : types) {
        TRMutableTextRef text = createParagraphText();
        TRAttribute attribute = makeFloat(type, -4.25f);
        TRUInteger length = 0;

        TRTextSetAttribute(text, 0, 2, &attribute);

        TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 1);
        assert(readItem(list, 0).type == type);
        assert(readFloat(readItem(list, 0)) == -4.25f);
        SBAttributeListRelease(list);

        TRTextRemoveAttribute(text, 0, 8, type);
        list = TRTextGetAttributes(text, 0, &length);
        assert(TRAttributeListGetCount(list) == 0);
        SBAttributeListRelease(list);

        TRTextRelease(text);
    }

    /* The alignment and the line count are not floating-point values. */
    TRMutableTextRef text = createParagraphText();
    TRAttribute alignment = {};
    alignment.type = TRAttributeTextAlignment;
    alignment.value.textAlignment = TRTextAlignmentTrailing;

    TRAttribute lineCount = {};
    lineCount.type = TRAttributeFirstIndentLineCount;
    lineCount.value.firstIndentLineCount = 3;

    TRTextSetAttribute(text, 0, 2, &alignment);
    TRTextSetAttribute(text, 0, 2, &lineCount);

    TRUInteger length = 0;
    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
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

    SBAttributeListRelease(list);
    TRTextRelease(text);
}

void AttributeListTests::testParagraphAttributesCoverParagraphs() {
    TRMutableTextRef text = createParagraphText();
    TRAttribute attribute = makeFloat(TRAttributeHeadIndent, 12.0f);
    TRUInteger length = 0;

    /* A range inside of the second paragraph covers the whole paragraph, including its newline. */
    TRTextSetAttribute(text, 4, 1, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 3);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 3, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 3);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 5, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(readFloat(readItem(list, 0)) == 12.0f);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 6, &length);
    assert(TRAttributeListGetCount(list) == 0);
    assert(length == 2);
    SBAttributeListRelease(list);

    /* A range across two paragraphs covers both of them. */
    TRAttribute spacing = makeFloat(TRAttributeParagraphSpacing, 6.0f);
    TRTextSetAttribute(text, 2, 3, &spacing);

    list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 3);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 3, &length);
    assert(TRAttributeListGetCount(list) == 2);
    SBAttributeListRelease(list);

    list = TRTextGetAttributes(text, 6, &length);
    assert(TRAttributeListGetCount(list) == 0);
    SBAttributeListRelease(list);

    TRTextRelease(text);
}

void AttributeListTests::testEqualParagraphAttributesMerge() {
    TRMutableTextRef text = createParagraphText();
    TRAttribute attribute = makeFloat(TRAttributeLineSpacing, 3.0f);
    TRUInteger length = 0;

    TRTextSetAttribute(text, 0, 1, &attribute);
    TRTextSetAttribute(text, 3, 1, &attribute);
    TRTextSetAttribute(text, 6, 1, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 8);
    SBAttributeListRelease(list);

    TRAttribute other = makeFloat(TRAttributeLineSpacing, 4.0f);
    TRTextSetAttribute(text, 3, 1, &other);

    list = TRTextGetAttributes(text, 0, &length);
    assert(length == 3);
    SBAttributeListRelease(list);

    TRTextRelease(text);
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
    return TRReplacementCreate(&callbacks, state, 1.5f, isBlock);
}

}

void AttributeListTests::testReplacementAttribute() {
    ReplacementState state;
    TRReplacementRef replacement = createReplacement(&state);
    TRMutableTextRef text = createText();
    TRAttribute attribute = {};
    TRUInteger length = 0;

    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTextSetAttribute(text, 2, 1, &attribute);

    TRAttributeListRef list = TRTextGetAttributes(text, 2, &length);
    assert(TRAttributeListGetCount(list) == 1);
    assert(length == 1);
    assert(readItem(list, 0).type == TRAttributeReplacement);
    assert(readItem(list, 0).value.replacement == replacement);

    /* The replacement that is read back is still usable. */
    TRReplacementRoom room;
    TRReplacementComputeRoom(readItem(list, 0).value.replacement, 20.0f, &room);
    assert(room.extent == 25.0f);
    assert(TRReplacementGetUserData(readItem(list, 0).value.replacement) == &state);

    SBAttributeListRelease(list);

    /* The same replacement on adjacent characters is one run, a different one is not. */
    TRTextSetAttribute(text, 3, 1, &attribute);
    list = TRTextGetAttributes(text, 2, &length);
    assert(length == 2);
    SBAttributeListRelease(list);

    ReplacementState otherState;
    TRReplacementRef other = createReplacement(&otherState);
    attribute.value.replacement = other;
    TRTextSetAttribute(text, 4, 1, &attribute);

    list = TRTextGetAttributes(text, 3, &length);
    assert(length == 1);
    SBAttributeListRelease(list);

    TRTextRelease(text);
    TRReplacementRelease(other);
    TRReplacementRelease(replacement);

    assert(state.finalizeCalls == 1);
    assert(otherState.finalizeCalls == 1);
}

void AttributeListTests::testReplacementRetainBalance() {
    ReplacementState state;
    TRReplacementRef replacement = createReplacement(&state);
    TRMutableTextRef text = createText();
    TRAttribute attribute = {};
    TRUInteger length = 0;

    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 1);

    /* The text holds on to the replacement, however many references it takes for that. */
    TRTextSetAttribute(text, 0, 2, &attribute);
    size_t storedCount = AtomicUIntLoad(&replacement->_base.retainCount);
    assert(storedCount > 1);

    TRAttributeListRef list = TRTextGetAttributes(text, 0, &length);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == storedCount + 1);
    SBAttributeListRelease(list);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == storedCount);

    /* The text may keep unused values around, but never takes more references by removing. */
    TRTextRemoveAttribute(text, 0, 2, TRAttributeReplacement);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) <= storedCount);

    /* The text lets go of everything when it is destroyed. */
    TRTextSetAttribute(text, 0, 2, &attribute);
    TRTextSetAttribute(text, 3, 2, &attribute);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) > 1);
    assert(state.finalizeCalls == 0);

    TRTextRelease(text);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 1);
    assert(state.finalizeCalls == 0);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 1);
}

#ifdef STANDALONE_TESTING

int main() {
    AttributeListTests tests;
    tests.run();

    return 0;
}

#endif
