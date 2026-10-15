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
#include <stdlib.h>
#include <string.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRSheenBidi.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRText.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Layout/BreakResolver.h>
#include <Layout/LineResolver.h>
#include <Layout/TokenResolver.h>
#include <Layout/ShapeResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

#include "TRTypesetter.h"

static void FinalizeTypesetter(ObjectRef object)
{
    TRTypesetter *typesetter = object;
    TRUInteger count = ArrayGetCount(&typesetter->runs);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TextRunRelease(*(TextRunRef *)ArrayGetItem(&typesetter->runs, index));
    }

    ArrayFinalize(&typesetter->runs);
    BreakClassifierRelease(typesetter->breaks);
    AllocatorDeallocateBlock((void *)typesetter->buffer.codeUnits);
    TRTextRelease(typesetter->text);
}

static void *CreateCodeUnits(TRTextRef text, TRStringEncoding encoding)
{
    TRUInteger length = TRTextGetLength(text);
    TRUInteger unitSize = (encoding == TRStringEncodingUTF8 ? 1
                           : (encoding == TRStringEncodingUTF16 ? 2 : 4));

    /* A block is always allocated, so that an empty text has valid code units too. */
    void *codeUnits = AllocatorAllocateBlock((length > 0 ? length * unitSize : 1));

    if (codeUnits && length > 0) {
        TRTextGetCodeUnits(text, 0, length, codeUnits);
    }

    return codeUnits;
}

/* Takes over the text, the code units and the breaks if it succeeds. The runs are still empty. */
static TRTypesetter *CreateEmptyTypesetter(TRTextRef text, const TextBuffer *buffer,
    BreakClassifierRef breaks)
{
    const TRUInteger size = sizeof(TRTypesetter);
    void *pointer = NULL;
    TRTypesetter *typesetter;

    typesetter = ObjectCreate(&size, 1, &pointer, FinalizeTypesetter);

    if (typesetter) {
        typesetter->text = text;
        typesetter->buffer = *buffer;
        typesetter->breaks = breaks;
        ArrayInitialize(&typesetter->runs, sizeof(TextRunRef));
    }

    return typesetter;
}

TR_INTERNAL void TRTypesetterGetParagraph(TRTypesetterRef typesetter, TRUInteger index,
    ParagraphInfo *paragraph)
{
    SBParagraphInfo sbParagraph;

    /* The code unit MUST be within the text. */
    TRAssert(index < typesetter->buffer.length);

    SBTextGetCodeUnitParagraphInfo(TRTextGetSheenBidiText(typesetter->text), index, &sbParagraph);

    paragraph->start = sbParagraph.index;
    paragraph->end = sbParagraph.index + sbParagraph.length;
    paragraph->baseLevel = sbParagraph.baseLevel;
}

TR_INTERNAL TRUInteger TRTypesetterGetRunCount(TRTypesetterRef typesetter)
{
    return ArrayGetCount(&typesetter->runs);
}

TR_INTERNAL TextRunRef TRTypesetterGetRun(TRTypesetterRef typesetter, TRUInteger index)
{
    return *(TextRunRef *)ArrayGetItem(&typesetter->runs, index);
}

TR_INTERNAL TRUInteger TRTypesetterFindRun(TRTypesetterRef typesetter, TRUInteger index)
{
    TRUInteger runIndex = TRInvalidIndex;
    TRUInteger low = 0;
    TRUInteger high = TRTypesetterGetRunCount(typesetter);

    while (low < high) {
        TRUInteger mid = (low + high) >> 1;
        const TextRun *textRun = TRTypesetterGetRun(typesetter, mid);

        if (index >= textRun->codeUnitEnd) {
            low = mid + 1;
        } else if (index < textRun->codeUnitStart) {
            high = mid;
        } else {
            runIndex = mid;
            break;
        }
    }

    return runIndex;
}

TR_INTERNAL TRFloat TRTypesetterMeasureRange(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end)
{
    TRFloat extent = 0.0f;

    if (start < end) {
        TRUInteger runIndex = TRTypesetterFindRun(typesetter, start);

        do {
            TextRunRef textRun = TRTypesetterGetRun(typesetter, runIndex);
            TRUInteger segmentEnd = NumberMin(end, textRun->codeUnitEnd);

            extent += TextRunGetDistance(textRun, start, segmentEnd);

            start = segmentEnd;
            runIndex += 1;
        } while (start < end);
    }

    return extent;
}

TRTypesetterRef TRTypesetterCreate(TRTextRef text, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount)
{
    TRTypesetter *typesetter = NULL;

    if (text) {
        TRStringEncoding encoding = TRTextGetEncoding(text);

        /* The copy is immutable, so the text can be changed by the caller afterwards. */
        TRTextRef textCopy = TRTextCreateCopy(text);
        BreakClassifierRef breaks = NULL;
        TextBuffer buffer;

        buffer.codeUnits = NULL;
        buffer.length = 0;
        buffer.encoding = encoding;

        if (textCopy) {
            buffer.codeUnits = CreateCodeUnits(textCopy, encoding);
            buffer.length = TRTextGetLength(textCopy);
        }
        if (buffer.codeUnits) {
            breaks = BreakClassifierCreate(buffer.codeUnits, buffer.length, encoding);
        }
        if (breaks) {
            typesetter = CreateEmptyTypesetter(textCopy, &buffer, breaks);
        }

        if (typesetter) {
            if (!ShapeResolverResolve(typesetter, defaultAttributes, defaultAttributeCount)) {
                ObjectRelease(typesetter);
                typesetter = NULL;
            }
        } else {
            if (breaks) {
                BreakClassifierRelease(breaks);
            }
            AllocatorDeallocateBlock((void *)buffer.codeUnits);
            if (textCopy) {
                TRTextRelease(textCopy);
            }
        }
    }

    return typesetter;
}

TRUInteger TRTypesetterGetCodeUnitCount(TRTypesetterRef typesetter)
{
    return typesetter->buffer.length;
}

TRUInteger TRTypesetterSuggestForwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode)
{
    TRUInteger breakIndex = TRInvalidIndex;

    if (length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
        breakIndex = BreakResolverSuggestForwardBreak(typesetter, extent, index, index + length,
            breakMode);
    }

    return breakIndex;
}

TRUInteger TRTypesetterSuggestBackwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode)
{
    TRUInteger breakIndex = TRInvalidIndex;

    if (length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
        breakIndex = BreakResolverSuggestBackwardBreak(typesetter, extent, index, index + length,
            breakMode);
    }

    return breakIndex;
}

TRComposedLineRef TRTypesetterCreateSimpleLine(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat layoutWidth)
{
    TRComposedLineRef line = NULL;

    if (length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
        line = LineResolverCreateSimpleLine(typesetter, index, index + length, layoutWidth);
    }

    return line;
}

TRComposedLineRef TRTypesetterCreateTruncationToken(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRTruncationPlace truncationPlace, const void *tokenString,
    TRUInteger tokenLength, TRStringEncoding tokenEncoding)
{
    TRComposedLineRef token = NULL;

    if (length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
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

    if (tokenLine && length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
        line = LineResolverCreateTruncatedLine(typesetter, index, index + length, extent,
            breakMode, truncationPlace, tokenLine);
    }

    return line;
}

TRComposedLineRef TRTypesetterCreateJustifiedLine(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat justificationFactor, TRFloat justificationExtent)
{
    TRComposedLineRef line = NULL;

    if (length > 0 && RangeIsValid(index, length, typesetter->buffer.length)) {
        line = LineResolverCreateJustifiedLine(typesetter, index, index + length,
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
