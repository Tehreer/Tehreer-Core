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
#include <cstdint>
#include <cstring>

#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" {
#include <Graphics/GlyphBitmap.h>
}

#include "GlyphBitmapTests.h"

using namespace std;
using namespace Tehreer;

void GlyphBitmapTests::run() {
    testGrayBitmap();
    testMonoBitmap();
    testColorBitmap();
    testNegativePitch();
    testEmptyBitmap();
    testUnsupportedPixelMode();
}

static FT_GlyphSlotRec makeSlot(unsigned char pixelMode, unsigned int width, unsigned int rows,
    int pitch, unsigned char *buffer, int left, int top) {
    FT_GlyphSlotRec slot = {};

    slot.bitmap.pixel_mode = pixelMode;
    slot.bitmap.width = width;
    slot.bitmap.rows = rows;
    slot.bitmap.pitch = pitch;
    slot.bitmap.buffer = buffer;
    slot.bitmap_left = left;
    slot.bitmap_top = top;

    return slot;
}

void GlyphBitmapTests::testGrayBitmap() {
    unsigned char pixels[] = {
        1, 2, 3, 0xEE,
        4, 5, 6, 0xEE
    };
    FT_GlyphSlotRec slot = makeSlot(FT_PIXEL_MODE_GRAY, 3, 2, 4, pixels, -2, 7);

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromSlot(&slot);

    assert(bitmap != nullptr);
    assert(bitmap->left == -2);
    assert(bitmap->top == 7);
    assert(bitmap->width == 3);
    assert(bitmap->height == 2);
    assert(bitmap->format == BitmapFormatAlpha);

    const uint8_t expected[] = { 1, 2, 3, 4, 5, 6 };
    assert(memcmp(bitmap->buffer, expected, sizeof(expected)) == 0);

    GlyphBitmapDestroy(bitmap);
}

void GlyphBitmapTests::testMonoBitmap() {
    unsigned char pixels[] = {
        0xA5, 0xC0,
        0x00, 0x40
    };
    FT_GlyphSlotRec slot = makeSlot(FT_PIXEL_MODE_MONO, 10, 2, 2, pixels, 0, 0);

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromSlot(&slot);

    assert(bitmap != nullptr);
    assert(bitmap->width == 10);
    assert(bitmap->height == 2);
    assert(bitmap->format == BitmapFormatAlpha);

    const uint8_t expected[] = {
        255, 0, 255, 0, 0, 255, 0, 255, 255, 255,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 255
    };
    assert(memcmp(bitmap->buffer, expected, sizeof(expected)) == 0);

    GlyphBitmapDestroy(bitmap);
}

void GlyphBitmapTests::testColorBitmap() {
    unsigned char pixels[] = {
        0x01, 0x02, 0x03, 0x04,  0x11, 0x12, 0x13, 0x14,
        0x21, 0x22, 0x23, 0x24,  0x31, 0x32, 0x33, 0x34
    };
    FT_GlyphSlotRec slot = makeSlot(FT_PIXEL_MODE_BGRA, 2, 2, 8, pixels, 3, 4);

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromSlot(&slot);

    assert(bitmap != nullptr);
    assert(bitmap->left == 3);
    assert(bitmap->top == 4);
    assert(bitmap->format == BitmapFormatARGB);

    const uint8_t expected[] = {
        0x04, 0x03, 0x02, 0x01,  0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,  0x34, 0x33, 0x32, 0x31
    };
    assert(memcmp(bitmap->buffer, expected, sizeof(expected)) == 0);

    GlyphBitmapDestroy(bitmap);
}

void GlyphBitmapTests::testNegativePitch() {
    unsigned char pixels[] = {
        4, 5, 6, 0xEE,
        1, 2, 3, 0xEE
    };
    FT_GlyphSlotRec slot = makeSlot(FT_PIXEL_MODE_GRAY, 3, 2, -4, pixels + 4, 0, 0);

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromSlot(&slot);

    assert(bitmap != nullptr);

    const uint8_t expected[] = { 1, 2, 3, 4, 5, 6 };
    assert(memcmp(bitmap->buffer, expected, sizeof(expected)) == 0);

    GlyphBitmapDestroy(bitmap);
}

void GlyphBitmapTests::testEmptyBitmap() {
    unsigned char pixel = 0;

    FT_GlyphSlotRec noWidth = makeSlot(FT_PIXEL_MODE_GRAY, 0, 2, 0, &pixel, 0, 0);
    assert(GlyphBitmapCreateFromSlot(&noWidth) == nullptr);

    FT_GlyphSlotRec noRows = makeSlot(FT_PIXEL_MODE_GRAY, 2, 0, 2, &pixel, 0, 0);
    assert(GlyphBitmapCreateFromSlot(&noRows) == nullptr);
}

void GlyphBitmapTests::testUnsupportedPixelMode() {
    unsigned char pixels[16] = { 0 };

    FT_GlyphSlotRec gray2 = makeSlot(FT_PIXEL_MODE_GRAY2, 4, 2, 2, pixels, 0, 0);
    assert(GlyphBitmapCreateFromSlot(&gray2) == nullptr);

    FT_GlyphSlotRec lcd = makeSlot(FT_PIXEL_MODE_LCD, 6, 2, 6, pixels, 0, 0);
    assert(GlyphBitmapCreateFromSlot(&lcd) == nullptr);
}

#ifdef STANDALONE_TESTING

int main() {
    GlyphBitmapTests tests;
    tests.run();

    return 0;
}

#endif
