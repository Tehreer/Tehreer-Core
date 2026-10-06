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
    TRUInteger index;

    do {
        stack = AtomicUIntLoad(&renderableFace->_faceStack);
        expected = stack;
        index = FaceStackGetIndex(stack);

        if (index == 0) {
            faceNode = NULL;
            break;
        }

        faceNode = &renderableFace->_facePool[index - 1];
    } while (!AtomicUIntCompareAndSet(&renderableFace->_faceStack, &expected,
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
    TRGlyphID glyphID;

    GetUsableFace(renderableFace, &usableFace);

    glyphID = FT_Get_Char_Index(usableFace.ftFace, codePoint);

    YieldUsableFace(renderableFace, &usableFace);

    return glyphID;
}

TR_INTERNAL TRGlyphID RenderableFaceGetVariantGlyphID(RenderableFaceRef renderableFace,
    TRUInt32 codePoint, TRUInt32 variantSelector)
{
    UsableFace usableFace;
    TRGlyphID glyphID;

    GetUsableFace(renderableFace, &usableFace);

    glyphID = FT_Face_GetCharVariantIndex(usableFace.ftFace, codePoint, variantSelector);

    YieldUsableFace(renderableFace, &usableFace);

    return glyphID;
}

TR_INTERNAL void RenderableFaceGetDescription(RenderableFaceRef renderableFace,
    const TRFloat *variationCoordinates, const TRStringView **subfamilyName,
    FaceDescription *description)
{
    const TRFloat minValue = 1.0 / (TRFloat)0x10000;
    FaceMetadataRef metadata = renderableFace->metadata;
    TRUInteger axisCount = metadata->variationAxisCount;

    if (subfamilyName) {
        *subfamilyName = NULL;

        if (variationCoordinates) {
            TRUInteger styleCount = metadata->namedStyleCount;
            TRUInteger styleIndex;

            for (styleIndex = 0; styleIndex < styleCount; styleIndex++) {
                const TRNamedStyle *namedStyle = &metadata->namedStylesPtr[styleIndex];
                const TRFloat *styleCoordinates = namedStyle->coordinatesPtr;
                TRBoolean matched = TRTrue;
                TRUInteger index;

                if (!namedStyle->subfamilyName) {
                    continue;
                }

                for (index = 0; index < axisCount; index++) {
                    if (fabs(variationCoordinates[index] - styleCoordinates[index]) >= minValue) {
                        matched = TRFalse;
                        break;
                    }
                }

                if (matched) {
                    *subfamilyName = namedStyle->subfamilyName;
                    break;
                }
            }
        } else {
            *subfamilyName = metadata->subfamilyName;
        }
    }

    if (description) {
        description->weight = metadata->weight;
        description->width = metadata->width;
        description->slope = metadata->slope;

        if (variationCoordinates) {
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

        metrics->unitsPerEM = usableFace.ftFace->units_per_EM;
        metrics->ascent = usableFace.ftFace->ascender;
        metrics->descent = -usableFace.ftFace->descender;
        metrics->leading = 0;
        if (usableFace.ftFace->height > extent) {
            metrics->leading = usableFace.ftFace->height - extent;
        }
        metrics->underlinePosition = usableFace.ftFace->underline_position;
        metrics->underlineThickness = usableFace.ftFace->underline_thickness;
    }

    YieldUsableFace(renderableFace, &usableFace);
}

TR_INTERNAL TRInt32 RenderableFaceGetGlyphAdvance(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID)
{
    FT_Fixed advance = 0;
    UsableFace usableFace;

    GetUsableFace(renderableFace, &usableFace);
    ActivateFont(usableFace.ftFace, fontParams, TRFalse);

    FT_Get_Advance(usableFace.ftFace, glyphID, FT_LOAD_NO_SCALE, &advance);

    YieldUsableFace(renderableFace, &usableFace);

    return advance;
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
