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

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRRenderer.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRRenderer.h>
#include <Core/Object.h>
#include <Layout/CaretUtils.h>
#include <Layout/TextRun.h>
#include <Text/CaretEdgesBuilder.h>

#include "TRGlyphRun.h"

#define RUN             0
#define ADVANCES        1
#define CLUSTER_MAP     2
#define CARET_EDGES     3
#define COUNT           4

static void FinalizeGlyphRun(ObjectRef object)
{
    TRGlyphRun *glyphRun = object;

    TextRunRelease(glyphRun->textRun);
}

static TRGlyphRun *AllocateGlyphRun(TextRunRef textRun, TRUInteger codeUnitStart,
    TRUInteger codeUnitEnd, TRUInteger startExtra, TRUInteger endExtra, TRBoolean hasAdvances,
    TRUInteger glyphStart, TRUInteger glyphCount)
{
    TRUInteger clusterCount = (codeUnitEnd + endExtra) - (codeUnitStart - startExtra);
    TRUInteger sizes[COUNT] = { 0 };
    void *pointers[COUNT] = { NULL };
    TRGlyphRun *glyphRun;

    sizes[RUN] = sizeof(TRGlyphRun);
    sizes[ADVANCES] = (hasAdvances ? sizeof(TRFloat) * glyphCount : 0);
    sizes[CLUSTER_MAP] = sizeof(TRUInteger) * clusterCount;
    sizes[CARET_EDGES] = sizeof(TRFloat) * (clusterCount + 1);

    glyphRun = ObjectCreate(sizes, COUNT, pointers, FinalizeGlyphRun);

    if (glyphRun) {
        glyphRun->textRun = TextRunRetain(textRun);
        glyphRun->codeUnitStart = codeUnitStart;
        glyphRun->codeUnitEnd = codeUnitEnd;
        glyphRun->startExtra = startExtra;
        glyphRun->endExtra = endExtra;
        glyphRun->glyphStart = glyphStart;
        glyphRun->glyphCount = glyphCount;
        glyphRun->justifiedAdvances = pointers[ADVANCES];
        glyphRun->clusterMap = pointers[CLUSTER_MAP];
        glyphRun->caretEdges = pointers[CARET_EDGES];
        glyphRun->origin.x = 0.0f;
        glyphRun->origin.y = 0.0f;
        glyphRun->hasForegroundColor = TRFalse;
        glyphRun->foregroundColor = 0;
        glyphRun->userData = NULL;
    }

    return glyphRun;
}

#undef RUN
#undef ADVANCES
#undef CLUSTER_MAP
#undef CARET_EDGES
#undef COUNT

static void SetPaint(TRGlyphRun *glyphRun, const GlyphRunPaint *paint)
{
    if (paint) {
        glyphRun->hasForegroundColor = paint->hasForegroundColor;
        glyphRun->foregroundColor = paint->foregroundColor;
        glyphRun->userData = paint->userData;
    }
}

TR_INTERNAL TRGlyphRun *TRGlyphRunCreate(TextRunRef textRun, TRUInteger start, TRUInteger end,
    const GlyphRunPaint *paint)
{
    TRUInteger startExtra = 0;
    TRUInteger endExtra = 0;
    TRUInteger glyphStart = 0;
    TRUInteger glyphEnd = 1;
    TRGlyphRun *glyphRun;

    /* The range MUST NOT be empty, and MUST be within the text run. */
    TRAssert(start < end && start >= textRun->codeUnitStart && end <= textRun->codeUnitEnd);

    if (textRun->kind == TextRunKindReplacement) {
        start = textRun->codeUnitStart;
        end = textRun->codeUnitEnd;
    } else {
        startExtra = start - TextRunGetClusterStart(textRun, start);
        endExtra = TextRunGetClusterEnd(textRun, end - 1) - end;
        TextRunGetGlyphRange(textRun, start, end, &glyphStart, &glyphEnd);
    }

    glyphRun = AllocateGlyphRun(textRun, start, end, startExtra, endExtra, TRFalse, glyphStart,
        glyphEnd - glyphStart);

    if (glyphRun) {
        TRUInteger actualStart = start - startExtra;
        TRUInteger offset = actualStart - textRun->codeUnitStart;
        TRUInteger clusterCount = (end + endExtra) - actualStart;
        TRUInteger index;
        TRFloat boundary;

        boundary = TextRunGetCaretBoundary(textRun, start, end);

        for (index = 0; index < clusterCount; index++) {
            glyphRun->clusterMap[index] = textRun->clusterMap[offset + index] - glyphStart;
        }
        for (index = 0; index <= clusterCount; index++) {
            glyphRun->caretEdges[index] = textRun->caretEdges[offset + index] - boundary;
        }

        SetPaint(glyphRun, paint);
    }

    return glyphRun;
}

