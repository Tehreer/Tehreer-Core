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

#include <stddef.h>

#include <SheenBidi/SBText.h>

#include <Tehreer/TRSheenBidi.h>

#include <API/TRAttributeList.h>
#include <API/TRBase.h>
#include <Core/Object.h>
#include <Text/AttributeRegistry.h>

#include "TRText.h"

static void FinalizeText(ObjectRef object)
{
    TRText *text = object;

    SBTextRelease(text->_sbText);
}

static TRText *CreateText(SBMutableTextRef sbText, TRBoolean isMutable)
{
    TRText *text = NULL;

    if (sbText) {
        const TRUInteger size = sizeof(TRText);
        void *pointer = NULL;

        text = ObjectCreate(&size, 1, &pointer, FinalizeText);

        if (text) {
            text->_sbText = sbText;
            text->isMutable = isMutable;
            text->isEditing = TRFalse;
        } else {
            SBTextRelease(sbText);
        }
    }

    return text;
}

TRTextRef TRTextCreate(const void *string, TRUInteger length, TRStringEncoding encoding)
{
    SBTextConfigRef config = AttributeRegistryGetDefaultConfig();
    SBTextRef sbText = NULL;

    if (config && (string || length == 0)) {
        sbText = SBTextCreate(string, length, encoding, config);
    }

    return CreateText((SBMutableTextRef)sbText, TRFalse);
}

TRTextRef TRTextCreateCopy(TRTextRef text)
{
    return CreateText((SBMutableTextRef)SBTextCreateCopy(text->_sbText), TRFalse);
}

TRMutableTextRef TRTextCreateMutableCopy(TRTextRef text)
{
    return CreateText(SBTextCreateMutableCopy(text->_sbText), TRTrue);
}

TRStringEncoding TRTextGetEncoding(TRTextRef text)
{
    return SBTextGetEncoding(text->_sbText);
}

TRUInteger TRTextGetLength(TRTextRef text)
{
    return SBTextGetLength(text->_sbText);
}

TRBoolean TRTextGetCodeUnits(TRTextRef text, TRUInteger index, TRUInteger length, void *buffer)
{
    TRUInteger textLength = SBTextGetLength(text->_sbText);
    TRBoolean isCopied = TRFalse;

    if (buffer && RangeIsValid(index, length, textLength)) {
        if (length > 0) {
            SBTextGetCodeUnits(text->_sbText, index, length, buffer);
        }

        isCopied = TRTrue;
    }

    return isCopied;
}

TRAttributeListRef TRTextCopyAttributes(TRTextRef text, TRUInteger index, TRUInteger *outLength)
{
    SBAttributeListRef sbList = NULL;
    TRUInteger length = 0;

    if (IndexIsValid(index, SBTextGetLength(text->_sbText))) {
        sbList = SBTextGetAttributes(text->_sbText, SBAttributeFilterMakeAny(), index, &length);
    }

    if (outLength) {
        *outLength = length;
    }

    return TRAttributeListMake(sbList);
}

TRTextRef TRTextRetain(TRTextRef text)
{
    return ObjectRetain((ObjectRef)text);
}

void TRTextRelease(TRTextRef text)
{
    ObjectRelease((ObjectRef)text);
}


/* ----------------------------------
 * Mutable Text
 * ---------------------------------- */

TRMutableTextRef TRTextCreateMutable(TRStringEncoding encoding)
{
    SBTextConfigRef config = AttributeRegistryGetDefaultConfig();
    SBMutableTextRef sbText = NULL;

    if (config) {
        sbText = SBTextCreateMutable(encoding, config);
    }

    return CreateText(sbText, TRTrue);
}

void TRTextBeginEditing(TRMutableTextRef text)
{
    SBTextBeginEditing(text->_sbText);
    text->isEditing = TRTrue;
}

void TRTextEndEditing(TRMutableTextRef text)
{
    SBTextEndEditing(text->_sbText);
    text->isEditing = TRFalse;
}

TRBoolean TRTextAppendCodeUnits(TRMutableTextRef text, const void *codeUnitBuffer,
    TRUInteger codeUnitCount)
{
    TRBoolean isAppended = (codeUnitBuffer != NULL);

    if (isAppended && codeUnitCount > 0) {
        SBTextAppendCodeUnits(text->_sbText, codeUnitBuffer, codeUnitCount);
    }

    return isAppended;
}

TRBoolean TRTextInsertCodeUnits(TRMutableTextRef text, TRUInteger index,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRBoolean isInserted = TRFalse;

    if (codeUnitBuffer && index <= SBTextGetLength(text->_sbText)) {
        if (codeUnitCount > 0) {
            SBTextInsertCodeUnits(text->_sbText, index, codeUnitBuffer, codeUnitCount);
        }

        isInserted = TRTrue;
    }

    return isInserted;
}

TRBoolean TRTextDeleteCodeUnits(TRMutableTextRef text, TRUInteger index, TRUInteger length)
{
    TRUInteger textLength = SBTextGetLength(text->_sbText);
    TRBoolean isDeleted = RangeIsValid(index, length, textLength);

    if (isDeleted && length > 0) {
        SBTextDeleteCodeUnits(text->_sbText, index, length);
    }

    return isDeleted;
}

TRBoolean TRTextSetCodeUnits(TRMutableTextRef text, const void *codeUnitBuffer,
    TRUInteger codeUnitCount)
{
    TRBoolean isSet = (codeUnitBuffer || codeUnitCount == 0);

    if (isSet) {
        SBTextSetCodeUnits(text->_sbText, codeUnitBuffer, codeUnitCount);
    }

    return isSet;
}

TRBoolean TRTextReplaceCodeUnits(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRUInteger textLength = SBTextGetLength(text->_sbText);
    TRBoolean isReplaced = (RangeIsValid(index, length, textLength)
                            && (codeUnitBuffer || codeUnitCount == 0));

    if (isReplaced) {
        if (codeUnitCount == 0) {
            if (length > 0) {
                SBTextDeleteCodeUnits(text->_sbText, index, length);
            }
        } else {
            SBTextReplaceCodeUnits(text->_sbText, index, length, codeUnitBuffer, codeUnitCount);
        }
    }

    return isReplaced;
}

TRBoolean TRTextSetAttribute(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    const TRAttribute *attribute)
{
    TRUInteger textLength = SBTextGetLength(text->_sbText);
    SBAttributeID attributeID = SBAttributeIDNone;
    TRBoolean isSet = TRFalse;

    if (attribute) {
        attributeID = AttributeRegistryGetAttributeID(attribute->type);
    }

    if (attributeID != SBAttributeIDNone && RangeIsValid(index, length, textLength)) {
        if (length > 0) {
            SBTextSetAttribute(text->_sbText, index, length, attributeID, attribute);
        }

        isSet = TRTrue;
    }

    return isSet;
}

TRBoolean TRTextRemoveAttribute(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    TRAttributeType attributeType)
{
    TRUInteger textLength = SBTextGetLength(text->_sbText);
    SBAttributeID attributeID = AttributeRegistryGetAttributeID(attributeType);
    TRBoolean isRemoved = TRFalse;

    if (attributeID != SBAttributeIDNone && RangeIsValid(index, length, textLength)) {
        if (length > 0) {
            SBTextRemoveAttribute(text->_sbText, index, length, attributeID);
        }

        isRemoved = TRTrue;
    }

    return isRemoved;
}

/* ----------------------------------
 * SheenBidi
 * ---------------------------------- */

SBTextRef TRTextGetSheenBidiText(TRTextRef text)
{
    return text->_sbText;
}
