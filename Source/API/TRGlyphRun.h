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

#ifndef _TEHREER_API_GLYPH_RUN_H
#define _TEHREER_API_GLYPH_RUN_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphRun.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Layout/TextRun.h>

/*
 * A glyph run looks at a range of a text run, which it keeps alive. It shares the glyphs of the text
 * run, and has the cluster map and the caret edges of its own range, which start at the first
 * cluster that the range touches. Only a justified run has advances of its own.
 */
typedef struct _TRGlyphRun {
    ObjectBase _base;
    TextRunRef textRun;
    TRUInteger codeUnitStart;
    TRUInteger codeUnitEnd;
    TRUInteger startExtra;
    TRUInteger endExtra;
    TRUInteger glyphStart;
    TRUInteger glyphCount;
    TRFloat *justifiedAdvances;
    TRUInteger *clusterMap;
    TRFloat *caretEdges;
    TRPoint origin;
    TRBoolean hasForegroundColor;
    TRColor foregroundColor;
    const void *userData;
} GlyphRun, *GlyphRunRef;

/* What a run is painted with. */
typedef struct _GlyphRunPaint {
    TRBoolean hasForegroundColor;
    TRColor foregroundColor;
    const void *userData;
} GlyphRunPaint;

/*
 * Creates a run for a range of a text run, which MUST NOT be empty. A replacement run is always
 * taken as a whole. Returns NULL on failure.
 */
TR_INTERNAL GlyphRunRef GlyphRunCreate(TextRunRef textRun, TRUInteger start, TRUInteger end,
    const GlyphRunPaint *paint);

/* Creates a copy of a run, which can be given another origin. */
TR_INTERNAL GlyphRunRef GlyphRunCreateCopy(GlyphRunRef glyphRun);

/*
 * Creates a copy of a run with other advances for its glyphs, one for each of them, and the caret
 * edges that follow from them.
 */
TR_INTERNAL GlyphRunRef GlyphRunCreateJustified(GlyphRunRef glyphRun, const TRFloat *advances);

TR_INTERNAL const TRFloat *GlyphRunGetAdvances(GlyphRunRef glyphRun);
TR_INTERNAL TRBoolean GlyphRunIsRTL(GlyphRunRef glyphRun);

/* The first code unit of the clusters, which can be before the run. */
TR_INTERNAL TRUInteger GlyphRunGetActualStart(GlyphRunRef glyphRun);

/* The extent of a range of code units of the run, which MAY be empty. */
TR_INTERNAL TRFloat GlyphRunGetDistanceInRange(GlyphRunRef glyphRun, TRUInteger start,
    TRUInteger end);

#endif
