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
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphCache.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRGlyphCache.h>
#include <API/TRGlyphImage.h>
#include <API/TRRenderer.h>
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
}

#include "TestPath.h"
#include "TestTypeface.h"

#include "RendererTests.h"

using namespace std;
using namespace Tehreer;

void RendererTests::run() {
    testDefaults();
    testWithoutTypeface();
    testSettersRetain();
    testImages();
    testRenderScale();
    testScalesAndSkew();
    testIsRenderable();
    testGlyphBoundingBox();
    testPlacementsLeftToRight();
    testPlacementsRightToLeft();
    testPlacementOffsets();
    testStrokePlacements();
    testReleasePlacements();
    testRunBoundingBox();
    testRunBoundingBoxRightToLeft();
    testEmptyRuns();
    testEnumerateGlyphPaths();
    testEnumerateGlyphPathsRightToLeft();
    testEnumerateWithRenderScale();
    testColorGlyphs();
    testConcurrentRenderers();
}

/*
 * The images of the glyphs 'a', 'b' and 'c' of the test font at the type size of 32, which are
 * 15 by 17, 15 by 24 and 15 by 17 pixels, with the left and top edges at 1 and 17, at 2 and 24,
 * and at 1 and 17.
 */
constexpr TRGlyphID GlyphA = 1;
constexpr TRGlyphID GlyphB = 2;
constexpr TRGlyphID GlyphC = 3;

struct Fixture {
    TRTypefaceRef typeface;
    TRGlyphCacheRef cache;
    TRRendererRef renderer;

    explicit Fixture(const char *fontName = "Roboto-Regular.abc.ttf") {
        typeface = createTestTypeface(fontName);
        cache = TRGlyphCacheCreate(4 * 1024 * 1024);
        renderer = TRRendererCreate();

        TRRendererSetTypeface(renderer, typeface);
        TRRendererSetGlyphCache(renderer, cache);
        TRRendererSetTypeSize(renderer, 32.0f);
    }

    ~Fixture() {
        TRRendererRelease(renderer);
        TRGlyphCacheRelease(cache);
        TRTypefaceRelease(typeface);
    }
};

static void assertSize(TRGlyphImageRef image, int left, int top, uint32_t width, uint32_t height) {
    assert(image != nullptr);
    assert(TRGlyphImageGetLeft(image) == left);
    assert(TRGlyphImageGetTop(image) == top);
    assert(TRGlyphImageGetWidth(image) == width);
    assert(TRGlyphImageGetHeight(image) == height);
}

static bool near(TRFloat a, TRFloat b, TRFloat tolerance = 1e-4f) {
    return fabsf(a - b) <= tolerance;
}

void RendererTests::testDefaults() {
    TRRendererRef renderer = TRRendererCreate();
    assert(renderer != nullptr);

    assert(renderer->cache == nullptr);
    assert(renderer->typeface == nullptr);
    assert(renderer->typeSize == 16.0f);
    assert(renderer->scaleX == 1.0f && renderer->scaleY == 1.0f);
    assert(renderer->skewX == 0.0f);
    assert(renderer->renderScale == 1.0f);
    assert(renderer->writingDirection == TRWritingDirectionLeftToRight);
    assert(renderer->foregroundColor == TRColorMake(0xFF, 0, 0, 0));
    assert(renderer->strokeWidth == 1.0f);
    assert(renderer->strokeCap == TRStrokeCapButt);
    assert(renderer->strokeJoin == TRStrokeJoinRound);
    assert(renderer->strokeMiter == 1.0f);

    TRRendererSetTypeSize(renderer, 20.0f);
    TRRendererSetScaleX(renderer, 2.0f);
    TRRendererSetScaleY(renderer, 3.0f);
    TRRendererSetSkewX(renderer, 0.5f);
    TRRendererSetRenderScale(renderer, 4.0f);
    TRRendererSetWritingDirection(renderer, TRWritingDirectionRightToLeft);
    TRRendererSetForegroundColor(renderer, 0x12345678);
    TRRendererSetStrokeWidth(renderer, 7.0f);
    TRRendererSetStrokeCap(renderer, TRStrokeCapSquare);
    TRRendererSetStrokeJoin(renderer, TRStrokeJoinBevel);
    TRRendererSetStrokeMiter(renderer, 9.0f);

    assert(renderer->typeSize == 20.0f);
    assert(renderer->scaleX == 2.0f && renderer->scaleY == 3.0f);
    assert(renderer->skewX == 0.5f);
    assert(renderer->renderScale == 4.0f);
    assert(renderer->writingDirection == TRWritingDirectionRightToLeft);
    assert(renderer->foregroundColor == 0x12345678);
    assert(renderer->strokeWidth == 7.0f);
    assert(renderer->strokeCap == TRStrokeCapSquare);
    assert(renderer->strokeJoin == TRStrokeJoinBevel);
    assert(renderer->strokeMiter == 9.0f);

    TRRendererRelease(renderer);
}

