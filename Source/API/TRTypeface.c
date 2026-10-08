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
#define FULL_NAME           5
#define COUNT               6

static void FinalizeTypeface(ObjectRef object)
{
    TRTypeface *typeface = object;

    ShapableFaceRelease(typeface->shapableFace);
    RenderableFaceRelease(typeface->renderableFace);
}

static TRTypeface *AllocateTypeface(TRUInteger variationAxisCount, TRUInteger paletteEntryCount,
    TRUInteger fullNameUnits)
{
    void *pointers[COUNT] = { NULL };
    TRUInteger sizes[COUNT] = { 0 };
    TRTypeface *typeface;

    sizes[TYPEFACE]         = sizeof(TRTypeface);
    sizes[RAW_COORDINATES]  = sizeof(FT_Fixed) * variationAxisCount;
    sizes[FACE_COORDINATES] = sizeof(TRFloat) * variationAxisCount;
    sizes[RAW_COLORS]       = sizeof(FT_Color) * paletteEntryCount;
    sizes[FACE_COLORS]      = sizeof(TRColor) * paletteEntryCount;
    sizes[FULL_NAME]        = sizeof(TRUInt16) * fullNameUnits;

    typeface = ObjectCreate(sizes, COUNT, pointers, FinalizeTypeface);

    if (typeface) {
        typeface->renderableFace = NULL;
        typeface->shapableFace = NULL;
        typeface->rawCoordinates = pointers[RAW_COORDINATES];
        typeface->faceCoordinates = pointers[FACE_COORDINATES];
        typeface->rawColors = pointers[RAW_COLORS];
        typeface->faceColors = pointers[FACE_COLORS];
        typeface->fullNameUnits = pointers[FULL_NAME];
        typeface->fullName = NULL;
    }

    return typeface;
}

#undef TYPEFACE
#undef RAW_COORDINATES
#undef FACE_COORDINATES
#undef RAW_COLORS
#undef FACE_COLORS
#undef FULL_NAME
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

