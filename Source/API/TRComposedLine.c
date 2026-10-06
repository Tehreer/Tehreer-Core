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

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRRenderer.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRGlyphRun.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>

#include "TRComposedLine.h"

static void FinalizeComposedLine(ObjectRef object)
{
    ComposedLineRef line = object;
    TRUInteger index;

    for (index = 0; index < line->runCount; index++) {
        TRGlyphRunRelease(line->runs[index]);
    }

    AllocatorDeallocateBlock(line->runs);
}

TR_INTERNAL ComposedLineRef ComposedLineCreate(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end, GlyphRunRef *runs, TRUInteger runCount, TRUInt8 paragraphLevel)
{
    const TRUInteger size = sizeof(TRComposedLine);
    void *pointer = NULL;
    ComposedLineRef line;
    GlyphRunRef blockRun = NULL;
    TRUInteger trailingWhitespaceStart;
    TRUInteger index;

    line = ObjectCreate(&size, 1, &pointer, FinalizeComposedLine);

    if (line) {
        line->runs = NULL;
        line->runCount = 0;

        if (runCount > 0) {
            line->runs = AllocatorAllocateBlock(runCount * sizeof(GlyphRunRef));
        }
    }

    if (!line || (runCount > 0 && !line->runs)) {
        for (index = 0; index < runCount; index++) {
            TRGlyphRunRelease(runs[index]);
        }
        if (line) {
            ObjectRelease(line);
        }

        return NULL;
    }

    memcpy(line->runs, runs, runCount * sizeof(GlyphRunRef));
    line->runCount = runCount;

    line->codeUnitStart = start;
    line->codeUnitEnd = end;
    line->paragraphLevel = paragraphLevel;
    line->origin.x = 0.0f;
    line->origin.y = 0.0f;
    line->ascent = 0.0f;
    line->descent = 0.0f;
    line->leading = 0.0f;
    line->extent = 0.0f;
    line->trailingWhitespaceExtent = 0.0f;
    line->isBlock = TRFalse;
    line->flushFactor = 0.0f;
    line->intrinsicMargin = 0.0f;

    trailingWhitespaceStart = TextBufferGetTrailingWhitespaceStart(buffer, start, end);

    for (index = 0; index < runCount; index++) {
        GlyphRunRef glyphRun = line->runs[index];
        TRUInteger wsStart = (glyphRun->codeUnitStart > trailingWhitespaceStart
                              ? glyphRun->codeUnitStart : trailingWhitespaceStart);
        TRUInteger wsEnd = (glyphRun->codeUnitEnd < end ? glyphRun->codeUnitEnd : end);

        glyphRun->origin.x = line->extent;

        if (wsStart < wsEnd) {
            line->trailingWhitespaceExtent += GlyphRunGetDistanceInRange(glyphRun, wsStart, wsEnd);
        }

        if (TRGlyphRunGetAscent(glyphRun) > line->ascent) {
            line->ascent = TRGlyphRunGetAscent(glyphRun);
        }
        if (TRGlyphRunGetDescent(glyphRun) > line->descent) {
            line->descent = TRGlyphRunGetDescent(glyphRun);
        }
        if (TRGlyphRunGetLeading(glyphRun) > line->leading) {
            line->leading = TRGlyphRunGetLeading(glyphRun);
        }

        line->extent += TRGlyphRunGetWidth(glyphRun);

        if (TextRunIsBlock(glyphRun->textRun)) {
            blockRun = glyphRun;
        }
    }

    /*
     * A line that holds a block replacement is as tall as the replacement and its margins, and
     * nothing else: the metrics of the newline that ends its paragraph would only add blank space.
     */
    if (blockRun) {
        line->ascent = TRGlyphRunGetAscent(blockRun);
        line->descent = TRGlyphRunGetDescent(blockRun);
        line->leading = TRGlyphRunGetLeading(blockRun);
        line->isBlock = TRTrue;
    }

    return line;
}

TR_INTERNAL TRFloat ComposedLineGetTop(ComposedLineRef line)
{
    return line->origin.y - line->ascent;
}

TR_INTERNAL TRFloat ComposedLineGetBottom(ComposedLineRef line)
{
    return line->origin.y + line->descent + line->leading;
}

TRRange TRComposedLineGetCodeUnitRange(TRComposedLineRef line)
{
    TRRange range;

    range.index = line->codeUnitStart;
    range.length = line->codeUnitEnd - line->codeUnitStart;

    return range;
}

TRUInt8 TRComposedLineGetParagraphLevel(TRComposedLineRef line)
{
    return line->paragraphLevel;
}

TRPoint TRComposedLineGetOrigin(TRComposedLineRef line)
{
    return line->origin;
}

TRFloat TRComposedLineGetAscent(TRComposedLineRef line)
{
    return line->ascent;
}

TRFloat TRComposedLineGetDescent(TRComposedLineRef line)
{
    return line->descent;
}

TRFloat TRComposedLineGetLeading(TRComposedLineRef line)
{
    return line->leading;
}

TRFloat TRComposedLineGetWidth(TRComposedLineRef line)
{
    return line->extent;
}

TRFloat TRComposedLineGetHeight(TRComposedLineRef line)
{
    return line->ascent + line->descent + line->leading;
}

TRFloat TRComposedLineGetTop(TRComposedLineRef line)
{
    return ComposedLineGetTop((ComposedLineRef)line);
}

TRFloat TRComposedLineGetBottom(TRComposedLineRef line)
{
    return ComposedLineGetBottom((ComposedLineRef)line);
}

TRFloat TRComposedLineGetLeft(TRComposedLineRef line)
{
    return line->origin.x;
}

TRFloat TRComposedLineGetRight(TRComposedLineRef line)
{
    return line->origin.x + line->extent;
}

