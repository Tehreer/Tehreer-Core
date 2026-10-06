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

#ifndef _TEHREER_API_GLYPH_IMAGE_H
#define _TEHREER_API_GLYPH_IMAGE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGlyphImage.h>

#include <API/TRBase.h>
#include <Core/AtomicPtr.h>
#include <Core/Object.h>
#include <Graphics/GlyphBitmap.h>

typedef struct _TRGlyphImage {
    ObjectBase _base;
    GlyphBitmapRef bitmap;
    AtomicPtr _nativeData;
    void (*_destroyNativeData)(void *data);
} TRGlyphImage;

/*
 * Creates an image that takes the ownership of the bitmap, which is destroyed with the image. If
 * the image cannot be created, the bitmap is destroyed right away. The bitmap MUST NOT be NULL.
 */
TR_INTERNAL TRGlyphImageRef TRGlyphImageCreate(GlyphBitmapRef bitmap);

#endif
