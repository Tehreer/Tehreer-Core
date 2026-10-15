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
#include <Tehreer/TRDrawCallbacks.h>
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
    TRStrokeCapButt = 0,        /**< The line ends exactly at the end point. */
    TRStrokeCapRound = 1,       /**< The line ends with a semicircle around the end point. */
    TRStrokeCapSquare = 2       /**< The line extends beyond the end point by the stroke radius. */
};
/**
 * A type to represent the shape at the ends of stroked lines, which is one of the `TRStrokeCap*`
 * constants above.
 */
typedef TRUInt32 TRStrokeCap;

/**
 * The shape at the corners of a stroked line.
 */
enum {
    TRStrokeJoinRound = 0,          /**< The corner is rounded. */
    TRStrokeJoinBevel = 1,          /**< The corner is cut off flat. */
    TRStrokeJoinMiterVariable = 2,  /**< Falls back to a bevel when the miter is too long. */
    TRStrokeJoinMiterFixed = 3      /**< Clips the miter at the limit. */
};
/**
 * A type to represent the shape at the corners of stroked lines, which is one of the
 * `TRStrokeJoin*` constants above.
 */
typedef TRUInt32 TRStrokeJoin;

/**
 * Selects which images of the glyphs are wanted.
 */
enum {
    TRGlyphImageKindFill = 0,       /**< The images that are painted to fill the glyphs. */
    TRGlyphImageKindStroke = 1      /**< The images of the outlines of the glyphs. */
};
/**
 * A type to represent the kind of glyph images that are wanted, which is one of the
 * `TRGlyphImageKind*` constants above.
 */
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
 * stroke whose radius is 0.5 with butt caps, round joins and a miter limit of 1, drawn in opaque
 * black. It fills glyphs when drawing, and has no draw callbacks. The typeface has to be set
 * before using it.
 *
 * @return
 *      New renderer, or `NULL` on failure.
 */
TR_PUBLIC TRRendererRef TRRendererCreate(void);

/**
 * Returns the glyph cache that the renderer finds glyphs in. It is the default cache unless one
 * was set. The caller does not own it.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The glyph cache, or `NULL` if the renderer uses the default cache and it could not be
 *      created.
 */
TR_PUBLIC TRGlyphCacheRef TRRendererGetGlyphCache(TRRendererRef renderer);

/**
 * Sets the glyph cache to use. The renderer retains it.
 *
 * @param renderer
 *      The renderer.
 * @param cache
 *      The glyph cache, or `NULL` to select the default cache.
 */
TR_PUBLIC void TRRendererSetGlyphCache(TRRendererRef renderer, TRGlyphCacheRef cache);

/**
 * Returns the typeface of the glyphs. The caller does not own it.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The typeface, or `NULL` if it is not set.
 */
TR_PUBLIC TRTypefaceRef TRRendererGetTypeface(TRRendererRef renderer);

/**
 * Sets the typeface of the glyphs. The renderer retains it. No image, path or placement is made
 * while it is `NULL`.
 *
 * @param renderer
 *      The renderer.
 * @param typeface
 *      The typeface, or `NULL` to clear it.
 */
TR_PUBLIC void TRRendererSetTypeface(TRRendererRef renderer, TRTypefaceRef typeface);

/**
 * Returns the size of the em square, in the unit of the user space.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The size of the em square.
 */
TR_PUBLIC TRFloat TRRendererGetTypeSize(TRRendererRef renderer);

/**
 * Sets the size of the em square, in the unit of the user space. The default is 16.
 *
 * @param renderer
 *      The renderer.
 * @param typeSize
 *      The size of the em square.
 */
TR_PUBLIC void TRRendererSetTypeSize(TRRendererRef renderer, TRFloat typeSize);

/**
 * Returns the horizontal scale of the glyphs, which is applied to the type size.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The horizontal scale.
 */
TR_PUBLIC TRFloat TRRendererGetScaleX(TRRendererRef renderer);

/**
 * Sets the horizontal scale of the glyphs, which is applied to the type size. The default is 1.
 *
 * @param renderer
 *      The renderer.
 * @param scaleX
 *      The horizontal scale.
 */
TR_PUBLIC void TRRendererSetScaleX(TRRendererRef renderer, TRFloat scaleX);

