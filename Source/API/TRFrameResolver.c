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


#include <math.h>
#include <stddef.h>
#include <string.h>

#include <SheenBidi/SBAttributeList.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRFrameResolver.h>
#include <Tehreer/TRText.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRComposedFrame.h>
#include <API/TRComposedLine.h>
#include <API/TRTypesetter.h>
#include <Core/Array.h>
#include <Core/Object.h>
#include <Layout/BreakResolver.h>
#include <Layout/LineResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Layout/TokenResolver.h>

#include "TRFrameResolver.h"

#define MaxFloat    3.402823466e+38f

/* The style of a paragraph, as its attributes say it. */
typedef struct _ParagraphStyle {
    TRBoolean hasAlignment;
    TRTextAlignment alignment;
    TRFloat firstLineHeadIndent;
    TRFloat headIndent;
    TRFloat tailIndent;
    TRUInteger firstIndentLineCount;
    TRFloat spacingBefore;
    TRFloat spacing;
    TRFloat lineHeightMultiple;
    TRFloat minimumLineHeight;
    TRFloat maximumLineHeight;
    TRFloat lineSpacing;
} ParagraphStyle;

typedef struct _FrameContext {
    TRFloat layoutWidth;
    TRFloat layoutHeight;
    TRUInteger maxLines;

    Array lines;
    TRBoolean isFilled;
    TRBoolean hasFailed;
    TRBoolean isTruncated;

    TRFloat occupiedWidth;
    TRFloat occupiedHeight;
    TRFloat lastWidth;
    TRFloat lastHeight;

    TRUInteger paragraphIndex;
    TRUInteger startIndex;
    TRUInteger endIndex;
    TRUInt8 baseLevel;
    ParagraphStyle style;

    TRFloat lineExtent;
    TRFloat lineMargins;
    TRFloat leftIndent;
    TRFloat flushFactor;
} FrameContext;

static void FinalizeFrameResolver(ObjectRef object)
{
    TRFrameResolverRef resolver = object;

    if (resolver->typesetter) {
        TRTypesetterRelease(resolver->typesetter);
    }
}

static TRFloat Clamp(TRFloat value)
{
    TRFloat clamped = 0.0f;

    if (value > 0.0f) {
        clamped = (value < MaxFloat ? value : MaxFloat);
    }

    return clamped;
}

static TRFloat GetFlushFactor(TRTextAlignment alignment, TRUInt8 baseLevel)
{
    TRFloat flushFactor = 0.0f;
    TRBoolean isRTL = (baseLevel & 1) == 1;

    switch (alignment) {
    case TRTextAlignmentLeft:
        flushFactor = 0.0f;
        break;

    case TRTextAlignmentCenter:
        flushFactor = 0.5f;
        break;

    case TRTextAlignmentRight:
        flushFactor = 1.0f;
        break;

    case TRTextAlignmentLeading:
        flushFactor = (isRTL ? 1.0f : 0.0f);
        break;

    case TRTextAlignmentTrailing:
        flushFactor = (isRTL ? 0.0f : 1.0f);
        break;
    }

    return flushFactor;
}

static TRFloat GetVerticalMultiplier(TRVerticalAlignment alignment)
{
    TRFloat multiplier = 0.0f;

    switch (alignment) {
    case TRVerticalAlignmentCenter:
        multiplier = 0.5f;
        break;

    case TRVerticalAlignmentBottom:
        multiplier = 1.0f;
        break;
    }

    return multiplier;
}

/* ---------- Context Handling ---------- */

static TRUInteger GetLineCount(const FrameContext *context)
{
    return ArrayGetCount(&context->lines);
}

static TRComposedLine *GetLine(const FrameContext *context, TRUInteger index)
{
    return *(TRComposedLine **)ArrayGetItem(&context->lines, index);
}

static void SetLine(FrameContext *context, TRUInteger index, TRComposedLine *line)
{
    *(TRComposedLine **)ArrayGetItem(&context->lines, index) = line;
}

