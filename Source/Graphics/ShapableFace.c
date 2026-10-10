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

#include <stdlib.h>

#include <hb.h>

#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Core/Once.h>
#include <Font/FaceMetadata.h>
#include <Graphics/AdvanceCache.h>
#include <Graphics/RenderableFace.h>

#include "ShapableFace.h"

#define StrideAt(pointer_, stride_, index_, type_)  \
    ((type_ *)((const TRUInt8 *)(pointer_) + ((TRUInteger)(index_) * (stride_))))

static hb_blob_t *GetFontTableData(hb_face_t *face, hb_tag_t tag, void *object)
{
    ShapableFaceRef shapableFace = object;
    RenderableFaceRef renderableFace = shapableFace->renderableFace;
    void *data;
    TRUInteger length;

    RenderableFaceCopyTable(renderableFace, tag, &data, &length);

    return hb_blob_create(data, length, HB_MEMORY_MODE_WRITABLE, data, AllocatorDeallocateBlock);
}

static void SetupAdvanceParams(ShapableFaceRef shapableFace, FontParams *fontParams)
{
    /* Advances are not scaled, so the size does not matter. */
    fontParams->coordinatesPtr = shapableFace->coordinates;
    fontParams->coordinateCount = (FT_UInt)shapableFace->coordinateCount;
    fontParams->colorsPtr = NULL;
    fontParams->colorCount = 0;
    fontParams->pixelWidth = 16 * 64;
    fontParams->pixelHeight = 16 * 64;
    fontParams->transform.xx = 0x10000;
    fontParams->transform.xy = 0;
    fontParams->transform.yx = 0;
    fontParams->transform.yy = 0x10000;
}

static TRInt32 GetGlyphAdvance(ShapableFaceRef shapableFace, TRGlyphID glyphID)
{
    TRInt32 advance;

    if (!AdvanceCacheGet(&shapableFace->advanceCache, glyphID, &advance)) {
        FontParams fontParams;

        SetupAdvanceParams(shapableFace, &fontParams);
        advance = RenderableFaceGetGlyphAdvance(shapableFace->renderableFace, &fontParams, glyphID);

        AdvanceCachePut(&shapableFace->advanceCache, glyphID, advance);
    }

    return advance;
}

static hb_bool_t GetNominalGlyph(hb_font_t *font, void *object, hb_codepoint_t unicode,
    hb_codepoint_t *glyph, void *userData)
{
    hb_bool_t hasGlyph = 0;
    ShapableFaceRef shapableFace = object;
    TRGlyphID glyphID = RenderableFaceGetCodePointGlyphID(shapableFace->renderableFace, unicode);

    if (glyphID != 0) {
        *glyph = glyphID;
        hasGlyph = 1;
    }

    return hasGlyph;
}

static unsigned int GetNominalGlyphs(hb_font_t *font, void *object, unsigned int count,
    const hb_codepoint_t *firstUnicode, unsigned int unicodeStride, hb_codepoint_t *firstGlyph,
    unsigned int glyphStride, void *userData)
{
    ShapableFaceRef shapableFace = object;
    unsigned int done;

    for (done = 0; done < count; done++) {
        hb_codepoint_t unicode = *StrideAt(firstUnicode, unicodeStride, done, const hb_codepoint_t);
        TRGlyphID glyphID = RenderableFaceGetCodePointGlyphID(shapableFace->renderableFace,
            unicode);

        if (glyphID == 0) {
            break;
        }

        *StrideAt(firstGlyph, glyphStride, done, hb_codepoint_t) = glyphID;
    }

    return done;
}

static hb_bool_t GetVariationGlyph(hb_font_t *font, void *object, hb_codepoint_t unicode,
    hb_codepoint_t variationSelector, hb_codepoint_t *glyph, void *userData)
{
    hb_bool_t hasGlyph = 0;
    ShapableFaceRef shapableFace = object;
    TRGlyphID glyphID = RenderableFaceGetVariantGlyphID(shapableFace->renderableFace, unicode,
        variationSelector);

    if (glyphID != 0) {
        *glyph = glyphID;
        hasGlyph = 1;
    }

    return hasGlyph;
}

static hb_position_t GetGlyphHAdvance(hb_font_t *font, void *object, hb_codepoint_t glyph,
    void *userData)
{
    return GetGlyphAdvance(object, (TRGlyphID)glyph);
}

static void GetGlyphHAdvances(hb_font_t *font, void *object, unsigned int count,
    const hb_codepoint_t *firstGlyph, unsigned int glyphStride, hb_position_t *firstAdvance,
    unsigned int advanceStride, void *userData)
{
    unsigned int index;

    for (index = 0; index < count; index++) {
        hb_codepoint_t glyph = *StrideAt(firstGlyph, glyphStride, index, const hb_codepoint_t);

        *StrideAt(firstAdvance, advanceStride, index, hb_position_t) =
            GetGlyphAdvance(object, (TRGlyphID)glyph);
    }
}

static hb_font_funcs_t *DefaultFontFuncs;

