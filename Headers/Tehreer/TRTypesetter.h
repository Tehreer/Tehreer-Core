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
#include <Tehreer/TRAttributeList.h>

TR_EXTERN_C_BEGIN

/**
 * How lines are broken when a text does not fit.
 */
enum {
    TRBreakModeLine = 0,        /**< Lines break at the places that Unicode allows. */
    TRBreakModeCharacter = 1    /**< Lines break between any two characters (grapheme clusters). */
};
/**
 * A type to represent how lines are broken, which is one of the `TRBreakMode*` constants above. A
 * value that is not one of them is handled as `TRBreakModeLine` by the functions that take it.
 */
typedef TRUInt32 TRBreakMode;

/**
 * Where the part of a line that does not fit is cut out and replaced with a token.
 */
enum {
    TRTruncationPlaceStart = 0,     /**< The text at the start of the line is cut out. */
    TRTruncationPlaceMiddle = 1,    /**< The text in the middle of the line is cut out. */
    TRTruncationPlaceEnd = 2        /**< The text at the end of the line is cut out. */
};
/**
 * A type to represent where text is cut out of a line, which is one of the `TRTruncationPlace*`
 * constants above. A value that is not one of them is handled as `TRTruncationPlaceEnd` by
 * `TRTypesetterCreateTruncationToken()`, while `TRTypesetterCreateTruncatedLine()` does not create
 * a line for it.
 */
typedef TRUInt32 TRTruncationPlace;

/**
 * Constants that specify the kinds of boundaries that a text can be searched for.
 */
enum {
    TRBoundaryTypeGrapheme = 0,     /**< A boundary between two grapheme clusters. */
    TRBoundaryTypeWord = 1,         /**< A boundary between two word segments, which are words,
                                         spaces or punctuations. */
    TRBoundaryTypeParagraph = 2     /**< A boundary between two paragraphs, which end with their
                                         separator. */
};
/**
 * A type to represent the kind of a boundary.
 */
typedef TRUInt8 TRBoundaryType;

/**
 * Constants that specify the direction of searching for a boundary.
 */
enum {
    TRBoundaryDirectionBackward = 0,    /**< Search for the last boundary before an index. */
    TRBoundaryDirectionForward = 1      /**< Search for the first boundary after an index. */
};
/**
 * A type to represent the direction of searching for a boundary.
 */
typedef TRUInt8 TRBoundaryDirection;

/**
 * A typesetter holds a text with attributes on its ranges, shapes it, and creates lines from it. It
 * shapes each paragraph of the text when the paragraph is made or changed, so lines can be made
 * over and over again for different ranges, extents and settings without shaping it again.
 *
 * The text is a string of code units in one encoding, which the typesetter keeps as a SheenBidi
 * text; see `Tehreer/TRSheenBidi.h`. All ranges and indexes count its code units.
 *
 * A typesetter that is created as immutable never changes, so it can be used from multiple
 * threads. A mutable one is used by a single thread at a time, and the lines or the boundaries it
 * is asked for are available only outside of an editing batch; see `TRTypesetterBeginEditing()`.
 *
 * A paragraph that has some text without a typeface cannot be shaped, so the functions that
 * suggest a break or create a line fail over a range that touches it, as they do for a range that
 * is not valid.
 */
typedef const struct _TRTypesetter *TRTypesetterRef;

/**
 * Opaque reference to a mutable typesetter, which can be edited through the editing functions
 * below. It is a typesetter too, so it can be passed to the functions of the typesetter, and
 * retained and released with `TRTypesetterRetain()` and `TRTypesetterRelease()`.
 */
typedef struct _TRTypesetter *TRMutableTypesetterRef;

