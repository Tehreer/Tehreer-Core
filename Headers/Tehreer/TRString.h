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

/**
 * The encoding of the code units of a string.
 */
enum {
    TRStringEncodingUTF8  = 0,  /**< An 8-bit representation of Unicode code points. */
    TRStringEncodingUTF16 = 1,  /**< 16-bit UTF encoding in native endianness. */
    TRStringEncodingUTF32 = 2   /**< 32-bit UTF encoding in native endianness. */
};
/**
 * A type to represent the encoding of the code units of a string, which is one of the
 * `TRStringEncoding*` constants above.
 */
typedef TRUInt32 TRStringEncoding;

/**
 * A view of a string that someone else owns. It holds a pointer to the code units, their count and
 * their encoding, but it does not own or copy them, so the buffer must outlive the view.
 */
typedef struct _TRStringView {
    const void *buffer;             /**< The code units of the string, in the given encoding. */
    TRUInteger length;              /**< The number of code units, not of bytes or characters. */
    TRStringEncoding encoding;      /**< The encoding of the code units. */
} TRStringView;

TR_EXTERN_C_END

#endif
