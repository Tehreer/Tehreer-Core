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
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Mutex.h>
#include <Core/Object.h>
#include <Graphics/FreeType.h>

#include "FontData.h"

static TRUInteger GetFaceCountInData(const FT_Open_Args *args)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    FT_Face rawFace = NULL;
    FT_Long faceCount = 0;
    FT_Error error;

    MutexLock(&freetype->mutex);

    error = FT_Open_Face(freetype->library, args, -1, &rawFace);
    if (error == FT_Err_Ok) {
        faceCount = rawFace->num_faces;
        FT_Done_Face(rawFace);
    }

    MutexUnlock(&freetype->mutex);

    return faceCount;
}

static void FinalizeFontData(ObjectRef object)
{
    FontData *fontData = object;
    const FT_Open_Args *arguments = &fontData->_arguments;
    void *buffer = (void *)arguments->memory_base;
    void *pathname = arguments->pathname;

    if (buffer) {
        AllocatorDeallocateBlock(buffer);
    }
    if (pathname) {
        AllocatorDeallocateBlock(pathname);
    }
}

/* Takes over the memory that the arguments refer to, even if the data cannot be created. */
static FontDataRef CreateFontDataWithArguments(const FT_Open_Args *arguments)
{
    const TRUInteger size = sizeof(FontData);
    void *pointer = NULL;
    FontData *fontData;

    fontData = ObjectCreate(&size, 1, &pointer, FinalizeFontData);

    if (fontData) {
        fontData->_arguments = *arguments;
        fontData->faceCount = GetFaceCountInData(arguments);

        if (fontData->faceCount == 0) {
            ObjectRelease(fontData);
            fontData = NULL;
        }
    } else {
        AllocatorDeallocateBlock((void *)arguments->memory_base);
        AllocatorDeallocateBlock(arguments->pathname);
    }

    return fontData;
}

TR_INTERNAL FontDataRef FontDataCreateFromPath(const char *path)
{
    FontDataRef fontData = NULL;

    if (path) {
        TRUInteger length = strlen(path) + 1;
        char *pathCopy = AllocatorAllocateBlock(length);

        if (pathCopy) {
            FT_Open_Args arguments;

            memcpy(pathCopy, path, length);

            arguments.flags = FT_OPEN_PATHNAME;
            arguments.memory_base = NULL;
            arguments.memory_size = 0;
            arguments.pathname = pathCopy;
            arguments.stream = NULL;

            fontData = CreateFontDataWithArguments(&arguments);
        }
    }

    return fontData;
}

TR_INTERNAL FontDataRef FontDataCreateFromMemory(const void *memory, TRUInteger size)
{
    FontDataRef fontData = NULL;

    if (memory && size > 0) {
        void *memoryCopy = AllocatorAllocateBlock(size);

        if (memoryCopy) {
            FT_Open_Args arguments;

            memcpy(memoryCopy, memory, size);

            arguments.flags = FT_OPEN_MEMORY;
            arguments.memory_base = memoryCopy;
            arguments.memory_size = (FT_Long)size;
            arguments.pathname = NULL;
            arguments.stream = NULL;

            fontData = CreateFontDataWithArguments(&arguments);
        }
    }

    return fontData;
}

TR_INTERNAL FT_Face FontDataCreateFTFace(FontDataRef fontData, TRUInteger faceIndex)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
    FT_Face face = NULL;
    FT_Error error;

    MutexLock(&freetype->mutex);

    error = FT_Open_Face(freetype->library, &fontData->_arguments, (FT_Long)faceIndex, &face);
    if (error == FT_Err_Ok) {
        if (!FT_IS_SCALABLE(face) && !FT_HAS_FIXED_SIZES(face)) {
            FT_Done_Face(face);
            face = NULL;
        }
    }

    MutexUnlock(&freetype->mutex);

    return face;
}

TR_INTERNAL FontDataRef FontDataRetain(FontDataRef fontData)
{
    return ObjectRetain((ObjectRef)fontData);
}

TR_INTERNAL void FontDataRelease(FontDataRef fontData)
{
    ObjectRelease((ObjectRef)fontData);
}
