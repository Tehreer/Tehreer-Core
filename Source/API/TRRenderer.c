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
#include <Font/FaceMetadata.h>
#include <Graphics/RenderableFace.h>

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

/*
 * Converts a value to fixed point with the given unit, rounding it. A value that is out of range
 * is saturated, and one that is not a number is zero, as a conversion of them is not defined.
 */
static TRInt32 ToFixed(TRFloat value, TRFloat unit)
{
    TRFloat scaled = (value * unit) + 0.5f;
    TRInt32 fixed = 0;

    if (scaled >= 2147483648.0f) {
        fixed = INT32_MAX;
    } else if (scaled <= -2147483648.0f) {
        fixed = INT32_MIN;
    } else if (scaled == scaled) {
        fixed = (TRInt32)scaled;
    }

    return fixed;
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

/*
 * The key refers to the typeface without retaining it. The renderer keeps the typeface alive during
 * the lookup, and the cache retains it for each entry that it creates.
 *
 * A face without outlines only has the images of its strikes, so the key is the size of the strike
 * that is nearest to the size that was asked for, and the images are kept at that size. The scale
 * says how much they have to be scaled to match the size that was asked for. It is one for any
 * other face.
 */
static void SetupDataKey(const TRRenderer *renderer, GlyphDataKey *key, TRFloat *scaleX,
    TRFloat *scaleY)
{
    FaceMetadataRef metadata = renderer->typeface->renderableFace->metadata;

    key->typeface = renderer->typeface;
    key->pixelWidth = ToPixelSize(renderer, renderer->scaleX);
    key->pixelHeight = ToPixelSize(renderer, renderer->scaleY);
    key->skewX = ToFixed(renderer->skewX, 65536.0f);

    *scaleX = 1.0f;
    *scaleY = 1.0f;

    if (!metadata->isScalable && metadata->bitmapStrikeCount > 0 && key->pixelWidth > 0
            && key->pixelHeight > 0) {
        TRUInteger index = FaceMetadataFindBitmapStrike(metadata, key->pixelHeight);
        const TRBitmapStrike *strike = &metadata->bitmapStrikesPtr[index];
        TRInt32 strikeWidth = ToFixed(strike->pixelWidth, 64.0f);
        TRInt32 strikeHeight = ToFixed(strike->pixelHeight, 64.0f);

        if (strikeWidth > 0 && strikeHeight > 0) {
            *scaleX = (TRFloat)key->pixelWidth / (TRFloat)strikeWidth;
            *scaleY = (TRFloat)key->pixelHeight / (TRFloat)strikeHeight;

            key->pixelWidth = strikeWidth;
            key->pixelHeight = strikeHeight;
            key->skewX = 0;
        }
    }
}

/* Gets the scale of the images that the renderer finds, which is one if it has no typeface. */
static void GetImageScale(const TRRenderer *renderer, TRFloat *scaleX, TRFloat *scaleY)
{
    *scaleX = 1.0f;
    *scaleY = 1.0f;

    if (renderer->typeface) {
        GlyphDataKey key;

        SetupDataKey(renderer, &key, scaleX, scaleY);
    }
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
        renderer->strokeRadius = 0.5f;
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

void TRRendererSetStrokeRadius(TRRendererRef renderer, TRFloat strokeRadius)
{
    renderer->strokeRadius = strokeRadius;
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

TRGlyphImageRef TRRendererCopyGlyphImage(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef glyphImage = NULL;

    if (renderer->typeface) {
        TRFloat scaleX, scaleY;
        GlyphDataKey key;

        SetupDataKey(renderer, &key, &scaleX, &scaleY);

        glyphImage = TRGlyphCacheCopyImage(GetCache(renderer), &key, glyphID,
            renderer->foregroundColor);
    }

    return glyphImage;
}

TRGlyphImageRef TRRendererCopyStrokeImage(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef strokeImage = NULL;

    if (renderer->typeface) {
        TRFloat radius = (renderer->strokeRadius > 0.0f ? renderer->strokeRadius : 0.0f);
        TRFloat miter = (renderer->strokeMiter > 0.0f ? renderer->strokeMiter : 0.0f);
        TRFloat scaleX, scaleY;
        GlyphDataKey key;
        GlyphStrokeKey strokeKey;

        SetupDataKey(renderer, &key, &scaleX, &scaleY);

        strokeKey.lineRadius = ToFixed(radius, 64.0f);
        strokeKey.lineCap = renderer->strokeCap;
        strokeKey.lineJoin = renderer->strokeJoin;
        strokeKey.miterLimit = ToFixed(miter, 65536.0f);

        strokeImage = TRGlyphCacheCopyStrokeImage(GetCache(renderer), &key, &strokeKey, glyphID);
    }

    return strokeImage;
}

TRPathRef TRRendererCopyGlyphPath(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRPathRef glyphPath = NULL;

    if (renderer->typeface) {
        TRFloat scaleX, scaleY;
        GlyphDataKey key;

        SetupDataKey(renderer, &key, &scaleX, &scaleY);

        glyphPath = TRGlyphCacheCopyPath(GetCache(renderer), &key, glyphID);
    }

    return glyphPath;
}

TRRect TRRendererGetGlyphInkBox(TRRendererRef renderer, TRGlyphID glyphID)
{
    TRGlyphImageRef image = TRRendererCopyGlyphImage(renderer, glyphID);
    TRRect box;

    box.origin.x = 0.0f;
    box.origin.y = 0.0f;
    box.size.width = 0.0f;
    box.size.height = 0.0f;

    if (image) {
        TRFloat scale = renderer->renderScale;
        TRFloat scaleX, scaleY;

        GetImageScale(renderer, &scaleX, &scaleY);

        box.origin.x = ((TRFloat)TRGlyphImageGetLeft(image) * scaleX) / scale;
        box.origin.y = ((TRFloat)(-TRGlyphImageGetTop(image)) * scaleY) / scale;
        box.size.width = ((TRFloat)TRGlyphImageGetWidth(image) * scaleX) / scale;
        box.size.height = ((TRFloat)TRGlyphImageGetHeight(image) * scaleY) / scale;

        TRGlyphImageRelease(image);
    }

    return box;
}

TRRect TRRendererGetRunInkBox(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count)
{
    TRFloat minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    TRBoolean hasBox = TRFalse;
    TRFloat scaleX, scaleY;
    RunPen pen;
    TRRect box;
    TRUInteger index;

    GetImageScale(renderer, &scaleX, &scaleY);
    SetupPen(renderer, &pen);

    for (index = 0; index < count; index++) {
        TRFloat offsetX, offsetY, advance;
        TRGlyphImageRef image;

        BeginGlyph(renderer, &pen, offsets, advances, index, &offsetX, &offsetY, &advance);

        image = TRRendererCopyGlyphImage(renderer, glyphIDs[index]);
        if (image) {
            TRFloat left = RoundPixel(pen.penX + offsetX
                                      + ((TRFloat)TRGlyphImageGetLeft(image) * scaleX));
            TRFloat top = RoundPixel(-offsetY - ((TRFloat)TRGlyphImageGetTop(image) * scaleY));
            TRFloat right = left + ((TRFloat)TRGlyphImageGetWidth(image) * scaleX);
            TRFloat bottom = top + ((TRFloat)TRGlyphImageGetHeight(image) * scaleY);

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

void TRRendererEnumerateGlyphPlacements(TRRendererRef renderer, TRGlyphImageKind kind,
    const TRGlyphID *glyphIDs, const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    TRGlyphPlacementFunc func, void *userData)
{
    TRBoolean shouldStop = TRFalse;
    TRFloat scaleX, scaleY;
    RunPen pen;
    TRUInteger index;

    GetImageScale(renderer, &scaleX, &scaleY);
    SetupPen(renderer, &pen);

    for (index = 0; index < count && !shouldStop; index++) {
        TRFloat offsetX, offsetY, advance;
        TRGlyphImageRef image;

        BeginGlyph(renderer, &pen, offsets, advances, index, &offsetX, &offsetY, &advance);

        image = (kind == TRGlyphImageKindStroke
                 ? TRRendererCopyStrokeImage(renderer, glyphIDs[index])
                 : TRRendererCopyGlyphImage(renderer, glyphIDs[index]));

        if (image) {
            TRPoint origin;

            origin.x = RoundPixel(pen.penX + offsetX
                                  + ((TRFloat)TRGlyphImageGetLeft(image) * scaleX));
            origin.y = RoundPixel(-offsetY - ((TRFloat)TRGlyphImageGetTop(image) * scaleY));

            func(userData, index, image, origin, scaleX, scaleY, &shouldStop);
            TRGlyphImageRelease(image);
        }

        EndGlyph(&pen, advance);
    }
}

void TRRendererEnumerateGlyphPaths(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    const TRPathCallbacks *callbacks, void *userData)
{
    TRBoolean isReverse = (renderer->writingDirection == TRWritingDirectionRightToLeft);
    TRFloat inverseScale = 1.0f / renderer->renderScale;
    TRBoolean isStopped = TRFalse;
    TRFloat penX = 0.0f;
    TRUInteger index;

    for (index = 0; index < count && !isStopped; index++) {
        TRPathRef path;

        if (isReverse) {
            penX -= advances[index];
        }

        path = TRRendererCopyGlyphPath(renderer, glyphIDs[index]);
        if (path) {
            TRAffineTransform transform;

            /* The path is in pixels, while the position of the glyph is in the user space. */
            transform.a = inverseScale;
            transform.b = 0.0f;
            transform.c = 0.0f;
            transform.d = inverseScale;
            transform.tx = penX + offsets[index].x;
            transform.ty = -offsets[index].y;

            isStopped = !TRPathEnumerate(path, &transform, callbacks, userData);
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
