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
#include <thread>
#include <vector>
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
    testNominalGlyphs();
    testVariationGlyph();
    testAdvances();
    testAdvancesAreCached();
    testShapingUsesFontFuncs();
    testDerived();
    testDerivedOfDerived();
    testDerivedKeepsRootAlive();
    testSharedFaceDoesNotLeakCoordinates();
    testConcurrentAdvances();
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

void ShapableFaceTests::testNominalGlyphs() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    hb_codepoint_t glyph = 0;
    assert(hb_font_get_nominal_glyph(face->hbFont, 'a', &glyph) && glyph == 1);
    assert(hb_font_get_nominal_glyph(face->hbFont, 'c', &glyph) && glyph == 3);
    assert(!hb_font_get_nominal_glyph(face->hbFont, 'z', &glyph));
    assert(!hb_font_get_nominal_glyph(face->hbFont, 0x1F600, &glyph));

    /* The batch version honors the strides, and stops at the first missing glyph. */
    struct Pair { hb_codepoint_t unicode; hb_codepoint_t padding; };
    struct Out { hb_codepoint_t padding[2]; hb_codepoint_t glyph; };
    const Pair input[] = { { 'c', 0 }, { 'a', 0 }, { 'z', 0 }, { 'b', 0 } };
    Out output[4] = {};

    unsigned int done = hb_font_get_nominal_glyphs(face->hbFont, 4,
        &input[0].unicode, sizeof(Pair), &output[0].glyph, sizeof(Out));
    assert(done == 2);
    assert(output[0].glyph == 3);
    assert(output[1].glyph == 1);
    assert(output[2].glyph == 0);

    ShapableFaceRelease(face);
}

void ShapableFaceTests::testVariationGlyph() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    /* The font has no variation sequences. */
    hb_codepoint_t glyph = 0;
    assert(!hb_font_get_variation_glyph(face->hbFont, 'a', 0xFE0F, &glyph));

    ShapableFaceRelease(face);
}

void ShapableFaceTests::testAdvances() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    assert(hb_font_get_glyph_h_advance(face->hbFont, 1) == 1114);
    assert(hb_font_get_glyph_h_advance(face->hbFont, 2) == 1149);
    assert(hb_font_get_glyph_h_advance(face->hbFont, 3) == 1072);

    struct In { hb_codepoint_t padding; hb_codepoint_t glyph; };
    struct Out { hb_position_t advance; hb_position_t padding[3]; };
    const In input[] = { { 0, 3 }, { 0, 1 }, { 0, 2 }, { 0, 3 } };
    Out output[4] = {};

    hb_font_get_glyph_h_advances(face->hbFont, 4, &input[0].glyph, sizeof(In),
        &output[0].advance, sizeof(Out));
    assert(output[0].advance == 1072);
    assert(output[1].advance == 1114);
    assert(output[2].advance == 1149);
    assert(output[3].advance == 1072);

    ShapableFaceRelease(face);
}

void ShapableFaceTests::testAdvancesAreCached() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    TRInt32 advance = 0;
    assert(!AdvanceCacheGet(&face->advanceCache, 2, &advance));

    assert(hb_font_get_glyph_h_advance(face->hbFont, 2) == 1149);
    assert(AdvanceCacheGet(&face->advanceCache, 2, &advance));
    assert(advance == 1149);
    assert(!AdvanceCacheGet(&face->advanceCache, 3, &advance));

    /* A value that is already cached is returned without asking the font. */
    AdvanceCachePut(&face->advanceCache, 2, 4242);
    assert(hb_font_get_glyph_h_advance(face->hbFont, 2) == 4242);

    ShapableFaceRelease(face);
}

