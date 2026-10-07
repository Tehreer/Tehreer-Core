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
#include <thread>
#include <vector>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGlyphCache.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRGlyphCache.h>
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
}

#include "TestTypeface.h"

#include "GlyphCacheTests.h"

using namespace std;
using namespace Tehreer;

void GlyphCacheTests::run() {
    testCreate();
    testDefaultCache();
    testImageIsCached();
    testKeysDistinguishSettings();
    testForegroundColorDoesNotSplitMaskGlyphs();
    testPathIsCached();
    testStrokeImages();
    testMissingGlyphs();
    testEvictionByCapacity();
    testLeastRecentlyUsedGoesFirst();
    testEvictedImagesStayValid();
    testTypefaceRetention();
    testClearAndCapacity();
    testTableGrowth();
    testNativeDataFollowsImage();
    testConcurrentLookups();
    testSeparateCaches();
}

constexpr TRUInteger Megabyte = 1024 * 1024;

/* The glyphs of the test font, which are 'a', 'b' and 'c'. */
constexpr TRGlyphID GlyphA = 1;
constexpr TRGlyphID GlyphB = 2;
constexpr TRGlyphID GlyphC = 3;

static TRRendererRef createRenderer(TRTypefaceRef typeface, TRGlyphCacheRef cache, TRFloat typeSize = 32.0f) {
    TRRendererRef renderer = TRRendererCreate();
    assert(renderer != nullptr);

    TRRendererSetTypeface(renderer, typeface);
    TRRendererSetGlyphCache(renderer, cache);
    TRRendererSetTypeSize(renderer, typeSize);

    return renderer;
}

/* Looks an image up and lets go of it again, which tells whether it comes from the cache. */
static TRGlyphImageRef peek(TRRendererRef renderer, TRGlyphID glyphID) {
    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, glyphID);
    if (image) {
        TRGlyphImageRelease(image);
    }

    return image;
}

static size_t retainCount(TRTypefaceRef typeface) {
    return AtomicUIntLoad(&typeface->_base.retainCount);
}

void GlyphCacheTests::testCreate() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(3 * Megabyte);

    assert(cache != nullptr);
    assert(TRGlyphCacheGetCapacity(cache) == 3 * Megabyte);
    assert(TRGlyphCacheGetSize(cache) == 0);

    TRGlyphCacheRelease(cache);

    TRGlyphCacheRef empty = TRGlyphCacheCreate(0);
    assert(empty != nullptr);
    assert(TRGlyphCacheGetCapacity(empty) == 0);
    TRGlyphCacheRelease(empty);
}

