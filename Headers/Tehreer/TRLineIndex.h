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


#ifndef _TEHREER_LINE_INDEX_H
#define _TEHREER_LINE_INDEX_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRRenderer.h>

TR_EXTERN_C_BEGIN

/**
 * A spatial index of the lines of a frame, which tells the lines that are within a part of the
 * frame without looking at all of them. A view that shows a long text uses it to find the lines
 * that are visible.
 *
 * The index keeps a box for each line, in the coordinates of the frame with the y axis pointing
 * downward. The box of a line is the union of the rectangle that spans the width of the frame from
 * the top to the bottom of the line, and the ink box of its glyphs, which can be taller than the
 * line. The edges of the box are rounded outward to integers. The boxes of the lines can overlap.
 *
 * The boxes are grouped into bands that are 512 units tall, and a line is kept in every band that
 * its box touches. An index is immutable, and it can be used from any thread. It does not retain
 * the frame it was made from.
 */
typedef struct _TRLineIndex *TRLineIndexRef;

/**
 * The function that gets the lines whose boxes intersect a rectangle.
 *
 * @param userData
 *      The pointer that was passed to the enumeration.
 * @param lineIndex
 *      The index of a line that intersects the rectangle, less than `TRLineIndexGetLineCount()`.
 * @param stop
 *      Set it to `TRTrue` to stop the enumeration after the function returns; it is `TRFalse` when
 *      the function is called.
 */
typedef void (*TRLineIndexFunc)(void *userData, TRUInteger lineIndex, TRBoolean *stop);

/**
 * Creates an index of the lines of a frame. The box of every line is computed here, so the time it
 * takes grows with the number of lines. The ink boxes are measured by the renderer, which is set up
 * for each run and gets back what it had, as `TRComposedLineGetInkBox()` does.
 *
 * @param frame
 *      The frame whose lines are indexed.
 * @param renderer
 *      The renderer that decides how the glyphs are measured.
 * @return
 *      New index that the caller owns, or `NULL` if the frame or the renderer is `NULL`, or on
 *      failure.
 */
TR_PUBLIC TRLineIndexRef TRLineIndexCreate(TRComposedFrameRef frame, TRRendererRef renderer);

/**
 * Returns the number of lines of the frame that the index was made from. The lines are numbered
 * from zero, as `TRComposedFrameGetLine()` does.
 *
 * @param index
 *      The line index.
 * @return
 *      The number of lines, which is never zero.
 */
TR_PUBLIC TRUInteger TRLineIndexGetLineCount(TRLineIndexRef index);

/**
 * Gets the box of a line, in the coordinates of the frame.
 *
 * @param index
 *      The line index.
 * @param lineIndex
 *      The index of the line, less than `TRLineIndexGetLineCount()`.
 * @param box
 *      The rectangle that receives the box of the line, which has integral edges.
 * @return
 *      `TRTrue` if the box is set, `TRFalse` if the line index is not less than the line count or
 *      the box is `NULL`, in which case `box` is left as it was.
 */
TR_PUBLIC TRBoolean TRLineIndexGetLineBox(TRLineIndexRef index, TRUInteger lineIndex,
    TRRect *box);

/**
 * Passes the lines whose boxes intersect a rectangle to the function, without allocating memory.
 * Two rectangles intersect if they overlap by more than their edges, so a box that only touches the
 * rectangle is not passed. Each line is passed once.
 *
 * The lines are found band by band, from the top of the rectangle to its bottom, and each band
 * passes its lines in the order of the frame. A line is passed by the first band that the search
 * reaches. Thus the lines come in the order of the frame, unless the ink of a line reaches into a
 * band above the bands of the lines before it, in which case that line comes first.
 *
 * @param index
 *      The line index.
 * @param rect
 *      The rectangle in the coordinates of the frame. Its width and height MUST NOT be negative.
 *      It can be bigger than the frame, or lie outside of it, in which case no line is passed.
 * @param func
 *      The function to call for each line, which can stop the enumeration.
 * @param userData
 *      An opaque pointer that is passed to the function.
 * @return
 *      `TRTrue` if the enumeration is done, or stopped, `TRFalse` if the function is `NULL`, or the
 *      rectangle has a negative size or is not a number, in which case nothing is passed.
 */
TR_PUBLIC TRBoolean TRLineIndexEnumerateLines(TRLineIndexRef index, TRRect rect,
    TRLineIndexFunc func, void *userData);

/**
 * Returns the index of the first line whose box reaches below a y, which means that the bottom of
 * its box is greater than the y. The lines are looked at band by band, from the band of the y
 * downward, and in the order of the frame within a band. The first one that reaches below the y is
 * returned, which is also the one with the smallest index unless boxes overlap.
 *
 * @param index
 *      The line index.
 * @param y
 *      The vertical position in the coordinates of the frame.
 * @return
 *      The index of the line, or `TRInvalidIndex` if no box reaches below the y, or the y is not a
 *      number.
 */
TR_PUBLIC TRUInteger TRLineIndexGetFirstLineIndexBelow(TRLineIndexRef index, TRFloat y);

/**
 * Increments the reference count of a line index.
 *
 * @param index
 *      The line index whose reference count will be incremented.
 * @return
 *      The same line index passed in as the parameter.
 */
TR_PUBLIC TRLineIndexRef TRLineIndexRetain(TRLineIndexRef index);

/**
 * Decrements the reference count of a line index. The line index will be deallocated when its
 * reference count reaches zero.
 *
 * @param index
 *      The line index whose reference count will be decremented.
 */
TR_PUBLIC void TRLineIndexRelease(TRLineIndexRef index);

TR_EXTERN_C_END

#endif