/* Fills the coordinates of every axis, using the default value of those that are not given. */
static void ResolveCoordinates(FaceMetadataRef metadata, const TRFloat *coordinates,
    TRUInteger count, TRFloat *resolved)
{
    TRUInteger index;

    for (index = 0; index < metadata->variationAxisCount; index++) {
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
}

/* Returns the number of code units that the longest full name of a font can have. */
static TRUInteger GetFullNameCapacity(FaceMetadataRef metadata)
{
    TRUInteger styleLength = (metadata->subfamilyName ? metadata->subfamilyName->length : 0);
    TRUInteger familyLength = (metadata->familyName ? metadata->familyName->length : 0);
    TRUInteger index;

    for (index = 0; index < metadata->namedStyleCount; index++) {
        const TRStringView *name = metadata->namedStylesPtr[index].subfamilyName;

        if (name && name->length > styleLength) {
            styleLength = name->length;
        }
    }

    return familyLength + 1 + styleLength;
}

/*
 * Sets the full name of a typeface. A variable font that has named styles shows the full name of
 * the style that the coordinates match, or none if they match nothing. Otherwise it is the full
 * name of the font, or the family name followed by the style name if the font has none.
 */
static void InitializeFullName(TRTypeface *typeface, FaceMetadataRef metadata)
{
    const TRStringView *family = metadata->familyName;
    const TRStringView *style = typeface->subfamilyName;
    TRBoolean isStyled = (metadata->variationAxisCount > 0 && metadata->namedStyleCount > 0);

    if (isStyled && !style) {
        /* The coordinates match no style. */
        typeface->fullName = NULL;
    } else if (!isStyled && metadata->fullName) {
        typeface->fullName = metadata->fullName;
    } else if (family || style) {
        TRUInt16 *units = typeface->fullNameUnits;
        TRUInteger length = 0;

        if (family) {
            memcpy(units, family->buffer, family->length * sizeof(TRUInt16));
            length = family->length;
        }
        if (style) {
            if (family) {
                units[length++] = ' ';
            }
            memcpy(units + length, style->buffer, style->length * sizeof(TRUInt16));
            length += style->length;
        }

        typeface->fullNameView.buffer = units;
        typeface->fullNameView.length = length;
        typeface->fullNameView.encoding = TRStringEncodingUTF16;
        typeface->fullName = &typeface->fullNameView;
    }
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

TR_INTERNAL TRTypefaceRef TRTypefaceCreateDefault(RenderableFaceRef renderableFace,
    ShapableFaceRef sourceFace, const TRFloat *variationCoordinates)
{
    TRUInteger axisCount = renderableFace->metadata->variationAxisCount;
    TRTypefaceRef derived = NULL;
    ShapableFaceRef shapableFace;

    /* The shapable face has to follow the variation coordinates of the typeface. */
    if (variationCoordinates && axisCount > 0) {
        shapableFace = ShapableFaceCreateDerived(sourceFace, variationCoordinates, axisCount);
    } else {
        shapableFace = ShapableFaceRetain(sourceFace);
    }

    if (shapableFace) {
        derived = TRTypefaceCreateDerived(renderableFace, shapableFace, variationCoordinates, NULL);
        ShapableFaceRelease(shapableFace);
    }

    return derived;
}

TR_INTERNAL TRTypefaceRef TRTypefaceCreateDerived(RenderableFaceRef renderableFace,
    ShapableFaceRef shapableFace, const TRFloat *variationCoordinates, const TRColor *colors)
{
    FaceMetadataRef metadata = renderableFace->metadata;
    TRTypeface *typeface;

    typeface = AllocateTypeface(metadata->variationAxisCount, metadata->paletteEntryCount,
        GetFullNameCapacity(metadata));

    if (typeface) {
        const TRStringView *subfamilyName = NULL;
        FaceDescription description;
        FaceMetrics metrics;

        InitializeCoordinates(typeface, metadata, variationCoordinates);
        InitializeColors(typeface, metadata, colors);

        RenderableFaceGetDescription(renderableFace, typeface->faceCoordinates, &subfamilyName,
            &description);
        RenderableFaceGetMetrics(renderableFace, typeface->rawCoordinates, &metrics);

        /* A variable font without named styles keeps the style name of the font. */
        if (!subfamilyName && metadata->namedStyleCount == 0) {
            subfamilyName = metadata->subfamilyName;
        }

        typeface->renderableFace = RenderableFaceRetain(renderableFace);
        typeface->shapableFace = ShapableFaceRetain(shapableFace);
        typeface->familyName = metadata->familyName;
        typeface->subfamilyName = subfamilyName;
        InitializeFullName(typeface, metadata);
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

TRTypefaceRef TRTypefaceCreate(TRFontFileRef fontFile, TRUInteger faceIndex)
{
    TRTypefaceRef typeface = NULL;

    if (fontFile && faceIndex < fontFile->numFaces) {
        RenderableFaceRef renderableFace;

        renderableFace = RenderableFaceCreate(fontFile, faceIndex);

        if (renderableFace) {
            ShapableFaceRef shapableFace;

            shapableFace = ShapableFaceCreate(renderableFace);

            if (shapableFace) {
                typeface = TRTypefaceCreateDefault(renderableFace, shapableFace, NULL);
                ShapableFaceRelease(shapableFace);
            }

            RenderableFaceRelease(renderableFace);
        }
    }

    return typeface;
}

TRTypefaceRef TRTypefaceCreateWithVariation(TRTypefaceRef typeface, const TRFloat *coordinates,
    TRUInteger count)
{
    FaceMetadataRef metadata = typeface->renderableFace->metadata;
    TRUInteger axisCount = metadata->variationAxisCount;
    TRTypefaceRef derived = NULL;
    TRFloat *resolved = NULL;

    if (axisCount > 0) {
        resolved = AllocatorAllocateBlock(sizeof(TRFloat) * axisCount);
    }

    if (resolved) {
        ShapableFaceRef shapableFace;

        ResolveCoordinates(metadata, coordinates, count, resolved);

        shapableFace = ShapableFaceCreateDerived(typeface->shapableFace, resolved, axisCount);

        if (shapableFace) {
            derived = TRTypefaceCreateDerived(typeface->renderableFace, shapableFace, resolved,
                typeface->faceColors);
            ShapableFaceRelease(shapableFace);
        }

        AllocatorDeallocateBlock(resolved);
    }

    return derived;
}

TRTypefaceRef TRTypefaceCreateWithColors(TRTypefaceRef typeface, const TRColor *colors,
    TRUInteger count)
{
    FaceMetadataRef metadata = typeface->renderableFace->metadata;
    TRUInteger entryCount = metadata->paletteEntryCount;
    TRTypefaceRef derived = NULL;
    TRColor *resolved = NULL;

    if (entryCount > 0) {
        resolved = AllocatorAllocateBlock(sizeof(TRColor) * entryCount);
    }

    if (resolved) {
        TRUInteger index;

        for (index = 0; index < entryCount; index++) {
            resolved[index] = (colors && index < count
                               ? colors[index]
                               : TRColorMake(0xFF, 0x00, 0x00, 0x00));
        }

        derived = TRTypefaceCreateDerived(typeface->renderableFace, typeface->shapableFace,
            typeface->faceCoordinates, resolved);

        AllocatorDeallocateBlock(resolved);
    }

    return derived;
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
    return typeface->fullName;
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

TRFloat TRTypefaceGetGlyphAdvance(TRTypefaceRef typeface, TRGlyphID glyphID, TRFloat typeSize,
    TRBoolean isVertical)
{
    TRFloat glyphAdvance = 0.0f;

    if (typeface->unitsPerEM > 0) {
        FontParams fontParams;
        TRInt32 advance;

        SetupFontParams(typeface, &fontParams, typeSize);
        advance = RenderableFaceGetDirectionalAdvance(typeface->renderableFace, &fontParams,
            glyphID, isVertical);

        glyphAdvance = ((TRFloat)advance * typeSize) / (TRFloat)typeface->unitsPerEM;
    }

    return glyphAdvance;
}

TRPathRef TRTypefaceCreateGlyphPath(TRTypefaceRef typeface, TRGlyphID glyphID, TRFloat typeSize)
{
    TRPathRef glyphPath = NULL;

    if (typeSize > 0.0f) {
        FontParams fontParams;

        SetupFontParams(typeface, &fontParams, typeSize);

        glyphPath = RenderableFaceCreateGlyphPath(typeface->renderableFace, &fontParams, glyphID);
    }

    return glyphPath;
}

TRUInteger TRTypefaceGetTableData(TRTypefaceRef typeface, TRTag tag, void *buffer,
    TRUInteger capacity)
{
    void *table = NULL;
    TRUInteger size = 0;

    RenderableFaceCopyTable(typeface->renderableFace, tag, &table, &size);

    if (table) {
        if (buffer) {
            memcpy(buffer, table, (size < capacity ? size : capacity));
        }

        AllocatorDeallocateBlock(table);
    }

    return size;
}

TRUInteger TRTypefaceGetGlyphName(TRTypefaceRef typeface, TRGlyphID glyphID, char *buffer,
    TRUInteger capacity)
{
    return RenderableFaceCopyGlyphName(typeface->renderableFace, glyphID, buffer, capacity);
}

TRTypefaceRef TRTypefaceRetain(TRTypefaceRef typeface)
{
    return ObjectRetain((ObjectRef)typeface);
}

void TRTypefaceRelease(TRTypefaceRef typeface)
{
    ObjectRelease((ObjectRef)typeface);
}
