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
        return;
    }

    if (list->count == list->capacity) {
        TRUInteger newCapacity = (list->capacity == 0 ? 8 : list->capacity * 2);
        GlyphRunRef *newItems = AllocatorReallocateBlock(list->items,
            newCapacity * sizeof(GlyphRunRef));

        if (!newItems) {
            TRGlyphRunRelease(glyphRun);
            list->hasFailed = TRTrue;
            return;
        }

        list->items = newItems;
        list->capacity = newCapacity;
    }

    memmove(&list->items[index + 1], &list->items[index],
        (list->count - index) * sizeof(GlyphRunRef));
    list->items[index] = glyphRun;
    list->count += 1;
}

TR_INTERNAL TRBoolean LineResolverForEachVisualRun(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, VisualRunFunc func, void *context)
{
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);
    TRBoolean isRTL;
    TRUInteger feasibleStart;
    TRUInteger feasibleEnd;

    if (paragraphIndex == TRInvalidIndex) {
        return TRFalse;
    }

    isRTL = (typesetter->paragraphs[paragraphIndex].baseLevel & 1) == 1;

    /* The paragraphs of a right-to-left line are shown from the last one to the first. */
    if (isRTL && typesetter->paragraphs[paragraphIndex].end < end) {
        paragraphIndex = TypesetterFindParagraph(typesetter, end - 1);
    }

    do {
        const ParagraphInfo *paragraph = &typesetter->paragraphs[paragraphIndex];
        SBVisualRunIteratorRef iterator;

        feasibleStart = (paragraph->start > start ? paragraph->start : start);
        feasibleEnd = (paragraph->end < end ? paragraph->end : end);

        iterator = SBTextCreateVisualRunIterator(sbText, feasibleStart, feasibleEnd - feasibleStart);
        if (!iterator) {
            return TRFalse;
        }

        while (SBVisualRunIteratorMoveNext(iterator)) {
            const SBVisualRun *sbRun = SBVisualRunIteratorGetCurrent(iterator);
            VisualRun visualRun;

            visualRun.start = sbRun->index;
            visualRun.end = sbRun->index + sbRun->length;
            visualRun.level = sbRun->level;

            func(context, &visualRun);
        }

        SBVisualRunIteratorRelease(iterator);

        if (isRTL) {
            if (paragraphIndex == 0) {
                break;
            }
            paragraphIndex -= 1;
        } else {
            paragraphIndex += 1;
        }
    } while (isRTL ? feasibleStart != start : feasibleEnd != end);

    return TRTrue;
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