static void AppendLine(FrameContext *context, TRComposedLine *line)
{
    if (!ArrayAppend(&context->lines, &line)) {
        TRComposedLineRelease(line);
        context->hasFailed = TRTrue;
    }
}

static void FinalizeContext(FrameContext *context)
{
    TRUInteger count = GetLineCount(context);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TRComposedLineRelease(GetLine(context, index));
    }

    ArrayFinalize(&context->lines);
}

/* ---------- Paragraph Handling ---------- */

static void LoadParagraphStyle(FrameContext *context, TRTextRef text, TRUInteger index)
{
    ParagraphStyle *style = &context->style;
    TRAttributeListRef list;
    TRUInteger length;

    memset(style, 0, sizeof(ParagraphStyle));
    style->firstIndentLineCount = 1;

    /* The paragraph attributes are the same for the whole paragraph. */
    list = TRTextGetAttributes(text, index, &length);

    if (list) {
        TRUInteger count;
        TRUInteger itemIndex;

        count = TRAttributeListGetCount(list);

        for (itemIndex = 0; itemIndex < count; itemIndex++) {
            const TRAttribute *attribute = TRAttributeListGetItem(list, itemIndex);

            switch (attribute->type) {
            case TRAttributeTextAlignment:
                style->hasAlignment = TRTrue;
                style->alignment = attribute->value.textAlignment;
                break;

            case TRAttributeFirstLineHeadIndent:
                style->firstLineHeadIndent = attribute->value.firstLineHeadIndent;
                break;

            case TRAttributeHeadIndent:
                style->headIndent = attribute->value.headIndent;
                break;

            case TRAttributeTailIndent:
                style->tailIndent = attribute->value.tailIndent;
                break;

            case TRAttributeFirstIndentLineCount:
                style->firstIndentLineCount = attribute->value.firstIndentLineCount;
                break;

            case TRAttributeParagraphSpacingBefore:
                style->spacingBefore = attribute->value.paragraphSpacingBefore;
                break;

            case TRAttributeParagraphSpacing:
                style->spacing = attribute->value.paragraphSpacing;
                break;

            case TRAttributeLineHeightMultiple:
                style->lineHeightMultiple = attribute->value.lineHeightMultiple;
                break;

            case TRAttributeMinimumLineHeight:
                style->minimumLineHeight = attribute->value.minimumLineHeight;
                break;

            case TRAttributeMaximumLineHeight:
                style->maximumLineHeight = attribute->value.maximumLineHeight;
                break;

            case TRAttributeLineSpacing:
                style->lineSpacing = attribute->value.lineSpacing;
                break;

            default:
                break;
            }
        }

        SBAttributeListRelease(list);
    }
}

static void ResolveIndents(FrameContext *context, TRFloat headIndent, TRFloat tailIndent)
{
    TRBoolean isRTL = (context->baseLevel & 1) == 1;

    if (tailIndent > 0.0f) {
        TRFloat resolvedIndent = context->layoutWidth - (headIndent + tailIndent);

        context->leftIndent = (isRTL ? resolvedIndent : headIndent);
        context->lineMargins = headIndent + resolvedIndent;
        context->lineExtent = tailIndent;
    } else {
        context->leftIndent = (isRTL ? -tailIndent : headIndent);
        context->lineMargins = headIndent + -tailIndent;
        context->lineExtent = context->layoutWidth - context->lineMargins;
    }
}

/* Sets the indents that the line of the paragraph has, counting from its first one. */
static void ResolveLineIndents(FrameContext *context, TRUInteger lineIndex)
{
    const ParagraphStyle *style = &context->style;
    TRFloat headIndent = (lineIndex < style->firstIndentLineCount
                          ? style->firstLineHeadIndent
                          : style->headIndent);

    ResolveIndents(context, headIndent, style->tailIndent);
}