void RendererTests::testWithoutTypeface() {
    TRRendererRef renderer = TRRendererCreate();
    TRGlyphID glyphs[] = { GlyphA, GlyphB };
    TRPoint offsets[] = { { 0, 0 }, { 0, 0 } };
    TRFloat advances[] = { 10.0f, 10.0f };

    assert(TRRendererGetGlyphImage(renderer, GlyphA) == nullptr);
    assert(TRRendererGetStrokeImage(renderer, GlyphA) == nullptr);
    assert(TRRendererGetGlyphPath(renderer, GlyphA) == nullptr);

    TRRect box = TRRendererGetGlyphBoundingBox(renderer, GlyphA);
    assert(box.size.width == 0.0f && box.size.height == 0.0f);
    box = TRRendererGetRunBoundingBox(renderer, glyphs, offsets, advances, 2);
    assert(box.origin.x == 0.0f && box.size.width == 0.0f && box.size.height == 0.0f);

    TRGlyphPlacement placements[2];
    TRRendererGetGlyphPlacements(renderer, TRGlyphImageKindFill, glyphs, offsets, advances, 2, placements);
    assert(placements[0].image == nullptr && placements[1].image == nullptr);

    size_t calls = 0;
    TRPathCallbacks callbacks = {};
    callbacks.moveTo = [](void *data, TRFloat, TRFloat) { (*static_cast<size_t *>(data))++; };
    TRRendererEnumerateGlyphPaths(renderer, glyphs, offsets, advances, 2, &callbacks, &calls);
    assert(calls == 0);

    TRRendererRelease(renderer);
}

void RendererTests::testSettersRetain() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRTypefaceRef other = createTestTypeface("Roboto-Variable.abc.ttf");
    TRGlyphCacheRef cache = TRGlyphCacheCreate(1024);
    TRRendererRef renderer = TRRendererCreate();

    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 1);

    TRRendererSetTypeface(renderer, typeface);
    TRRendererSetTypeface(renderer, typeface);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 2);

    TRRendererSetTypeface(renderer, other);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 1);
    assert(AtomicUIntLoad(&other->_base.retainCount) == 2);

    TRRendererSetTypeface(renderer, nullptr);
    assert(AtomicUIntLoad(&other->_base.retainCount) == 1);

    TRRendererSetGlyphCache(renderer, cache);
    TRRendererSetGlyphCache(renderer, cache);
    assert(AtomicUIntLoad(&cache->_base.retainCount) == 2);
    TRRendererSetGlyphCache(renderer, nullptr);
    assert(AtomicUIntLoad(&cache->_base.retainCount) == 1);

    /* Everything that the renderer holds is released with it. */
    TRRendererSetTypeface(renderer, typeface);
    TRRendererSetGlyphCache(renderer, cache);
    TRRendererRelease(renderer);
    assert(AtomicUIntLoad(&typeface->_base.retainCount) == 1);
    assert(AtomicUIntLoad(&cache->_base.retainCount) == 1);

    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(other);
    TRTypefaceRelease(typeface);
}

