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

#include <API/TRBase.h>
#include <Core/Object.h>

#include "TRFontFeatures.h"

TR_INTERNAL TRBoolean TRFontFeaturesIsEqual(TRFontFeaturesRef first, TRFontFeaturesRef second)
{
    TRBoolean isEqual = (first == second);

    if (!isEqual && first->count == second->count) {
        isEqual = (first->count == 0
                   || memcmp(first->items, second->items,
                             sizeof(TROpenTypeFeature) * first->count) == 0);
    }

    return isEqual;
}

#define FEATURES    0
#define ITEMS       1
#define COUNT       2

TRFontFeaturesRef TRFontFeaturesCreate(const TROpenTypeFeature *features, TRUInteger count)
{
    TRFontFeatures *fontFeatures = NULL;

    if (features || count == 0) {
        TRUInteger sizes[COUNT] = { 0 };
        void *pointers[COUNT] = { NULL };

        sizes[FEATURES] = sizeof(TRFontFeatures);
        sizes[ITEMS] = sizeof(TROpenTypeFeature) * count;

        fontFeatures = ObjectCreate(sizes, COUNT, pointers, NULL);

        if (fontFeatures) {
            fontFeatures->items = pointers[ITEMS];
            fontFeatures->count = count;

            if (count > 0) {
                memcpy(fontFeatures->items, features, sizeof(TROpenTypeFeature) * count);
            }
        }
    }

    return fontFeatures;
}

#undef FEATURES
#undef ITEMS
#undef COUNT

const TROpenTypeFeature *TRFontFeaturesGetItemsPtr(TRFontFeaturesRef features)
{
    return features->items;
}

TRUInteger TRFontFeaturesGetCount(TRFontFeaturesRef features)
{
    return features->count;
}

TRFontFeaturesRef TRFontFeaturesRetain(TRFontFeaturesRef features)
{
    return ObjectRetain((ObjectRef)features);
}

void TRFontFeaturesRelease(TRFontFeaturesRef features)
{
    ObjectRelease((ObjectRef)features);
}
