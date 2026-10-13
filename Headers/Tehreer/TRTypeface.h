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
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRString.h>

TR_EXTERN_C_BEGIN

/**
 * The weight of a typeface, which is its thickness on the scale of the CSS and OpenType weight
 * classes. A value between the constants is possible, e.g. from the `wght` axis of a variable font.
 */
enum {
    TRWeightThin = 100,         /**< Thin. */
    TRWeightExtraLight = 200,   /**< Extra light, also known as ultra light. */
    TRWeightLight = 300,        /**< Light. */
    TRWeightRegular = 400,      /**< Regular, also known as normal. */
    TRWeightMedium = 500,       /**< Medium. */
    TRWeightSemiBold = 600,     /**< Semi bold, also known as demi bold. */
    TRWeightBold = 700,         /**< Bold. */
    TRWeightExtraBold = 800,    /**< Extra bold, also known as ultra bold. */
    TRWeightExtraHeavy = 900    /**< Extra heavy, also known as black. */
};
/**
 * A type to represent the weight of a typeface, which is one of the `TRWeight*` constants above or
 * a value between them.
 */
typedef TRUInt32 TRWeight;

/**
 * The width of a typeface, which is how much it is condensed or expanded. The constants follow the
 * width classes of OpenType, from 1 to 9.
 */
enum {
    TRWidthUltraCondensed = 1,  /**< Ultra condensed. */
    TRWidthExtraCondensed = 2,  /**< Extra condensed. */
    TRWidthCondensed = 3,       /**< Condensed. */
    TRWidthSemiCondensed = 4,   /**< Semi condensed. */
    TRWidthNormal = 5,          /**< Normal. */
    TRWidthSemiExpanded = 6,    /**< Semi expanded. */
    TRWidthExpanded = 7,        /**< Expanded. */
    TRWidthExtraExpanded = 8,   /**< Extra expanded. */
    TRWidthUltraExpanded = 9    /**< Ultra expanded. */
};
/**
 * A type to represent the width of a typeface, which is one of the `TRWidth*` constants above.
 */
typedef TRUInt32 TRWidth;

/**
 * The slope of a typeface.
 */
enum {
    TRSlopePlain = 0,       /**< Upright. */
    TRSlopeItalic = 1,      /**< Italic. */
    TRSlopeOblique = 2      /**< Upright forms slanted, without being drawn as italic. */
};
/**
 * A type to represent the slope of a typeface, which is one of the `TRSlope*` constants above.
 */
typedef TRUInt32 TRSlope;

/**
 * Flags of a variation axis.
 */
enum {
    TRVariationAxisFlagHidden = 0x0001  /**< The axis should not be shown in user interfaces. */
};

/**
 * An axis along which the design of a variable font varies, such as `wght` or `wdth`. The values of
 * the pointers in it are owned by the typeface that returned it.
 */
typedef struct _TRVariationAxis {
    const TRStringView *name;       /**< The English name of the axis, or `NULL` if it has none. */
    TRTag tag;                      /**< The OpenType tag of the axis, e.g. `wght`. */
    TRUInt32 flags;                 /**< Combination of the `TRVariationAxisFlag` constants. */
    TRFloat minValue;               /**< The least value that the axis can take. */
    TRFloat maxValue;               /**< The greatest value that the axis can take. */
    TRFloat defaultValue;           /**< The value of the axis in the default instance. */
} TRVariationAxis;

/**
 * A size at which a font has the images of its glyphs, such as the `CBDT` and `sbix` fonts of color
 * emoji. The image of a glyph is not drawn from an outline, so it is picked from the strike whose
 * size is the nearest to the one asked for, and the placement of the glyph tells how much it has to
 * be scaled to match that size.
 */
typedef struct _TRBitmapStrike {
    TRFloat pixelWidth;     /**< Number of pixels in the em square, horizontally. */
    TRFloat pixelHeight;    /**< Number of pixels in the em square, vertically. */
} TRBitmapStrike;

/**
 * A named instance of a variable font, such as "Bold" or "Light Italic". The values of the pointers
 * in it are owned by the typeface that returned it.
 */
typedef struct _TRNamedStyle {
    const TRStringView *subfamilyName;  /**< The English style name, or `NULL` if it has none. */
    const TRFloat *coordinatesPtr;      /**< The coordinates in the order of the variation axes. */
    TRUInteger coordinateCount;         /**< The number of coordinates, same as the axis count. */
    const TRStringView *postScriptName; /**< The PostScript name of the style, or `NULL` if the font
                                             gives none. */
} TRNamedStyle;

