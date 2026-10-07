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
#include <stdlib.h>

#include <hb.h>
#include <hb-ot.h>

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRShapingResult.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <API/TRTypesetter.h>
#include <Core/Allocator.h>
#include <Layout/TextRun.h>

#include "ShapeResolver.h"

typedef struct _ShapingStyle {
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat baselineOffset;
    TRFloat obliqueness;
    TRFloat scaleX;
    TRFloat scaleY;
    TRReplacementRef replacement;
} ShapingStyle;

typedef struct _PendingRun {
    TRBoolean isValid;
    TRUInteger start;
    TRUInteger end;
    TRUInt8 level;
    SBScript script;
    ShapingStyle style;
} PendingRun;

typedef struct _ResolverContext {
    TypesetterRef typesetter;
    TRShapingEngineRef engine;
    TRUInteger unitSize;
    ShapingStyle defaultStyle;
    TRUInteger runCapacity;
} ResolverContext;

/* Returns the OpenType tag of a script, which is the newest one if the script has more. */
static TRTag GetScriptTag(SBScript script)
{
    hb_script_t hbScript = hb_script_from_iso15924_tag(SBScriptGetUnicodeTag(script));
    unsigned int count = HB_OT_MAX_TAGS_PER_SCRIPT;
    hb_tag_t tags[HB_OT_MAX_TAGS_PER_SCRIPT];

    hb_ot_tags_from_script_and_language(hbScript, HB_LANGUAGE_INVALID, &count, tags, NULL, NULL);

    return (count > 0 ? tags[0] : TRTagMake('D', 'F', 'L', 'T'));
}

static void InitializeStyle(ShapingStyle *style)
{
    style->typeface = NULL;
    style->typeSize = 16.0f;
    style->baselineOffset = 0.0f;
    style->obliqueness = 0.0f;
    style->scaleX = 1.0f;
    style->scaleY = 1.0f;
    style->replacement = NULL;
}

static void ApplyAttributes(ShapingStyle *style, const TRAttribute *items, TRUInteger count)
{
    TRUInteger index;

    for (index = 0; index < count; index++) {
        const TRAttribute *item = &items[index];

        switch (item->type) {
        case TRAttributeTypeface:
            style->typeface = item->value.typeface;
            break;
        case TRAttributePointSize:
            style->typeSize = item->value.pointSize;
            break;
        case TRAttributeScaleX:
            style->scaleX = item->value.scaleX;
            break;
        case TRAttributeScaleY:
            style->scaleY = item->value.scaleY;
            break;
        case TRAttributeBaselineOffset:
            style->baselineOffset = item->value.baselineOffset;
            break;
        case TRAttributeObliqueness:
            style->obliqueness = item->value.obliqueness;
            break;
        case TRAttributeReplacement:
            style->replacement = item->value.replacement;
            break;
        }
    }

    if (style->typeSize < 0.0f) {
        style->typeSize = 0.0f;
    }
}

static TRBoolean EqualStyles(const ShapingStyle *first, const ShapingStyle *second)
{
    return first->typeface == second->typeface
        && first->typeSize == second->typeSize
        && first->baselineOffset == second->baselineOffset
        && first->obliqueness == second->obliqueness
        && first->scaleX == second->scaleX
        && first->scaleY == second->scaleY
        && first->replacement == second->replacement;
}

static TRBoolean AppendRun(ResolverContext *context, TextRunRef textRun)
{
    TRBoolean isAppended = TRTrue;
    TypesetterRef typesetter = context->typesetter;

    if (typesetter->runCount == context->runCapacity) {
        TRUInteger newCapacity;
        TextRunRef *newRuns;

        newCapacity = (context->runCapacity == 0 ? 16 : context->runCapacity * 2);
        newRuns = AllocatorReallocateBlock(typesetter->runs, newCapacity * sizeof(TextRunRef));

        if (newRuns) {
            typesetter->runs = newRuns;
            context->runCapacity = newCapacity;
        } else {
            isAppended = TRFalse;
        }
    }

    if (isAppended) {
        typesetter->runs[typesetter->runCount++] = textRun;
    }

    return isAppended;
}

