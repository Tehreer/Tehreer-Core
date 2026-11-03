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

#ifndef _TEHREER_LAYOUT_SHAPED_PARAGRAPH_H
#define _TEHREER_LAYOUT_SHAPED_PARAGRAPH_H

#include <SheenBidi/SBTextConfig.h>

#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Object.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

/*
 * What a typesetter knows of a paragraph, which SheenBidi keeps as the user info of the paragraph.
 * It is made when an edit creates or changes the paragraph, shared by the copies of the text, and
 * never changes after that, so it can be used from multiple threads.
 *
 * Everything in it is relative to the start of the paragraph, since an edit before the paragraph
 * moves it without changing it. The runs are in the order of the code units and cover all of them
 * if the paragraph is shaped.
 */
typedef struct _ShapedParagraph {
    ObjectBase _base;
    TextBuffer buffer;
    BreakClassifierRef breaks;
    Array runs;

    /* Tells that every part of the paragraph has a typeface, so it has runs to make lines from. */
    TRBoolean isShaped;

    /*
     * Tells that the paragraph was made by the provider, and SheenBidi has not retained it yet. The
     * first retain takes over the reference of the creation, and the later ones, which come from
     * the copies of the text, add a reference.
     */
    TRBoolean isUnclaimed;
} ShapedParagraph, *ShapedParagraphRef;

/*
 * Creates a paragraph with a copy of the code units, whose breaks are classified and which has no
 * runs yet. Returns `NULL` on failure.
 */
TR_INTERNAL ShapedParagraphRef ShapedParagraphCreate(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding);

/* Returns the run of the paragraph at the index, which MUST be less than the count. */
TR_INTERNAL TextRunRef ShapedParagraphGetRun(ShapedParagraphRef paragraph, TRUInteger index);

/*
 * Returns the index of the run that has the code unit, which is relative to the start of the
 * paragraph, or the invalid index if there is none.
 */
TR_INTERNAL TRUInteger ShapedParagraphFindRun(ShapedParagraphRef paragraph, TRUInteger index);

/* The callbacks that let SheenBidi keep a paragraph as the user info of a paragraph of the text. */
TR_INTERNAL const SBParagraphUserInfoCallbacks *ShapedParagraphGetUserInfoCallbacks(void);

TR_INTERNAL ShapedParagraphRef ShapedParagraphRetain(ShapedParagraphRef paragraph);
TR_INTERNAL void ShapedParagraphRelease(ShapedParagraphRef paragraph);

#endif
