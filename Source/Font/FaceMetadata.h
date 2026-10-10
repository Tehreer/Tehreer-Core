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

#ifndef _TEHREER_FONT_FACE_METADATA_H
#define _TEHREER_FONT_FACE_METADATA_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _FaceMetadata {
    ObjectBase _base;

    TRStringView *familyName;
    TRStringView *subfamilyName;
    TRStringView *fullName;

    TRWeight weight;
    TRWidth width;
    TRSlope slope;

    TRBoolean isScalable;

    TRBitmapStrike *bitmapStrikesPtr;
    TRUInteger bitmapStrikeCount;

    TRVariationAxis *variationAxesPtr;
    TRUInteger variationAxisCount;

    TRNamedStyle *namedStylesPtr;
    TRUInteger namedStyleCount;

    TRPaletteEntry *paletteEntriesPtr;
    TRUInteger paletteEntryCount;

    TRPredefinedPalette *predefinedPalettesPtr;
    TRUInteger predefinedPaletteCount;
} FaceMetadata, *FaceMetadataRef;

TR_INTERNAL FaceMetadataRef FaceMetadataCreate(FT_Face ftFace);

/*
 * Finds the bitmap strike that suits a pixel height in 26.6 format: the smallest one that is at
 * least as tall, or the tallest one if there is none. The face MUST have strikes.
 */
TR_INTERNAL TRUInteger FaceMetadataFindBitmapStrike(FaceMetadataRef faceMetadata,
    TRInt32 pixelHeight);

TR_INTERNAL FaceMetadataRef FaceMetadataRetain(FaceMetadataRef faceMetadata);
TR_INTERNAL void FaceMetadataRelease(FaceMetadataRef faceMetadata);

#endif
