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

#ifndef _TEHREER_API_RENDERER_H
#define _TEHREER_API_RENDERER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGlyphCache.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _TRRenderer {
    ObjectBase _base;
    TRGlyphCacheRef cache;
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat scaleX;
    TRFloat scaleY;
    TRFloat skewX;
    TRFloat renderScale;
    TRWritingDirection writingDirection;
    TRColor foregroundColor;
    TRFloat strokeRadius;
    TRStrokeCap strokeCap;
    TRStrokeJoin strokeJoin;
    TRFloat strokeMiter;
    TRColor strokeColor;
    TRDrawStyle drawStyle;
    TRDrawCallbacks drawCallbacks;
    void *drawUserData;
} TRRenderer;

/* Rounds half up, which does not depend on the sign as the truncation of a cast does. */
TR_INTERNAL TRFloat TRRendererRoundPixel(TRFloat value);

/*
 * Called for the outline of a glyph, which is in pixels. The origin is where the pen position of
 * the glyph goes, relative to the start of the run and not rounded.
 */
typedef void (*TRPathPlacementFunc)(void *userData, TRUInteger glyphIndex, TRPathRef path,
    TRPoint origin, TRBoolean *stop);

/*
 * Passes the outline of each glyph of a run to a function, with its position in pixels, which
 * follows the pen in the same way as `TRRendererEnumerateGlyphPlacements()`. Glyphs without an
 * outline are skipped.
 */
TR_INTERNAL void TRRendererEnumeratePathPlacements(TRRendererRef renderer,
    const TRGlyphID *glyphIDs, const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    TRPathPlacementFunc func, void *userData);

#endif
