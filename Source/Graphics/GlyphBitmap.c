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
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_STROKER_H

#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Mutex.h>
#include <Graphics/FreeType.h>

#include "GlyphBitmap.h"


#define GLYPH_BITMAP    0
#define BUFFER          1
#define COUNT           2

static GlyphBitmapRef AllocateGlyphBitmap(TRUInteger bufferSize)
{
    void *pointers[COUNT] = { NULL };
    TRUInteger sizes[COUNT] = { 0 };
    GlyphBitmapRef glyphBitmap = NULL;

    sizes[GLYPH_BITMAP] = sizeof(GlyphBitmap);
    sizes[BUFFER]       = bufferSize;

    if (AllocatorAllocateChunks(sizes, COUNT, pointers)) {
        glyphBitmap = pointers[GLYPH_BITMAP];
        glyphBitmap->buffer = pointers[BUFFER];
    }

    return glyphBitmap;
}

#undef GLYPH_BITMAP
#undef BUFFER
#undef COUNT


static void InitializeGlyphBitmap(GlyphBitmapRef bitmap, TRInt32 left, TRInt32 top,
    TRUInt32 width, TRUInt32 height, BitmapFormat format)
{
    bitmap->left = left;
    bitmap->top = top;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->format = format;
}

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromBitmap(const FT_Bitmap *ftBitmap, TRInt32 left,
    TRInt32 top)
{
    TRUInt32 width = ftBitmap->width;
    TRUInt32 height = ftBitmap->rows;
    GlyphBitmapRef glyphBitmap = NULL;

    if (width > 0 && height > 0) {
        unsigned char pixelMode = ftBitmap->pixel_mode;
        TRUInt32 bufferSize;

        switch (pixelMode) {
        case FT_PIXEL_MODE_MONO:
            bufferSize = width * height;
            glyphBitmap = AllocateGlyphBitmap(bufferSize);

            if (glyphBitmap) {
                const TRUInt8 *row = ftBitmap->buffer;
                TRInt32 pitch = ftBitmap->pitch;
                TRUInteger x, y;

                InitializeGlyphBitmap(glyphBitmap, left, top, width, height, BitmapFormatAlpha);

                for (y = 0; y < height; y++) {
                    const TRUInt8 *source = row;
                    TRUInt8 *destination = glyphBitmap->buffer + (y * width);

                    for (x = 0; x < width;) {
                        TRUInt8 byte = *(source++);
                        TRInt32 bit;

                        for (bit = 7; bit >= 0 && x < width; bit--) {
                            *destination = (byte & (1 << bit)) ? 255 : 0;
                            destination += 1;
                            x += 1;
                        }
                    }

                    row += pitch;
                }
            }
            break;

        case FT_PIXEL_MODE_GRAY:
            bufferSize = width * height;
            glyphBitmap = AllocateGlyphBitmap(bufferSize);

            if (glyphBitmap) {
                const TRUInt8 *row = ftBitmap->buffer;
                TRInt32 pitch = ftBitmap->pitch;
                TRUInteger x, y;

                InitializeGlyphBitmap(glyphBitmap, left, top, width, height, BitmapFormatAlpha);

                for (y = 0; y < height; y++) {
                    const TRUInt8 *source = row;
                    TRUInt8 *destination = glyphBitmap->buffer + (y * width);

                    memcpy(destination, source, width);
                    row += pitch;
                }
            }
            break;

        case FT_PIXEL_MODE_BGRA:
            bufferSize = width * height * 4;
            glyphBitmap = AllocateGlyphBitmap(bufferSize);

            if (glyphBitmap) {
                const TRUInt8 *row = ftBitmap->buffer;
                TRInt32 pitch = ftBitmap->pitch;
                TRUInteger x, y;

                InitializeGlyphBitmap(glyphBitmap, left, top, width, height, BitmapFormatARGB);

                for (y = 0; y < height; y++) {
                    const TRUInt8 *source = row;
                    TRUInt8 *destination = glyphBitmap->buffer + (y * width * 4);

                    for (x = 0; x < width; x++) {
                        TRUInt8 b = *(source++);
                        TRUInt8 g = *(source++);
                        TRUInt8 r = *(source++);
                        TRUInt8 a = *(source++);

                        destination[0] = a;
                        destination[1] = r;
                        destination[2] = g;
                        destination[3] = b;
                        destination += 4;
                    }

                    row += pitch;
                }
            }
            break;
        }
    }

    return glyphBitmap;
}

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromSlot(FT_GlyphSlot slot)
{
    return GlyphBitmapCreateFromBitmap(&slot->bitmap, slot->bitmap_left, slot->bitmap_top);
}

