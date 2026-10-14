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

#ifndef _TEHREER_COMPOSED_LINE_H
#define _TEHREER_COMPOSED_LINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphRun.h>
#include <Tehreer/TRRenderer.h>

TR_EXTERN_C_BEGIN

/**
 * A line of text: glyph runs in the order they appear on the screen, which is not the order of the
 * text where it has directions that are mixed. The y axis of all of its positions points downward,
 * and the baseline of the line is at the y of its origin.
 *
 * A line that comes from a frame has its origin in the coordinates of the frame. One that was
 * made by a typesetter has its origin at zero. A line never changes once it is handed out, so it
 * can be used from multiple threads.
 */
typedef const struct _TRComposedLine *TRComposedLineRef;

/**
 * A function that receives the left and right edges of a part of a line.
 *
 * @param userData
 *      The pointer that was passed to the enumeration.
 * @param left
 *      The left edge of the part, measured from the start of the line.
 * @param right
 *      The right edge of the part, measured from the start of the line.
 * @param stop
 *      Set it to `TRTrue` to stop the enumeration after the function returns; it is `TRFalse` when
 *      the function is called.
 */
typedef void (*TREdgeFunc)(void *userData, TRFloat left, TRFloat right, TRBoolean *stop);

/**
 * Returns the index of the first code unit that the line covers.
 *
 * @param line
 *      The line.
 * @return
 *      The index of the first code unit that the line covers.
 */
TR_PUBLIC TRUInteger TRComposedLineGetCodeUnitStart(TRComposedLineRef line);

/**
 * Returns the index after the last code unit that the line covers.
 *
 * @param line
 *      The line.
 * @return
 *      The index after the last code unit that the line covers.
 */
TR_PUBLIC TRUInteger TRComposedLineGetCodeUnitEnd(TRComposedLineRef line);

/**
 * Returns the base level of the paragraph of the line: even for left-to-right and odd for
 * right-to-left.
 *
 * @param line
 *      The line.
 * @return
 *      The base level of the paragraph of the line: even for left-to-right and odd for right-to-
 *      left.
 */
TR_PUBLIC TRUInt8 TRComposedLineGetParagraphLevel(TRComposedLineRef line);

/**
 * Returns the position of the start of the line on its baseline.
 *
 * @param line
 *      The line.
 * @return
 *      The position of the start of the line on its baseline.
 */
TR_PUBLIC TRPoint TRComposedLineGetOrigin(TRComposedLineRef line);

/**
 * Returns the distance from the baseline to the top of the line. It is the greatest ascent of the
 * glyph runs of the line, or the ascent of the replacement for a block line.
 *
 * @param line
 *      The line.
 * @return
 *      The distance from the baseline to the top of the line.
 */
TR_PUBLIC TRFloat TRComposedLineGetAscent(TRComposedLineRef line);

/**
 * Returns the distance from the baseline to the bottom of the line, which is the greatest descent
 * of the glyph runs of the line, or the descent of the replacement for a block line.
 *
 * @param line
 *      The line.
 * @return
 *      The distance from the baseline to the bottom of the line, which is the greatest descent of
 *      the glyph runs of the line, or the descent of the replacement for a block line.
 */
TR_PUBLIC TRFloat TRComposedLineGetDescent(TRComposedLineRef line);

/**
 * Returns the extra space below the descent of the line, which is the greatest leading of the glyph
 * runs of the line, or the leading of the replacement for a block line.
 *
 * @param line
 *      The line.
 * @return
 *      The extra space below the descent of the line, which is the greatest leading of the glyph
 *      runs of the line, or the leading of the replacement for a block line.
 */
TR_PUBLIC TRFloat TRComposedLineGetLeading(TRComposedLineRef line);

/**
 * Returns the extent of the line along its direction, which is the sum of the widths of its glyph
 * runs, including the trailing whitespace.
 *
 * @param line
 *      The line.
 * @return
 *      The extent of the line along its direction, which is the sum of the widths of its glyph
 *      runs, including the trailing whitespace.
 */
TR_PUBLIC TRFloat TRComposedLineGetWidth(TRComposedLineRef line);

