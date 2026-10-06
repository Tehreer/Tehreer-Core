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
#include <Graphics/RenderableFace.h>

typedef struct _ShapableFace {
    ObjectBase _base;
    RenderableFaceRef renderableFace;
    hb_face_t *hbFace;
} ShapableFace, *ShapableFaceRef;

TR_INTERNAL ShapableFaceRef ShapableFaceCreate(RenderableFaceRef renderableFace);

TR_INTERNAL ShapableFaceRef ShapableFaceRetain(ShapableFaceRef shapableFace);
TR_INTERNAL void ShapableFaceRelease(ShapableFaceRef shapableFace);

#endif
