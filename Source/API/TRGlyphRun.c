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

#include <math.h>

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRRenderer.h>
#include <Core/Object.h>
#include <Layout/CaretUtils.h>
#include <Layout/TextRun.h>
#include <Text/CaretEdgesBuilder.h>

#include "TRGlyphRun.h"

/* How far the clip of a split cluster is spread beyond the ink of the glyphs, in pixels. */
#define CLIP_SLACK  2.0f

/* What a renderer had before it was set up for a run. */
typedef struct _RendererState {
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat scaleX;
    TRFloat scaleY;
    TRWritingDirection writingDirection;
    TRColor foregroundColor;
} RendererState;

/*
 * The extent of the clusters of a run, which are the glyphs that it draws, as distances from the
 * left edge of the run, and the distances of the start and the end of the run itself. The clusters
 * at its ends can reach out of the run. The caret edges are read once, by `GetGlyphExtent()`.
 */
typedef struct _GlyphExtent {
    TRFloat left;
    TRFloat right;
    TRFloat startCut;
    TRFloat endCut;
} GlyphExtent;

/* The glyphs of a cluster, relative to the run. */
typedef struct _GlyphSpan {
    TRUInteger start;
    TRUInteger end;
} GlyphSpan;

/*
 * The glyphs that belong to a cluster which is split by the start or the end of the run are
 * clipped to the part of the cluster that is in the run, so that the other part is left to the run
 * that has it. A glyph that is in both clusters gets the intersection of the clips. The rectangles
 * are in pixels.
 */
typedef struct _GlyphClips {
    TRBoolean isStartCut;
    TRBoolean isEndCut;
    GlyphSpan startGlyphs;
    GlyphSpan endGlyphs;
    TRRect startClip;
    TRRect endClip;
    TRRect bothClip;
} GlyphClips;

/* What the functions that receive the glyphs of a pass need to pass them on. */
typedef struct _GlyphPass {
    const TRDrawCallbacks *callbacks;
    void *userData;
    const GlyphClips *clips;
    TRPoint pixelPen;
    TRPoint exactPen;
    TRDrawStyle style;
    TRColor color;
} GlyphPass;

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
        glyphRun->flags = 0;
        glyphRun->foregroundColor = 0;
        glyphRun->backgroundColor = 0;
        glyphRun->decorationColor = 0;
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
        glyphRun->flags = paint->flags;
        glyphRun->foregroundColor = paint->foregroundColor;
        glyphRun->backgroundColor = paint->backgroundColor;
        glyphRun->decorationColor = paint->decorationColor;
        glyphRun->userData = paint->userData;
    }
}

static void CopyPaint(TRGlyphRun *glyphRun, TRGlyphRunRef source)
{
    glyphRun->flags = source->flags;
    glyphRun->foregroundColor = source->foregroundColor;
    glyphRun->backgroundColor = source->backgroundColor;
    glyphRun->decorationColor = source->decorationColor;
    glyphRun->userData = source->userData;
}

static void SaveRendererState(TRRendererRef renderer, RendererState *state)
{
    state->typeface = renderer->typeface;
    state->typeSize = renderer->typeSize;
    state->scaleX = renderer->scaleX;
    state->scaleY = renderer->scaleY;
    state->writingDirection = renderer->writingDirection;
    state->foregroundColor = renderer->foregroundColor;

    if (state->typeface) {
        TRTypefaceRetain(state->typeface);
    }
}

static void ApplyRunToRenderer(TRRendererRef renderer, TextRunRef textRun)
{
    TRRendererSetTypeface(renderer, textRun->typeface);
    TRRendererSetTypeSize(renderer, textRun->typeSize);
    TRRendererSetScaleX(renderer, textRun->scaleX);
    TRRendererSetScaleY(renderer, textRun->scaleY);
    TRRendererSetWritingDirection(renderer, textRun->writingDirection);
}