void RendererTests::testImages() {
    Fixture f;

    TRGlyphImageRef a = TRRendererGetGlyphImage(f.renderer, GlyphA);
    TRGlyphImageRef b = TRRendererGetGlyphImage(f.renderer, GlyphB);
    TRGlyphImageRef c = TRRendererGetGlyphImage(f.renderer, GlyphC);

    assertSize(a, 1, 17, 15, 17);
    assertSize(b, 2, 24, 15, 24);
    assertSize(c, 1, 17, 15, 17);
    assert(TRGlyphImageGetFormat(a) == TRGlyphImageFormatAlpha);

    /* The images have ink. */
    bool hasInk = false;
    for (size_t i = 0; i < TRGlyphImageGetByteCount(a); i++) {
        hasInk = hasInk || TRGlyphImageGetPixelsPtr(a)[i] != 0;
    }
    assert(hasInk);

    /* A stroke is a little bigger than the fill. */
    TRGlyphImageRef stroke = TRRendererGetStrokeImage(f.renderer, GlyphA);
    assertSize(stroke, 1, 18, 16, 19);

    /* The notdef glyph of this font has nothing to show. */
    assert(TRRendererGetGlyphImage(f.renderer, 0) == nullptr);

    for (TRGlyphImageRef image : { a, b, c, stroke }) {
        TRGlyphImageRelease(image);
    }
}

void RendererTests::testRenderScale() {
    Fixture f;

    /* Half the type size at twice the render scale is the same pixel size, so the same image. */
    TRGlyphImageRef base = TRRendererGetGlyphImage(f.renderer, GlyphA);

    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);
    TRGlyphImageRef scaled = TRRendererGetGlyphImage(f.renderer, GlyphA);
    assert(scaled == base);

    TRRendererSetRenderScale(f.renderer, 1.0f);
    TRGlyphImageRef smaller = TRRendererGetGlyphImage(f.renderer, GlyphA);
    assert(smaller != base);
    assert(TRGlyphImageGetWidth(smaller) < TRGlyphImageGetWidth(base));

    /* Strokes are as wide as asked in pixels, whatever the render scale. */
    TRRendererSetTypeSize(f.renderer, 32.0f);
    TRGlyphImageRef stroke = TRRendererGetStrokeImage(f.renderer, GlyphA);
    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);
    assert(TRRendererGetStrokeImage(f.renderer, GlyphA) == stroke);

    for (TRGlyphImageRef image : { base, scaled, smaller, stroke, stroke }) {
        TRGlyphImageRelease(image);
    }
}

void RendererTests::testScalesAndSkew() {
    Fixture f;

    TRGlyphImageRef base = TRRendererGetGlyphImage(f.renderer, GlyphA);

    TRRendererSetScaleX(f.renderer, 2.0f);
    TRGlyphImageRef wide = TRRendererGetGlyphImage(f.renderer, GlyphA);
    assert(TRGlyphImageGetWidth(wide) > TRGlyphImageGetWidth(base) * 3 / 2);
    assert(TRGlyphImageGetHeight(wide) == TRGlyphImageGetHeight(base));
    TRRendererSetScaleX(f.renderer, 1.0f);

    TRRendererSetScaleY(f.renderer, 2.0f);
    TRGlyphImageRef tall = TRRendererGetGlyphImage(f.renderer, GlyphA);
    assert(TRGlyphImageGetHeight(tall) > TRGlyphImageGetHeight(base) * 3 / 2);
    assert(TRGlyphImageGetWidth(tall) == TRGlyphImageGetWidth(base));
    TRRendererSetScaleY(f.renderer, 1.0f);

    /* A skew moves the top of a glyph to the right, which makes its image wider. */
    TRRendererSetSkewX(f.renderer, 0.25f);
    TRGlyphImageRef skewed = TRRendererGetGlyphImage(f.renderer, GlyphA);
    assertSize(skewed, -2, 17, 18, 17);

    for (TRGlyphImageRef image : { base, wide, tall, skewed }) {
        TRGlyphImageRelease(image);
    }
}