static void InitFontFuncs(void)
{
    hb_font_funcs_t *funcs = hb_font_funcs_create();

    hb_font_funcs_set_nominal_glyph_func(funcs, GetNominalGlyph, NULL, NULL);
    hb_font_funcs_set_nominal_glyphs_func(funcs, GetNominalGlyphs, NULL, NULL);
    hb_font_funcs_set_variation_glyph_func(funcs, GetVariationGlyph, NULL, NULL);
    hb_font_funcs_set_glyph_h_advance_func(funcs, GetGlyphHAdvance, NULL, NULL);
    hb_font_funcs_set_glyph_h_advances_func(funcs, GetGlyphHAdvances, NULL, NULL);
    hb_font_funcs_make_immutable(funcs);

    DefaultFontFuncs = funcs;
}

static hb_font_funcs_t *GetFontFuncs(void)
{
    static Once once = OnceMake();

    OnceExecute(&once, InitFontFuncs);

    return DefaultFontFuncs;
}

static void FinalizeShapableFace(ObjectRef object)
{
    ShapableFaceRef shapableFace = object;

    hb_font_destroy(shapableFace->hbFont);
    AdvanceCacheFinalize(&shapableFace->advanceCache);

    if (shapableFace->rootFace) {
        ShapableFaceRelease(shapableFace->rootFace);
    } else {
        hb_face_destroy(shapableFace->hbFace);
    }

    RenderableFaceRelease(shapableFace->renderableFace);
}

static ShapableFaceRef AllocateShapableFace(TRUInteger coordinateCount)
{
    void *pointers[2] = { NULL };
    TRUInteger sizes[2];
    ShapableFaceRef shapableFace;

    sizes[0] = sizeof(ShapableFace);
    sizes[1] = sizeof(FT_Fixed) * coordinateCount;

    shapableFace = ObjectCreate(sizes, 2, pointers, FinalizeShapableFace);

    if (shapableFace) {
        shapableFace->renderableFace = NULL;
        shapableFace->rootFace = NULL;
        shapableFace->hbFace = NULL;
        shapableFace->hbFont = NULL;
        shapableFace->coordinates = pointers[1];
        shapableFace->coordinateCount = coordinateCount;
    }

    return shapableFace;
}

TR_INTERNAL ShapableFaceRef ShapableFaceCreate(RenderableFaceRef renderableFace)
{
    FaceMetadataRef metadata = renderableFace->metadata;
    ShapableFaceRef shapableFace = AllocateShapableFace(metadata->variationAxisCount);

    if (shapableFace) {
        FaceMetrics metrics;
        TRUInteger index;

        /*
         * The FreeType faces are shared with other shapable faces, and keep the coordinates that
         * were set last. The default coordinates have to be activated explicitly.
         */
        for (index = 0; index < metadata->variationAxisCount; index++) {
            shapableFace->coordinates[index] =
                (FT_Fixed)(metadata->variationAxesPtr[index].defaultValue * 65536.0);
        }

        RenderableFaceGetMetrics(renderableFace, NULL, &metrics);

        shapableFace->renderableFace = RenderableFaceRetain(renderableFace);
        AdvanceCacheInitialize(&shapableFace->advanceCache, renderableFace->glyphCount);

        shapableFace->hbFace = hb_face_create_for_tables(GetFontTableData, shapableFace, NULL);
        hb_face_set_index(shapableFace->hbFace, (unsigned int)renderableFace->faceIndex);
        hb_face_set_upem(shapableFace->hbFace, metrics.unitsPerEM);

        shapableFace->hbFont = hb_font_create(shapableFace->hbFace);
        hb_font_set_funcs(shapableFace->hbFont, GetFontFuncs(), shapableFace, NULL);
    }

    return shapableFace;
}

TR_INTERNAL ShapableFaceRef ShapableFaceCreateDerived(ShapableFaceRef parent,
    const TRFloat *coordinates, TRUInteger coordinateCount)
{
    ShapableFaceRef shapableFace = AllocateShapableFace(coordinateCount);

    if (shapableFace) {
        ShapableFaceRef rootFace = (parent->rootFace ? parent->rootFace : parent);
        TRUInteger index;

        for (index = 0; index < coordinateCount; index++) {
            shapableFace->coordinates[index] = (FT_Fixed)(coordinates[index] * 65536.0);
        }

        shapableFace->renderableFace = RenderableFaceRetain(rootFace->renderableFace);
        shapableFace->rootFace = ShapableFaceRetain(rootFace);
        shapableFace->hbFace = rootFace->hbFace;
        AdvanceCacheInitialize(&shapableFace->advanceCache, rootFace->renderableFace->glyphCount);

        shapableFace->hbFont = hb_font_create_sub_font(rootFace->hbFont);
        hb_font_set_funcs(shapableFace->hbFont, GetFontFuncs(), shapableFace, NULL);

        if (coordinateCount > 0) {
            hb_font_set_var_coords_design(shapableFace->hbFont, coordinates,
                (unsigned int)coordinateCount);
        }
    }

    return shapableFace;
}

TR_INTERNAL ShapableFaceRef ShapableFaceRetain(ShapableFaceRef shapableFace)
{
    return ObjectRetain((ObjectRef)shapableFace);
}

TR_INTERNAL void ShapableFaceRelease(ShapableFaceRef shapableFace)
{
    ObjectRelease((ObjectRef)shapableFace);
}
