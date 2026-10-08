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

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <ft2build.h>
#include FT_BITMAP_H
#include FT_ADVANCES_H
#include FT_COLOR_H
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRFontFile.h>
#include <API/TRPath.h>
#include <Core/Allocator.h>
#include <Core/AtomicUInt.h>
#include <Core/Mutex.h>
#include <Core/Once.h>
#include <Font/FaceMetadata.h>
#include <Graphics/FreeType.h>
#include <SFNT/Utilities.h>

#include "RenderableFace.h"

typedef struct _UsableFace {
    FaceNodeRef _faceNode;
    FT_Face ftFace;
} UsableFace;

#define FaceStackIndexBits  8
#define FaceStackIndexMask  (((TRUInteger)1 << FaceStackIndexBits) - 1)

#define FaceStackMake(version_, index_) \
    (((version_) << FaceStackIndexBits) | (index_))
#define FaceStackGetVersion(stack_)     ((stack_) >> FaceStackIndexBits)
#define FaceStackGetIndex(stack_)       ((stack_) & FaceStackIndexMask)

static void InitializeFacePool(FaceNode *facePool, TRUInteger poolSize)
{
    TRUInteger index;

    for (index = 0; index < poolSize - 1; index++) {
        facePool[index].ftFace = NULL;
        AtomicUIntInit(&facePool[index].next, index + 2);
    }

    facePool[index].ftFace = NULL;
    AtomicUIntInit(&facePool[index].next, 0);
}

static FaceNodeRef DetachFaceNode(RenderableFaceRef renderableFace)
{
    FaceNodeRef faceNode;
    TRUInteger stack;
    TRUInteger expected;

    do {
        TRUInteger index;

        stack = AtomicUIntLoad(&renderableFace->_faceStack);
        expected = stack;
        index = FaceStackGetIndex(stack);

        faceNode = (index == 0 ? NULL : &renderableFace->_facePool[index - 1]);
    } while (faceNode && !AtomicUIntCompareAndSet(&renderableFace->_faceStack, &expected,
        FaceStackMake(FaceStackGetVersion(stack) + 1, AtomicUIntLoad(&faceNode->next))));

    return faceNode;
}

static void ReattachFaceNode(RenderableFaceRef renderableFace, FaceNodeRef faceNode)
{
    TRUInteger index = (faceNode - renderableFace->_facePool) + 1;
    TRUInteger stack;
    TRUInteger expected;

    do {
        stack = AtomicUIntLoad(&renderableFace->_faceStack);
        expected = stack;
        AtomicUIntStore(&faceNode->next, FaceStackGetIndex(stack));
    } while (!AtomicUIntCompareAndSet(&renderableFace->_faceStack, &expected,
        FaceStackMake(FaceStackGetVersion(stack) + 1, index)));
}

static void GetUsableFace(RenderableFaceRef renderableFace, UsableFace *usableFace)
{
    FaceNodeRef faceNode = DetachFaceNode(renderableFace);

    if (faceNode) {
        if (!faceNode->ftFace) {
            faceNode->ftFace = TRFontFileCreateFTFace(renderableFace->_fontFile,
                renderableFace->faceIndex);
        }

        usableFace->_faceNode = faceNode;
        usableFace->ftFace = faceNode->ftFace;
    } else {
        usableFace->_faceNode = NULL;

        MutexLock(&renderableFace->_fallbackMutex);

        if (!renderableFace->_fallbackFace) {
            renderableFace->_fallbackFace = TRFontFileCreateFTFace(renderableFace->_fontFile,
                renderableFace->faceIndex);
        }

        usableFace->ftFace = renderableFace->_fallbackFace;
    }
}

static void YieldUsableFace(RenderableFaceRef renderableFace, UsableFace *usableFace)
{
    FaceNodeRef faceNode = usableFace->_faceNode;

    if (faceNode) {
        ReattachFaceNode(renderableFace, faceNode);
    } else {
        MutexUnlock(&renderableFace->_fallbackMutex);
    }
}

