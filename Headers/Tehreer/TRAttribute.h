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

#ifndef _TEHREER_ATTRIBUTE_H
#define _TEHREER_ATTRIBUTE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRFontFeatures.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/**
 * The type of an attribute. The attributes of the first group apply to the exact range they are
 * set on, while the paragraph attributes of the second group apply to every paragraph that the
 * range touches.
 */
enum {
    /* Run attributes */
    TRAttributeTypeface = 1,                /**< The typeface of the text. */
    TRAttributeTypeSize = 2,               /**< The size of the em square. */
    TRAttributeScaleX = 3,                  /**< Horizontal scale of the glyphs; 1.0 by default. */
    TRAttributeScaleY = 4,                  /**< Vertical scale of the glyphs; 1.0 by default. */
    TRAttributeBaselineOffset = 5,          /**< Distance to raise the text above its baseline. */
    TRAttributeObliqueness = 6,             /**< Skew of the glyphs. It keeps runs apart. */
    TRAttributeReplacement = 7,             /**< Content that replaces the text of the range. */
    TRAttributeForegroundColor = 19,        /**< The color that the text is painted with. */
    TRAttributeUserData = 20,               /**< Opaque data of the owner; see below. */
    TRAttributeLanguage = 21,               /**< OpenType language tag of the text; see below. */
    TRAttributeFontFeatures = 22,           /**< OpenType feature settings; see below. */
    TRAttributeBackgroundColor = 23,        /**< The color that is filled behind the text. */
    TRAttributeUnderline = 24,              /**< Whether a line is drawn below the text. */
    TRAttributeStrikethrough = 25,          /**< Whether a line is drawn through the text. */
    TRAttributeDecorationColor = 26,        /**< The color of the underline and strikethrough. */

    /* Paragraph attributes */
    TRAttributeTextAlignment = 8,           /**< The alignment of the lines. */
    TRAttributeFirstLineHeadIndent = 9,     /**< Indent of the lines before the head indent. */
    TRAttributeHeadIndent = 10,             /**< Indent of the other lines at the leading edge. */
    TRAttributeTailIndent = 11,             /**< Indent at the trailing edge; see below. */
    TRAttributeFirstIndentLineCount = 12,   /**< Number of lines that use the first line indent. */
    TRAttributeParagraphSpacingBefore = 13, /**< Space before the paragraph. */
    TRAttributeParagraphSpacing = 14,       /**< Space after the paragraph. */
    TRAttributeLineHeightMultiple = 15,     /**< Factor that scales the height of the lines. */
    TRAttributeMinimumLineHeight = 16,      /**< Least height of a line. */
    TRAttributeMaximumLineHeight = 17,      /**< Greatest height of a line. */
    TRAttributeLineSpacing = 18             /**< Extra space added below each line. */
};
typedef TRUInt32 TRAttributeType;

/**
 * The alignment of the lines of a paragraph. Leading and trailing follow the base direction of the
 * paragraph, while left and right are absolute.
 */
enum {
    TRTextAlignmentLeft = 0,
    TRTextAlignmentCenter = 1,
    TRTextAlignmentRight = 2,
    TRTextAlignmentLeading = 3,
    TRTextAlignmentTrailing = 4
};
typedef TRUInt32 TRTextAlignment;

/**
 * The value of an attribute. The member to use depends on the type of the attribute.
 *
 * The foreground color, the background color, the underline, the strikethrough, the decoration
 * color and the user data do not change how text is laid out, but lines never have a glyph run
 * that spans a change of them, so a wrapper can paint each run with one set of attributes. The
 * decoration color is that of the underline and the strikethrough, and it is the foreground color
 * of the run if it is not set. The user data is a pointer that Core only compares: a wrapper uses
 * it to tag ranges of text with its own attributes, such as links, and has to keep what it points
 * to alive as long as the text is used.
 *
 * The language and the font features change how the text is shaped, so a glyph run never spans a
 * change of them. The language is the OpenType tag of a language, which makes the font pick the
 * forms of it, as for the Han characters of Japanese or Chinese, or the dotted and dotless i of
 * Turkish; `TRShapingEngineGetLanguageTag()` makes it from a name in the form of BCP 47, and it is
 * the default language if it is not set. The font features are the settings that are applied on
 * top of the defaults of the font, e.g. `smcp` for small capitals or `tnum` for tabular figures.
 *
 * The indents of a paragraph are measured from the leading edge, except the tail indent: if it is
 * positive, it is the distance from the leading edge to the trailing margin, and if it is zero or
 * negative, its absolute value is the distance from the trailing edge. The first indent line count
 * is the number of lines that use the first line head indent; it is 1 by default.
 */
typedef union _TRAttributeValue {
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat scaleX;
    TRFloat scaleY;
    TRFloat baselineOffset;
    TRFloat obliqueness;
    TRReplacementRef replacement;
    TRColor foregroundColor;
    TRColor backgroundColor;
    TRBoolean underline;
    TRBoolean strikethrough;
    TRColor decorationColor;
    const void *userData;
    TRTag language;
    TRFontFeaturesRef fontFeatures;
    TRTextAlignment textAlignment;
    TRFloat firstLineHeadIndent;
    TRFloat headIndent;
    TRFloat tailIndent;
    TRUInteger firstIndentLineCount;
    TRFloat paragraphSpacingBefore;
    TRFloat paragraphSpacing;
    TRFloat lineHeightMultiple;
    TRFloat minimumLineHeight;
    TRFloat maximumLineHeight;
    TRFloat lineSpacing;
} TRAttributeValue;

typedef struct _TRAttribute {
    TRAttributeType type;
    TRAttributeValue value;
} TRAttribute;

TR_EXTERN_C_END

#endif