/**
 * A color slot of the palettes of a color font. The pointer in it is owned by the typeface that
 * returned it.
 */
typedef struct _TRPaletteEntry {
    const TRStringView *name;       /**< The English name of the entry, or `NULL` if it has none. */
} TRPaletteEntry;

/**
 * A set of colors that a color font defines for its palette entries. The values of the pointers in
 * it are owned by the typeface that returned it.
 */
typedef struct _TRPredefinedPalette {
    const TRStringView *name;       /**< The English name, or `NULL` if it has none. */
    const TRColor *colorsPtr;       /**< The colors, one for each palette entry. */
    TRUInteger colorCount;          /**< The number of colors, same as the palette entry count. */
    TRUInt32 flags;                 /**< The flags of the `CPAL` table for the palette: 0x0001
                                         means that it suits a light background and 0x0002 a dark
                                         one. It is zero if the font gives none. */
} TRPredefinedPalette;

/**
 * A reference to an immutable typeface, which is one face of a font file with a given variation
 * and a given set of palette colors. The default typefaces of a font are obtained from a font file
 * with `TRFontFileGetTypeface()`, and other ones are derived from a typeface with
 * `TRTypefaceCreateWithVariation()` or `TRTypefaceCreateWithColors()`.
 *
 * The pointers that its getters return, such as the names and the arrays, are owned by the typeface
 * and stay valid as long as it is alive.
 */
typedef const struct _TRTypeface *TRTypefaceRef;

/**
 * Returns the weight of the typeface. It comes from the font, and follows the `wght` coordinate for
 * a variable font.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The weight of the typeface.
 */
TR_PUBLIC TRWeight TRTypefaceGetWeight(TRTypefaceRef typeface);

/**
 * Returns the width of the typeface. It comes from the font, and follows the `wdth` coordinate for
 * a variable font.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The width of the typeface.
 */
TR_PUBLIC TRWidth TRTypefaceGetWidth(TRTypefaceRef typeface);

/**
 * Returns the slope of the typeface. It comes from the font, and follows the `ital` and `slnt`
 * coordinates for a variable font.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The slope of the typeface.
 */
TR_PUBLIC TRSlope TRTypefaceGetSlope(TRTypefaceRef typeface);

/**
 * Returns the number of font units in the em square. The metrics of the typeface that are in font
 * units, such as the ascent, are relative to this.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of font units in the em square.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetUnitsPerEM(TRTypefaceRef typeface);

/**
 * Returns the distance from the baseline to the top of the line, in font units. It follows the
 * variation coordinates of the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The distance from the baseline to the top of the line, in font units.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetAscent(TRTypefaceRef typeface);

/**
 * Returns the distance from the baseline to the bottom of the line, in font units, as a positive
 * number. It follows the variation coordinates of the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The distance from the baseline to the bottom of the line, in font units, as a positive
 *      number.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetDescent(TRTypefaceRef typeface);

/**
 * Returns the extra space between two lines, in font units, which is the part of the line height
 * of the font that exceeds the sum of the ascent and the descent. It is zero if the font has no
 * such part.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The extra space between two lines, in font units, which is the part of the line height of
 *      the font that exceeds the sum of the ascent and the descent.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetLeading(TRTypefaceRef typeface);

/**
 * Returns the number of glyphs in the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of glyphs in the typeface.
 */
TR_PUBLIC TRUInteger TRTypefaceGetGlyphCount(TRTypefaceRef typeface);

/**
 * Returns the variation axes of the typeface, which are empty if it is not a variable font. Their
 * number is given by `TRTypefaceGetVariationAxisCount()`.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The variation axes of the typeface, which are empty if it is not a variable font.
 */
TR_PUBLIC const TRVariationAxis *TRTypefaceGetVariationAxesPtr(TRTypefaceRef typeface);

/**
 * Returns the number of variation axes of the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of variation axes of the typeface.
 */
TR_PUBLIC TRUInteger TRTypefaceGetVariationAxisCount(TRTypefaceRef typeface);

/**
 * Returns the named styles of the typeface, which are empty if it is not a variable font or has
 * none. Their number is given by `TRTypefaceGetNamedStyleCount()`.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The named styles of the typeface, which are empty if it is not a variable font or has none.
 */
TR_PUBLIC const TRNamedStyle *TRTypefaceGetNamedStylesPtr(TRTypefaceRef typeface);

/**
 * Returns the number of named styles of the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of named styles of the typeface.
 */
TR_PUBLIC TRUInteger TRTypefaceGetNamedStyleCount(TRTypefaceRef typeface);

