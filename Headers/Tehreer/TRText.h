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

#ifndef _TEHREER_TEXT_H
#define _TEHREER_TEXT_H

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>

TR_EXTERN_C_BEGIN

/**
 * Opaque reference to an immutable text object.
 */
typedef const struct _TRText *TRTextRef;

/**
 *  Opaque reference to a mutable text object.
 */
typedef struct _TRText *TRMutableTextRef;

/**
 * Creates an immutable text object from raw code units.
 *
 * @param string
 *      Pointer to code units in the specified encoding.
 * @param length
 *      Number of code units in string.
 * @param encoding
 *      String encoding (UTF-8, UTF-16, or UTF-32).
 * @return
 *      A reference to a text object if the call was successful, `NULL` otherwise.
 */
TR_PUBLIC TRTextRef TRTextCreate(const void *string, TRUInteger length, TRStringEncoding encoding);

/**
 * Creates a new immutable text object that is an exact copy of the source text.
 *
 * @param text
 *      Source text object to copy.
 * @return
 *      New immutable copy, or `NULL` on failure.
 */
TR_PUBLIC TRTextRef TRTextCreateCopy(TRTextRef text);

/**
 * Creates a new mutable text object that is an exact copy of the source text.
 *
 * The copy can be modified using the mutable text interface while the original remains unchanged.
 *
 * @param text
 *      Source text object to copy (can be immutable or mutable).
 * @return
 *      New mutable text object with the same content and properties as the source.
 */
TR_PUBLIC TRMutableTextRef TRTextCreateMutableCopy(TRTextRef text);

/**
 * Returns the encoding specified at text object creation.
 *
 * This encoding remains constant for the lifetime of the text object.
 *
 * @param text
 *      Text object.
 * @return
 *      String encoding used by this text object.
 */
TR_PUBLIC TRStringEncoding TRTextGetEncoding(TRTextRef text);

/**
 * Returns the number of code units in the text object.
 *
 * @param text
 *      Text object.
 * @return
 *      Number of code units in the text.
 */
TR_PUBLIC TRUInteger TRTextGetLength(TRTextRef text);

/**
 * Copies code units from the text into a caller-provided buffer.
 *
 * The buffer must be large enough to hold the requested number of code units in the text's encoding
 * format.
 *
 * @param text
 *      Text object.
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
TR_PUBLIC TRBoolean TRTextGetCodeUnits(TRTextRef text, TRUInteger index, TRUInteger length,
    void *buffer);

/**
 * Copies the attributes active at `index`, and the length of the run over which they apply (the
 * extent, starting at `index`, over which the attribute set stays the same).
 *
 * For more complex queries (filtering by a specific attribute, or by group/scope), use
 * `TRTextGetSheenBidiText()` of `Tehreer/TRSheenBidi.h` to access the underlying `SBTextRef`.
 *
 * @param text
 *      Text object.
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
TR_PUBLIC TRAttributeListRef TRTextCopyAttributes(TRTextRef text, TRUInteger index,
    TRUInteger *outLength);

/**
 * Increments the reference count of a text object.
 *
 * @param text
 *      The text object whose reference count will be incremented.
 * @return
 *      The same text object passed in as the parameter.
 */
TR_PUBLIC TRTextRef TRTextRetain(TRTextRef text);

/**
 * Decrements the reference count of a text object. The object will be deallocated when its
 * reference count reaches zero.
 *
 * @param text
 *      The text object whose reference count will be decremented.
 */
TR_PUBLIC void TRTextRelease(TRTextRef text);


/* ----------------------------------
 * Mutable Text
 * ---------------------------------- */

/**
 * Creates a new mutable text object that starts empty but can be modified through the mutable text
 * interface.
 *
 * @param encoding
 *      Target encoding for the new text (UTF-8/16/32).
 * @return
 *      A reference to a mutable text object if the call was successful, `NULL` otherwise.
 */
TR_PUBLIC TRMutableTextRef TRTextCreateMutable(TRStringEncoding encoding);

/**
 * Signals the start of a batch of editing operations. While in editing mode, text analysis
 * is deferred until TRTextEndEditing() is called. This improves performance when making multiple
 * sequential modifications.
 *
 * @param text
 *      Mutable text object.
 */