static TextRunRef ShapeIntrinsicRun(ResolverContext *context, const PendingRun *pending)
{
    TextRunRef textRun = NULL;
    TypesetterRef typesetter = context->typesetter;
    const ShapingStyle *style = &pending->style;
    TRTag scriptTag = GetScriptTag(pending->script);
    TRWritingDirection direction = TRShapingEngineGetScriptDefaultDirection(scriptTag);
    TRBoolean isRTL = (pending->level & 1) == 1;
    TRBoolean isBackward = (isRTL && direction == TRWritingDirectionLeftToRight)
                        || (!isRTL && direction == TRWritingDirectionRightToLeft);
    const TRUInt8 *units = (const TRUInt8 *)typesetter->buffer.codeUnits;
    TRShapingResultRef shapingResult;

    TRShapingEngineSetTypeface(context->engine, style->typeface);
    TRShapingEngineSetTypeSize(context->engine, style->typeSize);
    TRShapingEngineSetScriptTag(context->engine, scriptTag);
    TRShapingEngineSetWritingDirection(context->engine, direction);
    TRShapingEngineSetShapingOrder(context->engine,
        isBackward ? TRShapingOrderBackward : TRShapingOrderForward);

    shapingResult = TRShapingEngineShape(context->engine,
        units + (pending->start * context->unitSize), pending->end - pending->start,
        typesetter->buffer.encoding);

    if (shapingResult) {
        textRun = TextRunCreateIntrinsic(pending->start, pending->end, pending->level,
            style->typeface, style->typeSize, style->scaleX, style->scaleY,
            style->baselineOffset, shapingResult, direction);

        TRShapingResultRelease(shapingResult);
    }

    return textRun;
}

static TRBoolean ShapeRun(ResolverContext *context, const PendingRun *pending)
{
    TRBoolean isShaped = TRFalse;
    const ShapingStyle *style = &pending->style;

    if (style->typeface) {
        TextRunRef textRun = NULL;

        if (style->replacement) {
            /* The room is provisional here, as the frame that a line is in decides it. */
            textRun = TextRunCreateReplacement(pending->start, pending->end, pending->level,
                style->replacement, style->typeface, style->typeSize, 0.0f);
        } else {
            textRun = ShapeIntrinsicRun(context, pending);
        }

        if (textRun) {
            if (AppendRun(context, textRun)) {
                isShaped = TRTrue;
            } else {
                TextRunRelease(textRun);
            }
        }
    }

    return isShaped;
}

/* Gets the style of a uniform run from the attributes of its characters. */
static void ResolveStyle(const ResolverContext *context, SBAttributeListRef attributes,
    ShapingStyle *style)
{
    TRUInteger count = TRAttributeListGetCount(attributes);
    TRUInteger index;

    *style = context->defaultStyle;

    for (index = 0; index < count; index++) {
        ApplyAttributes(style, TRAttributeListGetItem(attributes, index), 1);
    }
}

static TRBoolean ResolveParagraph(ResolverContext *context, SBUniformRunIteratorRef iterator,
    const ParagraphInfo *paragraph)
{
    TRBoolean isResolved = TRTrue;
    PendingRun pending;

    pending.isValid = TRFalse;

    SBUniformRunIteratorReset(iterator, paragraph->start, paragraph->end - paragraph->start);

    while (SBUniformRunIteratorMoveNext(iterator)) {
        const SBUniformRun *uniformRun = SBUniformRunIteratorGetCurrent(iterator);
        ShapingStyle style;

        ResolveStyle(context, uniformRun->attributes, &style);

        /* Runs that only differ in what is not about shaping are shaped together. */
        if (pending.isValid && pending.level == uniformRun->level
                && pending.script == uniformRun->script && EqualStyles(&pending.style, &style)) {
            pending.end = uniformRun->index + uniformRun->length;
            continue;
        }

        if (pending.isValid) {
            isResolved = ShapeRun(context, &pending);

            if (!isResolved) {
                break;
            }
        }

        pending.isValid = TRTrue;
        pending.start = uniformRun->index;
        pending.end = uniformRun->index + uniformRun->length;
        pending.level = uniformRun->level;
        pending.script = uniformRun->script;
        pending.style = style;
    }

    if (isResolved && pending.isValid) {
        isResolved = ShapeRun(context, &pending);
    }

    return isResolved;
}

