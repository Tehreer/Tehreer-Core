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

#ifndef _TEHREER_TYPEFACE_H
#define _TEHREER_TYPEFACE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRNamedStyle.h>
#include <Tehreer/TRPalette.h>
#include <Tehreer/TRVariationAxis.h>

TR_EXTERN_C_BEGIN

enum {
    TRWeightThin = 100,
    TRWeightExtraLight = 200,
    TRWeightLight = 300,
    TRWeightRegular = 400,
    TRWeightMedium = 500,
    TRWeightSemiBold = 600,
    TRWeightBold = 700,
    TRWeightExtraBold = 800,
    TRWeightExtraHeavy = 900
};
typedef TRUInt32 TRWeight;

enum {
    TRWidthUltraCondensed = 1,
    TRWidthExtraCondensed = 2,
    TRWidthCondensed = 3,
    TRWidthSemiCondensed = 4,
    TRWidthNormal = 5,
    TRWidthSemiExpanded = 6,
    TRWidthExpanded = 7,
    TRWidthExtraExpanded = 8,
    TRWidthUltraExpanded = 9
};
typedef TRUInt32 TRWidth;

enum {
    TRSlopePlain = 0,
    TRSlopeItalic = 1,
    TRSlopeOblique = 2
};
typedef TRUInt32 TRSlope;

typedef struct _TRVariationAxis {
    const TRStringView *name;
    TRTag tag;
    TRFloat minValue;
    TRFloat maxValue;
    TRFloat defaultValue;
} TRVariationAxis;

typedef struct _TRNamedStyle {
    const TRStringView *subfamilyName;
    const TRFloat *coordinatesPtr;
    TRUInteger coordinateCount;
    const TRStringView *postScriptName;
} TRNamedStyle;

typedef struct _TRPaletteEntry {
    const TRStringView *name;
} TRPaletteEntry;

typedef struct _TRPredefinedPalette {
    const TRStringView *name;
    const TRColor *colorsPtr;
    TRUInteger colorCount;
    TRUInt32 flags;
} TRPredefinedPalette;

typedef const struct _TRTypeface *TRTypefaceRef;

TR_PUBLIC TRWeight TRTypefaceGetWeight(TRTypefaceRef typeface);

TR_PUBLIC TRWidth TRTypefaceGetWidth(TRTypefaceRef typeface);

TR_PUBLIC TRSlope TRTypefaceGetSlope(TRTypefaceRef typeface);

TR_PUBLIC TRUInt32 TRTypefaceGetUnitsPerEM(TRTypefaceRef typeface);

TR_PUBLIC TRUInt32 TRTypefaceGetAscent(TRTypefaceRef typeface);

TR_PUBLIC TRUInt32 TRTypefaceGetDescent(TRTypefaceRef typeface);

TR_PUBLIC TRUInt32 TRTypefaceGetLeading(TRTypefaceRef typeface);

TR_PUBLIC TRUInteger TRTypefaceGetGlyphCount(TRTypefaceRef typeface);

TR_PUBLIC const TRVariationAxis *TRTypefaceGetVariationAxesPtr(TRTypefaceRef typeface);

TR_PUBLIC TRUInteger TRTypefaceGetVariationAxisCount(TRTypefaceRef typeface);

TR_PUBLIC const TRNamedStyle *TRTypefaceGetNamedStylesPtr(TRTypefaceRef typeface);

TR_PUBLIC TRUInteger TRTypefaceGetNamedStyleCount(TRTypefaceRef typeface);

TR_PUBLIC const TRPaletteEntry *TRTypefaceGetPaletteEntriesPtr(TRTypefaceRef typeface);

TR_PUBLIC TRUInteger TRTypefaceGetPaletteEntryCount(TRTypefaceRef typeface);

TR_PUBLIC const TRPredefinedPalette *TRTypefaceGetPredefinedPalettesPtr(TRTypefaceRef typeface);

TR_PUBLIC TRUInteger TRTypefaceGetPredefinedPaletteCount(TRTypefaceRef typeface);

TR_PUBLIC const TRFloat *TRTypefaceGetVariationCoordinatesPtr(TRTypefaceRef typeface);

TR_PUBLIC const TRColor *TRTypefaceGetAssociatedColorsPtr(TRTypefaceRef typeface);

TR_PUBLIC TRTypefaceRef TRTypefaceRetain(TRTypefaceRef typeface);

TR_PUBLIC void TRTypefaceRelease(TRTypefaceRef typeface);

TR_EXTERN_C_END

#endif
