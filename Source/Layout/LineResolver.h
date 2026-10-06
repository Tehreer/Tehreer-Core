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

#ifndef _TEHREER_LAYOUT_LINE_RESOLVER_H
#define _TEHREER_LAYOUT_LINE_RESOLVER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRGlyphRun.h>
#include <API/TRTypesetter.h>

/* A growable list of glyph runs, which owns the references that it holds. */
typedef struct _RunList {
    GlyphRunRef *items;
    TRUInteger count;
    TRUInteger capacity;
    TRBoolean hasFailed;
} RunList;

TR_INTERNAL void RunListInitialize(RunList *list);

/* Releases the runs and the memory of the list. */
TR_INTERNAL void RunListFinalize(RunList *list);

/*
 * Inserts a run at an index that MUST NOT be greater than the count. The list takes the reference
 * of the run, or releases it and fails if there is no memory.
 */
TR_INTERNAL void RunListInsert(RunList *list, TRUInteger index, GlyphRunRef glyphRun);

/* A part of a line in the order that it is shown, with the bidirectional level of its text. */
typedef struct _VisualRun {
    TRUInteger start;
    TRUInteger end;
    TRUInt8 level;
} VisualRun;

typedef void (*VisualRunFunc)(void *context, const VisualRun *visualRun);

/*
 * Calls the function for the runs of the range in the order that they are shown, which is not the
 * order of the text where it has mixed directions. The range MUST NOT be empty. A line that spans
 * paragraphs shows them one after another in the direction of the first of them. Returns TRFalse
 * if the runs could not be found.
 */
TR_INTERNAL TRBoolean LineResolverForEachVisualRun(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, VisualRunFunc func, void *context);

/*
 * Adds the glyph runs of a range that is within a visual run. The runs of a right-to-left text are
 * added before those that precede them, which keeps the order in which they are shown. The room of
 * the replacements is that of the layout width, if there is one.
 */
TR_INTERNAL void LineResolverAppendVisualRuns(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRBoolean hasLayoutWidth, TRFloat layoutWidth);

/*
 * Creates a line from a list of runs, whose references the line takes over. The list is left
 * empty. Returns NULL on failure.
 */
TR_INTERNAL ComposedLineRef LineResolverCreateLine(TypesetterRef typesetter, TRUInteger start,
    TRUInteger end, RunList *list, TRUInt8 paragraphLevel);

/*
 * Creates a line that has all of the text of the range, in the order that it is shown. The range
 * MUST NOT be empty. The runs of the replacements of the line are sized for the layout width, if
 * there is one.
 */
TR_INTERNAL ComposedLineRef LineResolverCreateSimpleLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRBoolean hasLayoutWidth, TRFloat layoutWidth);

/*
 * Creates a line of the range that fits in the extent by cutting out text and showing the token
 * line in its place. The suggested breaks decide what is cut out. If nothing has to be cut, the
 * line is a simple one. The range MUST NOT be empty. Returns NULL on failure.
 */
TR_INTERNAL ComposedLineRef LineResolverCreateTruncatedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat extent, TRBreakMode breakMode,
    TRTruncationPlace truncationPlace, ComposedLineRef tokenLine);

/*
 * Creates a line of the range whose inner spaces are widened or squeezed so that it takes the
 * justification extent, as far as the factor says. The range MUST NOT be empty. Returns NULL on
 * failure.
 */
TR_INTERNAL ComposedLineRef LineResolverCreateJustifiedLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRFloat justificationFactor, TRFloat justificationExtent);

#endif
