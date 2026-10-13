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

#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRLineIndex.h>
#include <Tehreer/TRRenderer.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Object.h>

/* The height of a band, in the units of the frame. */
#define BandHeight      512.0

/*
 * The greatest number of bands, and of entries in them, that an index takes. It keeps the sizes of
 * the allocation from overflowing, and refuses lines which are placed absurdly far from each other.
 */
#define MaxBandCount    ((double)(TRInvalidIndex / 16))

/*
 * The bands cover the boxes without a gap, from the band of the top of the topmost box, which is
 * number `firstBand`, for `bandCount` bands. The lines of the band `b` are
 * `bandLines[bandStarts[b]]` up to, but not including, `bandLines[bandStarts[b + 1]]`, in the
 * order of the frame.
 */
typedef struct _TRLineIndex {
    ObjectBase _base;
    double firstBand;
    TRUInteger lineCount;
    TRUInteger bandCount;
    TRRect *boxes;
    TRUInteger *bandStarts;
    TRUInteger *bandLines;
} TRLineIndex;

/* ---------- Boxes ---------- */

static double GetBandNumber(TRFloat y)
{
    return floor((double)y / BandHeight);
}

static TRRect MakeLineBox(TRComposedLineRef line, TRFloat frameWidth, TRRendererRef renderer)
{
    TRPoint origin = TRComposedLineGetOrigin(line);
    TRRect inkBox = TRComposedLineGetInkBox(line, renderer);
    TRFloat left = 0.0f;
    TRFloat top = TRComposedLineGetTop(line);
    TRFloat right = frameWidth;
    TRFloat bottom = TRComposedLineGetBottom(line);
    TRRect box;

    /* An empty ink box means that the line has no ink. */
    if (inkBox.size.width != 0.0f || inkBox.size.height != 0.0f) {
        TRFloat inkLeft = inkBox.origin.x + origin.x;
        TRFloat inkTop = inkBox.origin.y + origin.y;
        TRFloat inkRight = inkLeft + inkBox.size.width;
        TRFloat inkBottom = inkTop + inkBox.size.height;

        left = NumberMin(left, inkLeft);
        top = NumberMin(top, inkTop);
        right = NumberMax(right, inkRight);
        bottom = NumberMax(bottom, inkBottom);
    }

    left = (TRFloat)floor(left);
    top = (TRFloat)floor(top);
    right = (TRFloat)ceil(right);
    bottom = (TRFloat)ceil(bottom);

    box.origin.x = left;
    box.origin.y = top;
    box.size.width = right - left;
    box.size.height = bottom - top;

    return box;
}

static TRBoolean AppendLineBoxes(Array *boxes, TRComposedFrameRef frame, TRRendererRef renderer)
{
    TRBoolean isAppended = TRTrue;
    TRUInteger lineCount = TRComposedFrameGetLineCount(frame);
    TRFloat frameWidth = TRComposedFrameGetWidth(frame);
    TRUInteger lineIndex;

    for (lineIndex = 0; lineIndex < lineCount; lineIndex++) {
        TRComposedLineRef line = TRComposedFrameGetLine(frame, lineIndex);
        TRRect box = MakeLineBox(line, frameWidth, renderer);

        if (!ArrayAppend(boxes, &box)) {
            isAppended = TRFalse;
            break;
        }
    }

    return isAppended;
}

/* ---------- Bands ---------- */

/* Gets the numbers of the first and the last band that a box touches. */
static void GetBoxBands(const TRRect *box, double *firstBand, double *lastBand)
{
    TRFloat top = box->origin.y;
    TRFloat bottom = top + box->size.height;

    *firstBand = GetBandNumber(top);
    *lastBand = GetBandNumber(bottom);
}

/* Returns the offset of a band from the first band of the index. */
static TRUInteger GetBandOffset(const TRLineIndex *lineIndex, double band)
{
    return (TRUInteger)(band - lineIndex->firstBand);
}

/* Tells if the bands take a number of bands and entries that the index can have. */
static TRBoolean BandsAreSupported(const TRRect *boxes, TRUInteger lineCount, double *firstBand,
    TRUInteger *bandCount, TRUInteger *entryCount)
{
    TRBoolean isSupported = TRFalse;
    double minBand = 0.0;
    double maxBand = 0.0;
    double entries = 0.0;
    double span;
    TRUInteger lineIndex;

    for (lineIndex = 0; lineIndex < lineCount; lineIndex++) {
        double boxFirst;
        double boxLast;

        GetBoxBands(&boxes[lineIndex], &boxFirst, &boxLast);

        if (lineIndex == 0) {
            minBand = boxFirst;
            maxBand = boxLast;
        } else {
            minBand = NumberMin(minBand, boxFirst);
            maxBand = NumberMax(maxBand, boxLast);
        }

        entries += boxLast - boxFirst + 1.0;
    }

    span = maxBand - minBand + 1.0;

    if (span <= MaxBandCount && entries <= MaxBandCount) {
        *firstBand = minBand;
        *bandCount = (TRUInteger)span;
        *entryCount = (TRUInteger)entries;

        isSupported = TRTrue;
    }

    return isSupported;
}

