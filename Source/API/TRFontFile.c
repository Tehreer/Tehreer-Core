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

#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <API/TRTypeface.h>
#include <Core/Array.h>
#include <Core/Object.h>
#include <Font/FaceMetadata.h>
#include <Font/FontData.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>

#include "TRFontFile.h"

static void FinalizeFontFile(ObjectRef object)
{
    TRFontFile *fontFile = object;
    TRUInteger count = ArrayGetCount(&fontFile->_typefaces);
    TRUInteger index;

    for (index = 0; index < count; index++) {
        TRTypefaceRelease(*(TRTypefaceRef *)ArrayGetItem(&fontFile->_typefaces, index));
    }

    ArrayFinalize(&fontFile->_typefaces);
    FontDataRelease(fontFile->_data);
}

/* Creates the typeface of a face, which has the default coordinates and colors of the font. */
static TRTypefaceRef CreateFaceTypeface(FontDataRef fontData, TRUInteger faceIndex)
{
    RenderableFaceRef renderableFace = RenderableFaceCreate(fontData, faceIndex);
    TRTypefaceRef typeface = NULL;

    if (renderableFace) {
        ShapableFaceRef shapableFace = ShapableFaceCreate(renderableFace);

        if (shapableFace) {
            typeface = TRTypefaceCreateDefault(renderableFace, shapableFace, NULL);

            ShapableFaceRelease(shapableFace);
        }

        RenderableFaceRelease(renderableFace);
    }

    return typeface;
}

/* Keeps a typeface in the font file, which takes over the reference that the caller has. */
static TRBoolean AppendTypeface(TRFontFile *fontFile, TRTypefaceRef typeface)
{
    TRBoolean isAppended = (typeface && ArrayAppend(&fontFile->_typefaces, &typeface));

    if (!isAppended && typeface) {
        TRTypefaceRelease(typeface);
    }

    return isAppended;
}

/*
 * Adds the default typefaces of a face: one for each of its named styles, or the face itself if it
 * has none. A face that cannot be loaded adds none, and it does not fail the other faces.
 */
static TRBoolean AddFaceTypefaces(TRFontFile *fontFile, TRUInteger faceIndex)
{
    TRTypefaceRef faceTypeface = CreateFaceTypeface(fontFile->_data, faceIndex);
    TRBoolean isAdded = TRTrue;

    if (faceTypeface) {
        TRUInteger styleCount = TRTypefaceGetNamedStyleCount(faceTypeface);

        if (styleCount == 0) {
            isAdded = AppendTypeface(fontFile, TRTypefaceRetain(faceTypeface));
        } else {
            const TRNamedStyle *styles = TRTypefaceGetNamedStylesPtr(faceTypeface);
            TRUInteger styleIndex;

            for (styleIndex = 0; isAdded && styleIndex < styleCount; styleIndex++) {
                const TRNamedStyle *style = &styles[styleIndex];

                isAdded = AppendTypeface(fontFile, TRTypefaceCreateWithVariation(faceTypeface,
                    style->coordinatesPtr, style->coordinateCount));
            }
        }

        TRTypefaceRelease(faceTypeface);
    }

    return isAdded;
}

/* Takes over the data, even if the font file cannot be created. */
static TRFontFileRef CreateFontFileWithData(FontDataRef fontData)
{
    TRFontFile *fontFile = NULL;

    if (fontData) {
        const TRUInteger size = sizeof(TRFontFile);
        void *pointer = NULL;

        fontFile = ObjectCreate(&size, 1, &pointer, FinalizeFontFile);

        if (fontFile) {
            TRBoolean isLoaded = TRTrue;
            TRUInteger faceIndex;

            fontFile->_data = fontData;
            ArrayInitialize(&fontFile->_typefaces, sizeof(TRTypefaceRef));

            for (faceIndex = 0; isLoaded && faceIndex < fontData->faceCount; faceIndex++) {
                isLoaded = AddFaceTypefaces(fontFile, faceIndex);
            }

            /* A font file with no typeface cannot be used, so it is not made. */
            if (!isLoaded || ArrayGetCount(&fontFile->_typefaces) == 0) {
                ObjectRelease(fontFile);
                fontFile = NULL;
            }
        } else {
            FontDataRelease(fontData);
        }
    }

    return fontFile;
}

TRFontFileRef TRFontFileCreateFromPath(const char *path)
{
    return CreateFontFileWithData(FontDataCreateFromPath(path));
}

TRFontFileRef TRFontFileCreateFromMemory(const void *memory, TRUInteger size)
{
    return CreateFontFileWithData(FontDataCreateFromMemory(memory, size));
}

TRUInteger TRFontFileGetTypefaceCount(TRFontFileRef fontFile)
{
    return ArrayGetCount(&fontFile->_typefaces);
}

TRTypefaceRef TRFontFileGetTypeface(TRFontFileRef fontFile, TRUInteger index)
{
    TRTypefaceRef typeface = NULL;

    if (IndexIsValid(index, ArrayGetCount(&fontFile->_typefaces))) {
        typeface = *(TRTypefaceRef *)ArrayGetItem(&fontFile->_typefaces, index);
    }

    return typeface;
}

TRFontFileRef TRFontFileRetain(TRFontFileRef fontFile)
{
    return ObjectRetain((ObjectRef)fontFile);
}

void TRFontFileRelease(TRFontFileRef fontFile)
{
    ObjectRelease((ObjectRef)fontFile);
}
