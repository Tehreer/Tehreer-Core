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
#include <Tehreer/TRLineLocator.h>
#include <Tehreer/TRRenderer.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Object.h>

/* The height of a band, in the units of the frame. */
#define BandHeight      512.0

/*
 * The greatest number of bands that a locator takes: 2^22, which are 2^31 units, about 2.1 billion,
 * of height. A frame that is ten million units tall takes about 20 thousand bands. The starts of
 * the bands take 8 bytes each, so at most 32 MB.
 */
#define MaxBandCount    ((double)(1 << 22))

/*
 * The greatest number of entries in the bands: 2^24, which take at most 128 MB. A frame whose lines
 * have boxes of a height of a few bands needs a small multiple of its line count. Both limits also
 * keep the sums of the sizes of the chunks far from overflowing.
 */
#define MaxEntryCount   ((double)(1 << 24))

/*
 * The bands cover the boxes without a gap, from the band of the top of the topmost box, which is
 * number `firstBand`, for `bandCount` bands. The lines of the band `b` are
 * `bandLines[bandStarts[b]]` up to, but not including, `bandLines[bandStarts[b + 1]]`, in the
 * order of the frame.
 */
typedef struct _TRLineLocator {
    ObjectBase _base;
    double firstBand;
    TRUInteger lineCount;
    TRUInteger bandCount;
    TRRect *boxes;
    TRUInteger *bandStarts;
    TRUInteger *bandLines;
} TRLineLocator;

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

/* Returns the offset of a band from the first band of the locator. */
static TRUInteger GetBandOffset(const TRLineLocator *locator, double band)
{
    return (TRUInteger)(band - locator->firstBand);
}

