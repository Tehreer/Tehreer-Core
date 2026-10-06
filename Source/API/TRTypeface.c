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
#include <string.h>

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <Tehreer/TRString.h>

#include <API/TRBase.h>
#include <API/TRFontFile.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Font/FaceMetadata.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>

#include "TRTypeface.h"


#define TYPEFACE            0
#define RAW_COORDINATES     1
#define FACE_COORDINATES    2
#define RAW_COLORS          3
#define FACE_COLORS         4
#define COUNT               5

static void FinalizeTypeface(ObjectRef object)
{
    TRTypeface *typeface = object;

    ShapableFaceRelease(typeface->shapableFace);
    RenderableFaceRelease(typeface->renderableFace);
}

static TRTypeface *AllocateTypeface(TRUInteger variationAxisCount, TRUInteger paletteEntryCount)
{
    void *pointers[COUNT] = { NULL };
    TRUInteger sizes[COUNT] = { 0 };
    TRTypeface *typeface;

    sizes[TYPEFACE]         = sizeof(TRTypeface);
    sizes[RAW_COORDINATES]  = sizeof(FT_Fixed) * variationAxisCount;
    sizes[FACE_COORDINATES] = sizeof(TRFloat) * variationAxisCount;
    sizes[RAW_COLORS]       = sizeof(FT_Color) * paletteEntryCount;
    sizes[FACE_COLORS]      = sizeof(TRColor) * paletteEntryCount;

    typeface = ObjectCreate(sizes, COUNT, pointers, FinalizeTypeface);

    if (typeface) {
        typeface->renderableFace = NULL;
        typeface->shapableFace = NULL;
        typeface->rawCoordinates = pointers[RAW_COORDINATES];
        typeface->faceCoordinates = pointers[FACE_COORDINATES];
        typeface->rawColors = pointers[RAW_COLORS];
        typeface->faceColors = pointers[FACE_COLORS];
    }

    return typeface;
}

#undef TYPEFACE
#undef RAW_COORDINATES
#undef FACE_COORDINATES
#undef RAW_COLORS
#undef FACE_COLORS
#undef COUNT


static void InitializeCoordinates(TRTypeface *typeface, FaceMetadataRef metadata,
    const TRFloat *variationCoordinates)
{
    TRUInteger index;

    for (index = 0; index < metadata->variationAxisCount; index++) {
        TRFloat coordinate = (variationCoordinates
                              ? variationCoordinates[index]
                              : metadata->variationAxesPtr[index].defaultValue);

        typeface->faceCoordinates[index] = coordinate;
        typeface->rawCoordinates[index] = (FT_Fixed)(coordinate * 65536.0);
    }
}

static void InitializeColors(TRTypeface *typeface, FaceMetadataRef metadata,
    const TRColor *colors)
{
    TRUInteger index;

    for (index = 0; index < metadata->paletteEntryCount; index++) {
        TRColor color = TRColorMake(0xFF, 0x00, 0x00, 0x00);

        if (colors) {
            color = colors[index];
        } else if (metadata->predefinedPaletteCount > 0) {
            color = metadata->predefinedPalettesPtr[0].colorsPtr[index];
        }

        typeface->faceColors[index] = color;
        typeface->rawColors[index].alpha = (color >> 24) & 0xFF;
        typeface->rawColors[index].red = (color >> 16) & 0xFF;
        typeface->rawColors[index].green = (color >> 8) & 0xFF;
        typeface->rawColors[index].blue = color & 0xFF;
    }
}

