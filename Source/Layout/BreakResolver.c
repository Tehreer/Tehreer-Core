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

#include <Tehreer/TRTypesetter.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRTypesetter.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

#include "BreakResolver.h"

static BreakType GetBreakType(TRBreakMode breakMode)
{
    return (breakMode == TRBreakModeCharacter ? BreakTypeGrapheme : BreakTypeLine);
}

/* Returns the index of the first block that starts at or after the code unit. */
static TRUInteger GetFirstBlockIndex(TypesetterRef typesetter, TRUInteger codeUnitIndex)
{
    TRUInteger low = 0;
    TRUInteger high = typesetter->blockCount;

    while (low < high) {
        TRUInteger mid = (low + high) >> 1;

        if (typesetter->blocks[mid]->codeUnitStart < codeUnitIndex) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return low;
}

static TextRunRef FindBlockForward(TypesetterRef typesetter, TRUInteger start, TRUInteger end)
{
    TRUInteger index;

    if (typesetter->blockCount == 0) {
        return NULL;
    }

    index = GetFirstBlockIndex(typesetter, start);
    if (index < typesetter->blockCount && typesetter->blocks[index]->codeUnitStart < end) {
        return typesetter->blocks[index];
    }

    return NULL;
}

static TextRunRef FindBlockBackward(TypesetterRef typesetter, TRUInteger start, TRUInteger end)
{
    TRUInteger index;

    if (typesetter->blockCount == 0) {
        return NULL;
    }

    index = GetFirstBlockIndex(typesetter, end);
    if (index > 0 && typesetter->blocks[index - 1]->codeUnitStart >= start) {
        return typesetter->blocks[index - 1];
    }

    return NULL;
}

static TRBoolean IsWhitespaceAt(TypesetterRef typesetter, TRUInteger index)
{
    return TextBufferIsWhitespace(TextBufferGetCodePoint(&typesetter->buffer, index));
}

static TRBoolean IsNewlineAt(TypesetterRef typesetter, TRUInteger index)
{
    return TextBufferGetCodeUnit(&typesetter->buffer, index) == 0x0A;
}

/* A line of a block holds the whitespace after it, until the newline that ends its paragraph. */
static TRUInteger KeepBlockLine(TypesetterRef typesetter, TextRunRef block, TRUInteger endIndex)
{
    TRUInteger lineEnd = block->codeUnitEnd;

    while (lineEnd < endIndex && IsWhitespaceAt(typesetter, lineEnd)) {
        lineEnd += 1;

        if (IsNewlineAt(typesetter, lineEnd - 1)) {
            break;
        }
    }

    return lineEnd;
}

static TRUInteger KeepBlockLineBackward(TypesetterRef typesetter, TextRunRef block,
    TRUInteger startIndex)
{
    TRUInteger lineStart = block->codeUnitStart;

    while (lineStart > startIndex && IsWhitespaceAt(typesetter, lineStart - 1)) {
        lineStart -= 1;

        if (IsNewlineAt(typesetter, lineStart)) {
            break;
        }
    }

    return lineStart;
}

/*
 * The range of the sequence of breaks goes from `clampedStart` to `clampedEnd`, while the line
 * starts at `startIndex` and may take whitespace of a block until `limitIndex`.
 */
static TRUInteger FindForwardBreak(TypesetterRef typesetter, TRFloat extent, BreakType type,
    TRUInteger startIndex, TRUInteger limitIndex, TRUInteger clampedEnd)
{
    TRUInteger forwardIndex = startIndex;
    TRUInteger cursor = startIndex;
    TRFloat measurement = 0.0f;

    while (cursor < clampedEnd) {
        TRUInteger endIndex = BreakClassifierGetForwardBreak(typesetter->breaks, type, cursor,
            clampedEnd);
        TextRunRef block = FindBlockForward(typesetter, forwardIndex, endIndex);

        cursor = endIndex;

        if (block) {
            /*
             * A block that is not the very first thing on the line ends the line right before it,
             * whatever text has been accepted so far; the block gets a line of its own later. A
             * block that is the first thing is the line, which stretches through the whitespace
             * that follows it.
             */
            if (block->codeUnitStart > startIndex) {
                return block->codeUnitStart;
            }

            return KeepBlockLine(typesetter, block, limitIndex);
        }

        measurement += TypesetterMeasureRange(typesetter, forwardIndex, endIndex);
        if (measurement > extent) {
            TRUInteger wsStart = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer,
                forwardIndex, endIndex);
            TRFloat wsExtent = TypesetterMeasureRange(typesetter, wsStart, endIndex);

            /* Break if excluding the extent of the whitespace helps. */
            if ((measurement - wsExtent) <= extent) {
                forwardIndex = endIndex;
            }
            break;
        }

        forwardIndex = endIndex;
    }

    return forwardIndex;
}

