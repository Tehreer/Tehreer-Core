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
#include <Core/Object.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

typedef struct _ParagraphInfo {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 baseLevel;
} ParagraphInfo;

typedef struct _TRTypesetter {
    ObjectBase _base;
    TRTextRef text;
    TextBuffer buffer;
    ParagraphInfo *paragraphs;
    TRUInteger paragraphCount;
    TextRunRef *runs;
    TRUInteger runCount;
    TextRunRef *blocks;
    TRUInteger blockCount;
    BreakClassifierRef breaks;
} TRTypesetter, *TypesetterRef;

/* Returns the index of the paragraph that has the code unit, which MUST be within the text. */
TR_INTERNAL TRUInteger TypesetterFindParagraph(TypesetterRef typesetter, TRUInteger index);

/* Returns the index of the run that has the code unit, which MUST be within the text. */
TR_INTERNAL TRUInteger TypesetterFindRun(TypesetterRef typesetter, TRUInteger index);

/* Returns the extent of a range of code units, which MAY be empty, as the runs measure it. */
TR_INTERNAL TRFloat TypesetterMeasureRange(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end);

#endif
