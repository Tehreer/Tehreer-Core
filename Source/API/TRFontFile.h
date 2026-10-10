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

#include <Tehreer/TRFontFile.h>

#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Object.h>
#include <Font/FontData.h>

/*
 * A font file keeps the default typefaces that it made when it was created. The typefaces share the
 * data of the file, and not the file itself, so they and the file do not hold each other alive.
 */
typedef struct _TRFontFile {
    ObjectBase _base;
    FontDataRef _data;
    Array _typefaces;
} TRFontFile;

#endif
