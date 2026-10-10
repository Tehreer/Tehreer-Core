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
#include <Graphics/AdvanceCache.h>
}

#include "AdvanceCacheTests.h"

using namespace std;
using namespace Tehreer;

void AdvanceCacheTests::run() {
    testMissAndHit();
    testNegativeAdvance();
    testOutOfRange();
    testPagesAreIndependent();
    testEmptyCache();
    testLastGlyph();
    testConcurrentAccess();
}

void AdvanceCacheTests::testMissAndHit() {
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 600);

    TRInt32 advance = 123;
    assert(!AdvanceCacheGet(&cache, 5, &advance));
    assert(advance == 123);

    AdvanceCachePut(&cache, 5, 1114);
    assert(AdvanceCacheGet(&cache, 5, &advance));
    assert(advance == 1114);

    /* A zero advance is a value like any other. */
    AdvanceCachePut(&cache, 6, 0);
    advance = 99;
    assert(AdvanceCacheGet(&cache, 6, &advance));
    assert(advance == 0);

    /* Storing again replaces the value. */
    AdvanceCachePut(&cache, 5, 7);
    assert(AdvanceCacheGet(&cache, 5, &advance));
    assert(advance == 7);

    /* The neighbors of a stored glyph are still missing. */
    assert(!AdvanceCacheGet(&cache, 4, &advance));
    assert(!AdvanceCacheGet(&cache, 7, &advance));

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testNegativeAdvance() {
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 10);

    TRInt32 advance = 0;
    AdvanceCachePut(&cache, 3, -250);
    assert(AdvanceCacheGet(&cache, 3, &advance));
    assert(advance == -250);

    AdvanceCachePut(&cache, 4, INT32_MAX);
    assert(AdvanceCacheGet(&cache, 4, &advance));
    assert(advance == INT32_MAX);

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testOutOfRange() {
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 600);

    TRInt32 advance = 0;
    AdvanceCachePut(&cache, 600, 5);
    AdvanceCachePut(&cache, 65535, 5);
    assert(!AdvanceCacheGet(&cache, 600, &advance));
    assert(!AdvanceCacheGet(&cache, 65535, &advance));

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testPagesAreIndependent() {
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 1000);

    /* Only the page of a stored glyph is allocated. */
    assert(cache._pageCount == 4);
    for (size_t i = 0; i < cache._pageCount; i++) {
        assert(cache._pages[i] == nullptr);
    }

    AdvanceCachePut(&cache, 300, 30);
    assert(cache._pages[0] == nullptr);
    assert(cache._pages[1] != nullptr);
    assert(cache._pages[2] == nullptr);

    TRInt32 advance = 0;
    assert(AdvanceCacheGet(&cache, 300, &advance) && advance == 30);
    assert(!AdvanceCacheGet(&cache, 44, &advance));
    assert(!AdvanceCacheGet(&cache, 600, &advance));

    AdvanceCachePut(&cache, 999, 99);
    assert(AdvanceCacheGet(&cache, 999, &advance) && advance == 99);

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testEmptyCache() {
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 0);

    TRInt32 advance = 0;
    AdvanceCachePut(&cache, 0, 5);
    assert(!AdvanceCacheGet(&cache, 0, &advance));

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testLastGlyph() {
    /* The glyph count is not a multiple of the page size. */
    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, 257);

    assert(cache._pageCount == 2);

    TRInt32 advance = 0;
    AdvanceCachePut(&cache, 256, 11);
    assert(AdvanceCacheGet(&cache, 256, &advance) && advance == 11);
    assert(!AdvanceCacheGet(&cache, 257, &advance));

    AdvanceCacheFinalize(&cache);
}

void AdvanceCacheTests::testConcurrentAccess() {
    constexpr size_t NumThreads = 8;
    constexpr TRGlyphID GlyphCount = 2000;

    AdvanceCache cache;
    AdvanceCacheInitialize(&cache, GlyphCount);

    vector<thread> threads;
    for (size_t t = 0; t < NumThreads; t++) {
        threads.emplace_back([&cache]() {
            for (TRGlyphID glyph = 0; glyph < GlyphCount; glyph++) {
                TRInt32 advance = 0;

                /* Every thread stores the same value for a glyph, as the callers do. */
                if (!AdvanceCacheGet(&cache, glyph, &advance)) {
                    AdvanceCachePut(&cache, glyph, glyph * 3);
                } else {
                    assert(advance == glyph * 3);
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (TRGlyphID glyph = 0; glyph < GlyphCount; glyph++) {
        TRInt32 advance = -1;
        assert(AdvanceCacheGet(&cache, glyph, &advance));
        assert(advance == glyph * 3);
    }

    AdvanceCacheFinalize(&cache);
}

#ifdef STANDALONE_TESTING

int main() {
    AdvanceCacheTests tests;
    tests.run();

    return 0;
}

#endif
