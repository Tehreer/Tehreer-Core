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

#ifndef _TEHREER_API_FONT_FILE_H
#define _TEHREER_API_FONT_FILE_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRFontFile.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _TRFontFile {
    ObjectBase _base;
    FT_Open_Args _arguments;
    TRUInteger numFaces;
} TRFontFile;

TR_INTERNAL FT_Face TRFontFileCreateFTFace(TRFontFileRef fontFile, TRUInteger faceIndex);

#endif
