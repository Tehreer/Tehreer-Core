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
#include <Core/Allocator.h>
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

    ComposedLineRef *lines;
    TRUInteger lineCount;
    TRUInteger lineCapacity;
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
    FrameResolverRef resolver = object;

    if (resolver->typesetter) {
        TRTypesetterRelease(resolver->typesetter);
    }
}

TRFrameResolverRef TRFrameResolverCreate(void)
{
    const TRUInteger size = sizeof(TRFrameResolver);
    void *pointer = NULL;
    FrameResolverRef resolver = ObjectCreate(&size, 1, &pointer, FinalizeFrameResolver);

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

TRFrameResolverRef TRFrameResolverRetain(TRFrameResolverRef resolver)
{
    return ObjectRetain((ObjectRef)resolver);
}

void TRFrameResolverRelease(TRFrameResolverRef resolver)
{
    ObjectRelease((ObjectRef)resolver);
}

static TRFloat Clamp(TRFloat value)
{
    if (!(value > 0.0f)) {
        return 0.0f;
    }

    return (value < MaxFloat ? value : MaxFloat);
}

static TRFloat GetFlushFactor(TRTextAlignment alignment, TRUInt8 baseLevel)
{
    TRBoolean isRTL = (baseLevel & 1) == 1;

    switch (alignment) {
    case TRTextAlignmentLeft:
        return 0.0f;

    case TRTextAlignmentCenter:
        return 0.5f;

    case TRTextAlignmentRight:
        return 1.0f;

    case TRTextAlignmentLeading:
        return (isRTL ? 1.0f : 0.0f);

    case TRTextAlignmentTrailing:
        return (isRTL ? 0.0f : 1.0f);
    }

    return 0.0f;
}

static TRFloat GetVerticalMultiplier(TRVerticalAlignment alignment)
{
    switch (alignment) {
    case TRVerticalAlignmentCenter:
        return 0.5f;

    case TRVerticalAlignmentBottom:
        return 1.0f;
    }

    return 0.0f;
}

static void AppendLine(FrameContext *context, ComposedLineRef line)
{
    if (context->lineCount == context->lineCapacity) {
        TRUInteger capacity = (context->lineCapacity ? context->lineCapacity * 2 : 8);
        ComposedLineRef *lines = AllocatorReallocateBlock(context->lines,
            capacity * sizeof(ComposedLineRef));

        if (!lines) {
            TRComposedLineRelease((TRComposedLineRef)line);
            context->hasFailed = TRTrue;
            return;
        }

        context->lines = lines;
        context->lineCapacity = capacity;
    }

    context->lines[context->lineCount++] = line;
}

static void LoadParagraphStyle(FrameContext *context, TRTextRef text, TRUInteger index)
{
    ParagraphStyle *style = &context->style;
    TRAttributeListRef list;
    TRUInteger length;
    TRUInteger count;
    TRUInteger itemIndex;

    memset(style, 0, sizeof(ParagraphStyle));
    style->firstIndentLineCount = 1;

    /* The paragraph attributes are the same for the whole paragraph. */
    list = TRTextGetAttributes(text, index, &length);
    if (!list) {
        return;
    }

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
static void SetupParagraph(FrameContext *context, FrameResolverRef resolver,
    TRUInteger paragraphIndex, TRUInteger segmentStart, TRUInteger segmentEnd)
{
    TypesetterRef typesetter = (TypesetterRef)resolver->typesetter;
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

static void ResolveLineStyle(const ParagraphStyle *style, ComposedLineRef line)
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

static void ResolveAttributes(FrameContext *context, FrameResolverRef resolver,
    ComposedLineRef line)
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
        + TRComposedLineGetPenOffset((TRComposedLineRef)line, context->flushFactor,
            context->lineExtent);
    line->origin.y = context->occupiedHeight + line->ascent;
    line->intrinsicMargin = context->layoutWidth - context->lineExtent;
    line->flushFactor = context->flushFactor;
}

static void ComputeOccupiedSize(const FrameContext *context, ComposedLineRef line,
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

static void ResolveParagraphLines(FrameContext *context, FrameResolverRef resolver)
{
    TypesetterRef typesetter = (TypesetterRef)resolver->typesetter;
    const ParagraphInfo *paragraph = &typesetter->paragraphs[context->paragraphIndex];
    TRUInteger lineIndex = 0;
    TRUInteger lineStart = context->startIndex;

    /* Resolve `paragraphSpacingBefore` if it is not the first paragraph. */
    if (paragraph->start > 0) {
        context->occupiedHeight += context->style.spacingBefore;
    }

    /* Iterate over each line of this paragraph. */
    while (lineStart != context->endIndex) {
        TRUInteger lineEnd;
        ComposedLineRef line;
        TRFloat width, height;

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
            return;
        }
        ResolveAttributes(context, resolver, line);

        /* Make sure that at least one line is added even if frame is smaller in height. */
        ComputeOccupiedSize(context, line, &width, &height);
        if (height > resolver->frameHeight && context->lineCount > 0) {
            TRComposedLineRelease((TRComposedLineRef)line);
            context->isFilled = TRTrue;
            return;
        }

        /* Append the line, and update the occupied size. */
        AppendLine(context, line);
        if (context->hasFailed) {
            return;
        }
        ResolveOccupiedSize(context, width, height);

        /* Stop the filling process if maximum lines have been added. */
        if (context->lineCount == context->maxLines) {
            context->isFilled = TRTrue;
            return;
        }

        lineIndex++;
        lineStart = lineEnd;
    }

    /* Resolve `paragraphSpacing` if it is not the last paragraph. */
    if (context->endIndex < typesetter->buffer.length) {
        context->occupiedHeight += context->style.spacing;
    }
}

static void ResolveTruncation(FrameContext *context, FrameResolverRef resolver,
    TRUInteger frameEnd)
{
    TypesetterRef typesetter = (TypesetterRef)resolver->typesetter;
    ComposedLineRef lastLine;
    ComposedLineRef token;
    ComposedLineRef line;
    TRUInteger lastStart;
    TRUInteger paragraphIndex;
    TRUInteger segmentStart;
    TRUInteger lineIndex;
    TRUInteger index;
    TRFloat width, height;

    if (!resolver->isTruncationEnabled || context->lineCount == 0) {
        return;
    }

    lastLine = context->lines[context->lineCount - 1];
    lastStart = lastLine->codeUnitStart;

    /* No need to truncate if frame range is already covered. */
    if (lastLine->codeUnitEnd == frameEnd) {
        return;
    }

    /* The last line might be in a paragraph before the one that the filling stopped in. */
    paragraphIndex = TypesetterFindParagraph(typesetter, lastStart);
    segmentStart = typesetter->paragraphs[paragraphIndex].start;
    if (paragraphIndex == context->paragraphIndex) {
        segmentStart = context->startIndex;
    } else if (lastLine->codeUnitStart < segmentStart) {
        segmentStart = lastLine->codeUnitStart;
    }

    lineIndex = 0;
    for (index = 0; index + 1 < context->lineCount; index++) {
        TRUInteger start = context->lines[index]->codeUnitStart;

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
    if (!token) {
        context->hasFailed = TRTrue;
        return;
    }

    line = LineResolverCreateTruncatedLine(typesetter, lastStart, frameEnd, context->lineExtent,
        resolver->truncationMode, resolver->truncationPlace, token);
    TRComposedLineRelease((TRComposedLineRef)token);
    if (!line) {
        context->hasFailed = TRTrue;
        return;
    }
    ResolveAttributes(context, resolver, line);

    /* Replace the line and update the occupied size. */
    TRComposedLineRelease((TRComposedLineRef)lastLine);
    context->lines[context->lineCount - 1] = line;
    context->isTruncated = TRTrue;

    ComputeOccupiedSize(context, line, &width, &height);
    ResolveOccupiedSize(context, width, height);
}

static void ResolveAlignments(FrameContext *context, FrameResolverRef resolver)
{
    TRUInteger index;

    if (resolver->fitsVertically) {
        /* Update the layout height to occupied height. */
        context->layoutHeight = context->occupiedHeight;
    } else {
        /* Find out the additional top for vertical alignment. */
        TRFloat extraHeight = context->layoutHeight - context->occupiedHeight;
        TRFloat additionalTop = extraHeight * GetVerticalMultiplier(resolver->verticalAlignment);

        /* Readjust the vertical position of each line. */
        for (index = 0; index < context->lineCount; index++) {
            context->lines[index]->origin.y += additionalTop;
        }
    }

    if (resolver->fitsHorizontally) {
        TRFloat extraWidth = context->layoutWidth - context->occupiedWidth;

        /* Readjust the horizontal position of each line. */
        for (index = 0; index < context->lineCount; index++) {
            ComposedLineRef line = context->lines[index];
            line->origin.x -= extraWidth * line->flushFactor;
        }

        /* Update the layout width to occupied width. */
        context->layoutWidth = context->occupiedWidth;
    }
}

static TRBoolean EndsBeforeBlock(TypesetterRef typesetter, TRUInteger codeUnitEnd)
{
    if (codeUnitEnd >= typesetter->buffer.length) {
        return TRFalse;
    }

    return TextRunIsBlock(typesetter->runs[TypesetterFindRun(typesetter, codeUnitEnd)]);
}

static void ResolveJustification(FrameContext *context, FrameResolverRef resolver)
{
    TypesetterRef typesetter = (TypesetterRef)resolver->typesetter;
    TRUInteger index;

    if (!resolver->isJustificationEnabled) {
        return;
    }

    for (index = 0; index < context->lineCount; index++) {
        ComposedLineRef line = context->lines[index];
        ComposedLineRef justified;
        TRUInteger paragraphIndex;
        TRFloat availableWidth;
        TRFloat alignedLeft;
        TRFloat marginalLeft = 0.0f;

        /* Skip the last line of paragraph if it's smaller in width. */
        paragraphIndex = TypesetterFindParagraph(typesetter, line->codeUnitEnd - 1);
        if (typesetter->paragraphs[paragraphIndex].end == line->codeUnitEnd) {
            continue;
        }

        /* The line that shows a token cannot be made again from its text. */
        if (line->isTruncated) {
            continue;
        }

        /* The line of a view has nothing to justify, and the one before it ends there. */
        if (line->isBlock || EndsBeforeBlock(typesetter, line->codeUnitEnd)) {
            continue;
        }

        /* The line is justified to what is left after the margins of its paragraph. */
        availableWidth = context->layoutWidth - line->intrinsicMargin;

        justified = LineResolverCreateJustifiedLine(typesetter, line->codeUnitStart,
            line->codeUnitEnd, resolver->justificationLevel, availableWidth);
        if (!justified) {
            /* The line is shown as it is. */
            continue;
        }

        alignedLeft = TRComposedLineGetPenOffset((TRComposedLineRef)justified, line->flushFactor,
            availableWidth);

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

        TRComposedLineRelease((TRComposedLineRef)line);
        context->lines[index] = justified;
    }
}

static void FinalizeContext(FrameContext *context)
{
    TRUInteger index;

    for (index = 0; index < context->lineCount; index++) {
        TRComposedLineRelease((TRComposedLineRef)context->lines[index]);
    }

    AllocatorDeallocateBlock(context->lines);
}

TRComposedFrameRef TRFrameResolverCreateFrame(TRFrameResolverRef resolver, TRRange range)
{
    TypesetterRef typesetter = (TypesetterRef)resolver->typesetter;
    TRUInteger rangeEnd = range.index + range.length;
    FrameContext context;
    ComposedFrameRef frame;
    TRUInteger frameEnd;

    /* The resolver MUST have a typesetter, and the range MUST be within the text. */
    TRAssert(typesetter != NULL && rangeEnd <= typesetter->buffer.length);

    memset(&context, 0, sizeof(FrameContext));
    context.layoutWidth = Clamp(resolver->frameWidth);
    context.layoutHeight = Clamp(resolver->frameHeight);
    context.maxLines = (resolver->maxLines ? resolver->maxLines : (TRUInteger)(-1));
    context.endIndex = rangeEnd;

    if (range.length > 0) {
        TRUInteger paragraphIndex = TypesetterFindParagraph(typesetter, range.index);
        TRUInteger segmentStart = range.index;

        /* Iterate over all paragraphs in provided range. */
        do {
            const ParagraphInfo *paragraph = &typesetter->paragraphs[paragraphIndex];
            TRUInteger segmentEnd = (rangeEnd < paragraph->end ? rangeEnd : paragraph->end);

            SetupParagraph(&context, resolver, paragraphIndex, segmentStart, segmentEnd);
            ResolveParagraphLines(&context, resolver);

            if (context.isFilled || context.hasFailed) {
                break;
            }

            segmentStart = segmentEnd;
            paragraphIndex++;
        } while (segmentStart < rangeEnd);

        if (!context.hasFailed) {
            ResolveTruncation(&context, resolver, rangeEnd);
        }
    }

    if (context.hasFailed) {
        FinalizeContext(&context);
        return NULL;
    }

    ResolveAlignments(&context, resolver);
    ResolveJustification(&context, resolver);

    /* The frame ends where its last line does, unless that line is cut out of the range. */
    frameEnd = (context.isTruncated || context.lineCount == 0
                ? context.endIndex
                : context.lines[context.lineCount - 1]->codeUnitEnd);

    frame = ComposedFrameCreate(range.index, frameEnd, context.lines, context.lineCount,
        context.layoutWidth, context.layoutHeight);
    AllocatorDeallocateBlock(context.lines);

    return (TRComposedFrameRef)frame;
}
