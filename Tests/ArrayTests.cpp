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
#include <Core/Array.h>
}

#include "ArrayTests.h"

using namespace Tehreer;

void ArrayTests::run() {
    testInitialState();
    testAppendAndGet();
    testGrowth();
    testResize();
    testStructItems();
}

void ArrayTests::testInitialState() {
    Array array;
    ArrayInitialize(&array, sizeof(int));

    assert(ArrayGetCount(&array) == 0);
    assert(ArrayGetItems(&array) == nullptr);

    ArrayFinalize(&array);
    assert(ArrayGetCount(&array) == 0);
}

void ArrayTests::testAppendAndGet() {
    Array array;
    ArrayInitialize(&array, sizeof(int));

    for (int value = 10; value < 13; value++) {
        assert(ArrayAppend(&array, &value));
    }

    assert(ArrayGetCount(&array) == 3);
    assert(*static_cast<int *>(ArrayGetItem(&array, 0)) == 10);
    assert(*static_cast<int *>(ArrayGetItem(&array, 1)) == 11);
    assert(*static_cast<int *>(ArrayGetItem(&array, 2)) == 12);

    *static_cast<int *>(ArrayGetItem(&array, 1)) = 99;
    assert(static_cast<int *>(ArrayGetItems(&array))[1] == 99);

    ArrayFinalize(&array);
}

void ArrayTests::testGrowth() {
    constexpr int Count = 1000;

    Array array;
    ArrayInitialize(&array, sizeof(int));

    for (int value = 0; value < Count; value++) {
        assert(ArrayAppend(&array, &value));
    }

    assert(ArrayGetCount(&array) == Count);
    for (int value = 0; value < Count; value++) {
        assert(*static_cast<int *>(ArrayGetItem(&array, value)) == value);
    }

    ArrayFinalize(&array);
}

void ArrayTests::testResize() {
    Array array;
    ArrayInitialize(&array, sizeof(int));

    int value = 7;
    assert(ArrayAppend(&array, &value));

    /* The items that are added are zero. */
    assert(ArrayResize(&array, 100));
    assert(ArrayGetCount(&array) == 100);
    assert(*static_cast<int *>(ArrayGetItem(&array, 0)) == 7);
    for (TRUInteger index = 1; index < 100; index++) {
        assert(*static_cast<int *>(ArrayGetItem(&array, index)) == 0);
    }

    assert(ArrayResize(&array, 1));
    assert(ArrayGetCount(&array) == 1);

    ArrayFinalize(&array);
}

void ArrayTests::testStructItems() {
    struct Item {
        void *pointer;
        double number;
        char tag;
    };

    Array array;
    ArrayInitialize(&array, sizeof(Item));

    for (int index = 0; index < 20; index++) {
        Item item = { &array, index * 0.5, static_cast<char>('a' + index) };
        assert(ArrayAppend(&array, &item));
    }

    for (int index = 0; index < 20; index++) {
        const Item *item = static_cast<const Item *>(ArrayGetItem(&array, index));
        assert(item->pointer == &array);
        assert(item->number == index * 0.5);
        assert(item->tag == 'a' + index);
    }

    ArrayFinalize(&array);
}

#ifdef STANDALONE_TESTING

int main() {
    ArrayTests tests;
    tests.run();

    return 0;
}

#endif
