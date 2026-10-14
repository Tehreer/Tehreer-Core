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


#ifndef _TEHREER_COMPOSED_FRAME_H
#define _TEHREER_COMPOSED_FRAME_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRRenderer.h>

TR_EXTERN_C_BEGIN

/**
 * A frame of text: lines that are placed one below the other, in a space of a width and a height.
 * It is what a frame resolver makes. The origin of each line is relative to the top left of the
 * frame, and the y axis points downward. A frame is immutable, and it can be used from any thread.
 */
typedef struct _TRComposedFrame *TRComposedFrameRef;

/**
 * The function that gets the rectangles of a selection.
 *
 * @param userData
 *      The pointer that was passed to the enumeration.
 * @param rect
 *      A rectangle of the selection, in the coordinates of the frame.
 * @param stop
 *      Set it to `TRTrue` to stop the enumeration after the function returns; it is `TRFalse` when
 *      the function is called.
 */
typedef void (*TRSelectionFunc)(void *userData, TRRect rect, TRBoolean *stop);

/**
 * Returns the index of the first code unit that the frame covers.
 *
 * @param frame
 *      The frame.
 * @return
 *      The index of the first code unit that the frame covers.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetCodeUnitStart(TRComposedFrameRef frame);

/**
 * Returns the index after the last code unit that the frame covers.
 *
 * @param frame
 *      The frame.
 * @return
 *      The index after the last code unit that the frame covers.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetCodeUnitEnd(TRComposedFrameRef frame);

/**
 * Returns the width of the frame.
 *
 * @param frame
 *      The frame.
 * @return
 *      The width of the frame.
 */
TR_PUBLIC TRFloat TRComposedFrameGetWidth(TRComposedFrameRef frame);

/**
 * Returns the height of the frame.
 *
 * @param frame
 *      The frame.
 * @return
 *      The height of the frame.
 */
TR_PUBLIC TRFloat TRComposedFrameGetHeight(TRComposedFrameRef frame);

/**
 * Returns the number of lines in the frame.
 *
 * @param frame
 *      The frame.
 * @return
 *      The number of lines in the frame.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetLineCount(TRComposedFrameRef frame);

/**
 * Returns a line of the frame, which is not retained for the caller, and stays valid as long as
 * the frame does.
 *
 * @param frame
 *      The frame.
 * @param index
 *      The index of the line, less than `TRComposedFrameGetLineCount()`.
 * @return
 *      The line, or `NULL` if the index is not less than the line count.
 */
TR_PUBLIC TRComposedLineRef TRComposedFrameGetLine(TRComposedFrameRef frame, TRUInteger index);

/**
 * Returns the index of the line that has a code unit, or `TRInvalidIndex` if there is none, as for
 * a code unit that is not within the range of the frame.
 *
 * @param frame
 *      The frame.
 * @param codeUnitIndex
 *      The index of the code unit.
 * @return
 *      The index of the line.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetIndexOfLineForCodeUnit(TRComposedFrameRef frame,
    TRUInteger codeUnitIndex);

/**
 * Returns the index of the line that suits a position best: the line whose top and bottom have its
 * y, or the last line if there is none. A frame always has at least one line. Only the y of the
 * position is considered.
 *
 * @param frame
 *      The frame.
 * @param position
 *      The position in the coordinates of the frame.
 * @return
 *      The index of the line, which is the first one if more than one has the y.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetIndexOfLineAtPosition(TRComposedFrameRef frame,
    TRPoint position);

/**
 * Passes the rectangles that cover a range of code units to the function. A range that spans
 * lines is covered with the paddings of the lines too: the space after the text of the first line
 * up to the edge of the frame, the whole width of each line in between, and the space before the
 * text of the last line. The edges are those of the frame, and the padding of a line follows the
 * direction of its own paragraph. A rectangle is clipped to the horizontal range from zero to the width of the frame, and a
 * rectangle that is empty or has a NaN edge is not passed. The text that a truncated line cuts out
 * has no rectangle of its own.
 *
 * @param frame
 *      The frame.
 * @param index
 *      The index of the first code unit of the range.
 * @param length
 *      The number of code units of the range.
 * @param func
 *      The function to call for each rectangle, which can stop the enumeration. It MUST NOT be
 *      `NULL` if the range is within the frame and is not empty, as it is called then.
 * @param userData
 *      An opaque pointer that is passed to the function, which can be `NULL`.
 * @return
 *      `TRTrue` if the enumeration is done, or stopped, `TRFalse` if the range is not within the
 *      range of the frame, in which case nothing is passed.
 */
TR_PUBLIC TRBoolean TRComposedFrameEnumerateSelection(TRComposedFrameRef frame, TRUInteger index,
    TRUInteger length, TRSelectionFunc func, void *userData);

/**
 * Draws the lines of the frame through the draw callbacks of a renderer, in the order of the text,
 * as `TRComposedLineDraw()` does. Selections and carets are not drawn: a wrapper fills the
 * rectangles of `TRComposedFrameEnumerateSelection()` before or after it draws the frame.
 *
 * @param frame
 *      The frame.
 * @param renderer
 *      A renderer that has the draw callbacks. Nothing is drawn if it is `NULL`. It is set up for
 *      each run while it is drawn, and gets back what it had.
 * @param origin
 *      The position of the top left of the frame, in the user space with the y axis pointing
 *      downward.
 */
TR_PUBLIC void TRComposedFrameDraw(TRComposedFrameRef frame, TRRendererRef renderer,
    TRPoint origin);

/**
 * Increments the reference count of a frame.
 *
 * @param frame
 *      The frame whose reference count will be incremented.
 * @return
 *      The same frame passed in as the parameter.
 */
TR_PUBLIC TRComposedFrameRef TRComposedFrameRetain(TRComposedFrameRef frame);

/**
 * Decrements the reference count of a frame, and destroys it when the count reaches zero.
 *
 * @param frame
 *      The frame whose reference count will be decremented.
 */
TR_PUBLIC void TRComposedFrameRelease(TRComposedFrameRef frame);

TR_EXTERN_C_END

#endif