TR_INTERNAL TRGlyphRun *TRGlyphRunCreateCopy(TRGlyphRunRef source)
{
    TRGlyphRun *glyphRun = AllocateGlyphRun(source->textRun, source->codeUnitStart,
        source->codeUnitEnd, source->startExtra, source->endExtra,
        (source->justifiedAdvances != NULL), source->glyphStart, source->glyphCount);

    if (glyphRun) {
        TRUInteger clusterCount = (source->codeUnitEnd + source->endExtra)
                                - (source->codeUnitStart - source->startExtra);

        if (source->justifiedAdvances) {
            memcpy(glyphRun->justifiedAdvances, source->justifiedAdvances,
                sizeof(TRFloat) * source->glyphCount);
        }

        memcpy(glyphRun->clusterMap, source->clusterMap, sizeof(TRUInteger) * clusterCount);
        memcpy(glyphRun->caretEdges, source->caretEdges, sizeof(TRFloat) * (clusterCount + 1));

        glyphRun->origin = source->origin;
        glyphRun->hasForegroundColor = source->hasForegroundColor;
        glyphRun->foregroundColor = source->foregroundColor;
        glyphRun->userData = source->userData;
    }

    return glyphRun;
}

TR_INTERNAL TRGlyphRun *TRGlyphRunCreateJustified(TRGlyphRunRef source, const TRFloat *advances)
{
    TRGlyphRun *glyphRun = AllocateGlyphRun(source->textRun, source->codeUnitStart,
        source->codeUnitEnd, source->startExtra, source->endExtra, TRTrue, source->glyphStart,
        source->glyphCount);

    if (glyphRun) {
        TRUInteger clusterCount = (source->codeUnitEnd + source->endExtra)
                                - (source->codeUnitStart - source->startExtra);
        TRUInteger length = source->codeUnitEnd - source->codeUnitStart;
        TRBoolean isRTL = TRGlyphRunIsRTL(source);
        TRFloat boundary;
        TRUInteger index;

        memcpy(glyphRun->justifiedAdvances, advances, sizeof(TRFloat) * source->glyphCount);
        memcpy(glyphRun->clusterMap, source->clusterMap, sizeof(TRUInteger) * clusterCount);

        CaretEdgesBuild(source->textRun->isBackward, isRTL, glyphRun->justifiedAdvances,
            glyphRun->glyphCount, glyphRun->clusterMap, clusterCount, NULL, glyphRun->caretEdges);

        boundary = CaretUtilsGetLeftMargin(glyphRun->caretEdges, isRTL, source->startExtra,
            source->startExtra + length);
        for (index = 0; index <= clusterCount; index++) {
            glyphRun->caretEdges[index] -= boundary;
        }

        glyphRun->origin = source->origin;
        glyphRun->hasForegroundColor = source->hasForegroundColor;
        glyphRun->foregroundColor = source->foregroundColor;
        glyphRun->userData = source->userData;
    }

    return glyphRun;
}

TR_INTERNAL const TRFloat *TRGlyphRunGetAdvances(TRGlyphRunRef glyphRun)
{
    const TRFloat *advances = glyphRun->justifiedAdvances;

    if (!advances) {
        advances = glyphRun->textRun->glyphAdvances + glyphRun->glyphStart;
    }

    return advances;
}

TR_INTERNAL TRBoolean TRGlyphRunIsRTL(TRGlyphRunRef glyphRun)
{
    return TextRunIsRTL(glyphRun->textRun);
}

TR_INTERNAL TRUInteger TRGlyphRunGetActualStart(TRGlyphRunRef glyphRun)
{
    return glyphRun->codeUnitStart - glyphRun->startExtra;
}