/**
 * Returns the height of the line, which is the sum of its ascent, descent and leading.
 *
 * @param line
 *      The line.
 * @return
 *      The height of the line, which is the sum of its ascent, descent and leading.
 */
TR_PUBLIC TRFloat TRComposedLineGetHeight(TRComposedLineRef line);

/**
 * Returns the top edge of the line, which is its ascent above the y of its origin.
 *
 * @param line
 *      The line.
 * @return
 *      The top edge of the line, which is its ascent above the y of its origin.
 */
TR_PUBLIC TRFloat TRComposedLineGetTop(TRComposedLineRef line);

/**
 * Returns the bottom edge of the line, which is its descent and leading below the y of its origin.
 *
 * @param line
 *      The line.
 * @return
 *      The bottom edge of the line, which is its descent and leading below the y of its origin.
 */
TR_PUBLIC TRFloat TRComposedLineGetBottom(TRComposedLineRef line);

/**
 * Returns the left edge of the line, which is the x of its origin.
 *
 * @param line
 *      The line.
 * @return
 *      The left edge of the line, which is the x of its origin.
 */
TR_PUBLIC TRFloat TRComposedLineGetLeft(TRComposedLineRef line);

/**
 * Returns the right edge of the line, which is its width after the x of its origin.
 *
 * @param line
 *      The line.
 * @return
 *      The right edge of the line, which is its width after the x of its origin.
 */
TR_PUBLIC TRFloat TRComposedLineGetRight(TRComposedLineRef line);

/**
 * Returns the extent of the whitespace at the end of the line, which is part of its width.
 *
 * @param line
 *      The line.
 * @return
 *      The extent of the whitespace at the end of the line, which is part of its width.
 */
TR_PUBLIC TRFloat TRComposedLineGetTrailingWhitespaceExtent(TRComposedLineRef line);

/**
 * Returns whether the line holds a block replacement, which is as tall as the replacement and has
 * nothing else.
 *
 * @param line
 *      The line.
 * @return
 *      `TRTrue` if the line holds a block replacement, `TRFalse` otherwise.
 */
TR_PUBLIC TRBoolean TRComposedLineIsBlock(TRComposedLineRef line);

/**
 * Returns whether the line shows a token in place of some of its text. Such a line covers the text
 * that was cut out too, but cannot be made again from its range, as that would show it.
 *
 * @param line
 *      The line.
 * @return
 *      `TRTrue` if the line shows a token in place of some of its text, `TRFalse` otherwise.
 */
TR_PUBLIC TRBoolean TRComposedLineIsTruncated(TRComposedLineRef line);

/**
 * Returns the number of glyph runs in the line.
 *
 * @param line
 *      The line.
 * @return
 *      The number of glyph runs.
 */
TR_PUBLIC TRUInteger TRComposedLineGetGlyphRunCount(TRComposedLineRef line);

/**
 * Returns a glyph run of the line, in the order they appear on the screen. The run is not retained
 * for the caller, and stays valid as long as the line is alive.
 *
 * @param line
 *      The line.
 * @param index
 *      The index of the glyph run, less than `TRComposedLineGetGlyphRunCount()`.
 * @return
 *      The glyph run, or `NULL` if the index is not less than the count.
 */
TR_PUBLIC TRGlyphRunRef TRComposedLineGetGlyphRun(TRComposedLineRef line, TRUInteger index);

/**
 * Gets the distance from the start of the line to the boundary before a code unit, which can be
 * the end of the line too. The start of the line is its left edge, so the distance is measured
 * along the screen, also in a right-to-left run.
 *
 * @param line
 *      The line.
 * @param codeUnitIndex
 *      The index of the code unit, from the start of the line to its end.
 * @param distance
 *      Receives the distance. It MUST NOT be `NULL`, as it is written to whenever the distance is
 *      given.
 * @return
 *      `TRTrue` if the distance was given, `TRFalse` if the code unit is out of the line, in which
 *      case `distance` is left as it was.
 */
TR_PUBLIC TRBoolean TRComposedLineGetCodeUnitDistance(TRComposedLineRef line,
    TRUInteger codeUnitIndex, TRFloat *distance);

