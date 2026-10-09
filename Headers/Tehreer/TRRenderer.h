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

#ifndef _TEHREER_RENDERER_H
#define _TEHREER_RENDERER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphCache.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/**
 * The shape at the ends of an open stroked line.
 */
enum {
    TRStrokeCapButt = 0,
    TRStrokeCapRound = 1,
    TRStrokeCapSquare = 2
};
typedef TRUInt32 TRStrokeCap;

/**
 * The shape at the corners of a stroked line.
 */
enum {
    TRStrokeJoinRound = 0,
    TRStrokeJoinBevel = 1,
    TRStrokeJoinMiterVariable = 2,  /**< Falls back to a bevel when the miter is too long. */
    TRStrokeJoinMiterFixed = 3      /**< Clips the miter at the limit. */
};
typedef TRUInt32 TRStrokeJoin;

/**
 * Selects which images of the glyphs are wanted.
 */
enum {
    TRGlyphImageKindFill = 0,       /**< The images that are painted to fill the glyphs. */
    TRGlyphImageKindStroke = 1      /**< The images of the outlines of the glyphs. */
};
typedef TRUInt32 TRGlyphImageKind;

/**
 * Called for the image of a glyph that is to be drawn.
 *
 * @param userData
 *      The pointer that was passed to the enumeration.
 * @param glyphIndex
 *      The index of the glyph in the run.
 * @param image
 *      The image of the glyph. It is valid until the function returns, so a caller that needs to
 *      keep it has to retain it.
 * @param origin
 *      The position of the top-left corner of the image, in pixels.
 * @param scaleX
 *      How much to scale the image horizontally to match the size that was asked for. It is one for
 *      the image of an outline, which is rendered at that size. The image of a glyph of a bitmap
 *      font is the one of its nearest strike, as it is stored, so it has to be drawn with the width
 *      `TRGlyphImageGetWidth() * scaleX`, from the origin that is already scaled.
 * @param scaleY
 *      How much to scale the image vertically, just like `scaleX`.
 * @param stop
 *      Set it to `TRTrue` to stop the enumeration after the function returns; it is `TRFalse` when
 *      the function is called.
 */
typedef void (*TRGlyphPlacementFunc)(void *userData, TRUInteger glyphIndex, TRGlyphImageRef image,
    TRPoint origin, TRFloat scaleX, TRFloat scaleY, TRBoolean *stop);

/**
 * A renderer prepares glyphs for drawing: it finds their images and outlines in a glyph cache for
 * the properties that were set on it, and works out where each glyph of a run goes. The platform
 * wrappers do the actual drawing with the images.
 *
 * Distances that are passed in and out are in the user space of the caller, and the images are in
 * pixels. The render scale is the number of pixels in a unit of the user space, so a glyph is
 * rendered at its type size multiplied by it. To draw sharp text, the wrapper scales its context
 * by the inverse of the render scale and draws the images at their pixel positions.
 *
 * A renderer must not be used from multiple threads at once.
 */
typedef struct _TRRenderer *TRRendererRef;

/**
 * Creates a renderer that uses the default glyph cache. It has a type size of 16, no scaling, no
 * skew, a render scale of 1, left to right direction, an opaque black foreground color, and a
 * stroke whose radius is 0.5 with butt caps, round joins and a miter limit of 1. The typeface has to be
 * set before using it.
 *
 * @return
 *      New renderer, or `NULL` on failure.
 */
TR_PUBLIC TRRendererRef TRRendererCreate(void);

/**
 * Sets the glyph cache to use. The renderer retains it. `NULL` selects the default cache.
 */
TR_PUBLIC void TRRendererSetGlyphCache(TRRendererRef renderer, TRGlyphCacheRef cache);

/**
 * Sets the typeface of the glyphs. The renderer retains it.
 */
TR_PUBLIC void TRRendererSetTypeface(TRRendererRef renderer, TRTypefaceRef typeface);

/**
 * Sets the size of the em square, in the unit of the user space.
 */
TR_PUBLIC void TRRendererSetTypeSize(TRRendererRef renderer, TRFloat typeSize);

/**
 * Sets the horizontal scale of the glyphs.
 */
TR_PUBLIC void TRRendererSetScaleX(TRRendererRef renderer, TRFloat scaleX);

/**
 * Sets the vertical scale of the glyphs.
 */
TR_PUBLIC void TRRendererSetScaleY(TRRendererRef renderer, TRFloat scaleY);

/**
 * Sets the horizontal skew of the glyphs: the distance that a point moves to the right for each
 * unit that it is above the baseline. Slanted text has a positive skew.
 */
TR_PUBLIC void TRRendererSetSkewX(TRRendererRef renderer, TRFloat skewX);

/**
 * Sets the number of pixels in a unit of the user space.
 */
TR_PUBLIC void TRRendererSetRenderScale(TRRendererRef renderer, TRFloat renderScale);

/**
 * Sets the direction of the runs that are passed in. In right-to-left runs, the pen moves to the
 * left before a glyph is placed.
 */