TR_INTERNAL TRTypefaceRef TRTypefaceCreateDerived(RenderableFaceRef renderableFace,
    ShapableFaceRef shapableFace, const TRFloat *variationCoordinates, const TRColor *colors)
{
    FaceMetadataRef metadata = renderableFace->metadata;
    TRTypeface *typeface;

    typeface = AllocateTypeface(metadata->variationAxisCount, metadata->paletteEntryCount);

    if (typeface) {
        const TRStringView *subfamilyName = NULL;
        FaceDescription description;
        FaceMetrics metrics;

        InitializeCoordinates(typeface, metadata, variationCoordinates);
        InitializeColors(typeface, metadata, colors);

        RenderableFaceGetDescription(renderableFace, typeface->faceCoordinates, &subfamilyName,
            &description);
        RenderableFaceGetMetrics(renderableFace, typeface->rawCoordinates, &metrics);

        typeface->renderableFace = RenderableFaceRetain(renderableFace);
        typeface->shapableFace = ShapableFaceRetain(shapableFace);
        typeface->familyName = metadata->familyName;
        typeface->subfamilyName = subfamilyName;
        typeface->weight = description.weight;
        typeface->width = description.width;
        typeface->slope = description.slope;
        typeface->unitsPerEM = metrics.unitsPerEM;
        typeface->ascent = metrics.ascent;
        typeface->descent = metrics.descent;
        typeface->leading = metrics.leading;
        typeface->underlinePosition = metrics.underlinePosition;
        typeface->underlineThickness = metrics.underlineThickness;
        typeface->strikeoutPosition = metrics.strikeoutPosition;
        typeface->strikeoutThickness = metrics.strikeoutThickness;
        typeface->xMin = metrics.xMin;
        typeface->yMin = metrics.yMin;
        typeface->xMax = metrics.xMax;
        typeface->yMax = metrics.yMax;
    }

    return typeface;
}

TR_INTERNAL TRTypefaceRef TRTypefaceCreateDefault(RenderableFaceRef renderableFace,
    ShapableFaceRef shapableFace, const TRFloat *variationCoordinates)
{
    TRUInteger axisCount = renderableFace->metadata->variationAxisCount;
    ShapableFaceRef typefaceFace;
    TRTypefaceRef typeface;

    /* The shapable face has to follow the variation coordinates of the typeface. */
    if (variationCoordinates && axisCount > 0) {
        typefaceFace = ShapableFaceCreateDerived(shapableFace, variationCoordinates, axisCount);
    } else {
        typefaceFace = ShapableFaceRetain(shapableFace);
    }

    if (!typefaceFace) {
        return NULL;
    }

    typeface = TRTypefaceCreateDerived(renderableFace, typefaceFace, variationCoordinates, NULL);
    ShapableFaceRelease(typefaceFace);

    return typeface;
}

TRTypefaceRef TRTypefaceCreate(TRFontFileRef fontFile, TRUInteger faceIndex)
{
    RenderableFaceRef renderableFace;
    ShapableFaceRef shapableFace;
    TRTypefaceRef typeface = NULL;

    if (!fontFile || faceIndex >= fontFile->numFaces) {
        return NULL;
    }

    renderableFace = RenderableFaceCreate(fontFile, faceIndex);
    if (!renderableFace) {
        return NULL;
    }

    shapableFace = ShapableFaceCreate(renderableFace);
    if (shapableFace) {
        typeface = TRTypefaceCreateDefault(renderableFace, shapableFace, NULL);
        ShapableFaceRelease(shapableFace);
    }

    RenderableFaceRelease(renderableFace);

    return typeface;
}

