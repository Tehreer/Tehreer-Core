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
    GlyphRunRef glyphRun = object;

    TextRunRelease(glyphRun->textRun);
}

static GlyphRunRef AllocateGlyphRun(TextRunRef textRun, TRUInteger codeUnitStart,
    TRUInteger codeUnitEnd, TRUInteger startExtra, TRUInteger endExtra, TRBoolean hasAdvances,
    TRUInteger glyphStart, TRUInteger glyphCount)
{
    TRUInteger clusterCount = (codeUnitEnd + endExtra) - (codeUnitStart - startExtra);
    TRUInteger sizes[COUNT] = { 0 };
    void *pointers[COUNT] = { NULL };
    GlyphRunRef glyphRun;

    sizes[RUN] = sizeof(GlyphRun);
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

static void SetPaint(GlyphRunRef glyphRun, const GlyphRunPaint *paint)
{
    if (paint) {
        glyphRun->hasForegroundColor = paint->hasForegroundColor;
        glyphRun->foregroundColor = paint->foregroundColor;
        glyphRun->userData = paint->userData;
    }
}

TR_INTERNAL GlyphRunRef GlyphRunCreate(TextRunRef textRun, TRUInteger start, TRUInteger end,
    const GlyphRunPaint *paint)
{
    TRUInteger startExtra = 0;
    TRUInteger endExtra = 0;
    TRUInteger glyphStart = 0;
    TRUInteger glyphEnd = 1;
    GlyphRunRef glyphRun;

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

TR_INTERNAL GlyphRunRef GlyphRunCreateCopy(GlyphRunRef source)
{
    GlyphRunRef glyphRun = AllocateGlyphRun(source->textRun, source->codeUnitStart,
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

TR_INTERNAL GlyphRunRef GlyphRunCreateJustified(GlyphRunRef source, const TRFloat *advances)
{
    GlyphRunRef glyphRun = AllocateGlyphRun(source->textRun, source->codeUnitStart,
        source->codeUnitEnd, source->startExtra, source->endExtra, TRTrue, source->glyphStart,
        source->glyphCount);

    if (glyphRun) {
        TRUInteger clusterCount = (source->codeUnitEnd + source->endExtra)
                                - (source->codeUnitStart - source->startExtra);
        TRUInteger length = source->codeUnitEnd - source->codeUnitStart;
        TRBoolean isRTL = GlyphRunIsRTL(source);
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

TR_INTERNAL const TRFloat *GlyphRunGetAdvances(GlyphRunRef glyphRun)
{
    const TRFloat *advances = glyphRun->justifiedAdvances;

    if (!advances) {
        advances = glyphRun->textRun->glyphAdvances + glyphRun->glyphStart;
    }

    return advances;
}

TR_INTERNAL TRBoolean GlyphRunIsRTL(GlyphRunRef glyphRun)
{
    return TextRunIsRTL(glyphRun->textRun);
}

TR_INTERNAL TRUInteger GlyphRunGetActualStart(GlyphRunRef glyphRun)
{
    return glyphRun->codeUnitStart - glyphRun->startExtra;
}

TR_INTERNAL TRFloat GlyphRunGetDistanceInRange(GlyphRunRef glyphRun, TRUInteger start,
    TRUInteger end)
{
    TRUInteger actualStart = GlyphRunGetActualStart(glyphRun);

    return CaretUtilsGetDistance(glyphRun->caretEdges, GlyphRunIsRTL(glyphRun),
        start - actualStart, end - actualStart);
}

TRRange TRGlyphRunGetCodeUnitRange(TRGlyphRunRef run)
{
    TRRange range;

    range.index = run->codeUnitStart;
    range.length = run->codeUnitEnd - run->codeUnitStart;

    return range;
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
    return GlyphRunGetDistanceInRange((GlyphRunRef)run, run->codeUnitStart, run->codeUnitEnd);
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
    return GlyphRunGetAdvances((GlyphRunRef)run);
}

const TRUInteger *TRGlyphRunGetClusterMapPtr(TRGlyphRunRef run)
{
    return run->clusterMap;
}

TRUInteger TRGlyphRunGetClusterMapCount(TRGlyphRunRef run)
{
    return (run->codeUnitEnd + run->endExtra) - (run->codeUnitStart - run->startExtra);
}

TRUInteger TRGlyphRunGetClusterStart(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run. */
    TRAssert(index >= run->codeUnitStart && index < run->codeUnitEnd);

    return TextRunGetClusterStart(run->textRun, index);
}

TRUInteger TRGlyphRunGetClusterEnd(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run. */
    TRAssert(index >= run->codeUnitStart && index < run->codeUnitEnd);

    return TextRunGetClusterEnd(run->textRun, index);
}

TRUInteger TRGlyphRunGetLeadingGlyphIndex(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run. */
    TRAssert(index >= run->codeUnitStart && index < run->codeUnitEnd);

    return TextRunGetLeadingGlyphIndex(run->textRun, index) - run->glyphStart;
}

TRUInteger TRGlyphRunGetTrailingGlyphIndex(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run. */
    TRAssert(index >= run->codeUnitStart && index < run->codeUnitEnd);

    return TextRunGetTrailingGlyphIndex(run->textRun, index) - run->glyphStart;
}

TRFloat TRGlyphRunGetDistance(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run, or its end. */
    TRAssert(index >= run->codeUnitStart && index <= run->codeUnitEnd);

    return run->caretEdges[index - GlyphRunGetActualStart((GlyphRunRef)run)];
}

TRFloat TRGlyphRunGetClusterDistance(TRGlyphRunRef run, TRUInteger index)
{
    /* The index MUST be within the run, including its start and end extras, or the end of them. */
    TRAssert(index >= run->codeUnitStart - run->startExtra
          && index <= run->codeUnitEnd + run->endExtra);

    return run->caretEdges[index - GlyphRunGetActualStart((GlyphRunRef)run)];
}

TRUInteger TRGlyphRunGetIndexOfCodeUnit(TRGlyphRunRef run, TRFloat distance)
{
    TRUInteger actualStart = GlyphRunGetActualStart((GlyphRunRef)run);
    TRUInteger first = run->codeUnitStart - actualStart;
    TRUInteger last = run->codeUnitEnd - actualStart;

    return actualStart + CaretUtilsGetIndexOfEdge(run->caretEdges,
        GlyphRunIsRTL((GlyphRunRef)run), distance, first, last);
}

TRRect TRGlyphRunGetBoundingBox(TRGlyphRunRef run, TRRange glyphRange, TRRendererRef renderer)
{
    TextRunRef textRun = run->textRun;
    TRRect box;

    /* The range MUST be within the glyphs of the run. */
    TRAssert(glyphRange.index + glyphRange.length <= run->glyphCount);

    if (textRun->kind == TextRunKindReplacement) {
        box.origin.x = 0.0f;
        box.origin.y = 0.0f;
        box.size.width = TRGlyphRunGetWidth(run);
        box.size.height = TRGlyphRunGetHeight(run);
    } else {
        TRRendererSetTypeface(renderer, textRun->typeface);
        TRRendererSetTypeSize(renderer, textRun->typeSize);
        TRRendererSetScaleX(renderer, textRun->scaleX);
        TRRendererSetScaleY(renderer, textRun->scaleY);
        TRRendererSetWritingDirection(renderer, textRun->writingDirection);

        box = TRRendererGetRunBoundingBox(renderer,
            TRGlyphRunGetGlyphIDsPtr(run) + glyphRange.index,
            TRGlyphRunGetGlyphOffsetsPtr(run) + glyphRange.index,
            GlyphRunGetAdvances((GlyphRunRef)run) + glyphRange.index, glyphRange.length);
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
