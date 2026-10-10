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

#ifndef _TEHREER__GLYPH_BITMAP_TESTS_H
#define _TEHREER__GLYPH_BITMAP_TESTS_H

namespace Tehreer {

class GlyphBitmapTests {
public:
    GlyphBitmapTests() = default;

    void run();

private:
    void testGrayBitmap();
    void testMonoBitmap();
    void testColorBitmap();
    void testNegativePitch();
    void testEmptyBitmap();
    void testUnsupportedPixelMode();
    void testCreateFromBitmap();
    void testStrokeSquare();
    void testStrokeJoins();
    void testStrokeCaps();
    void testStrokeRadius();
    void testStrokeEmptyOutline();
};

}

#endif