TR_INTERNAL void LineResolverAppendVisualRuns(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    TRUInteger insertIndex = list->count;
    TRUInteger visualStart = start;
    TRBoolean hasPrevious = TRFalse;
    TRUInt8 previousLevel = 0;

    if (start >= end) {
        return;
    }

    /*
     * ASSUMPTIONS:
     *      - The range may fall in one or more text runs.
     *      - Consecutive text runs may have the same bidirectional level.
     */
    do {
        TRUInteger runIndex = TypesetterFindRun(typesetter, visualStart);
        TextRunRef textRun;
        TRUInteger feasibleStart;
        TRUInteger feasibleEnd;
        TRUInt8 bidiLevel;
        TRBoolean isForwardRun;

        if (runIndex == TRInvalidIndex) {
            list->hasFailed = TRTrue;
            return;
        }

        textRun = typesetter->runs[runIndex];
        feasibleStart = (textRun->codeUnitStart > visualStart ? textRun->codeUnitStart : visualStart);
        feasibleEnd = (textRun->codeUnitEnd < end ? textRun->codeUnitEnd : end);
        bidiLevel = textRun->bidiLevel;
        isForwardRun = (bidiLevel & 1) == 0;

        if (hasPrevious && (bidiLevel != previousLevel || isForwardRun)) {
            insertIndex = list->count;
        }

        if (textRun->kind == TextRunKindReplacement) {
            /* A replacement is taken as a whole, and its room is decided by the layout width. */
            GlyphRunPaint paint;
            TextRunRef sizedRun;

            GetPaint(typesetter, feasibleStart, &paint);
            sizedRun = (hasLayoutWidth ? TextRunCreateForLayoutWidth(textRun, layoutWidth)
                                       : TextRunRetain(textRun));

            if (sizedRun) {
                RunListInsert(list, insertIndex, GlyphRunCreate(sizedRun, feasibleStart,
                    feasibleEnd, &paint));
                TextRunRelease(sizedRun);
            } else {
                list->hasFailed = TRTrue;
            }

            if (isForwardRun) {
                insertIndex += 1;
            }
        } else {
            TRUInteger spanStart = feasibleStart;

            /* Each part that is painted alike has a run of its own. */
            while (spanStart < feasibleEnd) {
                GlyphRunPaint paint;
                TRUInteger spanLength = GetPaint(typesetter, spanStart, &paint);
                TRUInteger spanEnd = spanStart + spanLength;

                if (spanLength == 0 || spanEnd > feasibleEnd) {
                    spanEnd = feasibleEnd;
                }

                RunListInsert(list, insertIndex, GlyphRunCreate(textRun, spanStart, spanEnd, &paint));

                if (isForwardRun) {
                    insertIndex += 1;
                }

                spanStart = spanEnd;
            }
        }

        hasPrevious = TRTrue;
        previousLevel = bidiLevel;
        visualStart = feasibleEnd;
    } while (visualStart < end && !list->hasFailed);
}

TR_INTERNAL ComposedLineRef LineResolverCreateLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRUInt8 paragraphLevel)
{
    ComposedLineRef line;

    if (list->hasFailed) {
        RunListFinalize(list);
        return NULL;
    }

    line = ComposedLineCreate(&typesetter->buffer, start, end, list->items, list->count,
        paragraphLevel);

    /* The line took the references of the runs, and the memory of the list is still the list's. */
    list->count = 0;
    RunListFinalize(list);

    return line;
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

TR_INTERNAL ComposedLineRef LineResolverCreateSimpleLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRBoolean hasLayoutWidth, TRFloat layoutWidth)
{
    RunList list;
    SimpleLineContext context;
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);

    RunListInitialize(&list);

    context.typesetter = typesetter;
    context.list = &list;
    context.hasLayoutWidth = hasLayoutWidth;
    context.layoutWidth = layoutWidth;

    if (paragraphIndex == TRInvalidIndex
            || !LineResolverForEachVisualRun(typesetter, start, end, AppendRunOfSimpleLine,
                &context)) {
        RunListFinalize(&list);
        return NULL;
    }

    return LineResolverCreateLine(typesetter, start, end, &list,
        typesetter->paragraphs[paragraphIndex].baseLevel);
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