static TRBoolean ResolveAllParagraphs(ResolverContext *context, SBUniformRunIteratorRef iterator)
{
    TRBoolean isResolved = TRTrue;
    TypesetterRef typesetter = context->typesetter;
    TRUInteger index;

    /* Only what the characters set decides the runs; paragraph attributes are not run attributes. */
    SBUniformRunIteratorSetupFilter(iterator,
        SBAttributeFilterMakeCollection(SBAttributeGroupNone, SBAttributeScopeCharacter));

    for (index = 0; index < typesetter->paragraphCount; index++) {
        isResolved = ResolveParagraph(context, iterator, &typesetter->paragraphs[index]);

        if (!isResolved) {
            break;
        }
    }

    return isResolved;
}

static TRBoolean AppendParagraph(TypesetterRef typesetter, TRUInteger *capacity,
    const SBParagraphInfo *info)
{
    TRBoolean isAppended = TRTrue;

    if (typesetter->paragraphCount == *capacity) {
        ParagraphInfo *newParagraphs;
        TRUInteger newCapacity;

        newCapacity = (*capacity == 0 ? 8 : *capacity * 2);
        newParagraphs = AllocatorReallocateBlock(typesetter->paragraphs,
            newCapacity * sizeof(ParagraphInfo));

        if (newParagraphs) {
            typesetter->paragraphs = newParagraphs;
            *capacity = newCapacity;
        } else {
            isAppended = TRFalse;
        }
    }

    if (isAppended) {
        ParagraphInfo *paragraph;

        paragraph = &typesetter->paragraphs[typesetter->paragraphCount++];
        paragraph->start = info->index;
        paragraph->end = info->index + info->length;
        paragraph->baseLevel = info->baseLevel;
    }

    return isAppended;
}

static TRBoolean ResolveParagraphs(TypesetterRef typesetter)
{
    TRBoolean isResolved = TRFalse;
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    SBParagraphIteratorRef iterator = SBTextCreateParagraphIterator(sbText);

    if (iterator) {
        TRUInteger capacity = 0;

        isResolved = TRTrue;

        while (SBParagraphIteratorMoveNext(iterator)) {
            isResolved = AppendParagraph(typesetter, &capacity,
                SBParagraphIteratorGetCurrent(iterator));

            if (!isResolved) {
                break;
            }
        }

        SBParagraphIteratorRelease(iterator);
    }

    return isResolved;
}

TR_INTERNAL TRBoolean ShapeResolverResolve(TypesetterRef typesetter,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount)
{
    TRBoolean isResolved = ResolveParagraphs(typesetter);

    if (isResolved) {
        ResolverContext context;
        SBUniformRunIteratorRef iterator;

        context.typesetter = typesetter;
        context.runCapacity = 0;
        context.unitSize = (typesetter->buffer.encoding == TRStringEncodingUTF8 ? 1
                            : (typesetter->buffer.encoding == TRStringEncodingUTF16 ? 2 : 4));

        InitializeStyle(&context.defaultStyle);
        if (defaultAttributes) {
            ApplyAttributes(&context.defaultStyle, defaultAttributes, defaultAttributeCount);
        }

        context.engine = TRShapingEngineCreate();
        iterator = SBTextCreateUniformRunIterator(TRTextGetSheenBidiText(typesetter->text));

        if (!context.engine || !iterator) {
            isResolved = TRFalse;
        } else {
            isResolved = ResolveAllParagraphs(&context, iterator);
        }

        if (iterator) {
            SBUniformRunIteratorRelease(iterator);
        }
        if (context.engine) {
            TRShapingEngineRelease(context.engine);
        }
    }

    return isResolved;
}
