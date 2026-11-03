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
#include <string.h>

#include <Tehreer/TRAttribute.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRGlyphRun.h>
#include <Core/Allocator.h>
#include <Core/Array.h>
#include <Layout/BreakResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Layout/TextSource.h>

#include "LineResolver.h"

/* The part of a range that is skipped by a truncation, and where the token goes. */
typedef struct _TruncationHandler {
    TextSourceRef source;
    RunList *list;
    TRUInteger skipStart;
    TRUInteger skipEnd;
    TRUInteger leadingTokenIndex;
    TRUInteger trailingTokenIndex;
} TruncationHandler;

/*
 * Finds the visual runs of a range in the order that they are shown, which is not the order of the
 * text where it has mixed directions. A range that spans paragraphs shows them one after another in
 * the direction of the first of them. The range MUST NOT be empty. Returns TRFalse if the runs
 * could not be found.
 */
static TRBoolean FindVisualRuns(TextSourceRef source, TRUInteger start, TRUInteger end,
    Array *visualRuns)
{
    TRBoolean isFound = TRFalse;
    TRBoolean hasMore = TRTrue;
    ParagraphInfo paragraph;
    TRBoolean isRTL;

    TextSourceGetParagraph(source, start, &paragraph);
    isRTL = (paragraph.baseLevel & 1) == 1;

    /* The paragraphs of a right-to-left line are shown from the last one to the first. */
    if (isRTL && paragraph.end < end) {
        TextSourceGetParagraph(source, end - 1, &paragraph);
    }

    while (hasMore) {
        TRUInteger feasibleStart = NumberMax(paragraph.start, start);
        TRUInteger feasibleEnd = NumberMin(paragraph.end, end);

        isFound = TextSourceAddVisualRuns(source, feasibleStart, feasibleEnd, visualRuns);

        if (isRTL) {
            hasMore = (isFound && feasibleStart != start);

            if (hasMore) {
                TextSourceGetParagraph(source, feasibleStart - 1, &paragraph);
            }
        } else {
            hasMore = (isFound && feasibleEnd != end);

            if (hasMore) {
                TextSourceGetParagraph(source, feasibleEnd, &paragraph);
            }
        }
    }

    return isFound;
}

/* A replacement is taken as a whole, and its room is decided by the layout width. */
static TRUInteger AppendReplacementRun(TextSourceRef source, RunList *list,
    const RunCursor *run, TRUInteger insertIndex, TRUInteger start, TRUInteger end,
    TRBoolean isForwardRun, TRFloat layoutWidth)
{
    GlyphRunPaint paint;
    TextRunRef sizedRun;

    TextSourceGetPaint(source, start, &paint);
    sizedRun = TextRunCreateForLayoutWidth(run->textRun, layoutWidth);

    if (sizedRun) {
        RunListInsert(list, insertIndex,
            TRGlyphRunCreate(sizedRun, run->paragraph.start, start, end, &paint));
        TextRunRelease(sizedRun);
    } else {
        list->hasFailed = TRTrue;
    }

    if (isForwardRun) {
        insertIndex += 1;
    }

    return insertIndex;
}

/* Each part that is painted alike has a run of its own. */
static TRUInteger AppendPaintedRuns(TextSourceRef source, RunList *list,
    const RunCursor *run, TRUInteger insertIndex, TRUInteger start, TRUInteger end,
    TRBoolean isForwardRun)
{
    TRUInteger spanStart = start;

    while (spanStart < end) {
        GlyphRunPaint paint;
        TRUInteger spanLength;
        TRUInteger spanEnd;

        spanLength = TextSourceGetPaint(source, spanStart, &paint);
        spanEnd = spanStart + spanLength;

        if (spanLength == 0 || spanEnd > end) {
            spanEnd = end;
        }

        RunListInsert(list, insertIndex,
            TRGlyphRunCreate(run->textRun, run->paragraph.start, spanStart, spanEnd, &paint));

        if (isForwardRun) {
            insertIndex += 1;
        }

        spanStart = spanEnd;
    }

    return insertIndex;
}

/* Handles the first part of the characters, which comes before the skipped ones. */
static void AppendRunBeforeSkip(TruncationHandler *handler, const VisualRun *visualRun)
{
    TRUInteger visualEnd = visualRun->end;

    LineResolverAppendVisualRuns(handler->source, visualRun->start,
        NumberMin(visualEnd, handler->skipStart), handler->list, 0.0f);
}