void RendererTests::testIsRenderable() {
    Fixture f;

    assert(TRRendererIsRenderable(f.renderer));

    /* The least size is one pixel in each direction. */
    TRRendererSetTypeSize(f.renderer, 1.0f);
    assert(TRRendererIsRenderable(f.renderer));
    TRRendererSetTypeSize(f.renderer, 0.9f);
    assert(!TRRendererIsRenderable(f.renderer));
    assert(TRRendererGetGlyphImage(f.renderer, GlyphA) == nullptr);
    assert(TRRendererGetGlyphPath(f.renderer, GlyphA) == nullptr);

    /* The render scale makes up for a small size, and a scale of zero removes it. */
    TRRendererSetRenderScale(f.renderer, 2.0f);
    assert(TRRendererIsRenderable(f.renderer));
    TRRendererSetTypeSize(f.renderer, 32.0f);
    TRRendererSetScaleX(f.renderer, 0.0f);
    assert(!TRRendererIsRenderable(f.renderer));
    TRRendererSetScaleX(f.renderer, 1.0f);
    TRRendererSetScaleY(f.renderer, -1.0f);
    assert(!TRRendererIsRenderable(f.renderer));
    assert(TRRendererGetGlyphImage(f.renderer, GlyphA) == nullptr);
}

void RendererTests::testGlyphBoundingBox() {
    Fixture f;

    /* The top of the box is above the baseline, so it is negative with the y axis down. */
    TRRect box = TRRendererGetGlyphBoundingBox(f.renderer, GlyphA);
    assert(near(box.origin.x, 1.0f) && near(box.origin.y, -17.0f));
    assert(near(box.size.width, 15.0f) && near(box.size.height, 17.0f));

    box = TRRendererGetGlyphBoundingBox(f.renderer, GlyphB);
    assert(near(box.origin.x, 2.0f) && near(box.origin.y, -24.0f));
    assert(near(box.size.width, 15.0f) && near(box.size.height, 24.0f));

    /* The box is in the user space, so a render scale divides the pixels. */
    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);
    box = TRRendererGetGlyphBoundingBox(f.renderer, GlyphA);
    assert(near(box.origin.x, 0.5f) && near(box.origin.y, -8.5f));
    assert(near(box.size.width, 7.5f) && near(box.size.height, 8.5f));

    /* A glyph without an image has an empty box. */
    box = TRRendererGetGlyphBoundingBox(f.renderer, 0);
    assert(box.origin.x == 0.0f && box.origin.y == 0.0f);
    assert(box.size.width == 0.0f && box.size.height == 0.0f);
}

static const TRGlyphID Run[] = { GlyphA, GlyphB, GlyphC };
static const TRFloat RunAdvances[] = { 20.0f, 22.0f, 18.0f };
static const TRPoint NoOffsets[] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };

void RendererTests::testPlacementsLeftToRight() {
    Fixture f;
    TRGlyphPlacement placements[3];

    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 3, placements);

    /* The pen moves after each glyph, and the image starts at its left and top edges. */
    assertSize(placements[0].image, 1, 17, 15, 17);
    assert(placements[0].origin.x == 1.0f && placements[0].origin.y == -17.0f);
    assert(placements[1].origin.x == 22.0f && placements[1].origin.y == -24.0f);
    assert(placements[2].origin.x == 43.0f && placements[2].origin.y == -17.0f);

    TRRendererReleaseGlyphPlacements(placements, 3);
}

