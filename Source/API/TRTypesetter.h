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
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Object.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

/* A paragraph of the text, which is found from the text when it is needed. */
typedef struct _ParagraphInfo {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 baseLevel;
} ParagraphInfo;

/*
 * The paragraphs are those of the text, which has them analyzed already, and the typesetter keeps
 * only the shaped runs, that are in the order of the code units and cover all of them.
 */
typedef struct _TRTypesetter {
    ObjectBase _base;
    TRTextRef text;
    TextBuffer buffer;
    Array runs;
    BreakClassifierRef breaks;
} TRTypesetter;

/* Gets the paragraph that has the code unit, which MUST be within the text. */
TR_INTERNAL void TRTypesetterGetParagraph(TRTypesetterRef typesetter, TRUInteger index,
    ParagraphInfo *paragraph);

TR_INTERNAL TRUInteger TRTypesetterGetRunCount(TRTypesetterRef typesetter);

/* Returns a run of the typesetter, whose index MUST be less than the count. */
TR_INTERNAL TextRunRef TRTypesetterGetRun(TRTypesetterRef typesetter, TRUInteger index);

/* Returns the index of the run that has the code unit, which MUST be within the text. */
TR_INTERNAL TRUInteger TRTypesetterFindRun(TRTypesetterRef typesetter, TRUInteger index);

/* Returns the extent of a range of code units, which MAY be empty, as the runs measure it. */
TR_INTERNAL TRFloat TRTypesetterMeasureRange(TRTypesetterRef typesetter, TRUInteger start,
    TRUInteger end);

#endif