TR_INTERNAL TRFloat TRGlyphRunGetCaretEdge(TRGlyphRunRef glyphRun, TRUInteger codeUnitIndex)
{
    /* The code unit MUST be within the run and its clusters. */
    TRAssert(codeUnitIndex >= glyphRun->codeUnitStart - glyphRun->startExtra
          && codeUnitIndex <= glyphRun->codeUnitEnd + glyphRun->endExtra);

    return glyphRun->caretEdges[codeUnitIndex - TRGlyphRunGetActualStart(glyphRun)];
}

TR_INTERNAL TRFloat TRGlyphRunGetDistanceInRange(TRGlyphRunRef glyphRun, TRUInteger start,
    TRUInteger end)
{
    TRUInteger actualStart = TRGlyphRunGetActualStart(glyphRun);

    return CaretUtilsGetDistance(glyphRun->caretEdges, TRGlyphRunIsRTL(glyphRun),
        start - actualStart, end - actualStart);
}

TRUInteger TRGlyphRunGetCodeUnitStart(TRGlyphRunRef run)
{
    return run->codeUnitStart;
}

TRUInteger TRGlyphRunGetCodeUnitEnd(TRGlyphRunRef run)
{
    return run->codeUnitEnd;
}

TRUInteger TRGlyphRunGetStartExtraLength(TRGlyphRunRef run)
{
    return run->startExtra;
}

TRUInteger TRGlyphRunGetEndExtraLength(TRGlyphRunRef run)
{
    return run->endExtra;
}

TRUInt8 TRGlyphRunGetBidiLevel(TRGlyphRunRef run)
{
    return run->textRun->bidiLevel;
}

TRWritingDirection TRGlyphRunGetWritingDirection(TRGlyphRunRef run)
{
    return run->textRun->writingDirection;
}

TRBoolean TRGlyphRunIsBackward(TRGlyphRunRef run)
{
    return run->textRun->isBackward;
}

TRTypefaceRef TRGlyphRunGetTypeface(TRGlyphRunRef run)
{
    return run->textRun->typeface;
}

TRFloat TRGlyphRunGetTypeSize(TRGlyphRunRef run)
{
    return run->textRun->typeSize;
}

TRFloat TRGlyphRunGetScaleX(TRGlyphRunRef run)
{
    return run->textRun->scaleX;
}

TRFloat TRGlyphRunGetScaleY(TRGlyphRunRef run)
{
    return run->textRun->scaleY;
}

TRReplacementRef TRGlyphRunGetReplacement(TRGlyphRunRef run)
{
    return run->textRun->replacement;
}

TRBoolean TRGlyphRunGetForegroundColor(TRGlyphRunRef run, TRColor *foregroundColor)
{
    if (run->hasForegroundColor) {
        *foregroundColor = run->foregroundColor;
    }

    return run->hasForegroundColor;
}

const void *TRGlyphRunGetUserData(TRGlyphRunRef run)
{
    return run->userData;
}

TRFloat TRGlyphRunGetAscent(TRGlyphRunRef run)
{
    return run->textRun->ascent;
}

TRFloat TRGlyphRunGetDescent(TRGlyphRunRef run)
{
    return run->textRun->descent;
}

TRFloat TRGlyphRunGetLeading(TRGlyphRunRef run)
{
    return run->textRun->leading;
}

TRPoint TRGlyphRunGetOrigin(TRGlyphRunRef run)
{
    return run->origin;
}

TRFloat TRGlyphRunGetWidth(TRGlyphRunRef run)
{
    return TRGlyphRunGetDistanceInRange(run, run->codeUnitStart, run->codeUnitEnd);
}

TRFloat TRGlyphRunGetHeight(TRGlyphRunRef run)
{
    return run->textRun->ascent + run->textRun->descent + run->textRun->leading;
}

TRUInteger TRGlyphRunGetGlyphCount(TRGlyphRunRef run)
{
    return run->glyphCount;
}

const TRGlyphID *TRGlyphRunGetGlyphIDsPtr(TRGlyphRunRef run)
{
    return run->textRun->glyphIDs + run->glyphStart;
}

const TRPoint *TRGlyphRunGetGlyphOffsetsPtr(TRGlyphRunRef run)
{
    return run->textRun->glyphOffsets + run->glyphStart;
}

const TRFloat *TRGlyphRunGetGlyphAdvancesPtr(TRGlyphRunRef run)
{
    return TRGlyphRunGetAdvances(run);
}

const TRUInteger *TRGlyphRunGetClusterMapPtr(TRGlyphRunRef run)
{
    return run->clusterMap;
}