/* Sets up the paragraph that has the segment, and the properties of its first line. */
static void SetupParagraph(FrameContext *context, TRFrameResolverRef resolver,
    TRUInteger paragraphIndex, TRUInteger segmentStart, TRUInteger segmentEnd)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    const ParagraphInfo *paragraph = &typesetter->paragraphs[paragraphIndex];

    context->paragraphIndex = paragraphIndex;
    context->startIndex = segmentStart;
    context->endIndex = segmentEnd;
    context->baseLevel = paragraph->baseLevel;

    LoadParagraphStyle(context, typesetter->text, segmentStart);

    context->flushFactor = GetFlushFactor(context->style.hasAlignment
                                          ? context->style.alignment
                                          : resolver->textAlignment, context->baseLevel);
    ResolveLineIndents(context, 0);
}

/* ---------- Line Handling ---------- */

static void ResolveLineStyle(const ParagraphStyle *style, TRComposedLine *line)
{
    TRFloat height;

    /* Resolve `lineHeightMultiple`. */
    if (style->lineHeightMultiple > 0.0f) {
        TRFloat oldHeight = line->ascent + line->descent + line->leading;
        TRFloat newHeight = oldHeight * style->lineHeightMultiple;

        line->ascent += newHeight - oldHeight;
    }

    /* Resolve `minimumLineHeight`. */
    height = line->ascent + line->descent + line->leading;
    if (style->minimumLineHeight > 0.0f && height < style->minimumLineHeight) {
        line->ascent += style->minimumLineHeight - height;
    }

    /* Resolve `maximumLineHeight`. */
    height = line->ascent + line->descent + line->leading;
    if (style->maximumLineHeight > 0.0f && height > style->maximumLineHeight) {
        line->ascent -= height - style->maximumLineHeight;
    }

    /* Resolve `lineSpacing`. */
    line->leading += style->lineSpacing;
}

static void ResolveAttributes(FrameContext *context, TRFrameResolverRef resolver,
    TRComposedLine *line)
{
    ResolveLineStyle(&context->style, line);

    /* The line of a view is as tall as the view and its margins. */
    if (!line->isBlock) {
        if (resolver->lineHeightMultiplier > 0.0f) {
            TRFloat oldHeight = line->ascent + line->descent + line->leading;
            TRFloat newHeight = oldHeight * resolver->lineHeightMultiplier;
            TRFloat midOffset = (newHeight - oldHeight) / 2.0f;

            /* Adjust metrics in such a way that text remains in the middle of the line. */
            line->ascent += midOffset;
            line->descent += midOffset;
        }

        if (resolver->extraLineSpacing > 0.0f) {
            line->leading += resolver->extraLineSpacing;
        }
    }

    line->origin.x = context->leftIndent
        + TRComposedLineGetPenOffset(line, context->flushFactor,
            context->lineExtent);
    line->origin.y = context->occupiedHeight + line->ascent;
    line->intrinsicMargin = context->layoutWidth - context->lineExtent;
    line->flushFactor = context->flushFactor;
}

/* ---------- Layout Handling ---------- */

static void ComputeOccupiedSize(const FrameContext *context, TRComposedLineRef line,
    TRFloat *width, TRFloat *height)
{
    TRFloat lineWidth = context->lineMargins + line->extent - line->trailingWhitespaceExtent;
    TRFloat lineHeight = line->ascent + line->descent + line->leading;

    *width = (context->occupiedWidth > lineWidth ? context->occupiedWidth : lineWidth);
    *height = context->occupiedHeight + lineHeight;
}

static void ResolveOccupiedSize(FrameContext *context, TRFloat width, TRFloat height)
{
    context->lastWidth = context->occupiedWidth;
    context->lastHeight = context->occupiedHeight;
    context->occupiedWidth = width;
    context->occupiedHeight = height;
}

static TRBoolean AddResolvedLine(FrameContext *context, TRFrameResolverRef resolver,
    TRComposedLine *line)
{
    TRBoolean isStopped = TRFalse;
    TRFloat width, height;

    ResolveAttributes(context, resolver, line);

    /* Make sure that at least one line is added even if frame is smaller in height. */
    ComputeOccupiedSize(context, line, &width, &height);

    if (height > resolver->frameHeight && GetLineCount(context) > 0) {
        TRComposedLineRelease(line);
        context->isFilled = TRTrue;
        isStopped = TRTrue;
    } else {
        /* Append the line, and update the occupied size. */
        AppendLine(context, line);

        if (context->hasFailed) {
            isStopped = TRTrue;
        } else {
            ResolveOccupiedSize(context, width, height);

            /* Stop the filling process if maximum lines have been added. */
            if (GetLineCount(context) == context->maxLines) {
                context->isFilled = TRTrue;
                isStopped = TRTrue;
            }
        }
    }

    return isStopped;
}

