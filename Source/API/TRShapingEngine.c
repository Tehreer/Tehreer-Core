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

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <hb.h>
#include <hb-ot.h>

#include <Tehreer/TRString.h>

#include <API/TRBase.h>
#include <API/TRShapingResult.h>
#include <API/TRTypeface.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Graphics/ShapableFace.h>

#include "TRShapingEngine.h"

static void FinalizeShapingEngine(ObjectRef object)
{
    TRShapingEngine *engine = object;

    if (engine->typeface) {
        TRTypefaceRelease(engine->typeface);
    }

    AllocatorDeallocateBlock(engine->features);
}

static void AddCodeUnits(hb_buffer_t *buffer, const void *codeUnits, int length,
    TRStringEncoding encoding)
{
    switch (encoding) {
    case TRStringEncodingUTF8:
        hb_buffer_add_utf8(buffer, codeUnits, length, 0, length);
        break;

    case TRStringEncodingUTF16:
        hb_buffer_add_utf16(buffer, codeUnits, length, 0, length);
        break;

    case TRStringEncodingUTF32:
        hb_buffer_add_utf32(buffer, codeUnits, length, 0, length);
        break;
    }
}

static TRBoolean IsShapingPossible(TRShapingEngineRef engine, const void *codeUnits,
    TRUInteger length, TRStringEncoding encoding)
{
    TRBoolean isPossible = TRFalse;

    if (engine->typeface && (length == 0 || codeUnits) && length <= INT_MAX
            && encoding <= TRStringEncodingUTF32) {
        isPossible = TRTrue;
    }

    return isPossible;
}

static hb_feature_t *CreateHBFeatures(TRShapingEngineRef engine, TRUInteger length)
{
    hb_feature_t *hbFeatures = NULL;

    if (engine->featureCount > 0) {
        hbFeatures = AllocatorAllocateBlock(sizeof(hb_feature_t) * engine->featureCount);
    }

    if (hbFeatures) {
        TRUInteger index;

        for (index = 0; index < engine->featureCount; index++) {
            hbFeatures[index].tag = engine->features[index].tag;
            hbFeatures[index].value = engine->features[index].value;
            hbFeatures[index].start = 0;
            hbFeatures[index].end = (unsigned int)length;
        }
    }

    return hbFeatures;
}

static TRShapingResultRef ShapeCodeUnits(TRShapingEngineRef engine, hb_feature_t *hbFeatures,
    const void *codeUnits, TRUInteger length, TRStringEncoding encoding)
{
    TRTypefaceRef typeface = engine->typeface;
    TRShapingResultRef shapingResult = NULL;
    hb_buffer_t *buffer;
    hb_font_t *hbFont;
    unsigned int ppem;
    TRBoolean isBackward;
    TRBoolean isRTL;
    TRFloat typeSize;

    buffer = hb_buffer_create();
    hb_buffer_set_script(buffer, hb_ot_tag_to_script(engine->scriptTag));
    hb_buffer_set_language(buffer, hb_ot_tag_to_language(engine->languageTag));
    hb_buffer_set_direction(buffer, (engine->writingDirection == TRWritingDirectionRightToLeft
                                     ? HB_DIRECTION_RTL : HB_DIRECTION_LTR));

    if (length > 0) {
        AddCodeUnits(buffer, codeUnits, (int)length, encoding);
    }

    typeSize = (engine->typeSize > 0.0f ? engine->typeSize : 0.0f);
    ppem = (unsigned int)(typeSize + 0.5f);

    /* The sub font keeps the glyph lookups of the typeface, but has a size of its own. */
    hbFont = hb_font_create_sub_font(typeface->shapableFace->hbFont);
    hb_font_set_ppem(hbFont, ppem, ppem);

    hb_shape(hbFont, buffer, hbFeatures, (unsigned int)engine->featureCount);

    hb_font_destroy(hbFont);

    isBackward = (engine->shapingOrder == TRShapingOrderBackward);
    isRTL = (isBackward
             ? engine->writingDirection != TRWritingDirectionRightToLeft
             : engine->writingDirection == TRWritingDirectionRightToLeft);

    if (typeface->unitsPerEM > 0 && hb_buffer_allocation_successful(buffer)) {
        shapingResult = TRShapingResultCreate(buffer, length, typeSize / (TRFloat)typeface->unitsPerEM,
            isBackward, isRTL);
    }

    hb_buffer_destroy(buffer);

    return shapingResult;
}

