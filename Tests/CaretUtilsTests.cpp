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
#include <vector>

#include <Tehreer/TRBase.h>

extern "C" {
#include <Layout/CaretUtils.h>
}

#include "CaretUtilsTests.h"

using namespace std;
using namespace Tehreer;

void CaretUtilsTests::run() {
    testDistance();
    testLeftMargin();
    testIndexOfEdgeLeftToRight();
    testIndexOfEdgeRightToLeft();
    testIndexOfEdgeInSubrange();
    testIndexOfEdgeSingleAndZeroWidth();
}

/*
 * The expected values were produced by running the original Swift implementation of the
 * algorithm (`CaretUtils` of TehreerCocoa) on the same inputs.
 */
static const TRFloat LeftToRight[] = { 0.0f, 10.0f, 25.0f, 40.0f };
static const TRFloat RightToLeft[] = { 40.0f, 25.0f, 10.0f, 0.0f };

static vector<TRUInteger> indexes(const TRFloat *edges, bool isRTL, TRUInteger first, TRUInteger last,
    const vector<TRFloat> &distances) {
    vector<TRUInteger> result;

    for (TRFloat distance : distances) {
        result.push_back(CaretUtilsGetIndexOfEdge(edges, isRTL, distance, first, last));
    }

    return result;
}

void CaretUtilsTests::testDistance() {
    assert(CaretUtilsGetDistance(LeftToRight, false, 1, 3) == 30.0f);
    assert(CaretUtilsGetDistance(LeftToRight, false, 2, 2) == 0.0f);
    assert(CaretUtilsGetDistance(LeftToRight, false, 0, 3) == 40.0f);

    /* Right-to-left edges go down, so the distance is the first edge minus the last. */
    assert(CaretUtilsGetDistance(RightToLeft, true, 0, 2) == 30.0f);
    assert(CaretUtilsGetDistance(RightToLeft, true, 1, 3) == 25.0f);
    assert(CaretUtilsGetDistance(RightToLeft, true, 3, 3) == 0.0f);
}

void CaretUtilsTests::testLeftMargin() {
    assert(CaretUtilsGetLeftMargin(LeftToRight, false, 1, 3) == 10.0f);
    assert(CaretUtilsGetLeftMargin(LeftToRight, false, 0, 3) == 0.0f);

    /* The left side of a right-to-left range is its last edge. */
    assert(CaretUtilsGetLeftMargin(RightToLeft, true, 0, 2) == 10.0f);
    assert(CaretUtilsGetLeftMargin(RightToLeft, true, 0, 3) == 0.0f);
}

void CaretUtilsTests::testIndexOfEdgeLeftToRight() {
    const vector<TRFloat> distances = { -5, 0, 4, 5, 5.01f, 10, 17.5f, 17.51f, 25, 32.5f, 39, 40, 100 };

    assert((indexes(LeftToRight, false, 0, 3, distances)
            == vector<TRUInteger>{ 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3 }));
}

void CaretUtilsTests::testIndexOfEdgeRightToLeft() {
    const vector<TRFloat> distances = { -5, 0, 4, 5, 5.01f, 10, 17.5f, 17.51f, 25, 32.5f, 39, 40, 100 };

    assert((indexes(RightToLeft, true, 0, 3, distances)
            == vector<TRUInteger>{ 0, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 3, 3 }));
}

void CaretUtilsTests::testIndexOfEdgeInSubrange() {
    const vector<TRFloat> distances = { -5, 0, 4, 7.4f, 7.5f, 7.51f, 15, 100 };

    assert((indexes(LeftToRight, false, 1, 2, distances) == vector<TRUInteger>{ 1, 1, 1, 1, 1, 2, 2, 2 }));
    assert((indexes(RightToLeft, true, 1, 2, distances) == vector<TRUInteger>{ 1, 2, 2, 2, 2, 1, 2, 2 }));
}

void CaretUtilsTests::testIndexOfEdgeSingleAndZeroWidth() {
    /* A range with one edge always gives it. */
    assert(CaretUtilsGetIndexOfEdge(LeftToRight, false, 3.0f, 2, 2) == 2);
    assert(CaretUtilsGetIndexOfEdge(RightToLeft, true, 3.0f, 2, 2) == 2);

    /* Edges at the same place are all covered by any distance that is not negative. */
    const TRFloat flat[] = { 0.0f, 0.0f, 0.0f };
    assert(CaretUtilsGetIndexOfEdge(flat, false, 0.0f, 0, 2) == 2);
    assert(CaretUtilsGetIndexOfEdge(flat, false, 1.0f, 0, 2) == 2);
    assert(CaretUtilsGetIndexOfEdge(flat, true, 0.0f, 0, 2) == 2);
}

#ifdef STANDALONE_TESTING

int main() {
    CaretUtilsTests tests;
    tests.run();

    return 0;
}

#endif