/**
 * Returns the vertical scale of the glyphs, which is applied to the type size.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The vertical scale.
 */
TR_PUBLIC TRFloat TRRendererGetScaleY(TRRendererRef renderer);

/**
 * Sets the vertical scale of the glyphs, which is applied to the type size. The default is 1.
 *
 * @param renderer
 *      The renderer.
 * @param scaleY
 *      The vertical scale.
 */
TR_PUBLIC void TRRendererSetScaleY(TRRendererRef renderer, TRFloat scaleY);

/**
 * Returns the horizontal skew of the glyphs.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The horizontal skew.
 */
TR_PUBLIC TRFloat TRRendererGetSkewX(TRRendererRef renderer);

/**
 * Sets the horizontal skew of the glyphs: the distance that a point moves to the right for each
 * unit that it is above the baseline. Slanted text has a positive skew. The default is 0. It does
 * not apply to the images of a font that has only bitmap strikes.
 *
 * @param renderer
 *      The renderer.
 * @param skewX
 *      The horizontal skew.
 */
TR_PUBLIC void TRRendererSetSkewX(TRRendererRef renderer, TRFloat skewX);

/**
 * Returns the number of pixels in a unit of the user space.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The number of pixels in a unit.
 */
TR_PUBLIC TRFloat TRRendererGetRenderScale(TRRendererRef renderer);

/**
 * Sets the number of pixels in a unit of the user space. The default is 1.
 *
 * @param renderer
 *      The renderer.
 * @param renderScale
 *      The number of pixels in a unit.
 */
TR_PUBLIC void TRRendererSetRenderScale(TRRendererRef renderer, TRFloat renderScale);

/**
 * Returns the direction of the runs that are passed in.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The direction of the runs.
 */
TR_PUBLIC TRWritingDirection TRRendererGetWritingDirection(TRRendererRef renderer);

/**
 * Sets the direction of the runs that are passed in. In right-to-left runs, the pen moves to the
 * left before a glyph is placed. The default is left to right. A value that is not valid is kept
 * as it is, and the runs are treated as left to right.
 *
 * @param renderer
 *      The renderer.
 * @param writingDirection
 *      The direction of the runs.
 */
TR_PUBLIC void TRRendererSetWritingDirection(TRRendererRef renderer,
    TRWritingDirection writingDirection);

/**
 * Returns the color that is used for the glyphs, or the parts of them, that follow the
 * foreground.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The foreground color.
 */
TR_PUBLIC TRColor TRRendererGetForegroundColor(TRRendererRef renderer);

/**
 * Sets the color that is used for the glyphs, or the parts of them, that follow the foreground. The
 * images that fill the glyphs are made in it. The default is opaque black.
 *
 * @param renderer
 *      The renderer.
 * @param foregroundColor
 *      The foreground color.
 */
TR_PUBLIC void TRRendererSetForegroundColor(TRRendererRef renderer, TRColor foregroundColor);

/**
 * Returns the radius of the stroked lines, as it was set. A negative or NaN radius is taken as zero
 * when drawing.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The radius of the stroked lines.
 */
TR_PUBLIC TRFloat TRRendererGetStrokeRadius(TRRendererRef renderer);

/**
 * Sets the radius of the stroked lines, in the unit of the user space: how far the stroke reaches
 * from the outline of a glyph, so a stroked line is twice as wide. It is as many pixels as it is,
 * without the render scale. The default is 0.5. A negative or NaN radius is taken as zero.
 *
 * @param renderer
 *      The renderer.
 * @param strokeRadius
 *      The radius of the stroked lines.
 */
TR_PUBLIC void TRRendererSetStrokeRadius(TRRendererRef renderer, TRFloat strokeRadius);

/**
 * Returns the shape at the ends of stroked lines.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The shape at the ends.
 */
TR_PUBLIC TRStrokeCap TRRendererGetStrokeCap(TRRendererRef renderer);

/**
 * Sets the shape at the ends of stroked lines. The default is `TRStrokeCapButt`. A value that is
 * not one of the `TRStrokeCap*` constants is kept and returned as it is, and the stroked images are
 * still made with it; the shape that it gives is not specified.
 *
 * @param renderer
 *      The renderer.
 * @param strokeCap
 *      The shape at the ends.
 */