/**
 * Creates an immutable typesetter from raw code units.
 *
 * The typeface, size and other attributes of the text decide how each part of it is shaped. The
 * default attributes fill in what the text does not set; a typeface MUST be given by one of them
 * or by the attributes for every part of the text that is not a replacement, or lines cannot be
 * made over that part.
 *
 * @param string
 *      Pointer to code units in the specified encoding.
 * @param length
 *      Number of code units in string.
 * @param encoding
 *      String encoding (UTF-8, UTF-16, or UTF-32).
 * @param defaultAttributes
 *      Attributes to use where the text has none, or `NULL`. Only the ones that decide how the
 *      text is shaped are looked at: the typeface, type size, scales, baseline offset,
 *      obliqueness, replacement, language and font features. A negative type size is taken as
 *      zero. They are copied, and the typesetter retains what they hold.
 * @param defaultAttributeCount
 *      Number of default attributes.
 * @return
 *      A reference to a typesetter that the caller owns, or `NULL` if the string is `NULL` for a
 *      length that is not zero, the encoding is not valid, or on failure.
 */
TR_PUBLIC TRTypesetterRef TRTypesetterCreate(const void *string, TRUInteger length,
    TRStringEncoding encoding, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount);

/**
 * Creates an empty mutable typesetter, which can be edited through the editing functions below. The
 * encoding of the code units is fixed for its lifetime.
 *
 * @param encoding
 *      Target encoding for the new typesetter (UTF-8/16/32).
 * @param defaultAttributes
 *      Attributes to use where the text has none, as for `TRTypesetterCreate()`, or `NULL`.
 * @param defaultAttributeCount
 *      Number of default attributes.
 * @return
 *      A reference to a mutable typesetter that the caller owns, or `NULL` if the encoding is not
 *      valid or on failure.
 */
TR_PUBLIC TRMutableTypesetterRef TRTypesetterCreateMutable(TRStringEncoding encoding,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount);

/**
 * Creates an immutable typesetter that is an exact copy of the source, with its attributes and
 * default attributes. The copy shares the shaping of the paragraphs, so it costs little, and
 * the source can be changed afterwards without changing the copy.
 *
 * @param typesetter
 *      Source typesetter to copy (can be immutable or mutable).
 * @return
 *      New immutable copy that the caller owns, or `NULL` if the source is in an editing batch
 *      or on failure.
 */
TR_PUBLIC TRTypesetterRef TRTypesetterCreateCopy(TRTypesetterRef typesetter);

/**
 * Creates a mutable typesetter that is an exact copy of the source. The copy can be modified
 * while the original remains unchanged.
 *
 * @param typesetter
 *      Source typesetter to copy (can be immutable or mutable).
 * @return
 *      New mutable typesetter that the caller owns, with the same content, attributes and default
 *      attributes as the source, or `NULL` if the source is in an editing batch or on failure.
 */
TR_PUBLIC TRMutableTypesetterRef TRTypesetterCreateMutableCopy(TRTypesetterRef typesetter);

/**
 * Returns the encoding specified at typesetter creation.
 *
 * This encoding remains constant for the lifetime of the typesetter.
 *
 * @param typesetter
 *      The typesetter.
 * @return
 *      String encoding used by this typesetter.
 */
TR_PUBLIC TRStringEncoding TRTypesetterGetEncoding(TRTypesetterRef typesetter);

/**
 * Returns the number of code units in the typesetter.
 *
 * @param typesetter
 *      The typesetter.
 * @return
 *      Number of code units in the text.
 */
TR_PUBLIC TRUInteger TRTypesetterGetLength(TRTypesetterRef typesetter);

/**
 * Copies code units from the text into a caller-provided buffer.
 *
 * The buffer must be large enough to hold the requested number of code units in the text's encoding
 * format.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      Start index (in code units).
 * @param length
 *      Number of code units to copy.
 * @param buffer
 *      Output buffer (must be large enough for `length` code units).
 * @return
 *      `TRTrue` if the code units were copied, `TRFalse` if the range is not within the text or the
 *      buffer is `NULL`.
 */
TR_PUBLIC TRBoolean TRTypesetterGetCodeUnits(TRTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, void *buffer);

/**
 * Copies the attributes active at `index`, and the length of the run over which they apply (the
 * extent, starting at `index`, over which the attribute set stays the same). The list has an
 * attribute for each type that is set at the index.
 *
 * For more complex queries (filtering by a specific attribute, or by group/scope), use
 * `TRTypesetterGetSheenBidiText()` of `Tehreer/TRSheenBidi.h` to access the underlying `SBTextRef`.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      Code-unit index to query.
 * @param outLength
 *      Receives the run length in code units, which is zero if there is no list. It may be `NULL`
 *      if it is not needed.
 * @return
 *      A new attribute list that the caller owns, and has to release via
 *      `TRAttributeListRelease()`. It is `NULL` if the index is not less than the length of the
 *      text, or on failure.
 */
