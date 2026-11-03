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

#include <Tehreer/TRSheenBidi.h>

#include <API/TRAssert.h>
#include <API/TRAttributeList.h>
#include <API/TRBase.h>
#include <Core/Object.h>
#include <Layout/BreakResolver.h>
#include <Layout/LineResolver.h>
#include <Layout/TokenResolver.h>
#include <Layout/TextBuffer.h>

#include "TRTypesetter.h"

static void FinalizeTypesetter(ObjectRef object)
{
    TRTypesetter *typesetter = object;

    TextSourceFinalize(&typesetter->source);
}

static TRBoolean IsEncodingValid(TRStringEncoding encoding)
{
    return (encoding == TRStringEncodingUTF8
            || encoding == TRStringEncodingUTF16
            || encoding == TRStringEncodingUTF32);
}

/* Takes the source, which the typesetter finalizes with itself. */
static TRTypesetter *WrapSource(const TextSource *source, TRBoolean isMutable)
{
    const TRUInteger size = sizeof(TRTypesetter);
    void *pointer = NULL;
    TRTypesetter *typesetter = ObjectCreate(&size, 1, &pointer, FinalizeTypesetter);

    if (typesetter) {
        typesetter->source = *source;
        typesetter->isMutable = isMutable;
        typesetter->isEditing = TRFalse;
    }

    return typesetter;
}

static TRTypesetter *CreateFromString(const void *string, TRUInteger length,
    TRStringEncoding encoding, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount, TRBoolean isMutable)
{
    TRTypesetter *typesetter = NULL;
    TextSource source;

    if (TextSourceInitialize(&source, string, length, encoding, defaultAttributes,
            defaultAttributeCount, isMutable)) {
        typesetter = WrapSource(&source, isMutable);

        if (!typesetter) {
            TextSourceFinalize(&source);
        }
    }

    return typesetter;
}

static TRTypesetter *CreateCopy(TRTypesetterRef typesetter, TRBoolean isMutable)
{
    TRTypesetter *copy = NULL;

    if (!typesetter->isEditing) {
        TextSource source;

        if (TextSourceInitializeCopy(&source, &typesetter->source, isMutable)) {
            copy = WrapSource(&source, isMutable);

            if (!copy) {
                TextSourceFinalize(&source);
            }
        }
    }

    return copy;
}

static TRBoolean IsBoundaryTypeValid(TRBoundaryType type)
{
    return (type == TRBoundaryTypeGrapheme
            || type == TRBoundaryTypeWord
            || type == TRBoundaryTypeParagraph);
}

static BreakType GetBoundaryBreakType(TRBoundaryType type)
{
    return (type == TRBoundaryTypeGrapheme ? BreakTypeGrapheme : BreakTypeWord);
}

/* ---------- Internal Functions ---------- */

TR_INTERNAL TRBoolean TRTypesetterIsShaped(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end)
{
    return (!typesetter->isEditing && TextSourceIsShaped(&typesetter->source, start, end));
}

/* ---------- Public Functions ---------- */

TRTypesetterRef TRTypesetterCreate(const void *string, TRUInteger length,
    TRStringEncoding encoding, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount)
{
    TRTypesetter *typesetter = NULL;

    if ((string || length == 0) && IsEncodingValid(encoding)) {
        typesetter = CreateFromString(string, length, encoding, defaultAttributes,
            defaultAttributeCount, TRFalse);
    }

    return typesetter;
}

TRMutableTypesetterRef TRTypesetterCreateMutable(TRStringEncoding encoding,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount)
{
    TRTypesetter *typesetter = NULL;

    if (IsEncodingValid(encoding)) {
        typesetter = CreateFromString(NULL, 0, encoding, defaultAttributes, defaultAttributeCount,
            TRTrue);
    }

    return typesetter;
}

TRTypesetterRef TRTypesetterCreateCopy(TRTypesetterRef typesetter)
{
    return CreateCopy(typesetter, TRFalse);
}

TRMutableTypesetterRef TRTypesetterCreateMutableCopy(TRTypesetterRef typesetter)
{
    return CreateCopy(typesetter, TRTrue);
}

TRStringEncoding TRTypesetterGetEncoding(TRTypesetterRef typesetter)
{
    return TextSourceGetEncoding(&typesetter->source);
}

TRUInteger TRTypesetterGetLength(TRTypesetterRef typesetter)
{
    return TextSourceGetLength(&typesetter->source);
}

