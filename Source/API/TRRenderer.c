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

#include <ft2build.h>
#include FT_STROKER_H

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>

#include <API/TRBase.h>
#include <API/TRGlyphCache.h>
#include <API/TRGlyphImage.h>
#include <API/TRTypeface.h>
#include <Core/Object.h>

#include "TRRenderer.h"

static void FinalizeRenderer(ObjectRef object)
{
    TRRenderer *renderer = object;

    if (renderer->cache) {
        TRGlyphCacheRelease(renderer->cache);
    }
    if (renderer->typeface) {
        TRTypefaceRelease(renderer->typeface);
    }
}

/* Converts a value to fixed point with the given number of fractional bits, rounding it. */
static TRInt32 ToFixed(TRFloat value, TRFloat unit)
{
    return (TRInt32)((value * unit) + 0.5f);
}

static TRInt32 ToPixelSize(const TRRenderer *renderer, TRFloat scale)
{
    TRFloat size = renderer->typeSize * scale * renderer->renderScale;

    return (size > 0.0f ? ToFixed(size, 64.0f) : 0);
}

static TRGlyphCacheRef GetCache(const TRRenderer *renderer)
{
    return (renderer->cache ? renderer->cache : TRGlyphCacheGetDefault());
}

static void SetupDataKey(const TRRenderer *renderer, GlyphDataKey *key)
{
    key->typeface = renderer->typeface;
    key->pixelWidth = ToPixelSize(renderer, renderer->scaleX);
    key->pixelHeight = ToPixelSize(renderer, renderer->scaleY);
    key->skewX = ToFixed(renderer->skewX, 65536.0f);
}

/* Rounds half up, which does not depend on the sign as the truncation of a cast does. */
static TRFloat RoundPixel(TRFloat value)
{
    return (TRFloat)floor(value + 0.5f);
}

/*
 * The position of the glyph at an index in pixels. The pen is moved before the glyph in the
 * reverse mode of right-to-left runs, and after it otherwise. The caller keeps the pen and the
 * total advance, which MUST start from zero.
 */
typedef struct _RunPen {
    TRBoolean isReverse;
    TRFloat penX;
    TRFloat totalAdvance;
} RunPen;

static void BeginGlyph(const TRRenderer *renderer, RunPen *pen, const TRPoint *offsets,
    const TRFloat *advances, TRUInteger index, TRFloat *outOffsetX, TRFloat *outOffsetY,
    TRFloat *outAdvance)
{
    TRFloat scale = renderer->renderScale;

    *outOffsetX = offsets[index].x * scale;
    *outOffsetY = offsets[index].y * scale;
    *outAdvance = advances[index] * scale;

    if (pen->isReverse) {
        pen->penX -= *outAdvance;
    }
}

static void EndGlyph(RunPen *pen, TRFloat advance)
{
    if (!pen->isReverse) {
        pen->penX += advance;
    }

    pen->totalAdvance += advance;
}

static void SetupPen(const TRRenderer *renderer, RunPen *pen)
{
    pen->isReverse = (renderer->writingDirection == TRWritingDirectionRightToLeft);
    pen->penX = 0.0f;
    pen->totalAdvance = 0.0f;
}

TRRendererRef TRRendererCreate(void)
{
    const TRUInteger size = sizeof(TRRenderer);
    void *pointer = NULL;
    TRRenderer *renderer;

    renderer = ObjectCreate(&size, 1, &pointer, FinalizeRenderer);

    if (renderer) {
        renderer->cache = NULL;
        renderer->typeface = NULL;
        renderer->typeSize = 16.0f;
        renderer->scaleX = 1.0f;
        renderer->scaleY = 1.0f;
        renderer->skewX = 0.0f;
        renderer->renderScale = 1.0f;
        renderer->writingDirection = TRWritingDirectionLeftToRight;
        renderer->foregroundColor = TRColorMake(0xFF, 0x00, 0x00, 0x00);
        renderer->strokeWidth = 1.0f;
        renderer->strokeCap = TRStrokeCapButt;
        renderer->strokeJoin = TRStrokeJoinRound;
        renderer->strokeMiter = 1.0f;
    }

    return renderer;
}

void TRRendererSetGlyphCache(TRRendererRef renderer, TRGlyphCacheRef cache)
{
    if (cache) {
        TRGlyphCacheRetain(cache);
    }
    if (renderer->cache) {
        TRGlyphCacheRelease(renderer->cache);
    }

    renderer->cache = cache;
}

