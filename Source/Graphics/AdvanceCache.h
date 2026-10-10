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

#ifndef _TEHREER_GRAPHICS_ADVANCE_CACHE_H
#define _TEHREER_GRAPHICS_ADVANCE_CACHE_H

#include <API/TRBase.h>
#include <Core/Mutex.h>

/**
 * A thread-safe cache of unscaled glyph advances. Advances live in pages that are allocated when a
 * glyph of their range is first stored, so the memory use follows the glyphs that are used.
 */
typedef struct _AdvanceCache {
    Mutex _mutex;
    TRInt32 **_pages;
    TRUInteger _pageCount;
    TRUInteger _glyphCount;
} AdvanceCache, *AdvanceCacheRef;

/**
 * Initializes a cache for glyph IDs below `glyphCount`.
 */
TR_INTERNAL void AdvanceCacheInitialize(AdvanceCacheRef cache, TRUInteger glyphCount);
TR_INTERNAL void AdvanceCacheFinalize(AdvanceCacheRef cache);

/**
 * Looks up the advance of a glyph. Returns `TRFalse` if it is not cached.
 */
TR_INTERNAL TRBoolean AdvanceCacheGet(AdvanceCacheRef cache, TRGlyphID glyphID, TRInt32 *advance);

/**
 * Stores the advance of a glyph. Glyphs out of range are not cached.
 */
TR_INTERNAL void AdvanceCachePut(AdvanceCacheRef cache, TRGlyphID glyphID, TRInt32 advance);

#endif
