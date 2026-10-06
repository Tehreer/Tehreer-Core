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

#ifndef _TEHREER_STRING_H
#define _TEHREER_STRING_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

enum {
    TRStringEncodingUTF8  = 0,  /**< An 8-bit representation of Unicode code points. */
    TRStringEncodingUTF16 = 1,  /**< 16-bit UTF encoding in native endianness. */
    TRStringEncodingUTF32 = 2   /**< 32-bit UTF encoding in native endianness. */
};
typedef TRUInt32 TRStringEncoding;

typedef struct _TRStringView {
    const void *buffer;
    TRUInteger length;
    TRStringEncoding encoding;
} TRStringView;

TR_EXTERN_C_END

#endif
