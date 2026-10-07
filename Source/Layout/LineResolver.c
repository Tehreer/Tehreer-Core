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
#include <Tehreer/TRText.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRGlyphRun.h>
#include <API/TRTypesetter.h>
#include <Core/Allocator.h>
#include <Layout/BreakResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>

#include "LineResolver.h"

static TRBoolean EnsureRunListCapacity(RunList *list)
{
    TRBoolean hasCapacity = TRTrue;

    if (list->count == list->capacity) {
        TRUInteger newCapacity;
        GlyphRunRef *newItems;

        newCapacity = (list->capacity == 0 ? 8 : list->capacity * 2);
        newItems = AllocatorReallocateBlock(list->items, newCapacity * sizeof(GlyphRunRef));

        if (newItems) {
            list->items = newItems;
            list->capacity = newCapacity;
        } else {
            hasCapacity = TRFalse;
        }
    }

    return hasCapacity;
}

/* Goes through the visual runs of a range that is inside a paragraph. */
static TRBoolean ForEachVisualRunInRange(SBTextRef sbText, TRUInteger start, TRUInteger end,
    VisualRunFunc func, void *context)
{
    TRBoolean isEnumerated = TRFalse;
    SBVisualRunIteratorRef iterator;

    iterator = SBTextCreateVisualRunIterator(sbText, start, end - start);

    if (iterator) {
        while (SBVisualRunIteratorMoveNext(iterator)) {
            const SBVisualRun *sbRun = SBVisualRunIteratorGetCurrent(iterator);
            VisualRun visualRun;

            visualRun.start = sbRun->index;
            visualRun.end = sbRun->index + sbRun->length;
            visualRun.level = sbRun->level;

            func(context, &visualRun);
        }

        SBVisualRunIteratorRelease(iterator);

        isEnumerated = TRTrue;
    }

    return isEnumerated;
}

/* Moves to the paragraph that is shown next, and tells if there is one to move to. */
static TRBoolean MoveToNextParagraph(TRUInteger *paragraphIndex, TRBoolean isRTL)
{
    TRBoolean hasNext = TRTrue;

    if (!isRTL) {
        *paragraphIndex += 1;
    } else if (*paragraphIndex > 0) {
        *paragraphIndex -= 1;
    } else {
        hasNext = TRFalse;
    }

    return hasNext;
}

/* Gets what the text is painted with from the attributes at an index. */
static TRUInteger GetPaint(TypesetterRef typesetter, TRUInteger index, GlyphRunPaint *paint)
{
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    TRUInteger length = 0;
    SBAttributeListRef attributes;

    paint->hasForegroundColor = TRFalse;
    paint->foregroundColor = 0;
    paint->userData = NULL;

    attributes = SBTextGetAttributes(sbText,
        SBAttributeFilterMakeCollection(SBAttributeGroupNone, SBAttributeScopeCharacter),
        index, &length);

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

        SBAttributeListRelease(attributes);
    }

    return length;
}