void ShapableFaceTests::testShapingUsesFontFuncs() {
    RenderableFaceHolder holder("Roboto-Regular.abc.ttf");
    ShapableFaceRef face = ShapableFaceCreate(holder.get());

    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, "abcz", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(face->hbFont, buffer, nullptr, 0);

    unsigned int count = 0;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, nullptr);

    assert(count == 4);
    assert(infos[0].codepoint == 1 && positions[0].x_advance == 1114);
    assert(infos[1].codepoint == 2 && positions[1].x_advance == 1149);
    assert(infos[2].codepoint == 3 && positions[2].x_advance == 1072);
    /* The missing glyph is the notdef one. */
    assert(infos[3].codepoint == 0 && positions[3].x_advance == 908);

    hb_buffer_destroy(buffer);
    ShapableFaceRelease(face);
}

void ShapableFaceTests::testDerived() {
    RenderableFaceHolder holder("Roboto-Variable.abc.ttf");
    ShapableFaceRef root = ShapableFaceCreate(holder.get());

    const TRFloat thin[] = { 100.0f, 100.0f };
    const TRFloat black[] = { 900.0f, 100.0f };
    ShapableFaceRef thinFace = ShapableFaceCreateDerived(root, thin, 2);
    ShapableFaceRef blackFace = ShapableFaceCreateDerived(root, black, 2);

    assert(thinFace != nullptr && blackFace != nullptr);
    assert(root->rootFace == nullptr);
    assert(thinFace->rootFace == root);
    assert(blackFace->rootFace == root);

    /* They share the HarfBuzz face and the renderable face, but not the fonts. */
    assert(thinFace->hbFace == root->hbFace);
    assert(blackFace->renderableFace == holder.get());
    assert(thinFace->hbFont != root->hbFont);
    assert(thinFace->hbFont != blackFace->hbFont);

    assert(thinFace->coordinateCount == 2);
    assert(thinFace->coordinates[0] == 100 * 65536);
    assert(blackFace->coordinates[0] == 900 * 65536);
    /* The root has the default coordinates of the axes. */
    assert(root->coordinateCount == 2);
    assert(root->coordinates[0] == 400 * 65536);
    assert(root->coordinates[1] == 100 * 65536);

    /* Each font follows its own coordinates. */
    hb_position_t regular = hb_font_get_glyph_h_advance(root->hbFont, 2);
    hb_position_t thinAdvance = hb_font_get_glyph_h_advance(thinFace->hbFont, 2);
    hb_position_t blackAdvance = hb_font_get_glyph_h_advance(blackFace->hbFont, 2);

    assert(regular == 1150);
    assert(thinAdvance != regular);
    assert(blackAdvance != regular);
    assert(thinAdvance != blackAdvance);

    /* The advances were cached separately for each of them. */
    TRInt32 advance = 0;
    assert(AdvanceCacheGet(&thinFace->advanceCache, 2, &advance) && advance == thinAdvance);
    assert(AdvanceCacheGet(&blackFace->advanceCache, 2, &advance) && advance == blackAdvance);
    assert(!AdvanceCacheGet(&thinFace->advanceCache, 3, &advance));
    assert(hb_font_get_glyph_h_advance(root->hbFont, 2) == regular);

    /* Shaping applies the variations through the HarfBuzz font too. */
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, "b", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(blackFace->hbFont, buffer, nullptr, 0);

    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    assert(positions[0].x_advance == blackAdvance);

    hb_buffer_destroy(buffer);
    ShapableFaceRelease(blackFace);
    ShapableFaceRelease(thinFace);
    ShapableFaceRelease(root);
}