static void ActivateVariation(FT_Face ftFace, FT_Fixed *coordinatesPtr, FT_UInt coordinateCount)
{
    if (FT_HAS_MULTIPLE_MASTERS(ftFace) && coordinatesPtr && coordinateCount > 0) {
        FT_Set_Var_Design_Coordinates(ftFace, coordinateCount, coordinatesPtr);
    }
}

static void ActivateSize(FT_Face ftFace, FT_F26Dot6 pixelWidth, FT_F26Dot6 pixelHeight)
{
    FT_Set_Char_Size(ftFace, pixelWidth, pixelHeight, 0, 0);
}

static void ActivateTransform(FT_Face ftFace, FT_Matrix transform)
{
    FT_Set_Transform(ftFace, &transform, NULL);
}

static void ActivatePalette(FT_Face ftFace, FT_Color *colorsPtr, FT_UInt colorCount)
{
    if (colorCount > 0) {
        FT_Color *palette = NULL;
        FT_Error error;

        error = FT_Palette_Select(ftFace, 0, &palette);
        if (error == FT_Err_Ok) {
            memcpy(palette, colorsPtr, colorCount * sizeof(FT_Color));
        }
    }
}

static void ActivateFont(FT_Face ftFace, const FontParams *fontParams, TRBoolean needsPalette)
{
    ActivateVariation(ftFace, fontParams->coordinatesPtr, fontParams->coordinateCount);
    ActivateSize(ftFace, fontParams->pixelWidth, fontParams->pixelHeight);
    ActivateTransform(ftFace, fontParams->transform);

    if (needsPalette) {
        ActivatePalette(ftFace, fontParams->colorsPtr, fontParams->colorCount);
    }
}

static void FinalizeFreeTypeFaces(RenderableFaceRef renderableFace)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    FaceNode *facePool = renderableFace->_facePool;
    FT_Face fallbackFace = renderableFace->_fallbackFace;
    TRUInteger index;

    MutexLock(&freetype->mutex);

    for (index = 0; index < RawFacePoolSize; index++) {
        FT_Face ftFace = facePool[index].ftFace;

        if (ftFace) {
            FT_Done_Face(ftFace);
        }
    }

    if (fallbackFace) {
        FT_Done_Face(fallbackFace);
    }

    MutexUnlock(&freetype->mutex);
}

static void FinalizeRenderableFace(ObjectRef object)
{
    RenderableFaceRef renderableFace = object;

    FaceMetadataRelease(renderableFace->metadata);
    FinalizeFreeTypeFaces(renderableFace);
    TRFontFileRelease(renderableFace->_fontFile);
}

static TRBoolean AreCoordinatesEqual(const TRFloat *first, const TRFloat *second,
    TRUInteger count)
{
    const TRFloat minValue = 1.0 / (TRFloat)0x10000;
    TRBoolean areEqual = TRTrue;
    TRUInteger index;

    for (index = 0; areEqual && index < count; index++) {
        if (fabs(first[index] - second[index]) >= minValue) {
            areEqual = TRFalse;
        }
    }

    return areEqual;
}

/* Returns the name of the first named style that has the coordinates, or `NULL` if none has. */
static const TRStringView *FindStyleName(FaceMetadataRef metadata,
    const TRFloat *variationCoordinates)
{
    const TRStringView *styleName = NULL;
    TRUInteger axisCount = metadata->variationAxisCount;
    TRUInteger styleCount = metadata->namedStyleCount;
    TRUInteger styleIndex;

    for (styleIndex = 0; styleIndex < styleCount; styleIndex++) {
        const TRNamedStyle *namedStyle = &metadata->namedStylesPtr[styleIndex];

        if (namedStyle->subfamilyName
                && AreCoordinatesEqual(variationCoordinates, namedStyle->coordinatesPtr,
                    axisCount)) {
            styleName = namedStyle->subfamilyName;
            break;
        }
    }

    return styleName;
}