static void AppendRunOfTruncation(void *context, const VisualRun *visualRun)
{
    TruncationHandler *handler = context;
    TypesetterRef typesetter = handler->typesetter;
    RunList *list = handler->list;
    TRUInteger visualStart = visualRun->start;
    TRUInteger visualEnd = visualRun->end;

    if ((visualRun->level & 1) == 1) {
        /* Handle the second part of the characters. */
        if (visualEnd >= handler->skipEnd) {
            LineResolverAppendVisualRuns(typesetter,
                (visualStart > handler->skipEnd ? visualStart : handler->skipEnd), visualEnd, list,
                TRFalse, 0.0f);

            if (visualStart < handler->skipEnd) {
                handler->trailingTokenIndex = list->count;
            }
        }

        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = list->count;
            }

            LineResolverAppendVisualRuns(typesetter, visualStart,
                (visualEnd < handler->skipStart ? visualEnd : handler->skipStart), list, TRFalse,
                0.0f);
        }
    } else {
        /* Handle the first part of the characters. */
        if (visualStart <= handler->skipStart) {
            LineResolverAppendVisualRuns(typesetter, visualStart,
                (visualEnd < handler->skipStart ? visualEnd : handler->skipStart), list, TRFalse,
                0.0f);

            if (visualEnd > handler->skipStart) {
                handler->leadingTokenIndex = list->count;
            }
        }

        /* Handle the second part of the characters. */
        if (visualEnd >= handler->skipEnd) {
            if (visualStart < handler->skipEnd) {
                handler->trailingTokenIndex = list->count;
            }

            LineResolverAppendVisualRuns(typesetter,
                (visualStart > handler->skipEnd ? visualStart : handler->skipEnd), visualEnd, list,
                TRFalse, 0.0f);
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

static TRUInt8 GetBaseLevel(TypesetterRef typesetter, TRUInteger index)
{
    return typesetter->paragraphs[TypesetterFindParagraph(typesetter, index)].baseLevel;
}

static ComposedLineRef CreateStartTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    TRUInteger truncatedStart = BreakResolverSuggestBackwardBreak(typesetter, tokenlessWidth,
        start, end, breakMode);

    if (truncatedStart > start) {
        RunList list;
        TRUInteger tokenInsertIndex = 0;

        RunListInitialize(&list);

        if (truncatedStart < end) {
            TRUInteger leadingIndex, trailingIndex;

            if (!AppendRunsAroundSkip(typesetter, start, end, start, truncatedStart, &list,
                    &leadingIndex, &trailingIndex)) {
                RunListFinalize(&list);
                return NULL;
            }

            tokenInsertIndex = (trailingIndex == TRInvalidIndex ? 0 : trailingIndex);
        }

        AppendTokenRuns(token, &list, tokenInsertIndex);

        return LineResolverCreateLine(typesetter, truncatedStart, end, &list,
            GetBaseLevel(typesetter, truncatedStart));
    }

    return LineResolverCreateSimpleLine(typesetter, truncatedStart, end, TRFalse, 0.0f);
}

static ComposedLineRef CreateMiddleTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    TRFloat halfWidth = tokenlessWidth / 2.0f;
    TRUInteger firstMidEnd = BreakResolverSuggestForwardBreak(typesetter, halfWidth, start, end,
        breakMode);
    TRUInteger secondMidStart = BreakResolverSuggestBackwardBreak(typesetter, halfWidth, start, end,
        breakMode);

    if (firstMidEnd < secondMidStart) {
        RunList list;
        TRUInteger tokenInsertIndex = 0;

        /* The whitespaces that are inside are excluded, as the token replaces them. */
        firstMidEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, firstMidEnd);
        secondMidStart = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, secondMidStart, end);

        RunListInitialize(&list);

        if (start < firstMidEnd || secondMidStart < end) {
            TRUInteger leadingIndex, trailingIndex;

            if (!AppendRunsAroundSkip(typesetter, start, end, firstMidEnd, secondMidStart, &list,
                    &leadingIndex, &trailingIndex)) {
                RunListFinalize(&list);
                return NULL;
            }

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        AppendTokenRuns(token, &list, tokenInsertIndex);

        return LineResolverCreateLine(typesetter, start, end, &list, GetBaseLevel(typesetter, start));
    }

    return LineResolverCreateSimpleLine(typesetter, start, end, TRFalse, 0.0f);
}

static ComposedLineRef CreateEndTruncatedLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, TRFloat tokenlessWidth, TRBreakMode breakMode, ComposedLineRef token)
{
    TRUInteger truncatedEnd = BreakResolverSuggestForwardBreak(typesetter, tokenlessWidth, start,
        end, breakMode);

    if (truncatedEnd < end) {
        RunList list;
        TRUInteger tokenInsertIndex = 0;

        /* The trailing whitespaces are excluded, as the token replaces them. */
        truncatedEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, truncatedEnd);

        RunListInitialize(&list);

        if (start < truncatedEnd) {
            TRUInteger leadingIndex, trailingIndex;

            if (!AppendRunsAroundSkip(typesetter, start, end, truncatedEnd, end, &list,
                    &leadingIndex, &trailingIndex)) {
                RunListFinalize(&list);
                return NULL;
            }

            tokenInsertIndex = (leadingIndex == TRInvalidIndex ? 0 : leadingIndex);
        }

        AppendTokenRuns(token, &list, tokenInsertIndex);

        return LineResolverCreateLine(typesetter, start, truncatedEnd, &list,
            GetBaseLevel(typesetter, start));
    }

    return LineResolverCreateSimpleLine(typesetter, start, truncatedEnd, TRFalse, 0.0f);
}

