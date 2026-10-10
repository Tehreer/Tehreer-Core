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

#ifndef _TEHREER_RENDERABLE_FACE_H
#define _TEHREER_RENDERABLE_FACE_H

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/AtomicUInt.h>
#include <Core/Data.h>
#include <Core/Mutex.h>
#include <Core/Object.h>
#include <Font/FaceMetadata.h>
#include <Font/FontData.h>
#include <Graphics/GlyphBitmap.h>
#include <SFNT/Utilities.h>

typedef struct _FaceNode {
    FT_Face ftFace;
    AtomicUInt next;
} FaceNode, *FaceNodeRef;

#define RawFacePoolSize 3

typedef struct _RenderableFace {
    ObjectBase _base;

    FontDataRef _fontData;
    TRUInteger faceIndex;

    FaceNode _facePool[RawFacePoolSize];
    AtomicUInt _faceStack;

    Mutex _fallbackMutex;
    FT_Face _fallbackFace;

    FaceMetadataRef metadata;
    TRUInteger glyphCount;
} RenderableFace, *RenderableFaceRef;

/*
 * How the image of a glyph depends on the foreground color. A mask glyph is painted only with the
 * foreground color, a color glyph never uses it, and a mixed one has layers of both kinds. The
 * type is unknown until it is looked up.
 */
enum {
    GlyphTypeUnknown,
    GlyphTypeMask,
    GlyphTypeColor,
    GlyphTypeMixed
};
typedef TRUInt32 GlyphType;

typedef struct _FaceDescription {
    TRWeight weight;
    TRWidth width;
    TRSlope slope;
} FaceDescription;

typedef struct _FaceMetrics {
    TRUInt32 unitsPerEM;
    TRUInt32 ascent;
    TRUInt32 descent;
    TRUInt32 leading;
    TRInt32 underlinePosition;
    TRUInt32 underlineThickness;
    TRInt32 strikeoutPosition;
    TRInt32 strikeoutThickness;
    TRInt32 xMin;
    TRInt32 yMin;
    TRInt32 xMax;
    TRInt32 yMax;
} FaceMetrics;

typedef struct _FontParams {
    FT_Fixed *coordinatesPtr;
    FT_UInt coordinateCount;
    FT_Color *colorsPtr;
    FT_UInt colorCount;
    FT_F26Dot6 pixelWidth;
    FT_F26Dot6 pixelHeight;
    FT_Matrix transform;
} FontParams;

TR_INTERNAL RenderableFaceRef RenderableFaceCreate(FontDataRef fontData, TRUInteger faceIndex);

TR_INTERNAL void RenderableFaceCopyTable(RenderableFaceRef renderableFace, TRTag tag,
    void **buffer, TRUInteger *size);

TR_INTERNAL TRUInteger RenderableFaceGetTableSize(RenderableFaceRef renderableFace, TRTag tag);

TR_INTERNAL TRUInteger RenderableFaceReadTable(RenderableFaceRef renderableFace, TRTag tag,
    TRUInteger offset, void *buffer, TRUInteger capacity);

TR_INTERNAL TRUInteger RenderableFaceCopyGlyphName(RenderableFaceRef renderableFace,
    TRGlyphID glyphID, char *buffer, TRUInteger capacity);

TR_INTERNAL TRBoolean RenderableFaceSearchEnglishName(RenderableFaceRef renderableFace,
    TRUInt16 nameID, NameString *nameString);

TR_INTERNAL TRGlyphID RenderableFaceGetCodePointGlyphID(RenderableFaceRef renderableFace,
    TRUInt32 codePoint);
TR_INTERNAL TRGlyphID RenderableFaceGetVariantGlyphID(RenderableFaceRef renderableFace,
    TRUInt32 codePoint, TRUInt32 variantSelector);

TR_INTERNAL void RenderableFaceGetDescription(RenderableFaceRef renderableFace,
    const TRFloat *variationCoordinates, const TRStringView **subfamilyName,
    FaceDescription *description);
TR_INTERNAL void RenderableFaceGetMetrics(RenderableFaceRef renderableFace,
    FT_Fixed *variationCoordinates, FaceMetrics *metrics);

TR_INTERNAL TRInt32 RenderableFaceGetGlyphAdvance(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID);

/*
 * Returns the advance of a glyph in font units, along the vertical axis if `isVertical` is true.
 */
TR_INTERNAL TRInt32 RenderableFaceGetDirectionalAdvance(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID, TRBoolean isVertical);

/*
 * Creates the outline of a glyph at the size given in `fontParams`, in 26.6 fixed point format.
 * Returns NULL if the glyph has no outline or cannot be loaded.
 */
TR_INTERNAL TRPathRef RenderableFaceCreateGlyphPath(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID);

TR_INTERNAL GlyphType RenderableFaceGetGlyphType(RenderableFaceRef renderableFace, TRGlyphID glyphID);

TR_INTERNAL GlyphBitmapRef RenderableFaceRasterizeGlyph(RenderableFaceRef renderableFace,
    const FontParams *fontParams, TRGlyphID glyphID, FT_Color foregroundColor);

TR_INTERNAL RenderableFaceRef RenderableFaceRetain(RenderableFaceRef renderableFace);
TR_INTERNAL void RenderableFaceRelease(RenderableFaceRef renderableFace);

#endif