TRTypefaceRef TRTypefaceCreateWithVariation(TRTypefaceRef typeface, const TRFloat *coordinates,
    TRUInteger count)
{
    FaceMetadataRef metadata = typeface->renderableFace->metadata;
    TRUInteger axisCount = metadata->variationAxisCount;
    TRTypefaceRef derived = NULL;
    ShapableFaceRef shapableFace;
    TRFloat *resolved;
    TRUInteger index;

    if (axisCount == 0) {
        return NULL;
    }

    resolved = AllocatorAllocateBlock(sizeof(TRFloat) * axisCount);
    if (!resolved) {
        return NULL;
    }

    for (index = 0; index < axisCount; index++) {
        const TRVariationAxis *axis = &metadata->variationAxesPtr[index];
        TRFloat value = axis->defaultValue;

        if (coordinates && index < count) {
            value = coordinates[index];

            if (value < axis->minValue) {
                value = axis->minValue;
            } else if (value > axis->maxValue) {
                value = axis->maxValue;
            }
        }

        resolved[index] = value;
    }

    shapableFace = ShapableFaceCreateDerived(typeface->shapableFace, resolved, axisCount);

    if (shapableFace) {
        derived = TRTypefaceCreateDerived(typeface->renderableFace, shapableFace, resolved,
            typeface->faceColors);
        ShapableFaceRelease(shapableFace);
    }

    AllocatorDeallocateBlock(resolved);

    return derived;
}

TRTypefaceRef TRTypefaceCreateWithColors(TRTypefaceRef typeface, const TRColor *colors,
    TRUInteger count)
{
    FaceMetadataRef metadata = typeface->renderableFace->metadata;
    TRUInteger entryCount = metadata->paletteEntryCount;
    TRTypefaceRef derived = NULL;
    TRColor *resolved;
    TRUInteger index;

    if (entryCount == 0) {
        return NULL;
    }

    resolved = AllocatorAllocateBlock(sizeof(TRColor) * entryCount);
    if (!resolved) {
        return NULL;
    }

    for (index = 0; index < entryCount; index++) {
        resolved[index] = (colors && index < count
                           ? colors[index]
                           : TRColorMake(0xFF, 0x00, 0x00, 0x00));
    }

    derived = TRTypefaceCreateDerived(typeface->renderableFace, typeface->shapableFace,
        typeface->faceCoordinates, resolved);

    AllocatorDeallocateBlock(resolved);

    return derived;
}

TRWeight TRTypefaceGetWeight(TRTypefaceRef typeface)
{
    return typeface->weight;
}

TRWidth TRTypefaceGetWidth(TRTypefaceRef typeface)
{
    return typeface->width;
}

TRSlope TRTypefaceGetSlope(TRTypefaceRef typeface)
{
    return typeface->slope;
}

TRUInt32 TRTypefaceGetUnitsPerEM(TRTypefaceRef typeface)
{
    return typeface->unitsPerEM;
}

TRUInt32 TRTypefaceGetAscent(TRTypefaceRef typeface)
{
    return typeface->ascent;
}

TRUInt32 TRTypefaceGetDescent(TRTypefaceRef typeface)
{
    return typeface->descent;
}

TRUInt32 TRTypefaceGetLeading(TRTypefaceRef typeface)
{
    return typeface->leading;
}

TRUInteger TRTypefaceGetGlyphCount(TRTypefaceRef typeface)
{
    return typeface->renderableFace->glyphCount;
}

const TRVariationAxis *TRTypefaceGetVariationAxesPtr(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->variationAxesPtr;
}

TRUInteger TRTypefaceGetVariationAxisCount(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->variationAxisCount;
}

const TRNamedStyle *TRTypefaceGetNamedStylesPtr(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->namedStylesPtr;
}

TRUInteger TRTypefaceGetNamedStyleCount(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->namedStyleCount;
}

const TRPaletteEntry *TRTypefaceGetPaletteEntriesPtr(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->paletteEntriesPtr;
}

TRUInteger TRTypefaceGetPaletteEntryCount(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->paletteEntryCount;
}

const TRPredefinedPalette *TRTypefaceGetPredefinedPalettesPtr(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->predefinedPalettesPtr;
}

TRUInteger TRTypefaceGetPredefinedPaletteCount(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->predefinedPaletteCount;
}

const TRFloat *TRTypefaceGetVariationCoordinatesPtr(TRTypefaceRef typeface)
{
    return typeface->faceCoordinates;
}

const TRColor *TRTypefaceGetAssociatedColorsPtr(TRTypefaceRef typeface)
{
    return typeface->faceColors;
}

