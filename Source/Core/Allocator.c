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

#include <stdlib.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>

#include "Allocator.h"

#define AlignChunkSize(size_)   \
    (((size_) + sizeof(void *) - 1) / sizeof(void *) * sizeof(void *))

TR_INTERNAL void *AllocatorAllocateBlock(TRUInteger size)
{
    return malloc(size);
}

TR_INTERNAL void *AllocatorAllocateZeroedBlock(TRUInteger size)
{
    return calloc(1, size);
}

TR_INTERNAL void *AllocatorReallocateBlock(void *pointer, TRUInteger newSize)
{
    return realloc(pointer, newSize);
}

TR_INTERNAL void AllocatorDeallocateBlock(void *pointer)
{
    free(pointer);
}

TR_INTERNAL TRUInteger AllocatorCalculateBlockSize(const TRUInteger *chunkSizes, TRUInteger chunkCount)
{
    TRUInteger totalSize = 0;
    TRUInteger index;

    for (index = 0; index < chunkCount; index++) {
        totalSize += AlignChunkSize(chunkSizes[index]);
    }

    return totalSize;
}

TR_INTERNAL void AllocatorSplitBlock(void *pointer, const TRUInteger *chunkSizes,
    TRUInteger chunkCount, void **outPointers)
{
    TRUInt8 *chunkPointer = pointer;
    TRUInteger offset = 0;
    TRUInteger index;

    for (index = 0; index < chunkCount; index++) {
        TRUInteger chunkSize = chunkSizes[index];

        outPointers[index] = (chunkSize > 0 ? chunkPointer + offset : NULL);
        offset += AlignChunkSize(chunkSize);
    }
}

TR_INTERNAL TRBoolean AllocatorAllocateChunks(const TRUInteger *chunkSizes, TRUInteger chunkCount,
    void **outPointers)
{
    TRUInteger totalSize = AllocatorCalculateBlockSize(chunkSizes, chunkCount);
    TRBoolean succeeded = TRFalse;
    void *pointer;

    /* Total size MUST be greater than zero. */
    TRAssert(totalSize > 0);

    pointer = AllocatorAllocateBlock(totalSize);

    if (pointer) {
        AllocatorSplitBlock(pointer, chunkSizes, chunkCount, outPointers);
        succeeded = TRTrue;
    }

    return succeeded;
}
