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
#include <Tehreer/TRFontFile.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRNamedStyle.h>
#include <Tehreer/TRPalette.h>
#include <Tehreer/TRPath.h>
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

/**
 * Flags of a variation axis.
 */
enum {
    TRVariationAxisFlagHidden = 0x0001  /**< The axis should not be shown in user interfaces. */
};

typedef struct _TRVariationAxis {
    const TRStringView *name;
    TRTag tag;
    TRUInt32 flags;
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

/**
 * Creates a typeface from a face of the font file, using the default variation coordinates and the
 * first predefined palette.
 *
 * @param fontFile
 *      The font file that contains the face.
 * @param faceIndex
 *      Index of the face, less than `TRFontFileGetFaceCount()`.
 * @return
 *      New typeface, or `NULL` if the index is out of range or the face cannot be loaded.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceCreate(TRFontFileRef fontFile, TRUInteger faceIndex);

/**
 * Creates a variation instance of a typeface. The new typeface shares the font data with the
 * source and keeps its colors.
 *
 * @param typeface
 *      Source typeface.
 * @param coordinates
 *      Coordinates in the order of the variation axes. Each one is clamped to the range of its
 *      axis. Missing coordinates (when `count` is less than the axis count, or `coordinates` is
 *      `NULL`) take the default values of their axes.
 * @param count
 *      Number of values in `coordinates`.
 * @return
 *      New typeface, or `NULL` if the typeface has no variation axes or on failure.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceCreateWithVariation(TRTypefaceRef typeface,
    const TRFloat *coordinates, TRUInteger count);

/**
 * Creates a color instance of a typeface. The new typeface shares the font data with the source
 * and keeps its variation coordinates.
 *
 * @param typeface
 *      Source typeface.
 * @param colors
 *      Colors for the palette entries, in order. Missing colors (when `count` is less than the
 *      palette entry count, or `colors` is `NULL`) are opaque black.
 * @param count
 *      Number of values in `colors`.
 * @return
 *      New typeface, or `NULL` if the typeface has no palette entries or on failure.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceCreateWithColors(TRTypefaceRef typeface, const TRColor *colors,
    TRUInteger count);

/**
 * Returns the family name, or `NULL` if the font has none in a supported encoding.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetFamilyName(TRTypefaceRef typeface);

/**
 * Returns the subfamily (style) name; it follows the variation coordinates when they match a named
 * style. `NULL` if there is none.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetSubfamilyName(TRTypefaceRef typeface);

/**
 * Returns the full name, or `NULL` if the font has none in a supported encoding.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetFullName(TRTypefaceRef typeface);

/**
 * Returns the bounding box that contains all glyphs, in font units with the y axis pointing up.
 */
TR_PUBLIC TRRect TRTypefaceGetBoundingBox(TRTypefaceRef typeface);

/**
 * Returns the position of the underline relative to the baseline, in font units.
 */
TR_PUBLIC TRInt32 TRTypefaceGetUnderlinePosition(TRTypefaceRef typeface);

/**
 * Returns the thickness of the underline, in font units.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetUnderlineThickness(TRTypefaceRef typeface);

/**
 * Returns the position of the strikeout relative to the baseline, in font units. It is zero if the
 * font has no `OS/2` table.
 */
TR_PUBLIC TRInt32 TRTypefaceGetStrikeoutPosition(TRTypefaceRef typeface);

/**
 * Returns the thickness of the strikeout, in font units. It is zero if the font has no `OS/2`
 * table.
 */
TR_PUBLIC TRInt32 TRTypefaceGetStrikeoutThickness(TRTypefaceRef typeface);

/**
 * Returns the glyph that represents a code point.
 *
 * @return
 *      The glyph ID, or 0 (the missing glyph) if the typeface has no glyph for it.
 */
TR_PUBLIC TRGlyphID TRTypefaceGetGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint);

/**
 * Returns the glyph that represents a code point followed by a variation selector.
 *
 * @return
 *      The glyph ID, or 0 if the typeface has no such variant.
 */
TR_PUBLIC TRGlyphID TRTypefaceGetVariantGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint,
    TRUInt32 variantSelector);

/**
 * Returns the unhinted advance of a glyph at the given size, following the variation coordinates of
 * the typeface.
 *
 * @param typeSize
 *      Size of the em square, in the unit the caller wants the advance in.
 * @param isVertical
 *      `TRTrue` for the vertical advance.
 */
TR_PUBLIC TRFloat TRTypefaceGetGlyphAdvance(TRTypefaceRef typeface, TRGlyphID glyphID,
    TRFloat typeSize, TRBoolean isVertical);

/**
 * Creates the outline of a glyph. The origin is at the glyph's pen position on the baseline, and
 * the y axis points downward.
 *
 * @param typeSize
 *      Size of the em square.
 * @return
 *      New path, empty for glyphs without an outline such as a space, or `NULL` on failure.
 */
TR_PUBLIC TRPathRef TRTypefaceCreateGlyphPath(TRTypefaceRef typeface, TRGlyphID glyphID,
    TRFloat typeSize);

/**
 * Copies the data of a table of the font.
 *
 * @param typeface
 *      The typeface.
 * @param tag
 *      The tag of the table.
 * @param buffer
 *      Receives the data of the table, or `NULL` if only its size is wanted.
 * @param capacity
 *      The number of bytes that `buffer` can hold. A table that is bigger is cut short.
 * @return
 *      The size of the table in bytes, which is zero if the font has no such table.
 */
TR_PUBLIC TRUInteger TRTypefaceGetTableData(TRTypefaceRef typeface, TRTag tag, void *buffer,
    TRUInteger capacity);

/**
 * Copies the name of a glyph, as the font gives it in its post table, or its charset if it is a CFF
 * font.
 *
 * @param typeface
 *      The typeface.
 * @param glyphID
 *      The ID of the glyph.
 * @param buffer
 *      Receives the name as a null-terminated string of ASCII characters. A name that does not fit
 *      is cut short.
 * @param capacity
 *      The number of bytes that `buffer` can hold, including the terminator.
 * @return
 *      The length of the name copied to `buffer`, not counting the terminator. It is zero if the
 *      font has no name for the glyph, or `capacity` is zero.
 */
TR_PUBLIC TRUInteger TRTypefaceGetGlyphName(TRTypefaceRef typeface, TRGlyphID glyphID,
    char *buffer, TRUInteger capacity);

TR_PUBLIC TRTypefaceRef TRTypefaceRetain(TRTypefaceRef typeface);

TR_PUBLIC void TRTypefaceRelease(TRTypefaceRef typeface);

TR_EXTERN_C_END

#endif