/*
 * Puts the lines into the bands by counting them first, so that each band takes exactly the entries
 * it needs. The starts of the bands are used as the positions to fill, and are moved back after.
 */
static void FillBands(TRLineIndex *lineIndex)
{
    TRUInteger lineCount = lineIndex->lineCount;
    TRUInteger bandCount = lineIndex->bandCount;
    TRUInteger *bandStarts = lineIndex->bandStarts;
    TRUInteger line;
    TRUInteger band;

    for (line = 0; line < lineCount; line++) {
        double boxFirst;
        double boxLast;
        TRUInteger offset;
        TRUInteger lastOffset;

        GetBoxBands(&lineIndex->boxes[line], &boxFirst, &boxLast);
        lastOffset = GetBandOffset(lineIndex, boxLast);

        for (offset = GetBandOffset(lineIndex, boxFirst); offset <= lastOffset; offset++) {
            bandStarts[offset + 1] += 1;
        }
    }

    for (band = 0; band < bandCount; band++) {
        bandStarts[band + 1] += bandStarts[band];
    }

    for (line = 0; line < lineCount; line++) {
        double boxFirst;
        double boxLast;
        TRUInteger offset;
        TRUInteger lastOffset;

        GetBoxBands(&lineIndex->boxes[line], &boxFirst, &boxLast);
        lastOffset = GetBandOffset(lineIndex, boxLast);

        for (offset = GetBandOffset(lineIndex, boxFirst); offset <= lastOffset; offset++) {
            lineIndex->bandLines[bandStarts[offset]] = line;
            bandStarts[offset] += 1;
        }
    }

    for (band = bandCount; band > 0; band--) {
        bandStarts[band] = bandStarts[band - 1];
    }
    bandStarts[0] = 0;
}

static TRLineIndex *CreateLineIndex(const TRRect *boxes, TRUInteger lineCount)
{
    TRLineIndex *lineIndex = NULL;
    double firstBand;
    TRUInteger bandCount;
    TRUInteger entryCount;

    if (BandsAreSupported(boxes, lineCount, &firstBand, &bandCount, &entryCount)) {
        TRUInteger sizes[4];
        void *pointers[4];

        sizes[0] = sizeof(TRLineIndex);
        sizes[1] = sizeof(TRRect) * lineCount;
        sizes[2] = sizeof(TRUInteger) * (bandCount + 1);
        sizes[3] = sizeof(TRUInteger) * entryCount;

        /* The boxes and the bands are in the memory of the object, so it needs no finalizer. */
        lineIndex = ObjectCreate(sizes, 4, pointers, NULL);

        if (lineIndex) {
            lineIndex->firstBand = firstBand;
            lineIndex->lineCount = lineCount;
            lineIndex->bandCount = bandCount;
            lineIndex->boxes = pointers[1];
            lineIndex->bandStarts = pointers[2];
            lineIndex->bandLines = pointers[3];

            memcpy(lineIndex->boxes, boxes, sizes[1]);
            memset(lineIndex->bandStarts, 0, sizes[2]);

            FillBands(lineIndex);
        }
    }

    return lineIndex;
}

/* ---------- Queries ---------- */

static TRBoolean BoxesIntersect(const TRRect *box, const TRRect *rect, TRFloat rectRight,
    TRFloat rectBottom)
{
    TRFloat boxRight = box->origin.x + box->size.width;
    TRFloat boxBottom = box->origin.y + box->size.height;

    return box->origin.x < rectRight && rect->origin.x < boxRight
        && box->origin.y < rectBottom && rect->origin.y < boxBottom;
}

/*
 * Passes the lines of a band that intersect the rectangle, but not the ones which are passed by a
 * band before it. A line that is in several bands is passed by the first band that has it within
 * the bands of the rectangle, which is the band of the top of its box, or the one of the top of the
 * rectangle if the box starts above it.
 */
