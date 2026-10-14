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
 * Returns the index of the first code unit that the run covers.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The index of the first code unit that the run covers.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetCodeUnitStart(TRGlyphRunRef run);

/**
 * Returns the index after the last code unit that the run covers.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The index after the last code unit that the run covers.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetCodeUnitEnd(TRGlyphRunRef run);

/**
 * Returns how many code units of the cluster at the start of the run are before the run. It is
 * zero unless the run starts in the middle of a cluster.
 *
 * @param run
 *      The glyph run.
 * @return
 *      How many code units of the cluster at the start of the run are before the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetStartExtraLength(TRGlyphRunRef run);

/**
 * Returns how many code units of the cluster at the end of the run are after the run. It is zero
 * unless the run ends in the middle of a cluster.
 *
 * @param run
 *      The glyph run.
 * @return
 *      How many code units of the cluster at the end of the run are after the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetEndExtraLength(TRGlyphRunRef run);

/**
 * Returns the bidirectional level of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The bidirectional level of the run.
 */
TR_PUBLIC TRUInt8 TRGlyphRunGetBidiLevel(TRGlyphRunRef run);

/**
 * Returns the direction in which the glyphs of the run are written.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The direction in which the glyphs of the run are written.
 */
TR_PUBLIC TRWritingDirection TRGlyphRunGetWritingDirection(TRGlyphRunRef run);

/**
 * Returns whether the run was shaped backward, which means that its code units go against the
 * direction in which its glyphs are written.
 *
 * @param run
 *      The glyph run.
 * @return
 *      `TRTrue` if the run was shaped backward, `TRFalse` otherwise.
 */
TR_PUBLIC TRBoolean TRGlyphRunIsBackward(TRGlyphRunRef run);

/**
 * Returns the typeface of the run. It is not retained for the caller.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The typeface, which is not retained for the caller.
 */
TR_PUBLIC TRTypefaceRef TRGlyphRunGetTypeface(TRGlyphRunRef run);

/**
 * Returns the type size of the run, which is the size of its em square.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The type size of the run, which is the size of its em square.
 */
TR_PUBLIC TRFloat TRGlyphRunGetTypeSize(TRGlyphRunRef run);

/**
 * Returns the horizontal scale that was applied to the glyphs of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The horizontal scale that was applied to the glyphs of the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetScaleX(TRGlyphRunRef run);

/**
 * Returns the vertical scale that was applied to the glyphs of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The vertical scale that was applied to the glyphs of the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetScaleY(TRGlyphRunRef run);

/**
 * Returns the replacement that the run is for, or `NULL` if its glyphs come from the text. The
 * replacement is not retained for the caller.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The replacement, or `NULL` if the glyphs come from the text.
 */
TR_PUBLIC TRReplacementRef TRGlyphRunGetReplacement(TRGlyphRunRef run);

/**
 * Gets the foreground color of the run.
 *
 * @param run
 *      The run.
 * @param foregroundColor
 *      Receives the color, if the text sets one.
 * @return
 *      `TRTrue` if the color was set, `TRFalse` if the text does not set one, in which case
 *      `foregroundColor` is left as it was.
 */
TR_PUBLIC TRBoolean TRGlyphRunGetForegroundColor(TRGlyphRunRef run, TRColor *foregroundColor);

/**
 * Gets the background color of the run.
 *
 * @param run
 *      The run.
 * @param backgroundColor
 *      Receives the color, if the text sets one.
 * @return
 *      `TRTrue` if the color was set, `TRFalse` if the text does not set one.
 */
TR_PUBLIC TRBoolean TRGlyphRunGetBackgroundColor(TRGlyphRunRef run, TRColor *backgroundColor);

/**
 * Returns whether the run is underlined.
 *
 * @param run
 *      The run.
 * @return
 *      `TRTrue` if the text underlines the run, `TRFalse` otherwise.
 */
TR_PUBLIC TRBoolean TRGlyphRunHasUnderline(TRGlyphRunRef run);

/**
 * Returns whether the run is struck through.
 *
 * @param run
 *      The run.
 * @return
 *      `TRTrue` if the text strikes the run through, `TRFalse` otherwise.
 */
TR_PUBLIC TRBoolean TRGlyphRunHasStrikethrough(TRGlyphRunRef run);

/**
 * Gets the color of the underline and the strikethrough of the run.
 *
 * @param run
 *      The run.
 * @param decorationColor
 *      Receives the color, if the text sets one.
 * @return
 *      `TRTrue` if the color was set, `TRFalse` if the text does not set one, in which case the
 *      decorations are drawn with the foreground color.
 */
