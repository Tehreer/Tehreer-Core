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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRFontFile.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRFontFile.h>
#include <Core/Mutex.h>
#include <Font/FontData.h>
#include <Graphics/FreeType.h>
}

#include "TestFonts.h"

#include "FontFileTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 50;

void FontFileTests::run() {
    testDataFromPath();
    testPathIsCopied();
    testDataFromMemory();
    testMemoryIsCopied();
    testInvalidDataInput();
    testCreateFTFace();
    testDataRetainRelease();
    testConcurrentFaceCreation();
    testCreateFromPath();
    testCreateFromMemory();
    testInvalidInput();
    testDefaultTypefaces();
    testNamedStyleTypefaces();
    testBitmapFontFile();
    testTypefaceOutlivesFontFile();
    testRetainRelease();
}

static vector<uint8_t> readFont(const char *fontName) {
    ifstream stream(testFontPath(fontName), ios::binary);
    assert(stream.good());

    return vector<uint8_t>(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());
}

static void destroyFace(FT_Face face) {
    FreeTypeRef freetype = FreeTypeGetDefault();

    MutexLock(&freetype->mutex);
    FT_Done_Face(face);
    MutexUnlock(&freetype->mutex);
}

static string toString(const TRStringView *view) {
    assert(view != nullptr && view->encoding == TRStringEncodingUTF16);

    const auto *units = static_cast<const char16_t *>(view->buffer);
    string result;
    for (size_t i = 0; i < view->length; i++) {
        result.push_back(static_cast<char>(units[i]));
    }

    return result;
}

/* The data of a font file is what the faces are opened from. */

void FontFileTests::testDataFromPath() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromPath(path.c_str());

    assert(fontData != nullptr);
    assert(fontData->faceCount == 1);

    FontDataRelease(fontData);
}

void FontFileTests::testPathIsCopied() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromPath(path.c_str());

    path.assign(path.size(), 'X');
    path.clear();
    path.shrink_to_fit();

    FT_Face face = FontDataCreateFTFace(fontData, 0);
    assert(face != nullptr);
    assert(face->num_glyphs == 4);

    destroyFace(face);
    FontDataRelease(fontData);
}

void FontFileTests::testDataFromMemory() {
    vector<uint8_t> data = readFont("Roboto-Variable.abc.ttf");
    FontDataRef fontData = FontDataCreateFromMemory(data.data(), data.size());

    assert(fontData != nullptr);
    assert(fontData->faceCount == 1);

    FT_Face face = FontDataCreateFTFace(fontData, 0);
    assert(face != nullptr);
    assert(FT_HAS_MULTIPLE_MASTERS(face));

    destroyFace(face);
    FontDataRelease(fontData);
}

void FontFileTests::testMemoryIsCopied() {
    vector<uint8_t> data = readFont("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromMemory(data.data(), data.size());

    fill(data.begin(), data.end(), 0);
    data.clear();
    data.shrink_to_fit();

    FT_Face face = FontDataCreateFTFace(fontData, 0);
    assert(face != nullptr);
    assert(face->num_glyphs == 4);

    destroyFace(face);
    FontDataRelease(fontData);
}

void FontFileTests::testInvalidDataInput() {
    vector<uint8_t> garbage(256, 0x7F);
    vector<uint8_t> data = readFont("Roboto-Regular.abc.ttf");

    assert(FontDataCreateFromPath(nullptr) == nullptr);
    assert(FontDataCreateFromPath("/nonexistent/font.ttf") == nullptr);
    assert(FontDataCreateFromPath(TEST_FONTS_DIR) == nullptr);
    assert(FontDataCreateFromMemory(nullptr, 16) == nullptr);
    assert(FontDataCreateFromMemory(data.data(), 0) == nullptr);
    assert(FontDataCreateFromMemory(garbage.data(), garbage.size()) == nullptr);
    assert(FontDataCreateFromMemory(data.data(), 12) == nullptr);
}

void FontFileTests::testCreateFTFace() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromPath(path.c_str());

    FT_Face first = FontDataCreateFTFace(fontData, 0);
    FT_Face second = FontDataCreateFTFace(fontData, 0);

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first != second);

    assert(FontDataCreateFTFace(fontData, 1) == nullptr);
    assert(FontDataCreateFTFace(fontData, 100) == nullptr);

    destroyFace(first);
    destroyFace(second);
    FontDataRelease(fontData);
}

