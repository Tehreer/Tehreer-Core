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

TR_EXTERN_C_BEGIN

/**
 * A frame of text: lines that are placed one below the other, in a space of a width and a height.
 * It is what a frame resolver makes. The origin of each line is relative to the top left of the
 * frame, and the y axis points downward. A frame is immutable, and it can be used from any thread.
 */
typedef struct _TRComposedFrame *TRComposedFrameRef;

/**
 * The function that gets the rectangles of a selection.
 */
typedef void (*TRSelectionFunc)(void *userData, TRRect rect);

/**
 * Returns the code units that the frame covers.
 */
TR_PUBLIC TRRange TRComposedFrameGetCodeUnitRange(TRComposedFrameRef frame);

/**
 * Returns the width of the frame.
 */
TR_PUBLIC TRFloat TRComposedFrameGetWidth(TRComposedFrameRef frame);

/**
 * Returns the height of the frame.
 */
TR_PUBLIC TRFloat TRComposedFrameGetHeight(TRComposedFrameRef frame);

/**
 * Returns the number of lines in the frame.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetLineCount(TRComposedFrameRef frame);

/**
 * Returns a line of the frame, which stays valid as long as the frame does. The index MUST be less
 * than the line count.
 */
TR_PUBLIC TRComposedLineRef TRComposedFrameGetLine(TRComposedFrameRef frame, TRUInteger index);

/**
 * Returns the index of the line that has a code unit, or `TRInvalidIndex` if there is none. The
 * code unit MUST be within the range of the frame, or at its end, which has no line.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetIndexOfLineForCodeUnit(TRComposedFrameRef frame,
    TRUInteger index);

/**
 * Returns the index of the line that suits a position best: the line whose top and bottom have its
 * y, or the last line if there is none. The frame MUST have at least one line.
 */
TR_PUBLIC TRUInteger TRComposedFrameGetIndexOfLineAtPosition(TRComposedFrameRef frame,
    TRPoint position);

/**
 * Passes the rectangles that cover a range of code units to the function. A range that spans
 * lines is covered with the paddings of the lines too. The range MUST NOT be empty, and MUST be
 * within the range of the frame.
 */
TR_PUBLIC void TRComposedFrameEnumerateSelection(TRComposedFrameRef frame, TRRange range,
    TRSelectionFunc func, void *userData);

/**
 * Increments the reference count of a frame.
 */
TR_PUBLIC TRComposedFrameRef TRComposedFrameRetain(TRComposedFrameRef frame);

/**
 * Decrements the reference count of a frame, and destroys it when the count reaches zero.
 */
TR_PUBLIC void TRComposedFrameRelease(TRComposedFrameRef frame);

TR_EXTERN_C_END

#endif
