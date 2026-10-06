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

#include <ft2build.h>
#include FT_OUTLINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRPath.h>

extern "C" {
#include <API/TRPath.h>
#include <Core/AtomicUInt.h>
}

#include "TestPath.h"

#include "PathTests.h"

using namespace std;
using namespace Tehreer;

void PathTests::run() {
    testPolygon();
    testQuadraticCurve();
    testCubicCurve();
    testMultipleContours();
    testEmptyOutline();
    testTransform();
    testMissingCallbacks();
    testOutlineIsCopied();
    testRetainRelease();
}

namespace {

/* Builds an outline from points in pixels, which FreeType stores in 26.6 format. */
struct TestOutline {
    vector<FT_Vector> points;
    vector<unsigned char> tags;
    vector<unsigned short> contours;

    void add(int x, int y, unsigned char tag) {
        points.push_back({ x * 64, y * 64 });
        tags.push_back(tag);
    }

    void endContour() {
        contours.push_back(static_cast<unsigned short>(points.size() - 1));
    }

    TRPathRef createPath() {
        FT_Outline outline = {};
        outline.n_points = static_cast<short>(points.size());
        outline.n_contours = static_cast<short>(contours.size());
        outline.points = points.data();
        outline.tags = tags.data();
        outline.contours = contours.data();

        return TRPathCreateFromOutline(&outline);
    }
};

const unsigned char On = FT_CURVE_TAG_ON;
const unsigned char Conic = FT_CURVE_TAG_CONIC;
const unsigned char Cubic = FT_CURVE_TAG_CUBIC;

bool isPoint(const TRPoint &point, TRFloat x, TRFloat y) {
    return point.x == x && point.y == y;
}

}

void PathTests::testPolygon() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(10, 0, On);
    outline.add(10, 20, On);
    outline.endContour();

    TRPathRef path = outline.createPath();
    assert(path != nullptr);

    /* The y axis is flipped, and FreeType closes the contour with a line to its start. */
    vector<PathEvent> events = enumeratePath(path);
    assert(events.size() == 5);

    assert(events[0].kind == PathEvent::Move && isPoint(events[0].points[0], 0.0f, 0.0f));
    assert(events[1].kind == PathEvent::Line && isPoint(events[1].points[0], 10.0f, 0.0f));
    assert(events[2].kind == PathEvent::Line && isPoint(events[2].points[0], 10.0f, -20.0f));
    assert(events[3].kind == PathEvent::Line && isPoint(events[3].points[0], 0.0f, 0.0f));
    assert(events[4].kind == PathEvent::Close);

    TRPathRelease(path);
}

void PathTests::testQuadraticCurve() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(5, 10, Conic);
    outline.add(10, 0, On);
    outline.endContour();

    TRPathRef path = outline.createPath();
    vector<PathEvent> events = enumeratePath(path);

    assert(events.size() == 4);
    assert(events[0].kind == PathEvent::Move && isPoint(events[0].points[0], 0.0f, 0.0f));
    assert(events[1].kind == PathEvent::Quad);
    assert(isPoint(events[1].points[0], 5.0f, -10.0f));
    assert(isPoint(events[1].points[1], 10.0f, 0.0f));
    /* The contour is closed back to its start with a straight line. */
    assert(events[2].kind == PathEvent::Line && isPoint(events[2].points[0], 0.0f, 0.0f));
    assert(events[3].kind == PathEvent::Close);

    TRPathRelease(path);

    /* Consecutive off-curve points get an implied on-curve point in between. */
    TestOutline implied;
    implied.add(0, 0, On);
    implied.add(4, 8, Conic);
    implied.add(8, 8, Conic);
    implied.add(12, 0, On);
    implied.endContour();

    path = implied.createPath();
    events = enumeratePath(path);

    assert(events[1].kind == PathEvent::Quad);
    assert(isPoint(events[1].points[0], 4.0f, -8.0f));
    assert(isPoint(events[1].points[1], 6.0f, -8.0f));
    assert(events[2].kind == PathEvent::Quad);
    assert(isPoint(events[2].points[0], 8.0f, -8.0f));
    assert(isPoint(events[2].points[1], 12.0f, 0.0f));

    TRPathRelease(path);
}

void PathTests::testCubicCurve() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(0, 10, Cubic);
    outline.add(10, 10, Cubic);
    outline.add(10, 0, On);
    outline.endContour();

    TRPathRef path = outline.createPath();
    vector<PathEvent> events = enumeratePath(path);

    assert(events.size() == 4);
    assert(events[1].kind == PathEvent::Cubic);
    assert(isPoint(events[1].points[0], 0.0f, -10.0f));
    assert(isPoint(events[1].points[1], 10.0f, -10.0f));
    assert(isPoint(events[1].points[2], 10.0f, 0.0f));
    assert(events[3].kind == PathEvent::Close);

    TRPathRelease(path);
}

