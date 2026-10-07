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
#include <Core/Allocator.h>
#include <Core/Object.h>

#include "TRComposedFrame.h"

static void FinalizeComposedFrame(ObjectRef object)
{
    ComposedFrameRef frame = object;
    TRUInteger index;

    for (index = 0; index < frame->lineCount; index++) {
        TRComposedLineRelease((TRComposedLineRef)frame->lines[index]);
    }

    AllocatorDeallocateBlock(frame->lines);
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

typedef struct _SelectionContext {
    ComposedLineRef line;
    TRSelectionFunc func;
    void *userData;
} SelectionContext;

static void AddSelectionPart(void *userData, TRFloat left, TRFloat right)
{
    SelectionContext *context = userData;
    ComposedLineRef line = context->line;

    context->func(context->userData,
        MakeRect(left + line->origin.x, ComposedLineGetTop(line), right + line->origin.x,
            ComposedLineGetBottom(line)));
}

static void AddSelectionParts(ComposedLineRef line, TRUInteger start, TRUInteger end,
    TRSelectionFunc func, void *userData)
{
    SelectionContext context;
    TRRange range;

    context.line = line;
    context.func = func;
    context.userData = userData;

    range.index = start;
    range.length = end - start;

    TRComposedLineEnumerateEdges((TRComposedLineRef)line, range, AddSelectionPart, &context);
}

static void AddSelectionAcrossLines(TRComposedFrameRef frame, TRRange range, TRUInteger firstIndex,
    TRUInteger lastIndex, TRSelectionFunc func, void *userData)
{
    const TRFloat frameLeft = 0.0f;
    const TRFloat frameRight = frame->width;
    TRUInteger rangeEnd = range.index + range.length;
    ComposedLineRef firstLine = frame->lines[firstIndex];
    ComposedLineRef lastLine = frame->lines[lastIndex];
    TRBoolean isRTL = (lastLine->paragraphLevel & 1) == 1;
    TRUInteger midIndex;

    /* Select each intersecting part of first line. */
    AddSelectionParts(firstLine, range.index, firstLine->codeUnitEnd, func, userData);

    /* Select trailing padding of first line. */
    if (isRTL) {
        func(userData, MakeRect(frameLeft, ComposedLineGetTop(firstLine),
            firstLine->origin.x, ComposedLineGetBottom(firstLine)));
    } else {
        func(userData, MakeRect(firstLine->origin.x + firstLine->extent,
            ComposedLineGetTop(firstLine), frameRight, ComposedLineGetBottom(firstLine)));
    }

    /* Select whole part of each mid line. */
    for (midIndex = firstIndex + 1; midIndex < lastIndex; midIndex++) {
        ComposedLineRef midLine = frame->lines[midIndex];

        func(userData, MakeRect(frameLeft, ComposedLineGetTop(midLine), frameRight,
            ComposedLineGetBottom(midLine)));
    }

    /* Select leading padding of last line. */
    if (isRTL) {
        func(userData, MakeRect(lastLine->origin.x + lastLine->extent,
            ComposedLineGetTop(lastLine), frameRight, ComposedLineGetBottom(lastLine)));
    } else {
        func(userData, MakeRect(frameLeft, ComposedLineGetTop(lastLine),
            lastLine->origin.x, ComposedLineGetBottom(lastLine)));
    }

    /* Select each intersecting part of last line. */
    AddSelectionParts(lastLine, lastLine->codeUnitStart, rangeEnd, func, userData);
}

TR_INTERNAL ComposedFrameRef ComposedFrameCreate(TRUInteger start, TRUInteger end,
    ComposedLineRef *lines, TRUInteger lineCount, TRFloat width, TRFloat height)
{
    const TRUInteger size = sizeof(TRComposedFrame);
    void *pointer = NULL;
    ComposedFrameRef frame;

    frame = ObjectCreate(&size, 1, &pointer, FinalizeComposedFrame);

    if (frame) {
        frame->lines = NULL;
        frame->lineCount = 0;

        if (lineCount > 0) {
            frame->lines = AllocatorAllocateBlock(lineCount * sizeof(ComposedLineRef));
        }
    }

    if (frame && (lineCount == 0 || frame->lines)) {
        if (lineCount > 0) {
            memcpy(frame->lines, lines, lineCount * sizeof(ComposedLineRef));
        }
        frame->lineCount = lineCount;
        frame->codeUnitStart = start;
        frame->codeUnitEnd = end;
        frame->width = width;
        frame->height = height;
    } else {
        TRUInteger index;

        for (index = 0; index < lineCount; index++) {
            TRComposedLineRelease((TRComposedLineRef)lines[index]);
        }
        if (frame) {
            ObjectRelease(frame);
            frame = NULL;
        }
    }

    return frame;
}

TRRange TRComposedFrameGetCodeUnitRange(TRComposedFrameRef frame)
{
    TRRange range;

    range.index = frame->codeUnitStart;
    range.length = frame->codeUnitEnd - frame->codeUnitStart;

    return range;
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
    return frame->lineCount;
}

TRComposedLineRef TRComposedFrameGetLine(TRComposedFrameRef frame, TRUInteger index)
{
    /* The index MUST be less than the line count. */
    TRAssert(index < frame->lineCount);

    return (TRComposedLineRef)frame->lines[index];
}

TRUInteger TRComposedFrameGetIndexOfLineForCodeUnit(TRComposedFrameRef frame, TRUInteger index)
{
    TRUInteger lineIndex = TRInvalidIndex;
    TRUInteger low = 0;
    TRUInteger high = frame->lineCount;

    /* The code unit MUST be within the range of the frame, or at its end. */
    TRAssert(index >= frame->codeUnitStart && index <= frame->codeUnitEnd);

    while (low < high) {
        TRUInteger mid = low + ((high - low) >> 1);
        const ComposedLineRef line = frame->lines[mid];

        if (index >= line->codeUnitEnd) {
            low = mid + 1;
        } else if (index < line->codeUnitStart) {
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
    TRAssert(frame->lineCount > 0);

    for (index = 0; index < frame->lineCount; index++) {
        ComposedLineRef line = frame->lines[index];

        if (position.y >= ComposedLineGetTop(line) && position.y <= ComposedLineGetBottom(line)) {
            lineIndex = index;
            break;
        }
    }

    if (lineIndex == TRInvalidIndex) {
        lineIndex = frame->lineCount - 1;
    }

    return lineIndex;
}

void TRComposedFrameEnumerateSelection(TRComposedFrameRef frame, TRRange range,
    TRSelectionFunc func, void *userData)
{
    TRUInteger rangeEnd = range.index + range.length;
    TRUInteger firstIndex;
    TRUInteger lastIndex;

    /* The range MUST NOT be empty, and MUST be within the range of the frame. */
    TRAssert(range.length > 0 && range.index >= frame->codeUnitStart
             && rangeEnd <= frame->codeUnitEnd);

    firstIndex = TRComposedFrameGetIndexOfLineForCodeUnit(frame, range.index);
    lastIndex = TRComposedFrameGetIndexOfLineForCodeUnit(frame, rangeEnd - 1);

    if (firstIndex != TRInvalidIndex && lastIndex != TRInvalidIndex) {
        if (firstIndex == lastIndex) {
            AddSelectionParts(frame->lines[firstIndex], range.index, rangeEnd, func, userData);
        } else {
            AddSelectionAcrossLines(frame, range, firstIndex, lastIndex, func, userData);
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