void TRRendererSetTypeface(TRRendererRef renderer, TRTypefaceRef typeface)
{
    if (typeface) {
        TRTypefaceRetain(typeface);
    }
    if (renderer->typeface) {
        TRTypefaceRelease(renderer->typeface);
    }

    renderer->typeface = typeface;
}

void TRRendererSetTypeSize(TRRendererRef renderer, TRFloat typeSize)
{
    renderer->typeSize = typeSize;
}

void TRRendererSetScaleX(TRRendererRef renderer, TRFloat scaleX)
{
    renderer->scaleX = scaleX;
}

void TRRendererSetScaleY(TRRendererRef renderer, TRFloat scaleY)
{
    renderer->scaleY = scaleY;
}

void TRRendererSetSkewX(TRRendererRef renderer, TRFloat skewX)
{
    renderer->skewX = skewX;
}

void TRRendererSetRenderScale(TRRendererRef renderer, TRFloat renderScale)
{
    renderer->renderScale = renderScale;
}

void TRRendererSetWritingDirection(TRRendererRef renderer, TRWritingDirection writingDirection)
{
    renderer->writingDirection = writingDirection;
}

void TRRendererSetForegroundColor(TRRendererRef renderer, TRColor foregroundColor)
{
    renderer->foregroundColor = foregroundColor;
}

void TRRendererSetStrokeWidth(TRRendererRef renderer, TRFloat strokeWidth)
{
    renderer->strokeWidth = strokeWidth;
}

void TRRendererSetStrokeCap(TRRendererRef renderer, TRStrokeCap strokeCap)
{
    renderer->strokeCap = strokeCap;
}

void TRRendererSetStrokeJoin(TRRendererRef renderer, TRStrokeJoin strokeJoin)
{
    renderer->strokeJoin = strokeJoin;
}

void TRRendererSetStrokeMiter(TRRendererRef renderer, TRFloat strokeMiter)
{
    renderer->strokeMiter = strokeMiter;
}

TRBoolean TRRendererIsRenderable(TRRendererRef renderer)
{
    /* The least size that FreeType can render is one pixel in each direction. */
    return (ToPixelSize(renderer, renderer->scaleX) >= 64
            && ToPixelSize(renderer, renderer->scaleY) >= 64);
}

TRGlyphImageRef TRRendererGetGlyphImage(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef glyphImage = NULL;

    if (renderer->typeface) {
        GlyphDataKey key;

        SetupDataKey(renderer, &key);

        glyphImage = TRGlyphCacheGetImage(GetCache(renderer), &key, glyphID,
            renderer->foregroundColor);
    }

    return glyphImage;
}

TRGlyphImageRef TRRendererGetStrokeImage(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef strokeImage = NULL;

    if (renderer->typeface) {
        TRFloat radius = (renderer->strokeWidth > 0.0f ? renderer->strokeWidth / 2.0f : 0.0f);
        TRFloat miter = (renderer->strokeMiter > 0.0f ? renderer->strokeMiter : 0.0f);
        GlyphDataKey key;
        GlyphStrokeKey strokeKey;

        SetupDataKey(renderer, &key);

        strokeKey.lineRadius = ToFixed(radius, 64.0f);
        strokeKey.lineCap = renderer->strokeCap;
        strokeKey.lineJoin = renderer->strokeJoin;
        strokeKey.miterLimit = ToFixed(miter, 65536.0f);

        strokeImage = TRGlyphCacheGetStrokeImage(GetCache(renderer), &key, &strokeKey, glyphID);
    }

    return strokeImage;
}

TRPathRef TRRendererGetGlyphPath(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRPathRef glyphPath = NULL;

    if (renderer->typeface) {
        GlyphDataKey key;

        SetupDataKey(renderer, &key);

        glyphPath = TRGlyphCacheGetPath(GetCache(renderer), &key, glyphID);
    }

    return glyphPath;
}

TRRect TRRendererGetGlyphBoundingBox(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, glyphID);
    TRRect box;

    box.origin.x = 0.0f;
    box.origin.y = 0.0f;
    box.size.width = 0.0f;
    box.size.height = 0.0f;

    if (image) {
        TRFloat scale = renderer->renderScale;

        box.origin.x = (TRFloat)TRGlyphImageGetLeft(image) / scale;
        box.origin.y = (TRFloat)(-TRGlyphImageGetTop(image)) / scale;
        box.size.width = (TRFloat)TRGlyphImageGetWidth(image) / scale;
        box.size.height = (TRFloat)TRGlyphImageGetHeight(image) / scale;

        TRGlyphImageRelease(image);
    }

    return box;
}