TRBoolean TRTypesetterGetCodeUnits(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, void *buffer)
{
    TRUInteger textLength = TRTypesetterGetLength(typesetter);
    TRBoolean isCopied = TRFalse;

    if (buffer && RangeIsValid(index, length, textLength)) {
        TextSourceReadCodeUnits(&typesetter->source, index, length, buffer);

        isCopied = TRTrue;
    }

    return isCopied;
}

TRAttributeListRef TRTypesetterCopyAttributes(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger *outLength)
{
    TRAttributeListRef attributes = NULL;
    TRUInteger length = 0;

    if (IndexIsValid(index, TRTypesetterGetLength(typesetter))) {
        attributes = TextSourceCopyAttributes(&typesetter->source, index, &length);
    }

    if (outLength) {
        *outLength = length;
    }

    return attributes;
}

/*
 * The end and the start of a paragraph are boundaries of every type, so a search never leaves the
 * paragraph that holds the code unit it starts from. The breaks inside of it are classified with
 * the paragraph, whether it is shaped or not.
 */
TRUInteger TRTypesetterFindBoundary(TRTypesetterRef typesetter, TRUInteger index,
    TRBoundaryType type, TRBoundaryDirection direction)
{
    TRUInteger length = TRTypesetterGetLength(typesetter);
    TRBoolean isForward = (direction == TRBoundaryDirectionForward);
    TRBoolean isSearchable = (isForward ? index < length : (index > 0 && index <= length));
    TRUInteger boundary = TRInvalidIndex;

    if (isSearchable && !typesetter->isEditing && IsBoundaryTypeValid(type)
            && (isForward || direction == TRBoundaryDirectionBackward)) {
        ParagraphInfo paragraph;
        TRUInteger offset = TRInvalidIndex;

        TextSourceGetParagraph(&typesetter->source, isForward ? index : index - 1, &paragraph);

        if (type == TRBoundaryTypeParagraph) {
            offset = (isForward ? paragraph.end - paragraph.start : 0);
        } else if (paragraph.shaped) {
            BreakClassifierRef breaks = paragraph.shaped->breaks;
            BreakType breakType = GetBoundaryBreakType(type);
            TRUInteger from = index - paragraph.start;

            offset = (isForward
                      ? BreakClassifierGetForwardBreak(breaks, breakType, from,
                                                       paragraph.end - paragraph.start)
                      : BreakClassifierGetBackwardBreak(breaks, breakType, from, 0));
        }

        if (offset != TRInvalidIndex) {
            boundary = paragraph.start + offset;
        }
    }

    return boundary;
}

TRRange TRTypesetterGetBoundaryRange(TRTypesetterRef typesetter, TRUInteger index,
    TRBoundaryType type)
{
    TRUInteger length = TRTypesetterGetLength(typesetter);
    TRRange range = { TRInvalidIndex, 0 };

    if (index == length && !typesetter->isEditing && IsBoundaryTypeValid(type)) {
        range.index = length;
    } else if (index < length) {
        TRUInteger end = TRTypesetterFindBoundary(typesetter, index, type,
            TRBoundaryDirectionForward);
        TRUInteger start = TRTypesetterFindBoundary(typesetter, index + 1, type,
            TRBoundaryDirectionBackward);

        if (start != TRInvalidIndex && end != TRInvalidIndex) {
            range.index = start;
            range.length = end - start;
        }
    }

    return range;
}

/* ---------- Editing ---------- */

void TRTypesetterBeginEditing(TRMutableTypesetterRef typesetter)
{
    TextSourceBeginEditing(&typesetter->source);
    typesetter->isEditing = TRTrue;
}

void TRTypesetterEndEditing(TRMutableTypesetterRef typesetter)
{
    TextSourceEndEditing(&typesetter->source);
    typesetter->isEditing = TRFalse;
}

TRBoolean TRTypesetterAppendCodeUnits(TRMutableTypesetterRef typesetter,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRBoolean isAppended = (codeUnitBuffer != NULL);

    if (isAppended) {
        TextSourceAppendCodeUnits(&typesetter->source, codeUnitBuffer, codeUnitCount);
    }

    return isAppended;
}

TRBoolean TRTypesetterInsertCodeUnits(TRMutableTypesetterRef typesetter, TRUInteger index,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRBoolean isInserted = TRFalse;

    if (codeUnitBuffer && index <= TRTypesetterGetLength(typesetter)) {
        TextSourceInsertCodeUnits(&typesetter->source, index, codeUnitBuffer, codeUnitCount);

        isInserted = TRTrue;
    }

    return isInserted;
}

TRBoolean TRTypesetterDeleteCodeUnits(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length)
{
    TRUInteger textLength = TRTypesetterGetLength(typesetter);
    TRBoolean isDeleted = RangeIsValid(index, length, textLength);

    if (isDeleted) {
        TextSourceDeleteCodeUnits(&typesetter->source, index, length);
    }

    return isDeleted;
}

