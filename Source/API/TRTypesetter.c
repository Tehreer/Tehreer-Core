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
    TypesetterRef typesetter = object;
    TRUInteger index;

    for (index = 0; index < typesetter->runCount; index++) {
        TextRunRelease(typesetter->runs[index]);
    }

    AllocatorDeallocateBlock(typesetter->runs);
    AllocatorDeallocateBlock(typesetter->blocks);
    AllocatorDeallocateBlock(typesetter->paragraphs);
    AllocatorDeallocateBlock((void *)typesetter->buffer.codeUnits);

    if (typesetter->breaks) {
        BreakClassifierRelease(typesetter->breaks);
    }
    if (typesetter->text) {
        TRTextRelease(typesetter->text);
    }
}

static TRBoolean CopyCodeUnits(TypesetterRef typesetter, TRTextRef text)
{
    TRUInteger length = TRTextGetLength(text);
    TRUInteger unitSize = (typesetter->buffer.encoding == TRStringEncodingUTF8 ? 1
                           : (typesetter->buffer.encoding == TRStringEncodingUTF16 ? 2 : 4));
    void *units;

    /* A block is always allocated, so that an empty text has valid code units too. */
    units = AllocatorAllocateBlock((length > 0 ? length * unitSize : 1));
    if (!units) {
        return TRFalse;
    }

    if (length > 0) {
        TRTextGetCodeUnits(text, 0, length, units);
    }

    typesetter->buffer.codeUnits = units;
    typesetter->buffer.length = length;

    return TRTrue;
}

static TRBoolean CollectBlocks(TypesetterRef typesetter)
{
    TRUInteger blockCount = 0;
    TRUInteger index;

    for (index = 0; index < typesetter->runCount; index++) {
        if (TextRunIsBlock(typesetter->runs[index])) {
            blockCount += 1;
        }
    }

    if (blockCount > 0) {
        TRUInteger blockIndex = 0;

        typesetter->blocks = AllocatorAllocateBlock(blockCount * sizeof(TextRunRef));
        if (!typesetter->blocks) {
            return TRFalse;
        }

        for (index = 0; index < typesetter->runCount; index++) {
            if (TextRunIsBlock(typesetter->runs[index])) {
                typesetter->blocks[blockIndex++] = typesetter->runs[index];
            }
        }
    }

    typesetter->blockCount = blockCount;

    return TRTrue;
}

TRTypesetterRef TRTypesetterCreate(TRTextRef text, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount)
{
    const TRUInteger size = sizeof(TRTypesetter);
    void *pointer = NULL;
    TypesetterRef typesetter;

    if (!text) {
        return NULL;
    }

    typesetter = ObjectCreate(&size, 1, &pointer, FinalizeTypesetter);
    if (!typesetter) {
        return NULL;
    }

    typesetter->text = NULL;
    typesetter->buffer.codeUnits = NULL;
    typesetter->buffer.length = 0;
    typesetter->buffer.encoding = TRTextGetEncoding(text);
    typesetter->paragraphs = NULL;
    typesetter->paragraphCount = 0;
    typesetter->runs = NULL;
    typesetter->runCount = 0;
    typesetter->blocks = NULL;
    typesetter->blockCount = 0;
    typesetter->breaks = NULL;

    /* The copy is immutable, so the text can be changed by the caller afterwards. */
    typesetter->text = TRTextCreateCopy(text);

    if (typesetter->text && CopyCodeUnits(typesetter, typesetter->text)
            && ShapeResolverResolve(typesetter, defaultAttributes, defaultAttributeCount)
            && CollectBlocks(typesetter)) {
        typesetter->breaks = BreakClassifierCreate(typesetter->buffer.codeUnits,
            typesetter->buffer.length, typesetter->buffer.encoding);
    }

    if (!typesetter->breaks) {
        ObjectRelease(typesetter);
        return NULL;
    }

    return typesetter;
}

TRUInteger TRTypesetterGetCodeUnitCount(TRTypesetterRef typesetter)
{
    return typesetter->buffer.length;
}

TR_INTERNAL TRUInteger TypesetterFindParagraph(TypesetterRef typesetter, TRUInteger index)
{
    TRUInteger low = 0;
    TRUInteger high = typesetter->paragraphCount;

    while (low < high) {
        TRUInteger mid = (low + high) >> 1;
        const ParagraphInfo *paragraph = &typesetter->paragraphs[mid];

        if (index >= paragraph->end) {
            low = mid + 1;
        } else if (index < paragraph->start) {
            high = mid;
        } else {
            return mid;
        }
    }

    return TRInvalidIndex;
}

TR_INTERNAL TRUInteger TypesetterFindRun(TypesetterRef typesetter, TRUInteger index)
{
    TRUInteger low = 0;
    TRUInteger high = typesetter->runCount;

    while (low < high) {
        TRUInteger mid = (low + high) >> 1;
        const TextRun *textRun = typesetter->runs[mid];

        if (index >= textRun->codeUnitEnd) {
            low = mid + 1;
        } else if (index < textRun->codeUnitStart) {
            high = mid;
        } else {
            return mid;
        }
    }

    return TRInvalidIndex;
}

