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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <API/TRBase.h>
#include <API/TRTypeface.h>
#include <Core/Mutex.h>
#include <Core/Object.h>
#include <Font/FaceMetadata.h>
#include <Graphics/FreeType.h>
#include <Graphics/RenderableFace.h>

#include "TRFontFile.h"

static TRUInteger GetFaceCountInFontFile(const FT_Open_Args *args)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    FT_Face rawFace = NULL;
    FT_Long numFaces = 0;
    FT_Error error;

    MutexLock(&freetype->mutex);

    error = FT_Open_Face(freetype->library, args, -1, &rawFace);
    if (error == FT_Err_Ok) {
        numFaces = rawFace->num_faces;
        FT_Done_Face(rawFace);
    }

    MutexUnlock(&freetype->mutex);

    return numFaces;
}

static void FinalizeFontFile(ObjectRef object)
{
    TRFontFileRef fontFile = object;
    const FT_Open_Args *arguments = &fontFile->_arguments;
    void *buffer = (void *)arguments->memory_base;
    void *pathname = arguments->pathname;

    if (buffer) {
        free(buffer);
    }
    if (pathname) {
        free(pathname);
    }
}

static TRFontFileRef CreateFontFileWithArguments(const FT_Open_Args *arguments)
{
    const TRUInteger size = sizeof(TRFontFile);
    void *pointer = NULL;
    TRFontFile *fontFile;

    fontFile = ObjectCreate(&size, 1, &pointer, FinalizeFontFile);

    if (fontFile) {
        fontFile->_arguments = *arguments;
        fontFile->numFaces = GetFaceCountInFontFile(arguments);

        if (fontFile->numFaces == 0) {
            ObjectRelease(fontFile);
            fontFile = NULL;
        }
    } else {
        free((void *)arguments->memory_base);
        free(arguments->pathname);
    }

    return fontFile;
}

static void LoadDefaultTypefaces(TRFontFileRef fontFile)
{
    TRUInteger faceCount = fontFile->numFaces;
    TRUInteger faceIndex;

    for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
        RenderableFaceRef renderableFace = RenderableFaceCreate(fontFile, faceIndex);
        FaceMetadataRef metadata = renderableFace->metadata;
        TRNamedStyle *stylesPtr = metadata->namedStylesPtr;
        TRUInteger styleCount = metadata->namedStyleCount;
        TRUInteger styleIndex;

        for (styleIndex = 0; styleIndex < styleCount; styleIndex++) {
            TRNamedStyle *currentStyle = &stylesPtr[styleIndex];

        }
    }
}

TR_INTERNAL FT_Face TRFontFileCreateFTFace(TRFontFileRef fontFile, TRUInteger faceIndex)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    FT_Face face = NULL;
    FT_Error error;

    MutexLock(&freetype->mutex);

    error = FT_Open_Face(freetype->library, &fontFile->_arguments, (FT_Long)faceIndex, &face);
    if (error == FT_Err_Ok) {
        if (!FT_IS_SCALABLE(face)) {
            FT_Done_Face(face);
            face = NULL;
        }
    }

    MutexUnlock(&freetype->mutex);

    return face;
}

TR_PUBLIC TRFontFileRef TRFontFileCreateFromPath(const char *path)
{
    FT_Open_Args arguments;
    TRUInteger length;
    char *pathCopy;

    if (!path) {
        return NULL;
    }

    length = strlen(path) + 1;
    pathCopy = malloc(length);
    if (!pathCopy) {
        return NULL;
    }
    memcpy(pathCopy, path, length);

    arguments.flags = FT_OPEN_PATHNAME;
    arguments.memory_base = NULL;
    arguments.memory_size = 0;
    arguments.pathname = pathCopy;
    arguments.stream = NULL;

    return CreateFontFileWithArguments(&arguments);
}

TR_PUBLIC TRFontFileRef TRFontFileCreateFromMemory(const void *memory, TRUInteger size)
{
    FT_Open_Args arguments;
    void *memoryCopy;

    if (!memory || size == 0) {
        return NULL;
    }

    memoryCopy = malloc(size);
    if (!memoryCopy) {
        return NULL;
    }
    memcpy(memoryCopy, memory, size);

    arguments.flags = FT_OPEN_MEMORY;
    arguments.memory_base = memoryCopy;
    arguments.memory_size = (FT_Long)size;
    arguments.pathname = NULL;
    arguments.stream = NULL;

    return CreateFontFileWithArguments(&arguments);
}

TR_PUBLIC TRFontFileRef TRFontFileRetain(TRFontFileRef fontFile)
{
    return ObjectRetain((ObjectRef)fontFile);
}

TR_PUBLIC void TRFontFileRelease(TRFontFileRef fontFile)
{
    ObjectRelease((ObjectRef)fontFile);
}
