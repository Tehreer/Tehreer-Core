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

#ifndef _TEHREER_API_PATH_H
#define _TEHREER_API_PATH_H

#include <ft2build.h>
#include FT_OUTLINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRPath.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _TRPath {
    ObjectBase _base;
    FT_Outline outline;
} TRPath;

/*
 * Creates a path that owns a FreeType copy of the outline. The coordinates of the outline MUST be
 * in 26.6 fixed point format, as FreeType produces them. Returns NULL on failure.
 */
TR_INTERNAL TRPathRef TRPathCreateFromOutline(const FT_Outline *outline);

#endif
