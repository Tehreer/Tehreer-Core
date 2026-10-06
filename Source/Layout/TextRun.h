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

#ifndef _TEHREER_LAYOUT_TEXT_RUN_H
#define _TEHREER_LAYOUT_TEXT_RUN_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Object.h>

enum {
    TextRunKindIntrinsic,   /* Glyphs that a shaping engine produced. */
    TextRunKindReplacement  /* A replacement, which has a single glyph that is as wide as its room. */
};
typedef TRUInt32 TextRunKind;

/*
 * A run is a range of text that has been shaped as a whole, with a single typeface and size. It
 * never changes once it is created, so it can be shared by lines and threads.
 *
 * The cluster map has an entry for each code unit of the run, with the index of its glyph, and the
 * caret edges have one more than that. A replacement run has one glyph, so its cluster map is zero.
 */
typedef struct _TextRun {
    ObjectBase _base;
    TextRunKind kind;
    TRUInteger codeUnitStart;
    TRUInteger codeUnitEnd;
    TRBoolean isBackward;
    TRUInt8 bidiLevel;
    TRWritingDirection writingDirection;
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat scaleX;
    TRFloat scaleY;
    TRFloat ascent;
    TRFloat descent;
    TRFloat leading;
    TRUInteger glyphCount;
    TRGlyphID *glyphIDs;
    TRPoint *glyphOffsets;
    TRFloat *glyphAdvances;
    TRUInteger *clusterMap;
    TRFloat *caretEdges;

    /* The members below are only set for a replacement run. */
    TRReplacementRef replacement;
    TRFloat extent;
} TextRun, *TextRunRef;

/*
 * Creates a run for the text that a shaping engine shaped. The horizontal offsets, the advances and
 * the caret edges are scaled horizontally, and the vertical offsets vertically, and the baseline
 * offset is added to the vertical offsets. The ascent,
 * descent and leading are those of the typeface at the size, scaled vertically as well.
 */
TR_INTERNAL TextRunRef TextRunCreateIntrinsic(TRUInteger codeUnitStart, TRUInteger codeUnitEnd,
    TRUInt8 bidiLevel, TRTypefaceRef typeface, TRFloat typeSize, TRFloat scaleX, TRFloat scaleY,
    TRFloat baselineOffset, TRShapingResultRef shapingResult, TRWritingDirection writingDirection);

/*
 * Creates a run for a replacement with the room that it takes for the given layout width. The
 * leading is the one of the replacement.
 */
TR_INTERNAL TextRunRef TextRunCreateReplacement(TRUInteger codeUnitStart, TRUInteger codeUnitEnd,
    TRUInt8 bidiLevel, TRReplacementRef replacement, TRTypefaceRef typeface, TRFloat typeSize,
    TRFloat layoutWidth);

/*
 * Returns the same replacement run for another layout width, which a frame decides. Its leading is
 * zero, as the room that a frame gives includes the space around the replacement. A run that is
 * not a replacement is returned as it is, with a new reference.
 */
TR_INTERNAL TextRunRef TextRunCreateForLayoutWidth(TextRunRef textRun, TRFloat layoutWidth);

TR_INTERNAL TRBoolean TextRunIsBlock(TextRunRef textRun);
TR_INTERNAL TRBoolean TextRunIsRTL(TextRunRef textRun);

/*
 * The functions below take code unit indexes that MUST be within the code units of the run, and
 * return code unit indexes, glyph indexes or distances along the run.
 */

/* Returns the start of the cluster of a code unit, and its end. */
TR_INTERNAL TRUInteger TextRunGetClusterStart(TextRunRef textRun, TRUInteger index);
TR_INTERNAL TRUInteger TextRunGetClusterEnd(TextRunRef textRun, TRUInteger index);

/* Returns the first glyph in the order of the writing that a code unit has, and its last glyph. */
TR_INTERNAL TRUInteger TextRunGetLeadingGlyphIndex(TextRunRef textRun, TRUInteger index);
TR_INTERNAL TRUInteger TextRunGetTrailingGlyphIndex(TextRunRef textRun, TRUInteger index);

/* Finds the glyphs of a range of code units, which MUST NOT be empty. */
TR_INTERNAL void TextRunGetGlyphRange(TextRunRef textRun, TRUInteger start, TRUInteger end,
    TRUInteger *glyphStart, TRUInteger *glyphEnd);

/* Returns the edge that is at the left side of a range of code units. */
TR_INTERNAL TRFloat TextRunGetCaretBoundary(TextRunRef textRun, TRUInteger start, TRUInteger end);

/* Returns the distance of the boundary before a code unit from the start of the run. */
TR_INTERNAL TRFloat TextRunGetCaretEdge(TextRunRef textRun, TRUInteger index);

/* Returns the extent of a range of code units, which MAY be empty. */
TR_INTERNAL TRFloat TextRunGetDistance(TextRunRef textRun, TRUInteger start, TRUInteger end);

/* Returns the extent of the whole run. */
TR_INTERNAL TRFloat TextRunGetWidth(TextRunRef textRun);

TR_INTERNAL TextRunRef TextRunRetain(TextRunRef textRun);
TR_INTERNAL void TextRunRelease(TextRunRef textRun);

#endif