static void ApplyVariation(FaceDescription *description, FaceMetadataRef metadata,
    const TRFloat *variationCoordinates)
{
    TRUInteger axisCount = metadata->variationAxisCount;
    TRUInteger index;

    for (index = 0; index < axisCount; index++) {
        const TRVariationAxis *axis = &metadata->variationAxesPtr[index];
        TRFloat coordinate = variationCoordinates[index];

        switch (axis->tag) {
        case TRTagMake('i', 't', 'a', 'l'):
            description->slope = GetSlopeFromITALCoordinate(coordinate);
            break;

        case TRTagMake('s', 'l', 'n', 't'):
            description->slope = GetSlopeFromSLNTCoordinate(coordinate);
            break;

        case TRTagMake('w', 'd', 't', 'h'):
            description->width = GetWidthFromWDTHCoordinate(coordinate);
            break;

        case TRTagMake('w', 'g', 'h', 't'):
            description->weight = GetWeightFromWGHTCoordinate(coordinate);
            break;
        }
    }
}

TR_INTERNAL RenderableFaceRef RenderableFaceCreate(TRFontFileRef fontFile, TRUInteger faceIndex)
{
    FT_Face ftFace = TRFontFileCreateFTFace(fontFile, faceIndex);
    RenderableFace *renderableFace = NULL;

    if (ftFace) {
        const TRUInteger size = sizeof(RenderableFace);
        void *pointer = NULL;

        renderableFace = ObjectCreate(&size, 1, &pointer, FinalizeRenderableFace);

        if (renderableFace) {
            renderableFace->_fontFile = TRFontFileRetain(fontFile);
            renderableFace->faceIndex = faceIndex;

            InitializeFacePool(renderableFace->_facePool, RawFacePoolSize);
            renderableFace->_facePool[0].ftFace = ftFace;
            AtomicUIntInit(&renderableFace->_faceStack, FaceStackMake(0, 1));

            MutexInit(&renderableFace->_fallbackMutex);
            renderableFace->_fallbackFace = NULL;

            renderableFace->metadata = FaceMetadataCreate(ftFace);
            renderableFace->glyphCount = ftFace->num_glyphs;
        }
    }

    return renderableFace;
}

TR_INTERNAL void RenderableFaceCopyTable(RenderableFaceRef renderableFace, TRTag tag,
    void **buffer, TRUInteger *size)
{
    UsableFace usableFace;
    FT_ULong length;

    GetUsableFace(renderableFace, &usableFace);

    *buffer = NULL;
    *size = 0;

    length = 0;
    FT_Load_Sfnt_Table(usableFace.ftFace, tag, 0, NULL, &length);

    if (length > 0) {
        FT_Byte *data = AllocatorAllocateBlock(length);

        if (data && FT_Load_Sfnt_Table(usableFace.ftFace, tag, 0, data, &length) == FT_Err_Ok) {
            *buffer = data;
            *size = length;
        } else {
            AllocatorDeallocateBlock(data);
        }
    }

    YieldUsableFace(renderableFace, &usableFace);
}

TR_INTERNAL TRUInteger RenderableFaceCopyGlyphName(RenderableFaceRef renderableFace,
    TRGlyphID glyphID, char *buffer, TRUInteger capacity)
{
    TRUInteger nameLength = 0;

    if (capacity > 0) {
        UsableFace usableFace;
        FT_Error error;

        GetUsableFace(renderableFace, &usableFace);

        error = FT_Get_Glyph_Name(usableFace.ftFace, glyphID, buffer, (FT_UInt)capacity);

        YieldUsableFace(renderableFace, &usableFace);

        /* FreeType null-terminates the name, and leaves it empty if the font has none. */
        if (error == FT_Err_Ok) {
            nameLength = (TRUInteger)strlen(buffer);
        } else {
            buffer[0] = '\0';
        }
    }

    return nameLength;
}

