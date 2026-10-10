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
#include <string.h>

#include <hb.h>
#include <hb-ot.h>

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRFontFeatures.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRShapingResult.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRSheenBidi.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRAttributeList.h>
#include <API/TRBase.h>
#include <API/TRFontFeatures.h>
#include <API/TRTypesetter.h>
#include <Core/Allocator.h>
#include <Layout/TextRun.h>
#include <Text/AttributeRegistry.h>

#include "ShapeResolver.h"

typedef struct _ShapingStyle {
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat baselineOffset;
    TRFloat obliqueness;
    TRFloat scaleX;
    TRFloat scaleY;
    TRReplacementRef replacement;
    TRTag languageTag;
    TRFontFeaturesRef fontFeatures;
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
    TRTypesetter *typesetter;
    TRShapingEngineRef engine;
    TRUInteger unitSize;
    ShapingStyle defaultStyle;
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
    style->languageTag = 0;
    style->fontFeatures = NULL;
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
        case TRAttributeTypeSize:
            style->typeSize = item->value.typeSize;
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
        case TRAttributeLanguage:
            style->languageTag = item->value.language;
            break;
        case TRAttributeFontFeatures:
            style->fontFeatures = item->value.fontFeatures;
            break;
        }
    }

    if (style->typeSize < 0.0f) {
        style->typeSize = 0.0f;
    }
}

static TRBoolean EqualFontFeatures(TRFontFeaturesRef first, TRFontFeaturesRef second)
{
    TRBoolean isEqual = (first == second);

    if (!isEqual && first && second) {
        isEqual = TRFontFeaturesIsEqual(first, second);
    }

    return isEqual;
}

static TRBoolean EqualStyles(const ShapingStyle *first, const ShapingStyle *second)
{
    return first->typeface == second->typeface
        && first->typeSize == second->typeSize
        && first->baselineOffset == second->baselineOffset
        && first->obliqueness == second->obliqueness
        && first->scaleX == second->scaleX
        && first->scaleY == second->scaleY
        && first->replacement == second->replacement
        && first->languageTag == second->languageTag
        && EqualFontFeatures(first->fontFeatures, second->fontFeatures);
}

static TRBoolean AppendRun(ResolverContext *context, TextRunRef textRun)
{
    return ArrayAppend(&context->typesetter->runs, &textRun);
}

/* Sets the features of the style on the engine, which are none if the style has no features. */
static TRBoolean SetupFeatures(TRShapingEngineRef engine, const ShapingStyle *style)
{
    const TROpenTypeFeature *features = NULL;
    TRUInteger count = 0;

    if (style->fontFeatures) {
        features = TRFontFeaturesGetItemsPtr(style->fontFeatures);
        count = TRFontFeaturesGetCount(style->fontFeatures);
    }

    return TRShapingEngineSetOpenTypeFeatures(engine, features, count);
}

static TextRunRef ShapeIntrinsicRun(ResolverContext *context, const PendingRun *pending)
{
    TextRunRef textRun = NULL;
    TRTypesetter *typesetter = context->typesetter;
    const ShapingStyle *style = &pending->style;
    TRTag scriptTag = GetScriptTag(pending->script);
    TRTag languageTag = (style->languageTag ? style->languageTag : TRTagMake('d', 'f', 'l', 't'));
    TRWritingDirection direction = TRShapingEngineGetScriptDefaultDirection(scriptTag);
    TRBoolean isRTL = (pending->level & 1) == 1;
    TRBoolean isBackward = (isRTL && direction == TRWritingDirectionLeftToRight)
                        || (!isRTL && direction == TRWritingDirectionRightToLeft);
    const TRUInt8 *units = (const TRUInt8 *)typesetter->buffer.codeUnits;
    TRShapingResultRef shapingResult = NULL;

    TRShapingEngineSetTypeface(context->engine, style->typeface);
    TRShapingEngineSetTypeSize(context->engine, style->typeSize);
    TRShapingEngineSetScriptTag(context->engine, scriptTag);
    TRShapingEngineSetLanguageTag(context->engine, languageTag);
    TRShapingEngineSetWritingDirection(context->engine, direction);
    TRShapingEngineSetShapingOrder(context->engine,
        isBackward ? TRShapingOrderBackward : TRShapingOrderForward);

    if (SetupFeatures(context->engine, style)) {
        shapingResult = TRShapingEngineCreateShapingResult(context->engine,
            units + (pending->start * context->unitSize), pending->end - pending->start,
            typesetter->buffer.encoding);
    }

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
static void ResolveStyle(const ResolverContext *context, TRAttributeListRef attributes,
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
    const SBParagraphInfo *paragraph)
{
    TRBoolean isResolved = TRTrue;
    PendingRun pending;

    pending.isValid = TRFalse;

    SBUniformRunIteratorReset(iterator, paragraph->index, paragraph->length);

    while (SBUniformRunIteratorMoveNext(iterator)) {
        const SBUniformRun *uniformRun = SBUniformRunIteratorGetCurrent(iterator);
        ShapingStyle style;

        ResolveStyle(context, TRAttributeListMake(uniformRun->attributes), &style);

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

static TRBoolean ResolveAllParagraphs(ResolverContext *context, SBUniformRunIteratorRef runIterator,
    SBParagraphIteratorRef paragraphIterator)
{
    TRBoolean isResolved = TRTrue;

    /* Only the attributes of shaping decide the runs, not the paragraph ones nor the paint ones. */
    SBUniformRunIteratorSetupFilter(runIterator,
        SBAttributeFilterMakeCollection(AttributeGroupShaping, SBAttributeScopeCharacter));

    while (isResolved && SBParagraphIteratorMoveNext(paragraphIterator)) {
        isResolved = ResolveParagraph(context, runIterator,
            SBParagraphIteratorGetCurrent(paragraphIterator));
    }

    return isResolved;
}

TR_INTERNAL TRBoolean ShapeResolverResolve(TRTypesetter *typesetter,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount)
{
    SBTextRef sbText = TRTextGetSheenBidiText(typesetter->text);
    SBParagraphIteratorRef paragraphIterator = SBTextCreateParagraphIterator(sbText);
    SBUniformRunIteratorRef runIterator = SBTextCreateUniformRunIterator(sbText);
    TRShapingEngineRef engine = TRShapingEngineCreate();
    TRBoolean isResolved = TRFalse;

    if (paragraphIterator && runIterator && engine) {
        ResolverContext context;

        context.typesetter = typesetter;
        context.engine = engine;
        context.unitSize = (typesetter->buffer.encoding == TRStringEncodingUTF8 ? 1
                            : (typesetter->buffer.encoding == TRStringEncodingUTF16 ? 2 : 4));

        InitializeStyle(&context.defaultStyle);
        if (defaultAttributes) {
            ApplyAttributes(&context.defaultStyle, defaultAttributes, defaultAttributeCount);
        }

        isResolved = ResolveAllParagraphs(&context, runIterator, paragraphIterator);
    }

    if (engine) {
        TRShapingEngineRelease(engine);
    }
    if (runIterator) {
        SBUniformRunIteratorRelease(runIterator);
    }
    if (paragraphIterator) {
        SBParagraphIteratorRelease(paragraphIterator);
    }

    return isResolved;
}