TR_PUBLIC TRBoolean TRGlyphRunGetDecorationColor(TRGlyphRunRef run, TRColor *decorationColor);

/**
 * Returns the user data of the text for the run, or `NULL` if it has none.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The user data, or `NULL` if the text has none.
 */
TR_PUBLIC const void *TRGlyphRunGetUserData(TRGlyphRunRef run);

/**
 * Returns the distance from the baseline to the top of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The distance from the baseline to the top of the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetAscent(TRGlyphRunRef run);

/**
 * Returns the distance from the baseline to the bottom of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The distance from the baseline to the bottom of the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetDescent(TRGlyphRunRef run);

/**
 * Returns the extra space below the descent of the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The extra space below the descent of the run.
 */
TR_PUBLIC TRFloat TRGlyphRunGetLeading(TRGlyphRunRef run);

/**
 * Returns the position of the start of the run in its line, whose y is zero. It is set when the
 * run is put in a line, and the x of it is the sum of the widths of the runs before it.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The position of the start of the run in its line, whose y is zero.
 */
TR_PUBLIC TRPoint TRGlyphRunGetOrigin(TRGlyphRunRef run);

/**
 * Returns the extent of the run along its direction, which is the distance between the boundaries
 * at the start and at the end of its code units.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The extent of the run along its direction, which is the distance between the boundaries at
 *      the start and at the end of its code units.
 */
TR_PUBLIC TRFloat TRGlyphRunGetWidth(TRGlyphRunRef run);

/**
 * Returns the height of the run, which is the sum of its ascent, descent and leading.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The height of the run, which is the sum of its ascent, descent and leading.
 */
TR_PUBLIC TRFloat TRGlyphRunGetHeight(TRGlyphRunRef run);

/**
 * Returns the number of glyphs in the run.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The number of glyphs in the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetGlyphCount(TRGlyphRunRef run);

/**
 * Returns the glyph IDs of the run, one for each glyph. The pointer stays valid as long as the run
 * is alive.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The glyph IDs of the run, one for each glyph.
 */
TR_PUBLIC const TRGlyphID *TRGlyphRunGetGlyphIDsPtr(TRGlyphRunRef run);

/**
 * Returns the offsets of the glyphs of the run from their pen positions, with the y axis pointing
 * up as in the font, one for each glyph. They include the scales and the baseline offset of the
 * text. The pointer stays valid as long as the run is alive.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The offsets of the glyphs of the run from their pen positions, with the y axis pointing up
 *      as in the font, one for each glyph.
 */
TR_PUBLIC const TRPoint *TRGlyphRunGetGlyphOffsetsPtr(TRGlyphRunRef run);

/**
 * Returns the advances of the glyphs of the run, one for each glyph. A justified run has advances
 * of its own, which include the stretch of the justification. The pointer stays valid as long as
 * the run is alive.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The advances of the glyphs of the run, one for each glyph.
 */
TR_PUBLIC const TRFloat *TRGlyphRunGetGlyphAdvancesPtr(TRGlyphRunRef run);

/**
 * Returns the glyph of each code unit, from the start of the first cluster of the run to the end
 * of its last one, which are the code units counted by `TRGlyphRunGetClusterMapCount()`. The
 * glyphs are relative to the run. The pointer stays valid as long as the run is alive.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The glyph of each code unit, from the start of the first cluster of the run to the end of
 *      its last one, which are the code units counted by `TRGlyphRunGetClusterMapCount()`.
 */
TR_PUBLIC const TRUInteger *TRGlyphRunGetClusterMapPtr(TRGlyphRunRef run);

/**
 * Returns the number of code units in the cluster map, which are those of the run and the extra
 * code units of its first and last clusters.
 *
 * @param run
 *      The glyph run.
 * @return
 *      The number of code units in the cluster map.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetClusterMapCount(TRGlyphRunRef run);

/**
 * Returns the start of the cluster that a code unit of the run is in.
 *
 * @param run
 *      The run.
 * @param codeUnitIndex
 *      The index of a code unit of the run, which has to be within the range that it covers.
 * @return
 *      The index of the first code unit of the cluster, which can be before the run if the cluster
 *      is split by it. It is `TRInvalidIndex` if the code unit is not within the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetClusterStart(TRGlyphRunRef run, TRUInteger codeUnitIndex);

/**
 * Returns the end of the cluster that a code unit of the run is in.
 *
 * @param run
 *      The run.
 * @param codeUnitIndex
 *      The index of a code unit of the run, which has to be within the range that it covers.
 * @return
 *      The index after the last code unit of the cluster, which can be after the run if the
 *      cluster is split by it. It is `TRInvalidIndex` if the code unit is not within the run.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetClusterEnd(TRGlyphRunRef run, TRUInteger codeUnitIndex);

/**
 * Gets the distance from the start of the run to the boundary before a code unit, which can be the
 * end of the run too. The distance grows to the right in left-to-right runs.
 *
 * The code unit can also be one of the clusters that are split by the start or the end of the run,
 * counted by the extra lengths. The distance of such a code unit can be negative, or exceed the
 * width of the run, as it is meant for drawing the parts of those clusters that are in the run.
 *
 * @param run
 *      The run.
 * @param codeUnitIndex
 *      The index of the code unit, from the start of the run less its start extra length to the end
 *      of the run plus its end extra length.
 * @param distance
 *      Receives the distance. It MUST NOT be `NULL`, as it is written to whenever the distance is
 *      given.
 * @return
 *      `TRTrue` if the distance was given, `TRFalse` if the code unit is out of the run and its
 *      clusters.
 */
