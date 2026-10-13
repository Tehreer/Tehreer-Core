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

/* The extent of a cluster of a run, as distances from the left edge of the run. */
typedef struct _ClusterBox {
    TRUInteger start;
    TRUInteger end;
    TRFloat left;
    TRFloat right;
} ClusterBox;

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

/* ---------- Hit Testing ---------- */

/* Returns how far a distance is from the interval, or zero if it is within the interval. */
static TRFloat GetGapToInterval(TRFloat distance, TRFloat left, TRFloat right)
{
    TRFloat gap = 0.0f;

    if (distance < left) {
        gap = left - distance;
    } else if (distance > right) {
        gap = distance - right;
    }

    return gap;
}

/* Returns the cluster of the run that is nearest to a distance from the left edge of the run. */
static ClusterBox GetNearestCluster(TRGlyphRunRef glyphRun, TRFloat distance)
{
    TRUInteger runEnd = TRGlyphRunGetCodeUnitEnd(glyphRun);
    TRUInteger start = TRGlyphRunGetCodeUnitStart(glyphRun);
    TRBoolean hasNearest = TRFalse;
    TRFloat nearestGap = 0.0f;
    ClusterBox nearest;

    /* The run MUST have at least one code unit. */
    TRAssert(start < runEnd);

    nearest.start = start;
    nearest.end = start;
    nearest.left = 0.0f;
    nearest.right = 0.0f;

    while (start < runEnd) {
        TRUInteger clusterEnd = TRGlyphRunGetClusterEnd(glyphRun, start);
        TRUInteger end = NumberMin(clusterEnd, runEnd);
        TRFloat startEdge = TRGlyphRunGetCaretEdge(glyphRun, start);
        TRFloat endEdge = TRGlyphRunGetCaretEdge(glyphRun, end);
        TRFloat left = NumberMin(startEdge, endEdge);
        TRFloat right = NumberMax(startEdge, endEdge);
        TRFloat gap = GetGapToInterval(distance, left, right);

        if (!hasNearest || gap < nearestGap) {
            hasNearest = TRTrue;
            nearestGap = gap;
            nearest.start = start;
            nearest.end = end;
            nearest.left = left;
            nearest.right = right;
        }

        start = end;
    }

    return nearest;
}

/*
 * Returns the index of the line that a position is in, the first line for a position above it, or
 * the last one for a position below it.
 */
static TRUInteger GetHitLineIndex(TRComposedFrameRef frame, TRPoint position)
{
    TRComposedLineRef firstLine = GetFrameLine(frame, 0);
    TRUInteger lineIndex = 0;

    if (position.y >= TRComposedLineGetTop(firstLine)) {
        lineIndex = TRComposedFrameGetIndexOfLineAtPosition(frame, position);
    }

    return lineIndex;
}

/* Returns the index of the run that covers a distance from the left edge of the line. */
static TRUInteger GetRunIndexAtDistance(TRComposedLineRef line, TRFloat distance)
{
    TRUInteger runIndex = 0;
    TRUInteger index;

    for (index = line->runCount; index > 0; index--) {
        if (line->runs[index - 1]->origin.x <= distance) {
            runIndex = index - 1;
            break;
        }
    }

    return runIndex;
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

TRUInteger TRComposedFrameGetCodeUnitIndexAtPosition(TRComposedFrameRef frame, TRPoint position)
{
    TRHitTestResult hit = TRComposedFrameHitTest(frame, position);

    return hit.codeUnitIndex;
}

TRHitTestResult TRComposedFrameHitTest(TRComposedFrameRef frame, TRPoint position)
{
    TRUInteger lineIndex = GetHitLineIndex(frame, position);
    TRComposedLineRef line = GetFrameLine(frame, lineIndex);
    TRFloat lineTop = TRComposedLineGetTop(line);
    TRFloat lineBottom = TRComposedLineGetBottom(line);
    TRFloat lineDistance = position.x - line->origin.x;
    TRUInteger runIndex = GetRunIndexAtDistance(line, lineDistance);
    TRGlyphRunRef glyphRun = line->runs[runIndex];
    TRFloat runDistance = lineDistance - glyphRun->origin.x;
    ClusterBox cluster = GetNearestCluster(glyphRun, runDistance);
    TRFloat middle = (cluster.left + cluster.right) / 2.0f;
    TRBoolean isRTL = TRGlyphRunIsRTL(glyphRun);
    TRBoolean isLeftHalf = (runDistance <= middle);
    TRBoolean isLeading = (isLeftHalf != isRTL);
    TRBoolean isWithinLine = (position.y >= lineTop && position.y <= lineBottom);
    TRBoolean isWithinCluster = (runDistance >= cluster.left && runDistance <= cluster.right);
    TRHitTestResult hit;

    hit.clusterIndex = cluster.start;
    hit.clusterLength = cluster.end - cluster.start;
    hit.side = (isLeading ? TRHitSideLeading : TRHitSideTrailing);
    hit.codeUnitIndex = (isLeading ? cluster.start : cluster.end);
    hit.isInside = (isWithinLine && isWithinCluster);

    return hit;
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
