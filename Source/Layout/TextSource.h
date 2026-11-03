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

#ifndef _TEHREER_LAYOUT_TEXT_SOURCE_H
#define _TEHREER_LAYOUT_TEXT_SOURCE_H

#include <SheenBidi/SBText.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRBase.h>

#include <API/TRBase.h>
#include <API/TRGlyphRun.h>
#include <Core/Array.h>
#include <Layout/ShapeResolver.h>
#include <Layout/ShapedParagraph.h>
#include <Layout/TextRun.h>
#include <Text/BreakClassifier.h>

/*
 * The text that the lines are made of. It is the only code that talks to SheenBidi, which keeps
 * the text, has the paragraphs of it analyzed, and keeps a `ShapedParagraph` as the user info of
 * each of them. It also keeps the defaults of the shaping, which are the context of the provider
 * of the paragraphs, and which a copy of the source shares. A typesetter has one, and edits it
 * through the functions that take a `TextSource` to modify, while the resolvers of the breaks and
 * the lines read it through a `TextSourceRef`, so they know nothing about the typesetter or its
 * editing.
 */
typedef struct _TextSource {
    SBMutableTextRef _sbText;
    ShapeDefaultsRef _defaults;
} TextSource;

typedef const TextSource *TextSourceRef;

/*
 * A paragraph of the text, which is found from the text when it is needed. It has no shaped
 * paragraph if the typesetter is being edited.
 */
typedef struct _ParagraphInfo {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 baseLevel;
    ShapedParagraphRef shaped;
} ParagraphInfo;

/*
 * A position among the runs of the text, which are in the order of the code units across the
 * paragraphs. The indexes of a text run are relative to its paragraph, so the cursor gives the run
 * in the indexes of the text too.
 */
typedef struct _RunCursor {
    ParagraphInfo paragraph;
    TRUInteger runIndex;
    TextRunRef textRun;
    TRUInteger start;
    TRUInteger end;
} RunCursor;

/* A part of a line in the order that it is shown, with the bidirectional level of its text. */
typedef struct _VisualRun {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 level;
} VisualRun;

/*
 * Initializes a source with a text, whose paragraphs are shaped with the default attributes where
 * the text sets none. Only the attributes that decide the shaping are looked at. A mutable source
 * can be edited, and the text of an immutable one is the string. Returns TRFalse on failure.
 */
TR_INTERNAL TRBoolean TextSourceInitialize(TextSource *source, const void *string,
    TRUInteger length, TRStringEncoding encoding, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount, TRBoolean isMutable);

/*
 * Initializes a source with a copy of another one, which MUST NOT be in an editing batch. The
 * copy shares the defaults of the original. Returns TRFalse on failure.
 */
TR_INTERNAL TRBoolean TextSourceInitializeCopy(TextSource *source, TextSourceRef original,
    TRBoolean isMutable);

/* Releases what an initialized source holds. */
TR_INTERNAL void TextSourceFinalize(TextSource *source);

/*
 * Returns the text of SheenBidi, which is the one that `TRTypesetterGetSheenBidiText()` hands out.
 * It is not retained for the caller, and stays valid as long as the source is alive.
 */
TR_INTERNAL SBTextRef TextSourceGetSheenBidiText(TextSourceRef source);

/* Returns the encoding of the code units of the text. */
TR_INTERNAL TRStringEncoding TextSourceGetEncoding(TextSourceRef source);

/* Returns the number of code units of the text. */
TR_INTERNAL TRUInteger TextSourceGetLength(TextSourceRef source);

/*
 * Copies the code units of a range, which MUST be within the text, to a buffer that is big
 * enough.
 */
TR_INTERNAL void TextSourceReadCodeUnits(TextSourceRef source, TRUInteger index, TRUInteger length,
    void *buffer);

/*
 * Copies the attributes of the code unit at the index, which MUST be within the text, and gets the
 * number of code units from it that have the same attributes. The list is the caller's to release.
 * Returns NULL if there are no attributes or on failure.
 */
TR_INTERNAL TRAttributeListRef TextSourceCopyAttributes(TextSourceRef source, TRUInteger index,
    TRUInteger *length);

/*
 * Signal the start and the end of a batch of edits, in which the paragraphs are analyzed and shaped
 * once, when it ends.
 */
TR_INTERNAL void TextSourceBeginEditing(TextSource *source);
TR_INTERNAL void TextSourceEndEditing(TextSource *source);

/*
 * Edit the code units of a mutable text. An index and a range MUST be within the text, and the
 * buffer MUST NOT be NULL if the count is not zero.
 */
