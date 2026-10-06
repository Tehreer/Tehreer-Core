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
#include <cstring>
#include <string>

#include <hb.h>

#include <Tehreer/TRFontFile.h>

extern "C" {
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>
}

#include "TestFonts.h"

#include "ShapableFaceTests.h"

using namespace std;
using namespace Tehreer;

void ShapableFaceTests::run() {
    testCreate();
    testTableAccess();
    testMissingTable();
    testShaping();
    testRetainRelease();
}

class RenderableFaceHolder {
public:
    explicit RenderableFaceHolder(const char *fontName) {
        m_path = testFontPath(fontName);
        m_fontFile = TRFontFileCreateFromPath(m_path.c_str());
        assert(m_fontFile != nullptr);

        m_face = RenderableFaceCreate(m_fontFile, 0);
        assert(m_face != nullptr);
    }

    ~RenderableFaceHolder() {
        RenderableFaceRelease(m_face);
        TRFontFileRelease(m_fontFile);
    }

    RenderableFaceRef get() const { return m_face; }

private:
    string m_path;
    TRFontFileRef m_fontFile = nullptr;
    RenderableFaceRef m_face = nullptr;
};

void ShapableFaceTests::testCreate() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    assert(face != nullptr);
    assert(face->renderableFace == holder.get());
    assert(face->hbFace != nullptr);
    assert(hb_face_get_glyph_count(face->hbFace) == 4);
    assert(hb_face_get_upem(face->hbFace) == 2048);

    ShapableFaceRelease(face);
}

void ShapableFaceTests::testTableAccess() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    hb_blob_t *blob = hb_face_reference_table(face->hbFace, HB_TAG('h', 'e', 'a', 'd'));
    unsigned int length = 0;
    const char *data = hb_blob_get_data(blob, &length);

    assert(length == 54);
    assert(data != nullptr);
    assert(memcmp(data + 12, "\x5F\x0F\x3C\xF5", 4) == 0);

    hb_blob_destroy(blob);
    ShapableFaceRelease(face);
}

void ShapableFaceTests::testMissingTable() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    hb_blob_t *blob = hb_face_reference_table(face->hbFace, HB_TAG('Z', 'Z', 'Z', 'Z'));

    assert(hb_blob_get_length(blob) == 0);

    hb_blob_destroy(blob);
    ShapableFaceRelease(face);
}

void ShapableFaceTests::testShaping() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    hb_font_t *font = hb_font_create(face->hbFace);
    hb_buffer_t *buffer = hb_buffer_create();

    hb_buffer_add_utf8(buffer, "abc", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, nullptr, 0);

    unsigned int count = 0;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &count);

    assert(count == 3);
    assert(infos[0].codepoint == 1);
    assert(infos[1].codepoint == 2);
    assert(infos[2].codepoint == 3);
    assert(positions[0].x_advance == 1114);
    assert(positions[1].x_advance == 1149);
    assert(positions[2].x_advance == 1072);

    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    ShapableFaceRelease(face);
}

void ShapableFaceTests::testRetainRelease() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    assert(ShapableFaceRetain(face) == face);
    ShapableFaceRelease(face);
    assert(hb_face_get_glyph_count(face->hbFace) == 4);

    ShapableFaceRelease(face);
}

#ifdef STANDALONE_TESTING

int main() {
    ShapableFaceTests tests;
    tests.run();

    return 0;
}

#endif
