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
#include <Layout/TextSource.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

#include "BreakResolver.h"

static BreakType GetBreakType(TRBreakMode breakMode)
{
    return (breakMode == TRBreakModeCharacter ? BreakTypeGrapheme : BreakTypeLine);
}

/* Finds the first block that starts in the range, which is the one that the break stops before. */
static TRBoolean FindBlockForward(TextSourceRef source, TRUInteger start, TRUInteger end,
    RunCursor *block)
{
    TRBoolean isFound = TRFalse;
    TRBoolean hasRun = TextSourceFindRun(source, start, block);

    while (!isFound && hasRun && block->start < end) {
        isFound = (block->start >= start && TextRunIsBlock(block->textRun));

        if (!isFound) {
            hasRun = TextSourceMoveToNextRun(source, block);
        }
    }

    return isFound;
}

/* Finds the last block that starts in the range, which is the one that the break stops after. */
static TRBoolean FindBlockBackward(TextSourceRef source, TRUInteger start, TRUInteger end,
    RunCursor *block)
{
    TRBoolean isFound = TRFalse;
    TRBoolean hasRun = TextSourceFindRun(source, end - 1, block);

    while (!isFound && hasRun && block->start >= start) {
        isFound = TextRunIsBlock(block->textRun);

        if (!isFound) {
            hasRun = TextSourceMoveToPreviousRun(source, block);
        }
    }

    return isFound;
}

static TRBoolean IsWhitespaceAt(TextSourceRef source, TRUInteger index)
{
    return TextBufferIsWhitespace(TextSourceGetCodePoint(source, index));
}

static TRBoolean IsNewlineAt(TextSourceRef source, TRUInteger index)
{
    return TextSourceGetCodeUnit(source, index) == 0x0A;
}

/* A line of a block holds the whitespace after it, until the newline that ends its paragraph. */
static TRUInteger KeepBlockLine(TextSourceRef source, const RunCursor *block,
    TRUInteger endIndex)
{
    TRUInteger lineEnd = block->end;

    while (lineEnd < endIndex && IsWhitespaceAt(source, lineEnd)) {
        lineEnd += 1;

        if (IsNewlineAt(source, lineEnd - 1)) {
            break;
        }
    }

    return lineEnd;
}

static TRUInteger KeepBlockLineBackward(TextSourceRef source, const RunCursor *block,
    TRUInteger startIndex)
{
    TRUInteger lineStart = block->start;

    while (lineStart > startIndex && IsWhitespaceAt(source, lineStart - 1)) {
        lineStart -= 1;

        if (IsNewlineAt(source, lineStart)) {
            break;
        }
    }

    return lineStart;
}

/*
 * A block that is not the very first thing on the line ends the line right before it, whatever
 * text has been accepted so far; the block gets a line of its own later. A block that is the first
 * thing is the line, which stretches through the whitespace that follows it.
 */
static TRUInteger ResolveForwardBlock(TextSourceRef source, const RunCursor *block,
    TRUInteger startIndex, TRUInteger limitIndex)
{
    TRUInteger breakIndex = block->start;

    if (block->start <= startIndex) {
        breakIndex = KeepBlockLine(source, block, limitIndex);
    }

    return breakIndex;
}

/*
 * A block that is not the nearest thing to the boundary already reached leaves everything from it
 * onward for this line, and nothing from it or before it; a block that is the nearest thing is
 * part of the line, together with the whitespace that precedes it.
 */
static TRUInteger ResolveBackwardBlock(TextSourceRef source, const RunCursor *block,
    TRUInteger backwardIndex, TRUInteger limitIndex)
{
    TRUInteger breakIndex = block->end;

    if (block->end >= backwardIndex) {
        breakIndex = KeepBlockLineBackward(source, block, limitIndex);
    }

    return breakIndex;
}

/* Tells if the part still fits the extent after excluding the whitespace at its end. */
static TRBoolean FitsWithoutWhitespace(TextSourceRef source, TRFloat measurement,
    TRFloat extent, TRUInteger start, TRUInteger end)
{
    TRUInteger wsStart = TextSourceGetTrailingWhitespaceStart(source, start, end);
    TRFloat wsExtent = TextSourceMeasureRange(source, wsStart, end);

    return (measurement - wsExtent) <= extent;
}

/*
 * The range of the sequence of breaks goes from `clampedStart` to `clampedEnd`, while the line
 * starts at `startIndex` and may take whitespace of a block until `limitIndex`.
 */