/* ---------- Frame Filling ---------- */

static void ResolveParagraphLines(FrameContext *context, TRFrameResolverRef resolver)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    const ParagraphInfo *paragraph = &typesetter->paragraphs[context->paragraphIndex];
    TRBoolean isStopped = TRFalse;
    TRUInteger lineIndex = 0;
    TRUInteger lineStart = context->startIndex;

    /* Resolve `paragraphSpacingBefore` if it is not the first paragraph. */
    if (paragraph->start > 0) {
        context->occupiedHeight += context->style.spacingBefore;
    }

    /* Iterate over each line of this paragraph. */
    while (lineStart != context->endIndex) {
        TRUInteger lineEnd;
        TRComposedLine *line;

        ResolveLineIndents(context, lineIndex);

        /* Find out the length of new line. */
        lineEnd = BreakResolverSuggestForwardBreak(typesetter, context->lineExtent, lineStart,
            context->endIndex, TRBreakModeLine);
        if (lineEnd <= lineStart) {
            lineEnd = lineStart + 1;
        }

        /* Create the line and resolve its attributes. */
        line = LineResolverCreateSimpleLine(typesetter, lineStart, lineEnd, TRTrue,
            context->layoutWidth);

        if (!line) {
            context->hasFailed = TRTrue;
            isStopped = TRTrue;
            break;
        }

        isStopped = AddResolvedLine(context, resolver, line);
        if (isStopped) {
            break;
        }

        lineIndex++;
        lineStart = lineEnd;
    }

    /* Resolve `paragraphSpacing` if it is not the last paragraph. */
    if (!isStopped && context->endIndex < typesetter->buffer.length) {
        context->occupiedHeight += context->style.spacing;
    }
}

static void ResolveFrameParagraphs(FrameContext *context, TRFrameResolverRef resolver,
    TRRange range)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    TRUInteger rangeEnd = range.index + range.length;
    TRUInteger paragraphIndex = TRTypesetterFindParagraph(typesetter, range.index);
    TRUInteger segmentStart = range.index;

    /* Iterate over all paragraphs in provided range. */
    do {
        const ParagraphInfo *paragraph = &typesetter->paragraphs[paragraphIndex];
        TRUInteger segmentEnd = (rangeEnd < paragraph->end ? rangeEnd : paragraph->end);

        SetupParagraph(context, resolver, paragraphIndex, segmentStart, segmentEnd);
        ResolveParagraphLines(context, resolver);

        segmentStart = segmentEnd;
        paragraphIndex++;
    } while (!context->isFilled && !context->hasFailed && segmentStart < rangeEnd);
}

/* ---------- Truncation Handling ---------- */

