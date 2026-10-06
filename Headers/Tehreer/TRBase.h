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

#ifndef _TEHREER_BASE_H
#define _TEHREER_BASE_H

#include <stdint.h>

#ifdef __cplusplus
#define TR_EXTERN_C_BEGIN extern "C" {
#define TR_EXTERN_C_END }
#else
#define TR_EXTERN_C_BEGIN
#define TR_EXTERN_C_END
#endif

#ifdef _WIN32

#if defined(TR_CONFIG_DLL_EXPORT)
#define TR_PUBLIC extern __declspec(dllexport)
#elif defined(TR_CONFIG_DLL_IMPORT)
#define TR_PUBLIC extern __declspec(dllimport)
#endif

#endif

#ifndef TR_PUBLIC
#define TR_PUBLIC extern
#endif

TR_EXTERN_C_BEGIN

/**
 * A type to represent an 8-bit signed integer.
 */
typedef int8_t                      TRInt8;

/**
 * A type to represent a 16-bit signed integer.
 */
typedef int16_t                     TRInt16;

/**
 * A type to represent a 32-bit signed integer.
 */
typedef int32_t                     TRInt32;

/**
 * A type to represent an 8-bit unsigned integer.
 */
typedef uint8_t                     TRUInt8;

/**
 * A type to represent a 16-bit unsigned integer.
 */
typedef uint16_t                    TRUInt16;

/**
 * A type to represent a 32-bit unsigned integer.
 */
typedef uint32_t                    TRUInt32;

/**
 * A signed integer type whose width is equal to the width of the machine word.
 */
typedef intptr_t                    TRInteger;

/**
 * An unsigned integer type whose width is equal to the width of the machine word.
 */
typedef uintptr_t                   TRUInteger;

/**
 * A type to represent a single-precision floating-point number.
 * Conforms to IEEE 754 standard.
 */
typedef float                       TRFloat;

/**
 * Constants that specify the states of a boolean.
 */
enum {
    TRFalse = 0, /**< A value representing the false state. */
    TRTrue  = 1  /**< A value representing the true state. */
};
/**
 * A type to represent a boolean value.
 */
typedef TRUInt8                     TRBoolean;

/**
 * A type to represent a tag of 4 characters.
 */
typedef TRUInt32                    TRTag;

typedef TRUInt16                    TRGlyphID;

typedef TRUInt32                    TRColor;

#define TRColorMake(a, r, g, b)     \
(                                   \
    ((TRUInt32)(a) << 24)           \
  | ((TRUInt32)(r) << 16)           \
  | ((TRUInt32)(g) << 8)            \
  | ((TRUInt32)(b))                 \
)

TR_EXTERN_C_END

#endif