void ShapableFaceTests::testDerivedOfDerived() {
    RenderableFaceHolder holder("Roboto-Variable.abc.ttf");
    ShapableFaceRef root = ShapableFaceCreate(holder.get());

    const TRFloat first[] = { 100.0f, 100.0f };
    const TRFloat second[] = { 900.0f, 100.0f };
    ShapableFaceRef firstFace = ShapableFaceCreateDerived(root, first, 2);
    ShapableFaceRef secondFace = ShapableFaceCreateDerived(firstFace, second, 2);

    /* Derived faces always hang on the root, not on the face they were derived from. */
    assert(secondFace->rootFace == root);
    assert(secondFace->hbFace == root->hbFace);
    assert(secondFace->coordinates[0] == 900 * 65536);

    ShapableFaceRef reference = ShapableFaceCreateDerived(root, second, 2);
    assert(hb_font_get_glyph_h_advance(secondFace->hbFont, 2)
           == hb_font_get_glyph_h_advance(reference->hbFont, 2));

    ShapableFaceRelease(reference);
    ShapableFaceRelease(secondFace);
    ShapableFaceRelease(firstFace);
    ShapableFaceRelease(root);
}

void ShapableFaceTests::testDerivedKeepsRootAlive() {
    ShapableFaceRef derived = nullptr;

    {
        RenderableFaceHolder holder("Roboto-Variable.abc.ttf");
        ShapableFaceRef root = ShapableFaceCreate(holder.get());

        const TRFloat coordinates[] = { 700.0f, 100.0f };
        derived = ShapableFaceCreateDerived(root, coordinates, 2);

        ShapableFaceRelease(root);
    }

    /* The holder released its renderable face, but the derived face holds on to everything. */
    assert(hb_face_get_glyph_count(derived->hbFace) == 4);
    assert(hb_font_get_glyph_h_advance(derived->hbFont, 1) > 0);

    ShapableFaceRelease(derived);
}

void ShapableFaceTests::testSharedFaceDoesNotLeakCoordinates() {
    RenderableFaceHolder holder("Roboto-Variable.abc.ttf");
    ShapableFaceRef root = ShapableFaceCreate(holder.get());

    /* The derived faces use the same FreeType faces, and set their coordinates on them first. */
    const TRFloat black[] = { 900.0f, 100.0f };
    ShapableFaceRef blackFace = ShapableFaceCreateDerived(root, black, 2);
    hb_position_t blackAdvance = hb_font_get_glyph_h_advance(blackFace->hbFont, 2);

    /* The root must still measure with its own coordinates, in any order of use. */
    assert(hb_font_get_glyph_h_advance(root->hbFont, 2) == 1150);
    assert(hb_font_get_glyph_h_advance(blackFace->hbFont, 1) != hb_font_get_glyph_h_advance(root->hbFont, 1));
    assert(hb_font_get_glyph_h_advance(blackFace->hbFont, 2) == blackAdvance);
    assert(hb_font_get_glyph_h_advance(root->hbFont, 3) == 1072);

    ShapableFaceRelease(blackFace);
    ShapableFaceRelease(root);
}

void ShapableFaceTests::testConcurrentAdvances() {
    RenderableFaceHolder holder("Roboto-Variable.abc.ttf");
    ShapableFaceRef root = ShapableFaceCreate(holder.get());

    const TRFloat coordinates[] = { 700.0f, 100.0f };
    ShapableFaceRef derived = ShapableFaceCreateDerived(root, coordinates, 2);
    hb_position_t expected[4];
    for (hb_codepoint_t glyph = 0; glyph < 4; glyph++) {
        expected[glyph] = hb_font_get_glyph_h_advance(derived->hbFont, glyph);
    }

    /* A second face with the same coordinates starts cold, and is used from many threads. */
    ShapableFaceRef shared = ShapableFaceCreateDerived(root, coordinates, 2);
    vector<thread> threads;

    for (size_t i = 0; i < 8; i++) {
        threads.emplace_back([shared, &expected]() {
            for (size_t j = 0; j < 500; j++) {
                for (hb_codepoint_t glyph = 0; glyph < 4; glyph++) {
                    assert(hb_font_get_glyph_h_advance(shared->hbFont, glyph) == expected[glyph]);
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    ShapableFaceRelease(shared);
    ShapableFaceRelease(derived);
    ShapableFaceRelease(root);
}

#ifdef STANDALONE_TESTING

int main() {
    ShapableFaceTests tests;
    tests.run();

    return 0;
}

#endif