static void TruncateLastLine(FrameContext *context, TRFrameResolverRef resolver,
    TRUInteger frameEnd)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    TRUInteger lineCount = GetLineCount(context);
    TRComposedLineRef lastLine = GetLine(context, lineCount - 1);
    TRUInteger lastStart = lastLine->codeUnitStart;
    TRComposedLineRef token;
    TRUInteger paragraphIndex;
    TRUInteger segmentStart;
    TRUInteger lineIndex;
    TRUInteger index;
    TRFloat width, height;

    /* The last line might be in a paragraph before the one that the filling stopped in. */
    paragraphIndex = TRTypesetterFindParagraph(typesetter, lastStart);
    segmentStart = typesetter->paragraphs[paragraphIndex].start;
    if (paragraphIndex == context->paragraphIndex) {
        segmentStart = context->startIndex;
    } else if (lastLine->codeUnitStart < segmentStart) {
        segmentStart = lastLine->codeUnitStart;
    }

    lineIndex = 0;
    for (index = 0; index + 1 < lineCount; index++) {
        TRUInteger start = GetLine(context, index)->codeUnitStart;

        if (start >= segmentStart && start < lastStart) {
            lineIndex++;
        }
    }

    if (paragraphIndex != context->paragraphIndex) {
        SetupParagraph(context, resolver, paragraphIndex, segmentStart,
            typesetter->paragraphs[paragraphIndex].end);
    }
    ResolveLineIndents(context, lineIndex);

    /* Restore the occupied size to previous value. */
    context->occupiedWidth = context->lastWidth;
    context->occupiedHeight = context->lastHeight;

    /* Create the truncated line and resolve its attributes. */
    token = TokenResolverCreateTokenLine(typesetter, lastStart, frameEnd, resolver->truncationPlace,
        NULL, 0, TRStringEncodingUTF16);

    if (token) {
        TRComposedLine *line;

        line = LineResolverCreateTruncatedLine(typesetter, lastStart, frameEnd, context->lineExtent,
            resolver->truncationMode, resolver->truncationPlace, token);
        TRComposedLineRelease(token);

        if (line) {
            ResolveAttributes(context, resolver, line);

            /* Replace the line and update the occupied size. */
            TRComposedLineRelease(lastLine);
            SetLine(context, lineCount - 1, line);
            context->isTruncated = TRTrue;

            ComputeOccupiedSize(context, line, &width, &height);
            ResolveOccupiedSize(context, width, height);
        } else {
            context->hasFailed = TRTrue;
        }
    } else {
        context->hasFailed = TRTrue;
    }
}

static void ResolveTruncation(FrameContext *context, TRFrameResolverRef resolver,
    TRUInteger frameEnd)
{
    if (resolver->isTruncationEnabled) {
        TRComposedLineRef lastLine = GetLine(context, GetLineCount(context) - 1);

        /* No need to truncate if frame range is already covered. */
        if (lastLine->codeUnitEnd != frameEnd) {
            TruncateLastLine(context, resolver, frameEnd);
        }
    }
}

/* ---------- Alignment Handling ---------- */

static void ResolveAlignments(FrameContext *context, TRFrameResolverRef resolver)
{
    TRUInteger lineCount = GetLineCount(context);
    TRUInteger index;

    if (resolver->fitsVertically) {
        /* Update the layout height to occupied height. */
        context->layoutHeight = context->occupiedHeight;
    } else {
        /* Find out the additional top for vertical alignment. */
        TRFloat extraHeight = context->layoutHeight - context->occupiedHeight;
        TRFloat additionalTop = extraHeight * GetVerticalMultiplier(resolver->verticalAlignment);

        /* Readjust the vertical position of each line. */
        for (index = 0; index < lineCount; index++) {
            GetLine(context, index)->origin.y += additionalTop;
        }
    }

    if (resolver->fitsHorizontally) {
        TRFloat extraWidth = context->layoutWidth - context->occupiedWidth;

        /* Readjust the horizontal position of each line. */
        for (index = 0; index < lineCount; index++) {
            TRComposedLine *line = GetLine(context, index);

            line->origin.x -= extraWidth * line->flushFactor;
        }

        /* Update the layout width to occupied width. */
        context->layoutWidth = context->occupiedWidth;
    }
}

/* ---------- Justification Handling ---------- */

static TRBoolean EndsBeforeBlock(TRTypesetterRef typesetter, TRUInteger codeUnitEnd)
{
    TRBoolean isBeforeBlock = TRFalse;

    if (codeUnitEnd < typesetter->buffer.length) {
        TRUInteger runIndex = TRTypesetterFindRun(typesetter, codeUnitEnd);

        isBeforeBlock = TextRunIsBlock(typesetter->runs[runIndex]);
    }

    return isBeforeBlock;
}

