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
#include <API/TRBase.h>
#include <Core/Allocator.h>

#include "Array.h"

#define InitialCapacity     8

TR_INTERNAL void ArrayInitialize(ArrayRef array, TRUInteger itemSize)
{
    array->_items = NULL;
    array->_itemSize = itemSize;
    array->_count = 0;
    array->_capacity = 0;
}

TR_INTERNAL void ArrayFinalize(ArrayRef array)
{
    AllocatorDeallocateBlock(array->_items);

    array->_items = NULL;
    array->_count = 0;
    array->_capacity = 0;
}

TR_INTERNAL TRUInteger ArrayGetCount(const Array *array)
{
    return array->_count;
}

TR_INTERNAL void *ArrayGetItem(const Array *array, TRUInteger index)
{
    /* The index MUST be less than the count. */
    TRAssert(index < array->_count);

    return (TRUInt8 *)array->_items + (index * array->_itemSize);
}

TR_INTERNAL void *ArrayGetItems(const Array *array)
{
    return array->_items;
}

TR_INTERNAL TRBoolean ArrayResize(ArrayRef array, TRUInteger count)
{
    TRBoolean isResized = TRTrue;

    if (count > array->_capacity) {
        void *items = AllocatorReallocateBlock(array->_items, count * array->_itemSize);

        if (items) {
            array->_items = items;
            array->_capacity = count;
        } else {
            isResized = TRFalse;
        }
    }

    if (isResized) {
        if (count > array->_count) {
            memset((TRUInt8 *)array->_items + (array->_count * array->_itemSize), 0,
                (count - array->_count) * array->_itemSize);
        }

        array->_count = count;
    }

    return isResized;
}

TR_INTERNAL TRBoolean ArrayAppend(ArrayRef array, const void *item)
{
    TRBoolean isAppended = TRFalse;
    TRBoolean hasSpace = (array->_count < array->_capacity);

    if (!hasSpace) {
        TRUInteger capacity = (array->_capacity ? array->_capacity * 2 : InitialCapacity);
        void *items = AllocatorReallocateBlock(array->_items, capacity * array->_itemSize);

        if (items) {
            array->_items = items;
            array->_capacity = capacity;
            hasSpace = TRTrue;
        }
    }

    if (hasSpace) {
        memcpy((TRUInt8 *)array->_items + (array->_count * array->_itemSize), item,
            array->_itemSize);
        array->_count += 1;
        isAppended = TRTrue;
    }

    return isAppended;
}