TRBoolean TRTypesetterSetCodeUnits(TRMutableTypesetterRef typesetter,
    const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRBoolean isSet = (codeUnitBuffer || codeUnitCount == 0);

    if (isSet) {
        TextSourceSetCodeUnits(&typesetter->source, codeUnitBuffer, codeUnitCount);
    }

    return isSet;
}

TRBoolean TRTypesetterReplaceCodeUnits(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, const void *codeUnitBuffer, TRUInteger codeUnitCount)
{
    TRUInteger textLength = TRTypesetterGetLength(typesetter);
    TRBoolean isReplaced = (RangeIsValid(index, length, textLength)
                            && (codeUnitBuffer || codeUnitCount == 0));

    if (isReplaced) {
        TextSourceReplaceCodeUnits(&typesetter->source, index, length, codeUnitBuffer,
            codeUnitCount);
    }

    return isReplaced;
}

TRBoolean TRTypesetterSetAttribute(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, const TRAttribute *attribute)
{
    TRUInteger textLength = TRTypesetterGetLength(typesetter);
    TRBoolean isSet = TRFalse;

    if (attribute && RangeIsValid(index, length, textLength)) {
        isSet = TextSourceSetAttribute(&typesetter->source, index, length, attribute);
    }

    return isSet;
}

TRBoolean TRTypesetterRemoveAttribute(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRAttributeType attributeType)
{
    TRUInteger textLength = TRTypesetterGetLength(typesetter);
    TRBoolean isRemoved = TRFalse;

    if (RangeIsValid(index, length, textLength)) {
        isRemoved = TextSourceRemoveAttribute(&typesetter->source, index, length, attributeType);
    }

    return isRemoved;
}

/* ---------- Lines ---------- */

TRUInteger TRTypesetterSuggestForwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode)
{
    TRUInteger breakIndex = TRInvalidIndex;

    if (length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        breakIndex = BreakResolverSuggestForwardBreak(&typesetter->source, extent, index,
            index + length,
            breakMode);
    }

    return breakIndex;
}

TRUInteger TRTypesetterSuggestBackwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode)
{
    TRUInteger breakIndex = TRInvalidIndex;

    if (length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        breakIndex = BreakResolverSuggestBackwardBreak(&typesetter->source, extent, index,
            index + length,
            breakMode);
    }

    return breakIndex;
}

TRComposedLineRef TRTypesetterCreateSimpleLine(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat layoutWidth)
{
    TRComposedLineRef line = NULL;

    if (length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        line = LineResolverCreateSimpleLine(&typesetter->source, index, index + length,
            layoutWidth);
    }

    return line;
}

TRComposedLineRef TRTypesetterCreateTruncationToken(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRTruncationPlace truncationPlace, const void *tokenString,
    TRUInteger tokenLength, TRStringEncoding tokenEncoding)
{
    TRComposedLineRef token = NULL;

    if (length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        token = TokenResolverCreateTokenLine(typesetter, index, index + length, truncationPlace,
            tokenString, tokenLength, tokenEncoding);
    }

    return token;
}

TRComposedLineRef TRTypesetterCreateTruncatedLine(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode, TRTruncationPlace truncationPlace,
    TRComposedLineRef tokenLine)
{
    TRComposedLineRef line = NULL;

    if (tokenLine && length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        line = LineResolverCreateTruncatedLine(&typesetter->source, index, index + length, extent,
            breakMode, truncationPlace, tokenLine);
    }

    return line;
}

TRComposedLineRef TRTypesetterCreateJustifiedLine(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat justificationFactor, TRFloat justificationExtent)
{
    TRComposedLineRef line = NULL;

    if (length > 0 && RangeIsValid(index, length, TRTypesetterGetLength(typesetter))
            && TRTypesetterIsShaped(typesetter, index, index + length)) {
        line = LineResolverCreateJustifiedLine(&typesetter->source, index, index + length,
            justificationFactor, justificationExtent);
    }

    return line;
}

TRTypesetterRef TRTypesetterRetain(TRTypesetterRef typesetter)
{
    return ObjectRetain((ObjectRef)typesetter);
}

void TRTypesetterRelease(TRTypesetterRef typesetter)
{
    ObjectRelease((ObjectRef)typesetter);
}

/* ---------- SheenBidi ---------- */

SBTextRef TRTypesetterGetSheenBidiText(TRTypesetterRef typesetter)
{
    return TextSourceGetSheenBidiText(&typesetter->source);
}
