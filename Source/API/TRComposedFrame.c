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


#include <stddef.h>
#include <string.h>

#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRGeometry.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <Core/Array.h>
#include <Core/Object.h>

#include "TRComposedFrame.h"

typedef struct _SelectionContext {
    TRComposedLineRef line;
    TRSelectionFunc func;
    void *userData;
    TRBoolean shouldStop;
} SelectionContext;

static void ReleaseLines(Array *lines)
{
    TRUInteger count = ArrayGetCount(lines);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TRComposedLineRelease(*(TRComposedLine **)ArrayGetItem(lines, index));
    }

    ArrayFinalize(lines);
}

static void FinalizeComposedFrame(ObjectRef object)
{
    TRComposedFrame *frame = object;

    ReleaseLines(&frame->lines);
}

static TRUInteger GetFrameLineCount(TRComposedFrameRef frame)
{
    return ArrayGetCount(&frame->lines);
}

static TRComposedLineRef GetFrameLine(TRComposedFrameRef frame, TRUInteger index)
{
    return *(TRComposedLine **)ArrayGetItem(&frame->lines, index);
}

static TRRect MakeRect(TRFloat left, TRFloat top, TRFloat right, TRFloat bottom)
{
    TRRect rect;

    rect.origin.x = left;
    rect.origin.y = top;
    rect.size.width = right - left;
    rect.size.height = bottom - top;

    return rect;
}

/* Passes a rectangle to the function, unless it asked to stop. */
static void AddSelectionRect(SelectionContext *context, TRRect rect)
{
    if (!context->shouldStop) {
        context->func(context->userData, rect, &context->shouldStop);
    }
}

static void AddSelectionPart(void *userData, TRFloat left, TRFloat right, TRBoolean *stop)
{
    SelectionContext *context = userData;
    TRComposedLineRef line = context->line;
    TRFloat top = TRComposedLineGetTop(line);
    TRFloat bottom = TRComposedLineGetBottom(line);
    TRRect rect;

    rect = MakeRect(left + line->origin.x, top, right + line->origin.x, bottom);

    AddSelectionRect(context, rect);

    /* The enumeration of the line stops as the one of the selection does. */
    *stop = context->shouldStop;
}

static void AddSelectionParts(SelectionContext *context, TRComposedLineRef line, TRUInteger start,
    TRUInteger end)
{
    context->line = line;

    TRComposedLineEnumerateEdges(line, start, end - start, AddSelectionPart, context);
}

static void AddSelectionAcrossLines(SelectionContext *context, TRComposedFrameRef frame,
    TRUInteger rangeStart, TRUInteger rangeEnd, TRUInteger firstIndex, TRUInteger lastIndex)
{
    const TRFloat frameLeft = 0.0f;
    const TRFloat frameRight = frame->width;
    TRComposedLineRef firstLine = GetFrameLine(frame, firstIndex);
    TRComposedLineRef lastLine = GetFrameLine(frame, lastIndex);
    TRBoolean isRTL = (lastLine->paragraphLevel & 1) == 1;
    TRUInteger midIndex;
    TRRect rect;

    /* Select each intersecting part of first line. */
    AddSelectionParts(context, firstLine, rangeStart, firstLine->codeUnitEnd);

    /* Select trailing padding of first line. */
    if (isRTL) {
        rect = MakeRect(frameLeft, TRComposedLineGetTop(firstLine), firstLine->origin.x,
            TRComposedLineGetBottom(firstLine));
    } else {
        rect = MakeRect(firstLine->origin.x + firstLine->extent, TRComposedLineGetTop(firstLine),
            frameRight, TRComposedLineGetBottom(firstLine));
    }
    AddSelectionRect(context, rect);

    /* Select whole part of each mid line. */
    for (midIndex = firstIndex + 1; midIndex < lastIndex; midIndex++) {
        TRComposedLineRef midLine = GetFrameLine(frame, midIndex);

        rect = MakeRect(frameLeft, TRComposedLineGetTop(midLine), frameRight,
            TRComposedLineGetBottom(midLine));
        AddSelectionRect(context, rect);
    }

    /* Select leading padding of last line. */
    if (isRTL) {
        rect = MakeRect(lastLine->origin.x + lastLine->extent, TRComposedLineGetTop(lastLine),
            frameRight, TRComposedLineGetBottom(lastLine));
    } else {
        rect = MakeRect(frameLeft, TRComposedLineGetTop(lastLine), lastLine->origin.x,
            TRComposedLineGetBottom(lastLine));
    }
    AddSelectionRect(context, rect);

    /* Select each intersecting part of last line. */
    AddSelectionParts(context, lastLine, lastLine->codeUnitStart, rangeEnd);
}

TR_INTERNAL TRComposedFrame *TRComposedFrameCreate(TRUInteger start, TRUInteger end,
    Array *lines, TRFloat width, TRFloat height)
{
    const TRUInteger size = sizeof(TRComposedFrame);
    void *pointer = NULL;
    TRComposedFrame *frame;

    /* The frame MUST have at least one line. */
    TRAssert(ArrayGetCount(lines) > 0);

    frame = ObjectCreate(&size, 1, &pointer, FinalizeComposedFrame);

    if (frame) {
        frame->lines = *lines;
        frame->codeUnitStart = start;
        frame->codeUnitEnd = end;
        frame->width = width;
        frame->height = height;
    } else {
        ReleaseLines(lines);
    }

    ArrayInitialize(lines, sizeof(TRComposedLine *));

    return frame;
}

