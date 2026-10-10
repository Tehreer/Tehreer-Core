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
#include <Core/Object.h>

#include "TRReplacement.h"

static void FinalizeReplacement(ObjectRef object)
{
    TRReplacement *replacement = object;

    if (replacement->callbacks.finalize) {
        replacement->callbacks.finalize(replacement->userData);
    }
}

TRReplacementRef TRReplacementCreate(const TRReplacementCallbacks *callbacks, void *userData,
    TRReplacementKind kind)
{
    TRReplacement *replacement = NULL;

    if (callbacks && (kind == TRReplacementKindInline || kind == TRReplacementKindBlock)) {
        const TRUInteger size = sizeof(TRReplacement);
        void *pointer = NULL;

        replacement = ObjectCreate(&size, 1, &pointer, FinalizeReplacement);

        if (replacement) {
            replacement->callbacks = *callbacks;
            replacement->userData = userData;
            replacement->kind = kind;
        }
    }

    return replacement;
}

TRReplacementKind TRReplacementGetKind(TRReplacementRef replacement)
{
    return replacement->kind;
}

void TRReplacementComputeRoom(TRReplacementRef replacement, TRFloat layoutWidth,
    TRReplacementRoom *room)
{
    room->ascent = 0.0f;
    room->descent = 0.0f;
    room->extent = 0.0f;

    if (replacement->callbacks.computeRoom) {
        replacement->callbacks.computeRoom(replacement->userData, layoutWidth, room);
    }
}

TRReplacementRef TRReplacementRetain(TRReplacementRef replacement)
{
    return ObjectRetain((ObjectRef)replacement);
}

void TRReplacementRelease(TRReplacementRef replacement)
{
    ObjectRelease((ObjectRef)replacement);
}
