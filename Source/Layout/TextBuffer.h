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

#ifndef _TEHREER_LAYOUT_TEXT_BUFFER_H
#define _TEHREER_LAYOUT_TEXT_BUFFER_H

#include <Tehreer/TRString.h>

#include <API/TRBase.h>

/*
 * The code units of a text in one of the supported encodings. Indexes and lengths count code
 * units, not bytes, characters or code points.
 */
typedef struct _TextBuffer {
    const void *codeUnits;
    TRUInteger length;
    TRStringEncoding encoding;
} TextBuffer;

/*
 * Decodes the code point at `*index` and moves `*index` past it. The index MUST be less than the
 * length. Malformed sequences give a code point that is not whitespace.
 */
TR_INTERNAL TRUInt32 TextBufferDecodeNext(const TextBuffer *buffer, TRUInteger *index);

/*
 * Decodes the code point that ends at `*index` and moves `*index` to its start. The index MUST be
 * greater than zero and not greater than the length. Malformed sequences give a code point that is
 * not whitespace.
 */
TR_INTERNAL TRUInt32 TextBufferDecodePrevious(const TextBuffer *buffer, TRUInteger *index);

/* Returns the code point at the index, which MUST be less than the length. */
TR_INTERNAL TRUInt32 TextBufferGetCodePoint(const TextBuffer *buffer, TRUInteger index);

/* Returns the code unit at the index, which MUST be less than the length. */
TR_INTERNAL TRUInt32 TextBufferGetCodeUnit(const TextBuffer *buffer, TRUInteger index);

/*
 * Returns whether a code point has the White_Space property of Unicode, which includes the line
 * and paragraph separators, the no-break spaces and the ideographic space.
 */
TR_INTERNAL TRBoolean TextBufferIsWhitespace(TRUInt32 codePoint);

/* Returns the end of the whitespace at the start of the range, or its start if there is none. */
TR_INTERNAL TRUInteger TextBufferGetLeadingWhitespaceEnd(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end);

/* Returns the start of the whitespace at the end of the range, or its end if there is none. */
TR_INTERNAL TRUInteger TextBufferGetTrailingWhitespaceStart(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end);

/* Returns the index of the first whitespace in the range, or its end if there is none. */
TR_INTERNAL TRUInteger TextBufferFindWhitespace(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end);

#endif