/**
 * Returns the palette entries of the typeface, which are empty if it is not a color font. Their
 * number is given by `TRTypefaceGetPaletteEntryCount()`.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The palette entries of the typeface, which are empty if it is not a color font.
 */
TR_PUBLIC const TRPaletteEntry *TRTypefaceGetPaletteEntriesPtr(TRTypefaceRef typeface);

/**
 * Returns the number of palette entries of the typeface, which is the number of colors that a
 * palette of it has.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of palette entries of the typeface, which is the number of colors that a palette
 *      of it has.
 */
TR_PUBLIC TRUInteger TRTypefaceGetPaletteEntryCount(TRTypefaceRef typeface);

/**
 * Returns the palettes that the font defines, which are empty if it is not a color font. Their
 * number is given by `TRTypefaceGetPredefinedPaletteCount()`.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The palettes that the font defines, which are empty if it is not a color font.
 */
TR_PUBLIC const TRPredefinedPalette *TRTypefaceGetPredefinedPalettesPtr(TRTypefaceRef typeface);

/**
 * Returns the number of palettes that the font defines.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of palettes that the font defines.
 */
TR_PUBLIC TRUInteger TRTypefaceGetPredefinedPaletteCount(TRTypefaceRef typeface);

/**
 * Tells whether the glyphs of the typeface have outlines. The glyphs of a typeface that is not
 * scalable, such as a `CBDT` color emoji font, only have images at the sizes of its bitmap strikes,
 * so they have no paths and their images may need a scale to match the size that is asked for.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      `TRTrue` if the glyphs have outlines, `TRFalse` if they only have images at the sizes of the
 *      bitmap strikes.
 */
TR_PUBLIC TRBoolean TRTypefaceIsScalable(TRTypefaceRef typeface);

/**
 * Returns the sizes at which the typeface has the images of its glyphs, which is empty for a
 * typeface that has none.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The sizes at which the typeface has the images of its glyphs, which is empty for a typeface
 *      that has none.
 */
TR_PUBLIC const TRBitmapStrike *TRTypefaceGetBitmapStrikesPtr(TRTypefaceRef typeface);

/**
 * Returns the number of bitmap strikes of the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The number of bitmap strikes of the typeface.
 */
TR_PUBLIC TRUInteger TRTypefaceGetBitmapStrikeCount(TRTypefaceRef typeface);

/**
 * Returns the variation coordinates that the typeface uses, in the order of the variation axes.
 * They are the default values of the axes unless the typeface was made with
 * `TRTypefaceCreateWithVariation()`. They are empty if the typeface has no variation axes.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The variation coordinates that the typeface uses, in the order of the variation axes.
 */
TR_PUBLIC const TRFloat *TRTypefaceGetVariationCoordinatesPtr(TRTypefaceRef typeface);

/**
 * Returns the colors that the typeface uses for its palette entries, in order. They are the colors
 * of the first predefined palette, or opaque black if the font has none, unless the typeface was
 * made with `TRTypefaceCreateWithColors()`. They are empty if the typeface has no palette entries.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The colors that the typeface uses for its palette entries, in order.
 */
TR_PUBLIC const TRColor *TRTypefaceGetAssociatedColorsPtr(TRTypefaceRef typeface);

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
 *
 * The name is owned by the typeface.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The family name, or `NULL` if the font has none in a supported encoding.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetFamilyName(TRTypefaceRef typeface);

/**
 * Returns the subfamily (style) name; it follows the variation coordinates when they match a named
 * style. `NULL` if there is none.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The subfamily (style) name; it follows the variation coordinates when they match a named
 *      style.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetSubfamilyName(TRTypefaceRef typeface);

/**
 * Returns the full name. For a variable font that has named styles, it is made from the family name
 * and the name of the style that the variation coordinates match, and it is `NULL` if they match
 * none. For any other font, it is the full name of the font, or the family name followed by the
 * style name if the font has none. It is `NULL` if the font has none of these in a supported
 * encoding.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The full name.
 */
TR_PUBLIC const TRStringView *TRTypefaceGetFullName(TRTypefaceRef typeface);

/**
 * Returns the bounding box that contains all glyphs, in font units with the y axis pointing up.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The bounding box that contains all glyphs, in font units with the y axis pointing up.
 */
TR_PUBLIC TRRect TRTypefaceGetBoundingBox(TRTypefaceRef typeface);

/**
 * Returns the position of the underline relative to the baseline, in font units.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The position of the underline relative to the baseline, in font units.
 */
TR_PUBLIC TRInt32 TRTypefaceGetUnderlinePosition(TRTypefaceRef typeface);

/**
 * Returns the thickness of the underline, in font units.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The thickness of the underline, in font units.
 */
