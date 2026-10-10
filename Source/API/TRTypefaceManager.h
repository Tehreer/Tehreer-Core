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

#ifndef _TEHREER_API_TYPEFACE_MANAGER_H
#define _TEHREER_API_TYPEFACE_MANAGER_H

#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Mutex.h>

/* A registered typeface. The manager owns one reference of the typeface. */
typedef struct _TypefaceEntry {
    TRTypefaceRef typeface;
    TRUInteger tag;
    TRUInteger familyID;
} TypefaceEntry;

/* The entries are kept in the order of their registration. The mutex guards everything below it. */
typedef struct _TRTypefaceManager {
    Mutex mutex;
    Array entries;
} TRTypefaceManager;

#endif