void GlyphCacheTests::testDefaultCache() {
    TRGlyphCacheRef cache = TRGlyphCacheGetDefault();

    assert(cache != nullptr);
    assert(TRGlyphCacheGetDefault() == cache);
    assert(TRGlyphCacheGetCapacity(cache) == 8 * Megabyte);

    /* A renderer without a cache of its own uses it. */
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = TRRendererCreate();
    TRRendererSetTypeface(renderer, typeface);
    TRRendererSetTypeSize(renderer, 41.0f);

    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(image != nullptr);
    assert(TRRendererGetGlyphImage(renderer, GlyphA) == image);
    assert(TRGlyphCacheGetSize(cache) > 0);

    TRGlyphImageRelease(image);
    TRGlyphImageRelease(image);
    TRRendererRelease(renderer);
    TRGlyphCacheClear(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testImageIsCached() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef first = TRRendererGetGlyphImage(renderer, GlyphA);
    size_t sizeAfterFirst = TRGlyphCacheGetSize(cache);
    TRGlyphImageRef second = TRRendererGetGlyphImage(renderer, GlyphA);

    /* The same image comes back, and the cache does not grow. */
    assert(first != nullptr);
    assert(second == first);
    assert(TRGlyphCacheGetSize(cache) == sizeAfterFirst);
    assert(sizeAfterFirst >= TRGlyphImageGetByteCount(first));

    TRGlyphImageRef other = TRRendererGetGlyphImage(renderer, GlyphB);
    assert(other != nullptr && other != first);
    assert(TRGlyphCacheGetSize(cache) > sizeAfterFirst);

    TRGlyphImageRelease(other);
    TRGlyphImageRelease(second);
    TRGlyphImageRelease(first);
    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testKeysDistinguishSettings() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf");
    const TRFloat black[] = { 900.0f, 100.0f };
    TRTypefaceRef blackFace = TRTypefaceCreateWithVariation(typeface, black, 2);
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef base = peek(renderer, GlyphA);
    assert(base != nullptr);
    assert(peek(renderer, GlyphA) == base);

    /* Every property that changes how a glyph looks gives an entry of its own. */
    TRRendererSetTypeSize(renderer, 33.0f);
    TRGlyphImageRef bigger = peek(renderer, GlyphA);
    assert(bigger != nullptr && bigger != base);

    TRRendererSetTypeSize(renderer, 32.0f);
    assert(peek(renderer, GlyphA) == base);

    TRRendererSetScaleX(renderer, 1.5f);
    TRGlyphImageRef wider = peek(renderer, GlyphA);
    assert(wider != nullptr && wider != base && wider != bigger);
    TRRendererSetScaleX(renderer, 1.0f);

    TRRendererSetScaleY(renderer, 1.5f);
    TRGlyphImageRef taller = peek(renderer, GlyphA);
    assert(taller != nullptr && taller != base && taller != wider);
    TRRendererSetScaleY(renderer, 1.0f);

    TRRendererSetSkewX(renderer, 0.25f);
    TRGlyphImageRef skewed = peek(renderer, GlyphA);
    assert(skewed != nullptr && skewed != base && skewed != taller);
    TRRendererSetSkewX(renderer, 0.0f);

    TRRendererSetTypeface(renderer, blackFace);
    TRGlyphImageRef heavy = peek(renderer, GlyphA);
    assert(heavy != nullptr && heavy != base);

    TRRendererSetTypeface(renderer, typeface);
    assert(peek(renderer, GlyphA) == base);

    /* The render scale only matters through the pixel size that it gives. */
    TRRendererSetTypeSize(renderer, 16.0f);
    TRRendererSetRenderScale(renderer, 2.0f);
    assert(peek(renderer, GlyphA) == base);

    TRRendererRelease(renderer);
    TRTypefaceRelease(blackFace);
    TRTypefaceRelease(typeface);
    TRGlyphCacheRelease(cache);
}

void GlyphCacheTests::testForegroundColorDoesNotSplitMaskGlyphs() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    /* The image of a mask glyph is a coverage, so every color can share it. */
    TRRendererSetForegroundColor(renderer, TRColorMake(0xFF, 0xFF, 0x00, 0x00));
    TRGlyphImageRef red = peek(renderer, GlyphA);

    TRRendererSetForegroundColor(renderer, TRColorMake(0x80, 0x00, 0x00, 0xFF));
    TRGlyphImageRef blue = peek(renderer, GlyphA);

    assert(red != nullptr);
    assert(blue == red);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testPathIsCached() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRPathRef first = TRRendererGetGlyphPath(renderer, GlyphA);
    TRPathRef second = TRRendererGetGlyphPath(renderer, GlyphA);
    TRPathRef other = TRRendererGetGlyphPath(renderer, GlyphB);
    assert(first != nullptr);
    assert(second == first);
    assert(other != nullptr && other != first);

    /* The path and the image of a glyph share an entry, but are independent. */
    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphA);
    TRPathRef third = TRRendererGetGlyphPath(renderer, GlyphA);
    TRGlyphImageRef sameImage = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(third == first);
    assert(sameImage == image);

    TRGlyphImageRelease(sameImage);
    TRGlyphImageRelease(image);
    TRPathRelease(third);
    TRPathRelease(other);
    TRPathRelease(second);
    TRPathRelease(first);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

static TRGlyphImageRef peekStroke(TRRendererRef renderer, TRGlyphID glyphID) {
    TRGlyphImageRef image = TRRendererGetStrokeImage(renderer, glyphID);
    if (image) {
        TRGlyphImageRelease(image);
    }

    return image;
}