static TRBoolean IsLineJustifiable(TRTypesetterRef typesetter, TRComposedLine *line)
{
    TRBoolean isJustifiable = TRFalse;
    TRUInteger paragraphIndex = TRTypesetterFindParagraph(typesetter, line->codeUnitEnd - 1);

    /*
     * The last line of paragraph is skipped if it's smaller in width. The line that shows a token
     * cannot be made again from its text. The line of a view has nothing to justify, and the one
     * before it ends there.
     */
    if (typesetter->paragraphs[paragraphIndex].end != line->codeUnitEnd
            && !line->isTruncated
            && !line->isBlock
            && !EndsBeforeBlock(typesetter, line->codeUnitEnd)) {
        isJustifiable = TRTrue;
    }

    return isJustifiable;
}

static void JustifyLine(FrameContext *context, TRFrameResolverRef resolver, TRUInteger index)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    TRComposedLine *line = GetLine(context, index);

    if (IsLineJustifiable(typesetter, line)) {
        TRComposedLine *justified;
        TRFloat availableWidth;

        /* The line is justified to what is left after the margins of its paragraph. */
        availableWidth = context->layoutWidth - line->intrinsicMargin;

        justified = LineResolverCreateJustifiedLine(typesetter, line->codeUnitStart,
            line->codeUnitEnd, resolver->justificationLevel, availableWidth);

        /* The line is shown as it is if it cannot be justified. */
        if (justified) {
            TRFloat marginalLeft = 0.0f;
            TRFloat alignedLeft;

            alignedLeft = TRComposedLineGetPenOffset(justified,
                line->flushFactor, availableWidth);

            if ((justified->paragraphLevel & 1) == 0) {
                marginalLeft = line->intrinsicMargin;
            }

            justified->origin.x = marginalLeft + alignedLeft;
            justified->origin.y = line->origin.y;
            justified->intrinsicMargin = line->intrinsicMargin;
            justified->flushFactor = line->flushFactor;

            /* Setup the line metrics. */
            justified->ascent = line->ascent;
            justified->descent = line->descent;
            justified->leading = line->leading;

            TRComposedLineRelease(line);
            SetLine(context, index, justified);
        }
    }
}

static void ResolveJustification(FrameContext *context, TRFrameResolverRef resolver)
{
    if (resolver->isJustificationEnabled) {
        TRUInteger index;

        for (index = 0; index < GetLineCount(context); index++) {
            JustifyLine(context, resolver, index);
        }
    }
}

TRFrameResolverRef TRFrameResolverCreate(void)
{
    const TRUInteger size = sizeof(TRFrameResolver);
    void *pointer = NULL;
    TRFrameResolverRef resolver = ObjectCreate(&size, 1, &pointer, FinalizeFrameResolver);

    if (resolver) {
        resolver->typesetter = NULL;
        resolver->frameWidth = MaxFloat;
        resolver->frameHeight = MaxFloat;
        resolver->fitsHorizontally = TRFalse;
        resolver->fitsVertically = TRFalse;
        resolver->textAlignment = TRTextAlignmentLeading;
        resolver->verticalAlignment = TRVerticalAlignmentTop;
        resolver->truncationMode = TRBreakModeLine;
        resolver->isTruncationEnabled = TRFalse;
        resolver->truncationPlace = TRTruncationPlaceEnd;
        resolver->isJustificationEnabled = TRFalse;
        resolver->justificationLevel = 1.0f;
        resolver->maxLines = 0;
        resolver->extraLineSpacing = 0.0f;
        resolver->lineHeightMultiplier = 1.0f;
    }

    return resolver;
}

void TRFrameResolverSetTypesetter(TRFrameResolverRef resolver, TRTypesetterRef typesetter)
{
    if (typesetter) {
        TRTypesetterRetain(typesetter);
    }
    if (resolver->typesetter) {
        TRTypesetterRelease(resolver->typesetter);
    }
    resolver->typesetter = typesetter;
}

void TRFrameResolverSetFrameSize(TRFrameResolverRef resolver, TRFloat width, TRFloat height)
{
    resolver->frameWidth = width;
    resolver->frameHeight = height;
}

