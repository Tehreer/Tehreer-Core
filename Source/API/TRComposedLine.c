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
    TRComposedLine *line = object;
    TRUInteger index;

    for (index = 0; index < line->runCount; index++) {
        TRGlyphRunRelease(line->runs[index]);
    }

    AllocatorDeallocateBlock(line->runs);
}

static void ResolveLineMetrics(TRComposedLine *line, const TextBuffer *buffer)
{
    TRUInteger trailingWhitespaceStart = TextBufferGetTrailingWhitespaceStart(buffer,
        line->codeUnitStart, line->codeUnitEnd);
    TRGlyphRun *blockRun = NULL;
    TRUInteger index;

    for (index = 0; index < line->runCount; index++) {
        TRGlyphRun *glyphRun = line->runs[index];
        TRUInteger wsStart = NumberMax(glyphRun->codeUnitStart, trailingWhitespaceStart);
        TRUInteger wsEnd = NumberMin(glyphRun->codeUnitEnd, line->codeUnitEnd);
        TRFloat ascent = TRGlyphRunGetAscent(glyphRun);
        TRFloat descent = TRGlyphRunGetDescent(glyphRun);
        TRFloat leading = TRGlyphRunGetLeading(glyphRun);
        TRFloat width = TRGlyphRunGetWidth(glyphRun);

        glyphRun->origin.x = line->extent;

        if (wsStart < wsEnd) {
            line->trailingWhitespaceExtent += TRGlyphRunGetDistanceInRange(glyphRun, wsStart,
                wsEnd);
        }

        line->ascent = NumberMax(line->ascent, ascent);
        line->descent = NumberMax(line->descent, descent);
        line->leading = NumberMax(line->leading, leading);
        line->extent += width;

        if (TextRunIsBlock(glyphRun->textRun)) {
            blockRun = glyphRun;
        }
    }

    /*
     * A line that holds a block replacement is as tall as the replacement and its margins, and
     * nothing else: the metrics of the newline that ends its paragraph would only add blank
     * space.
     */
    if (blockRun) {
        line->ascent = TRGlyphRunGetAscent(blockRun);
        line->descent = TRGlyphRunGetDescent(blockRun);
        line->leading = TRGlyphRunGetLeading(blockRun);
        line->isBlock = TRTrue;
    }
}

TR_INTERNAL TRComposedLine *TRComposedLineCreate(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end, TRGlyphRun **runs, TRUInteger runCount, TRUInt8 paragraphLevel)
{
    const TRUInteger size = sizeof(TRComposedLine);
    TRComposedLine *line = NULL;
    TRGlyphRun **lineRuns;
    TRUInteger index;

    /* The line MUST have at least one run. */
    TRAssert(runCount > 0);

    lineRuns = AllocatorAllocateBlock(runCount * sizeof(TRGlyphRun *));

    if (lineRuns) {
        void *pointer = NULL;

        line = ObjectCreate(&size, 1, &pointer, FinalizeComposedLine);

        if (line) {
            memcpy(lineRuns, runs, runCount * sizeof(TRGlyphRun *));

            line->runs = lineRuns;
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
            line->isTruncated = TRFalse;
            line->flushFactor = 0.0f;
            line->intrinsicMargin = 0.0f;

            ResolveLineMetrics(line, buffer);
        } else {
            AllocatorDeallocateBlock(lineRuns);
        }
    }

    if (!line) {
        for (index = 0; index < runCount; index++) {
            TRGlyphRunRelease(runs[index]);
        }
    }

    return line;
}

TRUInteger TRComposedLineGetCodeUnitStart(TRComposedLineRef line)
{
    return line->codeUnitStart;
}

TRUInteger TRComposedLineGetCodeUnitEnd(TRComposedLineRef line)
{
    return line->codeUnitEnd;
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
    return line->origin.y - line->ascent;
}

