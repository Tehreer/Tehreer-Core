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
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Core/AtomicFlag.h>
#include <Core/AtomicPtr.h>
#include <Core/AtomicUInt.h>
}

#include "AtomicTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 10000;

void AtomicTests::run() {
    testAtomicFlagTestAndSet();
    testAtomicFlagClear();
    testAtomicFlagThreadSafety();
    testAtomicUIntLoadStore();
    testAtomicUIntCompareAndSet();
    testAtomicUIntIncrementDecrement();
    testAtomicUIntThreadSafety();
    testAtomicPtrLoadStore();
    testAtomicPtrCompareAndSet();
    testAtomicPtrThreadSafety();
}

void AtomicTests::testAtomicFlagTestAndSet() {
    AtomicFlag flag = AtomicFlagMake();

    assert(AtomicFlagTestAndSet(&flag) == false);
    assert(AtomicFlagTestAndSet(&flag) == true);
    assert(AtomicFlagTestAndSet(&flag) == true);
}

void AtomicTests::testAtomicFlagClear() {
    AtomicFlag flag = AtomicFlagMake();
    AtomicFlagTestAndSet(&flag);
    AtomicFlagClear(&flag);

    assert(AtomicFlagTestAndSet(&flag) == false);
}

void AtomicTests::testAtomicFlagThreadSafety() {
    AtomicFlag flag = AtomicFlagMake();

    size_t counter = 0;
    vector<thread> threads;

    auto worker = [&]() {
        for (size_t i = 0; i < Iterations; i++) {
            while (AtomicFlagTestAndSet(&flag)) { }
            counter += 1;

            AtomicFlagClear(&flag);
        }
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back(worker);
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(counter == NumThreads * Iterations);
}

void AtomicTests::testAtomicUIntLoadStore() {
    AtomicUInt value;

    AtomicUIntStore(&value, 0x1234);
    assert(AtomicUIntLoad(&value) == 0x1234);
}

void AtomicTests::testAtomicUIntCompareAndSet() {
    AtomicUInt value;

    TRUInteger initialNumber = 0x1234;
    TRUInteger newNumber = 0x5678;
    TRUInteger unknownNumber = 0x9999;
    TRUInteger expected;

    AtomicUIntStore(&value, initialNumber);

    expected = initialNumber;
    assert(AtomicUIntCompareAndSet(&value, &expected, newNumber) == true);
    assert(AtomicUIntLoad(&value) == newNumber);

    expected = unknownNumber;
    assert(AtomicUIntCompareAndSet(&value, &expected, unknownNumber) == false);
    assert(AtomicUIntLoad(&value) == newNumber);
}

void AtomicTests::testAtomicUIntIncrementDecrement() {
    AtomicUInt value;
    AtomicUIntInit(&value, 0);

    assert(AtomicUIntIncrement(&value) == 1);
    assert(AtomicUIntIncrement(&value) == 2);
    assert(AtomicUIntDecrement(&value) == 1);
    assert(AtomicUIntDecrement(&value) == 0);
}

void AtomicTests::testAtomicUIntThreadSafety() {
    AtomicUInt value;
    AtomicUIntInit(&value, 0);

    vector<thread> threads;

    auto worker = [&]() {
        for (size_t i = 0; i < Iterations; i++) {
            AtomicUIntIncrement(&value);
            AtomicUIntDecrement(&value);
        }
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back(worker);
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(AtomicUIntLoad(&value) == 0);
}

void AtomicTests::testAtomicPtrLoadStore() {
    AtomicPtrType(uint32_t) pointer = nullptr;

    uint32_t numbers[] = { 0x1234 };
    uint32_t *value = &numbers[0];

    AtomicPtrStore(&pointer, value);
    assert(AtomicPtrLoad(&pointer) == value);
}

void AtomicTests::testAtomicPtrCompareAndSet() {
    AtomicPtrType(uint32_t) pointer = nullptr;

    uint32_t numbers[] = { 0x1234, 0x5678, 0x9999 };
    uint32_t *initialValue = &numbers[0];
    uint32_t *newValue = &numbers[1];
    uint32_t *unknownValue = &numbers[2];
    uint32_t *expected;

    AtomicPtrStore(&pointer, initialValue);

    expected = initialValue;
    assert(AtomicPtrCompareAndSet(&pointer, &expected, newValue) == true);
    assert(AtomicPtrLoad(&pointer) == newValue);

    expected = unknownValue;
    assert(AtomicPtrCompareAndSet(&pointer, &expected, unknownValue) == false);
    assert(AtomicPtrLoad(&pointer) == newValue);
}

void AtomicTests::testAtomicPtrThreadSafety() {
    AtomicPtrType(uint32_t) pointer = nullptr;

    vector<uint32_t> numbers(NumThreads);
    vector<uint32_t *> values(NumThreads);
    size_t counter = 0;
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        numbers[i] = i;
        values[i] = &numbers[i];
    }

    auto worker = [&](size_t id) {
        uint32_t *desired = values[id];
        uint32_t *expected;

        for (size_t i = 0; i < Iterations; i++) {
            do {
                expected = nullptr;
            } while (!AtomicPtrCompareAndSet(&pointer, &expected, desired));

            if (AtomicPtrLoad(&pointer) == desired) {
                counter += 1;
            }

            AtomicPtrStore(&pointer, nullptr);
        }
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(counter == NumThreads * Iterations);
    assert(AtomicPtrLoad(&pointer) == nullptr);
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    AtomicTests atomicTests;
    atomicTests.run();

    return 0;
}

#endif
