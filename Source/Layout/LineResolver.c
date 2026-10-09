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

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRSheenBidi.h>
#include <Tehreer/TRText.h>

#include <API/TRAssert.h>
#include <API/TRAttributeList.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRGlyphRun.h>
#include <API/TRTypesetter.h>
#include <Core/Allocator.h>
#include <Core/Array.h>
#include <Layout/BreakResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>

#include "LineResolver.h"

static TRBoolean EnsureRunListCapacity(RunList *list)
{
    TRBoolean hasCapacity = TRTrue;

    if (list->count == list->capacity) {
        TRUInteger newCapacity;
        TRGlyphRun **newItems;

        newCapacity = (list->capacity == 0 ? 8 : list->capacity * 2);
        newItems = AllocatorReallocateBlock(list->items, newCapacity * sizeof(TRGlyphRun *));

        if (newItems) {
            list->items = newItems;
            list->capacity = newCapacity;
        } else {
            hasCapacity = TRFalse;
        }
    }

    return hasCapacity;
}

/* A part of a line in the order that it is shown, with the bidirectional level of its text. */
typedef struct _VisualRun {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 level;
} VisualRun;

/* Adds the visual runs of a range that is inside a paragraph. */
static TRBoolean AddVisualRuns(SBTextRef sbText, TRUInteger start, TRUInteger end,
    Array *visualRuns)
{
    SBVisualRunIteratorRef iterator = SBTextCreateVisualRunIterator(sbText, start, end - start);
    TRBoolean isAdded = TRFalse;

    if (iterator) {
        isAdded = TRTrue;

        while (isAdded && SBVisualRunIteratorMoveNext(iterator)) {
            const SBVisualRun *sbRun = SBVisualRunIteratorGetCurrent(iterator);
            VisualRun visualRun;

            visualRun.start = sbRun->index;
            visualRun.end = sbRun->index + sbRun->length;
            visualRun.level = sbRun->level;

            isAdded = ArrayAppend(visualRuns, &visualRun);
        }

        SBVisualRunIteratorRelease(iterator);
    }

    return isAdded;
}

/*
 * Finds the visual runs of a range in the order that they are shown, which is not the order of the
 * text where it has mixed directions. A range that spans paragraphs shows them one after another in
 * the direction of the first of them. The range MUST NOT be empty. Returns TRFalse if the runs
 * could not be found.
 */
static TRBoolean FindVisualRuns(TRTypesetterRef typesetter, TRUInteger start, TRUInteger end,
    Array *visualRuns)
{
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    TRBoolean isFound = TRFalse;
    TRBoolean hasMore = TRTrue;
    ParagraphInfo paragraph;
    TRBoolean isRTL;

    TRTypesetterGetParagraph(typesetter, start, &paragraph);
    isRTL = (paragraph.baseLevel & 1) == 1;

    /* The paragraphs of a right-to-left line are shown from the last one to the first. */
    if (isRTL && paragraph.end < end) {
        TRTypesetterGetParagraph(typesetter, end - 1, &paragraph);
    }

    while (hasMore) {
        TRUInteger feasibleStart = NumberMax(paragraph.start, start);
        TRUInteger feasibleEnd = NumberMin(paragraph.end, end);

        isFound = AddVisualRuns(sbText, feasibleStart, feasibleEnd, visualRuns);

        if (isRTL) {
            hasMore = (isFound && feasibleStart != start);

            if (hasMore) {
                TRTypesetterGetParagraph(typesetter, feasibleStart - 1, &paragraph);
            }
        } else {
            hasMore = (isFound && feasibleEnd != end);

            if (hasMore) {
                TRTypesetterGetParagraph(typesetter, feasibleEnd, &paragraph);
            }
        }
    }

    return isFound;
}

/* Gets what the text is painted with from the attributes at an index. */
static TRUInteger GetPaint(TRTypesetterRef typesetter, TRUInteger index, GlyphRunPaint *paint)
{
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    TRUInteger length = 0;
    SBAttributeListRef sbAttributes;
    TRAttributeListRef attributes;

    paint->hasForegroundColor = TRFalse;
    paint->foregroundColor = 0;
    paint->userData = NULL;

    sbAttributes = SBTextGetAttributes(sbText,
        SBAttributeFilterMakeCollection(SBAttributeGroupNone, SBAttributeScopeCharacter),
        index, &length);
    attributes = TRAttributeListMake(sbAttributes);

    if (attributes) {
        TRUInteger count = TRAttributeListGetCount(attributes);
        TRUInteger itemIndex;

        for (itemIndex = 0; itemIndex < count; itemIndex++) {
            const TRAttribute *item = TRAttributeListGetItem(attributes, itemIndex);

            if (item->type == TRAttributeForegroundColor) {
                paint->hasForegroundColor = TRTrue;
                paint->foregroundColor = item->value.foregroundColor;
            } else if (item->type == TRAttributeUserData) {
                paint->userData = item->value.userData;
            }
        }

        TRAttributeListRelease(attributes);
    }

    return length;
}