/* Handles the second part of the characters, which comes after the skipped ones. */
static void AppendRunAfterSkip(TruncationHandler *handler, const VisualRun *visualRun)
{
    TRUInteger visualStart = visualRun->start;

    LineResolverAppendVisualRuns(handler->source,
        NumberMax(visualStart, handler->skipEnd), visualRun->end,
        handler->list, 0.0f);
}

static void AppendRunOfTruncation(TruncationHandler *handler, const VisualRun *visualRun)
{
    RunList *list = handler->list;
    TRUInteger visualStart = visualRun->start;
    TRUInteger visualEnd = visualRun->end;

    if ((visualRun->level & 1) == 1) {
        /* Handle the second part of the characters. */
        if (visualEnd >= handler->skipEnd) {
            AppendRunAfterSkip(handler, visualRun);

            if (visualStart < handler->skipEnd) {
                handler->trailingTokenIndex = ArrayGetCount(&list->runs);
            }
        }

        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = ArrayGetCount(&list->runs);
            }

            AppendRunBeforeSkip(handler, visualRun);
        }
    } else {
        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            AppendRunBeforeSkip(handler, visualRun);

            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = ArrayGetCount(&list->runs);
            }
        }

        /* Handle the second part of the characters. */
        if (visualEnd >= handler->skipEnd) {
            if (visualStart < handler->skipEnd) {
                handler->trailingTokenIndex = ArrayGetCount(&list->runs);
            }

            AppendRunAfterSkip(handler, visualRun);
        }
    }
}

/* Appends all runs of the range except the skipped ones, and finds where the token goes. */
static TRBoolean AppendRunsAroundSkip(TextSourceRef source, TRUInteger start, TRUInteger end,
    TRUInteger skipStart, TRUInteger skipEnd, RunList *list, TRUInteger *leadingTokenIndex,
    TRUInteger *trailingTokenIndex)
{
    TruncationHandler handler;
    Array visualRuns;
    TRBoolean isFound;

    handler.source = source;
    handler.list = list;
    handler.skipStart = skipStart;
    handler.skipEnd = skipEnd;
    handler.leadingTokenIndex = TRInvalidIndex;
    handler.trailingTokenIndex = TRInvalidIndex;

    ArrayInitialize(&visualRuns, sizeof(VisualRun));
    isFound = FindVisualRuns(source, start, end, &visualRuns);

    if (isFound) {
        TRUInteger count = ArrayGetCount(&visualRuns);
        TRUInteger index;

        for (index = 0; index < count; index++) {
            AppendRunOfTruncation(&handler, ArrayGetItem(&visualRuns, index));
        }
    }

    ArrayFinalize(&visualRuns);

    *leadingTokenIndex = handler.leadingTokenIndex;
    *trailingTokenIndex = handler.trailingTokenIndex;

    return isFound;
}

/* Inserts copies of the runs of the token at the index, in the same order. */
static void AppendTokenRuns(TRComposedLineRef token, RunList *list, TRUInteger index)
{
    TRUInteger tokenIndex;

    for (tokenIndex = 0; tokenIndex < token->runCount; tokenIndex++) {
        TRGlyphRun *tokenRun = TRGlyphRunCreateCopy(token->runs[tokenIndex]);

        if (tokenRun) {
            tokenRun->flags |= TRGlyphRunFlagToken;
        }

        RunListInsert(list, index + tokenIndex, tokenRun);
    }
}

/* Marks a line that shows a token in place of some of its text. */
static TRComposedLine *MarkTruncated(TRComposedLine *line)
{
    if (line) {
        line->isTruncated = TRTrue;
    }

    return line;
}

static TRUInt8 GetBaseLevel(TextSourceRef source, TRUInteger index)
{
    ParagraphInfo paragraph;

    TextSourceGetParagraph(source, index, &paragraph);

    return paragraph.baseLevel;
}

/* Appends the glyph runs of a range, in the order of the visual runs that it has. */
static TRBoolean AppendRunsOfRange(TextSourceRef source, TRUInteger start, TRUInteger end,
    RunList *list, TRFloat layoutWidth)
{
    Array visualRuns;
    TRBoolean isAppended;

    ArrayInitialize(&visualRuns, sizeof(VisualRun));
    isAppended = FindVisualRuns(source, start, end, &visualRuns);

    if (isAppended) {
        TRUInteger count = ArrayGetCount(&visualRuns);
        TRUInteger index;

        for (index = 0; index < count; index++) {
            const VisualRun *visualRun = ArrayGetItem(&visualRuns, index);

            LineResolverAppendVisualRuns(source, visualRun->start, visualRun->end, list,
                layoutWidth);
        }
    }

    ArrayFinalize(&visualRuns);

    return isAppended;
}

