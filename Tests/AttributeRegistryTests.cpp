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
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include <SheenBidi/SheenBidi.h>

#include <Tehreer/TRAttribute.h>

extern "C" {
#include <Text/AttributeRegistry.h>
}

#include "AttributeRegistryTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 16;

void AttributeRegistryTests::run() {
    testAttributeIDs();
    testUnknownAttributeType();
    testDefaultConfig();
    testRegisteredAttributeInfo();
    testEqualValuesMergeRuns();
    testConcurrentAccess();
}

void AttributeRegistryTests::testAttributeIDs() {
    SBAttributeID typefaceID = AttributeRegistryGetAttributeID(TRAttributeTypeface);
    SBAttributeID pointSizeID = AttributeRegistryGetAttributeID(TRAttributePointSize);

    assert(typefaceID != SBAttributeIDNone);
    assert(pointSizeID != SBAttributeIDNone);
    assert(typefaceID != pointSizeID);

    assert(AttributeRegistryGetAttributeID(TRAttributeTypeface) == typefaceID);
    assert(AttributeRegistryGetAttributeID(TRAttributePointSize) == pointSizeID);
}

void AttributeRegistryTests::testUnknownAttributeType() {
    assert(AttributeRegistryGetAttributeID(0) == SBAttributeIDNone);
    assert(AttributeRegistryGetAttributeID(3) == SBAttributeIDNone);
    assert(AttributeRegistryGetAttributeID(0xFFFF) == SBAttributeIDNone);
}

void AttributeRegistryTests::testDefaultConfig() {
    SBTextConfigRef config = AttributeRegistryGetDefaultConfig();

    assert(config != nullptr);
    assert(AttributeRegistryGetDefaultConfig() == config);
}

void AttributeRegistryTests::testRegisteredAttributeInfo() {
    SBMutableTextRef text = SBTextCreateMutable(SBStringEncodingUTF8,
        AttributeRegistryGetDefaultConfig());
    SBAttributeRegistryRef registry = SBTextGetAttributeRegistry(text);
    SBAttributeID typefaceID = AttributeRegistryGetAttributeID(TRAttributeTypeface);
    SBAttributeID pointSizeID = AttributeRegistryGetAttributeID(TRAttributePointSize);
    SBAttributeInfo info;

    assert(SBAttributeRegistryGetAttributeID(registry, "Typeface") == typefaceID);
    assert(SBAttributeRegistryGetAttributeID(registry, "PointSize") == pointSizeID);
    assert(SBAttributeRegistryGetAttributeID(registry, "Unknown") == SBAttributeIDNone);

    assert(SBAttributeRegistryGetAttributeInfo(registry, typefaceID, &info) == SBTrue);
    assert(strcmp(info.name, "Typeface") == 0);
    assert(info.group == SBAttributeGroupNone);
    assert(info.scope == SBAttributeScopeCharacter);

    assert(SBAttributeRegistryGetAttributeInfo(registry, pointSizeID, &info) == SBTrue);
    assert(strcmp(info.name, "PointSize") == 0);
    assert(info.group == SBAttributeGroupNone);
    assert(info.scope == SBAttributeScopeCharacter);

    SBTextRelease(text);
}

static SBMutableTextRef createText() {
    SBMutableTextRef text = SBTextCreateMutable(SBStringEncodingUTF8,
        AttributeRegistryGetDefaultConfig());

    SBTextAppendCodeUnits(text, "abcd", 4);

    return text;
}

static void setPointSize(SBMutableTextRef text, SBUInteger index, SBUInteger length, TRFloat size) {
    TRAttribute attribute = {};
    attribute.type = TRAttributePointSize;
    attribute.value.pointSize = size;

    SBTextSetAttribute(text, index, length,
        AttributeRegistryGetAttributeID(TRAttributePointSize), &attribute);
}

static SBUInteger firstRunLength(SBMutableTextRef text) {
    SBUInteger length = 0;
    SBAttributeListRef list = SBTextGetAttributes(text, SBAttributeFilterMakeAny(), 0, &length);
    SBAttributeListRelease(list);

    return length;
}

void AttributeRegistryTests::testEqualValuesMergeRuns() {
    SBMutableTextRef text = createText();

    setPointSize(text, 0, 2, 12.0f);
    setPointSize(text, 2, 2, 12.0f);
    assert(firstRunLength(text) == 4);

    setPointSize(text, 2, 2, 14.0f);
    assert(firstRunLength(text) == 2);

    SBTextRelease(text);
}

void AttributeRegistryTests::testConcurrentAccess() {
    vector<SBAttributeID> typefaceIDs(NumThreads, SBAttributeIDNone);
    vector<SBAttributeID> pointSizeIDs(NumThreads, SBAttributeIDNone);
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&typefaceIDs, &pointSizeIDs, i]() {
            typefaceIDs[i] = AttributeRegistryGetAttributeID(TRAttributeTypeface);
            pointSizeIDs[i] = AttributeRegistryGetAttributeID(TRAttributePointSize);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (size_t i = 0; i < NumThreads; i++) {
        assert(typefaceIDs[i] != SBAttributeIDNone);
        assert(typefaceIDs[i] == typefaceIDs[0]);
        assert(pointSizeIDs[i] == pointSizeIDs[0]);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    AttributeRegistryTests tests;
    tests.run();

    return 0;
}

#endif
