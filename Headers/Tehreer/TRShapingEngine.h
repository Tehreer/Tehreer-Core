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
/**
 * A type to represent the direction in which text is written, which is one of the
 * `TRWritingDirection*` constants above.
 */
typedef TRUInt32 TRWritingDirection;

/**
 * The order in which text is shaped, relative to the order of the code units.
 */
enum {
    TRShapingOrderForward = 0,  /**< Text is shaped from the first code unit to the last. */
    TRShapingOrderBackward = 1  /**< Text is shaped from the last code unit to the first. */
};
/**
 * A type to represent the order in which text is shaped, which is one of the `TRShapingOrder*`
 * constants above.
 */
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
 * @return
 *      `TRWritingDirectionRightToLeft` if the script is written horizontally from right to left,
 *      and `TRWritingDirectionLeftToRight` for any other script, including an unknown one.
 */
TR_PUBLIC TRWritingDirection TRShapingEngineGetScriptDefaultDirection(TRTag scriptTag);

/**
 * Returns the OpenType tag of a language that is given by its name.
 *
 * @param languageName
 *      The null-terminated name of the language in the form of BCP 47, e.g. `"ur-PK"`.
 * @return
 *      The first tag that the name maps to, e.g. `TRTagMake('U', 'R', 'D', ' ')`, or
 *      `TRTagMake('d', 'f', 'l', 't')`, which is the default language, if the name is `NULL` or has
 *      no tag.
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
 *
 * @param engine
 *      The shaping engine.
 * @param typeface
 *      The typeface, or `NULL` to clear it, which makes shaping fail.
 */
TR_PUBLIC void TRShapingEngineSetTypeface(TRShapingEngineRef engine, TRTypefaceRef typeface);

/**
 * Sets the size of the em square that the results are measured in. The default is 16. A size that
 * is not positive is taken as zero.
 *
 * @param engine
 *      The shaping engine.
 * @param typeSize
 *      The size of the em square.
 */
TR_PUBLIC void TRShapingEngineSetTypeSize(TRShapingEngineRef engine, TRFloat typeSize);

/**
 * Sets the OpenType tag of the script of the text. The default is `DFLT`.
 *
 * @param engine
 *      The shaping engine.
 * @param scriptTag
 *      The tag of the script.
 */
TR_PUBLIC void TRShapingEngineSetScriptTag(TRShapingEngineRef engine, TRTag scriptTag);

/**
 * Sets the OpenType tag of the language of the text. The default is `dflt`.
 *
 * @param engine
 *      The shaping engine.
 * @param languageTag
 *      The tag of the language, which `TRShapingEngineGetLanguageTag()` can make from a name.
 */
TR_PUBLIC void TRShapingEngineSetLanguageTag(TRShapingEngineRef engine, TRTag languageTag);

/**
 * Sets the direction in which the text is written. The default is left to right. A value that is
 * not valid is treated as left to right.
 *
 * @param engine
 *      The shaping engine.
 * @param writingDirection
 *      The direction of the text.
 */
TR_PUBLIC void TRShapingEngineSetWritingDirection(TRShapingEngineRef engine,
    TRWritingDirection writingDirection);

/**
 * Sets the order in which the text is shaped. Backward order means that the code units run against
 * the writing direction, as in right-to-left text that is embedded in left-to-right text. The
 * default is forward. A value that is not valid is treated as forward.
 *
 * @param engine
 *      The shaping engine.
 * @param shapingOrder
 *      The order of the shaping.
 */
TR_PUBLIC void TRShapingEngineSetShapingOrder(TRShapingEngineRef engine,
    TRShapingOrder shapingOrder);

/**
 * Sets the OpenType features to apply on the whole text. The features are copied, and they replace
 * the ones that were set before.
 *
 * @param engine
 *      The shaping engine.
 * @param features
 *      The features. `NULL`, or a count of zero, removes them all.
 * @param count
 *      Number of features.
 * @return
 *      `TRTrue` if the features were set. `TRFalse` if they could not be stored, in which case the
 *      previous ones are kept.
 */
TR_PUBLIC TRBoolean TRShapingEngineSetOpenTypeFeatures(TRShapingEngineRef engine,
    const TROpenTypeFeature *features, TRUInteger count);

/**
 * Shapes a run of text into glyphs, with the typeface, size, script, language, direction, order and
 * features of the engine.
 *
 * @param engine
 *      The shaping engine.
 * @param codeUnits
 *      The code units of the text.
 * @param length
 *      Number of code units, which may be zero.
 * @param encoding
 *      The encoding of the code units.
 * @return
 *      New shaping result that the caller owns, or `NULL` if no typeface was set, the code units
 *      are `NULL` for a length that is not zero, the length is more than `INT_MAX`, the encoding is
 *      not valid, or on failure.
 */
TR_PUBLIC TRShapingResultRef TRShapingEngineCreateShapingResult(TRShapingEngineRef engine,
    const void *codeUnits, TRUInteger length, TRStringEncoding encoding);

/**
 * Increments the reference count of a shaping engine.
 *
 * @param engine
 *      The shaping engine whose reference count will be incremented.
 * @return
 *      The same shaping engine passed in as the parameter.
 */
TR_PUBLIC TRShapingEngineRef TRShapingEngineRetain(TRShapingEngineRef engine);

/**
 * Decrements the reference count of a shaping engine. The engine will be deallocated when its
 * reference count reaches zero.
 *
 * @param engine
 *      The shaping engine whose reference count will be decremented.
 */
TR_PUBLIC void TRShapingEngineRelease(TRShapingEngineRef engine);

TR_EXTERN_C_END

#endif
