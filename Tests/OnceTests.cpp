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

extern "C" {
#include <Core/Once.h>
}

#include "OnceTests.h"

using namespace std;
using namespace std::chrono;
using namespace Tehreer;

constexpr size_t NumThreads = 16;
constexpr size_t Iterations = 1000;

void OnceTests::run() {
    testBasicOnce();
    testMultipleThreads();
    testMultipleOnces();
    testRecursiveOnce();
    testStressTest();
}

void OnceTests::testBasicOnce() {
    static Once once = OnceMake();
    static size_t callCount = 0;

    auto func = []() {
        callCount++;
    };

    OnceExecute(&once, func);
    assert(callCount == 1);

    OnceExecute(&once, func);
    OnceExecute(&once, func);
    assert(callCount == 1);
}

void OnceTests::testMultipleThreads() {
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
                OnceExecute(&once, func);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(callCount == 1);
}

void OnceTests::testMultipleOnces() {
    static Once once1 = OnceMake();
    static Once once2 = OnceMake();
    static Once once3 = OnceMake();

    static atomic<size_t> count1{0};
    static atomic<size_t> count2{0};
    static atomic<size_t> count3{0};

    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            OnceExecute(&once1, []() { count1++; });
            OnceExecute(&once2, []() { count2++; });
            OnceExecute(&once3, []() { count3++; });
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(count1 == 1);
    assert(count2 == 1);
    assert(count3 == 1);
}

void OnceTests::testRecursiveOnce() {
    static Once outerOnce = OnceMake();
    static Once innerOnce = OnceMake();

    static atomic<size_t> innerCount(0);
    static atomic<size_t> outerCount(0);

    static auto innerFunc = []() {
        innerCount++;
        this_thread::sleep_for(microseconds(100));
    };

    static auto outerFunc = []() {
        outerCount++;
        OnceExecute(&innerOnce, innerFunc);
        this_thread::sleep_for(microseconds(100));
    };

    vector<thread> threads;
    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            OnceExecute(&outerOnce, outerFunc);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(outerCount == 1);
    assert(innerCount == 1);
}

void OnceTests::testStressTest() {
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
            for (size_t iter = 0; iter < Iterations; iter++) {
                thread_local size_t i = 0;

                for (; i < NumOnces; i++) {
                    OnceExecute(&onces[i], []() {
                        counts[i]++;
                        this_thread::yield();
                    });

                    if (iter % 10 == 0) {
                        this_thread::yield();
                    }
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (size_t i = 0; i < NumOnces; i++) {
        assert(counts[i] == 1);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    OnceTests tests;
    tests.run();

    return 0;
}

#endif
