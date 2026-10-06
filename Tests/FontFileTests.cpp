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
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRFontFile.h>

extern "C" {
#include <API/TRFontFile.h>
#include <Core/Mutex.h>
#include <Graphics/FreeType.h>
}

#include "TestFonts.h"

#include "FontFileTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 50;

void FontFileTests::run() {
    testCreateFromPath();
    testPathIsCopied();
    testCreateFromMemory();
    testMemoryIsCopied();
    testInvalidInput();
    testCreateFTFace();
    testRetainRelease();
    testConcurrentFaceCreation();
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

void FontFileTests::testCreateFromPath() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromPath(path.c_str());

    assert(fontFile != nullptr);
    assert(fontFile->numFaces == 1);

    TRFontFileRelease(fontFile);
}

void FontFileTests::testPathIsCopied() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromPath(path.c_str());

    path.assign(path.size(), 'X');
    path.clear();
    path.shrink_to_fit();

    FT_Face face = TRFontFileCreateFTFace(fontFile, 0);
    assert(face != nullptr);
    assert(face->num_glyphs == 4);

    destroyFace(face);
    TRFontFileRelease(fontFile);
}

void FontFileTests::testCreateFromMemory() {
    vector<uint8_t> data = readFont("Roboto-Variable.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromMemory(data.data(), data.size());

    assert(fontFile != nullptr);
    assert(fontFile->numFaces == 1);

    FT_Face face = TRFontFileCreateFTFace(fontFile, 0);
    assert(face != nullptr);
    assert(FT_HAS_MULTIPLE_MASTERS(face));

    destroyFace(face);
    TRFontFileRelease(fontFile);
}

void FontFileTests::testMemoryIsCopied() {
    vector<uint8_t> data = readFont("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromMemory(data.data(), data.size());

    fill(data.begin(), data.end(), 0);
    data.clear();
    data.shrink_to_fit();

    FT_Face face = TRFontFileCreateFTFace(fontFile, 0);
    assert(face != nullptr);
    assert(face->num_glyphs == 4);

    destroyFace(face);
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

void FontFileTests::testCreateFTFace() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromPath(path.c_str());

    FT_Face first = TRFontFileCreateFTFace(fontFile, 0);
    FT_Face second = TRFontFileCreateFTFace(fontFile, 0);

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first != second);

    assert(TRFontFileCreateFTFace(fontFile, 1) == nullptr);
    assert(TRFontFileCreateFTFace(fontFile, 100) == nullptr);

    destroyFace(first);
    destroyFace(second);
    TRFontFileRelease(fontFile);
}

void FontFileTests::testRetainRelease() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromPath(path.c_str());

    assert(TRFontFileRetain(fontFile) == fontFile);
    TRFontFileRelease(fontFile);

    FT_Face face = TRFontFileCreateFTFace(fontFile, 0);
    assert(face != nullptr);

    destroyFace(face);
    TRFontFileRelease(fontFile);
}

void FontFileTests::testConcurrentFaceCreation() {
    string path = testFontPath("Roboto-Regular.abc.ttf");
    TRFontFileRef fontFile = TRFontFileCreateFromPath(path.c_str());
    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([fontFile]() {
            for (size_t j = 0; j < Iterations; j++) {
                FT_Face face = TRFontFileCreateFTFace(fontFile, 0);
                assert(face != nullptr);
                assert(face->num_glyphs == 4);

                destroyFace(face);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    TRFontFileRelease(fontFile);
}

#ifdef STANDALONE_TESTING

int main() {
    FontFileTests tests;
    tests.run();

    return 0;
}

#endif
