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
#include <initializer_list>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

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
    testCreateFromBitmap();
    testStrokeSquare();
    testStrokeJoins();
    testStrokeCaps();
    testStrokeRadius();
    testStrokeEmptyOutline();
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

void GlyphBitmapTests::testCreateFromBitmap() {
    /* The same conversion as for a slot, with the position given directly. */
    unsigned char pixels[] = { 9, 8, 7, 6 };
    FT_Bitmap ftBitmap = {};
    ftBitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    ftBitmap.width = 2;
    ftBitmap.rows = 2;
    ftBitmap.pitch = 2;
    ftBitmap.buffer = pixels;

    GlyphBitmapRef bitmap = GlyphBitmapCreateFromBitmap(&ftBitmap, -3, 5);
    assert(bitmap != nullptr);
    assert(bitmap->left == -3 && bitmap->top == 5);
    assert(bitmap->width == 2 && bitmap->height == 2);
    assert(bitmap->format == BitmapFormatAlpha);
    assert(memcmp(bitmap->buffer, pixels, 4) == 0);
    GlyphBitmapDestroy(bitmap);

    ftBitmap.width = 0;
    assert(GlyphBitmapCreateFromBitmap(&ftBitmap, 0, 0) == nullptr);
}

/* A square of 10 by 10 pixels, with the corners in 26.6 format, as FreeType keeps them. */
struct SquareOutline {
    FT_Vector points[4];
    unsigned char tags[4];
    unsigned short contours[1];
    FT_Outline outline;

    SquareOutline() {
        const FT_Pos size = 10 * 64;
        points[0] = { 0, 0 };
        points[1] = { size, 0 };
        points[2] = { size, size };
        points[3] = { 0, size };
        for (auto &tag : tags) {
            tag = FT_CURVE_TAG_ON;
        }
        contours[0] = 3;

        outline = {};
        outline.n_points = 4;
        outline.n_contours = 1;
        outline.points = points;
        outline.tags = reinterpret_cast<unsigned char *>(tags);
        outline.contours = contours;
    }
};

static unsigned char pixelAt(GlyphBitmapRef bitmap, unsigned x, unsigned y) {
    return bitmap->buffer[y * bitmap->width + x];
}

void GlyphBitmapTests::testStrokeSquare() {
    SquareOutline square;

    /* A line of one pixel wide has a radius of half a pixel. */
    GlyphBitmapRef bitmap = GlyphBitmapCreateFromStroke(&square.outline, 32,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_MITER, 4 * 0x10000);

    assert(bitmap != nullptr);
    assert(bitmap->format == BitmapFormatAlpha);

    /* The stroke goes half a pixel out of the outline, which rounds up to a whole pixel. */
    assert(bitmap->left == -1);
    assert(bitmap->top == 11);
    assert(bitmap->width == 12);
    assert(bitmap->height == 12);

    /* The inside is empty, while the lines have ink. */
    assert(pixelAt(bitmap, 6, 6) == 0);
    assert(pixelAt(bitmap, 6, 0) > 0);
    assert(pixelAt(bitmap, 6, 11) > 0);
    assert(pixelAt(bitmap, 0, 6) > 0);
    assert(pixelAt(bitmap, 11, 6) > 0);

    GlyphBitmapDestroy(bitmap);

    /* The input is not changed. */
    assert(square.outline.n_points == 4);
    assert(square.points[1].x == 640);
}

void GlyphBitmapTests::testStrokeJoins() {
    SquareOutline square;

    GlyphBitmapRef miter = GlyphBitmapCreateFromStroke(&square.outline, 128,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_MITER, 4 * 0x10000);
    GlyphBitmapRef bevel = GlyphBitmapCreateFromStroke(&square.outline, 128,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_BEVEL, 4 * 0x10000);
    GlyphBitmapRef round = GlyphBitmapCreateFromStroke(&square.outline, 128,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 4 * 0x10000);

    assert(miter && bevel && round);

    /* The sharp corner is only fully covered with a miter. */
    assert(pixelAt(miter, 0, 0) == 255);
    assert(pixelAt(bevel, 0, 0) < pixelAt(miter, 0, 0));
    assert(pixelAt(round, 0, 0) < pixelAt(miter, 0, 0));

    GlyphBitmapDestroy(round);
    GlyphBitmapDestroy(bevel);
    GlyphBitmapDestroy(miter);
}

void GlyphBitmapTests::testStrokeCaps() {
    /* The caps only matter for open lines, so this uses a closed square as a smoke test. */
    SquareOutline square;

    for (FT_Stroker_LineCap cap : { FT_STROKER_LINECAP_BUTT, FT_STROKER_LINECAP_ROUND,
                                    FT_STROKER_LINECAP_SQUARE }) {
        GlyphBitmapRef bitmap = GlyphBitmapCreateFromStroke(&square.outline, 64, cap,
            FT_STROKER_LINEJOIN_ROUND, 0x10000);

        assert(bitmap != nullptr);
        assert(bitmap->width == 12 && bitmap->height == 12);

        GlyphBitmapDestroy(bitmap);
    }
}

void GlyphBitmapTests::testStrokeRadius() {
    SquareOutline square;

    GlyphBitmapRef thin = GlyphBitmapCreateFromStroke(&square.outline, 32,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_MITER, 4 * 0x10000);
    GlyphBitmapRef thick = GlyphBitmapCreateFromStroke(&square.outline, 3 * 64,
        FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_MITER, 4 * 0x10000);

    /* A radius of three pixels reaches three pixels out, and has a smaller hole. */
    assert(thick->left == -3);
    assert(thick->width == 16 && thick->height == 16);
    assert(thick->width > thin->width);
    assert(pixelAt(thick, 8, 8) == 0);
    assert(pixelAt(thick, 8, 1) == 255);

    GlyphBitmapDestroy(thick);
    GlyphBitmapDestroy(thin);
}

void GlyphBitmapTests::testStrokeEmptyOutline() {
    FT_Outline outline = {};

    assert(GlyphBitmapCreateFromStroke(&outline, 64, FT_STROKER_LINECAP_BUTT,
        FT_STROKER_LINEJOIN_ROUND, 0x10000) == nullptr);
}

#ifdef STANDALONE_TESTING

int main() {
    GlyphBitmapTests tests;
    tests.run();

    return 0;
}

#endif