static void RestoreRendererState(TRRendererRef renderer, const RendererState *state)
{
    TRRendererSetTypeface(renderer, state->typeface);
    TRRendererSetTypeSize(renderer, state->typeSize);
    TRRendererSetScaleX(renderer, state->scaleX);
    TRRendererSetScaleY(renderer, state->scaleY);
    TRRendererSetWritingDirection(renderer, state->writingDirection);
    TRRendererSetForegroundColor(renderer, state->foregroundColor);

    if (state->typeface) {
        TRTypefaceRelease(state->typeface);
    }
}

/* Makes a rectangle from the user space edges, which are rounded to whole pixels. */
static TRRect MakePixelRect(TRFloat scale, TRFloat left, TRFloat top, TRFloat right,
    TRFloat bottom)
{
    TRFloat pixelLeft = TRRendererRoundPixel(left * scale);
    TRFloat pixelTop = TRRendererRoundPixel(top * scale);
    TRFloat pixelRight = TRRendererRoundPixel(right * scale);
    TRFloat pixelBottom = TRRendererRoundPixel(bottom * scale);
    TRRect rect;

    rect.origin.x = pixelLeft;
    rect.origin.y = pixelTop;
    rect.size.width = pixelRight - pixelLeft;
    rect.size.height = pixelBottom - pixelTop;

    return rect;
}

static TRRect MakeClipRect(TRFloat firstX, TRFloat secondX, TRFloat top, TRFloat bottom)
{
    TRFloat left = NumberMin(firstX, secondX);
    TRFloat right = NumberMax(firstX, secondX);
    TRRect rect;

    rect.origin.x = left;
    rect.origin.y = top;
    rect.size.width = right - left;
    rect.size.height = bottom - top;

    return rect;
}

static TRRect IntersectClipRects(TRRect first, TRRect second)
{
    TRFloat left = NumberMax(first.origin.x, second.origin.x);
    TRFloat right = NumberMin(first.origin.x + first.size.width,
                              second.origin.x + second.size.width);
    TRRect rect;

    rect.origin.x = left;
    rect.origin.y = first.origin.y;
    rect.size.width = NumberMax(right - left, 0.0f);
    rect.size.height = first.size.height;

    return rect;
}

static GlyphExtent GetGlyphExtent(TRGlyphRunRef glyphRun)
{
    TRFloat startEdge = TRGlyphRunGetCaretEdge(glyphRun, TRGlyphRunGetActualStart(glyphRun));
    TRFloat endEdge = TRGlyphRunGetCaretEdge(glyphRun, glyphRun->codeUnitEnd + glyphRun->endExtra);
    GlyphExtent extent;

    extent.left = NumberMin(startEdge, endEdge);
    extent.right = NumberMax(startEdge, endEdge);
    extent.startCut = TRGlyphRunGetCaretEdge(glyphRun, glyphRun->codeUnitStart);
    extent.endCut = TRGlyphRunGetCaretEdge(glyphRun, glyphRun->codeUnitEnd);

    return extent;
}

/* The glyphs of the cluster that a code unit of the run is in. */
static GlyphSpan GetClusterGlyphs(TRGlyphRunRef glyphRun, TRUInteger codeUnitIndex)
{
    TextRunRef textRun = glyphRun->textRun;
    TRUInteger clusterStart = TextRunGetClusterStart(textRun, codeUnitIndex);
    TRUInteger clusterEnd = TextRunGetClusterEnd(textRun, codeUnitIndex);
    GlyphSpan glyphs;
    TRUInteger textGlyphStart;
    TRUInteger textGlyphEnd;

    TextRunGetGlyphRange(textRun, clusterStart, clusterEnd, &textGlyphStart, &textGlyphEnd);

    /* The glyphs of a cluster of the run MUST be among the glyphs of the run. */
    TRAssert(textGlyphStart >= glyphRun->glyphStart
          && textGlyphEnd <= glyphRun->glyphStart + glyphRun->glyphCount);

    glyphs.start = textGlyphStart - glyphRun->glyphStart;
    glyphs.end = textGlyphEnd - glyphRun->glyphStart;

    return glyphs;
}