/**
 * Passes the parts of the line that a range of code units covers to the function, as pairs of
 * their left and right edges, measured from the start of the line. There is a part for each glyph
 * run that the range touches, in the order of the runs.
 *
 * @param line
 *      The line.
 * @param index
 *      The index of the first code unit of the range.
 * @param length
 *      The number of code units of the range.
 * @param func
 *      The function to call for each part, which can stop the enumeration. It MUST NOT be `NULL`
 *      if the range is within the line and is not empty, as it is called then.
 * @param userData
 *      An opaque pointer that is passed to the function, which can be `NULL`.
 * @return
 *      `TRTrue` if the enumeration is done, or stopped, `TRFalse` if the range is not within the
 *      line, in which case nothing is passed.
 */
TR_PUBLIC TRBoolean TRComposedLineEnumerateEdges(TRComposedLineRef line, TRUInteger index,
    TRUInteger length, TREdgeFunc func, void *userData);

/**
 * Returns the code unit boundary that is closest to a distance from the start of the line. The
 * start of the line is its left edge. A distance before it gives the start of the code units of the
 * line, and one past its end gives the boundary at the end of the last run. A NaN
 * distance gives the start of the code units of the line, as a distance before it does.
 *
 * @param line
 *      The line.
 * @param distance
 *      The distance from the left edge of the line.
 * @return
 *      The index of the code unit before the boundary.
 */
TR_PUBLIC TRUInteger TRComposedLineGetCodeUnitIndex(TRComposedLineRef line, TRFloat distance);

/**
 * Returns how far to move the start of the line so that it is aligned. The extent that is not taken
 * by the line, without its trailing whitespace, is multiplied by the flush factor. For a
 * right-to-left paragraph, the trailing whitespace is subtracted too, since it lies before the
 * text on the screen. The offset is NaN if the flush factor or the extent is NaN.
 *
 * @param line
 *      The line.
 * @param flushFactor
 *      0 to align to the left of the extent and 1 to the right. Anything between them is possible,
 *      such as 0.5 for the center.
 * @param flushExtent
 *      The width of the space in which the line is aligned.
 * @return
 *      The offset to add to the left edge of the line.
 */
TR_PUBLIC TRFloat TRComposedLineGetPenOffset(TRComposedLineRef line, TRFloat flushFactor,
    TRFloat flushExtent);

/**
 * Returns the ink box of the glyphs of the line, relative to its origin. The renderer is set up for
 * each run, and restored, as `TRGlyphRunGetInkBox()` does.
 *
 * @param line
 *      The line.
 * @param renderer
 *      The renderer that decides how the glyphs are measured. It MUST NOT be `NULL`, as every run
 *      that is not a replacement uses it.
 * @return
 *      The union of the ink boxes of the runs that have one, or an empty rectangle at the origin if
 *      none has.
 */
TR_PUBLIC TRRect TRComposedLineGetInkBox(TRComposedLineRef line, TRRendererRef renderer);

/**
 * Draws the runs of the line through the draw callbacks of a renderer, one after another in their
 * visual order, as `TRGlyphRunDraw()` does: the background, the glyphs and then the decorations
 * of a run, before those of the next one. The backgrounds are as tall as the line.
 *
 * @param line
 *      The line.
 * @param renderer
 *      A renderer that has the draw callbacks. Nothing is drawn if it is `NULL`. It is set up for
 *      each run while it is drawn, and gets back what it had.
 * @param origin
 *      The position of the origin of the container of the line, in the user space with the y axis
 *      pointing downward. The origin of the line, and those of its runs, are added to it.
 */
TR_PUBLIC void TRComposedLineDraw(TRComposedLineRef line, TRRendererRef renderer, TRPoint origin);

/**
 * Increments the reference count of a line.
 *
 * @param line
 *      The line whose reference count will be incremented.
 * @return
 *      The same line passed in as the parameter.
 */
TR_PUBLIC TRComposedLineRef TRComposedLineRetain(TRComposedLineRef line);

/**
 * Decrements the reference count of a line. The line will be deallocated when its reference count
 * reaches zero.
 *
 * @param line
 *      The line whose reference count will be decremented.
 */
TR_PUBLIC void TRComposedLineRelease(TRComposedLineRef line);

TR_EXTERN_C_END

#endif
