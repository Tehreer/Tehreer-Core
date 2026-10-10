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

#ifndef _TEHREER_GLYPH_IMAGE_H
#define _TEHREER_GLYPH_IMAGE_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * The pixel format of a glyph image.
 */
enum {
    TRGlyphImageFormatAlpha = 0,    /**< One byte for each pixel, holding its coverage. */
    TRGlyphImageFormatARGB = 1      /**< Four bytes for each pixel in the order alpha, red, green and
                                         blue, with the color channels premultiplied by alpha. */
};
typedef TRUInt32 TRGlyphImageFormat;

/**
 * The rendered image of a glyph. The rows of its pixels follow each other without padding, from
 * the top row to the bottom one.
 *
 * The image of an outline is rendered at the size that was asked for. The image of a glyph of a
 * bitmap font is the one of its nearest strike, which is not scaled; the placement of the glyph
 * tells how much to scale it to match the size that was asked for.
 *
 * The pixels of an image never change, so it can be shared between threads. The only thing that
 * can be attached to it is native data, which lets a wrapper keep the platform object that it made
 * from the pixels. Attaching it is thread safe.
 */
typedef struct _TRGlyphImage *TRGlyphImageRef;

/**
 * Returns the pixel format of the image.
 */
TR_PUBLIC TRGlyphImageFormat TRGlyphImageGetFormat(TRGlyphImageRef image);

/**
 * Returns the distance from the origin of the glyph to the left edge of the image. The origin is
 * the pen position, and the distance grows to the right.
 */
TR_PUBLIC TRInt32 TRGlyphImageGetLeft(TRGlyphImageRef image);

/**
 * Returns the distance from the baseline of the glyph to the top edge of the image. The distance
 * grows upward, so it is positive for the part of the glyph that is above the baseline.
 */
TR_PUBLIC TRInt32 TRGlyphImageGetTop(TRGlyphImageRef image);

/**
 * Returns the width of the image in pixels.
 */
TR_PUBLIC TRUInt32 TRGlyphImageGetWidth(TRGlyphImageRef image);

/**
 * Returns the height of the image in pixels.
 */
TR_PUBLIC TRUInt32 TRGlyphImageGetHeight(TRGlyphImageRef image);

/**
 * Returns the pixels of the image. The pointer stays valid as long as the image is alive.
 */
TR_PUBLIC const TRUInt8 *TRGlyphImageGetPixelsPtr(TRGlyphImageRef image);

/**
 * Returns the size of the pixels in bytes.
 */
TR_PUBLIC TRUInteger TRGlyphImageGetByteCount(TRGlyphImageRef image);

/**
 * Returns the native data that was attached to the image, or `NULL` if there is none.
 */
TR_PUBLIC void *TRGlyphImageGetNativeData(TRGlyphImageRef image);

/**
 * Attaches native data to the image, if it has none yet. The data is destroyed when the image is
 * destroyed. If two threads attach data at the same time, one of them wins.
 *
 * @param image
 *      The image to attach the data to.
 * @param data
 *      The data, which must not be `NULL`.
 * @param destroy
 *      The function that destroys the data, or `NULL` if it needs none.
 * @return
 *      `TRTrue` if the data was attached. If it was not, the image already had some, and the caller
 *      still owns its own data and has to destroy it.
 */
TR_PUBLIC TRBoolean TRGlyphImageSetNativeData(TRGlyphImageRef image, void *data,
    void (*destroy)(void *data));

/**
 * Retains the image.
 */
TR_PUBLIC TRGlyphImageRef TRGlyphImageRetain(TRGlyphImageRef image);

/**
 * Releases the image.
 */
TR_PUBLIC void TRGlyphImageRelease(TRGlyphImageRef image);

TR_EXTERN_C_END

#endif
