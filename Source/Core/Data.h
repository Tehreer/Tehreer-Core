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

#ifndef _TEHREER_CORE_DATA_H
#define _TEHREER_CORE_DATA_H

#include <API/TRBase.h>

typedef const TRUInt8 * Data;

#define Data_UInt8(data, offset)            (data)[offset]

#define Data_BigUInt16(data, offset)        \
(TRUInt16)                                  \
(                                           \
   ((TRUInt16)(data)[(offset)] << 8)        \
 | ((TRUInt16)(data)[(offset) + 1])         \
)

#define Data_BigUInt32(data, offset)        \
(TRUInt32)                                  \
(                                           \
   ((TRUInt32)(data)[(offset)] << 24)       \
 | ((TRUInt32)(data)[(offset) + 1] << 16)   \
 | ((TRUInt32)(data)[(offset) + 2] << 8)    \
 | ((TRUInt32)(data)[(offset) + 3])         \
)

#define Data_Int8(data, offset)             (TRInt8)Data_UInt8(data, offset)
#define Data_BigInt16(data, offset)         (TRInt16)Data_BigUInt16(data, offset)
#define Data_BigInt32(data, offset)         (TRInt32)Data_BigUInt32(data, offset)

#define Data_Subdata(data, offset)          (&(data)[offset])

#endif