TRRect TRRendererGetRunBoundingBox(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count)
{
    TRFloat minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    TRBoolean hasBox = TRFalse;
    RunPen pen;
    TRRect box;
    TRUInteger index;

    SetupPen(renderer, &pen);

    for (index = 0; index < count; index++) {
        TRFloat offsetX, offsetY, advance;
        TRGlyphImageRef image;

        BeginGlyph(renderer, &pen, offsets, advances, index, &offsetX, &offsetY, &advance);

        image = TRRendererGetGlyphImage(renderer, glyphIDs[index]);
        if (image) {
            TRFloat left = RoundPixel(pen.penX + offsetX + (TRFloat)TRGlyphImageGetLeft(image));
            TRFloat top = RoundPixel(-offsetY - (TRFloat)TRGlyphImageGetTop(image));
            TRFloat right = left + (TRFloat)TRGlyphImageGetWidth(image);
            TRFloat bottom = top + (TRFloat)TRGlyphImageGetHeight(image);

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

            TRGlyphImageRelease(image);
        }

        EndGlyph(&pen, advance);
    }

    box.origin.x = 0.0f;
    box.origin.y = 0.0f;
    box.size.width = 0.0f;
    box.size.height = 0.0f;

    if (hasBox) {
        /* The glyphs of a right-to-left run are placed from the end of its advance. */
        TRFloat shift = (pen.isReverse ? (TRFloat)ceil(pen.totalAdvance) : 0.0f);
        TRFloat scale = renderer->renderScale;

        box.origin.x = (minX + shift) / scale;
        box.origin.y = minY / scale;
        box.size.width = (maxX - minX) / scale;
        box.size.height = (maxY - minY) / scale;
    }

    return box;
}

void TRRendererGetGlyphPlacements(TRRendererRef renderer, TRGlyphImageKind kind,
    const TRGlyphID *glyphIDs, const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    TRGlyphPlacement *placements)
{
    RunPen pen;
    TRUInteger index;

    SetupPen(renderer, &pen);

    for (index = 0; index < count; index++) {
        TRFloat offsetX, offsetY, advance;
        TRGlyphImageRef image;

        BeginGlyph(renderer, &pen, offsets, advances, index, &offsetX, &offsetY, &advance);

        image = (kind == TRGlyphImageKindStroke
                 ? TRRendererGetStrokeImage(renderer, glyphIDs[index])
                 : TRRendererGetGlyphImage(renderer, glyphIDs[index]));

        placements[index].image = image;
        placements[index].origin.x = 0.0f;
        placements[index].origin.y = 0.0f;

        if (image) {
            placements[index].origin.x = RoundPixel(pen.penX + offsetX
                + (TRFloat)TRGlyphImageGetLeft(image));
            placements[index].origin.y = RoundPixel(-offsetY - (TRFloat)TRGlyphImageGetTop(image));
        }

        EndGlyph(&pen, advance);
    }
}

void TRRendererReleaseGlyphPlacements(TRGlyphPlacement *placements, TRUInteger count)
{
    TRUInteger index;

    for (index = 0; index < count; index++) {
        if (placements[index].image) {
            TRGlyphImageRelease(placements[index].image);
            placements[index].image = NULL;
        }
    }
}

void TRRendererEnumerateGlyphPaths(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    const TRPathCallbacks *callbacks, void *userData)
{
    TRBoolean isReverse = (renderer->writingDirection == TRWritingDirectionRightToLeft);
    TRFloat inverseScale = 1.0f / renderer->renderScale;
    TRFloat penX = 0.0f;
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TRPathRef path;

        if (isReverse) {
            penX -= advances[index];
        }

        path = TRRendererGetGlyphPath(renderer, glyphIDs[index]);
        if (path) {
            TRAffineTransform transform;

            /* The path is in pixels, while the position of the glyph is in the user space. */
            transform.a = inverseScale;
            transform.b = 0.0f;
            transform.c = 0.0f;
            transform.d = inverseScale;
            transform.tx = penX + offsets[index].x;
            transform.ty = -offsets[index].y;

            TRPathEnumerate(path, &transform, callbacks, userData);
            TRPathRelease(path);
        }

        if (!isReverse) {
            penX += advances[index];
        }
    }
}

TRRendererRef TRRendererRetain(TRRendererRef renderer)
{
    return ObjectRetain((ObjectRef)renderer);
}

void TRRendererRelease(TRRendererRef renderer)
{
    ObjectRelease((ObjectRef)renderer);
}