TR_PUBLIC void TRRendererSetStrokeCap(TRRendererRef renderer, TRStrokeCap strokeCap);

/**
 * Returns the shape at the corners of stroked lines.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The shape at the corners.
 */
TR_PUBLIC TRStrokeJoin TRRendererGetStrokeJoin(TRRendererRef renderer);

/**
 * Sets the shape at the corners of stroked lines. The default is `TRStrokeJoinRound`. A value that
 * is not one of the `TRStrokeJoin*` constants is kept and returned as it is, and the stroked
 * images are still made with it; the shape that it gives is not specified.
 *
 * @param renderer
 *      The renderer.
 * @param strokeJoin
 *      The shape at the corners.
 */
TR_PUBLIC void TRRendererSetStrokeJoin(TRRendererRef renderer, TRStrokeJoin strokeJoin);

/**
 * Returns the limit of the length of miters, as it was set. A negative or NaN limit is taken as
 * zero when drawing.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The limit of the miters.
 */
TR_PUBLIC TRFloat TRRendererGetStrokeMiter(TRRendererRef renderer);

/**
 * Sets the limit of the length of miters, as a ratio of the stroke width. The default is 1. A
 * negative or NaN limit is taken as zero.
 *
 * @param renderer
 *      The renderer.
 * @param strokeMiter
 *      The limit of the miters.
 */
TR_PUBLIC void TRRendererSetStrokeMiter(TRRendererRef renderer, TRFloat strokeMiter);

/**
 * Returns the color that is used for the outlines of the glyphs when the draw style strokes
 * them.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The color of the outlines.
 */
TR_PUBLIC TRColor TRRendererGetStrokeColor(TRRendererRef renderer);

/**
 * Sets the color that is used for the outlines of the glyphs when the draw style strokes them. The
 * default is opaque black.
 *
 * @param renderer
 *      The renderer.
 * @param strokeColor
 *      The color of the outlines.
 */
TR_PUBLIC void TRRendererSetStrokeColor(TRRendererRef renderer, TRColor strokeColor);

/**
 * Returns the blur radius of the shadow that is drawn behind the glyphs, as it was set. A radius
 * that is negative, NaN or infinite is returned as it is, but no shadow is drawn with it.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The blur radius of the shadow, in pixels.
 */
TR_PUBLIC TRFloat TRRendererGetShadowRadius(TRRendererRef renderer);

/**
 * Sets the blur radius of the shadow that is drawn behind the glyphs. The default is 0, which
 * disables the shadow. The radius is in pixels and is not multiplied by the render scale. The
 * shadow is applied by the draw callbacks `beginShadow` and `endShadow`, which surround the glyphs
 * of each run.
 *
 * The value is stored as it is given. No shadow is drawn, and the callbacks are not called, when
 * the radius is not above zero or is not finite, or when the horizontal or vertical offset is not
 * finite (NaN or infinity). See `TRDrawCallbacks` for the other conditions.
 *
 * @param renderer
 *      The renderer.
 * @param shadowRadius
 *      The blur radius of the shadow, in pixels.
 */
TR_PUBLIC void TRRendererSetShadowRadius(TRRendererRef renderer, TRFloat shadowRadius);

/**
 * Returns the horizontal offset of the shadow, as it was set. A NaN or infinite offset is returned
 * as it is, but no shadow is drawn with it.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The offset of the shadow along the x axis, in pixels.
 */
TR_PUBLIC TRFloat TRRendererGetShadowDx(TRRendererRef renderer);

/**
 * Sets the horizontal offset of the shadow. The default is 0. The value is stored as it is given,
 * but no shadow is drawn while it is NaN or infinite.
 *
 * @param renderer
 *      The renderer.
 * @param shadowDx
 *      The offset of the shadow along the x axis, in pixels, which is positive towards the right.
 */
TR_PUBLIC void TRRendererSetShadowDx(TRRendererRef renderer, TRFloat shadowDx);

/**
 * Returns the vertical offset of the shadow, as it was set. A NaN or infinite offset is returned
 * as it is, but no shadow is drawn with it.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The offset of the shadow along the y axis, in pixels.
 */
