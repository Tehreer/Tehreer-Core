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

#ifndef _TEHREER_API_TYPESETTER_H
#define _TEHREER_API_TYPESETTER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Layout/ShapedParagraph.h>
#include <Layout/TextRun.h>
#include <Layout/TextSource.h>
#include <Text/BreakClassifier.h>

/* The text is kept by its source, which has the defaults of the shaping too. */
typedef struct _TRTypesetter {
    ObjectBase _base;
    TextSource source;
    TRBoolean isMutable;
    TRBoolean isEditing;
} TRTypesetter;

/*
 * Returns whether the typesetter is not being edited, and every paragraph that the range touches
 * is shaped, so lines can be made over it. The range MUST be within the text.
 */
TR_INTERNAL TRBoolean TRTypesetterIsShaped(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end);

#endif
