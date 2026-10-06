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

#include <API/TRBase.h>
#include <API/TRAssert.h>
#include <Core/AtomicUInt.h>
#include <Core/Memory.h>

#include "Object.h"

TR_INTERNAL void ObjectBaseInitialize(ObjectBaseRef objectBase)
{
    MemoryInitialize(&objectBase->memory);
    objectBase->finalize = NULL;
    objectBase->retainCount = 0;
}

TR_INTERNAL ObjectRef ObjectCreate(const TRUInteger *chunkSizes, TRUInteger chunkCount,
    void **outPointers, FinalizeFunc finalizer)
{
    ObjectBaseRef base = NULL;
    Memory memory;

    /* Number of chunks MUST be greater than or equal to one. */
    TRAssert(chunkCount >= 1);
    /* Size of first chunk MUST be at least the size of ObjectBase structure. */
    TRAssert(chunkSizes[0] >= sizeof(ObjectBase));

    MemoryInitialize(&memory);

    if (MemoryAllocateChunks(&memory, chunkSizes, chunkCount, outPointers)) {
        base = outPointers[0];
        base->memory = memory;
        base->finalize = finalizer;

        AtomicUIntInit(&base->retainCount, 1);
    }

    return base;
}

TR_INTERNAL ObjectRef ObjectRetain(ObjectRef object)
{
    ObjectBaseRef base = (ObjectBaseRef)object;

    AtomicUIntIncrement(&base->retainCount);

    return object;
}

TR_INTERNAL void ObjectRelease(ObjectRef object)
{
    ObjectBaseRef base = (ObjectBaseRef)object;

    if (AtomicUIntDecrement(&base->retainCount) == 0) {
        if (base->finalize) {
            base->finalize(object);
        }

        MemoryFinalize(&base->memory);
    }
}