TRWritingDirection TRShapingEngineGetScriptDefaultDirection(TRTag scriptTag)
{
    TRWritingDirection writingDirection = TRWritingDirectionLeftToRight;
    hb_script_t script = hb_ot_tag_to_script(scriptTag);

    if (hb_script_get_horizontal_direction(script) == HB_DIRECTION_RTL) {
        writingDirection = TRWritingDirectionRightToLeft;
    }

    return writingDirection;
}

TRTag TRShapingEngineGetLanguageTag(const char *languageName)
{
    TRTag languageTag = TRTagMake('d', 'f', 'l', 't');

    if (languageName) {
        hb_language_t language = hb_language_from_string(languageName, -1);
        hb_tag_t tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
        unsigned int count = HB_OT_MAX_TAGS_PER_LANGUAGE;

        hb_ot_tags_from_script_and_language(HB_SCRIPT_INVALID, language, NULL, NULL, &count, tags);

        if (count > 0) {
            languageTag = tags[0];
        }
    }

    return languageTag;
}

TRShapingEngineRef TRShapingEngineCreate(void)
{
    const TRUInteger size = sizeof(TRShapingEngine);
    void *pointer = NULL;
    TRShapingEngine *engine;

    engine = ObjectCreate(&size, 1, &pointer, FinalizeShapingEngine);

    if (engine) {
        engine->typeface = NULL;
        engine->typeSize = 16.0f;
        engine->scriptTag = TRTagMake('D', 'F', 'L', 'T');
        engine->languageTag = TRTagMake('d', 'f', 'l', 't');
        engine->writingDirection = TRWritingDirectionLeftToRight;
        engine->shapingOrder = TRShapingOrderForward;
        engine->features = NULL;
        engine->featureCount = 0;
    }

    return engine;
}

void TRShapingEngineSetTypeface(TRShapingEngineRef engine, TRTypefaceRef typeface)
{
    if (typeface) {
        TRTypefaceRetain(typeface);
    }
    if (engine->typeface) {
        TRTypefaceRelease(engine->typeface);
    }

    engine->typeface = typeface;
}

void TRShapingEngineSetTypeSize(TRShapingEngineRef engine, TRFloat typeSize)
{
    engine->typeSize = typeSize;
}

void TRShapingEngineSetScriptTag(TRShapingEngineRef engine, TRTag scriptTag)
{
    engine->scriptTag = scriptTag;
}

void TRShapingEngineSetLanguageTag(TRShapingEngineRef engine, TRTag languageTag)
{
    engine->languageTag = languageTag;
}

void TRShapingEngineSetWritingDirection(TRShapingEngineRef engine,
    TRWritingDirection writingDirection)
{
    engine->writingDirection = writingDirection;
}

void TRShapingEngineSetShapingOrder(TRShapingEngineRef engine, TRShapingOrder shapingOrder)
{
    engine->shapingOrder = shapingOrder;
}

TRBoolean TRShapingEngineSetOpenTypeFeatures(TRShapingEngineRef engine,
    const TROpenTypeFeature *features, TRUInteger count)
{
    TRBoolean isSet = TRFalse;

    if (!features || count == 0) {
        AllocatorDeallocateBlock(engine->features);
        engine->features = NULL;
        engine->featureCount = 0;

        isSet = TRTrue;
    } else if (count <= (TRUInteger)(-1) / sizeof(TROpenTypeFeature)) {
        TROpenTypeFeature *copy;

        copy = AllocatorAllocateBlock(sizeof(TROpenTypeFeature) * count);

        if (copy) {
            memcpy(copy, features, sizeof(TROpenTypeFeature) * count);

            AllocatorDeallocateBlock(engine->features);
            engine->features = copy;
            engine->featureCount = count;

            isSet = TRTrue;
        }
    }

    return isSet;
}

TRShapingResultRef TRShapingEngineCreateShapingResult(TRShapingEngineRef engine, const void *codeUnits,
    TRUInteger length, TRStringEncoding encoding)
{
    TRShapingResultRef shapingResult = NULL;

    if (IsShapingPossible(engine, codeUnits, length, encoding)) {
        hb_feature_t *hbFeatures;

        hbFeatures = CreateHBFeatures(engine, length);

        /* Without features, a NULL array is valid; otherwise it means allocation failed. */
        if (engine->featureCount == 0 || hbFeatures) {
            shapingResult = ShapeCodeUnits(engine, hbFeatures, codeUnits, length, encoding);
        }

        AllocatorDeallocateBlock(hbFeatures);
    }

    return shapingResult;
}

TRShapingEngineRef TRShapingEngineRetain(TRShapingEngineRef engine)
{
    return ObjectRetain((ObjectRef)engine);
}

void TRShapingEngineRelease(TRShapingEngineRef engine)
{
    ObjectRelease((ObjectRef)engine);
}