TR_PUBLIC void TRTextBeginEditing(TRMutableTextRef text);

/**
 * Signals the end of a batch editing session and triggers text analysis for all modified
 * paragraphs.
 *
 * @param text
 *      Mutable text object.
 */
TR_PUBLIC void TRTextEndEditing(TRMutableTextRef text);

/**
 * Adds new code units to the end of the text object. If the text is not in editing mode, analysis
 * is performed immediately on the new content.
 *
 * @param text
 *      Mutable text object.
 * @param codeUnitBuffer
 *      Pointer to code units in the text's encoding.
 * @param codeUnitCount
 *      Number of code units to append.
 * @return
 *      `TRTrue` if the code units were appended, `TRFalse` if the buffer is `NULL`.
 */
TR_PUBLIC TRBoolean TRTextAppendCodeUnits(TRMutableTextRef text, const void *codeUnitBuffer,
    TRUInteger codeUnitCount);

/**
 * Inserts new code units at the specified position in the text. Existing content at and after the
 * insertion point is shifted right. If the text is not in editing mode, analysis is performed
 * immediately.
 *
 * @param text
 *      Mutable text object.
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
TR_PUBLIC TRBoolean TRTextInsertCodeUnits(TRMutableTextRef text, TRUInteger index,
    const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Removes a contiguous range of code units from the text. Content after the deletion range is
 * shifted left. If the text is not in editing mode, analysis is performed immediately.
 *
 * @param text
 *      Mutable text object.
 * @param index
 *      Start index of the range to delete (in code units).
 * @param length
 *      Number of code units to delete.
 * @return
 *      `TRTrue` if the code units were deleted, `TRFalse` if the range is not within the text.
 */
TR_PUBLIC TRBoolean TRTextDeleteCodeUnits(TRMutableTextRef text, TRUInteger index,
    TRUInteger length);

/**
 * Completely replaces the current text content with the new code units. This is equivalent to
 * deleting all existing content and then inserting the new content at position 0.
 *
 * @param text
 *      Mutable text object.
 * @param codeUnitBuffer
 *      Pointer to code units in the text's encoding.
 * @param codeUnitCount
 *      Number of code units in codeUnitBuffer.
 * @return
 *      `TRTrue` if the content was replaced, `TRFalse` if the buffer is `NULL` for a count that is
 *      not zero.
 *
 * @warning
 *      All existing content and attributes are removed.
 *      The buffer must contain valid code units in the text's encoding format.
 */
TR_PUBLIC TRBoolean TRTextSetCodeUnits(TRMutableTextRef text, const void *codeUnitBuffer,
    TRUInteger codeUnitCount);

/**
 * Replaces a contiguous range of existing code units with new content. If the text is not in
 * editing mode, analysis is performed immediately.
 *
 * @param text
 *      Mutable text object.
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
TR_PUBLIC TRBoolean TRTextReplaceCodeUnits(TRMutableTextRef text, TRUInteger index,
    TRUInteger length, const void *codeUnitBuffer, TRUInteger codeUnitCount);

/**
 * Applies the specified attribute with the given value to the range of code units. If the attribute
 * already exists in parts of the range, those values are replaced.
 *
 * @param text
 *      Mutable text object.
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
TR_PUBLIC TRBoolean TRTextSetAttribute(TRMutableTextRef text, TRUInteger index, TRUInteger length,
    const TRAttribute *attribute);

/**
 * Removes the specified attribute from the given range of code units. If the attribute is not
 * present in the range, this operation has no effect.
 *
 * @param text
 *      Mutable text object.
 * @param index
 *      Start index of the range (in code units).
 * @param length
 *      Length of the range (in code units).
 * @param attributeType
 *      Type of the attribute to remove.
 * @return
 *      `TRTrue` if the attribute was removed, `TRFalse` if the range is not within the text or the
 *      type is unknown.
 */
TR_PUBLIC TRBoolean TRTextRemoveAttribute(TRMutableTextRef text, TRUInteger index,
    TRUInteger length, TRAttributeType attributeType);

TR_EXTERN_C_END

#endif
