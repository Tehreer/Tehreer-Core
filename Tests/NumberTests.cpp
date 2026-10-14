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
#include <cstdint>
#include <limits>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Core/Number.h>
}

#include "NumberTests.h"

using namespace std;
using namespace Tehreer;

void NumberTests::run() {
    testToFixed();
    testToFixedSaturation();
    testToFixedNotANumber();
}

void NumberTests::testToFixed() {
    assert(NumberToFixed(0.0f, 64.0f) == 0);
    assert(NumberToFixed(1.0f, 64.0f) == 64);
    assert(NumberToFixed(12.5f, 64.0f) == 800);
    assert(NumberToFixed(1.0f, 65536.0f) == 0x10000);

    /* The value is rounded to the nearest unit. */
    assert(NumberToFixed(1.4f, 1.0f) == 1);
    assert(NumberToFixed(1.6f, 1.0f) == 2);
}

void NumberTests::testToFixedSaturation() {
    constexpr TRFloat infinity = numeric_limits<TRFloat>::infinity();

    assert(NumberToFixed(infinity, 64.0f) == INT32_MAX);
    assert(NumberToFixed(-infinity, 64.0f) == INT32_MIN);
    assert(NumberToFixed(1e30f, 1.0f) == INT32_MAX);
    assert(NumberToFixed(-1e30f, 1.0f) == INT32_MIN);
    assert(NumberToFixed(3e9f, 1.0f) == INT32_MAX);
    assert(NumberToFixed(1.0f, infinity) == INT32_MAX);
}

void NumberTests::testToFixedNotANumber() {
    constexpr TRFloat notANumber = numeric_limits<TRFloat>::quiet_NaN();

    assert(NumberToFixed(notANumber, 64.0f) == 0);
    assert(NumberToFixed(1.0f, notANumber) == 0);
}

#ifdef STANDALONE_TESTING

int main() {
    NumberTests tests;
    tests.run();

    return 0;
}

#endif