void RendererTests::testPlacementsRightToLeft() {
    Fixture f;
    TRRendererSetWritingDirection(f.renderer, TRWritingDirectionRightToLeft);
    TRGlyphPlacement placements[3];

    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 3, placements);

    /* The pen moves before each glyph, and goes to the left. */
    assert(placements[0].origin.x == -19.0f && placements[0].origin.y == -17.0f);
    assert(placements[1].origin.x == -40.0f && placements[1].origin.y == -24.0f);
    assert(placements[2].origin.x == -59.0f && placements[2].origin.y == -17.0f);

    TRRendererReleaseGlyphPlacements(placements, 3);
}

void RendererTests::testPlacementOffsets() {
    Fixture f;
    const TRPoint offsets[] = { { 3.0f, 4.0f }, { 0.0f, -2.0f }, { -5.0f, 0.0f } };
    TRGlyphPlacement placements[3];

    /* An offset of the y axis that points up moves the glyph up, which lowers its top. */
    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, offsets, RunAdvances, 3, placements);
    assert(placements[0].origin.x == 4.0f && placements[0].origin.y == -21.0f);
    assert(placements[1].origin.x == 22.0f && placements[1].origin.y == -22.0f);
    assert(placements[2].origin.x == 38.0f && placements[2].origin.y == -17.0f);
    TRRendererReleaseGlyphPlacements(placements, 3);

    /* The values of the user space are scaled to pixels, and then rounded half up: -17.5 gives -17. */
    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);
    const TRPoint half[] = { { 0.25f, 0.25f }, { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    const TRFloat advances[] = { 10.0f, 11.0f, 9.0f };
    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, half, advances, 3, placements);
    assert(placements[0].origin.x == 2.0f && placements[0].origin.y == -17.0f);
    assert(placements[1].origin.x == 22.0f && placements[1].origin.y == -24.0f);
    TRRendererReleaseGlyphPlacements(placements, 3);
}

void RendererTests::testStrokePlacements() {
    Fixture f;
    TRGlyphPlacement fill[1];
    TRGlyphPlacement stroke[1];

    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 1, fill);
    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindStroke, Run, NoOffsets, RunAdvances, 1, stroke);

    assert(fill[0].image != stroke[0].image);
    assertSize(stroke[0].image, 1, 18, 16, 19);
    assert(stroke[0].origin.x == 1.0f && stroke[0].origin.y == -18.0f);

    TRRendererReleaseGlyphPlacements(fill, 1);
    TRRendererReleaseGlyphPlacements(stroke, 1);
}

void RendererTests::testReleasePlacements() {
    Fixture f;
    TRGlyphPlacement placements[3];
    TRGlyphImageRef image = TRRendererGetGlyphImage(f.renderer, GlyphA);

    /* The reference of the placement is the cache's, the caller's, and the one it holds. */
    size_t before = AtomicUIntLoad(&image->_base.retainCount);
    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 3, placements);
    assert(placements[0].image == image);
    assert(AtomicUIntLoad(&image->_base.retainCount) == before + 1);

    TRRendererReleaseGlyphPlacements(placements, 3);
    assert(AtomicUIntLoad(&image->_base.retainCount) == before);
    assert(placements[0].image == nullptr && placements[1].image == nullptr);

    /* Releasing again is harmless. */
    TRRendererReleaseGlyphPlacements(placements, 3);
    assert(AtomicUIntLoad(&image->_base.retainCount) == before);

    TRGlyphImageRelease(image);
}

