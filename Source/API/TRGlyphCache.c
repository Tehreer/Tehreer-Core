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
#include <Core/Once.h>
#include <Graphics/GlyphCache.h>

#include "TRGlyphCache.h"

#define DefaultCapacity (8 * 1024 * 1024)

static GlyphCacheRef DefaultCache;

static void FinalizeGlyphCache(ObjectRef object)
{
    GlyphCacheFinalize(object);
}

TRGlyphCacheRef TRGlyphCacheCreate(TRUInteger capacity)
{
    const TRUInteger size = sizeof(GlyphCache);
    void *pointer = NULL;
    GlyphCacheRef cache;

    cache = ObjectCreate(&size, 1, &pointer, FinalizeGlyphCache);

    if (cache && !GlyphCacheInitialize(cache, capacity)) {
        /* Nothing was initialized, so it must not be finalized as a cache. */
        cache->_base.finalize = NULL;
        ObjectRelease(cache);
        cache = NULL;
    }

    return cache;
}

static void InitDefaultCache(void)
{
    DefaultCache = TRGlyphCacheCreate(DefaultCapacity);
}

TRGlyphCacheRef TRGlyphCacheGetDefault(void)
{
    static Once once = OnceMake();
    OnceExecute(&once, InitDefaultCache);

    return DefaultCache;
}

TRUInteger TRGlyphCacheGetCapacity(TRGlyphCacheRef cache)
{
    TRUInteger capacity;

    MutexLock(&cache->mutex);
    capacity = cache->capacity;
    MutexUnlock(&cache->mutex);

    return capacity;
}

void TRGlyphCacheSetCapacity(TRGlyphCacheRef cache, TRUInteger capacity)
{
    GlyphCacheSetCapacity(cache, capacity);
}

TRUInteger TRGlyphCacheGetSize(TRGlyphCacheRef cache)
{
    TRUInteger size;

    MutexLock(&cache->mutex);
    size = cache->size;
    MutexUnlock(&cache->mutex);

    return size;
}

void TRGlyphCacheClear(TRGlyphCacheRef cache)
{
    GlyphCacheClear(cache);
}

TRGlyphCacheRef TRGlyphCacheRetain(TRGlyphCacheRef cache)
{
    return ObjectRetain((ObjectRef)cache);
}

void TRGlyphCacheRelease(TRGlyphCacheRef cache)
{
    ObjectRelease((ObjectRef)cache);
}
