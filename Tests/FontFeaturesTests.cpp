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

#include <cassert>
#include <cstddef>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRFontFeatures.h>
#include <Tehreer/TRShapingEngine.h>

extern "C" {
#include <API/TRFontFeatures.h>
}

#include "FontFeaturesTests.h"

using namespace std;
using namespace Tehreer;

void FontFeaturesTests::run() {
    testCreate();
    testInvalidCreate();
    testEquality();
    testRetainRelease();
    testLanguageTags();
}

void FontFeaturesTests::testCreate() {
    TROpenTypeFeature settings[] = {
        { TRTagMake('l', 'i', 'g', 'a'), 0 },
        { TRTagMake('s', 'a', 'l', 't'), 3 },
        { TRTagMake('t', 'n', 'u', 'm'), 1 }
    };

    TRFontFeaturesRef features = TRFontFeaturesCreate(settings, 3);
    assert(features != nullptr);
    assert(TRFontFeaturesGetCount(features) == 3);

    /* The settings are copied, in the order that they were given. */
    const TROpenTypeFeature *items = TRFontFeaturesGetItemsPtr(features);
    assert(items != settings);
    for (size_t i = 0; i < 3; i++) {
        assert(items[i].tag == settings[i].tag && items[i].value == settings[i].value);
    }

    settings[0].value = 1;
    assert(TRFontFeaturesGetItemsPtr(features)[0].value == 0);

    TRFontFeaturesRelease(features);

    /* A set without settings is valid, and changes nothing. */
    features = TRFontFeaturesCreate(nullptr, 0);
    assert(features != nullptr);
    assert(TRFontFeaturesGetCount(features) == 0);
    TRFontFeaturesRelease(features);
}

void FontFeaturesTests::testInvalidCreate() {
    /* Settings that are counted but not given cannot be copied. */
    assert(TRFontFeaturesCreate(nullptr, 2) == nullptr);
}

void FontFeaturesTests::testEquality() {
    TROpenTypeFeature first[] = { { TRTagMake('s', 'm', 'c', 'p'), 1 } };
    TROpenTypeFeature value[] = { { TRTagMake('s', 'm', 'c', 'p'), 2 } };
    TROpenTypeFeature tag[] = { { TRTagMake('c', '2', 's', 'c'), 1 } };
    TROpenTypeFeature longer[] = { { TRTagMake('s', 'm', 'c', 'p'), 1 }, { TRTagMake('k', 'e', 'r', 'n'), 0 } };

    TRFontFeaturesRef a = TRFontFeaturesCreate(first, 1);
    TRFontFeaturesRef b = TRFontFeaturesCreate(first, 1);
    TRFontFeaturesRef c = TRFontFeaturesCreate(value, 1);
    TRFontFeaturesRef d = TRFontFeaturesCreate(tag, 1);
    TRFontFeaturesRef e = TRFontFeaturesCreate(longer, 2);
    TRFontFeaturesRef none = TRFontFeaturesCreate(nullptr, 0);
    TRFontFeaturesRef noneToo = TRFontFeaturesCreate(nullptr, 0);

    /* The settings tell, whatever the objects are. */
    assert(TRFontFeaturesIsEqual(a, a));
    assert(TRFontFeaturesIsEqual(a, b));
    assert(!TRFontFeaturesIsEqual(a, c));
    assert(!TRFontFeaturesIsEqual(a, d));
    assert(!TRFontFeaturesIsEqual(a, e));
    assert(!TRFontFeaturesIsEqual(a, none));
    assert(TRFontFeaturesIsEqual(none, noneToo));

    TRFontFeaturesRelease(noneToo);
    TRFontFeaturesRelease(none);
    TRFontFeaturesRelease(e);
    TRFontFeaturesRelease(d);
    TRFontFeaturesRelease(c);
    TRFontFeaturesRelease(b);
    TRFontFeaturesRelease(a);
}

void FontFeaturesTests::testRetainRelease() {
    TROpenTypeFeature settings[] = { { TRTagMake('k', 'e', 'r', 'n'), 0 } };
    TRFontFeaturesRef features = TRFontFeaturesCreate(settings, 1);

    assert(TRFontFeaturesRetain(features) == features);
    TRFontFeaturesRelease(features);
    assert(TRFontFeaturesGetCount(features) == 1);
    assert(TRFontFeaturesGetItemsPtr(features)[0].tag == TRTagMake('k', 'e', 'r', 'n'));

    TRFontFeaturesRelease(features);
}

void FontFeaturesTests::testLanguageTags() {
    const TRTag defaultTag = TRTagMake('d', 'f', 'l', 't');

    assert(TRShapingEngineGetLanguageTag("en") == TRTagMake('E', 'N', 'G', ' '));
    assert(TRShapingEngineGetLanguageTag("ur-PK") == TRTagMake('U', 'R', 'D', ' '));
    assert(TRShapingEngineGetLanguageTag("tr-TR") == TRTagMake('T', 'R', 'K', ' '));
    assert(TRShapingEngineGetLanguageTag("sr") == TRTagMake('S', 'R', 'B', ' '));
    assert(TRShapingEngineGetLanguageTag("ja") == TRTagMake('J', 'A', 'N', ' '));
    assert(TRShapingEngineGetLanguageTag(nullptr) == defaultTag);
    assert(TRShapingEngineGetLanguageTag("") == defaultTag);
}

#ifdef STANDALONE_TESTING

int main() {
    FontFeaturesTests tests;
    tests.run();

    return 0;
}

#endif