void PathTests::testMultipleContours() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(10, 0, On);
    outline.add(10, 10, On);
    outline.endContour();
    outline.add(2, 2, On);
    outline.add(4, 2, On);
    outline.add(4, 4, On);
    outline.endContour();

    TRPathRef path = outline.createPath();
    vector<PathEvent> events = enumeratePath(path);

    /* Each contour has a move, three lines and a close, in this order. */
    assert(events.size() == 10);
    for (size_t contour = 0; contour < 2; contour++) {
        size_t base = contour * 5;
        assert(events[base].kind == PathEvent::Move);
        assert(events[base + 1].kind == PathEvent::Line);
        assert(events[base + 2].kind == PathEvent::Line);
        assert(events[base + 3].kind == PathEvent::Line);
        assert(events[base + 4].kind == PathEvent::Close);
    }
    assert(isPoint(events[5].points[0], 2.0f, -2.0f));

    TRPathRelease(path);
}

void PathTests::testEmptyOutline() {
    TestOutline outline;
    TRPathRef path = outline.createPath();
    assert(path != nullptr);

    assert(enumeratePath(path).empty());

    TRPathRelease(path);
}

void PathTests::testTransform() {
    TestOutline outline;
    outline.add(1, 2, On);
    outline.add(3, 4, On);
    outline.add(5, 0, On);
    outline.endContour();

    TRPathRef path = outline.createPath();

    /* Scale and translation, applied after the y flip. */
    TRAffineTransform scale = { 2.0f, 0.0f, 0.0f, 3.0f, 10.0f, 20.0f };
    vector<PathEvent> events = enumeratePath(path, &scale);
    assert(isPoint(events[0].points[0], 1.0f * 2.0f + 10.0f, -2.0f * 3.0f + 20.0f));
    assert(isPoint(events[1].points[0], 3.0f * 2.0f + 10.0f, -4.0f * 3.0f + 20.0f));

    /* x' = a * x + c * y and y' = b * x + d * y. */
    TRAffineTransform shear = { 1.0f, 0.5f, 0.25f, 1.0f, 0.0f, 0.0f };
    events = enumeratePath(path, &shear);
    assert(isPoint(events[1].points[0], 3.0f + 0.25f * -4.0f, 0.5f * 3.0f + -4.0f));

    /* The identity changes nothing. */
    TRAffineTransform identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    vector<PathEvent> plain = enumeratePath(path);
    events = enumeratePath(path, &identity);
    assert(events.size() == plain.size());
    for (size_t i = 0; i < events.size(); i++) {
        assert(events[i].kind == plain[i].kind);
    }

    TRPathRelease(path);
}

void PathTests::testMissingCallbacks() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(5, 10, Conic);
    outline.add(10, 0, On);
    outline.add(10, 10, Cubic);
    outline.add(0, 10, Cubic);
    outline.add(0, 0, On);
    outline.endContour();

    TRPathRef path = outline.createPath();

    /* Neither a missing set of callbacks nor missing members crash. */
    TRPathEnumerate(path, nullptr, nullptr, nullptr);

    TRPathCallbacks none = {};
    TRPathEnumerate(path, nullptr, &none, nullptr);

    size_t closeCount = 0;
    TRPathCallbacks onlyClose = {};
    onlyClose.close = [](void *data) { (*static_cast<size_t *>(data))++; };
    TRPathEnumerate(path, nullptr, &onlyClose, &closeCount);
    assert(closeCount == 1);

    TRPathRelease(path);
}

void PathTests::testOutlineIsCopied() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(10, 0, On);
    outline.add(10, 10, On);
    outline.endContour();

    TRPathRef path = outline.createPath();

    for (auto &point : outline.points) {
        point.x = 9999;
        point.y = 9999;
    }
    outline.points.clear();
    outline.points.shrink_to_fit();

    vector<PathEvent> events = enumeratePath(path);
    assert(isPoint(events[1].points[0], 10.0f, 0.0f));
    assert(isPoint(events[2].points[0], 10.0f, -10.0f));

    TRPathRelease(path);
}

void PathTests::testRetainRelease() {
    TestOutline outline;
    outline.add(0, 0, On);
    outline.add(1, 1, On);
    outline.add(2, 0, On);
    outline.endContour();

    TRPathRef path = outline.createPath();

    assert(TRPathRetain(path) == path);
    assert(AtomicUIntLoad(&path->_base.retainCount) == 2);

    TRPathRelease(path);
    assert(AtomicUIntLoad(&path->_base.retainCount) == 1);
    assert(enumeratePath(path).size() == 5);

    TRPathRelease(path);
}

#ifdef STANDALONE_TESTING

int main() {
    PathTests tests;
    tests.run();

    return 0;
}

#endif
