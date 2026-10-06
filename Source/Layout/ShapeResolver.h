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

#ifndef _TEHREER_LAYOUT_SHAPE_RESOLVER_H
#define _TEHREER_LAYOUT_SHAPE_RESOLVER_H

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>

#include <API/TRBase.h>
#include <API/TRTypesetter.h>

/*
 * Finds the paragraphs and the runs of the text of a typesetter, whose text and buffer MUST be set.
 * Each paragraph is split into runs that have a single bidirectional level, script, typeface, size
 * and the other properties that decide the shape. The runs are then shaped.
 *
 * The typesetter receives the paragraphs and the runs, which it has to free if this fails. Returns
 * `TRFalse` if some text has no typeface, or on failure.
 */
TR_INTERNAL TRBoolean ShapeResolverResolve(TypesetterRef typesetter,
    const TRAttribute *defaultAttributes, TRUInteger defaultAttributeCount);

#endif
