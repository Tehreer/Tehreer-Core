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

#ifndef _TEHREER_CORE_ARRAY_H
#define _TEHREER_CORE_ARRAY_H

#include <API/TRBase.h>

/**
 * A growable array of items of the same size. The items are stored contiguously.
 */
typedef struct _Array {
    void *_items;
    TRUInteger _itemSize;
    TRUInteger _count;
    TRUInteger _capacity;
} Array, *ArrayRef;

/**
 * Initializes an empty array. It does not allocate any memory.
 */
TR_INTERNAL void ArrayInitialize(ArrayRef array, TRUInteger itemSize);

/**
 * Releases the memory of the array, but not the items that it refers to.
 */
TR_INTERNAL void ArrayFinalize(ArrayRef array);

TR_INTERNAL TRUInteger ArrayGetCount(const Array *array);

/**
 * Returns the address of an item. The index MUST be less than the count.
 */
TR_INTERNAL void *ArrayGetItem(const Array *array, TRUInteger index);

/**
 * Returns the items as a contiguous block, or `NULL` if the array is empty.
 */
TR_INTERNAL void *ArrayGetItems(const Array *array);

/**
 * Changes the count of the array. The items that are added are zero. Returns `TRFalse` if memory
 * could not be allocated, in which case the array is left as it was.
 */
TR_INTERNAL TRBoolean ArrayResize(ArrayRef array, TRUInteger count);

/**
 * Copies an item to the end of the array. Returns `TRFalse` if memory could not be allocated, in
 * which case the array is left as it was.
 */
TR_INTERNAL TRBoolean ArrayAppend(ArrayRef array, const void *item);

#endif