void RendererTests::testRunBoundingBox() {
    Fixture f;

    /* The rectangles are those of the placements: from 1 to 58 across, and from -24 to 0 down. */
    TRRect box = TRRendererGetRunBoundingBox(f.renderer, Run, NoOffsets, RunAdvances, 3);
    assert(near(box.origin.x, 1.0f) && near(box.origin.y, -24.0f));
    assert(near(box.size.width, 57.0f) && near(box.size.height, 24.0f));

    /* A single glyph gives the box of its own. */
    box = TRRendererGetRunBoundingBox(f.renderer, Run, NoOffsets, RunAdvances, 1);
    assert(near(box.origin.x, 1.0f) && near(box.origin.y, -17.0f));
    assert(near(box.size.width, 15.0f) && near(box.size.height, 17.0f));

    /* An offset moves the glyph and so its box. */
    const TRPoint offsets[] = { { 0, 10.0f }, { 0, 0 }, { 0, 0 } };
    box = TRRendererGetRunBoundingBox(f.renderer, Run, offsets, RunAdvances, 3);
    assert(near(box.origin.y, -27.0f) && near(box.size.height, 27.0f));

    /* The user space divides the pixels by the render scale. */
    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);
    const TRFloat doubled[] = { 10.0f, 11.0f, 9.0f };
    box = TRRendererGetRunBoundingBox(f.renderer, Run, NoOffsets, doubled, 3);
    assert(near(box.origin.x, 0.5f) && near(box.origin.y, -12.0f));
    assert(near(box.size.width, 28.5f) && near(box.size.height, 12.0f));
}

void RendererTests::testRunBoundingBoxRightToLeft() {
    Fixture f;
    TRRendererSetWritingDirection(f.renderer, TRWritingDirectionRightToLeft);

    /*
     * The placements go from -59 to -4, and the box is shifted by the advance of the run, which is
     * 60, so that it is in the coordinates that the run has from its start.
     */
    TRRect box = TRRendererGetRunBoundingBox(f.renderer, Run, NoOffsets, RunAdvances, 3);
    assert(near(box.origin.x, 1.0f) && near(box.origin.y, -24.0f));
    assert(near(box.size.width, 55.0f) && near(box.size.height, 24.0f));
}

void RendererTests::testEmptyRuns() {
    Fixture f;
    TRGlyphPlacement placement;

    TRRect box = TRRendererGetRunBoundingBox(f.renderer, Run, NoOffsets, RunAdvances, 0);
    assert(box.origin.x == 0.0f && box.origin.y == 0.0f);
    assert(box.size.width == 0.0f && box.size.height == 0.0f);

    TRRendererGetGlyphPlacements(f.renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 0, &placement);
    TRRendererReleaseGlyphPlacements(&placement, 0);

    size_t calls = 0;
    TRPathCallbacks callbacks = {};
    callbacks.moveTo = [](void *data, TRFloat, TRFloat) { (*static_cast<size_t *>(data))++; };
    TRRendererEnumerateGlyphPaths(f.renderer, Run, NoOffsets, RunAdvances, 0, &callbacks, &calls);
    assert(calls == 0);

    /* A run of glyphs without any image has an empty box, while the pen still moves. */
    const TRGlyphID spaces[] = { 0, 0 };
    box = TRRendererGetRunBoundingBox(f.renderer, spaces, NoOffsets, RunAdvances, 2);
    assert(box.size.width == 0.0f && box.size.height == 0.0f);
}

static vector<PathEvent> glyphEvents(TRRendererRef renderer, TRGlyphID glyph, const TRAffineTransform &t) {
    TRPathRef path = TRRendererGetGlyphPath(renderer, glyph);
    assert(path != nullptr);

    vector<PathEvent> events = enumeratePath(path, &t);
    TRPathRelease(path);

    return events;
}

static vector<PathEvent> runEvents(TRRendererRef renderer, const TRGlyphID *glyphs,
    const TRPoint *offsets, const TRFloat *advances, size_t count) {
    using Events = vector<PathEvent>;

    TRPathCallbacks callbacks = {};
    callbacks.moveTo = [](void *data, TRFloat x, TRFloat y) {
        static_cast<Events *>(data)->push_back({ PathEvent::Move, { { x, y } } });
    };
    callbacks.lineTo = [](void *data, TRFloat x, TRFloat y) {
        static_cast<Events *>(data)->push_back({ PathEvent::Line, { { x, y } } });
    };
    callbacks.quadTo = [](void *data, TRFloat cx, TRFloat cy, TRFloat x, TRFloat y) {
        static_cast<Events *>(data)->push_back({ PathEvent::Quad, { { cx, cy }, { x, y } } });
    };
    callbacks.cubicTo = [](void *data, TRFloat c1x, TRFloat c1y, TRFloat c2x, TRFloat c2y, TRFloat x, TRFloat y) {
        static_cast<Events *>(data)->push_back({ PathEvent::Cubic, { { c1x, c1y }, { c2x, c2y }, { x, y } } });
    };
    callbacks.close = [](void *data) {
        static_cast<Events *>(data)->push_back({ PathEvent::Close, {} });
    };

    Events events;
    TRRendererEnumerateGlyphPaths(renderer, glyphs, offsets, advances, count, &callbacks, &events);

    return events;
}

