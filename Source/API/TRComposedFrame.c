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

typedef struct _SelectionContext {
    TRComposedLineRef line;
    TRSelectionFunc func;
    void *userData;
} SelectionContext;

static void FinalizeComposedFrame(ObjectRef object)
{
    TRComposedFrameRef frame = object;
    TRUInteger index;

    for (index = 0; index < frame->lineCount; index++) {
        TRComposedLineRelease(frame->lines[index]);
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

static void AddSelectionPart(void *userData, TRFloat left, TRFloat right)
{
    SelectionContext *context = userData;
    TRComposedLineRef line = context->line;
    TRFloat top = TRComposedLineGetTop(line);
    TRFloat bottom = TRComposedLineGetBottom(line);
    TRRect rect;

    rect = MakeRect(left + line->origin.x, top, right + line->origin.x, bottom);

    context->func(context->userData, rect);
}

static void AddSelectionParts(TRComposedLineRef line, TRUInteger start, TRUInteger end,
    TRSelectionFunc func, void *userData)
{
    SelectionContext context;
    TRRange range;

    context.line = line;
    context.func = func;
    context.userData = userData;

    range.index = start;
    range.length = end - start;

    TRComposedLineEnumerateEdges(line, range, AddSelectionPart, &context);
}

static void AddSelectionAcrossLines(TRComposedFrameRef frame, TRRange range, TRUInteger firstIndex,
    TRUInteger lastIndex, TRSelectionFunc func, void *userData)
{
    const TRFloat frameLeft = 0.0f;
    const TRFloat frameRight = frame->width;
    TRUInteger rangeEnd = range.index + range.length;
    TRComposedLineRef firstLine = frame->lines[firstIndex];
    TRComposedLineRef lastLine = frame->lines[lastIndex];
    TRBoolean isRTL = (lastLine->paragraphLevel & 1) == 1;
    TRUInteger midIndex;
    TRRect rect;

    /* Select each intersecting part of first line. */
    AddSelectionParts(firstLine, range.index, firstLine->codeUnitEnd, func, userData);

    /* Select trailing padding of first line. */
    if (isRTL) {
        rect = MakeRect(frameLeft, TRComposedLineGetTop(firstLine), firstLine->origin.x,
            TRComposedLineGetBottom(firstLine));
    } else {
        rect = MakeRect(firstLine->origin.x + firstLine->extent, TRComposedLineGetTop(firstLine),
            frameRight, TRComposedLineGetBottom(firstLine));
    }
    func(userData, rect);

    /* Select whole part of each mid line. */
    for (midIndex = firstIndex + 1; midIndex < lastIndex; midIndex++) {
        TRComposedLineRef midLine = frame->lines[midIndex];

        rect = MakeRect(frameLeft, TRComposedLineGetTop(midLine), frameRight,
            TRComposedLineGetBottom(midLine));
        func(userData, rect);
    }

    /* Select leading padding of last line. */
    if (isRTL) {
        rect = MakeRect(lastLine->origin.x + lastLine->extent, TRComposedLineGetTop(lastLine),
            frameRight, TRComposedLineGetBottom(lastLine));
    } else {
        rect = MakeRect(frameLeft, TRComposedLineGetTop(lastLine), lastLine->origin.x,
            TRComposedLineGetBottom(lastLine));
    }
    func(userData, rect);

    /* Select each intersecting part of last line. */
    AddSelectionParts(lastLine, lastLine->codeUnitStart, rangeEnd, func, userData);
}

TR_INTERNAL TRComposedFrame *TRComposedFrameCreate(TRUInteger start, TRUInteger end,
    TRComposedLine **lines, TRUInteger lineCount, TRFloat width, TRFloat height)
{
    const TRUInteger size = sizeof(TRComposedFrame);
    TRComposedFrame *frame = NULL;
    TRComposedLineRef *frameLines;
    TRUInteger index;

    /* The frame MUST have at least one line. */
    TRAssert(lineCount > 0);

    frameLines = AllocatorAllocateBlock(lineCount * sizeof(TRComposedLineRef));

    if (frameLines) {
        void *pointer = NULL;

        frame = ObjectCreate(&size, 1, &pointer, FinalizeComposedFrame);

        if (frame) {
            for (index = 0; index < lineCount; index++) {
                frameLines[index] = lines[index];
            }

            frame->lines = frameLines;
            frame->lineCount = lineCount;
            frame->codeUnitStart = start;
            frame->codeUnitEnd = end;
            frame->width = width;
            frame->height = height;
        } else {
            AllocatorDeallocateBlock(frameLines);
        }
    }

    if (!frame) {
        for (index = 0; index < lineCount; index++) {
            TRComposedLineRelease(lines[index]);
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

    return frame->lines[index];
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
        TRComposedLineRef line = frame->lines[mid];

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
        TRComposedLineRef line = frame->lines[index];

        if (position.y >= TRComposedLineGetTop(line)
                && position.y <= TRComposedLineGetBottom(line)) {
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

    /* The lines of the frame MUST cover each code unit of the range. */
    TRAssert(firstIndex != TRInvalidIndex && lastIndex != TRInvalidIndex);

    if (firstIndex == lastIndex) {
        AddSelectionParts(frame->lines[firstIndex], range.index, rangeEnd, func, userData);
    } else {
        AddSelectionAcrossLines(frame, range, firstIndex, lastIndex, func, userData);
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
