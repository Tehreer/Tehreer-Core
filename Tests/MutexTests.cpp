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

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

extern "C" {
#include <Core/Mutex.h>
}

#include "MutexTests.h"

using namespace std;
using namespace std::chrono;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 10'000;
constexpr size_t StressIterations = 100'000;

void MutexTests::run() {
    testInitDestroy();
    testLockUnlock();
    testTryLock();
    testThreadSafety();
    testTryLockSpinning();
    testStressTest();
    testMemoryVisibility();
    testNoDeadlock();
}

void MutexTests::testInitDestroy() {
    Mutex mutex;

    // Test init and destroy (should not crash)
    MutexInit(&mutex);
    MutexDestroy(&mutex);

    // Re-initialize after destroy
    MutexInit(&mutex);
    MutexDestroy(&mutex);
}

void MutexTests::testLockUnlock() {
    Mutex mutex;
    MutexInit(&mutex);

    // Basic lock/unlock
    MutexLock(&mutex);
    MutexUnlock(&mutex);

    // Verify we can lock again
    MutexLock(&mutex);
    MutexUnlock(&mutex);

    MutexDestroy(&mutex);
}

void MutexTests::testTryLock() {
    Mutex mutex;
    MutexInit(&mutex);

    auto result = MutexTryLock(&mutex);
    assert(result == TRTrue);

    // Second try should fail
    result = MutexTryLock(&mutex);
    assert(result == TRFalse);
    MutexUnlock(&mutex);

    // Try again after unlock
    result = MutexTryLock(&mutex);
    assert(result == TRTrue);
    MutexUnlock(&mutex);

    MutexDestroy(&mutex);
}

void MutexTests::testThreadSafety() {
    Mutex mutex;
    MutexInit(&mutex);

    size_t counter = 0;
    size_t tryLockSuccess = 0;
    vector<thread> threads;

    auto tryWorker = [&]() {
        for (size_t i = 0; i < Iterations; i++) {
            if (MutexTryLock(&mutex)) {
                tryLockSuccess++;

                auto current = counter;
                this_thread::yield();
                counter = current + 1;

                MutexUnlock(&mutex);
            }
        }
    };

    auto blockingWorker = [&]() {
        for (size_t i = 0; i < Iterations; i++) {
            MutexLock(&mutex);
            counter++;
            MutexUnlock(&mutex);
        }
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back(tryWorker);
        threads.emplace_back(blockingWorker);
    }

    for (auto &t : threads) {
        t.join();
    }

    // Counter should equal total operations
    assert(counter == ((NumThreads * Iterations) + (tryLockSuccess)));
    assert(tryLockSuccess > 0);

    MutexDestroy(&mutex);
}

void MutexTests::testTryLockSpinning() {
    Mutex mutex;
    MutexInit(&mutex);

    bool lockHeld = false;
    size_t spinSuccess = 0;
    size_t spinFailures = 0;

    // Lock the mutex in main thread
    MutexLock(&mutex);
    lockHeld = true;

    thread spinner([&]() {
        const size_t MaxAttempts = 1'000'000;

        // Try to acquire mutex with spinning using trylock
        for (size_t i = 0; MaxAttempts; i++) {
            if (MutexTryLock(&mutex)) {
                spinSuccess++;
                assert(lockHeld == false); // Should only succeed after main unlocks
                MutexUnlock(&mutex);
                break;
            } else {
                spinFailures++;

                // Yield occasionally to avoid consuming too much CPU
                if (i % 100 == 0) {
                    this_thread::yield();
                }
            }
        }
    });

    // Hold the lock for a bit
    this_thread::sleep_for(milliseconds(5));
    lockHeld = false;
    MutexUnlock(&mutex);

    spinner.join();

    // Should have eventually acquired the lock
    assert(spinSuccess == 1);
    assert(spinFailures > 0);

    MutexDestroy(&mutex);
}

void MutexTests::testStressTest() {
    Mutex mutex;
    MutexInit(&mutex);

    uint64_t totalCount = 0;
    array<uint64_t, NumThreads> threadCounts{};
    vector<thread> threads;

    auto worker = [&](size_t threadId) {
        uint64_t localCounter = 0;

        for (size_t i = 0; i < StressIterations; i++) {
            // Mix of lock and trylock
            if (i % 3 == 0) {
                MutexLock(&mutex);
                totalCount++;
                threadCounts[threadId]++;
                MutexUnlock(&mutex);

                localCounter++;
            } else {
                if (MutexTryLock(&mutex)) {
                    totalCount++;
                    threadCounts[threadId]++;
                    MutexUnlock(&mutex);

                    localCounter++;
                }
            }

            // Occasional yield to mix up scheduling
            if (i % 1000 == 0) {
                this_thread::yield();
            }
        }

        // Verify thread-local count matches increments under lock
        assert(threadCounts[threadId] == localCounter);
    };

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto &t : threads) {
        t.join();
    }

    // Verify total count matches sum of thread counts
    uint64_t expectedTotal = 0;
    for (auto &count : threadCounts) {
        expectedTotal += count;
    }
    assert(totalCount == expectedTotal);

    MutexDestroy(&mutex);
}

void MutexTests::testMemoryVisibility() {
    Mutex mutex;
    MutexInit(&mutex);

    size_t sharedData = 0;
    bool ready = false;

    thread writer([&]() {
        MutexLock(&mutex);
        sharedData = 123;
        ready = true;
        MutexUnlock(&mutex);
    });

    thread reader([&]() {
        while (true) {
            MutexLock(&mutex);
            if (ready) {
                assert(sharedData == 123);
                MutexUnlock(&mutex);
                break;
            }
            MutexUnlock(&mutex);
        }
    });

    writer.join();
    reader.join();

    MutexDestroy(&mutex);
}

void MutexTests::testNoDeadlock() {
    Mutex mutex;
    MutexInit(&mutex);

    atomic<bool> keepRunning{true};
    size_t lockCount = 0;

    // Create a thread that constantly locks/unlocks
    thread worker([&]() {
        while (keepRunning) {
            MutexLock(&mutex);
            lockCount++;
            MutexUnlock(&mutex);

            // Small yield to prevent CPU starvation
            this_thread::yield();
        }
    });

    // Main thread also constantly locks/unlocks
    for (size_t i = 0; i < 10'000; i++) {
        MutexLock(&mutex);

        // Simulate some work
        for (volatile size_t j = 0; j < 10; j++) { }

        MutexUnlock(&mutex);

        if (i % 100 == 0) {
            this_thread::yield();
        }
    }

    keepRunning = false;
    worker.join();

    // If we got here without deadlock, test passes
    assert(lockCount > 0);

    MutexDestroy(&mutex);
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    MutexTests mutexTests;
    mutexTests.run();

    return 0;
}

#endif
