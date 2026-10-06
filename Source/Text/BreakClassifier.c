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

#include <graphemebreak.h>
#include <linebreak.h>

#include <Tehreer/TRString.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Object.h>

#include "BreakClassifier.h"

static void ClassifyLineBreaks(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding, char *types)
{
    switch (encoding) {
    case TRStringEncodingUTF8:
        set_linebreaks_utf8(codeUnits, length, NULL, types);
        break;

    case TRStringEncodingUTF16:
        set_linebreaks_utf16(codeUnits, length, NULL, types);
        break;

    case TRStringEncodingUTF32:
        set_linebreaks_utf32(codeUnits, length, NULL, types);
        break;
    }
}

static void ClassifyGraphemeBreaks(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding, char *types)
{
    switch (encoding) {
    case TRStringEncodingUTF8:
        set_graphemebreaks_utf8(codeUnits, length, NULL, types);
        break;

    case TRStringEncodingUTF16:
        set_graphemebreaks_utf16(codeUnits, length, NULL, types);
        break;

    case TRStringEncodingUTF32:
        set_graphemebreaks_utf32(codeUnits, length, NULL, types);
        break;
    }
}

TR_INTERNAL BreakClassifierRef BreakClassifierCreate(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding)
{
    TRUInteger sizes[2];
    void *pointers[2] = { NULL };
    BreakClassifierRef classifier;
    char *types;

    if (encoding > TRStringEncodingUTF32 || (length > 0 && !codeUnits)) {
        return NULL;
    }

    sizes[0] = sizeof(BreakClassifier);
    sizes[1] = length;

    types = (length > 0 ? AllocatorAllocateBlock(length) : NULL);
    if (length > 0 && !types) {
        return NULL;
    }

    classifier = ObjectCreate(sizes, 2, pointers, NULL);

    if (classifier) {
        TRUInteger index;

        classifier->length = length;
        classifier->breaks = pointers[1];

        if (length > 0) {
            for (index = 0; index < length; index++) {
                classifier->breaks[index] = 0;
            }

            ClassifyLineBreaks(codeUnits, length, encoding, types);
            for (index = 0; index < length; index++) {
                if (types[index] == LINEBREAK_MUSTBREAK || types[index] == LINEBREAK_ALLOWBREAK) {
                    classifier->breaks[index] |= BreakTypeLine;
                }
            }

            ClassifyGraphemeBreaks(codeUnits, length, encoding, types);
            for (index = 0; index < length; index++) {
                if (types[index] == GRAPHEMEBREAK_BREAK) {
                    classifier->breaks[index] |= BreakTypeGrapheme;
                }
            }
        }
    }

    AllocatorDeallocateBlock(types);

    return classifier;
}

TR_INTERNAL TRBoolean BreakClassifierHasBreak(BreakClassifierRef classifier, BreakType type,
    TRUInteger index)
{
    /* Index MUST be within the text. */
    TRAssert(index <= classifier->length);

    if (index == 0) {
        return TRFalse;
    }

    return (classifier->breaks[index - 1] & type) != 0;
}

TR_INTERNAL TRUInteger BreakClassifierGetForwardBreak(BreakClassifierRef classifier,
    BreakType type, TRUInteger from, TRUInteger to)
{
    TRUInteger index = from;

    /* The range MUST be non-empty and within the text. */
    TRAssert(from < to && to <= classifier->length);

    while (index < to) {
        TRUInt8 breakTypes = classifier->breaks[index];
        index += 1;

        if (breakTypes & type) {
            break;
        }
    }

    return index;
}

TR_INTERNAL TRUInteger BreakClassifierGetBackwardBreak(BreakClassifierRef classifier,
    BreakType type, TRUInteger from, TRUInteger to)
{
    TRUInteger index = from - 1;

    /* The range MUST be non-empty and within the text. */
    TRAssert(from > to && from <= classifier->length);

    while (index > to) {
        if (classifier->breaks[index - 1] & type) {
            break;
        }

        index -= 1;
    }

    return index;
}

TR_INTERNAL BreakClassifierRef BreakClassifierRetain(BreakClassifierRef classifier)
{
    return ObjectRetain((ObjectRef)classifier);
}

TR_INTERNAL void BreakClassifierRelease(BreakClassifierRef classifier)
{
    ObjectRelease((ObjectRef)classifier);
}