TR_PUBLIC void TRRendererSetWritingDirection(TRRendererRef renderer,
    TRWritingDirection writingDirection);

/**
 * Sets the color that is used for the glyphs, or the parts of them, that follow the foreground.
 */
TR_PUBLIC void TRRendererSetForegroundColor(TRRendererRef renderer, TRColor foregroundColor);

/**
 * Sets the radius of the stroked lines, in the unit of the user space: how far the stroke reaches
 * from the outline of a glyph, so a stroked line is twice as wide. It is as many pixels as it is,
 * without the render scale. The default is 0.5.
 */
TR_PUBLIC void TRRendererSetStrokeRadius(TRRendererRef renderer, TRFloat strokeRadius);

/**
 * Sets the shape at the ends of stroked lines.
 */
TR_PUBLIC void TRRendererSetStrokeCap(TRRendererRef renderer, TRStrokeCap strokeCap);

/**
 * Sets the shape at the corners of stroked lines.
 */
TR_PUBLIC void TRRendererSetStrokeJoin(TRRendererRef renderer, TRStrokeJoin strokeJoin);

/**
 * Sets the limit of the length of miters, as a ratio of the stroke width.
 */
TR_PUBLIC void TRRendererSetStrokeMiter(TRRendererRef renderer, TRFloat strokeMiter);

/**
 * Returns whether the glyphs are big enough to be rendered, which takes at least one pixel in each
 * direction.
 */
TR_PUBLIC TRBoolean TRRendererIsRenderable(TRRendererRef renderer);

/**
 * Copies the image that fills a glyph from the glyph cache. The image of a glyph of a bitmap font
 * is the one of its nearest strike, which is not scaled to the type size.
 *
 * @return
 *      The image, which the caller owns and has to release, or `NULL` if there is no typeface or
 *      the glyph has no image, such as a space.
 */
TR_PUBLIC TRGlyphImageRef TRRendererCopyGlyphImage(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Copies the image of the outline of a glyph from the glyph cache, which the caller owns and has to
 * release, or `NULL`. A glyph of a bitmap font has no outline.
 */
TR_PUBLIC TRGlyphImageRef TRRendererCopyStrokeImage(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the outline of a glyph in pixels, with the y axis pointing downward and the origin at
 * the pen position on the baseline. Enumerate it with a transform that scales it by the inverse of
 * the render scale to get it in the user space.
 *
 * @return
 *      The path, which the caller owns and has to release, or `NULL` if there is no typeface or the
 *      glyph has no outline.
 */
TR_PUBLIC TRPathRef TRRendererCopyGlyphPath(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the ink box of a glyph: the box around its image when it is placed at its pen position,
 * in the user space with the y axis pointing downward. It is empty if the glyph has no image.
 */
TR_PUBLIC TRRect TRRendererGetGlyphInkBox(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the ink box of a run of glyphs: the box around their images, in the user space with the y
 * axis pointing downward. The box is relative to the start of the run, so that of a right-to-left run is shifted
 * by the advance of the run, unlike the placements. It is empty if no glyph has an image.
 *
 * @param glyphIDs
 *      The glyphs, in the order of the writing direction.
 * @param offsets
 *      The offsets of the glyphs from their pen positions, with the y axis pointing up.
 * @param advances
 *      The advances of the glyphs.
 * @param count
 *      Number of glyphs.
 */
TR_PUBLIC TRRect TRRendererGetRunInkBox(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count);

/**
 * Passes the image of each glyph of a run, along with the position to draw it at, to a function.
 * The function is called in the order of the glyphs, and skips those that have no image. The
 * positions are in pixels and are rounded to whole ones. The pen starts at the origin: in a
 * right-to-left run it moves to the left before each glyph, so the positions are mostly negative,
 * relative to the right end of the run.
 *
 * The images are retained only while the function is being called, so nothing has to be released
 * by the caller.
 *
 * @param kind
 *      The kind of the images to find.
 * @param func
 *      The function to call for each image.
 * @param userData
 *      An opaque pointer that is passed to the function.
 *
 * The other parameters are those of `TRRendererGetRunInkBox()`.
 */
TR_PUBLIC void TRRendererEnumerateGlyphPlacements(TRRendererRef renderer, TRGlyphImageKind kind,
    const TRGlyphID *glyphIDs, const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    TRGlyphPlacementFunc func, void *userData);

/**
 * Passes the outlines of a run of glyphs to the callbacks, placed where the glyphs go, in the user
 * space and with the y axis pointing downward. The positions are not rounded, and are relative to
 * the pen in the same way as those of `TRRendererEnumerateGlyphPlacements()`.
 *
 * The other parameters are those of `TRRendererGetRunInkBox()`.
 */
TR_PUBLIC void TRRendererEnumerateGlyphPaths(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    const TRPathCallbacks *callbacks, void *userData);

/**
 * Retains the renderer.
 */
TR_PUBLIC TRRendererRef TRRendererRetain(TRRendererRef renderer);

/**
 * Releases the renderer.
 */
TR_PUBLIC void TRRendererRelease(TRRendererRef renderer);

TR_EXTERN_C_END

#endif