TR_INTERNAL ComposedLineRef LineResolverCreateTruncatedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, ComposedLineRef tokenLine)
{
    TRFloat tokenlessWidth = extent - tokenLine->extent;

    switch (truncationPlace) {
    case TRTruncationPlaceStart:
        return CreateStartTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);

    case TRTruncationPlaceMiddle:
        return CreateMiddleTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);

    case TRTruncationPlaceEnd:
        return CreateEndTruncatedLine(typesetter, start, end, tokenlessWidth, breakMode,
            tokenLine);
    }

    return NULL;
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

TR_INTERNAL ComposedLineRef LineResolverCreateJustifiedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat justificationFactor, TRFloat justificationExtent)
{
    TRUInteger wordStart = TextBufferGetLeadingWhitespaceEnd(&typesetter->buffer, start, end);
    TRUInteger wordEnd = TextBufferGetTrailingWhitespaceStart(&typesetter->buffer, start, end);
    TRFloat actualWidth = TypesetterMeasureRange(typesetter, start, wordEnd);
    TRFloat extraWidth = justificationExtent - actualWidth;
    TRFloat availableWidth = extraWidth * justificationFactor;
    TRUInteger innerSpaceCount = ComputeSpaceCount(typesetter, wordStart, wordEnd);
    TRFloat spaceAddition = (innerSpaceCount > 0 ? availableWidth / (TRFloat)innerSpaceCount : 0.0f);
    SimpleLineContext context;
    TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, start);
    RunList list;
    TRUInteger runIndex;

    RunListInitialize(&list);

    context.typesetter = typesetter;
    context.list = &list;
    context.hasLayoutWidth = TRFalse;
    context.layoutWidth = 0.0f;

    if (paragraphIndex == TRInvalidIndex
            || !LineResolverForEachVisualRun(typesetter, start, end, AppendRunOfSimpleLine,
                &context) || list.hasFailed) {
        RunListFinalize(&list);
        return NULL;
    }

    for (runIndex = 0; runIndex < list.count; runIndex++) {
        GlyphRunRef glyphRun = list.items[runIndex];
        TRUInteger runStart = (wordStart > glyphRun->codeUnitStart ? wordStart : glyphRun->codeUnitStart);
        TRUInteger runEnd = (wordEnd < glyphRun->codeUnitEnd ? wordEnd : glyphRun->codeUnitEnd);
        TRFloat *advances;
        GlyphRunRef justifiedRun;
        TRUInteger index;

        /* There is nothing to add to a replacement, or to a run without any space in it. */
        if (glyphRun->textRun->kind == TextRunKindReplacement || glyphRun->glyphCount == 0) {
            continue;
        }

        advances = AllocatorAllocateBlock(sizeof(TRFloat) * glyphRun->glyphCount);
        if (!advances) {
            RunListFinalize(&list);
            return NULL;
        }

        memcpy(advances, GlyphRunGetAdvances(glyphRun), sizeof(TRFloat) * glyphRun->glyphCount);

        index = runStart;
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

        justifiedRun = GlyphRunCreateJustified(glyphRun, advances);
        AllocatorDeallocateBlock(advances);

        if (!justifiedRun) {
            RunListFinalize(&list);
            return NULL;
        }

        TRGlyphRunRelease(glyphRun);
        list.items[runIndex] = justifiedRun;
    }

    return LineResolverCreateLine(typesetter, start, end, &list,
        typesetter->paragraphs[paragraphIndex].baseLevel);
}