TRUInteger TRComposedFrameGetCodeUnitStart(TRComposedFrameRef frame)
{
    return frame->codeUnitStart;
}

TRUInteger TRComposedFrameGetCodeUnitEnd(TRComposedFrameRef frame)
{
    return frame->codeUnitEnd;
}

TRFloat TRComposedFrameGetWidth(TRComposedFrameRef frame)
{
    return frame->width;
}

TRFloat TRComposedFrameGetHeight(TRComposedFrameRef frame)
{
    return frame->height;
}

TRUInteger TRComposedFrameGetLineCount(TRComposedFrameRef frame)
{
    return GetFrameLineCount(frame);
}

TRComposedLineRef TRComposedFrameGetLine(TRComposedFrameRef frame, TRUInteger index)
{
    TRComposedLineRef line = NULL;

    if (index < GetFrameLineCount(frame)) {
        line = GetFrameLine(frame, index);
    }

    return line;
}

TRUInteger TRComposedFrameGetIndexOfLineForCodeUnit(TRComposedFrameRef frame,
    TRUInteger codeUnitIndex)
{
    TRUInteger lineIndex = TRInvalidIndex;
    TRUInteger low = 0;
    TRUInteger high = GetFrameLineCount(frame);

    if (codeUnitIndex < frame->codeUnitStart || codeUnitIndex >= frame->codeUnitEnd) {
        /* The code unit is not within the frame, which has no line for it. */
        low = high;
    }

    while (low < high) {
        TRUInteger mid = low + ((high - low) >> 1);
        TRComposedLineRef line = GetFrameLine(frame, mid);

        if (codeUnitIndex >= line->codeUnitEnd) {
            low = mid + 1;
        } else if (codeUnitIndex < line->codeUnitStart) {
            high = mid;
        } else {
            lineIndex = mid;
            break;
        }
    }

    return lineIndex;
}

TRUInteger TRComposedFrameGetIndexOfLineAtPosition(TRComposedFrameRef frame, TRPoint position)
{
    TRUInteger lineIndex = TRInvalidIndex;
    TRUInteger index;

    /* The frame MUST have at least one line. */
    TRAssert(GetFrameLineCount(frame) > 0);

    for (index = 0; index < GetFrameLineCount(frame); index++) {
        TRComposedLineRef line = GetFrameLine(frame, index);

        if (position.y >= TRComposedLineGetTop(line)
                && position.y <= TRComposedLineGetBottom(line)) {
            lineIndex = index;
            break;
        }
    }

    if (lineIndex == TRInvalidIndex) {
        lineIndex = GetFrameLineCount(frame) - 1;
    }

    return lineIndex;
}

TRBoolean TRComposedFrameEnumerateSelection(TRComposedFrameRef frame, TRUInteger index,
    TRUInteger length, TRSelectionFunc func, void *userData)
{
    TRUInteger frameLength = frame->codeUnitEnd - frame->codeUnitStart;
    TRBoolean isValid = (index >= frame->codeUnitStart
                         && RangeIsValid(index - frame->codeUnitStart, length, frameLength));

    if (isValid && length > 0) {
        TRUInteger rangeEnd = index + length;
        TRUInteger firstIndex = TRComposedFrameGetIndexOfLineForCodeUnit(frame, index);
        TRUInteger lastIndex = TRComposedFrameGetIndexOfLineForCodeUnit(frame, rangeEnd - 1);
        SelectionContext context;

        /* The lines of the frame MUST cover each code unit of the range. */
        TRAssert(firstIndex != TRInvalidIndex && lastIndex != TRInvalidIndex);

        context.line = NULL;
        context.func = func;
        context.userData = userData;
        context.shouldStop = TRFalse;

        if (firstIndex == lastIndex) {
            AddSelectionParts(&context, GetFrameLine(frame, firstIndex), index, rangeEnd);
        } else {
            AddSelectionAcrossLines(&context, frame, index, rangeEnd, firstIndex, lastIndex);
        }
    }

    return isValid;
}

void TRComposedFrameDraw(TRComposedFrameRef frame, TRRendererRef renderer, TRPoint origin)
{
    if (renderer) {
        TRUInteger lineCount = GetFrameLineCount(frame);
        TRUInteger lineIndex;

        for (lineIndex = 0; lineIndex < lineCount; lineIndex++) {
            TRComposedLineDraw(GetFrameLine(frame, lineIndex), renderer, origin);
        }
    }
}

TRComposedFrameRef TRComposedFrameRetain(TRComposedFrameRef frame)
{
    return ObjectRetain((ObjectRef)frame);
}

void TRComposedFrameRelease(TRComposedFrameRef frame)
{
    ObjectRelease((ObjectRef)frame);
}