TR_PUBLIC TRFloat TRRendererGetShadowDy(TRRendererRef renderer);

/**
 * Sets the vertical offset of the shadow. The default is 0. The value is stored as it is given,
 * but no shadow is drawn while it is NaN or infinite.
 *
 * @param renderer
 *      The renderer.
 * @param shadowDy
 *      The offset of the shadow along the y axis, in pixels, which is positive downward.
 */
TR_PUBLIC void TRRendererSetShadowDy(TRRendererRef renderer, TRFloat shadowDy);

/**
 * Returns the color of the shadow.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The color of the shadow.
 */
TR_PUBLIC TRColor TRRendererGetShadowColor(TRRendererRef renderer);

/**
 * Sets the color of the shadow. The default is transparent black.
 *
 * @param renderer
 *      The renderer.
 * @param shadowColor
 *      The color of the shadow.
 */
TR_PUBLIC void TRRendererSetShadowColor(TRRendererRef renderer, TRColor shadowColor);

/**
 * Returns what the draw functions of glyph runs, lines and frames produce for the glyphs.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      Whether the glyphs are filled, their outlines are stroked, or both.
 */
TR_PUBLIC TRDrawStyle TRRendererGetDrawStyle(TRRendererRef renderer);

/**
 * Sets what the draw functions of glyph runs, lines and frames produce for the glyphs. The default
 * is `TRDrawStyleFill`. A value that is not one of the `TRDrawStyle*` constants is kept and
 * returned as it is, and the glyphs are filled and stroked as with `TRDrawStyleFillStroke`.
 *
 * @param renderer
 *      The renderer.
 * @param drawStyle
 *      Whether to fill the glyphs, stroke their outlines, or both.
 */
TR_PUBLIC void TRRendererSetDrawStyle(TRRendererRef renderer, TRDrawStyle drawStyle);

/**
 * Copies the functions that the draw functions call. All of them are `NULL` if no callbacks are
 * set.
 *
 * @param renderer
 *      The renderer.
 * @param callbacks
 *      The structure that receives the callbacks.
 */
TR_PUBLIC void TRRendererGetDrawCallbacks(TRRendererRef renderer, TRDrawCallbacks *callbacks);

/**
 * Returns the opaque pointer that is passed to the draw callbacks.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      The user data that was set with the callbacks, or `NULL`.
 */
TR_PUBLIC void *TRRendererGetDrawUserData(TRRendererRef renderer);

/**
 * Sets the functions that the draw functions call. The callbacks are copied, and `NULL` clears
 * them, so that nothing is drawn.
 *
 * @param renderer
 *      The renderer.
 * @param callbacks
 *      The callbacks to use, or `NULL`.
 * @param userData
 *      An opaque pointer that is passed to each of them.
 */
TR_PUBLIC void TRRendererSetDrawCallbacks(TRRendererRef renderer, const TRDrawCallbacks *callbacks,
    void *userData);

/**
 * Returns whether the glyphs are big enough to be rendered, which takes at least one pixel in each
 * direction. It is the size of the em square multiplied by the scale and the render scale.
 *
 * @param renderer
 *      The renderer.
 * @return
 *      `TRTrue` if the em square takes at least one pixel horizontally and vertically.
 */
TR_PUBLIC TRBoolean TRRendererIsRenderable(TRRendererRef renderer);

/**
 * Copies the image that fills a glyph from the glyph cache. The image of a glyph of a bitmap font
 * is the one of its nearest strike, which is not scaled to the type size.
 *
 * The image is made in the foreground color of the renderer for the parts that follow it.
 *
 * @param renderer
 *      The renderer.
 * @param glyphID
 *      The ID of the glyph.
 * @return
 *      The image, which the caller owns and has to release, or `NULL` if there is no typeface or
 *      the glyph has no image, such as a space.
 */
