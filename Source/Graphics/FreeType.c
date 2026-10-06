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

#include <ft2build.h>
#include FT_FREETYPE_H

#include <API/TRBase.h>
#include <Core/Mutex.h>
#include <Core/Once.h>

#include "FreeType.h"

static FreeType DefaultFreeType;

static void InitFreeType(void)
{
    MutexInit(&DefaultFreeType.mutex);
    FT_Init_FreeType(&DefaultFreeType.library);
}

TR_INTERNAL FreeTypeRef FreeTypeGetDefault(void)
{
    static Once once = OnceMake();
    OnceExecute(&once, InitFreeType);

    return &DefaultFreeType;
}