TR_PUBLIC TRUInt32 TRTypefaceGetUnderlineThickness(TRTypefaceRef typeface);

/**
 * Returns the position of the strikeout relative to the baseline, in font units. It is zero if the
 * font has no `OS/2` table.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The position of the strikeout relative to the baseline, in font units.
 */
TR_PUBLIC TRInt32 TRTypefaceGetStrikeoutPosition(TRTypefaceRef typeface);

/**
 * Returns the thickness of the strikeout, in font units. It is zero if the font has no `OS/2`
 * table.
 *
 * @param typeface
 *      The typeface.
 * @return
 *      The thickness of the strikeout, in font units.
 */
TR_PUBLIC TRInt32 TRTypefaceGetStrikeoutThickness(TRTypefaceRef typeface);

/**
 * Returns the glyph that represents a code point.
 *
 * @param typeface
 *      The typeface.
 * @param codePoint
 *      The Unicode code point.
 * @return
 *      The glyph ID, or 0 (the missing glyph) if the typeface has no glyph for it.
 */
TR_PUBLIC TRGlyphID TRTypefaceGetGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint);

/**
 * Returns the glyph that represents a code point followed by a variation selector.
 *
 * @param typeface
 *      The typeface.
 * @param codePoint
 *      The Unicode code point.
 * @param variantSelector
 *      The Unicode code point of the variation selector.
 * @return
 *      The glyph ID, or 0 if the typeface has no such variant.
 */
TR_PUBLIC TRGlyphID TRTypefaceGetVariantGlyphID(TRTypefaceRef typeface, TRUInt32 codePoint,
    TRUInt32 variantSelector);

/**
 * Returns the unhinted advance of a glyph at the given size, following the variation coordinates of
 * the typeface.
 *
 * @param typeface
 *      The typeface.
 * @param glyphID
 *      The ID of the glyph.
 * @param typeSize
 *      Size of the em square, in the unit the caller wants the advance in.
 * @param isVertical
 *      `TRTrue` for the vertical advance.
 * @return
 *      The advance in the unit of `typeSize`, or zero if the typeface has no units per em.
 */
TR_PUBLIC TRFloat TRTypefaceGetGlyphAdvance(TRTypefaceRef typeface, TRGlyphID glyphID,
    TRFloat typeSize, TRBoolean isVertical);

/**
 * Creates the outline of a glyph. The origin is at the glyph's pen position on the baseline, and
 * the y axis points downward.
 *
 * @param typeface
 *      The typeface.
 * @param glyphID
 *      The ID of the glyph.
 * @param typeSize
 *      Size of the em square, which MUST be positive.
 * @return
 *      New path that the caller owns, or `NULL` if the size is not positive, the glyph has no
 *      outline, or on failure.
 */
TR_PUBLIC TRPathRef TRTypefaceCreateGlyphPath(TRTypefaceRef typeface, TRGlyphID glyphID,
    TRFloat typeSize);

/**
 * Returns the size of a table of the font in bytes, without reading it.
 *
 * @param typeface
 *      The typeface.
 * @param tag
 *      The tag of the table.
 * @return
 *      The size of the table, which is zero if the font has no such table.
 */
TR_PUBLIC TRUInteger TRTypefaceGetTableSize(TRTypefaceRef typeface, TRTag tag);

/**
 * Copies a part of the data of a table of the font. Only the part that is asked for is read from
 * the font, so a part of a big table is cheap.
 *
 * @param typeface
 *      The typeface.
 * @param tag
 *      The tag of the table.
 * @param offset
 *      The offset in the table from where to copy, in bytes.
 * @param buffer
 *      Receives the data of the table.
 * @param capacity
 *      The number of bytes that `buffer` can hold. A part that is bigger is cut short.
 * @return
 *      The number of bytes copied, which is zero if the font has no such table, the offset is not
 *      within it, or the buffer is `NULL`.
 */
TR_PUBLIC TRUInteger TRTypefaceGetTableData(TRTypefaceRef typeface, TRTag tag, TRUInteger offset,
    void *buffer, TRUInteger capacity);

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

/**
 * Increments the reference count of a typeface.
 *
 * @param typeface
 *      The typeface whose reference count will be incremented.
 * @return
 *      The same typeface passed in as the parameter.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceRetain(TRTypefaceRef typeface);

/**
 * Decrements the reference count of a typeface. The typeface will be deallocated when its
 * reference count reaches zero.
 *
 * @param typeface
 *      The typeface whose reference count will be decremented.
 */
TR_PUBLIC void TRTypefaceRelease(TRTypefaceRef typeface);

TR_EXTERN_C_END

#endif