static void EnumerateBandLines(const TRLineIndex *lineIndex, TRUInteger offset, double topBand,
    const TRRect *rect, TRFloat rectRight, TRFloat rectBottom, TRLineIndexFunc func,
    void *userData, TRBoolean *shouldStop)
{
    double band = lineIndex->firstBand + (double)offset;
    TRUInteger position = lineIndex->bandStarts[offset];
    TRUInteger end = lineIndex->bandStarts[offset + 1];

    for (; position < end; position++) {
        TRUInteger line = lineIndex->bandLines[position];
        const TRRect *box = &lineIndex->boxes[line];
        double boxBand = GetBandNumber(box->origin.y);

        if (NumberMax(boxBand, topBand) == band
            && BoxesIntersect(box, rect, rectRight, rectBottom)) {
            func(userData, line, shouldStop);

            if (*shouldStop) {
                break;
            }
        }
    }
}

/* ---------- Public API ---------- */

TRLineIndexRef TRLineIndexCreate(TRComposedFrameRef frame, TRRendererRef renderer)
{
    TRLineIndex *lineIndex = NULL;

    if (frame && renderer) {
        Array boxes;

        ArrayInitialize(&boxes, sizeof(TRRect));

        if (AppendLineBoxes(&boxes, frame, renderer)) {
            TRUInteger lineCount = ArrayGetCount(&boxes);

            /* A frame MUST have at least one line. */
            TRAssert(lineCount > 0);

            lineIndex = CreateLineIndex(ArrayGetItems(&boxes), lineCount);
        }

        ArrayFinalize(&boxes);
    }

    return lineIndex;
}

TRUInteger TRLineIndexGetLineCount(TRLineIndexRef index)
{
    return index->lineCount;
}

TRBoolean TRLineIndexGetLineBox(TRLineIndexRef index, TRUInteger lineIndex, TRRect *box)
{
    TRBoolean isSet = TRFalse;

    if (box && IndexIsValid(lineIndex, index->lineCount)) {
        *box = index->boxes[lineIndex];
        isSet = TRTrue;
    }

    return isSet;
}

TRBoolean TRLineIndexEnumerateLines(TRLineIndexRef index, TRRect rect, TRLineIndexFunc func,
    void *userData)
{
    TRBoolean isEnumerated = TRFalse;
    TRFloat rectRight = rect.origin.x + rect.size.width;
    TRFloat rectBottom = rect.origin.y + rect.size.height;

    /* Both sums are not a number if the origin, or the size, is not a number. */
    if (func && rect.size.width >= 0.0f && rect.size.height >= 0.0f
        && rectRight == rectRight && rectBottom == rectBottom) {
        double firstBand = index->firstBand;
        double lastBand = firstBand + (double)(index->bandCount - 1);
        double topBand = GetBandNumber(rect.origin.y);
        double bottomBand = GetBandNumber(rectBottom);

        isEnumerated = TRTrue;

        if (topBand <= lastBand && bottomBand >= firstBand) {
            TRBoolean shouldStop = TRFalse;
            TRUInteger offset;
            TRUInteger lastOffset;

            topBand = NumberMax(topBand, firstBand);
            bottomBand = NumberMin(bottomBand, lastBand);
            lastOffset = GetBandOffset(index, bottomBand);

            for (offset = GetBandOffset(index, topBand); offset <= lastOffset; offset++) {
                EnumerateBandLines(index, offset, topBand, &rect, rectRight, rectBottom, func,
                    userData, &shouldStop);

                if (shouldStop) {
                    break;
                }
            }
        }
    }

    return isEnumerated;
}

TRUInteger TRLineIndexGetFirstLineIndexBelow(TRLineIndexRef index, TRFloat y)
{
    TRUInteger firstLineIndex = TRInvalidIndex;

    if (y == y) {
        double firstBand = index->firstBand;
        double lastBand = firstBand + (double)(index->bandCount - 1);
        double band = GetBandNumber(y);

        if (band <= lastBand) {
            TRUInteger offset = GetBandOffset(index, NumberMax(band, firstBand));
            TRUInteger position = index->bandStarts[offset];
            TRUInteger end = index->bandStarts[index->bandCount];

            /* The entries of the bands are one after another, so they are looked at in one go. */
            for (; position < end; position++) {
                TRUInteger line = index->bandLines[position];
                const TRRect *box = &index->boxes[line];
                TRFloat boxBottom = box->origin.y + box->size.height;

                if (boxBottom > y) {
                    firstLineIndex = line;
                    break;
                }
            }
        }
    }

    return firstLineIndex;
}

TRLineIndexRef TRLineIndexRetain(TRLineIndexRef index)
{
    return ObjectRetain((ObjectRef)index);
}

void TRLineIndexRelease(TRLineIndexRef index)
{
    ObjectRelease((ObjectRef)index);
}
