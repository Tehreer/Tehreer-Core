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

#ifndef _TEHREER_FONT_FONT_DATA_H
#define _TEHREER_FONT_FONT_DATA_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include <API/TRBase.h>
#include <Core/Object.h>

/*
 * The data of a font file, from a path or from memory, which the faces of the font are opened
 * from. The render faces and the font file share it, so that the typefaces that the font file
 * keeps do not make a cycle with it.
 */
typedef struct _FontData {
    ObjectBase _base;
    FT_Open_Args _arguments;
    TRUInteger faceCount;
} FontData, *FontDataRef;

/* Creates the data from a path, which is copied. Returns NULL if it has no usable face. */
TR_INTERNAL FontDataRef FontDataCreateFromPath(const char *path);

/* Creates the data from memory, which is copied. Returns NULL if it has no usable face. */
TR_INTERNAL FontDataRef FontDataCreateFromMemory(const void *memory, TRUInteger size);

/*
 * Opens a face of the data, which the caller has to close with FreeType. Returns NULL if the face
 * cannot be opened, or has neither outlines nor bitmap strikes.
 */
TR_INTERNAL FT_Face FontDataCreateFTFace(FontDataRef fontData, TRUInteger faceIndex);

TR_INTERNAL FontDataRef FontDataRetain(FontDataRef fontData);
TR_INTERNAL void FontDataRelease(FontDataRef fontData);

#endif
