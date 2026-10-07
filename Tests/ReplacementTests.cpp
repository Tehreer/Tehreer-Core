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
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRReplacement.h>

extern "C" {
#include <API/TRReplacement.h>
#include <Core/AtomicUInt.h>
}

#include "ReplacementTests.h"

using namespace std;
using namespace Tehreer;

void ReplacementTests::run() {
    testCreate();
    testInvalidCreate();
    testComputeRoom();
    testMissingCallbacks();
    testRetainRelease();
    testFinalizeOnce();
    testConcurrentRoom();
}

namespace {

struct State {
    atomic<int> roomCalls{0};
    atomic<int> finalizeCalls{0};
    atomic<TRFloat> lastWidth{-1.0f};
};

void computeRoom(void *userData, TRFloat layoutWidth, TRReplacementRoom *room) {
    auto *state = static_cast<State *>(userData);
    state->roomCalls++;
    state->lastWidth = layoutWidth;

    room->ascent = 12.0f;
    room->descent = 3.0f;
    room->extent = layoutWidth * 2.0f;
}

void finalize(void *userData) {
    static_cast<State *>(userData)->finalizeCalls++;
}

}

void ReplacementTests::testCreate() {
    State state;
    TRReplacementCallbacks callbacks = { computeRoom, finalize };

    TRReplacementRef inlineOne = TRReplacementCreate(&callbacks, &state, TRReplacementKindInline);
    assert(inlineOne != nullptr);
    assert(TRReplacementGetKind(inlineOne) == TRReplacementKindInline);

    TRReplacementRef block = TRReplacementCreate(&callbacks, &state, TRReplacementKindBlock);
    assert(block != nullptr);
    assert(TRReplacementGetKind(block) == TRReplacementKindBlock);

    TRReplacementRelease(block);
    TRReplacementRelease(inlineOne);
}

void ReplacementTests::testInvalidCreate() {
    TRReplacementCallbacks none = {};

    assert(TRReplacementCreate(nullptr, nullptr, TRReplacementKindInline) == nullptr);

    /* A kind that is not known is refused. */
    assert(TRReplacementCreate(&none, nullptr, 7) == nullptr);
}

void ReplacementTests::testComputeRoom() {
    State state;
    TRReplacementCallbacks callbacks = { computeRoom, finalize };
    TRReplacementRef replacement = TRReplacementCreate(&callbacks, &state, TRReplacementKindInline);

    TRReplacementRoom room = {};
    TRReplacementComputeRoom(replacement, 0.0f, &room);
    assert(state.roomCalls == 1);
    assert(state.lastWidth == 0.0f);
    assert(room.ascent == 12.0f && room.descent == 3.0f && room.extent == 0.0f);

    /* The layout width is passed on, and the result is not cached. */
    TRReplacementComputeRoom(replacement, 150.0f, &room);
    assert(state.roomCalls == 2);
    assert(state.lastWidth == 150.0f);
    assert(room.extent == 300.0f);

    TRReplacementRelease(replacement);
}

void ReplacementTests::testMissingCallbacks() {
    /* Without a callback to compute the room, it is zero and nothing is called on release. */
    TRReplacementCallbacks none = {};
    TRReplacementRef replacement = TRReplacementCreate(&none, nullptr, TRReplacementKindInline);

    TRReplacementRoom room = { 1.0f, 2.0f, 3.0f };
    TRReplacementComputeRoom(replacement, 100.0f, &room);
    assert(room.ascent == 0.0f && room.descent == 0.0f && room.extent == 0.0f);

    TRReplacementRelease(replacement);
}

void ReplacementTests::testRetainRelease() {
    State state;
    TRReplacementCallbacks callbacks = { computeRoom, finalize };
    TRReplacementRef replacement = TRReplacementCreate(&callbacks, &state, TRReplacementKindInline);

    assert(TRReplacementRetain(replacement) == replacement);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 2);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 0);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 1);
}

void ReplacementTests::testFinalizeOnce() {
    State state;
    TRReplacementCallbacks callbacks = { computeRoom, finalize };
    TRReplacementRef replacement = TRReplacementCreate(&callbacks, &state, TRReplacementKindInline);

    /* The callbacks are copied, so the caller does not have to keep them alive. */
    callbacks.computeRoom = nullptr;
    callbacks.finalize = nullptr;

    TRReplacementRoom room = {};
    TRReplacementComputeRoom(replacement, 1.0f, &room);
    assert(room.ascent == 12.0f);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 1);
}

void ReplacementTests::testConcurrentRoom() {
    State state;
    TRReplacementCallbacks callbacks = { computeRoom, finalize };
    TRReplacementRef replacement = TRReplacementCreate(&callbacks, &state, TRReplacementKindInline);

    /* A replacement is shared between the threads that lay out text. */
    vector<thread> threads;
    for (size_t i = 0; i < 8; i++) {
        threads.emplace_back([replacement]() {
            for (size_t j = 0; j < 1000; j++) {
                TRReplacementRetain(replacement);

                TRReplacementRoom room = {};
                TRReplacementComputeRoom(replacement, 0.0f, &room);
                assert(room.ascent == 12.0f);

                TRReplacementRelease(replacement);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(state.roomCalls == 8000);
    assert(AtomicUIntLoad(&replacement->_base.retainCount) == 1);

    TRReplacementRelease(replacement);
    assert(state.finalizeCalls == 1);
}

#ifdef STANDALONE_TESTING

int main() {
    ReplacementTests tests;
    tests.run();

    return 0;
}

#endif
