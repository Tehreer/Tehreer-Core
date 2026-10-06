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

#ifndef _TEHREER_PATH_H
#define _TEHREER_PATH_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>

TR_EXTERN_C_BEGIN

/**
 * An immutable outline made of contours of lines and curves, such as the shape of a glyph.
 * Platform wrappers translate it into their native path types by enumerating it.
 */
typedef const struct _TRPath *TRPathRef;

/**
 * Callbacks that receive the elements of a path. Any of them can be `NULL` to ignore that kind of
 * element.
 */
typedef struct _TRPathCallbacks {
    /** Starts a new contour at the given point. */
    void (*moveTo)(void *userData, TRFloat x, TRFloat y);

    /** Adds a straight line from the current point. */
    void (*lineTo)(void *userData, TRFloat x, TRFloat y);

    /** Adds a quadratic curve from the current point. */
    void (*quadTo)(void *userData, TRFloat controlX, TRFloat controlY, TRFloat x, TRFloat y);

    /** Adds a cubic curve from the current point. */
    void (*cubicTo)(void *userData, TRFloat control1X, TRFloat control1Y,
        TRFloat control2X, TRFloat control2Y, TRFloat x, TRFloat y);

    /** Closes the current contour. */
    void (*close)(void *userData);
} TRPathCallbacks;

/**
 * Passes each element of the path to the callbacks, in order. Every contour starts with a move and
 * ends with a close. The y axis of the coordinates points downward.
 *
 * @param path
 *      The path to enumerate.
 * @param transform
 *      Optional transform that is applied to every point, or `NULL`.
 * @param callbacks
 *      The callbacks that receive the elements.
 * @param userData
 *      An opaque pointer that is passed to every callback.
 */
TR_PUBLIC void TRPathEnumerate(TRPathRef path, const TRAffineTransform *transform,
    const TRPathCallbacks *callbacks, void *userData);

/**
 * Retains the path.
 */
TR_PUBLIC TRPathRef TRPathRetain(TRPathRef path);

/**
 * Releases the path.
 */
TR_PUBLIC void TRPathRelease(TRPathRef path);

TR_EXTERN_C_END

#endif
