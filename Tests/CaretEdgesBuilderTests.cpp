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
#include <cmath>
#include <cstddef>
#include <vector>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Text/CaretEdgesBuilder.h>
}

#include "CaretEdgesBuilderTests.h"

using namespace std;
using namespace Tehreer;

void CaretEdgesBuilderTests::run() {
    testOneGlyphPerCodeUnit();
    testRightToLeft();
    testLigature();
    testDecomposition();
    testCaretStops();
    testBackward();
    testBackwardLigature();
    testSingleCodeUnit();
    testSurrogatePair();
    testEmpty();
}

/*
 * The expected values were produced by running the original Swift implementation of the
 * algorithm (`CaretEdgesBuilder` of TehreerCocoa) on the same inputs.
 */
static vector<TRFloat> build(bool isBackward, bool isRTL, const vector<TRFloat> &advances,
    const vector<TRUInteger> &clusterMap, const vector<TRBoolean> *caretStops = nullptr) {
    vector<TRFloat> edges(clusterMap.size() + 1, -1.0f);

    CaretEdgesBuild(isBackward, isRTL, advances.data(), advances.size(), clusterMap.data(),
        clusterMap.size(), caretStops ? caretStops->data() : nullptr, edges.data());

    return edges;
}

static void assertEdges(const vector<TRFloat> &actual, const vector<TRFloat> &expected) {
    assert(actual.size() == expected.size());

    for (size_t i = 0; i < actual.size(); i++) {
        assert(fabsf(actual[i] - expected[i]) < 1e-4f);
    }
}

void CaretEdgesBuilderTests::testOneGlyphPerCodeUnit() {
    assertEdges(build(false, false, { 10, 20, 30 }, { 0, 1, 2 }), { 0, 10, 30, 60 });
}

void CaretEdgesBuilderTests::testRightToLeft() {
    assertEdges(build(false, true, { 10, 20, 30 }, { 0, 1, 2 }), { 60, 50, 30, 0 });
}

void CaretEdgesBuilderTests::testLigature() {
    /* Two code units share one glyph, so its advance is split between them. */
    assertEdges(build(false, false, { 30, 10 }, { 0, 0, 1 }), { 0, 15, 30, 40 });
}

void CaretEdgesBuilderTests::testDecomposition() {
    /* The first code unit produces two glyphs, whose advances add up. */
    assertEdges(build(false, false, { 5, 5, 10 }, { 0, 2 }), { 0, 10, 20 });
}

void CaretEdgesBuilderTests::testCaretStops() {
    vector<TRBoolean> stops = { TRTrue, TRFalse, TRTrue };
    assertEdges(build(false, false, { 30 }, { 0, 0, 0 }, &stops), { 0, 15, 15, 30 });

    vector<TRBoolean> others = { TRTrue, TRTrue, TRFalse, TRTrue };
    assertEdges(build(false, false, { 40, 10 }, { 0, 0, 0, 1 }, &others),
        { 0, 50.0f / 3.0f, 100.0f / 3.0f, 100.0f / 3.0f, 50 });

    vector<TRBoolean> lastOff = { TRTrue, TRTrue, TRTrue, TRFalse };
    assertEdges(build(false, false, { 12, 12 }, { 0, 0, 1, 1 }, &lastOff), { 0, 6, 12, 18, 24 });

    /* Allowing a stop everywhere is the same as not giving any flags. */
    vector<TRBoolean> all(3, TRTrue);
    assertEdges(build(false, false, { 30, 10 }, { 0, 0, 1 }, &all),
        build(false, false, { 30, 10 }, { 0, 0, 1 }));
}

void CaretEdgesBuilderTests::testBackward() {
    /* Glyphs are in the writing direction, while the code units run against it. */
    assertEdges(build(true, false, { 30, 20, 10 }, { 2, 1, 0 }), { 0, 10, 30, 60 });
    assertEdges(build(true, true, { 30, 20, 10 }, { 2, 1, 0 }), { 60, 50, 30, 0 });
}

void CaretEdgesBuilderTests::testBackwardLigature() {
    assertEdges(build(true, true, { 10, 60 }, { 1, 1, 1, 0, 0 }), { 70, 50, 30, 10, 5, 0 });

    vector<TRBoolean> stops = { TRTrue, TRFalse, TRTrue, TRTrue, TRFalse };
    assertEdges(build(true, true, { 10, 60 }, { 1, 1, 1, 0, 0 }, &stops),
        { 70, 40, 40, 10, 5, 0 });
}

void CaretEdgesBuilderTests::testSingleCodeUnit() {
    assertEdges(build(false, false, { 7 }, { 0 }), { 0, 7 });
}

void CaretEdgesBuilderTests::testSurrogatePair() {
    /* The low surrogate has no cluster of its own and maps to the glyph of the high one. */
    assertEdges(build(false, false, { 10, 20, 10 }, { 0, 1, 1, 2 }), { 0, 10, 20, 30, 40 });
}

void CaretEdgesBuilderTests::testEmpty() {
    TRFloat edges[1] = { -1.0f };

    CaretEdgesBuild(false, false, nullptr, 0, nullptr, 0, nullptr, edges);
    assert(edges[0] == 0.0f);
}

#ifdef STANDALONE_TESTING

int main() {
    CaretEdgesBuilderTests tests;
    tests.run();

    return 0;
}

#endif