TR_PUBLIC TRBoolean TRGlyphRunGetCodeUnitDistance(TRGlyphRunRef run, TRUInteger codeUnitIndex,
    TRFloat *distance);

/**
 * Returns the code unit boundary that is closest to a distance from the start of the run, as for
 * placing a caret where a point is hit.
 *
 * @param run
 *      The run.
 * @param distance
 *      The distance from the start of the run, which grows to the right in left-to-right runs.
 * @return
 *      The index of the boundary, which is the index of the code unit that follows it. It is from
 *      the start of the run to its end, which is the boundary after the last code unit.
 */
TR_PUBLIC TRUInteger TRGlyphRunGetCodeUnitIndex(TRGlyphRunRef run, TRFloat distance);

/**
 * Returns the ink box of the glyphs of the run: the smallest box that holds what is drawn, which is
 * not the box of the advances. It is in the coordinates of the run with the pen at the origin and
 * the y axis pointing down.
 *
 * The renderer is set up for the run while it is measured, which is its typeface, type size,
 * scales and direction, and it gets back what it had before.
 *
 * @param run
 *      The run.
 * @param renderer
 *      A renderer with the glyph cache to use, and with the settings that are not the run's.
 *      It MUST NOT be `NULL`, unless the run is a replacement, which does not use it.
 * @return
 *      The ink box. For a replacement, which is not drawn from glyphs, it is the box of its width
 *      and height at the origin.
 */
TR_PUBLIC TRRect TRGlyphRunGetInkBox(TRGlyphRunRef run, TRRendererRef renderer);

/**
 * Draws the run through the draw callbacks of a renderer, which receive absolute positions in
 * pixels: the background of the run, then its glyphs or its replacement, then its underline and
 * strikethrough. See `TRDrawCallbacks` for what each callback is given.
 *
 * The glyphs are drawn in the foreground color of the run, or in that of the renderer if the run
 * has none, and the draw style of the renderer decides if they are filled, stroked with its stroke
 * color, or both. The glyphs of a cluster that is split by the start or the end of the run are
 * given with a clip, so that the run draws only its part of the cluster. The backgrounds and the
 * decorations are as wide as the run, and the backgrounds are as tall as its metrics. The
 * underline and strikethrough are drawn with the decoration color, or else the foreground color,
 * and take their position and thickness from the typeface; they are not drawn for a replacement.
 *
 * The renderer is set up for the run while it draws, with its typeface, type size, scales,
 * direction and foreground color, and it gets back what it had before. Its render scale, draw
 * style, stroke settings and callbacks are not changed.
 *
 * @param run
 *      The run.
 * @param renderer
 *      A renderer that has the draw callbacks. Nothing is drawn if it is `NULL`.
 * @param origin
 *      The position of the start of the run on its baseline, in the user space with the y axis
 *      pointing downward. The origin of the run in its line is not added.
 */
TR_PUBLIC void TRGlyphRunDraw(TRGlyphRunRef run, TRRendererRef renderer, TRPoint origin);

/**
 * Increments the reference count of a glyph run.
 *
 * @param run
 *      The run whose reference count will be incremented.
 * @return
 *      The same run passed in as the parameter.
 */
TR_PUBLIC TRGlyphRunRef TRGlyphRunRetain(TRGlyphRunRef run);

/**
 * Decrements the reference count of a glyph run. The run will be deallocated when its reference
 * count reaches zero.
 *
 * @param run
 *      The run whose reference count will be decremented.
 */
TR_PUBLIC void TRGlyphRunRelease(TRGlyphRunRef run);

TR_EXTERN_C_END

#endif