TR_INTERNAL TRBoolean RenderableFaceSearchEnglishName(RenderableFaceRef renderableFace,
    TRUInt16 nameID, NameString *nameString)
{
    UsableFace usableFace;
    TRBoolean recordFound;

    GetUsableFace(renderableFace, &usableFace);

    recordFound = SearchEnglishName(usableFace.ftFace, nameID, nameString);

    YieldUsableFace(renderableFace, &usableFace);

    return recordFound;
}

TR_INTERNAL TRGlyphID RenderableFaceGetCodePointGlyphID(RenderableFaceRef renderableFace,
    TRUInt32 codePoint)
{
    UsableFace usableFace;
    FT_UInt index;

    GetUsableFace(renderableFace, &usableFace);

    index = FT_Get_Char_Index(usableFace.ftFace, codePoint);

    YieldUsableFace(renderableFace, &usableFace);

    return (index <= 0xFFFF ? (TRGlyphID)index : 0);
}

TR_INTERNAL TRGlyphID RenderableFaceGetVariantGlyphID(RenderableFaceRef renderableFace,
    TRUInt32 codePoint, TRUInt32 variantSelector)
{
    UsableFace usableFace;
    FT_UInt index;

    GetUsableFace(renderableFace, &usableFace);

    index = FT_Face_GetCharVariantIndex(usableFace.ftFace, codePoint, variantSelector);

    YieldUsableFace(renderableFace, &usableFace);

    return (index <= 0xFFFF ? (TRGlyphID)index : 0);
}

TR_INTERNAL void RenderableFaceGetDescription(RenderableFaceRef renderableFace,
    const TRFloat *variationCoordinates, const TRStringView **subfamilyName,
    FaceDescription *description)
{
    FaceMetadataRef metadata = renderableFace->metadata;

    if (subfamilyName) {
        if (variationCoordinates) {
            *subfamilyName = FindStyleName(metadata, variationCoordinates);
        } else {
            *subfamilyName = metadata->subfamilyName;
        }
    }

    if (description) {
        description->weight = metadata->weight;
        description->width = metadata->width;
        description->slope = metadata->slope;

        if (variationCoordinates) {
            ApplyVariation(description, metadata, variationCoordinates);
        }
    }
}

TR_INTERNAL void RenderableFaceGetMetrics(RenderableFaceRef renderableFace,
    FT_Fixed *variationCoordinates, FaceMetrics *metrics)
{
    FaceMetadataRef metadata = renderableFace->metadata;
    TRUInteger axisCount = metadata->variationAxisCount;
    UsableFace usableFace;

    GetUsableFace(renderableFace, &usableFace);
    ActivateVariation(usableFace.ftFace, variationCoordinates, axisCount);

    if (metrics) {
        TRInt32 extent = usableFace.ftFace->ascender - usableFace.ftFace->descender;
        const TT_OS2 *os2Table;

        metrics->unitsPerEM = usableFace.ftFace->units_per_EM;
        metrics->ascent = usableFace.ftFace->ascender;
        metrics->descent = -usableFace.ftFace->descender;
        metrics->leading = 0;
        if (usableFace.ftFace->height > extent) {
            metrics->leading = usableFace.ftFace->height - extent;
        }
        metrics->underlinePosition = usableFace.ftFace->underline_position;
        metrics->underlineThickness = usableFace.ftFace->underline_thickness;

        metrics->strikeoutPosition = 0;
        metrics->strikeoutThickness = 0;
        os2Table = FT_Get_Sfnt_Table(usableFace.ftFace, FT_SFNT_OS2);
        if (os2Table) {
            metrics->strikeoutPosition = os2Table->yStrikeoutPosition;
            metrics->strikeoutThickness = os2Table->yStrikeoutSize;
        }

        metrics->xMin = usableFace.ftFace->bbox.xMin;
        metrics->yMin = usableFace.ftFace->bbox.yMin;
        metrics->xMax = usableFace.ftFace->bbox.xMax;
        metrics->yMax = usableFace.ftFace->bbox.yMax;
    }

    YieldUsableFace(renderableFace, &usableFace);
}