/*
 * Plans the clips of the glyphs of the clusters that are split. Their free sides reach beyond the
 * ink box of the run and its metrics, so that nothing but the part of the other run is cut. The
 * renderer MUST be set up for the run, as the ink box is measured with it.
 */
static void PlanGlyphClips(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRPoint baseline,
    GlyphExtent extent, GlyphClips *clips)
{
    TRFloat scale = renderer->renderScale;
    TRRect inkBox = TRGlyphRunGetInkBox(glyphRun, renderer);
    TRFloat inkLeft = baseline.x + extent.left + inkBox.origin.x;
    TRFloat inkRight = inkLeft + inkBox.size.width;
    TRFloat inkTop = baseline.y + inkBox.origin.y;
    TRFloat inkBottom = inkTop + inkBox.size.height;
    TRFloat ascent = TRGlyphRunGetAscent(glyphRun);
    TRFloat descent = TRGlyphRunGetDescent(glyphRun);
    TRFloat leading = TRGlyphRunGetLeading(glyphRun);
    TRFloat top = (TRFloat)floor(NumberMin(baseline.y - ascent, inkTop) * scale) - CLIP_SLACK;
    TRFloat bottom = (TRFloat)ceil(NumberMax(baseline.y + descent + leading, inkBottom) * scale)
                   + CLIP_SLACK;
    TRFloat farLeft = (TRFloat)floor(inkLeft * scale) - CLIP_SLACK;
    TRFloat farRight = (TRFloat)ceil(inkRight * scale) + CLIP_SLACK;
    TRFloat startCut = TRRendererRoundPixel((baseline.x + extent.startCut) * scale);
    TRFloat endCut = TRRendererRoundPixel((baseline.x + extent.endCut) * scale);
    TRBoolean isStartAtLeft = (extent.startCut <= extent.endCut);

    /* The run lies from its start toward its end, so the free side of a cut is the other one. */
    clips->startClip = MakeClipRect(startCut, (isStartAtLeft ? farRight : farLeft), top, bottom);
    clips->endClip = MakeClipRect(endCut, (isStartAtLeft ? farLeft : farRight), top, bottom);
    clips->bothClip = IntersectClipRects(clips->startClip, clips->endClip);
}

static void PlanGlyphCuts(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRPoint baseline,
    GlyphExtent extent, GlyphClips *clips)
{
    clips->isStartCut = (glyphRun->startExtra > 0);
    clips->isEndCut = (glyphRun->endExtra > 0);
    clips->startGlyphs.start = 0;
    clips->startGlyphs.end = 0;
    clips->endGlyphs.start = 0;
    clips->endGlyphs.end = 0;

    if (clips->isStartCut || clips->isEndCut) {
        if (clips->isStartCut) {
            clips->startGlyphs = GetClusterGlyphs(glyphRun, glyphRun->codeUnitStart);
        }
        if (clips->isEndCut) {
            clips->endGlyphs = GetClusterGlyphs(glyphRun, glyphRun->codeUnitEnd - 1);
        }

        PlanGlyphClips(glyphRun, renderer, baseline, extent, clips);
    }
}

static const TRRect *SelectGlyphClip(const GlyphClips *clips, TRUInteger glyphIndex)
{
    const TRRect *clip = NULL;
    TRBoolean isInStart = (clips->isStartCut && glyphIndex >= clips->startGlyphs.start
                           && glyphIndex < clips->startGlyphs.end);
    TRBoolean isInEnd = (clips->isEndCut && glyphIndex >= clips->endGlyphs.start
                         && glyphIndex < clips->endGlyphs.end);

    if (isInStart && isInEnd) {
        clip = &clips->bothClip;
    } else if (isInStart) {
        clip = &clips->startClip;
    } else if (isInEnd) {
        clip = &clips->endClip;
    }

    return clip;
}