#define PixelFloor(value_)  ((value_) & ~(FT_Pos)63)
#define PixelCeil(value_)   (((value_) + 63) & ~(FT_Pos)63)

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromStroke(const FT_Outline *outline,
    FT_Fixed lineRadius, FT_Stroker_LineCap lineCap, FT_Stroker_LineJoin lineJoin,
    FT_Fixed miterLimit)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    GlyphBitmapRef glyphBitmap = NULL;
    FT_Stroker stroker = NULL;
    FT_Outline stroked;
    FT_UInt pointCount = 0;
    FT_UInt contourCount = 0;

    if (outline->n_points == 0) {
        return NULL;
    }

    MutexLock(&freetype->mutex);

    if (FT_Stroker_New(freetype->library, &stroker) == FT_Err_Ok) {
        FT_Stroker_Set(stroker, lineRadius, lineCap, lineJoin, miterLimit);

        if (FT_Stroker_ParseOutline(stroker, (FT_Outline *)outline, 0) == FT_Err_Ok
                && FT_Stroker_GetCounts(stroker, &pointCount, &contourCount) == FT_Err_Ok
                && FT_Outline_New(freetype->library, pointCount, contourCount, &stroked) == FT_Err_Ok) {
            FT_BBox box;

            /* The outline MUST be empty before the stroker exports into it. */
            stroked.n_points = 0;
            stroked.n_contours = 0;
            FT_Stroker_Export(stroker, &stroked);

            FT_Outline_Get_CBox(&stroked, &box);
            box.xMin = PixelFloor(box.xMin);
            box.yMin = PixelFloor(box.yMin);
            box.xMax = PixelCeil(box.xMax);
            box.yMax = PixelCeil(box.yMax);

            if (box.xMax > box.xMin && box.yMax > box.yMin) {
                TRUInt32 width = (TRUInt32)((box.xMax - box.xMin) >> 6);
                TRUInt32 height = (TRUInt32)((box.yMax - box.yMin) >> 6);
                FT_Bitmap bitmap;

                bitmap.width = width;
                bitmap.rows = height;
                bitmap.pitch = (int)width;
                bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
                bitmap.num_grays = 256;
                bitmap.palette_mode = 0;
                bitmap.palette = NULL;
                bitmap.buffer = calloc(width * height, 1);

                if (bitmap.buffer) {
                    FT_Outline_Translate(&stroked, -box.xMin, -box.yMin);

                    if (FT_Outline_Get_Bitmap(freetype->library, &stroked, &bitmap) == FT_Err_Ok) {
                        glyphBitmap = GlyphBitmapCreateFromBitmap(&bitmap, (TRInt32)(box.xMin >> 6),
                            (TRInt32)(box.yMax >> 6));
                    }

                    free(bitmap.buffer);
                }
            }

            FT_Outline_Done(freetype->library, &stroked);
        }

        FT_Stroker_Done(stroker);
    }

    MutexUnlock(&freetype->mutex);

    return glyphBitmap;
}

#undef PixelFloor
#undef PixelCeil

TR_INTERNAL void GlyphBitmapDestroy(GlyphBitmapRef bitmap)
{
    AllocatorDeallocateBlock(bitmap);
}