void GlyphCacheTests::testStrokeImages() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef base = peekStroke(renderer, GlyphA);
    assert(base != nullptr);
    assert(peekStroke(renderer, GlyphA) == base);

    /* The stroke is not the fill. */
    assert(peek(renderer, GlyphA) != base);

    /* Each stroke setting has an entry of its own. */
    TRRendererSetStrokeWidth(renderer, 3.0f);
    TRGlyphImageRef wide = peekStroke(renderer, GlyphA);
    assert(wide != nullptr && wide != base);
    assert(TRGlyphImageGetWidth(wide) > TRGlyphImageGetWidth(base));
    TRRendererSetStrokeWidth(renderer, 1.0f);
    assert(peekStroke(renderer, GlyphA) == base);

    TRRendererSetStrokeCap(renderer, TRStrokeCapSquare);
    TRGlyphImageRef capped = peekStroke(renderer, GlyphA);
    assert(capped != nullptr && capped != base);
    TRRendererSetStrokeCap(renderer, TRStrokeCapButt);

    TRRendererSetStrokeJoin(renderer, TRStrokeJoinMiterFixed);
    TRGlyphImageRef joined = peekStroke(renderer, GlyphA);
    assert(joined != nullptr && joined != base && joined != capped);
    TRRendererSetStrokeJoin(renderer, TRStrokeJoinRound);

    TRRendererSetStrokeMiter(renderer, 5.0f);
    TRGlyphImageRef mitered = peekStroke(renderer, GlyphA);
    assert(mitered != nullptr && mitered != base && mitered != joined);
    TRRendererSetStrokeMiter(renderer, 1.0f);

    assert(peekStroke(renderer, GlyphA) == base);

    /* A stroke of the glyph at another size is another image. */
    TRRendererSetTypeSize(renderer, 40.0f);
    TRGlyphImageRef bigger = peekStroke(renderer, GlyphA);
    assert(bigger != nullptr && bigger != base);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testMissingGlyphs() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    /* The notdef glyph of the font is empty, and a glyph out of range has nothing at all. */
    assert(TRRendererGetGlyphImage(renderer, 0) == nullptr);
    assert(TRRendererGetStrokeImage(renderer, 0) == nullptr);
    assert(TRRendererGetGlyphImage(renderer, 500) == nullptr);
    assert(TRRendererGetGlyphPath(renderer, 500) == nullptr);

    /* A size that is too small to render gives nothing either. */
    TRRendererSetTypeSize(renderer, 0.0f);
    assert(TRRendererGetGlyphImage(renderer, GlyphA) == nullptr);

    /* The misses are not cached as images, so the glyphs still work afterwards. */
    TRRendererSetTypeSize(renderer, 32.0f);
    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(image != nullptr);
    TRGlyphImageRelease(image);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testEvictionByCapacity() {
    const TRUInteger capacity = 20 * 1024;
    TRGlyphCacheRef cache = TRGlyphCacheCreate(capacity);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef first = peek(renderer, GlyphA);

    /* Many sizes of the glyphs do not fit, so the cache stays within its capacity. */
    for (TRFloat size = 20.0f; size < 120.0f; size += 1.0f) {
        TRRendererSetTypeSize(renderer, size);

        for (TRGlyphID glyph : { GlyphA, GlyphB, GlyphC }) {
            peek(renderer, glyph);
            assert(TRGlyphCacheGetSize(cache) <= capacity);
        }
    }

    /* The oldest entry was dropped, so it has to be rendered again. */
    TRRendererSetTypeSize(renderer, 32.0f);
    TRGlyphImageRef again = peek(renderer, GlyphA);
    assert(again != nullptr);
    assert(again != first || TRGlyphCacheGetSize(cache) <= capacity);
    assert(TRGlyphCacheGetSize(cache) > 0);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testLeastRecentlyUsedGoesFirst() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef a = TRRendererGetGlyphImage(renderer, GlyphA);
    TRGlyphImageRef b = TRRendererGetGlyphImage(renderer, GlyphB);
    TRGlyphImageRef c = TRRendererGetGlyphImage(renderer, GlyphC);

    /* The capacity is just what the three of them take, and a is used again, so b is the oldest. */
    TRGlyphCacheSetCapacity(cache, TRGlyphCacheGetSize(cache));
    TRGlyphImageRef usedAgain = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(usedAgain == a);

    TRRendererSetTypeSize(renderer, 31.0f);
    TRGlyphImageRef d = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(d != nullptr && d != a);
    assert(TRGlyphCacheGetSize(cache) <= TRGlyphCacheGetCapacity(cache));

    TRRendererSetTypeSize(renderer, 32.0f);
    TRGlyphImageRef afterA = TRRendererGetGlyphImage(renderer, GlyphA);
    TRGlyphImageRef afterB = TRRendererGetGlyphImage(renderer, GlyphB);

    /* The one that was used last survived, and the one that was not used was dropped. */
    assert(afterA == a);
    assert(afterB != b);
    assert(TRGlyphImageGetWidth(afterB) == TRGlyphImageGetWidth(b));

    for (TRGlyphImageRef image : { a, a, b, c, d, afterA, afterB }) {
        TRGlyphImageRelease(image);
    }

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testEvictedImagesStayValid() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphA);
    TRPathRef path = TRRendererGetGlyphPath(renderer, GlyphA);
    TRGlyphImageRef stroke = TRRendererGetStrokeImage(renderer, GlyphA);
    uint32_t width = TRGlyphImageGetWidth(image);
    uint32_t height = TRGlyphImageGetHeight(image);

    /* Everything is dropped while the caller still holds the glyph. */
    TRGlyphCacheSetCapacity(cache, 0);
    assert(TRGlyphCacheGetSize(cache) == 0);

    assert(TRGlyphImageGetWidth(image) == width);
    assert(TRGlyphImageGetHeight(image) == height);
    assert(TRGlyphImageGetPixelsPtr(image)[0] <= 255);
    assert(TRGlyphImageGetWidth(stroke) > 0);

    size_t points = 0;
    TRPathCallbacks callbacks = {};
    callbacks.moveTo = [](void *data, TRFloat, TRFloat) { (*static_cast<size_t *>(data))++; };
    TRPathEnumerate(path, nullptr, &callbacks, &points);
    assert(points > 0);

    TRGlyphImageRelease(stroke);
    TRPathRelease(path);
    TRGlyphImageRelease(image);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testTypefaceRetention() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);
    size_t base = retainCount(typeface);

    assert(base == 2);  /* The test and the renderer. */

    /* An entry keeps its typeface alive, so that its address cannot be taken by another one. */
    peek(renderer, GlyphA);
    assert(retainCount(typeface) > base);

    TRRendererRelease(renderer);
    TRTypefaceRelease(typeface);
    assert(TRGlyphCacheGetSize(cache) > 0);

    /* Clearing the cache lets go of it, which destroys the typeface. */
    typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    renderer = createRenderer(typeface, cache);
    size_t before = retainCount(typeface);

    peek(renderer, GlyphA);
    peek(renderer, GlyphB);
    assert(retainCount(typeface) > before);

    TRGlyphCacheClear(cache);
    assert(retainCount(typeface) == before);
    assert(TRGlyphCacheGetSize(cache) == 0);

    /* The same is true for the entries that are dropped because of the capacity. */
    peek(renderer, GlyphA);
    assert(retainCount(typeface) > before);
    TRGlyphCacheSetCapacity(cache, 0);
    assert(retainCount(typeface) == before);

    /* And when the cache itself goes away. */
    TRGlyphCacheSetCapacity(cache, Megabyte);
    peek(renderer, GlyphA);
    assert(retainCount(typeface) > before);
    TRRendererSetGlyphCache(renderer, nullptr);
    TRGlyphCacheRelease(cache);
    assert(retainCount(typeface) == before);

    TRRendererRelease(renderer);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testClearAndCapacity() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);

    peek(renderer, GlyphA);
    peek(renderer, GlyphB);
    assert(TRGlyphCacheGetSize(cache) > 0);

    TRGlyphCacheClear(cache);
    assert(TRGlyphCacheGetSize(cache) == 0);
    assert(TRGlyphCacheGetCapacity(cache) == Megabyte);

    /* The cache works as before, with new images. */
    assert(peek(renderer, GlyphA) != nullptr);
    assert(TRGlyphCacheGetSize(cache) > 0);

    /* Clearing an empty cache is fine, and so is a capacity that cannot hold a glyph. */
    TRGlyphCacheClear(cache);
    TRGlyphCacheClear(cache);
    TRGlyphCacheSetCapacity(cache, 10);
    assert(TRGlyphCacheGetCapacity(cache) == 10);
    assert(TRRendererGetGlyphImage(renderer, GlyphA) != nullptr);
    assert(TRGlyphCacheGetSize(cache) <= 10);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testTableGrowth() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(64 * Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);
    vector<TRGlyphImageRef> images;

    /* The table starts with a few buckets, so this makes it grow many times. */
    for (int size = 10; size < 310; size++) {
        TRRendererSetTypeSize(renderer, static_cast<TRFloat>(size));
        images.push_back(peek(renderer, GlyphA));
    }

    /* All of the entries are still found afterwards, and the images are all different. */
    for (int size = 10; size < 310; size++) {
        TRRendererSetTypeSize(renderer, static_cast<TRFloat>(size));
        assert(peek(renderer, GlyphA) == images[size - 10]);
    }
    for (size_t i = 1; i < images.size(); i++) {
        assert(images[i] != images[i - 1]);
    }
    assert(cache->entryCount >= 300);
    assert(ArrayGetCount(&cache->buckets) >= 256);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