TR_INTERNAL void TextSourceAppendCodeUnits(TextSource *source, const void *buffer,
    TRUInteger count);
TR_INTERNAL void TextSourceInsertCodeUnits(TextSource *source, TRUInteger index,
    const void *buffer, TRUInteger count);
TR_INTERNAL void TextSourceDeleteCodeUnits(TextSource *source, TRUInteger index,
    TRUInteger length);
TR_INTERNAL void TextSourceSetCodeUnits(TextSource *source, const void *buffer, TRUInteger count);
TR_INTERNAL void TextSourceReplaceCodeUnits(TextSource *source, TRUInteger index,
    TRUInteger length, const void *buffer, TRUInteger count);

/*
 * Set or remove an attribute on a range of a mutable text, which MUST be within the text. Return
 * TRFalse, and change nothing, if the type of the attribute is not known.
 */
TR_INTERNAL TRBoolean TextSourceSetAttribute(TextSource *source, TRUInteger index,
    TRUInteger length, const TRAttribute *attribute);
TR_INTERNAL TRBoolean TextSourceRemoveAttribute(TextSource *source, TRUInteger index,
    TRUInteger length, TRAttributeType type);

/* Gets the paragraph that has the code unit, which MUST be within the text. */
TR_INTERNAL void TextSourceGetParagraph(TextSourceRef source, TRUInteger index,
    ParagraphInfo *paragraph);

/*
 * Returns whether every paragraph that the range touches is shaped, so lines can be made over it.
 * The range MUST be within the text.
 */
TR_INTERNAL TRBoolean TextSourceIsShaped(TextSourceRef source, TRUInteger start, TRUInteger end);

/* Finds the run that has the code unit, which MUST be within a shaped paragraph. */
TR_INTERNAL TRBoolean TextSourceFindRun(TextSourceRef source, TRUInteger index,
    RunCursor *cursor);

/* Moves the cursor to the run that follows or precedes it, if there is one. */
TR_INTERNAL TRBoolean TextSourceMoveToNextRun(TextSourceRef source, RunCursor *cursor);
TR_INTERNAL TRBoolean TextSourceMoveToPreviousRun(TextSourceRef source, RunCursor *cursor);

/* Returns the extent of a range of code units, which MAY be empty, as the runs measure it. */
TR_INTERNAL TRFloat TextSourceMeasureRange(TextSourceRef source, TRUInteger start,
    TRUInteger end);

/*
 * Finds the first break of the given type after `from`, but not after `to`, or the last break
 * before `from`, but not before `to`, as `BreakClassifierGetForwardBreak()` and
 * `BreakClassifierGetBackwardBreak()` do. The search stops at the end or the start of the
 * paragraph that it begins in, since the ends of a paragraph are breaks of every type.
 */
TR_INTERNAL TRUInteger TextSourceGetForwardBreak(TextSourceRef source, BreakType type,
    TRUInteger from, TRUInteger to);
TR_INTERNAL TRUInteger TextSourceGetBackwardBreak(TextSourceRef source, BreakType type,
    TRUInteger from, TRUInteger to);

/* Return the code unit or the code point at the index, which MUST be within a shaped paragraph. */
TR_INTERNAL TRUInt32 TextSourceGetCodeUnit(TextSourceRef source, TRUInteger index);
TR_INTERNAL TRUInt32 TextSourceGetCodePoint(TextSourceRef source, TRUInteger index);

/*
 * Find the whitespace in a range of shaped paragraphs, as the functions of `TextBuffer` do for a
 * buffer of one paragraph.
 */
TR_INTERNAL TRUInteger TextSourceGetLeadingWhitespaceEnd(TextSourceRef source, TRUInteger start,
    TRUInteger end);
TR_INTERNAL TRUInteger TextSourceGetTrailingWhitespaceStart(TextSourceRef source,
    TRUInteger start, TRUInteger end);
TR_INTERNAL TRUInteger TextSourceFindWhitespace(TextSourceRef source, TRUInteger start,
    TRUInteger end);

/*
 * Appends the visual runs of a range that is inside a paragraph to the array of `VisualRun`, in the
 * order that they are shown. The range MUST NOT be empty. Returns TRFalse if the runs could not be
 * found.
 */
TR_INTERNAL TRBoolean TextSourceAddVisualRuns(TextSourceRef source, TRUInteger start,
    TRUInteger end, Array *visualRuns);

/*
 * Gets what the text is painted with from the attributes at an index, and returns the number of
 * code units from the index that have the same attributes.
 */
TR_INTERNAL TRUInteger TextSourceGetPaint(TextSourceRef source, TRUInteger index,
    GlyphRunPaint *paint);

#endif
