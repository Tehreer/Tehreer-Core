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

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGlyphImage.h>

extern "C" {
#include <API/TRGlyphImage.h>
#include <Core/AtomicUInt.h>
#include <Graphics/GlyphBitmap.h>
}

#include "GlyphImageTests.h"

using namespace std;
using namespace Tehreer;

void GlyphImageTests::run() {
    testAlphaImage();
    testColorImage();
    testNativeData();
    testNativeDataWithoutDestroy();
    testNativeDataRace();
    testRetainRelease();
}

static TRGlyphImageRef createAlphaImage() {
    unsigned char pixels[] = { 1, 2, 3, 4, 5, 6 };
    FT_Bitmap ftBitmap = {};
    ftBitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    ftBitmap.width = 3;
    ftBitmap.rows = 2;
    ftBitmap.pitch = 3;
    ftBitmap.buffer = pixels;

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromBitmap(&ftBitmap, -2, 9);
    assert(bitmap != nullptr);

    return TRGlyphImageCreate(bitmap);
}

void GlyphImageTests::testAlphaImage() {
    TRGlyphImageRef image = createAlphaImage();
    assert(image != nullptr);

    assert(TRGlyphImageGetFormat(image) == TRGlyphImageFormatAlpha);
    assert(TRGlyphImageGetLeft(image) == -2);
    assert(TRGlyphImageGetTop(image) == 9);
    assert(TRGlyphImageGetWidth(image) == 3);
    assert(TRGlyphImageGetHeight(image) == 2);
    assert(TRGlyphImageGetByteCount(image) == 6);

    const uint8_t expected[] = { 1, 2, 3, 4, 5, 6 };
    assert(memcmp(TRGlyphImageGetPixelsPtr(image), expected, sizeof(expected)) == 0);

    TRGlyphImageRelease(image);
}

void GlyphImageTests::testColorImage() {
    /* FreeType gives blue, green, red and alpha; the image has alpha, red, green and blue. */
    unsigned char pixels[] = {
        10, 20, 30, 40,     50, 60, 70, 80
    };
    FT_Bitmap ftBitmap = {};
    ftBitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    ftBitmap.width = 2;
    ftBitmap.rows = 1;
    ftBitmap.pitch = 8;
    ftBitmap.buffer = pixels;

    TRGlyphImageRef image = TRGlyphImageCreate(GlyphBitmapCreateFromBitmap(&ftBitmap, 1, 2));
    assert(image != nullptr);

    assert(TRGlyphImageGetFormat(image) == TRGlyphImageFormatARGB);
    assert(TRGlyphImageGetWidth(image) == 2);
    assert(TRGlyphImageGetHeight(image) == 1);
    assert(TRGlyphImageGetByteCount(image) == 8);

    const uint8_t expected[] = { 40, 30, 20, 10,    80, 70, 60, 50 };
    assert(memcmp(TRGlyphImageGetPixelsPtr(image), expected, sizeof(expected)) == 0);

    TRGlyphImageRelease(image);
}

namespace {

atomic<int> destroyCount{0};
atomic<void *> lastDestroyed{nullptr};

void destroyData(void *data) {
    destroyCount++;
    lastDestroyed = data;
}

}

void GlyphImageTests::testNativeData() {
    TRGlyphImageRef image = createAlphaImage();
    int first = 1;
    int second = 2;

    destroyCount = 0;
    lastDestroyed = nullptr;

    assert(TRGlyphImageGetNativeData(image) == nullptr);

    /* Nothing can be attached without data. */
    assert(!TRGlyphImageSetNativeData(image, nullptr, destroyData));

    assert(TRGlyphImageSetNativeData(image, &first, destroyData));
    assert(TRGlyphImageGetNativeData(image) == &first);

    /* The first data stays, and the caller keeps the ownership of the one that lost. */
    assert(!TRGlyphImageSetNativeData(image, &second, destroyData));
    assert(TRGlyphImageGetNativeData(image) == &first);
    assert(destroyCount == 0);

    TRGlyphImageRetain(image);
    TRGlyphImageRelease(image);
    assert(destroyCount == 0);

    TRGlyphImageRelease(image);
    assert(destroyCount == 1);
    assert(lastDestroyed == &first);
}

void GlyphImageTests::testNativeDataWithoutDestroy() {
    TRGlyphImageRef image = createAlphaImage();
    int value = 5;

    destroyCount = 0;

    assert(TRGlyphImageSetNativeData(image, &value, nullptr));
    assert(TRGlyphImageGetNativeData(image) == &value);

    /* There is nothing to call when the image goes away. */
    TRGlyphImageRelease(image);
    assert(destroyCount == 0);

    /* An image without data can go away as well. */
    TRGlyphImageRelease(createAlphaImage());
}

void GlyphImageTests::testNativeDataRace() {
    TRGlyphImageRef image = createAlphaImage();
    constexpr size_t NumThreads = 8;
    vector<int> values(NumThreads);
    atomic<int> winners{0};

    destroyCount = 0;

    vector<thread> threads;
    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&, i]() {
            TRGlyphImageRetain(image);

            if (TRGlyphImageSetNativeData(image, &values[i], destroyData)) {
                winners++;
            }

            assert(TRGlyphImageGetNativeData(image) != nullptr);
            TRGlyphImageRelease(image);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    /* Exactly one of the threads wins, and its data is destroyed once. */
    assert(winners == 1);
    TRGlyphImageRelease(image);
    assert(destroyCount == 1);
}

void GlyphImageTests::testRetainRelease() {
    TRGlyphImageRef image = createAlphaImage();

    assert(TRGlyphImageRetain(image) == image);
    assert(AtomicUIntLoad(&image->_base.retainCount) == 2);

    TRGlyphImageRelease(image);
    assert(AtomicUIntLoad(&image->_base.retainCount) == 1);
    assert(TRGlyphImageGetWidth(image) == 3);

    TRGlyphImageRelease(image);
}

#ifdef STANDALONE_TESTING

int main() {
    GlyphImageTests tests;
    tests.run();

    return 0;
}

#endif