/* A replacement is taken as a whole, and its room is decided by the layout width. */
static TRUInteger AppendReplacementRun(TRTypesetterRef typesetter, RunList *list,
    TextRunRef textRun, TRUInteger insertIndex, TRUInteger start, TRUInteger end,
    TRBoolean isForwardRun, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    GlyphRunPaint paint;
    TextRunRef sizedRun;

    GetPaint(typesetter, start, &paint);
    sizedRun = (hasLayoutWidth ? TextRunCreateForLayoutWidth(textRun, layoutWidth)
                               : TextRunRetain(textRun));

    if (sizedRun) {
        RunListInsert(list, insertIndex, TRGlyphRunCreate(sizedRun, start, end, &paint));
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
static TRUInteger AppendPaintedRuns(TRTypesetterRef typesetter, RunList *list, TextRunRef textRun,
    TRUInteger insertIndex, TRUInteger start, TRUInteger end, TRBoolean isForwardRun)
{
    TRUInteger spanStart = start;

    while (spanStart < end) {
        GlyphRunPaint paint;
        TRUInteger spanLength;
        TRUInteger spanEnd;

        spanLength = GetPaint(typesetter, spanStart, &paint);
        spanEnd = spanStart + spanLength;

        if (spanLength == 0 || spanEnd > end) {
            spanEnd = end;
        }

        RunListInsert(list, insertIndex, TRGlyphRunCreate(textRun, spanStart, spanEnd, &paint));

        if (isForwardRun) {
            insertIndex += 1;
        }

        spanStart = spanEnd;
    }

    return insertIndex;
}

/* The part of a range that is skipped by a truncation, and where the token goes. */
typedef struct _TruncationHandler {
    TRTypesetterRef typesetter;
    RunList *list;
    TRUInteger skipStart;
    TRUInteger skipEnd;
    TRUInteger leadingTokenIndex;
    TRUInteger trailingTokenIndex;
} TruncationHandler;

/* Handles the first part of the characters, which comes before the skipped ones. */
static void AppendRunBeforeSkip(TruncationHandler *handler, const VisualRun *visualRun)
{
    TRUInteger visualEnd = visualRun->end;

    LineResolverAppendVisualRuns(handler->typesetter, visualRun->start,
        (visualEnd < handler->skipStart ? visualEnd : handler->skipStart), handler->list, TRFalse,
        0.0f);
}

/* Handles the second part of the characters, which comes after the skipped ones. */
static void AppendRunAfterSkip(TruncationHandler *handler, const VisualRun *visualRun)
{
    TRUInteger visualStart = visualRun->start;

    LineResolverAppendVisualRuns(handler->typesetter,
        (visualStart > handler->skipEnd ? visualStart : handler->skipEnd), visualRun->end,
        handler->list, TRFalse, 0.0f);
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
                handler->trailingTokenIndex = list->count;
            }
        }

        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = list->count;
            }

            AppendRunBeforeSkip(handler, visualRun);
        }
    } else {
        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            AppendRunBeforeSkip(handler, visualRun);

            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = list->count;
            }
        }

        /* Handle the second part of the characters. */
        if (visualEnd >= handler->skipEnd) {
            if (visualStart < handler->skipEnd) {
                handler->trailingTokenIndex = list->count;
            }

            AppendRunAfterSkip(handler, visualRun);
        }
    }
}

