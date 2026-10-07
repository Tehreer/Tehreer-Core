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

#include <stddef.h>

#include <ft2build.h>
#include FT_OUTLINE_H

#include <Tehreer/TRGeometry.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Graphics/FreeType.h>

#include "TRPath.h"

static void FinalizePath(ObjectRef object)
{
    TRPath *path = object;
    FreeTypeRef freetype = FreeTypeGetDefault();

    /* The outline functions only use the memory allocator of the library, so no lock is needed. */
    FT_Outline_Done(freetype->library, &path->outline);
}

typedef struct _Enumeration {
    const TRPathCallbacks *callbacks;
    void *userData;
    const TRAffineTransform *transform;
    TRBoolean isOpen;
} Enumeration;

static void MapPoint(const Enumeration *enumeration, const FT_Vector *vector,
    TRFloat *outX, TRFloat *outY)
{
    /* FreeType's y axis points upward while the path's one points downward. */
    TRFloat x = (TRFloat)vector->x / 64.0f;
    TRFloat y = (TRFloat)(-vector->y) / 64.0f;
    const TRAffineTransform *t = enumeration->transform;

    if (t) {
        *outX = (t->a * x) + (t->c * y) + t->tx;
        *outY = (t->b * x) + (t->d * y) + t->ty;
    } else {
        *outX = x;
        *outY = y;
    }
}

static void CloseContour(Enumeration *enumeration)
{
    if (enumeration->isOpen) {
        if (enumeration->callbacks->close) {
            enumeration->callbacks->close(enumeration->userData);
        }

        enumeration->isOpen = TRFalse;
    }
}

static int EnumerateMoveTo(const FT_Vector *to, void *user)
{
    Enumeration *enumeration = user;
    TRFloat x, y;

    CloseContour(enumeration);

    if (enumeration->callbacks->moveTo) {
        MapPoint(enumeration, to, &x, &y);
        enumeration->callbacks->moveTo(enumeration->userData, x, y);
    }

    enumeration->isOpen = TRTrue;

    return 0;
}

static int EnumerateLineTo(const FT_Vector *to, void *user)
{
    Enumeration *enumeration = user;
    TRFloat x, y;

    if (enumeration->callbacks->lineTo) {
        MapPoint(enumeration, to, &x, &y);
        enumeration->callbacks->lineTo(enumeration->userData, x, y);
    }

    return 0;
}

static int EnumerateConicTo(const FT_Vector *control, const FT_Vector *to, void *user)
{
    Enumeration *enumeration = user;
    TRFloat controlX, controlY, x, y;

    if (enumeration->callbacks->quadTo) {
        MapPoint(enumeration, control, &controlX, &controlY);
        MapPoint(enumeration, to, &x, &y);
        enumeration->callbacks->quadTo(enumeration->userData, controlX, controlY, x, y);
    }

    return 0;
}

static int EnumerateCubicTo(const FT_Vector *control1, const FT_Vector *control2,
    const FT_Vector *to, void *user)
{
    Enumeration *enumeration = user;
    TRFloat control1X, control1Y, control2X, control2Y, x, y;

    if (enumeration->callbacks->cubicTo) {
        MapPoint(enumeration, control1, &control1X, &control1Y);
        MapPoint(enumeration, control2, &control2X, &control2Y);
        MapPoint(enumeration, to, &x, &y);
        enumeration->callbacks->cubicTo(enumeration->userData, control1X, control1Y,
            control2X, control2Y, x, y);
    }

    return 0;
}

TR_INTERNAL TRPathRef TRPathCreateFromOutline(const FT_Outline *outline)
{
    const TRUInteger size = sizeof(TRPath);
    void *pointer = NULL;
    TRPath *path;

    path = ObjectCreate(&size, 1, &pointer, FinalizePath);

    if (path) {
        FreeTypeRef freetype = FreeTypeGetDefault();
        FT_Error error;

        /* The outline functions only use the memory allocator of the library, so no lock. */
        error = FT_Outline_New(freetype->library, outline->n_points, outline->n_contours,
            &path->outline);
        if (error == FT_Err_Ok) {
            error = FT_Outline_Copy(outline, &path->outline);
        }

        if (error != FT_Err_Ok) {
            ObjectRelease(path);
            path = NULL;
        }
    }

    return path;
}

void TRPathEnumerate(TRPathRef path, const TRAffineTransform *transform,
    const TRPathCallbacks *callbacks, void *userData)
{
    if (callbacks && path->outline.n_contours > 0) {
        FT_Outline_Funcs funcs;
        Enumeration enumeration;

        funcs.move_to = EnumerateMoveTo;
        funcs.line_to = EnumerateLineTo;
        funcs.conic_to = EnumerateConicTo;
        funcs.cubic_to = EnumerateCubicTo;
        funcs.shift = 0;
        funcs.delta = 0;

        enumeration.callbacks = callbacks;
        enumeration.userData = userData;
        enumeration.transform = transform;
        enumeration.isOpen = TRFalse;

        /* The outline is only read, so dropping the const qualifier is safe. */
        FT_Outline_Decompose((FT_Outline *)&path->outline, &funcs, &enumeration);

        CloseContour(&enumeration);
    }
}

TRPathRef TRPathRetain(TRPathRef path)
{
    return ObjectRetain((ObjectRef)path);
}

void TRPathRelease(TRPathRef path)
{
    ObjectRelease((ObjectRef)path);
}