static TRUInteger FindForwardBreak(TextSourceRef source, TRFloat extent, BreakType type,
    TRUInteger startIndex, TRUInteger limitIndex, TRUInteger clampedEnd)
{
    TRUInteger forwardIndex = startIndex;
    TRUInteger cursor = startIndex;
    TRFloat measurement = 0.0f;

    while (cursor < clampedEnd) {
        TRUInteger endIndex = TextSourceGetForwardBreak(source, type, cursor, clampedEnd);
        RunCursor block;

        cursor = endIndex;

        if (FindBlockForward(source, forwardIndex, endIndex, &block)) {
            forwardIndex = ResolveForwardBlock(source, &block, startIndex, limitIndex);
            break;
        }

        measurement += TextSourceMeasureRange(source, forwardIndex, endIndex);

        if (measurement > extent) {
            /* Break if excluding the extent of the whitespace helps. */
            if (FitsWithoutWhitespace(source, measurement, extent, forwardIndex, endIndex)) {
                forwardIndex = endIndex;
            }
            break;
        }

        forwardIndex = endIndex;
    }

    return forwardIndex;
}

static TRUInteger FindBackwardBreak(TextSourceRef source, TRFloat extent, BreakType type,
    TRUInteger endIndex, TRUInteger limitIndex, TRUInteger clampedStart)
{
    TRUInteger backwardIndex = endIndex;
    TRUInteger cursor = endIndex;
    TRFloat measurement = 0.0f;

    while (cursor > clampedStart) {
        TRUInteger startIndex = TextSourceGetBackwardBreak(source, type, cursor,
            clampedStart);
        RunCursor block;

        cursor = startIndex;

        if (FindBlockBackward(source, startIndex, backwardIndex, &block)) {
            backwardIndex = ResolveBackwardBlock(source, &block, backwardIndex, limitIndex);
            break;
        }

        measurement += TextSourceMeasureRange(source, startIndex, backwardIndex);

        if (measurement > extent) {
            /* Break if excluding the extent of the whitespace helps. */
            if (FitsWithoutWhitespace(source, measurement, extent, startIndex, backwardIndex)) {
                backwardIndex = startIndex;
            }
            break;
        }

        backwardIndex = startIndex;
    }

    return backwardIndex;
}

static TRUInteger FindForwardBreakInRange(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    ParagraphInfo paragraph;
    TRUInteger maxIndex;

    TextSourceGetParagraph(source, start, &paragraph);
    maxIndex = NumberMin(end, paragraph.end);

    return FindForwardBreak(source, extent, GetBreakType(breakMode), start, end, maxIndex);
}

static TRUInteger FindBackwardBreakInRange(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    ParagraphInfo paragraph;
    TRUInteger minIndex;

    TextSourceGetParagraph(source, end - 1, &paragraph);
    minIndex = NumberMax(start, paragraph.start);

    return FindBackwardBreak(source, extent, GetBreakType(breakMode), end, start, minIndex);
}

static TRUInteger SuggestForwardCharacterBreak(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end)
{
    TRUInteger breakIndex = FindForwardBreakInRange(source, extent, start, end,
        TRBreakModeCharacter);

    /* Take at least one character (grapheme) if the extent is too small. */
    if (breakIndex == start) {
        breakIndex = TextSourceGetForwardBreak(source, BreakTypeGrapheme, start, end);
    }

    return breakIndex;
}

static TRUInteger SuggestBackwardCharacterBreak(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end)
{
    TRUInteger breakIndex = FindBackwardBreakInRange(source, extent, start, end,
        TRBreakModeCharacter);

    /* Take at least one character (grapheme) if the extent is too small. */
    if (breakIndex == end) {
        breakIndex = TextSourceGetBackwardBreak(source, BreakTypeGrapheme, end, start);
    }

    return breakIndex;
}

TR_INTERNAL TRUInteger BreakResolverSuggestForwardBreak(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    TRUInteger breakIndex;

    /* The range MUST NOT be empty. */
    TRAssert(start < end && end <= TextSourceGetLength(source));

    if (breakMode == TRBreakModeCharacter) {
        breakIndex = SuggestForwardCharacterBreak(source, extent, start, end);
    } else {
        breakIndex = FindForwardBreakInRange(source, extent, start, end, TRBreakModeLine);

        /* Fall back to a character break if no line break occurs in the extent. */
        if (breakIndex == start) {
            breakIndex = SuggestForwardCharacterBreak(source, extent, start, end);
        }
    }

    return breakIndex;
}

TR_INTERNAL TRUInteger BreakResolverSuggestBackwardBreak(TextSourceRef source, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode)
{
    TRUInteger breakIndex;

    /* The range MUST NOT be empty. */
    TRAssert(start < end && end <= TextSourceGetLength(source));

    if (breakMode == TRBreakModeCharacter) {
        breakIndex = SuggestBackwardCharacterBreak(source, extent, start, end);
    } else {
        breakIndex = FindBackwardBreakInRange(source, extent, start, end, TRBreakModeLine);

        /* Fall back to a character break if no line break occurs in the extent. */
        if (breakIndex == end) {
            breakIndex = SuggestBackwardCharacterBreak(source, extent, start, end);
        }
    }

    return breakIndex;
}
