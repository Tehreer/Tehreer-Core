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

#ifndef _TEHREER_GLYPH_CACHE_H
#define _TEHREER_GLYPH_CACHE_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * A cache of the images and outlines of glyphs. It keeps the most recently used ones within a
 * capacity that is measured in bytes, and drops the least recently used ones when it is exceeded.
 *
 * A cache can be used from multiple threads. A glyph that is dropped while it is still in use
 * stays alive until its last user lets go of it.
 */
typedef struct _TRGlyphCache *TRGlyphCacheRef;

/**
 * Creates a cache.
 *
 * @param capacity
 *      The size in bytes up to which the cache keeps glyphs.
 * @return
 *      New cache, or `NULL` on failure.
 */
TR_PUBLIC TRGlyphCacheRef TRGlyphCacheCreate(TRUInteger capacity);

/**
 * Returns the cache that renderers use by default. It has a capacity of 8 MB, lives as long as the
 * process does, and must not be released by the caller.
 */
TR_PUBLIC TRGlyphCacheRef TRGlyphCacheGetDefault(void);

/**
 * Returns the capacity of the cache in bytes.
 */
TR_PUBLIC TRUInteger TRGlyphCacheGetCapacity(TRGlyphCacheRef cache);

/**
 * Changes the capacity of the cache, and drops glyphs if it is exceeded now.
 */
TR_PUBLIC void TRGlyphCacheSetCapacity(TRGlyphCacheRef cache, TRUInteger capacity);

/**
 * Returns the number of bytes that the cache holds at the moment.
 */
TR_PUBLIC TRUInteger TRGlyphCacheGetSize(TRGlyphCacheRef cache);

/**
 * Drops everything that the cache holds.
 */
TR_PUBLIC void TRGlyphCacheClear(TRGlyphCacheRef cache);

/**
 * Retains the cache.
 */
TR_PUBLIC TRGlyphCacheRef TRGlyphCacheRetain(TRGlyphCacheRef cache);

/**
 * Releases the cache.
 */
TR_PUBLIC void TRGlyphCacheRelease(TRGlyphCacheRef cache);

TR_EXTERN_C_END

#endif
