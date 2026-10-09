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

#ifndef _TEHREER__TEST_TYPEFACE_H
#define _TEHREER__TEST_TYPEFACE_H

#include <cassert>

#include <Tehreer/TRTypeface.h>

extern "C" {
#include <API/TRTypeface.h>
#include <Font/FontData.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>
}

#include "TestFonts.h"

namespace Tehreer {

inline TRTypefaceRef createTestTypeface(const char *fontName,
    const TRFloat *variationCoordinates = nullptr, TRUInteger faceIndex = 0) {
    FontDataRef fontData = FontDataCreateFromPath(testFontPath(fontName).c_str());
    assert(fontData != nullptr);

    RenderableFaceRef renderableFace = RenderableFaceCreate(fontData, faceIndex);
    assert(renderableFace != nullptr);

    ShapableFaceRef shapableFace = ShapableFaceCreate(renderableFace);
    assert(shapableFace != nullptr);

    TRTypefaceRef typeface = TRTypefaceCreateDefault(renderableFace, shapableFace,
        variationCoordinates);
    assert(typeface != nullptr);

    ShapableFaceRelease(shapableFace);
    RenderableFaceRelease(renderableFace);
    FontDataRelease(fontData);

    return typeface;
}

}

#endif
