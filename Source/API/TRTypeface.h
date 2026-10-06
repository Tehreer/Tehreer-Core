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

#ifndef _TEHREER_API_TYPEFACE_H
#define _TEHREER_API_TYPEFACE_H

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>

typedef struct _TRTypeface {
    ObjectBase _base;

    RenderableFaceRef renderableFace;
    ShapableFaceRef shapableFace;

    FT_Fixed *rawCoordinates;
    TRFloat *faceCoordinates;

    FT_Color *rawColors;
    TRColor *faceColors;

    TRWeight weight;
    TRWidth width;
    TRSlope slope;

    const TRStringView *familyName;
    const TRStringView *subfamilyName;

    TRUInt32 unitsPerEM;
    TRUInt32 ascent;
    TRUInt32 descent;
    TRUInt32 leading;
    TRInt32 underlinePosition;
    TRUInt32 underlineThickness;
} TRTypeface;

TR_INTERNAL TRTypefaceRef TRTypefaceCreateDefault(RenderableFaceRef renderableFace,
    ShapableFaceRef shapableFace, const TRFloat *variationCoordinates);

#endif
