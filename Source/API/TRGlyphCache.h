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
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Array.h>
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
    Array buckets;
    TRUInteger entryCount;
    struct _GlyphCacheEntry *firstEntry;
    struct _GlyphCacheEntry *lastEntry;
} TRGlyphCache;

/*
 * What decides the shape of a glyph: the typeface, the size in pixels in 26.6 format, and the
 * horizontal skew in 16.16 format.
 */
typedef struct _GlyphDataKey {
    TRTypefaceRef typeface;
    TRInt32 pixelWidth;
    TRInt32 pixelHeight;
    TRInt32 skewX;
} GlyphDataKey;

/*
 * The stroker settings: the line radius in 26.6 format, the cap and join in FreeType terms, and the
 * miter limit in 16.16 format.
 */
typedef struct _GlyphStrokeKey {
    TRInt32 lineRadius;
    TRUInt32 lineCap;
    TRUInt32 lineJoin;
    TRInt32 miterLimit;
} GlyphStrokeKey;

/*
 * The lookups below return a new reference, or NULL if the glyph has nothing to show or cannot be
 * loaded. Glyphs are rendered without holding the lock of the cache, so threads that miss on the
 * same glyph at the same time may all render it. The first result stays in the cache, and the
 * others are used once and dropped.
 */
TR_INTERNAL TRBoolean TRGlyphCacheInitialize(TRGlyphCacheRef cache, TRUInteger capacity);
TR_INTERNAL void TRGlyphCacheFinalize(TRGlyphCacheRef cache);

/*
 * Returns the image of the glyph in its default form, which uses the foreground color only if the
 * glyph has layers that are painted with it.
 */
TR_INTERNAL TRGlyphImageRef TRGlyphCacheCopyImage(TRGlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID, TRColor foregroundColor);

/* Returns the image of the glyph that is made by stroking its outline. */
TR_INTERNAL TRGlyphImageRef TRGlyphCacheCopyStrokeImage(TRGlyphCacheRef cache,
    const GlyphDataKey *key, const GlyphStrokeKey *strokeKey, TRGlyphID glyphID);

/* Returns the outline of the glyph in pixels. */
TR_INTERNAL TRPathRef TRGlyphCacheCopyPath(TRGlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID);

#endif
