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
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Core/AtomicUInt.h>
#include <Core/Object.h>
}

#include "ObjectTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 10000;

void ObjectTests::run() {
    testCreate();
    testChunks();
    testRetainRelease();
    testFinalizer();
    testNoFinalizer();
    testThreadSafety();
}

struct TestObject {
    ObjectBase base;
    uint32_t value;
};

static atomic<size_t> finalizeCount{0};
static uint32_t finalizedValue = 0;

static void FinalizeTestObject(ObjectRef object) {
    finalizedValue = static_cast<TestObject *>(object)->value;
    finalizeCount++;
}

void ObjectTests::testCreate() {
    const TRUInteger size = sizeof(TestObject);
    void *pointer = nullptr;

    auto *object = static_cast<TestObject *>(ObjectCreate(&size, 1, &pointer, nullptr));

    assert(object != nullptr);
    assert(object == pointer);
    assert(AtomicUIntLoad(&object->base.retainCount) == 1);

    ObjectRelease(object);
}

void ObjectTests::testChunks() {
    const TRUInteger sizes[] = { sizeof(TestObject), 5, 0, 32 };
    void *pointers[4];

    auto *object = static_cast<TestObject *>(ObjectCreate(sizes, 4, pointers, nullptr));

    assert(object == pointers[0]);
    assert(pointers[1] != nullptr);
    assert(pointers[2] == nullptr);
    assert(pointers[3] != nullptr);
    assert(reinterpret_cast<uintptr_t>(pointers[3]) % sizeof(void *) == 0);

    object->value = 0xCAFE;
    memset(pointers[1], 0x11, sizes[1]);
    memset(pointers[3], 0x22, sizes[3]);
    assert(object->value == 0xCAFE);

    ObjectRelease(object);
}

void ObjectTests::testRetainRelease() {
    const TRUInteger size = sizeof(TestObject);
    void *pointer = nullptr;

    auto *object = static_cast<TestObject *>(ObjectCreate(&size, 1, &pointer, nullptr));

    assert(ObjectRetain(object) == object);
    assert(AtomicUIntLoad(&object->base.retainCount) == 2);

    ObjectRetain(object);
    assert(AtomicUIntLoad(&object->base.retainCount) == 3);

    ObjectRelease(object);
    ObjectRelease(object);
    assert(AtomicUIntLoad(&object->base.retainCount) == 1);

    ObjectRelease(object);
}

void ObjectTests::testFinalizer() {
    const TRUInteger size = sizeof(TestObject);
    void *pointer = nullptr;

    finalizeCount = 0;
    finalizedValue = 0;

    auto *object = static_cast<TestObject *>(ObjectCreate(&size, 1, &pointer, FinalizeTestObject));
    object->value = 0x1234;

    ObjectRetain(object);
    ObjectRelease(object);
    assert(finalizeCount == 0);

    ObjectRelease(object);
    assert(finalizeCount == 1);
    assert(finalizedValue == 0x1234);
}

void ObjectTests::testNoFinalizer() {
    const TRUInteger size = sizeof(ObjectBase);
    void *pointer = nullptr;

    ObjectRef object = ObjectCreate(&size, 1, &pointer, nullptr);
    assert(object != nullptr);

    ObjectRelease(object);
}

void ObjectTests::testThreadSafety() {
    const TRUInteger size = sizeof(TestObject);
    void *pointer = nullptr;

    finalizeCount = 0;

    auto *object = static_cast<TestObject *>(ObjectCreate(&size, 1, &pointer, FinalizeTestObject));
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&]() {
            for (size_t j = 0; j < Iterations; j++) {
                ObjectRetain(object);
                ObjectRelease(object);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(AtomicUIntLoad(&object->base.retainCount) == 1);
    assert(finalizeCount == 0);

    ObjectRelease(object);
    assert(finalizeCount == 1);
}

#ifdef STANDALONE_TESTING

int main() {
    ObjectTests tests;
    tests.run();

    return 0;
}

#endif
