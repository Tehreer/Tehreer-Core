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
#include <thread>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" {
#include <Core/Mutex.h>
#include <Graphics/FreeType.h>
}

#include "FreeTypeTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 16;

void FreeTypeTests::run() {
    testDefaultInstance();
    testDefaultLibrary();
    testConcurrentAccess();
}

void FreeTypeTests::testDefaultInstance() {
    FreeTypeRef first = FreeTypeGetDefault();
    FreeTypeRef second = FreeTypeGetDefault();

    assert(first != nullptr);
    assert(first == second);
}

void FreeTypeTests::testDefaultLibrary() {
    FreeTypeRef freetype = FreeTypeGetDefault();
    FT_Int major = 0;
    FT_Int minor = 0;
    FT_Int patch = 0;

    assert(freetype->library != nullptr);

    MutexLock(&freetype->mutex);
    FT_Library_Version(freetype->library, &major, &minor, &patch);
    MutexUnlock(&freetype->mutex);

    assert(major >= 2);
}

void FreeTypeTests::testConcurrentAccess() {
    vector<FreeTypeRef> results(NumThreads, nullptr);
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&results, i]() {
            results[i] = FreeTypeGetDefault();
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (auto result : results) {
        assert(result == FreeTypeGetDefault());
        assert(result->library != nullptr);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    FreeTypeTests tests;
    tests.run();

    return 0;
}

#endif