static void PassGlyphImage(void *userData, TRUInteger glyphIndex, TRGlyphImageRef image,
    TRPoint origin, TRFloat scaleX, TRFloat scaleY, TRBoolean *stop)
{
    const GlyphPass *pass = userData;
    const TRRect *clip = SelectGlyphClip(pass->clips, glyphIndex);
    TRPoint imageOrigin;

    (void)stop;

    imageOrigin.x = pass->pixelPen.x + origin.x;
    imageOrigin.y = pass->pixelPen.y + origin.y;

    pass->callbacks->drawGlyphImage(pass->userData, image, imageOrigin, scaleX, scaleY,
        pass->color, clip);
}

static void PassGlyphPath(void *userData, TRUInteger glyphIndex, TRPathRef path, TRPoint origin,
    TRBoolean *stop)
{
    const GlyphPass *pass = userData;
    const TRRect *clip = SelectGlyphClip(pass->clips, glyphIndex);
    TRPoint pathOrigin;

    (void)stop;

    pathOrigin.x = pass->exactPen.x + origin.x;
    pathOrigin.y = pass->exactPen.y + origin.y;

    pass->callbacks->drawGlyphPath(pass->userData, path, pathOrigin, pass->style, pass->color,
        clip);
}

/* Passes the glyphs of the run once, as images if the callbacks take them, or else as paths. */
static void DrawGlyphPass(TRGlyphRunRef glyphRun, TRRendererRef renderer, GlyphPass *pass,
    TRDrawStyle style, TRColor color)
{
    const TRGlyphID *glyphIDs = TRGlyphRunGetGlyphIDsPtr(glyphRun);
    const TRPoint *offsets = TRGlyphRunGetGlyphOffsetsPtr(glyphRun);
    const TRFloat *advances = TRGlyphRunGetAdvances(glyphRun);

    pass->style = style;
    pass->color = color;

    if (pass->callbacks->drawGlyphImage) {
        TRGlyphImageKind kind = (style == TRDrawStyleStroke ? TRGlyphImageKindStroke
                                                            : TRGlyphImageKindFill);

        TRRendererEnumerateGlyphPlacements(renderer, kind, glyphIDs, offsets, advances,
            glyphRun->glyphCount, PassGlyphImage, pass);
    } else {
        TRRendererEnumeratePathPlacements(renderer, glyphIDs, offsets, advances,
            glyphRun->glyphCount, PassGlyphPath, pass);
    }
}

/* The glyphs are drawn from the start of their clusters, so those of split clusters are whole. */
static void DrawGlyphs(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRPoint baseline)
{
    const TRDrawCallbacks *callbacks = &renderer->drawCallbacks;
    TRBoolean hasOutput = (callbacks->drawGlyphImage || callbacks->drawGlyphPath);

    if (hasOutput && glyphRun->glyphCount > 0) {
        TRFloat scale = renderer->renderScale;
        TRDrawStyle style = renderer->drawStyle;
        TRColor strokeColor = renderer->strokeColor;
        TRBoolean hasForeground = ((glyphRun->flags & TRGlyphRunFlagForegroundColor) != 0);
        TRColor foregroundColor = (hasForeground ? glyphRun->foregroundColor
                                                 : renderer->foregroundColor);
        TRBoolean isWrittenRTL = (glyphRun->textRun->writingDirection
                                  == TRWritingDirectionRightToLeft);
        GlyphExtent extent = GetGlyphExtent(glyphRun);
        TRFloat penDistance = (isWrittenRTL ? extent.right : extent.left);
        TRFloat penX = (baseline.x + penDistance) * scale;
        TRFloat penY = baseline.y * scale;
        RendererState state;
        GlyphClips clips;
        GlyphPass pass;

        SaveRendererState(renderer, &state);
        ApplyRunToRenderer(renderer, glyphRun->textRun);
        TRRendererSetForegroundColor(renderer, foregroundColor);

        PlanGlyphCuts(glyphRun, renderer, baseline, extent, &clips);

        pass.callbacks = callbacks;
        pass.userData = renderer->drawUserData;
        pass.clips = &clips;
        pass.pixelPen.x = TRRendererRoundPixel(penX);
        pass.pixelPen.y = TRRendererRoundPixel(penY);
        pass.exactPen.x = penX;
        pass.exactPen.y = penY;

        if (style != TRDrawStyleStroke) {
            DrawGlyphPass(glyphRun, renderer, &pass, TRDrawStyleFill, foregroundColor);
        }
        if (style != TRDrawStyleFill) {
            DrawGlyphPass(glyphRun, renderer, &pass, TRDrawStyleStroke, strokeColor);
        }

        RestoreRendererState(renderer, &state);
    }
}

