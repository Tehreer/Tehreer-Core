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
    const TRUInteger size = sizeof(TRText);
    void *pointer = NULL;
    TRText *text = NULL;

    if (sbText) {
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

    if (config) {
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

void TRTextGetCodeUnits(TRTextRef text, TRUInteger index, TRUInteger length, void *buffer)
{
    SBTextGetCodeUnits(text->_sbText, index, length, buffer);
}

SBTextRef TRTextGetSheenBidiText(TRTextRef text)
{
    return text->_sbText;
}

TRAttributeListRef TRTextGetAttributes(TRTextRef text, TRUInteger index, TRUInteger *outLength)
{
    return SBTextGetAttributes(text->_sbText, SBAttributeFilterMakeAny(), index, outLength);
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

void TRTextAppendCodeUnits(TRMutableTextRef text, const void *codeUnitBuffer,
    TRUInteger codeUnitCount)
{
    SBTextAppendCodeUnits(text->_sbText, codeUnitBuffer, codeUnitCount);
}

void TRTextInsertCodeUnits(TRMutableTextRef text, TRUInteger index, const void *codeUnitBuffer,
    TRUInteger codeUnitCount)
{
    SBTextInsertCodeUnits(text->_sbText, index, codeUnitBuffer, codeUnitCount);
}

void TRTextDeleteCodeUnits(TRMutableTextRef text, TRUInteger index, TRUInteger length)
{
    SBTextDeleteCodeUnits(text->_sbText, index, length);
}

void TRTextSetCodeUnits(TRMutableTextRef text,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    SBTextSetCodeUnits(text->_sbText, codeUnitBuffer, codeUnitCount);
}

void TRTextReplaceCodeUnits(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    SBTextReplaceCodeUnits(text->_sbText, index, length, codeUnitBuffer, codeUnitCount);
}

void TRTextSetAttribute(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    const TRAttribute *attribute)
{
    SBAttributeID attributeID = AttributeRegistryGetAttributeID(attribute->type);

    if (attributeID != SBAttributeIDNone) {
        SBTextSetAttribute(text->_sbText, index, length, attributeID, attribute);
    }
}

void TRTextRemoveAttribute(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    TRAttributeType attributeType)
{
    SBAttributeID attributeID = AttributeRegistryGetAttributeID(attributeType);

    if (attributeID != SBAttributeIDNone) {
        SBTextRemoveAttribute(text->_sbText, index, length, attributeID);
    }
}
