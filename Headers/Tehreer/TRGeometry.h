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

#ifndef _TEHREER_GEOMETRY_H
#define _TEHREER_GEOMETRY_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * A point in a two-dimensional coordinate system. The y axis grows downward, as in text layout.
 */
typedef struct _TRPoint {
    TRFloat x;
    TRFloat y;
} TRPoint;

/**
 * A width and a height.
 */
typedef struct _TRSize {
    TRFloat width;
    TRFloat height;
} TRSize;

/**
 * A rectangle defined by its top-left corner and its size.
 */
typedef struct _TRRect {
    TRPoint origin;
    TRSize size;
} TRRect;

/**
 * A two-dimensional affine transform. A point (x, y) is mapped to
 * (a * x + c * y + tx, b * x + d * y + ty).
 */
typedef struct _TRAffineTransform {
    TRFloat a;
    TRFloat b;
    TRFloat c;
    TRFloat d;
    TRFloat tx;
    TRFloat ty;
} TRAffineTransform;

TR_EXTERN_C_END

#endif