TRUInteger TRGlyphRunGetClusterMapCount(TRGlyphRunRef run)
{
    return (run->codeUnitEnd + run->endExtra) - (run->codeUnitStart - run->startExtra);
}

TRUInteger TRGlyphRunGetClusterStart(TRGlyphRunRef run, TRUInteger codeUnitIndex)
{
    TRUInteger clusterStart = TRInvalidIndex;

    if (codeUnitIndex >= run->codeUnitStart && codeUnitIndex < run->codeUnitEnd) {
        clusterStart = TextRunGetClusterStart(run->textRun, codeUnitIndex);
    }

    return clusterStart;
}

TRUInteger TRGlyphRunGetClusterEnd(TRGlyphRunRef run, TRUInteger codeUnitIndex)
{
    TRUInteger clusterEnd = TRInvalidIndex;

    if (codeUnitIndex >= run->codeUnitStart && codeUnitIndex < run->codeUnitEnd) {
        clusterEnd = TextRunGetClusterEnd(run->textRun, codeUnitIndex);
    }

    return clusterEnd;
}

TRBoolean TRGlyphRunGetCodeUnitDistance(TRGlyphRunRef run, TRUInteger codeUnitIndex,
    TRFloat *distance)
{
    TRUInteger actualStart = TRGlyphRunGetActualStart(run);
    TRUInteger actualCount = (run->codeUnitEnd + run->endExtra) - actualStart;
    TRBoolean isFound = (codeUnitIndex >= actualStart
                         && codeUnitIndex - actualStart <= actualCount);

    if (isFound) {
        *distance = TRGlyphRunGetCaretEdge(run, codeUnitIndex);
    }

    return isFound;
}

TRUInteger TRGlyphRunGetCodeUnitIndex(TRGlyphRunRef run, TRFloat distance)
{
    TRUInteger actualStart = TRGlyphRunGetActualStart(run);
    TRUInteger first = run->codeUnitStart - actualStart;
    TRUInteger last = run->codeUnitEnd - actualStart;

    return actualStart + CaretUtilsGetIndexOfEdge(run->caretEdges, TRGlyphRunIsRTL(run), distance,
        first, last);
}

TRRect TRGlyphRunGetInkBox(TRGlyphRunRef run, TRRendererRef renderer)
{
    TextRunRef textRun = run->textRun;
    TRRect box;

    if (textRun->kind == TextRunKindReplacement) {
        box.origin.x = 0.0f;
        box.origin.y = 0.0f;
        box.size.width = TRGlyphRunGetWidth(run);
        box.size.height = TRGlyphRunGetHeight(run);
    } else {
        /* The renderer is set up for the run only while it measures, and then it is restored. */
        TRTypefaceRef oldTypeface = renderer->typeface;
        TRFloat oldTypeSize = renderer->typeSize;
        TRFloat oldScaleX = renderer->scaleX;
        TRFloat oldScaleY = renderer->scaleY;
        TRWritingDirection oldDirection = renderer->writingDirection;

        if (oldTypeface) {
            TRTypefaceRetain(oldTypeface);
        }

        TRRendererSetTypeface(renderer, textRun->typeface);
        TRRendererSetTypeSize(renderer, textRun->typeSize);
        TRRendererSetScaleX(renderer, textRun->scaleX);
        TRRendererSetScaleY(renderer, textRun->scaleY);
        TRRendererSetWritingDirection(renderer, textRun->writingDirection);

        box = TRRendererGetRunInkBox(renderer, TRGlyphRunGetGlyphIDsPtr(run),
            TRGlyphRunGetGlyphOffsetsPtr(run), TRGlyphRunGetAdvances(run), run->glyphCount);

        TRRendererSetTypeface(renderer, oldTypeface);
        TRRendererSetTypeSize(renderer, oldTypeSize);
        TRRendererSetScaleX(renderer, oldScaleX);
        TRRendererSetScaleY(renderer, oldScaleY);
        TRRendererSetWritingDirection(renderer, oldDirection);

        if (oldTypeface) {
            TRTypefaceRelease(oldTypeface);
        }
    }

    return box;
}

TRGlyphRunRef TRGlyphRunRetain(TRGlyphRunRef run)
{
    return ObjectRetain((ObjectRef)run);
}

void TRGlyphRunRelease(TRGlyphRunRef run)
{
    ObjectRelease((ObjectRef)run);
}