TR_INTERNAL TRInt32 RenderableFaceGetGlyphAdvance(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID)
{
    return RenderableFaceGetDirectionalAdvance(renderableFace, fontParams, glyphID, TRFalse);
}

TR_INTERNAL TRInt32 RenderableFaceGetDirectionalAdvance(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID, TRBoolean isVertical)
{
    FT_Int32 loadFlags = FT_LOAD_NO_SCALE;
    FT_Fixed advance = 0;
    UsableFace usableFace;

    if (isVertical) {
        loadFlags |= FT_LOAD_VERTICAL_LAYOUT;
    }

    GetUsableFace(renderableFace, &usableFace);
    ActivateFont(usableFace.ftFace, fontParams, TRFalse);

    FT_Get_Advance(usableFace.ftFace, glyphID, loadFlags, &advance);

    YieldUsableFace(renderableFace, &usableFace);

    return advance;
}

TR_INTERNAL TRPathRef RenderableFaceCreateGlyphPath(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID)
{
    TRPathRef path = NULL;
    UsableFace usableFace;

    GetUsableFace(renderableFace, &usableFace);
    ActivateFont(usableFace.ftFace, fontParams, TRFalse);

    if (FT_Load_Glyph(usableFace.ftFace, glyphID, FT_LOAD_NO_BITMAP) == FT_Err_Ok
            && usableFace.ftFace->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        path = TRPathCreateFromOutline(&usableFace.ftFace->glyph->outline);
    }

    YieldUsableFace(renderableFace, &usableFace);

    return path;
}

TR_INTERNAL GlyphType RenderableFaceGetGlyphType(RenderableFaceRef renderableFace, TRGlyphID glyphID)
{
    FT_UInt layerGlyphID = 0;
    FT_UInt colorIndex = 0;
    GlyphType glyphType = GlyphTypeMask;
    TRBoolean isColored = TRFalse;
    TRBoolean hasMask = TRFalse;
    FT_LayerIterator iterator;
    UsableFace usableFace;

    GetUsableFace(renderableFace, &usableFace);

    iterator.num_layers = 0;
    iterator.layer = 0;
    iterator.p = NULL;

    while (FT_Get_Color_Glyph_Layer(usableFace.ftFace, glyphID, &layerGlyphID, &colorIndex,
            &iterator)) {
        isColored = TRTrue;

        /* A layer with this color index is painted with the foreground color. */
        if (colorIndex == 0xFFFF) {
            hasMask = TRTrue;
            break;
        }
    }

    YieldUsableFace(renderableFace, &usableFace);

    if (isColored) {
        glyphType = (hasMask ? GlyphTypeMixed : GlyphTypeColor);
    }

    return glyphType;
}

TR_INTERNAL GlyphBitmapRef RenderableFaceRasterizeGlyph(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID, FT_Color foregroundColor)
{
    GlyphBitmapRef bitmap = NULL;
    UsableFace usableFace;
    FT_Error error;

    GetUsableFace(renderableFace, &usableFace);
    ActivateFont(usableFace.ftFace, fontParams, TRTrue);

    FT_Palette_Set_Foreground_Color(usableFace.ftFace, foregroundColor);

    error = FT_Load_Glyph(usableFace.ftFace, glyphID, FT_LOAD_COLOR | FT_LOAD_RENDER);
    if (error == FT_Err_Ok) {
        bitmap = GlyphBitmapCreateFromSlot(usableFace.ftFace->glyph);
    }

    YieldUsableFace(renderableFace, &usableFace);

    return bitmap;
}

TR_INTERNAL RenderableFaceRef RenderableFaceRetain(RenderableFaceRef renderableFace)
{
    return ObjectRetain((ObjectRef)renderableFace);
}

TR_INTERNAL void RenderableFaceRelease(RenderableFaceRef renderableFace)
{
    ObjectRelease((ObjectRef)renderableFace);
}
