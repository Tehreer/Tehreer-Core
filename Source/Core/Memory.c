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

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Allocator.h>

#include "Memory.h"

TR_INTERNAL void MemoryInitialize(MemoryRef memory)
{
    memory->_list = NULL;
}

TR_INTERNAL void *MemoryAllocateBlock(MemoryRef memory, TRUInteger size)
{
    MemoryListRef memoryList = memory->_list;
    void *pointer = NULL;

    /* Size MUST be greater than zero. */
    TRAssert(size > 0);

    if (memoryList) {
        const TRUInteger headerSize = sizeof(MemoryBlock);

        pointer = AllocatorAllocateBlock(headerSize + size);

        if (pointer) {
            TRUInt8 *base = pointer;
            MemoryBlockRef block;

            block = pointer;
            block->next = NULL;

            memoryList->last->next = block;
            memoryList->last = block;

            pointer = base + headerSize;
        }
    } else {
        const TRUInteger headerSize = sizeof(MemoryList);

        pointer = AllocatorAllocateBlock(headerSize + size);

        if (pointer) {
            TRUInt8 *base = pointer;

            memoryList = pointer;
            memoryList->first.next = NULL;
            memoryList->last = &memoryList->first;

            memory->_list = memoryList;

            pointer = base + headerSize;
        }
    }

    return pointer;
}

TR_INTERNAL TRBoolean MemoryAllocateChunks(MemoryRef memory, const TRUInteger *chunkSizes,
    TRUInteger chunkCount, void **outPointers)
{
    TRUInteger totalSize = AllocatorCalculateBlockSize(chunkSizes, chunkCount);
    TRBoolean succeeded = TRFalse;
    void *pointer;

    /* Total size MUST be greater than zero. */
    TRAssert(totalSize > 0);

    pointer = MemoryAllocateBlock(memory, totalSize);

    if (pointer) {
        AllocatorSplitBlock(pointer, chunkSizes, chunkCount, outPointers);
        succeeded = TRTrue;
    }

    return succeeded;
}

TR_INTERNAL void MemoryFinalize(MemoryRef memory)
{
    MemoryListRef memoryList = memory->_list;

    if (memoryList) {
        MemoryBlockRef block = &memoryList->first;

        while (block) {
            MemoryBlockRef next = block->next;
            /* Deallocate the block along with its data as they were allocated together. */
            AllocatorDeallocateBlock(block);

            block = next;
        }
    }
}
