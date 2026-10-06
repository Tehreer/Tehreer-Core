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

#ifndef _TEHREER_CORE_ALLOCATOR_H
#define _TEHREER_CORE_ALLOCATOR_H

#include <API/TRBase.h>

TR_INTERNAL void *AllocatorAllocateBlock(TRUInteger size);
TR_INTERNAL void *AllocatorReallocateBlock(void *pointer, TRUInteger newSize);
TR_INTERNAL void AllocatorDeallocateBlock(void *pointer);

/**
 * Computes the total size required for a set of memory chunks. Each chunk is padded so that the
 * next one starts at an aligned address.
 */
TR_INTERNAL TRUInteger AllocatorCalculateBlockSize(const TRUInteger *chunkSizes, TRUInteger chunkCount);

/**
 * Splits a contiguous memory block into multiple logical chunks.
 */
TR_INTERNAL void AllocatorSplitBlock(void *pointer, const TRUInteger *chunkSizes,
    TRUInteger chunkCount, void **outPointers);

TR_INTERNAL TRBoolean AllocatorAllocateChunks(const TRUInteger *chunkSizes, TRUInteger chunkCount,
    void **outPointers);

#endif