/* A replacement is taken as a whole, and its room is decided by the layout width. */
static TRUInteger AppendReplacementRun(TypesetterRef typesetter, RunList *list,
    TextRunRef textRun, TRUInteger insertIndex, TRUInteger start, TRUInteger end,
    TRBoolean isForwardRun, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    GlyphRunPaint paint;
    TextRunRef sizedRun;

    GetPaint(typesetter, start, &paint);
    sizedRun = (hasLayoutWidth ? TextRunCreateForLayoutWidth(textRun, layoutWidth)
                               : TextRunRetain(textRun));

    if (sizedRun) {
        RunListInsert(list, insertIndex, GlyphRunCreate(sizedRun, start, end, &paint));
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
static TRUInteger AppendPaintedRuns(TypesetterRef typesetter, RunList *list, TextRunRef textRun,
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

        RunListInsert(list, insertIndex, GlyphRunCreate(textRun, spanStart, spanEnd, &paint));

        if (isForwardRun) {
            insertIndex += 1;
        }

        spanStart = spanEnd;
    }

    return insertIndex;
}

typedef struct _SimpleLineContext {
    TypesetterRef typesetter;
    RunList *list;
    TRBoolean hasLayoutWidth;
    TRFloat layoutWidth;
} SimpleLineContext;

static void AppendRunOfSimpleLine(void *context, const VisualRun *visualRun)
{
    SimpleLineContext *simple = context;

    LineResolverAppendVisualRuns(simple->typesetter, visualRun->start, visualRun->end,
        simple->list, simple->hasLayoutWidth, simple->layoutWidth);
}

/* The part of a range that is skipped by a truncation, and where the token goes. */
typedef struct _TruncationHandler {
    TypesetterRef typesetter;
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

static void AppendRunOfTruncation(void *context, const VisualRun *visualRun)
{
    TruncationHandler *handler = context;
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
static TRBoolean AppendRunsAroundSkip(TypesetterRef typesetter, TRUInteger start, TRUInteger end,
    TRUInteger skipStart, TRUInteger skipEnd, RunList *list, TRUInteger *leadingTokenIndex,
    TRUInteger *trailingTokenIndex)
{
    TruncationHandler handler;
    TRBoolean isFound;

    handler.typesetter = typesetter;
    handler.list = list;
    handler.skipStart = skipStart;
    handler.skipEnd = skipEnd;
    handler.leadingTokenIndex = TRInvalidIndex;
    handler.trailingTokenIndex = TRInvalidIndex;

    isFound = LineResolverForEachVisualRun(typesetter, start, end, AppendRunOfTruncation, &handler);

    *leadingTokenIndex = handler.leadingTokenIndex;
    *trailingTokenIndex = handler.trailingTokenIndex;

    return isFound;
}

/* Inserts copies of the runs of the token at the index, in the same order. */
static void AppendTokenRuns(ComposedLineRef token, RunList *list, TRUInteger index)
{
    TRUInteger tokenIndex;

    for (tokenIndex = 0; tokenIndex < token->runCount; tokenIndex++) {
        RunListInsert(list, index + tokenIndex, GlyphRunCreateCopy(token->runs[tokenIndex]));
    }
}

/* Marks a line that shows a token in place of some of its text. */
static ComposedLineRef MarkTruncated(ComposedLineRef line)
{
    if (line) {
        line->isTruncated = TRTrue;
    }

    return line;
}

static TRUInt8 GetBaseLevel(TypesetterRef typesetter, TRUInteger index)
{
    return typesetter->paragraphs[TypesetterFindParagraph(typesetter, index)].baseLevel;
}

static ComposedLineRef CreateStartTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    ComposedLineRef truncatedLine = NULL;
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

static ComposedLineRef CreateMiddleTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    ComposedLineRef truncatedLine = NULL;
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

static ComposedLineRef CreateEndTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    ComposedLineRef truncatedLine = NULL;
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
static TRUInteger ComputeSpaceCount(TypesetterRef typesetter, TRUInteger start, TRUInteger end)
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
static void AddSpaceAdvances(TypesetterRef typesetter, GlyphRunRef glyphRun, TRUInteger runStart,
    TRUInteger runEnd, TRFloat spaceAddition, TRFloat *advances)
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
static TRBoolean JustifyRun(TypesetterRef typesetter, RunList *list, TRUInteger runIndex,
    TRUInteger wordStart, TRUInteger wordEnd, TRFloat spaceAddition)
{
    TRBoolean isJustified = TRTrue;
    GlyphRunRef glyphRun = list->items[runIndex];

    /* There is nothing to add to a replacement, or to a run without any space in it. */
    if (glyphRun->textRun->kind != TextRunKindReplacement && glyphRun->glyphCount > 0) {
        TRFloat *advances;

        advances = AllocatorAllocateBlock(sizeof(TRFloat) * glyphRun->glyphCount);

        if (advances) {
            TRUInteger runStart = (wordStart > glyphRun->codeUnitStart
                                   ? wordStart : glyphRun->codeUnitStart);
            TRUInteger runEnd = (wordEnd < glyphRun->codeUnitEnd
                                 ? wordEnd : glyphRun->codeUnitEnd);
            GlyphRunRef justifiedRun;

            memcpy(advances, GlyphRunGetAdvances(glyphRun), sizeof(TRFloat) * glyphRun->glyphCount);

            AddSpaceAdvances(typesetter, glyphRun, runStart, runEnd, spaceAddition, advances);

            justifiedRun = GlyphRunCreateJustified(glyphRun, advances);
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

TR_INTERNAL void RunListInsert(RunList *list, TRUInteger index, GlyphRunRef glyphRun)
{
    /* The index MUST NOT be greater than the count. */
    TRAssert(index <= list->count);

    if (!glyphRun) {
        list->hasFailed = TRTrue;
    } else if (EnsureRunListCapacity(list)) {
        memmove(&list->items[index + 1], &list->items[index],
            (list->count - index) * sizeof(GlyphRunRef));
        list->items[index] = glyphRun;
        list->count += 1;
    } else {
        TRGlyphRunRelease(glyphRun);
        list->hasFailed = TRTrue;
    }
}

TR_INTERNAL TRBoolean LineResolverForEachVisualRun(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, VisualRunFunc func, void *context)
{
    TRBoolean isEnumerated = TRFalse;
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);

    if (paragraphIndex != TRInvalidIndex) {
        SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
        TRBoolean isRTL;
        TRUInteger feasibleStart;
        TRUInteger feasibleEnd;

        isRTL = (typesetter->paragraphs[paragraphIndex].baseLevel & 1) == 1;

        /* The paragraphs of a right-to-left line are shown from the last one to the first. */
        if (isRTL && typesetter->paragraphs[paragraphIndex].end < end) {
            paragraphIndex = TypesetterFindParagraph(typesetter, end - 1);
        }

        do {
            const ParagraphInfo *paragraph = &typesetter->paragraphs[paragraphIndex];

            feasibleStart = (paragraph->start > start ? paragraph->start : start);
            feasibleEnd = (paragraph->end < end ? paragraph->end : end);

            isEnumerated = ForEachVisualRunInRange(sbText, feasibleStart, feasibleEnd, func, context);

            if (!isEnumerated || !MoveToNextParagraph(&paragraphIndex, isRTL)) {
                break;
            }
        } while (isRTL ? feasibleStart != start : feasibleEnd != end);
    }

    return isEnumerated;
}

TR_INTERNAL void LineResolverAppendVisualRuns(TypesetterRef typesetter, TRUInteger start,
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
        TRUInteger runIndex = TypesetterFindRun(typesetter, visualStart);

        if (runIndex == TRInvalidIndex) {
            list->hasFailed = TRTrue;
        } else {
            TextRunRef textRun;
            TRUInteger feasibleStart;
            TRUInteger feasibleEnd;
            TRUInt8 bidiLevel;
            TRBoolean isForwardRun;

            textRun = typesetter->runs[runIndex];
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

TR_INTERNAL ComposedLineRef LineResolverCreateLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRUInt8 paragraphLevel)
{
    ComposedLineRef line = NULL;

    if (list->hasFailed) {
        RunListFinalize(list);
    } else {
        line = ComposedLineCreate(&typesetter->buffer, start, end, list->items, list->count,
            paragraphLevel);

        /* The line took the references of the runs, and the memory of the list is still the list's. */
        list->count = 0;
        RunListFinalize(list);
    }

    return line;
}

TR_INTERNAL ComposedLineRef LineResolverCreateSimpleLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    ComposedLineRef simpleLine = NULL;
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);
    RunList list;
    SimpleLineContext context;

    RunListInitialize(&list);

    context.typesetter = typesetter;
    context.list = &list;
    context.hasLayoutWidth = hasLayoutWidth;
    context.layoutWidth = layoutWidth;

    if (paragraphIndex != TRInvalidIndex
            && LineResolverForEachVisualRun(typesetter, start, end, AppendRunOfSimpleLine,
                &context)) {
        simpleLine = LineResolverCreateLine(typesetter, start, end, &list,
            typesetter->paragraphs[paragraphIndex].baseLevel);
    } else {
        RunListFinalize(&list);
    }

    return simpleLine;
}

TR_INTERNAL ComposedLineRef LineResolverCreateTruncatedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, ComposedLineRef tokenLine)
{
    ComposedLineRef truncatedLine = NULL;
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

TR_INTERNAL ComposedLineRef LineResolverCreateJustifiedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat justificationFactor, TRFloat justificationExtent)
{
    ComposedLineRef justifiedLine = NULL;
    TRUInteger wordStart = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, start, end);
    TRUInteger wordEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, end);
    TRFloat actualWidth = TypesetterMeasureRange(typesetter, start, wordEnd);
    TRFloat extraWidth = justificationExtent - actualWidth;
    TRFloat availableWidth = extraWidth * justificationFactor;
    TRUInteger innerSpaceCount = ComputeSpaceCount(typesetter, wordStart, wordEnd);
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);
    TRBoolean isJustified = TRFalse;
    SimpleLineContext context;
    RunList list;

    RunListInitialize(&list);

    context.typesetter = typesetter;
    context.list = &list;
    context.hasLayoutWidth = TRFalse;
    context.layoutWidth = 0.0f;

    if (paragraphIndex != TRInvalidIndex
            && LineResolverForEachVisualRun(typesetter, start, end, AppendRunOfSimpleLine,
                &context) && !list.hasFailed) {
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
            typesetter->paragraphs[paragraphIndex].baseLevel);
    } else {
        RunListFinalize(&list);
    }

    return justifiedLine;
}
