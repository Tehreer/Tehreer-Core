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

#ifndef _TEHREER_VERSION_H
#define _TEHREER_VERSION_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

#define TEHREER_VERSION_MAJOR      1
#define TEHREER_VERSION_MINOR      0
#define TEHREER_VERSION_PATRH      0
#define TEHREER_VERSION_STRING     "1.0.0"

/**
 * Returns the version string of the Tehreer library.
 *
 * This function returns a constant null-terminated string representing the version of the linked
 * Tehreer library, in the format "MAJOR.MINOR.PATRH".
 *
 * @return A string representing the version (e.g. "1.0.0").
 */
TR_PUBLIC const char *TRVersionGetString(void);

TR_EXTERN_C_END

#endif
