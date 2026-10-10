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
    TRUInt32 codePoint = InvalidCodePoint;

    /* Index MUST be less than the length. */
    TRAssert(*index < buffer->length);

    switch (buffer->encoding) {
    case TRStringEncodingUTF8:
        codePoint = SBCodepointDecodeNextFromUTF8(buffer->codeUnits, buffer->length, index);
        break;

    case TRStringEncodingUTF16:
        codePoint = SBCodepointDecodeNextFromUTF16(buffer->codeUnits, buffer->length, index);
        break;

    case TRStringEncodingUTF32:
        codePoint = ((const TRUInt32 *)buffer->codeUnits)[(*index)++];
        break;

    default:
        (*index)++;
        break;
    }

    return codePoint;
}

TR_INTERNAL TRUInt32 TextBufferDecodePrevious(const TextBuffer *buffer, TRUInteger *index)
{
    TRUInt32 codePoint = InvalidCodePoint;

    /* Index MUST be in the range from one to the length. */
    TRAssert(*index > 0 && *index <= buffer->length);

    switch (buffer->encoding) {
    case TRStringEncodingUTF8:
        codePoint = SBCodepointDecodePreviousFromUTF8(buffer->codeUnits, buffer->length, index);
        break;

    case TRStringEncodingUTF16:
        codePoint = SBCodepointDecodePreviousFromUTF16(buffer->codeUnits, buffer->length, index);
        break;

    case TRStringEncodingUTF32:
        codePoint = ((const TRUInt32 *)buffer->codeUnits)[--(*index)];
        break;

    default:
        (*index)--;
        break;
    }

    return codePoint;
}

TR_INTERNAL TRUInt32 TextBufferGetCodePoint(const TextBuffer *buffer, TRUInteger index)
{
    return TextBufferDecodeNext(buffer, &index);
}

TR_INTERNAL TRUInt32 TextBufferGetCodeUnit(const TextBuffer *buffer, TRUInteger index)
{
    TRUInt32 codeUnit = 0;

    /* Index MUST be less than the length. */
    TRAssert(index < buffer->length);

    switch (buffer->encoding) {
    case TRStringEncodingUTF8:
        codeUnit = ((const TRUInt8 *)buffer->codeUnits)[index];
        break;

    case TRStringEncodingUTF16:
        codeUnit = ((const TRUInt16 *)buffer->codeUnits)[index];
        break;

    case TRStringEncodingUTF32:
        codeUnit = ((const TRUInt32 *)buffer->codeUnits)[index];
        break;
    }

    return codeUnit;
}

TR_INTERNAL TRBoolean TextBufferIsWhitespace(TRUInt32 codePoint)
{
    TRBoolean isWhitespace;

    if (codePoint <= 0x20) {
        isWhitespace = (codePoint >= 0x09 && codePoint <= 0x0D) || codePoint == 0x20;
    } else {
        switch (codePoint) {
        case 0x0085:
        case 0x00A0:
        case 0x1680:
        case 0x2028:
        case 0x2029:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            isWhitespace = TRTrue;
            break;

        default:
            isWhitespace = (codePoint >= 0x2000 && codePoint <= 0x200A);
            break;
        }
    }

    return isWhitespace;
}

TR_INTERNAL TRUInteger TextBufferGetLeadingWhitespaceEnd(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end)
{
    TRUInteger whitespaceEnd = end;
    TRUInteger index = start;

    while (index < end) {
        TRUInteger next = index;

        if (TextBufferIsWhitespace(TextBufferDecodeNext(buffer, &next))) {
            index = next;
        } else {
            whitespaceEnd = index;
            break;
        }
    }

    return whitespaceEnd;
}

TR_INTERNAL TRUInteger TextBufferGetTrailingWhitespaceStart(const TextBuffer *buffer,
    TRUInteger start, TRUInteger end)
{
    TRUInteger whitespaceStart = start;
    TRUInteger index = end;

    /* Scan backward by code points. */
    while (index > start) {
        TRUInteger previous = index;

        if (TextBufferIsWhitespace(TextBufferDecodePrevious(buffer, &previous))) {
            index = previous;
        } else {
            whitespaceStart = index;
            break;
        }
    }

    return whitespaceStart;
}

TR_INTERNAL TRUInteger TextBufferFindWhitespace(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end)
{
    TRUInteger whitespaceStart = end;
    TRUInteger index = start;

    while (index < end) {
        TRUInteger next = index;

        if (TextBufferIsWhitespace(TextBufferDecodeNext(buffer, &next))) {
            whitespaceStart = index;
            break;
        } else {
            index = next;
        }
    }

    return whitespaceStart;
}
