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

#ifndef _TEHREER_FONT_FEATURES_H
#define _TEHREER_FONT_FEATURES_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * An OpenType feature setting, such as `liga` with value 0 to turn ligatures off, or `smcp` with
 * value 1 to turn small capitals on. A value of two or more picks an alternate of the features that
 * have some, such as `salt`.
 */
typedef struct _TROpenTypeFeature {
    TRTag tag;
    TRUInt32 value;
} TROpenTypeFeature;

/**
 * An immutable set of OpenType feature settings, which is given to the text as an attribute. The
 * settings of the features that are not in the set are left to the defaults of the font and of the
 * script, so the set only has to name the ones that change.
 */
typedef const struct _TRFontFeatures *TRFontFeaturesRef;

/**
 * Creates a set of feature settings. If a feature is set more than once, the last setting wins.
 *
 * @param features
 *      The settings, which are copied. It may be `NULL` if the count is zero.
 * @param count
 *      The number of settings, which may be zero for a set that changes nothing.
 * @return
 *      A reference to a set of feature settings, or `NULL` on failure.
 */
TR_PUBLIC TRFontFeaturesRef TRFontFeaturesCreate(const TROpenTypeFeature *features,
    TRUInteger count);

/**
 * Returns the settings of the set, in the order that they were given.
 */
TR_PUBLIC const TROpenTypeFeature *TRFontFeaturesGetItemsPtr(TRFontFeaturesRef features);

/**
 * Returns the number of settings in the set.
 */
TR_PUBLIC TRUInteger TRFontFeaturesGetCount(TRFontFeaturesRef features);

/**
 * Increments the reference count of a set of feature settings.
 *
 * @param features
 *      The set whose reference count will be incremented.
 * @return
 *      The same set passed in as the parameter.
 */
TR_PUBLIC TRFontFeaturesRef TRFontFeaturesRetain(TRFontFeaturesRef features);

/**
 * Decrements the reference count of a set of feature settings. The set will be deallocated when its
 * reference count reaches zero.
 *
 * @param features
 *      The set whose reference count will be decremented.
 */
TR_PUBLIC void TRFontFeaturesRelease(TRFontFeaturesRef features);

TR_EXTERN_C_END

#endif