static void assertSameEvents(const vector<PathEvent> &actual, const vector<PathEvent> &expected) {
    assert(actual.size() == expected.size());

    for (size_t i = 0; i < actual.size(); i++) {
        assert(actual[i].kind == expected[i].kind);
        assert(actual[i].points.size() == expected[i].points.size());

        for (size_t j = 0; j < actual[i].points.size(); j++) {
            assert(near(actual[i].points[j].x, expected[i].points[j].x, 1e-3f));
            assert(near(actual[i].points[j].y, expected[i].points[j].y, 1e-3f));
        }
    }
}

void RendererTests::testEnumerateGlyphPaths() {
    Fixture f;
    const TRPoint offsets[] = { { 0, 0 }, { 2.0f, 5.0f }, { 0, 0 } };

    /* Each glyph goes where its pen position and offset say, with the y axis pointing down. */
    vector<PathEvent> expected;
    const TRAffineTransform first = { 1, 0, 0, 1, 0, 0 };
    const TRAffineTransform second = { 1, 0, 0, 1, 20.0f + 2.0f, -5.0f };
    const TRAffineTransform third = { 1, 0, 0, 1, 42.0f, 0 };

    for (auto &events : { glyphEvents(f.renderer, GlyphA, first), glyphEvents(f.renderer, GlyphB, second),
                          glyphEvents(f.renderer, GlyphC, third) }) {
        expected.insert(expected.end(), events.begin(), events.end());
    }

    vector<PathEvent> actual = runEvents(f.renderer, Run, offsets, RunAdvances, 3);
    assert(!actual.empty());
    assertSameEvents(actual, expected);

    /* The glyph without an outline does not break the run. */
    const TRGlyphID withMissing[] = { GlyphA, 500, GlyphC };
    actual = runEvents(f.renderer, withMissing, NoOffsets, RunAdvances, 3);
    vector<PathEvent> expectedMissing = glyphEvents(f.renderer, GlyphA, first);
    vector<PathEvent> others = glyphEvents(f.renderer, GlyphC, { 1, 0, 0, 1, 42.0f, 0 });
    expectedMissing.insert(expectedMissing.end(), others.begin(), others.end());
    assertSameEvents(actual, expectedMissing);
}

void RendererTests::testEnumerateGlyphPathsRightToLeft() {
    Fixture f;
    TRRendererSetWritingDirection(f.renderer, TRWritingDirectionRightToLeft);

    /* The pen moves to the left before each glyph, as it does for the images. */
    vector<PathEvent> expected;
    const TRAffineTransform first = { 1, 0, 0, 1, -20.0f, 0 };
    const TRAffineTransform second = { 1, 0, 0, 1, -42.0f, 0 };
    const TRAffineTransform third = { 1, 0, 0, 1, -60.0f, 0 };

    for (auto &events : { glyphEvents(f.renderer, GlyphA, first), glyphEvents(f.renderer, GlyphB, second),
                          glyphEvents(f.renderer, GlyphC, third) }) {
        expected.insert(expected.end(), events.begin(), events.end());
    }

    assertSameEvents(runEvents(f.renderer, Run, NoOffsets, RunAdvances, 3), expected);
}

