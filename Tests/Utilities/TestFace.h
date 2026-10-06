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

#ifndef _TEHREER__TEST_FACE_H
#define _TEHREER__TEST_FACE_H

#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "TestFonts.h"

namespace Tehreer {

class TestFace {
public:
    explicit TestFace(const char *fontName) {
        FT_Error error = FT_Init_FreeType(&m_library);
        assert(error == FT_Err_Ok);

        error = FT_New_Face(m_library, testFontPath(fontName).c_str(), 0, &m_face);
        assert(error == FT_Err_Ok);
    }

    ~TestFace() {
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_library);
    }

    TestFace(const TestFace &) = delete;
    TestFace &operator=(const TestFace &) = delete;

    FT_Face get() const { return m_face; }

private:
    FT_Library m_library = nullptr;
    FT_Face m_face = nullptr;
};

}

#endif
