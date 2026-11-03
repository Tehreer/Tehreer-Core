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
#include <string.h>

#include <API/TRAssert.h>
#include <Core/Allocator.h>

#include "ShapedParagraph.h"

static void FinalizeShapedParagraph(ObjectRef object)
{
    ShapedParagraph *paragraph = object;
    TRUInteger count = ArrayGetCount(&paragraph->runs);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TextRunRelease(*(TextRunRef *)ArrayGetItem(&paragraph->runs, index));
    }

    ArrayFinalize(&paragraph->runs);
    BreakClassifierRelease(paragraph->breaks);
    AllocatorDeallocateBlock((void *)paragraph->buffer.codeUnits);
}

static ShapedParagraph *CreateEmptyParagraph(void *codeUnits, TRUInteger length,
    TRStringEncoding encoding, BreakClassifierRef breaks)
{
    const TRUInteger size = sizeof(ShapedParagraph);
    void *pointer = NULL;
    ShapedParagraph *paragraph;

    paragraph = ObjectCreate(&size, 1, &pointer, FinalizeShapedParagraph);

    if (paragraph) {
        paragraph->buffer.codeUnits = codeUnits;
        paragraph->buffer.length = length;
        paragraph->buffer.encoding = encoding;
        paragraph->breaks = breaks;
        paragraph->isShaped = TRFalse;
        paragraph->isUnclaimed = TRFalse;
        ArrayInitialize(&paragraph->runs, sizeof(TextRunRef));
    }

    return paragraph;
}

static const void *RetainUserInfo(const void *userInfo)
{
    ShapedParagraphRef paragraph = (ShapedParagraphRef)userInfo;

    /* SheenBidi retains the value of the provider, which already holds the reference to give. */
    if (paragraph->isUnclaimed) {
        ((ShapedParagraph *)paragraph)->isUnclaimed = TRFalse;
    } else {
        ShapedParagraphRetain(paragraph);
    }

    return paragraph;
}

static void ReleaseUserInfo(const void *userInfo)
{
    ShapedParagraphRelease((ShapedParagraphRef)userInfo);
}

TR_INTERNAL ShapedParagraphRef ShapedParagraphCreate(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding)
{
    ShapedParagraph *paragraph = NULL;
    TRUInteger unitSize = TextBufferGetCodeUnitSize(encoding);

    /* A block is always allocated, so that an empty paragraph has valid code units too. */
    void *ownedUnits = AllocatorAllocateBlock(length > 0 ? length * unitSize : 1);
    BreakClassifierRef breaks = NULL;

    if (ownedUnits) {
        if (length > 0) {
            memcpy(ownedUnits, codeUnits, length * unitSize);
        }

        breaks = BreakClassifierCreate(ownedUnits, length, encoding);
    }
    if (breaks) {
        paragraph = CreateEmptyParagraph(ownedUnits, length, encoding, breaks);
    }

    if (!paragraph) {
        if (breaks) {
            BreakClassifierRelease(breaks);
        }
        if (ownedUnits) {
            AllocatorDeallocateBlock(ownedUnits);
        }
    }

    return paragraph;
}

TR_INTERNAL TextRunRef ShapedParagraphGetRun(ShapedParagraphRef paragraph, TRUInteger index)
{
    return *(TextRunRef *)ArrayGetItem(&paragraph->runs, index);
}

TR_INTERNAL TRUInteger ShapedParagraphFindRun(ShapedParagraphRef paragraph, TRUInteger index)
{
    TRUInteger runIndex = TRInvalidIndex;
    TRUInteger low = 0;
    TRUInteger high = ArrayGetCount(&paragraph->runs);

    while (low < high) {
        TRUInteger mid = (low + high) >> 1;
        const TextRun *textRun = ShapedParagraphGetRun(paragraph, mid);

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

TR_INTERNAL const SBParagraphUserInfoCallbacks *ShapedParagraphGetUserInfoCallbacks(void)
{
    static const SBParagraphUserInfoCallbacks callbacks = { RetainUserInfo, ReleaseUserInfo };

    return &callbacks;
}

TR_INTERNAL ShapedParagraphRef ShapedParagraphRetain(ShapedParagraphRef paragraph)
{
    return ObjectRetain((ObjectRef)paragraph);
}

TR_INTERNAL void ShapedParagraphRelease(ShapedParagraphRef paragraph)
{
    ObjectRelease((ObjectRef)paragraph);
}