TR_INTERNAL TRFloat TypesetterMeasureRange(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end)
{
    TRFloat extent = 0.0f;

    if (start < end) {
        TRUInteger runIndex = TypesetterFindRun(typesetter, start);

        do {
            TextRunRef textRun = typesetter->runs[runIndex];
            TRUInteger segmentEnd = (end < textRun->codeUnitEnd ? end : textRun->codeUnitEnd);

            extent += TextRunGetDistance(textRun, start, segmentEnd);

            start = segmentEnd;
            runIndex += 1;
        } while (start < end);
    }

    return extent;
}

void TRTypesetterGetParagraph(TRTypesetterRef typesetter, TRUInteger index, TRRange *range,
    TRUInt8 *baseLevel)
{
    TypesetterRef actual = (TypesetterRef)typesetter;
    const ParagraphInfo *paragraph;

    /* The index MUST be less than the number of code units. */
    TRAssert(index < actual->buffer.length);

    paragraph = &actual->paragraphs[TypesetterFindParagraph(actual, index)];

    if (range) {
        range->index = paragraph->start;
        range->length = paragraph->end - paragraph->start;
    }
    if (baseLevel) {
        *baseLevel = paragraph->baseLevel;
    }
}

TRUInteger TRTypesetterSuggestForwardBreak(TRTypesetterRef typesetter, TRRange range,
    TRFloat extent, TRBreakMode breakMode)
{
    return BreakResolverSuggestForwardBreak((TypesetterRef)typesetter, extent, range.index,
        range.index + range.length, breakMode);
}

TRUInteger TRTypesetterSuggestBackwardBreak(TRTypesetterRef typesetter, TRRange range,
    TRFloat extent, TRBreakMode breakMode)
{
    return BreakResolverSuggestBackwardBreak((TypesetterRef)typesetter, extent, range.index,
        range.index + range.length, breakMode);
}

TRComposedLineRef TRTypesetterCreateSimpleLine(TRTypesetterRef typesetter, TRRange range)
{
    /* The range MUST NOT be empty, and MUST be within the text. */
    TRAssert(range.length > 0 && range.index + range.length <= typesetter->buffer.length);

    return LineResolverCreateSimpleLine((TypesetterRef)typesetter, range.index,
        range.index + range.length, TRFalse, 0.0f);
}

TRComposedLineRef TRTypesetterCreateFrameLine(TRTypesetterRef typesetter, TRRange range,
    TRFloat layoutWidth)
{
    /* The range MUST NOT be empty, and MUST be within the text. */
    TRAssert(range.length > 0 && range.index + range.length <= typesetter->buffer.length);

    return LineResolverCreateSimpleLine((TypesetterRef)typesetter, range.index,
        range.index + range.length, TRTrue, layoutWidth);
}

TRComposedLineRef TRTypesetterCreateTruncationToken(TRTypesetterRef typesetter, TRRange range,
    TRTruncationPlace truncationPlace, const void *tokenString, TRUInteger tokenLength,
    TRStringEncoding tokenEncoding)
{
    /* The range MUST NOT be empty, and MUST be within the text. */
    TRAssert(range.length > 0 && range.index + range.length <= typesetter->buffer.length);

    return TokenResolverCreateTokenLine((TypesetterRef)typesetter, range.index,
        range.index + range.length, truncationPlace, tokenString, tokenLength, tokenEncoding);
}

TRComposedLineRef TRTypesetterCreateTruncatedLine(TRTypesetterRef typesetter, TRRange range,
    TRFloat extent, TRBreakMode breakMode, TRTruncationPlace truncationPlace,
    TRComposedLineRef tokenLine)
{
    /* The range MUST NOT be empty, and MUST be within the text. */
    TRAssert(range.length > 0 && range.index + range.length <= typesetter->buffer.length);

    return LineResolverCreateTruncatedLine((TypesetterRef)typesetter, range.index,
        range.index + range.length, extent, breakMode, truncationPlace, (ComposedLineRef)tokenLine);
}

TRComposedLineRef TRTypesetterCreateJustifiedLine(TRTypesetterRef typesetter, TRRange range,
    TRFloat justificationFactor, TRFloat justificationExtent)
{
    /* The range MUST NOT be empty, and MUST be within the text. */
    TRAssert(range.length > 0 && range.index + range.length <= typesetter->buffer.length);

    return LineResolverCreateJustifiedLine((TypesetterRef)typesetter, range.index,
        range.index + range.length, justificationFactor, justificationExtent);
}

TRTypesetterRef TRTypesetterRetain(TRTypesetterRef typesetter)
{
    return ObjectRetain((ObjectRef)typesetter);
}

void TRTypesetterRelease(TRTypesetterRef typesetter)
{
    ObjectRelease((ObjectRef)typesetter);
}