static TRComposedLine *CreateStartTruncatedLine(TextSourceRef source, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRUInteger truncatedStart = BreakResolverSuggestBackwardBreak(source, tokenlessWidth,
        start, end, breakMode);

    if (truncatedStart > start) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        RunListInitialize(&list);

        if (truncatedStart < end) {
            isAppended = AppendRunsAroundSkip(source, start, end, start, truncatedStart, &list,
                &leadingIndex, &trailingIndex);

            tokenInsertIndex = (trailingIndex == TRInvalidIndex ? 0 : trailingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(source, start, end, &list,
                GetBaseLevel(source, truncatedStart)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(source, start, end, 0.0f);
    }

    return truncatedLine;
}

static TRComposedLine *CreateMiddleTruncatedLine(TextSourceRef source, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRFloat halfWidth = tokenlessWidth / 2.0f;
    TRUInteger firstMidEnd = BreakResolverSuggestForwardBreak(source, halfWidth, start, end,
        breakMode);
    TRUInteger secondMidStart = BreakResolverSuggestBackwardBreak(source, halfWidth, start, end,
        breakMode);

    if (firstMidEnd < secondMidStart) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        /* The whitespaces that are inside are excluded, as the token replaces them. */
        firstMidEnd = TextSourceGetTrailingWhitespaceStart(source, start, firstMidEnd);
        secondMidStart = TextSourceGetLeadingWhitespaceEnd(source, secondMidStart, end);

        RunListInitialize(&list);

        if (start < firstMidEnd || secondMidStart < end) {
            isAppended = AppendRunsAroundSkip(source, start, end, firstMidEnd, secondMidStart,
                &list, &leadingIndex, &trailingIndex);

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(source, start, end, &list,
                GetBaseLevel(source, start)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(source, start, end, 0.0f);
    }

    return truncatedLine;
}

static TRComposedLine *CreateEndTruncatedLine(TextSourceRef source, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRUInteger truncatedEnd = BreakResolverSuggestForwardBreak(source, tokenlessWidth, start,
        end, breakMode);

    if (truncatedEnd < end) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        /* The trailing whitespaces are excluded, as the token replaces them. */
        truncatedEnd = TextSourceGetTrailingWhitespaceStart(source, start, truncatedEnd);

        RunListInitialize(&list);

        if (start < truncatedEnd) {
            isAppended = AppendRunsAroundSkip(source, start, end, truncatedEnd, end, &list,
                &leadingIndex, &trailingIndex);

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(source, start, end, &list,
                GetBaseLevel(source, start)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(source, start, end, 0.0f);
    }

    return truncatedLine;
}

/* Counts the code units of the inner whitespaces of a range. */
static TRUInteger ComputeSpaceCount(TextSourceRef source, TRUInteger start, TRUInteger end)
{
    TRUInteger spaceCount = 0;
    TRUInteger index = start;

    while (index < end) {
        TRUInteger spaceStart = TextSourceFindWhitespace(source, index, end);
        TRUInteger spaceEnd = TextSourceGetLeadingWhitespaceEnd(source, spaceStart,
            end);

        spaceCount += spaceEnd - spaceStart;
        index = spaceEnd + 1;
    }

    return spaceCount;
}

/* Adds the share of each inner whitespace to the advances of the glyphs that show it. */
static void AddSpaceAdvances(TextSourceRef source, TRGlyphRunRef glyphRun,
    TRUInteger runStart, TRUInteger runEnd, TRFloat spaceAddition, TRFloat *advances)
{
    TRUInteger index = runStart;

    while (index < runEnd) {
        TRUInteger spaceStart = TextSourceFindWhitespace(source, index, runEnd);
        TRUInteger spaceEnd = TextSourceGetLeadingWhitespaceEnd(source, spaceStart,
            runEnd);

        index = spaceEnd;

        if (spaceStart != spaceEnd) {
            TRUInteger glyphStart, glyphEnd, glyphIndex;
            TRFloat distribution;
            TRFloat advanceAddition;

            TextRunGetGlyphRange(glyphRun->textRun, spaceStart - glyphRun->paragraphStart,
                spaceEnd - glyphRun->paragraphStart, &glyphStart, &glyphEnd);

            /* The glyphs of the run start from the first glyph of its range. */
            glyphStart -= glyphRun->glyphStart;
            glyphEnd -= glyphRun->glyphStart;

            distribution = (TRFloat)(spaceEnd - spaceStart) / (TRFloat)(glyphEnd - glyphStart);
            advanceAddition = spaceAddition * distribution;

            for (glyphIndex = glyphStart; glyphIndex < glyphEnd; glyphIndex++) {
                advances[glyphIndex] += advanceAddition;
            }
        }
    }
}

/* Replaces the run at the index of the list with its justified version. */
static TRBoolean JustifyRun(TextSourceRef source, RunList *list, TRUInteger runIndex,
    TRUInteger wordStart, TRUInteger wordEnd, TRFloat spaceAddition)
{
    TRBoolean isJustified = TRTrue;
    TRGlyphRun **runItem = ArrayGetItem(&list->runs, runIndex);
    TRGlyphRunRef glyphRun = *runItem;

    /* There is nothing to add to a replacement, or to a run without any space in it. */
    if (glyphRun->textRun->kind != TextRunKindReplacement && glyphRun->glyphCount > 0) {
        TRFloat *advances;

        advances = AllocatorAllocateBlock(sizeof(TRFloat) * glyphRun->glyphCount);

        if (advances) {
            TRUInteger runStart = NumberMax(wordStart, glyphRun->codeUnitStart);
            TRUInteger runEnd = NumberMin(wordEnd, glyphRun->codeUnitEnd);
            TRGlyphRun *justifiedRun;

            memcpy(advances, TRGlyphRunGetAdvances(glyphRun),
                sizeof(TRFloat) * glyphRun->glyphCount);

            AddSpaceAdvances(source, glyphRun, runStart, runEnd, spaceAddition, advances);

            justifiedRun = TRGlyphRunCreateJustified(glyphRun, advances);
            AllocatorDeallocateBlock(advances);

            if (justifiedRun) {
                TRGlyphRunRelease(glyphRun);
                *runItem = justifiedRun;
            } else {
                isJustified = TRFalse;
            }
        } else {
            isJustified = TRFalse;
        }
    }

    return isJustified;
}

TR_INTERNAL void RunListInitialize(RunList *list)
{
    ArrayInitialize(&list->runs, sizeof(TRGlyphRun *));
    list->hasFailed = TRFalse;
}

TR_INTERNAL void RunListFinalize(RunList *list)
{
    TRUInteger count = ArrayGetCount(&list->runs);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TRGlyphRun **runItem = ArrayGetItem(&list->runs, index);

        TRGlyphRunRelease(*runItem);
    }

    ArrayFinalize(&list->runs);
    list->hasFailed = TRFalse;
}

TR_INTERNAL void RunListInsert(RunList *list, TRUInteger index, TRGlyphRun *glyphRun)
{
    if (!glyphRun) {
        list->hasFailed = TRTrue;
    } else if (!ArrayInsert(&list->runs, index, &glyphRun)) {
        TRGlyphRunRelease(glyphRun);
        list->hasFailed = TRTrue;
    }
}

TR_INTERNAL void LineResolverAppendVisualRuns(TextSourceRef source, TRUInteger start,
    TRUInteger end, RunList *list, TRFloat layoutWidth)
{
    TRUInteger insertIndex = ArrayGetCount(&list->runs);
    TRUInteger visualStart = start;
    TRBoolean hasPrevious = TRFalse;
    TRUInt8 previousLevel = 0;

    /*
     * ASSUMPTIONS:
     *      - The range may fall in one or more text runs.
     *      - Consecutive text runs may have the same bidirectional level.
     */
    while (visualStart < end && !list->hasFailed) {
        RunCursor run;

        if (!TextSourceFindRun(source, visualStart, &run)) {
            list->hasFailed = TRTrue;
        } else {
            TRUInteger feasibleStart;
            TRUInteger feasibleEnd;
            TRUInt8 bidiLevel;
            TRBoolean isForwardRun;

            feasibleStart = NumberMax(run.start, visualStart);
            feasibleEnd = NumberMin(run.end, end);
            bidiLevel = run.textRun->bidiLevel;
            isForwardRun = (bidiLevel & 1) == 0;

            if (hasPrevious && (bidiLevel != previousLevel || isForwardRun)) {
                insertIndex = ArrayGetCount(&list->runs);
            }

            if (run.textRun->kind == TextRunKindReplacement) {
                insertIndex = AppendReplacementRun(source, list, &run, insertIndex,
                    feasibleStart, feasibleEnd, isForwardRun, layoutWidth);
            } else {
                insertIndex = AppendPaintedRuns(source, list, &run, insertIndex,
                    feasibleStart, feasibleEnd, isForwardRun);
            }

            hasPrevious = TRTrue;
            previousLevel = bidiLevel;
            visualStart = feasibleEnd;
        }
    }
}

TR_INTERNAL TRComposedLine *LineResolverCreateLine(TextSourceRef source, TRUInteger start,
    TRUInteger end, RunList *list, TRUInt8 paragraphLevel)
{
    TRComposedLine *line = NULL;

    if (list->hasFailed) {
        RunListFinalize(list);
    } else {
        TRGlyphRun **runs = ArrayGetItems(&list->runs);
        TRUInteger runCount = ArrayGetCount(&list->runs);

        line = TRComposedLineCreate(start, end,
            TextSourceGetTrailingWhitespaceStart(source, start, end), runs, runCount,
            paragraphLevel);

        /* The line took the references of the runs, and the memory of the list is still the
         * list's. */
        ArrayResize(&list->runs, 0);
        RunListFinalize(list);
    }

    return line;
}

TR_INTERNAL TRComposedLine *LineResolverCreateSimpleLine(TextSourceRef source,
    TRUInteger start, TRUInteger end, TRFloat layoutWidth)
{
    TRComposedLine *simpleLine = NULL;
    RunList list;

    RunListInitialize(&list);

    if (AppendRunsOfRange(source, start, end, &list, layoutWidth)) {
        simpleLine = LineResolverCreateLine(source, start, end, &list,
            GetBaseLevel(source, start));
    } else {
        RunListFinalize(&list);
    }

    return simpleLine;
}

TR_INTERNAL TRComposedLine *LineResolverCreateTruncatedLine(TextSourceRef source,
    TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, TRComposedLineRef tokenLine)
{
    TRComposedLine *truncatedLine = NULL;
    TRFloat tokenlessWidth = extent - tokenLine->extent;

    switch (truncationPlace) {
    case TRTruncationPlaceStart:
        truncatedLine = CreateStartTruncatedLine(source, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;

    case TRTruncationPlaceMiddle:
        truncatedLine = CreateMiddleTruncatedLine(source, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;

    case TRTruncationPlaceEnd:
        truncatedLine = CreateEndTruncatedLine(source, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;
    }

    return truncatedLine;
}

TR_INTERNAL TRComposedLine *LineResolverCreateJustifiedLine(TextSourceRef source,
    TRUInteger start, TRUInteger end, TRFloat justificationFactor, TRFloat justificationExtent)
{
    TRComposedLine *justifiedLine = NULL;
    TRUInteger wordStart = TextSourceGetLeadingWhitespaceEnd(source, start, end);
    TRUInteger wordEnd = TextSourceGetTrailingWhitespaceStart(source, start, end);
    TRFloat actualWidth = TextSourceMeasureRange(source, start, wordEnd);
    TRFloat extraWidth = justificationExtent - actualWidth;
    TRFloat availableWidth = extraWidth * justificationFactor;
    TRUInteger innerSpaceCount = ComputeSpaceCount(source, wordStart, wordEnd);
    TRBoolean isJustified = TRFalse;
    RunList list;

    RunListInitialize(&list);

    if (AppendRunsOfRange(source, start, end, &list, 0.0f) && !list.hasFailed) {
        TRFloat spaceAddition = (innerSpaceCount > 0
                                 ? availableWidth / (TRFloat)innerSpaceCount : 0.0f);
        TRUInteger runIndex;

        isJustified = TRTrue;

        for (runIndex = 0; isJustified && runIndex < ArrayGetCount(&list.runs); runIndex++) {
            isJustified = JustifyRun(source, &list, runIndex, wordStart, wordEnd,
                spaceAddition);
        }
    }

    if (isJustified) {
        justifiedLine = LineResolverCreateLine(source, start, end, &list,
            GetBaseLevel(source, start));
    } else {
        RunListFinalize(&list);
    }

    return justifiedLine;
}
