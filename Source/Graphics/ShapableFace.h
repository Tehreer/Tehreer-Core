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

#ifndef _TEHREER_SHAPABLE_FACE_H
#define _TEHREER_SHAPABLE_FACE_H

#include <hb.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Graphics/AdvanceCache.h>
#include <Graphics/RenderableFace.h>

/**
 * Provides a HarfBuzz font whose glyph lookups and advances come from the FreeType faces of a
 * `RenderableFace`. A root shapable face uses the default variation coordinates, and a derived one
 * uses its own coordinates and advance cache while sharing the HarfBuzz face of its root. The
 * coordinates are FreeType design coordinates in 16.16 format, one for each variation axis.
 */
typedef struct _ShapableFace {
    ObjectBase _base;
    RenderableFaceRef renderableFace;
    struct _ShapableFace *rootFace;
    hb_face_t *hbFace;
    hb_font_t *hbFont;
    FT_Fixed *coordinates;
    TRUInteger coordinateCount;
    AdvanceCache advanceCache;
} ShapableFace, *ShapableFaceRef;

TR_INTERNAL ShapableFaceRef ShapableFaceCreate(RenderableFaceRef renderableFace);

/*
 * Creates a shapable face for the given design coordinates, one for each variation axis. The new
 * face shares the HarfBuzz face and the renderable face of the parent's root.
 */
TR_INTERNAL ShapableFaceRef ShapableFaceCreateDerived(ShapableFaceRef parent,
    const TRFloat *coordinates, TRUInteger coordinateCount);

TR_INTERNAL ShapableFaceRef ShapableFaceRetain(ShapableFaceRef shapableFace);
TR_INTERNAL void ShapableFaceRelease(ShapableFaceRef shapableFace);

#endif