void FontFileTests::testDataRetainRelease() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromPath(path.c_str());

    assert(FontDataRetain(fontData) == fontData);
    FontDataRelease(fontData);

    FT_Face face = FontDataCreateFTFace(fontData, 0);
    assert(face != nullptr);

    destroyFace(face);
    FontDataRelease(fontData);
}

void FontFileTests::testConcurrentFaceCreation() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    FontDataRef fontData = FontDataCreateFromPath(path.c_str());
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([fontData]() {
            for (size_t j = 0; j < Iterations; j++) {
                FT_Face face = FontDataCreateFTFace(fontData, 0);
                assert(face != nullptr);
                assert(face->num_glyphs == 4);

                destroyFace(face);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    FontDataRelease(fontData);
}

/* The font file has the default typefaces that were made from the data when it was created. */

void FontFileTests::testCreateFromPath() {
    TRFontFileRef fontFile = TRFontFileCreateFromPath(testFontPath("Roboto-Regular.abc.ttf").c_str());

    assert(fontFile != nullptr);
    assert(TRFontFileGetTypefaceCount(fontFile) == 1);

    TRTypefaceRef typeface = TRFontFileGetTypeface(fontFile, 0);
    assert(typeface != nullptr);
    assert(toString(TRTypefaceGetFamilyName(typeface)) == "Roboto");

    /* The path is copied, so the font file does not depend on the string. */
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef copied = TRFontFileCreateFromPath(path.c_str());
    path.assign(path.size(), 'X');
    assert(TRTypefaceGetGlyphID(TRFontFileGetTypeface(copied, 0), 'a') == 1);

    TRFontFileRelease(copied);
    TRFontFileRelease(fontFile);
}

void FontFileTests::testCreateFromMemory() {
    vector<uint8_t> data = readFont("Roboto-Variable.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromMemory(data.data(), data.size());
    assert(fontFile != nullptr);

    /* The data is copied, so the buffer can go away. */
    fill(data.begin(), data.end(), 0);
    data.clear();
    data.shrink_to_fit();

    TRTypefaceRef typeface = TRFontFileGetTypeface(fontFile, 0);
    assert(typeface != nullptr);
    assert(TRTypefaceGetVariationAxisCount(typeface) == 2);
    assert(TRTypefaceGetGlyphID(typeface, 'a') != 0);

    TRFontFileRelease(fontFile);
}

void FontFileTests::testInvalidInput() {
    vector<uint8_t> garbage(256, 0x7F);
    vector<uint8_t> data = readFont("Roboto-Regular.abc.ttf");

    assert(TRFontFileCreateFromPath(nullptr) == nullptr);
    assert(TRFontFileCreateFromPath("/nonexistent/font.ttf") == nullptr);
    assert(TRFontFileCreateFromPath(TEST_FONTS_DIR) == nullptr);
    assert(TRFontFileCreateFromMemory(nullptr, 16) == nullptr);
    assert(TRFontFileCreateFromMemory(data.data(), 0) == nullptr);
    assert(TRFontFileCreateFromMemory(garbage.data(), garbage.size()) == nullptr);
    assert(TRFontFileCreateFromMemory(data.data(), 12) == nullptr);
}

void FontFileTests::testDefaultTypefaces() {
    /* A static face has a single default typeface, which has its default style. */
    TRFontFileRef staticFile = TRFontFileCreateFromPath(testFontPath("Roboto-Regular.abc.ttf").c_str());
    assert(TRFontFileGetTypefaceCount(staticFile) == 1);

    TRTypefaceRef typeface = TRFontFileGetTypeface(staticFile, 0);
    assert(typeface != nullptr);
    assert(TRTypefaceGetVariationAxisCount(typeface) == 0);

    /* The typefaces are the same objects whenever they are asked for. */
    assert(TRFontFileGetTypeface(staticFile, 0) == typeface);

    assert(TRFontFileGetTypeface(staticFile, 1) == nullptr);
    assert(TRFontFileGetTypeface(staticFile, TRInvalidIndex) == nullptr);
    TRFontFileRelease(staticFile);
}

void FontFileTests::testNamedStyleTypefaces() {
    /* A variable font has a default typeface for each of its named styles. */
    TRFontFileRef variableFile = TRFontFileCreateFromPath(testFontPath("Roboto-Variable.abc.ttf").c_str());
    TRUInteger count = TRFontFileGetTypefaceCount(variableFile);
    assert(count == 18);

    TRTypefaceRef first = TRFontFileGetTypeface(variableFile, 0);
    const TRNamedStyle *styles = TRTypefaceGetNamedStylesPtr(first);
    assert(TRTypefaceGetNamedStyleCount(first) == count);

    for (TRUInteger index = 0; index < count; index++) {
        TRTypefaceRef styled = TRFontFileGetTypeface(variableFile, index);
        assert(styled != nullptr);

        /* The typeface takes the coordinates and the name of its style. */
        const TRFloat *coordinates = TRTypefaceGetVariationCoordinatesPtr(styled);
        assert(TRTypefaceGetVariationAxisCount(styled) == styles[index].coordinateCount);
        for (TRUInteger axis = 0; axis < styles[index].coordinateCount; axis++) {
            assert(coordinates[axis] == styles[index].coordinatesPtr[axis]);
        }

        const TRStringView *name = TRTypefaceGetSubfamilyName(styled);
        assert(name != nullptr);
        assert(toString(name) == toString(styles[index].subfamilyName));
    }

    assert(TRFontFileGetTypeface(variableFile, count) == nullptr);
    TRFontFileRelease(variableFile);
}

void FontFileTests::testBitmapFontFile() {
    /* A font that only has bitmaps is usable, as its face has strikes instead of outlines. */
    TRFontFileRef fontFile = TRFontFileCreateFromPath(testFontPath("NotoColorEmoji-CBDT.flags.ttf").c_str());
    assert(fontFile != nullptr);
    assert(TRFontFileGetTypefaceCount(fontFile) == 1);

    TRTypefaceRef typeface = TRFontFileGetTypeface(fontFile, 0);
    assert(typeface != nullptr);
    assert(!TRTypefaceIsScalable(typeface));

    TRFontFileRelease(fontFile);
}

void FontFileTests::testTypefaceOutlivesFontFile() {
    TRFontFileRef fontFile = TRFontFileCreateFromPath(testFontPath("Roboto-Variable.abc.ttf").c_str());
    TRTypefaceRef typeface = TRTypefaceRetain(TRFontFileGetTypeface(fontFile, 3));

    /* The font file keeps its typefaces, and they do not keep it, so it is destroyed here. */
    TRFontFileRelease(fontFile);

    assert(TRTypefaceGetGlyphID(typeface, 'a') != 0);
    assert(TRTypefaceGetGlyphAdvance(typeface, TRTypefaceGetGlyphID(typeface, 'a'), 2048.0f, TRFalse) > 0.0f);

    TRTypefaceRelease(typeface);
}

void FontFileTests::testRetainRelease() {
    TRFontFileRef fontFile = TRFontFileCreateFromPath(testFontPath("Roboto-Regular.abc.ttf").c_str());

    assert(TRFontFileRetain(fontFile) == fontFile);
    TRFontFileRelease(fontFile);

    assert(TRFontFileGetTypefaceCount(fontFile) == 1);

    TRFontFileRelease(fontFile);
}

#ifdef STANDALONE_TESTING

int main() {
    FontFileTests tests;
    tests.run();

    return 0;
}

#endif
