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

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/AtomicPtr.h>
#include <Core/Object.h>
#include <Graphics/GlyphBitmap.h>

#include "TRGlyphImage.h"

static void FinalizeGlyphImage(ObjectRef object)
{
    TRGlyphImage *image = object;
    void *nativeData = AtomicPtrLoad(&image->_nativeData);

    if (nativeData && image->_destroyNativeData) {
        image->_destroyNativeData(nativeData);
    }

    GlyphBitmapDestroy(image->bitmap);
}

static TRUInteger GetBytesPerPixel(const GlyphBitmap *bitmap)
{
    return (bitmap->format == BitmapFormatARGB ? 4 : 1);
}

TR_INTERNAL TRGlyphImageRef TRGlyphImageCreate(GlyphBitmapRef bitmap)
{
    const TRUInteger size = sizeof(TRGlyphImage);
    void *pointer = NULL;
    TRGlyphImage *image;

    /* Bitmap MUST NOT be NULL. */
    TRAssert(bitmap != NULL);

    image = ObjectCreate(&size, 1, &pointer, FinalizeGlyphImage);

    if (image) {
        image->bitmap = bitmap;
        AtomicPtrStore(&image->_nativeData, NULL);
        image->_destroyNativeData = NULL;
    } else {
        GlyphBitmapDestroy(bitmap);
    }

    return image;
}

TRGlyphImageFormat TRGlyphImageGetFormat(TRGlyphImageRef image)
{
    return (image->bitmap->format == BitmapFormatARGB
            ? TRGlyphImageFormatARGB : TRGlyphImageFormatAlpha);
}

TRInt32 TRGlyphImageGetLeft(TRGlyphImageRef image)
{
    return image->bitmap->left;
}

TRInt32 TRGlyphImageGetTop(TRGlyphImageRef image)
{
    return image->bitmap->top;
}

TRUInt32 TRGlyphImageGetWidth(TRGlyphImageRef image)
{
    return image->bitmap->width;
}

TRUInt32 TRGlyphImageGetHeight(TRGlyphImageRef image)
{
    return image->bitmap->height;
}

const TRUInt8 *TRGlyphImageGetPixelsPtr(TRGlyphImageRef image)
{
    return image->bitmap->buffer;
}

TRUInteger TRGlyphImageGetByteCount(TRGlyphImageRef image)
{
    const GlyphBitmap *bitmap = image->bitmap;

    return (TRUInteger)bitmap->width * bitmap->height * GetBytesPerPixel(bitmap);
}

void *TRGlyphImageGetNativeData(TRGlyphImageRef image)
{
    return AtomicPtrLoad((AtomicPtr *)&image->_nativeData);
}

TRBoolean TRGlyphImageSetNativeData(TRGlyphImageRef image, void *data, void (*destroy)(void *data))
{
    TRBoolean isSet = TRFalse;
    TRGlyphImage *mutableImage = (TRGlyphImage *)image;
    void *expected = NULL;

    /*
     * The destroy function is only read when the image is destroyed. That cannot happen while the
     * caller holds the image, so it is safe to store it after the exchange.
     */
    if (data && AtomicPtrCompareAndSet(&mutableImage->_nativeData, &expected, data)) {
        mutableImage->_destroyNativeData = destroy;
        isSet = TRTrue;
    }

    return isSet;
}

TRGlyphImageRef TRGlyphImageRetain(TRGlyphImageRef image)
{
    return ObjectRetain((ObjectRef)image);
}

void TRGlyphImageRelease(TRGlyphImageRef image)
{
    ObjectRelease((ObjectRef)image);
}