TRFloat TRComposedLineGetTrailingWhitespaceExtent(TRComposedLineRef line)
{
    return line->trailingWhitespaceExtent;
}

TRBoolean TRComposedLineIsBlock(TRComposedLineRef line)
{
    return line->isBlock;
}

TRUInteger TRComposedLineGetGlyphRunCount(TRComposedLineRef line)
{
    return line->runCount;
}

TRGlyphRunRef TRComposedLineGetGlyphRun(TRComposedLineRef line, TRUInteger index)
{
    /* The index MUST be less than the run count. */
    TRAssert(index < line->runCount);

    return line->runs[index];
}

TRFloat TRComposedLineGetDistance(TRComposedLineRef line, TRUInteger index)
{
    TRFloat distance = 0.0f;
    TRUInteger runIndex;

    /* The index MUST be within the line, or its end. */
    TRAssert(index >= line->codeUnitStart && index <= line->codeUnitEnd);

    for (runIndex = 0; runIndex < line->runCount; runIndex++) {
        GlyphRunRef glyphRun = line->runs[runIndex];

        if (index >= glyphRun->codeUnitStart && index < glyphRun->codeUnitEnd) {
            distance += TRGlyphRunGetDistance(glyphRun, index);
            break;
        }

        distance += TRGlyphRunGetWidth(glyphRun);
    }

    return distance;
}

void TRComposedLineEnumerateEdges(TRComposedLineRef line, TRRange range, TREdgeFunc func,
    void *userData)
{
    TRUInteger visualStart = (range.index > line->codeUnitStart ? range.index : line->codeUnitStart);
    TRUInteger rangeEnd = range.index + range.length;
    TRUInteger visualEnd = (rangeEnd < line->codeUnitEnd ? rangeEnd : line->codeUnitEnd);
    TRUInteger runIndex;

    if (visualStart >= visualEnd) {
        return;
    }

    for (runIndex = 0; runIndex < line->runCount; runIndex++) {
        GlyphRunRef glyphRun = line->runs[runIndex];

        if (glyphRun->codeUnitStart < visualEnd && glyphRun->codeUnitEnd > visualStart) {
            TRUInteger selectionStart = (visualStart > glyphRun->codeUnitStart
                                         ? visualStart : glyphRun->codeUnitStart);
            TRUInteger selectionEnd = (visualEnd < glyphRun->codeUnitEnd
                                       ? visualEnd : glyphRun->codeUnitEnd);
            TRFloat leadingEdge = TRGlyphRunGetDistance(glyphRun, selectionStart);
            TRFloat trailingEdge = TRGlyphRunGetDistance(glyphRun, selectionEnd);
            TRFloat relativeLeft = glyphRun->origin.x;
            TRFloat left = (leadingEdge < trailingEdge ? leadingEdge : trailingEdge) + relativeLeft;
            TRFloat right = (leadingEdge > trailingEdge ? leadingEdge : trailingEdge) + relativeLeft;

            func(userData, left, right);
        }
    }
}

TRUInteger TRComposedLineGetIndexOfCodeUnit(TRComposedLineRef line, TRFloat distance)
{
    TRUInteger runIndex = line->runCount;

    while (runIndex > 0) {
        GlyphRunRef glyphRun;

        runIndex -= 1;
        glyphRun = line->runs[runIndex];

        if (glyphRun->origin.x <= distance) {
            return TRGlyphRunGetIndexOfCodeUnit(glyphRun, distance - glyphRun->origin.x);
        }
    }

    return line->codeUnitStart;
}

TRFloat TRComposedLineGetPenOffset(TRComposedLineRef line, TRFloat flushFactor,
    TRFloat flushExtent)
{
    TRFloat penOffset = (flushExtent - (line->extent - line->trailingWhitespaceExtent))
                      * flushFactor;

    if ((line->paragraphLevel & 1) == 1) {
        penOffset -= line->trailingWhitespaceExtent;
    }

    return penOffset;
}

TRRect TRComposedLineGetBoundingBox(TRComposedLineRef line, TRRendererRef renderer)
{
    TRFloat minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    TRBoolean hasBox = TRFalse;
    TRRect box;
    TRUInteger runIndex;

    for (runIndex = 0; runIndex < line->runCount; runIndex++) {
        GlyphRunRef glyphRun = line->runs[runIndex];
        TRRange glyphRange;
        TRRect runBox;
        TRFloat left, top, right, bottom;

        glyphRange.index = 0;
        glyphRange.length = glyphRun->glyphCount;

        runBox = TRGlyphRunGetBoundingBox(glyphRun, glyphRange, renderer);
        left = runBox.origin.x + glyphRun->origin.x;
        top = runBox.origin.y + glyphRun->origin.y;
        right = left + runBox.size.width;
        bottom = top + runBox.size.height;

        if (runBox.size.width == 0.0f && runBox.size.height == 0.0f) {
            continue;
        }

        if (!hasBox) {
            minX = left;
            minY = top;
            maxX = right;
            maxY = bottom;
            hasBox = TRTrue;
        } else {
            minX = (left < minX ? left : minX);
            minY = (top < minY ? top : minY);
            maxX = (right > maxX ? right : maxX);
            maxY = (bottom > maxY ? bottom : maxY);
        }
    }

    box.origin.x = (hasBox ? minX : 0.0f);
    box.origin.y = (hasBox ? minY : 0.0f);
    box.size.width = (hasBox ? maxX - minX : 0.0f);
    box.size.height = (hasBox ? maxY - minY : 0.0f);

    return box;
}

TRComposedLineRef TRComposedLineRetain(TRComposedLineRef line)
{
    return ObjectRetain((ObjectRef)line);
}

void TRComposedLineRelease(TRComposedLineRef line)
{
    ObjectRelease((ObjectRef)line);
}