TR_PUBLIC TRGlyphImageRef TRRendererCopyGlyphImage(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Copies the image of the outline of a glyph from the glyph cache, as stroked with the radius, cap,
 * join and miter of the renderer.
 *
 * @param renderer
 *      The renderer.
 * @param glyphID
 *      The ID of the glyph.
 * @return
 *      The image, which the caller owns and has to release, or `NULL` if there is no typeface or
 *      the glyph has no outline, as a glyph of a bitmap font.
 */
TR_PUBLIC TRGlyphImageRef TRRendererCopyStrokeImage(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the outline of a glyph in pixels, with the y axis pointing downward and the origin at
 * the pen position on the baseline. Enumerate it with a transform that scales it by the inverse of
 * the render scale to get it in the user space.
 *
 * @param renderer
 *      The renderer.
 * @param glyphID
 *      The ID of the glyph.
 * @return
 *      The path, which the caller owns and has to release, or `NULL` if there is no typeface or the
 *      glyph has no outline.
 */
TR_PUBLIC TRPathRef TRRendererCopyGlyphPath(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the ink box of a glyph: the box around its image when it is placed at its pen position,
 * in the user space with the y axis pointing downward.
 *
 * @param renderer
 *      The renderer.
 * @param glyphID
 *      The ID of the glyph.
 * @return
 *      The box, which is empty if the glyph has no image.
 */
TR_PUBLIC TRRect TRRendererGetGlyphInkBox(TRRendererRef renderer, TRGlyphID glyphID);

/**
 * Returns the ink box of a run of glyphs: the box around their images, in the user space with the y
 * axis pointing downward. The box is relative to the start of the run, so that of a right-to-left
 * run is shifted by the advance of the run, unlike the placements. It is empty if no glyph has an
 * image.
 *
 * @param renderer
 *      The renderer.
 * @param glyphIDs
 *      The glyphs, in the order of the writing direction.
 * @param offsets
 *      The offsets of the glyphs from their pen positions, with the y axis pointing up.
 * @param advances
 *      The advances of the glyphs.
 * @param count
 *      Number of glyphs.
 * @return
 *      The ink box of the run.
 */
TR_PUBLIC TRRect TRRendererComputeRunInkBox(TRRendererRef renderer, const TRGlyphID *glyphIDs,
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
 * @param renderer
 *      The renderer.
 * @param kind
 *      The kind of the images to find.
 * @param glyphIDs
 *      The glyphs, in the order of the writing direction.
 * @param offsets
 *      The offsets of the glyphs from their pen positions, with the y axis pointing up.
 * @param advances
 *      The advances of the glyphs.
 * @param count
 *      Number of glyphs.
 * @param func
 *      The function to call for each image.
 * @param userData
 *      An opaque pointer that is passed to the function.
 */
TR_PUBLIC void TRRendererEnumerateGlyphPlacements(TRRendererRef renderer, TRGlyphImageKind kind,
    const TRGlyphID *glyphIDs, const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    TRGlyphPlacementFunc func, void *userData);

/**
 * Passes the outlines of a run of glyphs to the callbacks, placed where the glyphs go, in the user
 * space and with the y axis pointing downward. The positions are not rounded, and are relative to
 * the pen in the same way as those of `TRRendererEnumerateGlyphPlacements()`.
 *
 * @param renderer
 *      The renderer.
 * @param glyphIDs
 *      The glyphs, in the order of the writing direction.
 * @param offsets
 *      The offsets of the glyphs from their pen positions, with the y axis pointing up.
 * @param advances
 *      The advances of the glyphs.
 * @param count
 *      Number of glyphs.
 * @param callbacks
 *      The functions that receive the contours of each outline.
 * @param userData
 *      An opaque pointer that is passed to the callbacks.
 */
TR_PUBLIC void TRRendererEnumerateGlyphPaths(TRRendererRef renderer, const TRGlyphID *glyphIDs,
    const TRPoint *offsets, const TRFloat *advances, TRUInteger count,
    const TRPathCallbacks *callbacks, void *userData);

/**
 * Increments the reference count of a renderer.
 *
 * @param renderer
 *      The renderer whose reference count will be incremented.
 * @return
 *      The same renderer passed in as the parameter.
 */
TR_PUBLIC TRRendererRef TRRendererRetain(TRRendererRef renderer);

/**
 * Decrements the reference count of a renderer. The renderer will be deallocated when its
 * reference count reaches zero.
 *
 * @param renderer
 *      The renderer whose reference count will be decremented.
 */
TR_PUBLIC void TRRendererRelease(TRRendererRef renderer);

TR_EXTERN_C_END

#endif
