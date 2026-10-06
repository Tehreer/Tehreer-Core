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

#include <Tehreer/TRBase.h>

extern "C" {
#include <Core/Allocator.h>
}

#include "AllocatorTests.h"

using namespace std;
using namespace Tehreer;

void AllocatorTests::run() {
    testAllocateBlock();
    testReallocateBlock();
    testCalculateBlockSize();
    testSplitBlock();
    testSplitBlockAlignment();
    testAllocateChunks();
}

void AllocatorTests::testAllocateBlock() {
    constexpr size_t Size = 256;

    auto *block = static_cast<uint8_t *>(AllocatorAllocateBlock(Size));
    assert(block != nullptr);

    memset(block, 0xAB, Size);
    for (size_t i = 0; i < Size; i++) {
        assert(block[i] == 0xAB);
    }

    AllocatorDeallocateBlock(block);
}

void AllocatorTests::testReallocateBlock() {
    constexpr size_t OldSize = 16;
    constexpr size_t NewSize = 4096;

    auto *block = static_cast<uint8_t *>(AllocatorAllocateBlock(OldSize));
    assert(block != nullptr);

    for (size_t i = 0; i < OldSize; i++) {
        block[i] = static_cast<uint8_t>(i);
    }

    block = static_cast<uint8_t *>(AllocatorReallocateBlock(block, NewSize));
    assert(block != nullptr);

    for (size_t i = 0; i < OldSize; i++) {
        assert(block[i] == i);
    }

    AllocatorDeallocateBlock(block);
}

void AllocatorTests::testCalculateBlockSize() {
    const TRUInteger unit = sizeof(void *);
    const TRUInteger empty[] = { 0, 0 };
    const TRUInteger aligned[] = { unit, 2 * unit };
    const TRUInteger unaligned[] = { 1, unit + 1, 2 * unit + 1 };

    assert(AllocatorCalculateBlockSize(empty, 2) == 0);
    assert(AllocatorCalculateBlockSize(aligned, 2) == 3 * unit);
    assert(AllocatorCalculateBlockSize(unaligned, 3) == unit + 2 * unit + 3 * unit);
    assert(AllocatorCalculateBlockSize(unaligned, 0) == 0);
}

void AllocatorTests::testSplitBlock() {
    const TRUInteger unit = sizeof(void *);
    const TRUInteger sizes[] = { unit, 0, 2 * unit };
    void *pointers[3];
    uint8_t block[64];

    AllocatorSplitBlock(block, sizes, 3, pointers);

    assert(pointers[0] == block);
    assert(pointers[1] == nullptr);
    assert(pointers[2] == block + unit);
}

void AllocatorTests::testSplitBlockAlignment() {
    const TRUInteger unit = sizeof(void *);
    const TRUInteger sizes[] = { 1, 3, 5, 7 };
    void *pointers[4];
    uint8_t block[64];

    AllocatorSplitBlock(block, sizes, 4, pointers);

    for (size_t i = 0; i < 4; i++) {
        auto offset = static_cast<uint8_t *>(pointers[i]) - block;
        assert(offset == static_cast<ptrdiff_t>(i * unit));
    }
}

void AllocatorTests::testAllocateChunks() {
    const TRUInteger sizes[] = { 5, 0, 12, 33 };
    constexpr size_t Count = 4;
    void *pointers[Count];

    assert(AllocatorAllocateChunks(sizes, Count, pointers) == TRTrue);
    assert(pointers[1] == nullptr);

    for (size_t i = 0; i < Count; i++) {
        if (sizes[i] > 0) {
            assert(reinterpret_cast<uintptr_t>(pointers[i]) % sizeof(void *) == 0);
            memset(pointers[i], static_cast<int>(i + 1), sizes[i]);
        }
    }

    for (size_t i = 0; i < Count; i++) {
        auto *bytes = static_cast<uint8_t *>(pointers[i]);
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(bytes[j] == i + 1);
        }
    }

    AllocatorDeallocateBlock(pointers[0]);
}

#ifdef STANDALONE_TESTING

int main() {
    AllocatorTests tests;
    tests.run();

    return 0;
}

#endif