static void DrawReplacement(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRPoint baseline)
{
    const TRDrawCallbacks *callbacks = &renderer->drawCallbacks;

    if (callbacks->drawReplacement) {
        TRFloat scale = renderer->renderScale;
        TRPoint origin;

        origin.x = TRRendererRoundPixel(baseline.x * scale);
        origin.y = TRRendererRoundPixel(baseline.y * scale);

        callbacks->drawReplacement(renderer->drawUserData, glyphRun, origin);
    }
}

static void DrawBackground(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRFloat left,
    TRFloat top, TRFloat bottom)
{
    const TRDrawCallbacks *callbacks = &renderer->drawCallbacks;

    if ((glyphRun->flags & TRGlyphRunFlagBackgroundColor) != 0 && callbacks->fillRect) {
        TRFloat right = left + TRGlyphRunGetWidth(glyphRun);
        TRRect rect;

        rect = MakePixelRect(renderer->renderScale, left, top, right, bottom);

        if (rect.size.width > 0.0f && rect.size.height > 0.0f) {
            callbacks->fillRect(renderer->drawUserData, rect, glyphRun->backgroundColor);
        }
    }
}

/*
 * Fills a band of a decoration. The position is the distance of its top from the baseline, as
 * `post.underlinePosition` and `OS/2.yStrikeoutPosition` are, and it is in font units with the y
 * axis pointing up. A band that would be thinner than a pixel is a pixel thick.
 */
static void DrawDecorationBand(TRRendererRef renderer, TRColor color, TRFloat left,
    TRFloat right, TRFloat baselineY, TRInt32 position, TRInt32 thickness, TRFloat unitScale)
{
    if (thickness > 0) {
        TRFloat top = baselineY - ((TRFloat)position * unitScale);
        TRFloat bottom = top + ((TRFloat)thickness * unitScale);
        TRRect rect;

        rect = MakePixelRect(renderer->renderScale, left, top, right, bottom);
        rect.size.height = NumberMax(rect.size.height, 1.0f);

        if (rect.size.width > 0.0f) {
            renderer->drawCallbacks.fillRect(renderer->drawUserData, rect, color);
        }
    }
}