static TRUInteger FindBackwardBreak(TypesetterRef typesetter, TRFloat extent, BreakType type,
    TRUInteger endIndex, TRUInteger limitIndex, TRUInteger clampedStart)
{
    TRUInteger backwardIndex = endIndex;
    TRUInteger cursor = endIndex;
    TRFloat measurement = 0.0f;

    while (cursor > clampedStart) {
        TRUInteger startIndex = BreakClassifierGetBackwardBreak(typesetter->breaks, type, cursor,
            clampedStart);
        TextRunRef block = FindBlockBackward(typesetter, startIndex, backwardIndex);

        cursor = startIndex;

        if (block) {
            /*
             * A block that is not the nearest thing to the boundary already reached leaves
             * everything from it onward for this line, and nothing from it or before it; a block
             * that is the nearest thing is part of the line, together with the whitespace that
             * precedes it.
             */
            if (block->codeUnitEnd < backwardIndex) {
                return block->codeUnitEnd;
            }

            return KeepBlockLineBackward(typesetter, block, limitIndex);
        }

        measurement += TypesetterMeasureRange(typesetter, startIndex, backwardIndex);

        if (measurement > extent) {
            TRUInteger wsStart = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer,
                startIndex, backwardIndex);
            TRFloat wsExtent = TypesetterMeasureRange(typesetter, wsStart, backwardIndex);

            /* Break if excluding the extent of the whitespace helps. */
            if ((measurement - wsExtent) <= extent) {
                backwardIndex = startIndex;
            }
            break;
        }

        backwardIndex = startIndex;
    }

    return backwardIndex;
}

static TRUInteger FindForwardBreakInRange(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    ParagraphInfo *paragraph = &typesetter->paragraphs[TypesetterFindParagraph(typesetter, start)];
    TRUInteger maxIndex = (end < paragraph->end ? end : paragraph->end);

    return FindForwardBreak(typesetter, extent, GetBreakType(breakMode), start, end, maxIndex);
}

static TRUInteger FindBackwardBreakInRange(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    ParagraphInfo *paragraph = &typesetter->paragraphs[TypesetterFindParagraph(typesetter, end - 1)];
    TRUInteger minIndex = (start > paragraph->start ? start : paragraph->start);

    return FindBackwardBreak(typesetter, extent, GetBreakType(breakMode), end, start, minIndex);
}

static TRUInteger SuggestForwardCharacterBreak(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end)
{
    TRUInteger breakIndex = FindForwardBreakInRange(typesetter, extent, start, end,
        TRBreakModeCharacter);

    /* Take at least one character (grapheme) if the extent is too small. */
    if (breakIndex == start) {
        return BreakClassifierGetForwardBreak(typesetter->breaks, BreakTypeGrapheme, start, end);
    }

    return breakIndex;
}

static TRUInteger SuggestBackwardCharacterBreak(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end)
{
    TRUInteger breakIndex = FindBackwardBreakInRange(typesetter, extent, start, end,
        TRBreakModeCharacter);

    /* Take at least one character (grapheme) if the extent is too small. */
    if (breakIndex == end) {
        return BreakClassifierGetBackwardBreak(typesetter->breaks, BreakTypeGrapheme, end, start);
    }

    return breakIndex;
}

TR_INTERNAL TRUInteger BreakResolverSuggestForwardBreak(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    TRUInteger breakIndex;

    /* The range MUST NOT be empty. */
    TRAssert(start < end && end <= typesetter->buffer.length);

    if (breakMode == TRBreakModeCharacter) {
        return SuggestForwardCharacterBreak(typesetter, extent, start, end);
    }

    breakIndex = FindForwardBreakInRange(typesetter, extent, start, end, TRBreakModeLine);

    /* Fall back to a character break if no line break occurs in the extent. */
    if (breakIndex == start) {
        return SuggestForwardCharacterBreak(typesetter, extent, start, end);
    }

    return breakIndex;
}

TR_INTERNAL TRUInteger BreakResolverSuggestBackwardBreak(TypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    TRUInteger breakIndex;

    /* The range MUST NOT be empty. */
    TRAssert(start < end && end <= typesetter->buffer.length);

    if (breakMode == TRBreakModeCharacter) {
        return SuggestBackwardCharacterBreak(typesetter, extent, start, end);
    }

    breakIndex = FindBackwardBreakInRange(typesetter, extent, start, end, TRBreakModeLine);

    /* Fall back to a character break if no line break occurs in the extent. */
    if (breakIndex == end) {
        return SuggestBackwardCharacterBreak(typesetter, extent, start, end);
    }

    return breakIndex;
}