const TRStringView *TRTypefaceGetFamilyName(TRTypefaceRef typeface)
{
    return typeface->familyName;
}

const TRStringView *TRTypefaceGetSubfamilyName(TRTypefaceRef typeface)
{
    return typeface->subfamilyName;
}

const TRStringView *TRTypefaceGetFullName(TRTypefaceRef typeface)
{
    return typeface->renderableFace->metadata->fullName;
}

TRRect TRTypefaceGetBoundingBox(TRTypefaceRef typeface)
{
    TRRect box;

    box.origin.x = (TRFloat)typeface->xMin;
    box.origin.y = (TRFloat)typeface->yMin;
    box.size.width = (TRFloat)(typeface->xMax - typeface->xMin);
    box.size.height = (TRFloat)(typeface->yMax - typeface->yMin);

    return box;
}

TRInt32 TRTypefaceGetUnderlinePosition(TRTypefaceRef typeface)
{
    return typeface->underlinePosition;
}

TRUInt32 TRTypefaceGetUnderlineThickness(TRTypefaceRef typeface)
{
    return typeface->underlineThickness;
}

TRInt32 TRTypefaceGetStrikeoutPosition(TRTypefaceRef typeface)
{
    return typeface->strikeoutPosition;
}

TRInt32 TRTypefaceGetStrikeoutThickness(TRTypefaceRef typeface)
{
    return typeface->strikeoutThickness;
}

TRGlyphID TRTypefaceGetGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint)
{
    return RenderableFaceGetCodePointGlyphID(typeface->renderableFace, codePoint);
}

TRGlyphID TRTypefaceGetVariantGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint,
    TRUInt32 variantSelector)
{
    return RenderableFaceGetVariantGlyphID(typeface->renderableFace, codePoint, variantSelector);
}

static void SetupFontParams(TRTypefaceRef typeface, FontParams *fontParams, TRFloat typeSize)
{
    FT_F26Dot6 pixelSize = (FT_F26Dot6)((typeSize * 64.0f) + 0.5f);

    fontParams->coordinatesPtr = typeface->rawCoordinates;
    fontParams->coordinateCount = (FT_UInt)typeface->renderableFace->metadata->variationAxisCount;
    fontParams->colorsPtr = NULL;
    fontParams->colorCount = 0;
    fontParams->pixelWidth = pixelSize;
    fontParams->pixelHeight = pixelSize;
    fontParams->transform.xx = 0x10000;
    fontParams->transform.xy = 0;
    fontParams->transform.yx = 0;
    fontParams->transform.yy = 0x10000;
}

TRFloat TRTypefaceGetGlyphAdvance(TRTypefaceRef typeface, TRGlyphID glyphID, TRFloat typeSize,
    TRBoolean isVertical)
{
    FontParams fontParams;
    TRInt32 advance;

    if (typeface->unitsPerEM == 0) {
        return 0.0f;
    }

    SetupFontParams(typeface, &fontParams, typeSize);
    advance = RenderableFaceGetDirectionalAdvance(typeface->renderableFace, &fontParams, glyphID,
        isVertical);

    return ((TRFloat)advance * typeSize) / (TRFloat)typeface->unitsPerEM;
}

TRPathRef TRTypefaceCreateGlyphPath(TRTypefaceRef typeface, TRGlyphID glyphID, TRFloat typeSize)
{
    FontParams fontParams;

    if (typeSize <= 0.0f) {
        return NULL;
    }

    SetupFontParams(typeface, &fontParams, typeSize);

    return RenderableFaceCreateGlyphPath(typeface->renderableFace, &fontParams, glyphID);
}

TRTypefaceRef TRTypefaceRetain(TRTypefaceRef typeface)
{
    return ObjectRetain((ObjectRef)typeface);
}

void TRTypefaceRelease(TRTypefaceRef typeface)
{
    ObjectRelease((ObjectRef)typeface);
}
