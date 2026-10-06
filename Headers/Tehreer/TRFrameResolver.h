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


#ifndef _TEHREER_FRAME_RESOLVER_H
#define _TEHREER_FRAME_RESOLVER_H

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRTypesetter.h>

TR_EXTERN_C_BEGIN

/**
 * The vertical alignment of the lines in a frame, when the frame is taller than they are.
 */
enum {
    TRVerticalAlignmentTop = 0,
    TRVerticalAlignmentCenter = 1,
    TRVerticalAlignmentBottom = 2
};
typedef TRUInt32 TRVerticalAlignment;

/**
 * Makes frames of text with a typesetter, according to what is set on it. A resolver is not
 * thread safe; each thread uses its own, and the frames that it makes are independent of it.
 *
 * The paragraphs take their alignment, indents, spacing and line heights from the paragraph
 * attributes of the text. The alignment of a paragraph that has none is the one of the resolver.
 */
typedef struct _TRFrameResolver *TRFrameResolverRef;

/**
 * Creates a resolver that has no typesetter, an unbounded frame, leading alignment at the top, no
 * truncation, no justification, and no limit of lines.
 *
 * @return
 *      New resolver, or `NULL` on failure.
 */
TR_PUBLIC TRFrameResolverRef TRFrameResolverCreate(void);

/**
 * Sets the typesetter to make frames with. The resolver retains it.
 */
TR_PUBLIC void TRFrameResolverSetTypesetter(TRFrameResolverRef resolver,
    TRTypesetterRef typesetter);

/**
 * Sets the size of the frame, which is unbounded by default. A size that is negative is taken as
 * zero.
 */
TR_PUBLIC void TRFrameResolverSetFrameSize(TRFrameResolverRef resolver, TRFloat width,
    TRFloat height);

/**
 * Sets whether the frame is as wide as the widest line, instead of the width that is set.
 */
TR_PUBLIC void TRFrameResolverSetFitsHorizontally(TRFrameResolverRef resolver, TRBoolean fits);

/**
 * Sets whether the frame is as tall as its lines, instead of the height that is set.
 */
TR_PUBLIC void TRFrameResolverSetFitsVertically(TRFrameResolverRef resolver, TRBoolean fits);

/**
 * Sets the alignment of the lines of the paragraphs that do not have their own.
 */
TR_PUBLIC void TRFrameResolverSetTextAlignment(TRFrameResolverRef resolver,
    TRTextAlignment alignment);

/**
 * Sets where the lines are placed if the frame is taller than they are.
 */
TR_PUBLIC void TRFrameResolverSetVerticalAlignment(TRFrameResolverRef resolver,
    TRVerticalAlignment alignment);

/**
 * Sets how the last line of the frame is cut, if there is more text than the frame can show.
 */
TR_PUBLIC void TRFrameResolverSetTruncationMode(TRFrameResolverRef resolver, TRBreakMode mode);

/**
 * Sets where the last line of the frame is cut, and enables the truncation. It is disabled by
 * default.
 */
TR_PUBLIC void TRFrameResolverSetTruncationPlace(TRFrameResolverRef resolver,
    TRTruncationPlace place);

/**
 * Disables the truncation.
 */
TR_PUBLIC void TRFrameResolverDisableTruncation(TRFrameResolverRef resolver);

/**
 * Sets whether the lines that end before the end of their paragraph are justified.
 */
TR_PUBLIC void TRFrameResolverSetJustificationEnabled(TRFrameResolverRef resolver,
    TRBoolean isEnabled);

/**
 * Sets the justification level, from 0.0 to 1.0. A lower level keeps the words closer, while a
 * higher level spreads them to the width of the frame. It is 1.0 by default.
 */
TR_PUBLIC void TRFrameResolverSetJustificationLevel(TRFrameResolverRef resolver, TRFloat level);

/**
 * Sets the greatest number of lines in a frame. Zero means that there is no limit.
 */
TR_PUBLIC void TRFrameResolverSetMaxLines(TRFrameResolverRef resolver, TRUInteger maxLines);

/**
 * Sets the space that is added below each line. It is resolved before the line height multiplier.
 */
TR_PUBLIC void TRFrameResolverSetExtraLineSpacing(TRFrameResolverRef resolver, TRFloat spacing);

/**
 * Sets the factor that the height of each line is multiplied with, keeping its text in the
 * middle. It is resolved after the extra line spacing, and it is ignored if it is not positive.
 */
TR_PUBLIC void TRFrameResolverSetLineHeightMultiplier(TRFrameResolverRef resolver,
    TRFloat multiplier);

/**
 * Creates a frame of a range of the text. The resolver fills the frame until the text ends, or
 * until what is left does not fit in the frame. A frame has at least one line, even if it is
 * smaller than it. The frame ends where its last line does, unless that line was truncated, in
 * which case it covers the whole range.
 *
 * @param resolver
 *      The resolver, which MUST have a typesetter.
 * @param range
 *      The code units of the frame, which MUST be within the text.
 * @return
 *      New frame, or `NULL` on failure.
 */
TR_PUBLIC TRComposedFrameRef TRFrameResolverCreateFrame(TRFrameResolverRef resolver,
    TRRange range);

/**
 * Increments the reference count of a resolver.
 */
TR_PUBLIC TRFrameResolverRef TRFrameResolverRetain(TRFrameResolverRef resolver);

/**
 * Decrements the reference count of a resolver, and destroys it when the count reaches zero.
 */
TR_PUBLIC void TRFrameResolverRelease(TRFrameResolverRef resolver);

TR_EXTERN_C_END

#endif