static atomic<int> nativeDestroyCount{0};

void GlyphCacheTests::testNativeDataFollowsImage() {
    TRGlyphCacheRef cache = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef renderer = createRenderer(typeface, cache);
    int marker = 1;

    nativeDestroyCount = 0;

    /* A wrapper keeps its object on the cached image, so that it is made only once. */
    TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(TRGlyphImageSetNativeData(image, &marker, [](void *) { nativeDestroyCount++; }));
    TRGlyphImageRelease(image);

    image = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(TRGlyphImageGetNativeData(image) == &marker);

    /* It goes away together with the image that holds it, which the caller still does here. */
    TRGlyphCacheClear(cache);
    assert(nativeDestroyCount == 0);
    TRGlyphImageRelease(image);
    assert(nativeDestroyCount == 1);

    /* The image that is rendered next has none. */
    image = TRRendererGetGlyphImage(renderer, GlyphA);
    assert(TRGlyphImageGetNativeData(image) == nullptr);
    TRGlyphImageRelease(image);

    TRRendererRelease(renderer);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testConcurrentLookups() {
    constexpr size_t NumThreads = 8;
    constexpr size_t Iterations = 300;

    /* The capacity is small, so threads keep evicting what the others use. */
    TRGlyphCacheRef cache = TRGlyphCacheCreate(12 * 1024);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Variable.abc.ttf");
    const TRFloat black[] = { 900.0f, 100.0f };
    TRTypefaceRef blackFace = TRTypefaceCreateWithVariation(typeface, black, 2);

    /* What each lookup has to give, found without any contention. */
    TRRendererRef reference = createRenderer(typeface, TRGlyphCacheCreate(Megabyte), 32.0f);
    TRGlyphImageRef expected = TRRendererGetGlyphImage(reference, GlyphB);
    uint32_t width = TRGlyphImageGetWidth(expected);
    uint32_t height = TRGlyphImageGetHeight(expected);

    atomic<int> failures{0};
    vector<thread> threads;

    for (size_t t = 0; t < NumThreads; t++) {
        threads.emplace_back([&, t]() {
            TRRendererRef renderer = createRenderer((t % 2 ? blackFace : typeface), cache);

            for (size_t i = 0; i < Iterations; i++) {
                TRRendererSetTypeSize(renderer, 32.0f + static_cast<TRFloat>(i % 5));

                TRGlyphImageRef image = TRRendererGetGlyphImage(renderer, GlyphB);
                TRGlyphImageRef stroke = TRRendererGetStrokeImage(renderer, GlyphB);
                TRPathRef path = TRRendererGetGlyphPath(renderer, GlyphB);

                if (!image || !stroke || !path || TRGlyphImageGetByteCount(image) == 0) {
                    failures++;
                }

                if (t % 2 == 0 && i % 5 == 0 && image
                        && (TRGlyphImageGetWidth(image) != width || TRGlyphImageGetHeight(image) != height)) {
                    failures++;
                }

                if (image) TRGlyphImageRelease(image);
                if (stroke) TRGlyphImageRelease(stroke);
                if (path) TRPathRelease(path);

                if (i % 100 == 99) {
                    TRGlyphCacheSetCapacity(cache, 12 * 1024 + (i % 3) * 1024);
                }
            }

            TRRendererRelease(renderer);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(failures == 0);
    assert(TRGlyphCacheGetSize(cache) <= TRGlyphCacheGetCapacity(cache));

    TRGlyphImageRelease(expected);
    TRRendererRelease(reference);
    TRGlyphCacheRelease(cache);
    TRTypefaceRelease(blackFace);
    TRTypefaceRelease(typeface);
}

void GlyphCacheTests::testSeparateCaches() {
    TRGlyphCacheRef one = TRGlyphCacheCreate(Megabyte);
    TRGlyphCacheRef two = TRGlyphCacheCreate(Megabyte);
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRRendererRef first = createRenderer(typeface, one);
    TRRendererRef second = createRenderer(typeface, two);

    /* The caches do not share anything, so the images are not shared either. */
    TRGlyphImageRef a = peek(first, GlyphA);
    TRGlyphImageRef b = peek(second, GlyphA);
    assert(a != nullptr && b != nullptr && a != b);
    assert(TRGlyphImageGetWidth(a) == TRGlyphImageGetWidth(b));

    TRGlyphCacheClear(one);
    assert(TRGlyphCacheGetSize(one) == 0);
    assert(TRGlyphCacheGetSize(two) > 0);
    assert(peek(second, GlyphA) == b);

    TRRendererRelease(second);
    TRRendererRelease(first);
    TRGlyphCacheRelease(two);
    TRGlyphCacheRelease(one);
    TRTypefaceRelease(typeface);
}

#ifdef STANDALONE_TESTING

int main() {
    GlyphCacheTests tests;
    tests.run();

    return 0;
}

#endif