/*
 * Tells if the bands take no more bands and entries than the limits. The counts are summed as
 * doubles, so they cannot wrap, and a box that is not finite does not pass the comparisons.
 */
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

    if (span <= MaxBandCount && entries <= MaxEntryCount) {
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
static void FillBands(TRLineLocator *locator)
{
    TRUInteger lineCount = locator->lineCount;
    TRUInteger bandCount = locator->bandCount;
    TRUInteger *bandStarts = locator->bandStarts;
    TRUInteger line;
    TRUInteger band;

    for (line = 0; line < lineCount; line++) {
        double boxFirst;
        double boxLast;
        TRUInteger offset;
        TRUInteger lastOffset;

        GetBoxBands(&locator->boxes[line], &boxFirst, &boxLast);
        lastOffset = GetBandOffset(locator, boxLast);

        for (offset = GetBandOffset(locator, boxFirst); offset <= lastOffset; offset++) {
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

        GetBoxBands(&locator->boxes[line], &boxFirst, &boxLast);
        lastOffset = GetBandOffset(locator, boxLast);

        for (offset = GetBandOffset(locator, boxFirst); offset <= lastOffset; offset++) {
            locator->bandLines[bandStarts[offset]] = line;
            bandStarts[offset] += 1;
        }
    }

    for (band = bandCount; band > 0; band--) {
        bandStarts[band] = bandStarts[band - 1];
    }
    bandStarts[0] = 0;
}

#define LOCATOR         0
#define BOXES           1
#define BAND_STARTS     2
#define BAND_LINES      3
#define COUNT           4

static TRLineLocator *CreateLineLocator(const TRRect *boxes, TRUInteger lineCount)
{
    TRLineLocator *locator = NULL;
    double firstBand;
    TRUInteger bandCount;
    TRUInteger entryCount;

    if (BandsAreSupported(boxes, lineCount, &firstBand, &bandCount, &entryCount)) {
        TRUInteger sizes[COUNT];
        void *pointers[COUNT];

        sizes[LOCATOR] = sizeof(TRLineLocator);
        sizes[BOXES] = sizeof(TRRect) * lineCount;
        sizes[BAND_STARTS] = sizeof(TRUInteger) * (bandCount + 1);
        sizes[BAND_LINES] = sizeof(TRUInteger) * entryCount;

        /* The boxes and the bands are in the memory of the object, so it needs no finalizer. */
        locator = ObjectCreate(sizes, COUNT, pointers, NULL);

        if (locator) {
            locator->firstBand = firstBand;
            locator->lineCount = lineCount;
            locator->bandCount = bandCount;
            locator->boxes = pointers[BOXES];
            locator->bandStarts = pointers[BAND_STARTS];
            locator->bandLines = pointers[BAND_LINES];

            memcpy(locator->boxes, boxes, sizes[BOXES]);
            memset(locator->bandStarts, 0, sizes[BAND_STARTS]);

            FillBands(locator);
        }
    }

    return locator;
}

#undef LOCATOR
#undef BOXES
#undef BAND_STARTS
#undef BAND_LINES
#undef COUNT

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
static void EnumerateBandLines(const TRLineLocator *locator, TRUInteger offset, double topBand,
    const TRRect *rect, TRFloat rectRight, TRFloat rectBottom, TRLineLocatorFunc func,
    void *userData, TRBoolean *shouldStop)
{
    double band = locator->firstBand + (double)offset;
    TRUInteger position = locator->bandStarts[offset];
    TRUInteger end = locator->bandStarts[offset + 1];

    for (; position < end; position++) {
        TRUInteger line = locator->bandLines[position];
        const TRRect *box = &locator->boxes[line];
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

/*
 * Passes the lines of the bands from one offset to another, and stops after the band in which the
 * function asked to stop.
 */
static void EnumerateBands(const TRLineLocator *locator, TRUInteger firstOffset,
    TRUInteger lastOffset, double topBand, const TRRect *rect, TRFloat rectRight,
    TRFloat rectBottom, TRLineLocatorFunc func, void *userData)
{
    TRBoolean shouldStop = TRFalse;
    TRUInteger offset;

    for (offset = firstOffset; offset <= lastOffset; offset++) {
        EnumerateBandLines(locator, offset, topBand, rect, rectRight, rectBottom, func, userData,
            &shouldStop);

        if (shouldStop) {
            break;
        }
    }
}

/* Gets the edge of a rectangle that is the sum of the origin and the size. */
static TRBoolean GetRectEdge(TRFloat origin, TRFloat size, TRFloat *edge)
{
    TRBoolean isValid = TRFalse;

    /* The size is not a number, or is negative, if it fails the comparison. */
    if (origin == origin && size >= 0.0f) {
        /* The sum is not a number if the origin is negative infinity and the size is infinite. */
        *edge = (size == HUGE_VAL ? HUGE_VAL : origin + size);
        isValid = TRTrue;
    }

    return isValid;
}

/* Returns the first line whose box reaches below a y, among the entries from a position. */
static TRUInteger FindFirstLineBelow(const TRLineLocator *locator, TRUInteger position, TRFloat y)
{
    TRUInteger firstLineIndex = TRInvalidIndex;
    TRUInteger end = locator->bandStarts[locator->bandCount];

    /* The entries of the bands are one after another, so they are looked at in one go. */
    for (; position < end; position++) {
        TRUInteger line = locator->bandLines[position];
        const TRRect *box = &locator->boxes[line];
        TRFloat boxBottom = box->origin.y + box->size.height;

        if (boxBottom > y) {
            firstLineIndex = line;
            break;
        }
    }

    return firstLineIndex;
}

/* ---------- Public API ---------- */

TRLineLocatorRef TRLineLocatorCreate(TRComposedFrameRef frame, TRRendererRef renderer)
{
    TRLineLocator *locator = NULL;

    if (frame && renderer) {
        Array boxes;

        ArrayInitialize(&boxes, sizeof(TRRect));

        if (AppendLineBoxes(&boxes, frame, renderer)) {
            TRUInteger lineCount = ArrayGetCount(&boxes);

            /* A frame MUST have at least one line. */
            TRAssert(lineCount > 0);

            locator = CreateLineLocator(ArrayGetItems(&boxes), lineCount);
        }

        ArrayFinalize(&boxes);
    }

    return locator;
}

TRUInteger TRLineLocatorGetLineCount(TRLineLocatorRef locator)
{
    return locator->lineCount;
}

TRBoolean TRLineLocatorGetLineBox(TRLineLocatorRef locator, TRUInteger lineIndex, TRRect *box)
{
    TRBoolean isSet = TRFalse;

    if (box && IndexIsValid(lineIndex, locator->lineCount)) {
        *box = locator->boxes[lineIndex];
        isSet = TRTrue;
    }

    return isSet;
}

TRBoolean TRLineLocatorEnumerateLines(TRLineLocatorRef locator, TRRect rect, TRLineLocatorFunc func,
    void *userData)
{
    TRBoolean isEnumerated = TRFalse;
    TRFloat rectRight;
    TRFloat rectBottom;

    if (func && GetRectEdge(rect.origin.x, rect.size.width, &rectRight)
        && GetRectEdge(rect.origin.y, rect.size.height, &rectBottom)) {
        double firstBand = locator->firstBand;
        double lastBand = firstBand + (double)(locator->bandCount - 1);
        double topBand = GetBandNumber(rect.origin.y);
        double bottomBand = GetBandNumber(rectBottom);

        isEnumerated = TRTrue;

        if (topBand <= lastBand && bottomBand >= firstBand) {
            TRUInteger firstOffset;
            TRUInteger lastOffset;

            topBand = NumberMax(topBand, firstBand);
            bottomBand = NumberMin(bottomBand, lastBand);
            firstOffset = GetBandOffset(locator, topBand);
            lastOffset = GetBandOffset(locator, bottomBand);

            EnumerateBands(locator, firstOffset, lastOffset, topBand, &rect, rectRight,
                rectBottom, func, userData);
        }
    }

    return isEnumerated;
}

TRUInteger TRLineLocatorGetFirstLineIndexBelow(TRLineLocatorRef locator, TRFloat y)
{
    TRUInteger firstLineIndex = TRInvalidIndex;

    if (y == y) {
        double firstBand = locator->firstBand;
        double lastBand = firstBand + (double)(locator->bandCount - 1);
        double band = GetBandNumber(y);

        if (band <= lastBand) {
            TRUInteger offset = GetBandOffset(locator, NumberMax(band, firstBand));
            TRUInteger position = locator->bandStarts[offset];

            firstLineIndex = FindFirstLineBelow(locator, position, y);
        }
    }

    return firstLineIndex;
}

TRLineLocatorRef TRLineLocatorRetain(TRLineLocatorRef locator)
{
    return ObjectRetain((ObjectRef)locator);
}

void TRLineLocatorRelease(TRLineLocatorRef locator)
{
    ObjectRelease((ObjectRef)locator);
}
