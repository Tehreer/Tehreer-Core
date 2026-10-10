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

#ifndef _TEHREER_DRAW_CALLBACKS_H
#define _TEHREER_DRAW_CALLBACKS_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>

/*
 * The type of a glyph run, which `TRGlyphRunRef` is a pointer to. It is declared here only to name
 * the run in `TRDrawCallbacks`: the glyph run header needs the renderer, which needs this header,
 * and a typedef cannot be repeated in C89.
 */
struct _TRGlyphRun;

TR_EXTERN_C_BEGIN

/**
 * Selects what the draw functions of glyph runs, lines and frames produce for the glyphs.
 */
enum {
    TRDrawStyleFill = 0,            /**< The glyphs are filled with the foreground color. */
    TRDrawStyleStroke = 1,          /**< The outlines are drawn with the stroke color. */
    TRDrawStyleFillStroke = 2       /**< The glyphs are filled, and then their outlines drawn. */
};
typedef TRUInt32 TRDrawStyle;

/**
 * The functions that the draw functions of glyph runs, lines and frames call, which are all
 * optional. A draw function does all the math: it passes absolute positions that are already
 * multiplied by the render scale, so every distance given to a function is in pixels, with the y
 * axis pointing downward. A function that is `NULL` is skipped.
 *
 * The glyphs are passed as images if `drawGlyphImage` is set, and as outlines otherwise, if
 * `drawGlyphPath` is set. The functions must not modify the renderer that is drawing.
 */
typedef struct _TRDrawCallbacks {
    /**
     * Draws the image of a glyph.
     *
     * @param userData
     *      The pointer that was passed with the callbacks.
     * @param image
     *      The image of the glyph. It is valid until the function returns.
     * @param origin
     *      The position of the top-left corner of the image, in whole pixels. The draw function
     *      rounds the pen position of the run once, which is its origin times the render scale,
     *      and the renderer rounds the position of the glyph from that pen, as
     *      `TRRendererEnumerateGlyphPlacements()` does; the origin is the sum of the two.
     * @param scaleX
     *      How much to scale the image horizontally to match the size that was asked for, as in
     *      `TRGlyphPlacementFunc`.
     * @param scaleY
     *      How much to scale the image vertically.
     * @param color
     *      The foreground color for the images that fill the glyphs, and the stroke color for
     *      those of the outlines.
     * @param clip
     *      The rectangle that drawing has to be limited to, or `NULL` if it is not limited. It is
     *      given for the glyphs of a cluster that is split by a line, and it is finite.
     */
    void (*drawGlyphImage)(void *userData, TRGlyphImageRef image, TRPoint origin, TRFloat scaleX,
        TRFloat scaleY, TRColor color, const TRRect *clip);

    /**
     * Draws the outline of a glyph.
     *
     * @param userData
     *      The pointer that was passed with the callbacks.
     * @param path
     *      The outline, which is valid until the function returns. It is in pixels, with the y
     *      axis pointing downward and the pen position of the glyph at its origin.
     * @param origin
     *      Where the pen position of the glyph goes, in pixels. It is not rounded.
     * @param style
     *      `TRDrawStyleFill` if the path is to be filled with the color, and `TRDrawStyleStroke`
     *      if it is to be stroked with it, using the stroke settings of the renderer.
     * @param color
     *      The foreground color for a fill, and the stroke color for a stroke.
     * @param clip
     *      The rectangle that drawing has to be limited to, as for `drawGlyphImage`.
     */
    void (*drawGlyphPath)(void *userData, TRPathRef path, TRPoint origin, TRDrawStyle style,
        TRColor color, const TRRect *clip);

    /**
     * Fills a rectangle with a color, for the backgrounds of runs and their underlines and
     * strikethroughs.
     *
     * @param userData
     *      The pointer that was passed with the callbacks.
     * @param rect
     *      The rectangle, whose edges are whole pixels.
     * @param color
     *      The color to fill it with.
     */
    void (*fillRect)(void *userData, TRRect rect, TRColor color);

    /**
     * Draws the replacement of a run. A replacement is never split, so it has no clip.
     *
     * @param userData
     *      The pointer that was passed with the callbacks.
     * @param run
     *      The run, whose `TRGlyphRunGetReplacement()` is the replacement. Its extent and metrics
     *      are in the user space, to be multiplied by the render scale.
     * @param origin
     *      The position of the start of the run on its baseline, rounded to whole pixels.
     */
    void (*drawReplacement)(void *userData, const struct _TRGlyphRun *run, TRPoint origin);
} TRDrawCallbacks;

TR_EXTERN_C_END

#endif