TR_PUBLIC TRAttributeListRef TRTypesetterCopyAttributes(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger *outLength);

/* ----------------------------------
 * Editing
 * ---------------------------------- */

/**
 * Signals the start of a batch of editing operations. While in editing mode, the analysis and
 * shaping of the paragraphs are deferred until `TRTypesetterEndEditing()` is called, which improves
 * performance when making multiple sequential modifications. No line can be made, no break can be
 * suggested and no copy can be created until then. The attributes can be set and removed in the
 * batch, and the paragraphs that they change are shaped once at the end of it, so a text with many
 * attributes is built in a batch, not by one change after another, each of which shapes the
 * paragraph again.
 *
 * @param typesetter
 *      Mutable typesetter.
 */
TR_PUBLIC void TRTypesetterBeginEditing(TRMutableTypesetterRef typesetter);

/**
 * Signals the end of a batch editing session, and analyzes and shapes all modified paragraphs.
 *
 * @param typesetter
 *      Mutable typesetter.
 */
TR_PUBLIC void TRTypesetterEndEditing(TRMutableTypesetterRef typesetter);

/**
 * Adds new code units to the end of the typesetter. If it is not in editing mode, the paragraphs
 * that the new content touches are analyzed and shaped immediately.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param codeUnitBuffer
 *      Pointer to code units in the text's encoding.
 * @param codeUnitCount
 *      Number of code units to append.
 * @return
 *      `TRTrue` if the code units were appended, `TRFalse` if the buffer is `NULL`.
 *
 * @warning
 *      The buffer must contain valid code units in the text's encoding format.
 */