static void DrawDecorations(TRGlyphRunRef glyphRun, TRRendererRef renderer, TRPoint baseline)
{
    TextRunRef textRun = glyphRun->textRun;
    TRBoolean isUnderlined = ((glyphRun->flags & TRGlyphRunFlagUnderline) != 0);
    TRBoolean isStruckThrough = ((glyphRun->flags & TRGlyphRunFlagStrikethrough) != 0);
    TRBoolean hasDecoration = (isUnderlined || isStruckThrough);

    if (hasDecoration && renderer->drawCallbacks.fillRect
            && textRun->kind != TextRunKindReplacement && textRun->typeface) {
        TRTypefaceRef typeface = textRun->typeface;
        TRUInt32 unitsPerEM = TRTypefaceGetUnitsPerEM(typeface);
        TRFloat unitScale = (textRun->typeSize * textRun->scaleY) / (TRFloat)unitsPerEM;
        TRFloat right = baseline.x + TRGlyphRunGetWidth(glyphRun);
        TRColor color = renderer->foregroundColor;

        if ((glyphRun->flags & TRGlyphRunFlagDecorationColor) != 0) {
            color = glyphRun->decorationColor;
        } else if ((glyphRun->flags & TRGlyphRunFlagForegroundColor) != 0) {
            color = glyphRun->foregroundColor;
        }

        if (isUnderlined && unitsPerEM > 0) {
            DrawDecorationBand(renderer, color, baseline.x, right, baseline.y,
                TRTypefaceGetUnderlinePosition(typeface),
                (TRInt32)TRTypefaceGetUnderlineThickness(typeface), unitScale);
        }
        if (isStruckThrough && unitsPerEM > 0) {
            DrawDecorationBand(renderer, color, baseline.x, right, baseline.y,
                TRTypefaceGetStrikeoutPosition(typeface),
                (TRInt32)TRTypefaceGetStrikeoutThickness(typeface), unitScale);
        }
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
        CopyPaint(glyphRun, source);
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
        CopyPaint(glyphRun, source);
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

TR_INTERNAL void TRGlyphRunDrawInExtent(TRGlyphRunRef glyphRun, TRRendererRef renderer,
    TRPoint origin, TRFloat top, TRFloat bottom)
{
    DrawBackground(glyphRun, renderer, origin.x, top, bottom);

    if (glyphRun->textRun->kind == TextRunKindReplacement) {
        DrawReplacement(glyphRun, renderer, origin);
    } else {
        DrawGlyphs(glyphRun, renderer, origin);
    }

    DrawDecorations(glyphRun, renderer, origin);
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
    TRBoolean isSet = ((run->flags & TRGlyphRunFlagForegroundColor) != 0);

    if (isSet) {
        *foregroundColor = run->foregroundColor;
    }

    return isSet;
}

TRBoolean TRGlyphRunGetBackgroundColor(TRGlyphRunRef run, TRColor *backgroundColor)
{
    TRBoolean isSet = ((run->flags & TRGlyphRunFlagBackgroundColor) != 0);

    if (isSet) {
        *backgroundColor = run->backgroundColor;
    }

    return isSet;
}

TRBoolean TRGlyphRunHasUnderline(TRGlyphRunRef run)
{
    return ((run->flags & TRGlyphRunFlagUnderline) != 0);
}

TRBoolean TRGlyphRunHasStrikethrough(TRGlyphRunRef run)
{
    return ((run->flags & TRGlyphRunFlagStrikethrough) != 0);
}

TRBoolean TRGlyphRunGetDecorationColor(TRGlyphRunRef run, TRColor *decorationColor)
{
    TRBoolean isSet = ((run->flags & TRGlyphRunFlagDecorationColor) != 0);

    if (isSet) {
        *decorationColor = run->decorationColor;
    }

    return isSet;
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
        RendererState state;

        SaveRendererState(renderer, &state);
        ApplyRunToRenderer(renderer, textRun);

        box = TRRendererGetRunInkBox(renderer, TRGlyphRunGetGlyphIDsPtr(run),
            TRGlyphRunGetGlyphOffsetsPtr(run), TRGlyphRunGetAdvances(run), run->glyphCount);

        RestoreRendererState(renderer, &state);
    }

    return box;
}

void TRGlyphRunDraw(TRGlyphRunRef run, TRRendererRef renderer, TRPoint origin)
{
    if (renderer) {
        TRFloat top = origin.y - TRGlyphRunGetAscent(run);
        TRFloat bottom = origin.y + TRGlyphRunGetDescent(run) + TRGlyphRunGetLeading(run);

        TRGlyphRunDrawInExtent(run, renderer, origin, top, bottom);
    }
}

TRGlyphRunRef TRGlyphRunRetain(TRGlyphRunRef run)
{
    return ObjectRetain((ObjectRef)run);
}

void TRGlyphRunRelease(TRGlyphRunRef run)
{
    ObjectRelease((ObjectRef)run);
}
