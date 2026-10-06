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

#include <atomic>
#include <cassert>
#include <cstddef>
#include <chrono>
#include <thread>
#include <vector>

#include <API/TRBase.h>

#undef USE_C11_THREADS
#undef USE_PTHREADS
#undef USE_WIN_SYNC

extern "C" {
#include <Core/Once.h>
#include <Core/Once.c>
}

#include "SpinOnceTests.h"

using namespace std;
using namespace Tehreer;

using namespace std::chrono;

constexpr size_t NumThreads = 16;
constexpr size_t Iterations = 1000;

void SpinOnceTests::run() {
    testBasicOnce();
    testMultipleThreads();
    testPublishedWrites();
    testMultipleOnces();
    testStressTest();
}

void SpinOnceTests::testBasicOnce() {
    static Once once = OnceMake();
    static size_t callCount = 0;

    auto func = []() {
        callCount++;
    };

    ExecuteSpinOnce(&once, func);
    assert(callCount == 1);

    ExecuteSpinOnce(&once, func);
    ExecuteSpinOnce(&once, func);
    assert(callCount == 1);
}

void SpinOnceTests::testMultipleThreads() {
    static Once once = OnceMake();
    static atomic<size_t> callCount{0};

    vector<thread> threads;

    auto func = []() {
        callCount++;
        this_thread::sleep_for(milliseconds(10));
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            for (size_t j = 0; j < Iterations; j++) {
                ExecuteSpinOnce(&once, func);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(callCount == 1);
}

void SpinOnceTests::testPublishedWrites() {
    static Once once = OnceMake();
    static size_t values[64];

    auto func = []() {
        for (size_t i = 0; i < 64; i++) {
            values[i] = i + 1;
        }
    };

    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            ExecuteSpinOnce(&once, func);

            for (size_t j = 0; j < 64; j++) {
                assert(values[j] == j + 1);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }
}

void SpinOnceTests::testMultipleOnces() {
    static Once once1 = OnceMake();
    static Once once2 = OnceMake();

    static atomic<size_t> count1{0};
    static atomic<size_t> count2{0};

    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            ExecuteSpinOnce(&once1, []() { count1++; });
            ExecuteSpinOnce(&once2, []() { count2++; });
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(count1 == 1);
    assert(count2 == 1);
}

void SpinOnceTests::testStressTest() {
    constexpr size_t NumOnces = 100;

    static vector<Once> onces(NumOnces);
    static vector<atomic<size_t>> counts(NumOnces);

    for (size_t i = 0; i < NumOnces; i++) {
        onces[i] = OnceMake();
        counts[i] = 0;
    }

    vector<thread> threads;

    for (size_t t = 0; t < NumThreads; t++) {
        threads.emplace_back([&]() {
            for (size_t i = 0; i < NumOnces; i++) {
                ExecuteSpinOnce(&onces[i], []() {});
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (size_t i = 0; i < NumOnces; i++) {
        assert(AtomicUIntLoad(&onces[i]) == 2);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    SpinOnceTests tests;
    tests.run();

    return 0;
}

#endif
