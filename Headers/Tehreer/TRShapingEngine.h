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

#ifndef _TEHREER_SHAPING_ENGINE_H
#define _TEHREER_SHAPING_ENGINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRFontFeatures.h>
#include <Tehreer/TRShapingResult.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/**
 * The direction in which text is written.
 */
enum {
    TRWritingDirectionLeftToRight = 0,  /**< Text is written from left to right. */
    TRWritingDirectionRightToLeft = 1   /**< Text is written from right to left. */
};
typedef TRUInt32 TRWritingDirection;

/**
 * The order in which text is shaped, relative to the order of the code units.
 */
enum {
    TRShapingOrderForward = 0,  /**< Text is shaped from the first code unit to the last. */
    TRShapingOrderBackward = 1  /**< Text is shaped from the last code unit to the first. */
};
typedef TRUInt32 TRShapingOrder;

/**
 * A shaping engine converts runs of text into glyphs by using the properties that were set on it.
 * An engine can shape any number of runs, but it must not be used from multiple threads at once.
 */
typedef struct _TRShapingEngine *TRShapingEngineRef;

/**
 * Returns the direction that a script is written in by default.
 *
 * @param scriptTag
 *      The OpenType tag of the script, e.g. `TRTagMake('a', 'r', 'a', 'b')`.
 */
TR_PUBLIC TRWritingDirection TRShapingEngineGetScriptDefaultDirection(TRTag scriptTag);

/**
 * Returns the OpenType tag of a language that is given by its name.
 *
 * @param languageName
 *      The null-terminated name of the language in the form of BCP 47, e.g. `"ur-PK"`.
 * @return
 *      The tag of the language, e.g. `TRTagMake('U', 'R', 'D', ' ')`, or `TRTagMake('d', 'f', 'l',
 *      't')`, which is the default language, if the name is `NULL` or has no tag.
 */
TR_PUBLIC TRTag TRShapingEngineGetLanguageTag(const char *languageName);

/**
 * Creates a shaping engine with a type size of 16, the `DFLT` script, the `dflt` language, left to
 * right direction and forward order. The typeface has to be set before shaping.
 *
 * @return
 *      New shaping engine, or `NULL` on failure.
 */
TR_PUBLIC TRShapingEngineRef TRShapingEngineCreate(void);

/**
 * Sets the typeface to shape with. The engine retains it.
 */
TR_PUBLIC void TRShapingEngineSetTypeface(TRShapingEngineRef engine, TRTypefaceRef typeface);

/**
 * Sets the size of the em square that the results are measured in.
 */
TR_PUBLIC void TRShapingEngineSetTypeSize(TRShapingEngineRef engine, TRFloat typeSize);

/**
 * Sets the OpenType tag of the script of the text.
 */
TR_PUBLIC void TRShapingEngineSetScriptTag(TRShapingEngineRef engine, TRTag scriptTag);

/**
 * Sets the OpenType tag of the language of the text.
 */
TR_PUBLIC void TRShapingEngineSetLanguageTag(TRShapingEngineRef engine, TRTag languageTag);

/**
 * Sets the direction in which the text is written.
 */
TR_PUBLIC void TRShapingEngineSetWritingDirection(TRShapingEngineRef engine,
    TRWritingDirection writingDirection);

/**
 * Sets the order in which the text is shaped. Backward order means that the code units run against
 * the writing direction, as in right-to-left text that is embedded in left-to-right text.
 */
TR_PUBLIC void TRShapingEngineSetShapingOrder(TRShapingEngineRef engine,
    TRShapingOrder shapingOrder);

/**
 * Sets the OpenType features to apply on the whole text. The features are copied.
 *
 * @param engine
 *      The shaping engine.
 * @param features
 *      The features, or `NULL` to remove them all.
 * @param count
 *      Number of features.
 * @return
 *      `TRFalse` if the features could not be stored; the previous ones are kept.
 */
TR_PUBLIC TRBoolean TRShapingEngineSetOpenTypeFeatures(TRShapingEngineRef engine,
    const TROpenTypeFeature *features, TRUInteger count);

/**
 * Shapes a run of text into glyphs.
 *
 * @param engine
 *      The shaping engine.
 * @param codeUnits
 *      The code units of the text.
 * @param length
 *      Number of code units.
 * @param encoding
 *      The encoding of the code units.
 * @return
 *      New shaping result, or `NULL` if no typeface was set, the arguments are invalid or on
 *      failure.
 */
TR_PUBLIC TRShapingResultRef TRShapingEngineCreateShapingResult(TRShapingEngineRef engine,
    const void *codeUnits, TRUInteger length, TRStringEncoding encoding);

/**
 * Retains the shaping engine.
 */
TR_PUBLIC TRShapingEngineRef TRShapingEngineRetain(TRShapingEngineRef engine);

/**
 * Releases the shaping engine.
 */
TR_PUBLIC void TRShapingEngineRelease(TRShapingEngineRef engine);

TR_EXTERN_C_END

#endif
