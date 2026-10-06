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

TR_INTERNAL GlyphBitmapRef GlyphBitmapCreateFromSlot(FT_GlyphSlot slot);
TR_INTERNAL void GlyphBitmapDestroy(GlyphBitmapRef bitmap);

#endif
