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
#include <vector>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Core/Memory.h>
}

#include "MemoryTests.h"

using namespace std;
using namespace Tehreer;

void MemoryTests::run() {
    testInitialize();
    testAllocateBlock();
    testAllocateMultipleBlocks();
    testAllocateChunks();
    testFinalizeEmpty();
}

void MemoryTests::testInitialize() {
    Memory memory = MemoryMake();
    assert(memory._list == nullptr);

    memory._list = reinterpret_cast<MemoryListRef>(&memory);
    MemoryInitialize(&memory);
    assert(memory._list == nullptr);
}

void MemoryTests::testAllocateBlock() {
    constexpr size_t Size = 128;

    Memory memory = MemoryMake();
    auto *block = static_cast<uint8_t *>(MemoryAllocateBlock(&memory, Size));

    assert(block != nullptr);
    assert(memory._list != nullptr);

    memset(block, 0x5A, Size);
    MemoryFinalize(&memory);
}

void MemoryTests::testAllocateMultipleBlocks() {
    constexpr size_t BlockCount = 32;
    constexpr size_t Size = 64;

    Memory memory = MemoryMake();
    vector<uint8_t *> blocks;

    for (size_t i = 0; i < BlockCount; i++) {
        auto *block = static_cast<uint8_t *>(MemoryAllocateBlock(&memory, Size));
        assert(block != nullptr);

        memset(block, static_cast<int>(i), Size);
        blocks.push_back(block);
    }

    for (size_t i = 0; i < BlockCount; i++) {
        for (size_t j = 0; j < Size; j++) {
            assert(blocks[i][j] == i);
        }
    }

    MemoryFinalize(&memory);
}

void MemoryTests::testAllocateChunks() {
    const TRUInteger sizes[] = { 24, 0, 40 };
    void *pointers[3];

    Memory memory = MemoryMake();
    assert(MemoryAllocateChunks(&memory, sizes, 3, pointers) == TRTrue);

    assert(pointers[0] != nullptr);
    assert(pointers[1] == nullptr);
    assert(static_cast<uint8_t *>(pointers[2]) == static_cast<uint8_t *>(pointers[0]) + 24);

    memset(pointers[0], 0x11, sizes[0]);
    memset(pointers[2], 0x22, sizes[2]);
    assert(static_cast<uint8_t *>(pointers[0])[sizes[0] - 1] == 0x11);

    MemoryFinalize(&memory);
}

void MemoryTests::testFinalizeEmpty() {
    Memory memory = MemoryMake();

    MemoryFinalize(&memory);
}

#ifdef STANDALONE_TESTING

int main() {
    MemoryTests tests;
    tests.run();

    return 0;
}

#endif
