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

#ifndef _TEHREER_API_GLYPH_CACHE_H
#define _TEHREER_API_GLYPH_CACHE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGlyphCache.h>

#include <API/TRBase.h>
#include <Core/Mutex.h>
#include <Core/Object.h>

struct _GlyphCacheEntry;

/*
 * The entries are kept in a hash table, and in a list that goes from the most recently used one to
 * the least recently used one. The mutex guards everything below it.
 */
typedef struct _TRGlyphCache {
    ObjectBase _base;
    Mutex mutex;
    TRUInteger capacity;
    TRUInteger size;
    struct _GlyphCacheEntry **buckets;
    TRUInteger bucketCount;
    TRUInteger entryCount;
    struct _GlyphCacheEntry *firstEntry;
    struct _GlyphCacheEntry *lastEntry;
} GlyphCache, *GlyphCacheRef;

#endif