void TRFrameResolverSetFitsHorizontally(TRFrameResolverRef resolver, TRBoolean fits)
{
    resolver->fitsHorizontally = fits;
}

void TRFrameResolverSetFitsVertically(TRFrameResolverRef resolver, TRBoolean fits)
{
    resolver->fitsVertically = fits;
}

void TRFrameResolverSetTextAlignment(TRFrameResolverRef resolver, TRTextAlignment alignment)
{
    resolver->textAlignment = alignment;
}

void TRFrameResolverSetVerticalAlignment(TRFrameResolverRef resolver,
    TRVerticalAlignment alignment)
{
    resolver->verticalAlignment = alignment;
}

void TRFrameResolverSetTruncationMode(TRFrameResolverRef resolver, TRBreakMode mode)
{
    resolver->truncationMode = mode;
}

void TRFrameResolverSetTruncationPlace(TRFrameResolverRef resolver, TRTruncationPlace place)
{
    resolver->isTruncationEnabled = TRTrue;
    resolver->truncationPlace = place;
}

void TRFrameResolverDisableTruncation(TRFrameResolverRef resolver)
{
    resolver->isTruncationEnabled = TRFalse;
}

void TRFrameResolverSetJustificationEnabled(TRFrameResolverRef resolver, TRBoolean isEnabled)
{
    resolver->isJustificationEnabled = isEnabled;
}

void TRFrameResolverSetJustificationLevel(TRFrameResolverRef resolver, TRFloat level)
{
    resolver->justificationLevel = level;
}

void TRFrameResolverSetMaxLines(TRFrameResolverRef resolver, TRUInteger maxLines)
{
    resolver->maxLines = maxLines;
}

void TRFrameResolverSetExtraLineSpacing(TRFrameResolverRef resolver, TRFloat spacing)
{
    resolver->extraLineSpacing = spacing;
}

void TRFrameResolverSetLineHeightMultiplier(TRFrameResolverRef resolver, TRFloat multiplier)
{
    resolver->lineHeightMultiplier = multiplier;
}

TRComposedFrameRef TRFrameResolverCreateFrame(TRFrameResolverRef resolver, TRRange range)
{
    TRTypesetterRef typesetter = resolver->typesetter;
    TRComposedFrameRef composedFrame = NULL;
    TRUInteger rangeEnd = range.index + range.length;
    FrameContext context;

    /* The resolver MUST have a typesetter, and the range MUST NOT be empty or past the text. */
    TRAssert(typesetter != NULL && range.length > 0 && rangeEnd <= typesetter->buffer.length);

    memset(&context, 0, sizeof(FrameContext));
    context.layoutWidth = Clamp(resolver->frameWidth);
    context.layoutHeight = Clamp(resolver->frameHeight);
    context.maxLines = (resolver->maxLines ? resolver->maxLines : (TRUInteger)(-1));
    context.endIndex = rangeEnd;
    ArrayInitialize(&context.lines, sizeof(TRComposedLine *));

    ResolveFrameParagraphs(&context, resolver, range);

    if (!context.hasFailed) {
        ResolveTruncation(&context, resolver, rangeEnd);
    }

    if (context.hasFailed) {
        FinalizeContext(&context);
    } else {
        TRUInteger frameEnd;

        ResolveAlignments(&context, resolver);
        ResolveJustification(&context, resolver);

        /* The frame ends where its last line does, unless that line is cut out of the range. */
        frameEnd = (context.isTruncated
                    ? context.endIndex
                    : GetLine(&context, GetLineCount(&context) - 1)->codeUnitEnd);

        composedFrame = TRComposedFrameCreate(range.index, frameEnd, ArrayGetItems(&context.lines),
            GetLineCount(&context), context.layoutWidth, context.layoutHeight);
        ArrayFinalize(&context.lines);
    }

    return composedFrame;
}

TRFrameResolverRef TRFrameResolverRetain(TRFrameResolverRef resolver)
{
    return ObjectRetain((ObjectRef)resolver);
}

void TRFrameResolverRelease(TRFrameResolverRef resolver)
{
    ObjectRelease((ObjectRef)resolver);
}

