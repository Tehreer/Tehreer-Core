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

#ifndef _TEHREER_GLYPH_RUN_H
#define _TEHREER_GLYPH_RUN_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/**
 * A glyph run is a part of a line that has a single typeface, size and direction, and one set of
 * attributes for painting. The glyphs are in the order of the writing, so those of a right-to-left
 * run go from right to left.
 *
 * A run can start or end in the middle of a cluster, when a line breaks inside of one. Such a run
 * still has all the glyphs of the cluster, and the extra code units of the cluster are counted in
 * its cluster map and its caret edges, so that a wrapper can clip the glyphs that belong to the
 * other line.
 *
 * Indexes of code units are those of the text, and glyph indexes are relative to the run. A glyph
 * run never changes, so it can be used from multiple threads.
 */
typedef const struct _TRGlyphRun *TRGlyphRunRef;

/**
 * Returns the code units that the run covers.
 */
TR_PUBLIC TRRange TRGlyphRunGetCodeUnitRange(TRGlyphRunRef run);

/**
 * Returns how many code units of the cluster at the start of the run are before the run, and how
 * many of the cluster at its end are after it.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetStartExtraLength(TRGlyphRunRef run);
TR_PUBLIC TRUInteger TRGlyphRunGetEndExtraLength(TRGlyphRunRef run);

/**
 * Returns the bidirectional level of the run.
 */
TR_PUBLIC TRUInt8 TRGlyphRunGetBidiLevel(TRGlyphRunRef run);

/**
 * Returns the direction in which the glyphs of the run are written.
 */
TR_PUBLIC TRWritingDirection TRGlyphRunGetWritingDirection(TRGlyphRunRef run);

/**
 * Returns whether the run was shaped backward, which means that its code units go against the
 * direction in which its glyphs are written.
 */
TR_PUBLIC TRBoolean TRGlyphRunIsBackward(TRGlyphRunRef run);

/**
 * Returns the typeface of the run. It is not retained for the caller.
 */
TR_PUBLIC TRTypefaceRef TRGlyphRunGetTypeface(TRGlyphRunRef run);

/**
 * Returns the type size of the run, and the scales that were applied to its glyphs.
 */
TR_PUBLIC TRFloat TRGlyphRunGetTypeSize(TRGlyphRunRef run);
TR_PUBLIC TRFloat TRGlyphRunGetScaleX(TRGlyphRunRef run);
TR_PUBLIC TRFloat TRGlyphRunGetScaleY(TRGlyphRunRef run);

/**
 * Returns the replacement that the run is for, or `NULL` if its glyphs come from the text. The
 * replacement is not retained for the caller.
 */
TR_PUBLIC TRReplacementRef TRGlyphRunGetReplacement(TRGlyphRunRef run);

/**
 * Returns the foreground color of the run. The result is `TRFalse` if the text does not set one.
 */
TR_PUBLIC TRBoolean TRGlyphRunGetForegroundColor(TRGlyphRunRef run, TRColor *foregroundColor);

/**
 * Returns the user data of the text for the run, or `NULL` if it has none.
 */
TR_PUBLIC const void *TRGlyphRunGetUserData(TRGlyphRunRef run);

/**
 * Returns the metrics of the run: the distances from the baseline to its top and bottom, and the
 * extra space below.
 */
TR_PUBLIC TRFloat TRGlyphRunGetAscent(TRGlyphRunRef run);
TR_PUBLIC TRFloat TRGlyphRunGetDescent(TRGlyphRunRef run);
TR_PUBLIC TRFloat TRGlyphRunGetLeading(TRGlyphRunRef run);

/**
 * Returns the position of the start of the run in its line, whose y is zero.
 */
TR_PUBLIC TRPoint TRGlyphRunGetOrigin(TRGlyphRunRef run);

/**
 * Returns the extent of the run, and its height with the leading.
 */
TR_PUBLIC TRFloat TRGlyphRunGetWidth(TRGlyphRunRef run);
TR_PUBLIC TRFloat TRGlyphRunGetHeight(TRGlyphRunRef run);

/**
 * Returns the number of glyphs.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetGlyphCount(TRGlyphRunRef run);

/**
 * Returns the glyph IDs, the offsets of the glyphs from their pen positions with the y axis
 * pointing up, and the advances of the glyphs. The pointers stay valid as long as the run is
 * alive.
 */
TR_PUBLIC const TRGlyphID *TRGlyphRunGetGlyphIDsPtr(TRGlyphRunRef run);
TR_PUBLIC const TRPoint *TRGlyphRunGetGlyphOffsetsPtr(TRGlyphRunRef run);
TR_PUBLIC const TRFloat *TRGlyphRunGetGlyphAdvancesPtr(TRGlyphRunRef run);

/**
 * Returns the glyph of each code unit, from the start of the first cluster of the run to the end
 * of its last one, and the number of them. The glyphs are relative to the run.
 */
TR_PUBLIC const TRUInteger *TRGlyphRunGetClusterMapPtr(TRGlyphRunRef run);
TR_PUBLIC TRUInteger TRGlyphRunGetClusterMapCount(TRGlyphRunRef run);

/**
 * Returns the start and the end of the cluster that a code unit of the run is in.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetClusterStart(TRGlyphRunRef run, TRUInteger index);
TR_PUBLIC TRUInteger TRGlyphRunGetClusterEnd(TRGlyphRunRef run, TRUInteger index);

/**
 * Returns the first and the last glyph that a code unit of the run has, in the order of the
 * writing.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetLeadingGlyphIndex(TRGlyphRunRef run, TRUInteger index);
TR_PUBLIC TRUInteger TRGlyphRunGetTrailingGlyphIndex(TRGlyphRunRef run, TRUInteger index);

/**
 * Returns the distance from the start of the run to the boundary before a code unit, which can be
 * the end of the run too. The distance grows to the right in left-to-right runs.
 */
TR_PUBLIC TRFloat TRGlyphRunGetDistance(TRGlyphRunRef run, TRUInteger index);

/**
 * Returns the distance from the start of the run to the boundary before a code unit, just like
 * `TRGlyphRunGetDistance`, but the code unit can also belong to the clusters that are split by
 * the start or the end of the run. So, the distance can be negative or exceed the extent of the
 * run. It is meant for drawing the parts of those clusters that are in the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetClusterDistance(TRGlyphRunRef run, TRUInteger index);

/**
 * Returns the code unit boundary that is closest to a distance from the start of the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetIndexOfCodeUnit(TRGlyphRunRef run, TRFloat distance);

/**
 * Returns the box around the glyphs of a range, in the coordinates of the run with the pen at the
 * origin and the y axis pointing down. The renderer is set up for the run: its typeface, type
 * size, scales and direction are changed.
 *
 * @param run
 *      The run.
 * @param glyphRange
 *      The glyphs to measure, which MUST be within the run.
 * @param renderer
 *      A renderer with the glyph cache to use, and with the settings that are not the run's.
 */
TR_PUBLIC TRRect TRGlyphRunGetBoundingBox(TRGlyphRunRef run, TRRange glyphRange,
    TRRendererRef renderer);

/**
 * Retains the run.
 */
TR_PUBLIC TRGlyphRunRef TRGlyphRunRetain(TRGlyphRunRef run);

/**
 * Releases the run.
 */
TR_PUBLIC void TRGlyphRunRelease(TRGlyphRunRef run);

TR_EXTERN_C_END

#endif