TRFloat TRComposedLineGetBottom(TRComposedLineRef line)
{
    return line->origin.y + line->descent + line->leading;
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

TRBoolean TRComposedLineIsTruncated(TRComposedLineRef line)
{
    return line->isTruncated;
}

TRUInteger TRComposedLineGetGlyphRunCount(TRComposedLineRef line)
{
    return line->runCount;
}

TRGlyphRunRef TRComposedLineGetGlyphRun(TRComposedLineRef line, TRUInteger index)
{
    TRGlyphRunRef glyphRun = NULL;

    if (index < line->runCount) {
        glyphRun = line->runs[index];
    }

    return glyphRun;
}

TRBoolean TRComposedLineGetCodeUnitDistance(TRComposedLineRef line, TRUInteger codeUnitIndex,
    TRFloat *distance)
{
    TRBoolean isFound = (codeUnitIndex >= line->codeUnitStart && codeUnitIndex <= line->codeUnitEnd);

    if (isFound) {
        TRFloat extent = 0.0f;
        TRUInteger runIndex;

        for (runIndex = 0; runIndex < line->runCount; runIndex++) {
            TRGlyphRunRef glyphRun = line->runs[runIndex];

            if (codeUnitIndex >= glyphRun->codeUnitStart && codeUnitIndex < glyphRun->codeUnitEnd) {
                extent += TRGlyphRunGetCaretEdge(glyphRun, codeUnitIndex);
                break;
            } else {
                extent += TRGlyphRunGetWidth(glyphRun);
            }
        }

        *distance = extent;
    }

    return isFound;
}

TRBoolean TRComposedLineEnumerateEdges(TRComposedLineRef line, TRUInteger index,
    TRUInteger length, TREdgeFunc func, void *userData)
{
    TRUInteger lineLength = line->codeUnitEnd - line->codeUnitStart;
    TRBoolean isValid = (index >= line->codeUnitStart
                         && RangeIsValid(index - line->codeUnitStart, length, lineLength));

    if (isValid && length > 0) {
        TRUInteger rangeEnd = index + length;
        TRBoolean shouldStop = TRFalse;
        TRUInteger runIndex;

        for (runIndex = 0; runIndex < line->runCount && !shouldStop; runIndex++) {
            TRGlyphRunRef glyphRun = line->runs[runIndex];

            if (glyphRun->codeUnitStart < rangeEnd && glyphRun->codeUnitEnd > index) {
                TRUInteger selectionStart = NumberMax(index, glyphRun->codeUnitStart);
                TRUInteger selectionEnd = NumberMin(rangeEnd, glyphRun->codeUnitEnd);
                TRFloat leadingEdge = TRGlyphRunGetCaretEdge(glyphRun, selectionStart);
                TRFloat trailingEdge = TRGlyphRunGetCaretEdge(glyphRun, selectionEnd);
                TRFloat relativeLeft = glyphRun->origin.x;
                TRFloat left = NumberMin(leadingEdge, trailingEdge) + relativeLeft;
                TRFloat right = NumberMax(leadingEdge, trailingEdge) + relativeLeft;

                func(userData, left, right, &shouldStop);
            }
        }
    }

    return isValid;
}

TRUInteger TRComposedLineGetCodeUnitIndex(TRComposedLineRef line, TRFloat distance)
{
    TRUInteger codeUnitIndex = line->codeUnitStart;
    TRUInteger runIndex = line->runCount;

    while (runIndex > 0) {
        TRGlyphRunRef glyphRun;

        runIndex -= 1;
        glyphRun = line->runs[runIndex];

        if (glyphRun->origin.x <= distance) {
            codeUnitIndex = TRGlyphRunGetCodeUnitIndex(glyphRun, distance - glyphRun->origin.x);
            break;
        }
    }

    return codeUnitIndex;
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

TRRect TRComposedLineGetInkBox(TRComposedLineRef line, TRRendererRef renderer)
{
    TRFloat minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    TRBoolean hasBox = TRFalse;
    TRRect box;
    TRUInteger runIndex;

    for (runIndex = 0; runIndex < line->runCount; runIndex++) {
        TRGlyphRunRef glyphRun = line->runs[runIndex];
        TRRect runBox;
        TRFloat left, top, right, bottom;

        runBox = TRGlyphRunGetInkBox(glyphRun, renderer);
        left = runBox.origin.x + glyphRun->origin.x;
        top = runBox.origin.y + glyphRun->origin.y;
        right = left + runBox.size.width;
        bottom = top + runBox.size.height;

        if (runBox.size.width != 0.0f || runBox.size.height != 0.0f) {
            if (!hasBox) {
                minX = left;
                minY = top;
                maxX = right;
                maxY = bottom;
                hasBox = TRTrue;
            } else {
                minX = NumberMin(left, minX);
                minY = NumberMin(top, minY);
                maxX = NumberMax(right, maxX);
                maxY = NumberMax(bottom, maxY);
            }
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
