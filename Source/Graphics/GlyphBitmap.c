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

#include <API/TRBase.h>
#include <Core/Allocator.h>

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

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromSlot(FT_GlyphSlot slot)
{
    const FT_Bitmap *ftBitmap = &slot->bitmap;
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
                TRInt32 left = slot->bitmap_left;
                TRInt32 top = slot->bitmap_top;
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
                TRInt32 left = slot->bitmap_left;
                TRInt32 top = slot->bitmap_top;
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
                TRInt32 left = slot->bitmap_left;
                TRInt32 top = slot->bitmap_top;
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

TR_INTERNAL void GlyphBitmapDestroy(GlyphBitmapRef bitmap)
{
    AllocatorDeallocateBlock(bitmap);
}
