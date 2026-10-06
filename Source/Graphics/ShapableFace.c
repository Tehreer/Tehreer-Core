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

#include <stdlib.h>

#include <hb.h>

#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Object.h>
#include <Graphics/RenderableFace.h>

#include "ShapableFace.h"

static hb_blob_t *GetFontTableData(hb_face_t *face, hb_tag_t tag, void *object)
{
    ShapableFaceRef shapableFace = object;
    RenderableFaceRef renderableFace = shapableFace->renderableFace;
    void *data;
    TRUInteger length;

    RenderableFaceCopyTable(renderableFace, tag, &data, &length);

    return hb_blob_create(data, length, HB_MEMORY_MODE_WRITABLE, data, AllocatorDeallocateBlock);
}

static void FinalizeShapableFace(ObjectRef object)
{
    ShapableFaceRef shapableFace = object;

    hb_face_destroy(shapableFace->hbFace);
    RenderableFaceRelease(shapableFace->renderableFace);
}

TR_INTERNAL ShapableFaceRef ShapableFaceCreate(RenderableFaceRef renderableFace)
{
    const TRUInteger size = sizeof(ShapableFace);
    void *pointer = NULL;
    ShapableFace *shapableFace;

    shapableFace = ObjectCreate(&size, 1, &pointer, FinalizeShapableFace);

    if (shapableFace) {
        shapableFace->renderableFace = RenderableFaceRetain(renderableFace);
        shapableFace->hbFace = hb_face_create_for_tables(GetFontTableData, shapableFace, NULL);
    }

    return shapableFace;
}

TR_INTERNAL ShapableFaceRef ShapableFaceRetain(ShapableFaceRef shapableFace)
{
    return ObjectRetain((ObjectRef)shapableFace);
}

TR_INTERNAL void ShapableFaceRelease(ShapableFaceRef shapableFace)
{
    ObjectRelease((ObjectRef)shapableFace);
}
