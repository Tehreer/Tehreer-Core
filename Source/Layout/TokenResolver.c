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

#include <stddef.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypeface.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRTypesetter.h>
#include <Layout/LineResolver.h>
#include <Layout/TextRun.h>

#include "TokenResolver.h"

/* The ellipsis character and the dots that stand in for it. */
#define EllipsisCodePoint   0x2026

static TRUInteger EncodeDots(TRStringEncoding encoding, TRUInt8 *buffer)
{
    TRUInteger length = 0;
    TRUInteger index;

    switch (encoding) {
    case TRStringEncodingUTF8:
        for (index = 0; index < 3; index++) {
            buffer[index] = '.';
        }
        length = 3;
        break;

    case TRStringEncodingUTF16:
        for (index = 0; index < 3; index++) {
            ((TRUInt16 *)buffer)[index] = '.';
        }
        length = 3;
        break;

    case TRStringEncodingUTF32:
        for (index = 0; index < 3; index++) {
            ((TRUInt32 *)buffer)[index] = '.';
        }
        length = 3;
        break;
    }

    return length;
}

static TRUInteger EncodeEllipsis(TRStringEncoding encoding, TRUInt8 *buffer)
{
    TRUInteger length = 0;

    switch (encoding) {
    case TRStringEncodingUTF8:
        buffer[0] = 0xE2;
        buffer[1] = 0x80;
        buffer[2] = 0xA6;
        length = 3;
        break;

    case TRStringEncodingUTF16:
        ((TRUInt16 *)buffer)[0] = EllipsisCodePoint;
        length = 1;
        break;

    case TRStringEncodingUTF32:
        ((TRUInt32 *)buffer)[0] = EllipsisCodePoint;
        length = 1;
        break;
    }

    return length;
}

TR_INTERNAL TRComposedLine *TokenResolverCreateTokenLine(TRTypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRTruncationPlace truncationPlace, const void *tokenString,
    TRUInteger tokenLength, TRStringEncoding tokenEncoding)
{
    TRComposedLine *tokenLine = NULL;
    TRUInteger truncationIndex;
    TRUInteger runIndex;

    /* The range MUST NOT be empty. */
    TRAssert(start < end);

    switch (truncationPlace) {
    case TRTruncationPlaceStart:
        truncationIndex = start;
        break;

    case TRTruncationPlaceMiddle:
        truncationIndex = start + ((end - start) / 2);
        break;

    default:
        truncationIndex = end - 1;
        break;
    }

    runIndex = TRTypesetterFindRun(typesetter, truncationIndex);

    if (runIndex != TRInvalidIndex) {
        TextRunRef suitableRun;
        TRTextRef tokenText;
        TRAttribute attributes[2];
        TRUInt8 defaultToken[3 * sizeof(TRUInt32)];

        suitableRun = typesetter->runs[runIndex];

        if (!tokenString || tokenLength == 0) {
            /* The ellipsis character is used if the typeface has it, and three dots if not. */
            tokenEncoding = typesetter->buffer.encoding;
            tokenString = defaultToken;

            if (TRTypefaceGetGlyphID(suitableRun->typeface, EllipsisCodePoint) == 0) {
                tokenLength = EncodeDots(tokenEncoding, defaultToken);
            } else {
                tokenLength = EncodeEllipsis(tokenEncoding, defaultToken);
            }
        }

        attributes[0].type = TRAttributeTypeface;
        attributes[0].value.typeface = suitableRun->typeface;
        attributes[1].type = TRAttributePointSize;
        attributes[1].value.pointSize = suitableRun->typeSize;

        tokenText = TRTextCreate(tokenString, tokenLength, tokenEncoding);

        if (tokenText) {
            TRTypesetterRef tokenTypesetter;

            tokenTypesetter = TRTypesetterCreate(tokenText, attributes, 2);

            if (tokenTypesetter) {
                tokenLine = LineResolverCreateSimpleLine(tokenTypesetter, 0,
                    tokenLength, TRFalse, 0.0f);

                TRTypesetterRelease(tokenTypesetter);
            }

            TRTextRelease(tokenText);
        }
    }

    return tokenLine;
}