void RendererTests::testEnumerateWithRenderScale() {
    Fixture f;

    /* The pixel size is 32 either way, and the user space is twice as small as the pixels. */
    TRRendererSetTypeSize(f.renderer, 16.0f);
    TRRendererSetRenderScale(f.renderer, 2.0f);

    vector<PathEvent> expected = glyphEvents(f.renderer, GlyphA, { 0.5f, 0, 0, 0.5f, 0, 0 });
    vector<PathEvent> second = glyphEvents(f.renderer, GlyphB, { 0.5f, 0, 0, 0.5f, 10.0f, 0 });
    expected.insert(expected.end(), second.begin(), second.end());

    const TRFloat advances[] = { 10.0f, 11.0f, 9.0f };
    assertSameEvents(runEvents(f.renderer, Run, NoOffsets, advances, 2), expected);

    /* In the user space, the glyph is as big as the type size says. */
    vector<PathEvent> events = runEvents(f.renderer, Run, NoOffsets, advances, 1);
    TRFloat minY = 0.0f;
    for (const PathEvent &event : events) {
        for (const TRPoint &point : event.points) {
            minY = min(minY, point.y);
        }
    }
    assert(minY < -7.0f && minY > -9.5f);
}

void RendererTests::testColorGlyphs() {
    Fixture f("COLRv0.extents.ttf");

    /* A glyph with color layers comes as a color image, which has four bytes for each pixel. */
    TRGlyphImageRef color = TRRendererGetGlyphImage(f.renderer, 13);
    assert(color != nullptr);
    assert(TRGlyphImageGetFormat(color) == TRGlyphImageFormatARGB);
    assert(TRGlyphImageGetByteCount(color) == TRGlyphImageGetWidth(color) * TRGlyphImageGetHeight(color) * 4);

    /* The foreground color does not decide its image, so that it is shared. */
    TRRendererSetForegroundColor(f.renderer, TRColorMake(0xFF, 0xFF, 0x00, 0x00));
    assert(TRRendererGetGlyphImage(f.renderer, 13) == color);

    /* A mask glyph of the same font is an alpha image. */
    TRGlyphImageRef mask = TRRendererGetGlyphImage(f.renderer, 1);
    assert(mask != nullptr && TRGlyphImageGetFormat(mask) == TRGlyphImageFormatAlpha);

    TRGlyphImageRelease(color);
    TRGlyphImageRelease(color);
    TRGlyphImageRelease(mask);
}

void RendererTests::testConcurrentRenderers() {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf");
    TRGlyphCacheRef cache = TRGlyphCacheCreate(16 * 1024);
    atomic<int> failures{0};
    vector<thread> threads;

    /* Each thread has a renderer of its own, while the typeface and the cache are shared. */
    for (size_t t = 0; t < 8; t++) {
        threads.emplace_back([&, t]() {
            TRRendererRef renderer = TRRendererCreate();
            TRRendererSetTypeface(renderer, typeface);
            TRRendererSetGlyphCache(renderer, cache);
            TRRendererSetTypeSize(renderer, 28.0f + static_cast<TRFloat>(t % 3));
            TRRendererSetWritingDirection(renderer, t % 2 ? TRWritingDirectionRightToLeft : TRWritingDirectionLeftToRight);

            for (size_t i = 0; i < 100; i++) {
                TRGlyphPlacement placements[3];
                TRRendererGetGlyphPlacements(renderer, TRGlyphImageKindFill, Run, NoOffsets, RunAdvances, 3, placements);

                for (const TRGlyphPlacement &placement : placements) {
                    if (!placement.image || TRGlyphImageGetWidth(placement.image) == 0) {
                        failures++;
                    }
                }

                TRRendererReleaseGlyphPlacements(placements, 3);

                TRRect box = TRRendererGetRunBoundingBox(renderer, Run, NoOffsets, RunAdvances, 3);
                if (box.size.width <= 0.0f) {
                    failures++;
                }
            }

            TRRendererRelease(renderer);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(failures == 0);

    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    RendererTests tests;
    tests.run();

    return 0;
}

#endif
