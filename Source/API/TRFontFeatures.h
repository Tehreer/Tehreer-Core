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

#ifndef _TEHREER_API_FONT_FEATURES_H
#define _TEHREER_API_FONT_FEATURES_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRFontFeatures.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _TRFontFeatures {
    ObjectBase _base;
    TROpenTypeFeature *items;
    TRUInteger count;
} TRFontFeatures;

/* Tells if two sets have the same settings in the same order, whatever objects they are. */
TR_INTERNAL TRBoolean TRFontFeaturesIsEqual(TRFontFeaturesRef first, TRFontFeaturesRef second);

#endif