/* Appends all runs of the range except the skipped ones, and finds where the token goes. */
static TRBoolean AppendRunsAroundSkip(TRTypesetterRef typesetter, TRUInteger start, TRUInteger end,
    TRUInteger skipStart, TRUInteger skipEnd, RunList *list, TRUInteger *leadingTokenIndex,
    TRUInteger *trailingTokenIndex)
{
    TruncationHandler handler;
    Array visualRuns;
    TRBoolean isFound;

    handler.typesetter = typesetter;
    handler.list = list;
    handler.skipStart = skipStart;
    handler.skipEnd = skipEnd;
    handler.leadingTokenIndex = TRInvalidIndex;
    handler.trailingTokenIndex = TRInvalidIndex;

    ArrayInitialize(&visualRuns, sizeof(VisualRun));
    isFound = FindVisualRuns(typesetter, start, end, &visualRuns);

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
        RunListInsert(list, index + tokenIndex, TRGlyphRunCreateCopy(token->runs[tokenIndex]));
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

static TRUInt8 GetBaseLevel(TRTypesetterRef typesetter, TRUInteger index)
{
    ParagraphInfo paragraph;

    TRTypesetterGetParagraph(typesetter, index, &paragraph);

    return paragraph.baseLevel;
}

/* Appends the glyph runs of a range, in the order of the visual runs that it has. */
static TRBoolean AppendRunsOfRange(TRTypesetterRef typesetter, TRUInteger start, TRUInteger end,
    RunList *list, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    Array visualRuns;
    TRBoolean isAppended;

    ArrayInitialize(&visualRuns, sizeof(VisualRun));
    isAppended = FindVisualRuns(typesetter, start, end, &visualRuns);

    if (isAppended) {
        TRUInteger count = ArrayGetCount(&visualRuns);
        TRUInteger index;

        for (index = 0; index < count; index++) {
            const VisualRun *visualRun = ArrayGetItem(&visualRuns, index);

            LineResolverAppendVisualRuns(typesetter, visualRun->start, visualRun->end, list,
                hasLayoutWidth, layoutWidth);
        }
    }

    ArrayFinalize(&visualRuns);

    return isAppended;
}

static TRComposedLine *CreateStartTruncatedLine(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRUInteger truncatedStart = BreakResolverSuggestBackwardBreak(typesetter, tokenlessWidth,
        start, end, breakMode);

    if (truncatedStart > start) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        RunListInitialize(&list);

        if (truncatedStart < end) {
            isAppended = AppendRunsAroundSkip(typesetter, start, end, start, truncatedStart, &list,
                &leadingIndex, &trailingIndex);

            tokenInsertIndex = (trailingIndex == TRInvalidIndex ? 0 : trailingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(typesetter, truncatedStart, end, &list,
                GetBaseLevel(typesetter, truncatedStart)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(typesetter, truncatedStart, end, TRFalse, 0.0f);
    }

    return truncatedLine;
}

static TRComposedLine *CreateMiddleTruncatedLine(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRFloat halfWidth = tokenlessWidth / 2.0f;
    TRUInteger firstMidEnd = BreakResolverSuggestForwardBreak(typesetter, halfWidth, start, end,
        breakMode);
    TRUInteger secondMidStart = BreakResolverSuggestBackwardBreak(typesetter, halfWidth, start, end,
        breakMode);

    if (firstMidEnd < secondMidStart) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        /* The whitespaces that are inside are excluded, as the token replaces them. */
        firstMidEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, firstMidEnd);
        secondMidStart = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, secondMidStart, end);

        RunListInitialize(&list);

        if (start < firstMidEnd || secondMidStart < end) {
            isAppended = AppendRunsAroundSkip(typesetter, start, end, firstMidEnd, secondMidStart,
                &list, &leadingIndex, &trailingIndex);

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(typesetter, start, end, &list,
                GetBaseLevel(typesetter, start)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(typesetter, start, end, TRFalse, 0.0f);
    }

    return truncatedLine;
}

static TRComposedLine *CreateEndTruncatedLine(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, TRComposedLineRef token)
{
    TRComposedLine *truncatedLine = NULL;
    TRUInteger truncatedEnd = BreakResolverSuggestForwardBreak(typesetter, tokenlessWidth, start,
        end, breakMode);

    if (truncatedEnd < end) {
        TRUInteger tokenInsertIndex = 0;
        TRBoolean isAppended = TRTrue;
        RunList list;
        TRUInteger leadingIndex, trailingIndex;

        /* The trailing whitespaces are excluded, as the token replaces them. */
        truncatedEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, truncatedEnd);

        RunListInitialize(&list);

        if (start < truncatedEnd) {
            isAppended = AppendRunsAroundSkip(typesetter, start, end, truncatedEnd, end, &list,
                &leadingIndex, &trailingIndex);

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        if (isAppended) {
            AppendTokenRuns(token, &list, tokenInsertIndex);

            truncatedLine = MarkTruncated(LineResolverCreateLine(typesetter, start, truncatedEnd, &list,
                GetBaseLevel(typesetter, start)));
        } else {
            RunListFinalize(&list);
        }
    } else {
        truncatedLine = LineResolverCreateSimpleLine(typesetter, start, truncatedEnd, TRFalse, 0.0f);
    }

    return truncatedLine;
}

/* Counts the code units of the inner whitespaces of a range. */
static TRUInteger ComputeSpaceCount(TRTypesetterRef typesetter, TRUInteger start, TRUInteger end)
{
    TRUInteger spaceCount = 0;
    TRUInteger index = start;

    while (index < end) {
        TRUInteger spaceStart = TextBufferFindWhitespace(&typesetter->buffer, index, end);
        TRUInteger spaceEnd = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, spaceStart,
            end);

        spaceCount += spaceEnd - spaceStart;
        index = spaceEnd + 1;
    }

    return spaceCount;
}

/* Adds the share of each inner whitespace to the advances of the glyphs that show it. */
static void AddSpaceAdvances(TRTypesetterRef typesetter, TRGlyphRunRef glyphRun,
    TRUInteger runStart, TRUInteger runEnd, TRFloat spaceAddition, TRFloat *advances)
{
    TRUInteger index = runStart;

    while (index < runEnd) {
        TRUInteger spaceStart = TextBufferFindWhitespace(&typesetter->buffer, index, runEnd);
        TRUInteger spaceEnd = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, spaceStart,
            runEnd);

        index = spaceEnd;

        if (spaceStart != spaceEnd) {
            TRUInteger glyphStart, glyphEnd, glyphIndex;
            TRFloat distribution;
            TRFloat advanceAddition;

            TextRunGetGlyphRange(glyphRun->textRun, spaceStart, spaceEnd, &glyphStart, &glyphEnd);

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
static TRBoolean JustifyRun(TRTypesetterRef typesetter, RunList *list, TRUInteger runIndex,
    TRUInteger wordStart, TRUInteger wordEnd, TRFloat spaceAddition)
{
    TRBoolean isJustified = TRTrue;
    TRGlyphRunRef glyphRun = list->items[runIndex];

    /* There is nothing to add to a replacement, or to a run without any space in it. */
    if (glyphRun->textRun->kind != TextRunKindReplacement && glyphRun->glyphCount > 0) {
        TRFloat *advances;

        advances = AllocatorAllocateBlock(sizeof(TRFloat) * glyphRun->glyphCount);

        if (advances) {
            TRUInteger runStart = (wordStart > glyphRun->codeUnitStart
                                   ? wordStart : glyphRun->codeUnitStart);
            TRUInteger runEnd = (wordEnd < glyphRun->codeUnitEnd
                                 ? wordEnd : glyphRun->codeUnitEnd);
            TRGlyphRun *justifiedRun;

            memcpy(advances, TRGlyphRunGetAdvances(glyphRun),
                sizeof(TRFloat) * glyphRun->glyphCount);

            AddSpaceAdvances(typesetter, glyphRun, runStart, runEnd, spaceAddition, advances);

            justifiedRun = TRGlyphRunCreateJustified(glyphRun, advances);
            AllocatorDeallocateBlock(advances);

            if (justifiedRun) {
                TRGlyphRunRelease(glyphRun);
                list->items[runIndex] = justifiedRun;
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
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->hasFailed = TRFalse;
}

TR_INTERNAL void RunListFinalize(RunList *list)
{
    TRUInteger index;

    for (index = 0; index < list->count; index++) {
        TRGlyphRunRelease(list->items[index]);
    }

    AllocatorDeallocateBlock(list->items);
    RunListInitialize(list);
}

TR_INTERNAL void RunListInsert(RunList *list, TRUInteger index, TRGlyphRun *glyphRun)
{
    /* The index MUST NOT be greater than the count. */
    TRAssert(index <= list->count);

    if (!glyphRun) {
        list->hasFailed = TRTrue;
    } else if (EnsureRunListCapacity(list)) {
        memmove(&list->items[index + 1], &list->items[index],
            (list->count - index) * sizeof(TRGlyphRun *));
        list->items[index] = glyphRun;
        list->count += 1;
    } else {
        TRGlyphRunRelease(glyphRun);
        list->hasFailed = TRTrue;
    }
}

TR_INTERNAL void LineResolverAppendVisualRuns(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    TRUInteger insertIndex = list->count;
    TRUInteger visualStart = start;
    TRBoolean hasPrevious = TRFalse;
    TRUInt8 previousLevel = 0;

    /*
     * ASSUMPTIONS:
     *      - The range may fall in one or more text runs.
     *      - Consecutive text runs may have the same bidirectional level.
     */
    while (visualStart < end && !list->hasFailed) {
        TRUInteger runIndex = TRTypesetterFindRun(typesetter, visualStart);

        if (runIndex == TRInvalidIndex) {
            list->hasFailed = TRTrue;
        } else {
            TextRunRef textRun;
            TRUInteger feasibleStart;
            TRUInteger feasibleEnd;
            TRUInt8 bidiLevel;
            TRBoolean isForwardRun;

            textRun = TRTypesetterGetRun(typesetter, runIndex);
            feasibleStart = (textRun->codeUnitStart > visualStart
                             ? textRun->codeUnitStart : visualStart);
            feasibleEnd = (textRun->codeUnitEnd < end ? textRun->codeUnitEnd : end);
            bidiLevel = textRun->bidiLevel;
            isForwardRun = (bidiLevel & 1) == 0;

            if (hasPrevious && (bidiLevel != previousLevel || isForwardRun)) {
                insertIndex = list->count;
            }

            if (textRun->kind == TextRunKindReplacement) {
                insertIndex = AppendReplacementRun(typesetter, list, textRun, insertIndex,
                    feasibleStart, feasibleEnd, isForwardRun, hasLayoutWidth, layoutWidth);
            } else {
                insertIndex = AppendPaintedRuns(typesetter, list, textRun, insertIndex,
                    feasibleStart, feasibleEnd, isForwardRun);
            }

            hasPrevious = TRTrue;
            previousLevel = bidiLevel;
            visualStart = feasibleEnd;
        }
    }
}

TR_INTERNAL TRComposedLine *LineResolverCreateLine(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRUInt8 paragraphLevel)
{
    TRComposedLine *line = NULL;

    if (list->hasFailed) {
        RunListFinalize(list);
    } else {
        line = TRComposedLineCreate(&typesetter->buffer, start, end, list->items, list->count,
            paragraphLevel);

        /* The line took the references of the runs, and the memory of the list is still the list's. */
        list->count = 0;
        RunListFinalize(list);
    }

    return line;
}

TR_INTERNAL TRComposedLine *LineResolverCreateSimpleLine(TRTypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    TRComposedLine *simpleLine = NULL;
    RunList list;

    RunListInitialize(&list);

    if (AppendRunsOfRange(typesetter, start, end, &list, hasLayoutWidth, layoutWidth)) {
        simpleLine = LineResolverCreateLine(typesetter, start, end, &list,
            GetBaseLevel(typesetter, start));
    } else {
        RunListFinalize(&list);
    }

    return simpleLine;
}

TR_INTERNAL TRComposedLine *LineResolverCreateTruncatedLine(TRTypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, TRComposedLineRef tokenLine)
{
    TRComposedLine *truncatedLine = NULL;
    TRFloat tokenlessWidth = extent - tokenLine->extent;

    switch (truncationPlace) {
    case TRTruncationPlaceStart:
        truncatedLine = CreateStartTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;

    case TRTruncationPlaceMiddle:
        truncatedLine = CreateMiddleTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;

    case TRTruncationPlaceEnd:
        truncatedLine = CreateEndTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);
        break;
    }

    return truncatedLine;
}

TR_INTERNAL TRComposedLine *LineResolverCreateJustifiedLine(TRTypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat justificationFactor, TRFloat justificationExtent)
{
    TRComposedLine *justifiedLine = NULL;
    TRUInteger wordStart = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, start, end);
    TRUInteger wordEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, end);
    TRFloat actualWidth = TRTypesetterMeasureRange(typesetter, start, wordEnd);
    TRFloat extraWidth = justificationExtent - actualWidth;
    TRFloat availableWidth = extraWidth * justificationFactor;
    TRUInteger innerSpaceCount = ComputeSpaceCount(typesetter, wordStart, wordEnd);
    TRBoolean isJustified = TRFalse;
    RunList list;

    RunListInitialize(&list);

    if (AppendRunsOfRange(typesetter, start, end, &list, TRFalse, 0.0f) && !list.hasFailed) {
        TRFloat spaceAddition = (innerSpaceCount > 0
                                 ? availableWidth / (TRFloat)innerSpaceCount : 0.0f);
        TRUInteger runIndex;

        isJustified = TRTrue;

        for (runIndex = 0; isJustified && runIndex < list.count; runIndex++) {
            isJustified = JustifyRun(typesetter, &list, runIndex, wordStart, wordEnd,
                spaceAddition);
        }
    }

    if (isJustified) {
        justifiedLine = LineResolverCreateLine(typesetter, start, end, &list,
            GetBaseLevel(typesetter, start));
    } else {
        RunListFinalize(&list);
    }

    return justifiedLine;
}
