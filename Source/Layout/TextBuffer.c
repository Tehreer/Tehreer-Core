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

#include <SheenBidi/SBCodepoint.h>

#include <Tehreer/TRString.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>

#include "TextBuffer.h"

#define InvalidCodePoint    0xFFFFFFFF

TR_INTERNAL TRUInt32 TextBufferDecodeNext(const TextBuffer *buffer, TRUInteger *index)
{
    /* Index MUST be less than the length. */
    TRAssert(*index < buffer->length);

    switch (buffer->encoding) {
    case TRStringEncodingUTF8:
        return SBCodepointDecodeNextFromUTF8(buffer->codeUnits, buffer->length, index);

    case TRStringEncodingUTF16:
        return SBCodepointDecodeNextFromUTF16(buffer->codeUnits, buffer->length, index);

    case TRStringEncodingUTF32:
        return ((const TRUInt32 *)buffer->codeUnits)[(*index)++];
    }

    (*index)++;

    return InvalidCodePoint;
}

TR_INTERNAL TRUInt32 TextBufferGetCodePoint(const TextBuffer *buffer, TRUInteger index)
{
    return TextBufferDecodeNext(buffer, &index);
}

TR_INTERNAL TRUInt32 TextBufferGetCodeUnit(const TextBuffer *buffer, TRUInteger index)
{
    /* Index MUST be less than the length. */
    TRAssert(index < buffer->length);

    switch (buffer->encoding) {
    case TRStringEncodingUTF8:
        return ((const TRUInt8 *)buffer->codeUnits)[index];

    case TRStringEncodingUTF16:
        return ((const TRUInt16 *)buffer->codeUnits)[index];

    case TRStringEncodingUTF32:
        return ((const TRUInt32 *)buffer->codeUnits)[index];
    }

    return 0;
}

TR_INTERNAL TRBoolean TextBufferIsWhitespace(TRUInt32 codePoint)
{
    if (codePoint <= 0x20) {
        return (codePoint >= 0x09 && codePoint <= 0x0D) || codePoint == 0x20;
    }

    switch (codePoint) {
    case 0x0085:
    case 0x00A0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
        return TRTrue;
    }

    return (codePoint >= 0x2000 && codePoint <= 0x200A);
}

TR_INTERNAL TRUInteger TextBufferGetLeadingWhitespaceEnd(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end)
{
    TRUInteger index = start;

    while (index < end) {
        TRUInteger next = index;

        if (!TextBufferIsWhitespace(TextBufferDecodeNext(buffer, &next))) {
            return index;
        }

        index = next;
    }

    return end;
}

TR_INTERNAL TRUInteger TextBufferGetTrailingWhitespaceStart(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end)
{
    TRUInteger index = end;

    /* Scan backward by code points, which are found by going back to the start of each. */
    while (index > start) {
        TRUInteger previous = index - 1;

        /* Skip the continuation units of the code point that ends at the index. */
        if (buffer->encoding == TRStringEncodingUTF8) {
            while (previous > start && (TextBufferGetCodeUnit(buffer, previous) & 0xC0) == 0x80) {
                previous -= 1;
            }
        } else if (buffer->encoding == TRStringEncodingUTF16) {
            TRUInt32 unit = TextBufferGetCodeUnit(buffer, previous);

            if (unit >= 0xDC00 && unit <= 0xDFFF && previous > start) {
                TRUInt32 high = TextBufferGetCodeUnit(buffer, previous - 1);

                if (high >= 0xD800 && high <= 0xDBFF) {
                    previous -= 1;
                }
            }
        }

        if (!TextBufferIsWhitespace(TextBufferGetCodePoint(buffer, previous))) {
            return index;
        }

        index = previous;
    }

    return start;
}

TR_INTERNAL TRUInteger TextBufferFindWhitespace(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end)
{
    TRUInteger index = start;

    while (index < end) {
        TRUInteger next = index;

        if (TextBufferIsWhitespace(TextBufferDecodeNext(buffer, &next))) {
            return index;
        }

        index = next;
    }

    return end;
}
