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

#ifndef _TEHREER_GRAPHICS_GLYPH_BITMAP_H
#define _TEHREER_GRAPHICS_GLYPH_BITMAP_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <API/TRBase.h>

enum {
    BitmapFormatAlpha,
    BitmapFormatARGB
};
typedef TRUInt32 BitmapFormat;

typedef struct _GlyphBitmap {
    TRInt32 left;
    TRInt32 top;
    TRUInt32 width;
    TRUInt32 height;
    BitmapFormat format;
    TRUInt8 *buffer;
} GlyphBitmap, *GlyphBitmapRef;

/*
 * Creates a bitmap from a FreeType one, which can be in mono, gray or BGRA format, with its
 * position relative to the origin. Returns NULL if it is empty or in an unsupported format.
 */
TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromBitmap(const FT_Bitmap *ftBitmap, TRInt32 left,
    TRInt32 top);

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromSlot(FT_GlyphSlot slot);

/*
 * Strokes the outline, which MUST be in 26.6 format, and renders the result in an alpha bitmap.
 * Returns NULL if the outline has no points or the result is empty.
 */
TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromStroke(const FT_Outline *outline,
    FT_Fixed lineRadius, FT_Stroker_LineCap lineCap, FT_Stroker_LineJoin lineJoin,
    FT_Fixed miterLimit);

TR_INTERNAL void GlyphBitmapDestroy(GlyphBitmapRef bitmap);

#endif
