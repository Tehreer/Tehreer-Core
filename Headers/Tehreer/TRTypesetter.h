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

#ifndef _TEHREER_TYPESETTER_H
#define _TEHREER_TYPESETTER_H

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRText.h>

TR_EXTERN_C_BEGIN

/**
 * How lines are broken when a text does not fit.
 */
enum {
    TRBreakModeLine = 0,        /**< Lines break at the places that Unicode allows. */
    TRBreakModeCharacter = 1    /**< Lines break between any two characters (grapheme clusters). */
};
typedef TRUInt32 TRBreakMode;

/**
 * Where the part of a line that does not fit is cut out and replaced with a token.
 */
enum {
    TRTruncationPlaceStart = 0,     /**< The text at the start of the line is cut out. */
    TRTruncationPlaceMiddle = 1,    /**< The text in the middle of the line is cut out. */
    TRTruncationPlaceEnd = 2        /**< The text at the end of the line is cut out. */
};
typedef TRUInt32 TRTruncationPlace;

/**
 * A typesetter shapes the text that it is created with, and creates lines from it. It holds the
 * glyphs of the whole text, so lines can be made over and over again for different ranges, extents
 * and settings without shaping it again.
 *
 * A typesetter never changes once it is created, so it can be used from multiple threads.
 *
 * All ranges and indexes count the code units of the text.
 */
typedef const struct _TRTypesetter *TRTypesetterRef;

/**
 * Creates a typesetter for a text.
 *
 * The typeface, size and other attributes of the text decide how each part of it is shaped. The
 * default attributes fill in what the text does not set; a typeface MUST be given by one of them
 * for every part of the text that is not a replacement. The text is copied, so it can be changed or
 * released afterwards.
 *
 * @param text
 *      The text to typeset.
 * @param defaultAttributes
 *      Attributes to use where the text has none, or `NULL`. Only the typeface, point size, scales,
 *      baseline offset and obliqueness are looked at.
 * @param defaultAttributeCount
 *      Number of default attributes.
 * @return
 *      New typesetter, or `NULL` if some text has no typeface or on failure.
 */
TR_PUBLIC TRTypesetterRef TRTypesetterCreate(TRTextRef text,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount);

/**
 * Returns the number of code units in the text.
 */
TR_PUBLIC TRUInteger TRTypesetterGetCodeUnitCount(TRTypesetterRef typesetter);

/**
 * Suggests where to break a line forward from the start of a range. The measurement goes from the
 * first code unit to the last. If the whole range fits in the extent, its end is returned.
 * Otherwise the index of the first code unit that does not fit is returned, at least one character
 * after the start. A line break may not cross a paragraph, so the suggestion stops at the end of
 * the paragraph that the range starts in.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the range to break.
 * @param length
 *      The number of code units of the range to break, which MUST NOT be zero.
 * @param extent
 *      The extent that the line has to fit in.
 * @param breakMode
 *      The way to break the line.
 * @return
 *      The index (exclusive) that ends the line, or `TRInvalidIndex` if the range is empty or is
 *      not within the text.
 */
TR_PUBLIC TRUInteger TRTypesetterSuggestForwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode);

/**
 * Suggests where to break a line backward from the end of a range. The measurement goes from the
 * last code unit to the first. If the whole range fits in the extent, its start is returned.
 * Otherwise the index of the first code unit of the line that fits is returned, at least one
 * character before the end. A line break may not cross a paragraph.
 *
 * The parameters are those of `TRTypesetterSuggestForwardBreak()`.
 *
 * @return
 *      The index (inclusive) that starts the line, or `TRInvalidIndex` if the range is empty or is
 *      not within the text.
 */
TR_PUBLIC TRUInteger TRTypesetterSuggestBackwardBreak(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRFloat extent, TRBreakMode breakMode);

/**
 * Creates a line with all of the text of a range, in the order that it is shown. The line has no
 * limit to its extent, so the caller picks the range, usually with the help of
 * `TRTypesetterSuggestForwardBreak()`.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line.
 * @param length
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to be
 *      within the text.
 * @return
 *      New line, or `NULL` if the range is empty or is not within the text, or on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateSimpleLine(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length);

/**
 * Creates a simple line for a frame that is some width wide. The replacements that decide their
 * room by the width of the frame, such as a view that fills it, get it from `layoutWidth`, which
 * `TRTypesetterCreateSimpleLine()` leaves at zero.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line.
 * @param length
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to be
 *      within the text.
 * @param layoutWidth
 *      The width of the frame.
 * @return
 *      New line, or `NULL` on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateFrameLine(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRFloat layoutWidth);

/**
 * Creates a line that is made of a token, such as an ellipsis, to show where text was cut out of
 * a line. It is shaped like the text at the place of the truncation.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line that is truncated.
 * @param length
 *      The number of code units of the line that is truncated, which MUST NOT be zero. The range has
 *      to be within the text.
 * @param truncationPlace
 *      The place where text is cut out.
 * @param tokenString
 *      The code units of the token, or `NULL` to use an ellipsis. If the typeface has no ellipsis
 *      character, three dots are used.
 * @param tokenLength
 *      Number of code units of the token.
 * @param tokenEncoding
 *      The encoding of the token.
 * @return
 *      New line, or `NULL` on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateTruncationToken(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRTruncationPlace truncationPlace,
    const void *tokenString, TRUInteger tokenLength, TRStringEncoding tokenEncoding);

/**
 * Creates a line of a range, cutting out the part that does not fit in the extent and showing a
 * token instead. A line that fits is made as `TRTypesetterCreateSimpleLine()` makes it.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line.
 * @param length
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to be
 *      within the text.
 * @param extent
 *      The extent at which the truncation begins.
 * @param breakMode
 *      The way to find the text that is cut out.
 * @param truncationPlace
 *      The place where text is cut out.
 * @param tokenLine
 *      The line of the token, such as one from `TRTypesetterCreateTruncationToken()`.
 * @return
 *      New line, or `NULL` on failure. Its range does not include the text that was cut out at the
 *      start or at the end.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateTruncatedLine(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, TRComposedLineRef tokenLine);

/**
 * Creates a line of a range that is stretched to an extent by adding space to its inner spaces.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line.
 * @param length
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to be
 *      within the text.
 * @param justificationFactor
 *      How much of the extra space is used: 1 or more stretches the line fully, anything less does
 *      it partially, and 0 or less leaves it as it is.
 * @param justificationExtent
 *      The extent to stretch the line to. If it is less than the width of the line, the spaces are
 *      squeezed.
 * @return
 *      New line, or `NULL` on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateJustifiedLine(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRFloat justificationFactor, TRFloat justificationExtent);

/**
 * Retains the typesetter.
 */
TR_PUBLIC TRTypesetterRef TRTypesetterRetain(TRTypesetterRef typesetter);

/**
 * Releases the typesetter.
 */
TR_PUBLIC void TRTypesetterRelease(TRTypesetterRef typesetter);

TR_EXTERN_C_END

#endif
