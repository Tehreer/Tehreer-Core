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

#ifndef _TEHREER_TEXT_BREAK_CLASSIFIER_H
#define _TEHREER_TEXT_BREAK_CLASSIFIER_H

#include <Tehreer/TRString.h>

#include <API/TRBase.h>
#include <Core/Object.h>

/**
 * The kinds of breaks that a classifier knows.
 */
enum {
    BreakTypeLine = 1 << 0,         /* A line may be broken here. */
    BreakTypeGrapheme = 1 << 1      /* A grapheme cluster boundary. */
};
typedef TRUInt8 BreakType;

/**
 * Classifies the boundaries of a text into line breaks and grapheme cluster boundaries, according
 * to Unicode Standard Annex #14 and #29. It is immutable once created, so it can be shared between
 * threads.
 *
 * A break is identified by the index of the code unit that follows it, so a break at index `i`
 * lies between the code units `i - 1` and `i`. The start of the text is never a break, and its end
 * is one only if the rules say so, for example when the text ends with a line separator. The
 * searches below stop at the end of their range anyway.
 */
typedef struct _BreakClassifier {
    ObjectBase _base;
    TRUInteger length;
    TRUInt8 *breaks;
} BreakClassifier, *BreakClassifierRef;

/**
 * Creates a classifier for the code units of a text. The code units are not retained.
 *
 * @return
 *      New classifier, or `NULL` on failure.
 */
TR_INTERNAL BreakClassifierRef BreakClassifierCreate(const void *codeUnits, TRUInteger length,
    TRStringEncoding encoding);

/**
 * Returns whether there is a break of the given type before the code unit at the given index. The
 * index MUST NOT be greater than the length of the text.
 */
TR_INTERNAL TRBoolean BreakClassifierHasBreak(BreakClassifierRef classifier, BreakType type,
    TRUInteger index);

/**
 * Finds the first break of the given type after `from`, but not after `to`. If there is none, `to`
 * is returned. `from` MUST be less than `to`, and `to` MUST NOT be greater than the length.
 */
TR_INTERNAL TRUInteger BreakClassifierGetForwardBreak(BreakClassifierRef classifier,
    BreakType type, TRUInteger from, TRUInteger to);

/**
 * Finds the last break of the given type before `from`, but not before `to`. If there is none,
 * `to` is returned. `from` MUST be greater than `to`, and `from` MUST NOT be greater than the
 * length.
 */
TR_INTERNAL TRUInteger BreakClassifierGetBackwardBreak(BreakClassifierRef classifier,
    BreakType type, TRUInteger from, TRUInteger to);

TR_INTERNAL BreakClassifierRef BreakClassifierRetain(BreakClassifierRef classifier);
TR_INTERNAL void BreakClassifierRelease(BreakClassifierRef classifier);

#endif
