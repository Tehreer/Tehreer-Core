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

extern "C" {
#include <API/TRBase.h>
}

#include "BaseTests.h"

using namespace std;
using namespace Tehreer;

void BaseTests::run() {
    testTagMake();
    testTagMakeHighBytes();
    testColorMake();
    testInvalidIndex();
    testIndexAndRangeValidity();
    testTypeSizes();
}

void BaseTests::testTagMake() {
    assert(TRTagMake('a', 'b', 'c', 'd') == 0x61626364);
    assert(TRTagMake('w', 'g', 'h', 't') == 0x77676874);
    assert(TRTagMake(0, 0, 0, 0) == 0);
    assert(TRTagMake(0, 0, 0, 1) == 1);
}

void BaseTests::testTagMakeHighBytes() {
    assert(TRTagMake(0xFF, 0xFE, 0xFD, 0xFC) == 0xFFFEFDFC);
    assert(TRTagMake(0x80, 0, 0, 0) == 0x80000000);
    assert(TRTagMake(0, 0, 0, 0xFF) == 0xFF);
}

void BaseTests::testColorMake() {
    assert(TRColorMake(0x12, 0x34, 0x56, 0x78) == 0x12345678);
    assert(TRColorMake(0xFF, 0x00, 0x00, 0x00) == 0xFF000000);
    assert(TRColorMake(0xFF, 0xFF, 0xFF, 0xFF) == 0xFFFFFFFF);
    assert(TRColorMake(0x00, 0x00, 0x00, 0x00) == 0);
}

void BaseTests::testIndexAndRangeValidity() {
    const TRUInteger max = TRInvalidIndex;

    assert(IndexIsValid(0, 1) && IndexIsValid(4, 5));
    assert(!IndexIsValid(5, 5) && !IndexIsValid(max, 5) && !IndexIsValid(0, 0));

    /* A range is valid if it ends within the count, which is not told by adding them up. */
    assert(RangeIsValid(2, 3, 10) && RangeIsValid(0, 10, 10) && RangeIsValid(10, 0, 10));
    assert(!RangeIsValid(8, 5, 10) && !RangeIsValid(0, 11, 10) && !RangeIsValid(11, 0, 10));
    assert(RangeIsValid(0, 0, 0) && !RangeIsValid(1, 0, 0));

    /* The index and the length that would wrap around are not valid. */
    assert(!RangeIsValid(2, max, 10) && !RangeIsValid(max, 2, 10) && !RangeIsValid(max, max, 10));
    assert(!RangeIsValid(max - 1, 5, 10) && !RangeIsValid(max, 0, 10));
}

void BaseTests::testInvalidIndex() {
    assert(TRInvalidIndex == static_cast<TRUInteger>(-1));
    assert(TRInvalidIndex > 0);
}

void BaseTests::testTypeSizes() {
    assert(sizeof(TRInt8) == 1);
    assert(sizeof(TRUInt8) == 1);
    assert(sizeof(TRInt16) == 2);
    assert(sizeof(TRUInt16) == 2);
    assert(sizeof(TRInt32) == 4);
    assert(sizeof(TRUInt32) == 4);
    assert(sizeof(TRFloat) == 4);
    assert(sizeof(TRBoolean) == 1);
    assert(sizeof(TRInteger) == sizeof(void *));
    assert(sizeof(TRUInteger) == sizeof(void *));
}

#ifdef STANDALONE_TESTING

int main() {
    BaseTests tests;
    tests.run();

    return 0;
}

#endif