TR_PUBLIC TRBoolean TRTypesetterAppendCodeUnits(TRMutableTypesetterRef typesetter,
    const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Inserts new code units at the specified position in the text. Existing content at and after the
 * insertion point is shifted right. If the typesetter is not in editing mode, the paragraphs that
 * the insertion touches are analyzed and shaped immediately.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param index
 *      Insertion index (in code units), which can be the length of the text to insert at its end.
 * @param codeUnitBuffer
 *      Pointer to code units in the text's encoding.
 * @param codeUnitCount
 *      Number of code units to insert.
 * @return
 *      `TRTrue` if the code units were inserted, `TRFalse` if the index is past the end of the text
 *      or the buffer is `NULL`.
 *
 * @warning
 *      The buffer must contain valid code units in the text's encoding format.
 */
TR_PUBLIC TRBoolean TRTypesetterInsertCodeUnits(TRMutableTypesetterRef typesetter, TRUInteger index,
    const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Removes a contiguous range of code units from the text. Content after the deletion range is
 * shifted left. If the typesetter is not in editing mode, the paragraphs that the deletion touches
 * are analyzed and shaped immediately.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param index
 *      Start index of the range to delete (in code units).
 * @param length
 *      Number of code units to delete.
 * @return
 *      `TRTrue` if the code units were deleted, `TRFalse` if the range is not within the text.
 */
TR_PUBLIC TRBoolean TRTypesetterDeleteCodeUnits(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length);

/**
 * Completely replaces the current text content with the new code units. This is equivalent to
 * replacing the whole range of the text with `TRTypesetterReplaceCodeUnits()`, so the new content
 * takes on the attributes of the content that it replaces.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param codeUnitBuffer
 *      Pointer to code units in the text's encoding.
 * @param codeUnitCount
 *      Number of code units in codeUnitBuffer.
 * @return
 *      `TRTrue` if the content was replaced, `TRFalse` if the buffer is `NULL` for a count that is
 *      not zero.
 *
 * @warning
 *      All existing content is removed, but its attributes are kept for the new content.
 *      The buffer must contain valid code units in the text's encoding format.
 */
TR_PUBLIC TRBoolean TRTypesetterSetCodeUnits(TRMutableTypesetterRef typesetter,
    const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Replaces a contiguous range of existing code units with new content, which takes on the
 * attributes of the range that it replaces. If the typesetter is not in editing mode, the
 * paragraphs that the replacement touches are analyzed and shaped immediately.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param index
 *      Start index of the range to replace (in code units).
 * @param length
 *      Length of the range to replace (in code units).
 * @param codeUnitBuffer
 *      Pointer to replacement code units in the text's encoding. It may be `NULL` if the count is
 *      zero, which only deletes the range.
 * @param codeUnitCount
 *      Number of replacement code units.
 * @return
 *      `TRTrue` if the range was replaced, `TRFalse` if the range is not within the text or the
 *      buffer is `NULL` for a count that is not zero.
 *
 * @warning
 *      The buffer must contain valid code units in the text's encoding format.
 */
TR_PUBLIC TRBoolean TRTypesetterReplaceCodeUnits(TRMutableTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Applies the specified attribute with the given value to the range of code units. If the attribute
 * already exists in parts of the range, those values are replaced. The attribute is copied. The
 * text retains the typeface, replacement or font features that the attribute holds; the user data
 * is a pointer that it only compares.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param index
 *      Start index of the range (in code units).
 * @param length
 *      Length of the range (in code units).
 * @param attribute
 *      Pointer to the attribute to set.
 * @return
 *      `TRTrue` if the attribute was set, `TRFalse` if the range is not within the text, the
 *      attribute is `NULL` or its type is unknown.
 */
TR_PUBLIC TRBoolean TRTypesetterSetAttribute(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, const TRAttribute *attribute);

/**
 * Removes the specified attribute from the given range of code units. If the attribute is not
 * present in the range, this operation has no effect.
 *
 * @param typesetter
 *      Mutable typesetter.
 * @param index
 *      Start index of the range (in code units).
 * @param length
 *      Length of the range (in code units).
 * @param attributeType
 *      Type of the attribute to remove.
 * @return
 *      `TRTrue` if the attribute was removed, `TRFalse` if the range is not within the text, the
 *      type is unknown.
 */
TR_PUBLIC TRBoolean TRTypesetterRemoveAttribute(TRMutableTypesetterRef typesetter, TRUInteger index,
    TRUInteger length, TRAttributeType attributeType);

/**
 * Suggests where to break a line forward from the start of a range. The measurement goes from the
 * first code unit to the last. If the whole range fits in the extent, its end is returned.
 * Otherwise the index of the first code unit that does not fit is returned, at least one character
 * after the start. A line break may not cross a paragraph, so the suggestion stops at the end of
 * the paragraph that the range starts in. A NaN extent is never exceeded, so the whole range fits
 * in it.
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
 *      The way to break the line. A value that is not valid is handled as `TRBreakModeLine`.
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
 * character before the end. A line break may not cross a paragraph. A NaN extent is never
 * exceeded, so the whole range fits in it.
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
 *      The way to break the line. A value that is not valid is handled as `TRBreakModeLine`.
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
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to
 *      be within the text.
 * @param layoutWidth
 *      The width of the frame that the line is made for, or zero if there is none. The
 *      replacements that decide their room by the width of the frame, such as a view that fills
 *      it, get it from here.
 * @return
 *      New line, or `NULL` if the range is empty or is not within the text, or on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateSimpleLine(TRTypesetterRef typesetter,
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
 *      The number of code units of the line that is truncated, which MUST NOT be zero. The range
 *      has to be within the text.
 * @param truncationPlace
 *      The place where text is cut out. A value that is not valid is handled as
 *      `TRTruncationPlaceEnd`.
 * @param tokenString
 *      The code units of the token, or `NULL` to use an ellipsis. If the typeface has no ellipsis
 *      character, three dots are used.
 * @param tokenLength
 *      Number of code units of the token.
 * @param tokenEncoding
 *      The encoding of the token. It is ignored if the token string is `NULL` or empty.
 * @return
 *      New line, or `NULL` if the token encoding is not valid, or on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateTruncationToken(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRTruncationPlace truncationPlace,
    const void *tokenString, TRUInteger tokenLength, TRStringEncoding tokenEncoding);

/**
 * Creates a line of a range, cutting out the part that does not fit in the extent and showing a
 * token instead. A line that fits is made as `TRTypesetterCreateSimpleLine()` makes it. A NaN
 * extent is never exceeded, so the line is made that way too.
 *
 * @param typesetter
 *      The typesetter.
 * @param index
 *      The index of the first code unit of the line.
 * @param length
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to
 *      be within the text.
 * @param extent
 *      The extent at which the truncation begins.
 * @param breakMode
 *      The way to find the text that is cut out. A value that is not valid is handled as
 *      `TRBreakModeLine`.
 * @param truncationPlace
 *      The place where text is cut out. It MUST be one of the `TRTruncationPlace*` constants.
 * @param tokenLine
 *      The line of the token, such as one from `TRTypesetterCreateTruncationToken()`. It MUST NOT
 *      be `NULL`.
 * @return
 *      New line, or `NULL` if the token line is `NULL`, the range is empty or is not within the
 *      text, the truncation place is not valid, or on failure. Its range is the whole range,
 *      including the text that was cut out.
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
 *      The number of code units of the line, which MUST NOT be zero. The range of the line has to
 *      be within the text.
 * @param justificationFactor
 *      How much of the extra space is used: 1 or more stretches the line fully, anything less does
 *      it partially, and 0 or less leaves it as it is.
 * @param justificationExtent
 *      The extent to stretch the line to. If it is less than the width of the line, the spaces are
 *      squeezed. If it or the factor is NaN, the line is created, but its width is NaN.
 * @return
 *      New line, or `NULL` on failure.
 */
TR_PUBLIC TRComposedLineRef TRTypesetterCreateJustifiedLine(TRTypesetterRef typesetter,
    TRUInteger index, TRUInteger length, TRFloat justificationFactor, TRFloat justificationExtent);

/**
 * Finds the boundary of a given type that is next to a code unit index, according to Unicode
 * Standard Annex #29 for graphemes and words, and the paragraph separators of the text for
 * paragraphs. The start and the end of the text are boundaries of every type.
 *
 * The search is done on the current content of the text, and needs no typeface: the data it uses
 * is made with each paragraph, so a typesetter that is not being edited can be searched from
 * multiple threads.
 *
 * @param typesetter
 *      The typesetter to search in.
 * @param index
 *      Code-unit index to search from. It may be the length of the text.
 * @param type
 *      Type of the boundary.
 * @param direction
 *      Direction of the search. A forward search finds the first boundary after the index, and a
 *      backward search finds the last boundary before it.
 * @return
 *      Index of the boundary, or `TRInvalidIndex` if the index is greater than the length, there is
 *      nothing after it for a forward search or before it for a backward search, the type or the
 *      direction is not known, or the typesetter is being edited.
 */
TR_PUBLIC TRUInteger TRTypesetterFindBoundary(TRTypesetterRef typesetter, TRUInteger index,
    TRBoundaryType type, TRBoundaryDirection direction);

/**
 * Gets the range of the segment of a given boundary type that contains a code unit, for example
 * the word under a caret.
 *
 * @param typesetter
 *      The typesetter to search in.
 * @param index
 *      Code-unit index that the segment contains. It may be the length of the text, which has an
 *      empty segment.
 * @param type
 *      Type of the boundary.
 * @return
 *      The range of the segment, or a range that starts at `TRInvalidIndex` and is empty if the
 *      index is greater than the length, the type is not known, or the typesetter is being edited.
 */
TR_PUBLIC TRRange TRTypesetterGetBoundaryRange(TRTypesetterRef typesetter, TRUInteger index,
    TRBoundaryType type);

/**
 * Increments the reference count of a typesetter.
 *
 * @param typesetter
 *      The typesetter whose reference count will be incremented.
 * @return
 *      The same typesetter passed in as the parameter.
 */
TR_PUBLIC TRTypesetterRef TRTypesetterRetain(TRTypesetterRef typesetter);

/**
 * Decrements the reference count of a typesetter. The typesetter will be deallocated when its
 * reference count reaches zero.
 *
 * @param typesetter
 *      The typesetter whose reference count will be decremented.
 */
TR_PUBLIC void TRTypesetterRelease(TRTypesetterRef typesetter);

TR_EXTERN_C_END

#endif
