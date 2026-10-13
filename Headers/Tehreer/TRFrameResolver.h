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
    TRVerticalAlignmentTop = 0,     /**< The lines are placed at the top of the frame. */
    TRVerticalAlignmentCenter = 1,  /**< The lines are placed in the middle of the frame. */
    TRVerticalAlignmentBottom = 2   /**< The lines are placed at the bottom of the frame. */
};
/**
 * A type to represent the vertical alignment of the lines in a frame, which is one of the
 * `TRVerticalAlignment*` constants above.
 */
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
 * Creates a resolver that has no typesetter, an unbounded frame that does not fit its lines,
 * leading text alignment, top vertical alignment, no truncation (with the line break mode and the
 * end as its defaults), no justification (with the level 1.0), no limit of lines, no extra line
 * spacing, and a line height multiplier of 1.
 *
 * @return
 *      New resolver, or `NULL` on failure.
 */
TR_PUBLIC TRFrameResolverRef TRFrameResolverCreate(void);

/**
 * Returns the typesetter to make frames with. The caller does not own it.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The typesetter, or `NULL` if it is not set.
 */
TR_PUBLIC TRTypesetterRef TRFrameResolverGetTypesetter(TRFrameResolverRef resolver);

/**
 * Sets the typesetter to make frames with. The resolver retains it.
 *
 * @param resolver
 *      The resolver.
 * @param typesetter
 *      The typesetter, or `NULL` to clear it.
 */
TR_PUBLIC void TRFrameResolverSetTypesetter(TRFrameResolverRef resolver,
    TRTypesetterRef typesetter);

/**
 * Returns the size of the frame, as it was set. A negative width or height is taken as zero when
 * making a frame.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The width and the height of the frame.
 */
TR_PUBLIC TRSize TRFrameResolverGetFrameSize(TRFrameResolverRef resolver);

/**
 * Sets the size of the frame, which is unbounded by default. A width or a height that is negative
 * is taken as zero.
 *
 * @param resolver
 *      The resolver.
 * @param size
 *      The width and the height of the frame.
 */
TR_PUBLIC void TRFrameResolverSetFrameSize(TRFrameResolverRef resolver, TRSize size);

/**
 * Returns whether the frame is as wide as the widest line, instead of the width that is set.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      `TRTrue` if the width of the frame fits its lines.
 */
TR_PUBLIC TRBoolean TRFrameResolverGetFitsHorizontally(TRFrameResolverRef resolver);

/**
 * Sets whether the frame is as wide as the widest line, instead of the width that is set. The lines
 * are moved to keep their alignment in the narrower frame. It is disabled by default.
 *
 * @param resolver
 *      The resolver.
 * @param fits
 *      `TRTrue` to fit the width of the frame to its lines.
 */
TR_PUBLIC void TRFrameResolverSetFitsHorizontally(TRFrameResolverRef resolver, TRBoolean fits);

/**
 * Returns whether the frame is as tall as its lines, instead of the height that is set.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      `TRTrue` if the height of the frame fits its lines.
 */
TR_PUBLIC TRBoolean TRFrameResolverGetFitsVertically(TRFrameResolverRef resolver);

/**
 * Sets whether the frame is as tall as its lines, instead of the height that is set. The vertical
 * alignment has no effect then. It is disabled by default.
 *
 * @param resolver
 *      The resolver.
 * @param fits
 *      `TRTrue` to fit the height of the frame to its lines.
 */
TR_PUBLIC void TRFrameResolverSetFitsVertically(TRFrameResolverRef resolver, TRBoolean fits);

/**
 * Returns the alignment of the lines of the paragraphs that do not have their own.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The alignment of the lines.
 */
TR_PUBLIC TRTextAlignment TRFrameResolverGetTextAlignment(TRFrameResolverRef resolver);

/**
 * Sets the alignment of the lines of the paragraphs that do not have their own. The default is
 * `TRTextAlignmentLeading`.
 *
 * @param resolver
 *      The resolver.
 * @param alignment
 *      The alignment of the lines.
 */
TR_PUBLIC void TRFrameResolverSetTextAlignment(TRFrameResolverRef resolver,
    TRTextAlignment alignment);

/**
 * Returns where the lines are placed if the frame is taller than they are.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The vertical alignment of the lines.
 */
TR_PUBLIC TRVerticalAlignment TRFrameResolverGetVerticalAlignment(TRFrameResolverRef resolver);

/**
 * Sets where the lines are placed if the frame is taller than they are. The default is
 * `TRVerticalAlignmentTop`.
 *
 * @param resolver
 *      The resolver.
 * @param alignment
 *      The vertical alignment of the lines.
 */
TR_PUBLIC void TRFrameResolverSetVerticalAlignment(TRFrameResolverRef resolver,
    TRVerticalAlignment alignment);

/**
 * Returns how the last line of the frame is cut, if there is more text than the frame can show.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The break mode of the truncation.
 */
TR_PUBLIC TRBreakMode TRFrameResolverGetTruncationMode(TRFrameResolverRef resolver);

/**
 * Sets how the last line of the frame is cut, if there is more text than the frame can show. The
 * default is `TRBreakModeLine`. It does not enable the truncation.
 *
 * @param resolver
 *      The resolver.
 * @param mode
 *      The break mode of the truncation.
 */
TR_PUBLIC void TRFrameResolverSetTruncationMode(TRFrameResolverRef resolver, TRBreakMode mode);

/**
 * Returns where the last line of the frame is cut.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The place of the truncation.
 */
TR_PUBLIC TRTruncationPlace TRFrameResolverGetTruncationPlace(TRFrameResolverRef resolver);

/**
 * Sets where the last line of the frame is cut. It does not enable the truncation; the end is the
 * default.
 *
 * @param resolver
 *      The resolver.
 * @param place
 *      The place of the truncation.
 */
TR_PUBLIC void TRFrameResolverSetTruncationPlace(TRFrameResolverRef resolver,
    TRTruncationPlace place);

/**
 * Returns whether the last line of the frame is cut, if there is more text than the frame can
 * show.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      `TRTrue` if the last line is cut.
 */
TR_PUBLIC TRBoolean TRFrameResolverIsTruncationEnabled(TRFrameResolverRef resolver);

/**
 * Sets whether the last line of the frame is cut, if there is more text than the frame can show.
 * It is disabled by default.
 *
 * @param resolver
 *      The resolver.
 * @param isEnabled
 *      `TRTrue` to cut the last line.
 */
TR_PUBLIC void TRFrameResolverSetTruncationEnabled(TRFrameResolverRef resolver,
    TRBoolean isEnabled);

/**
 * Returns whether the lines that end before the end of their paragraph are justified.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      `TRTrue` if the lines are justified.
 */
TR_PUBLIC TRBoolean TRFrameResolverIsJustificationEnabled(TRFrameResolverRef resolver);

/**
 * Sets whether the lines that end before the end of their paragraph are justified. It is disabled
 * by default.
 *
 * @param resolver
 *      The resolver.
 * @param isEnabled
 *      `TRTrue` to justify the lines.
 */
TR_PUBLIC void TRFrameResolverSetJustificationEnabled(TRFrameResolverRef resolver,
    TRBoolean isEnabled);

/**
 * Returns the justification level, as it was set.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The justification level.
 */
TR_PUBLIC TRFloat TRFrameResolverGetJustificationLevel(TRFrameResolverRef resolver);

/**
 * Sets the justification level, from 0.0 to 1.0. A lower level keeps the words closer, while a
 * higher level spreads them to the width of the frame. It is 1.0 by default. A line is justified to
 * what is left of the width after the margins of its paragraph.
 *
 * @param resolver
 *      The resolver.
 * @param level
 *      The justification level.
 */
TR_PUBLIC void TRFrameResolverSetJustificationLevel(TRFrameResolverRef resolver, TRFloat level);

/**
 * Returns the greatest number of lines in a frame. Zero means that there is no limit.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The greatest number of lines.
 */
TR_PUBLIC TRUInteger TRFrameResolverGetMaxLines(TRFrameResolverRef resolver);

/**
 * Sets the greatest number of lines in a frame. Zero means that there is no limit, which is the
 * default.
 *
 * @param resolver
 *      The resolver.
 * @param maxLines
 *      The greatest number of lines.
 */
TR_PUBLIC void TRFrameResolverSetMaxLines(TRFrameResolverRef resolver, TRUInteger maxLines);

/**
 * Returns the space that is added to the leading of each line, as it was set.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The space below each line.
 */
TR_PUBLIC TRFloat TRFrameResolverGetExtraLineSpacing(TRFrameResolverRef resolver);

/**
 * Sets the space that is added to the leading of each line, which is below it. It is resolved after
 * the line height multiplier, and it is ignored if it is not positive. A line that holds a block
 * replacement is not changed. The default is zero.
 *
 * @param resolver
 *      The resolver.
 * @param spacing
 *      The space below each line.
 */
TR_PUBLIC void TRFrameResolverSetExtraLineSpacing(TRFrameResolverRef resolver, TRFloat spacing);

/**
 * Returns the factor that the height of each line is multiplied with, as it was set.
 *
 * @param resolver
 *      The resolver.
 * @return
 *      The factor of the height.
 */
TR_PUBLIC TRFloat TRFrameResolverGetLineHeightMultiplier(TRFrameResolverRef resolver);

/**
 * Sets the factor that the height of each line is multiplied with, keeping its text in the
 * middle, which is done by adding half of the change to the ascent and half to the descent. It is
 * resolved before the extra line spacing, and it is ignored if it is not positive. A line that
 * holds a block replacement is not changed. The default is 1.
 *
 * @param resolver
 *      The resolver.
 * @param multiplier
 *      The factor of the height.
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
 *      The resolver, which has to have a typesetter.
 * @param index
 *      The index of the first code unit of the frame.
 * @param length
 *      The number of code units of the frame, which MUST NOT be zero. The range of the frame has to
 *      be within the text.
 * @return
 *      New frame, or `NULL` if the resolver has no typesetter, the range is empty or is not within
 *      the text, or on failure.
 */
TR_PUBLIC TRComposedFrameRef TRFrameResolverCreateFrame(TRFrameResolverRef resolver,
    TRUInteger index, TRUInteger length);

/**
 * Increments the reference count of a resolver.
 *
 * @param resolver
 *      The resolver whose reference count will be incremented.
 * @return
 *      The same resolver passed in as the parameter.
 */
TR_PUBLIC TRFrameResolverRef TRFrameResolverRetain(TRFrameResolverRef resolver);

/**
 * Decrements the reference count of a resolver, and destroys it when the count reaches zero.
 *
 * @param resolver
 *      The resolver whose reference count will be decremented.
 */
TR_PUBLIC void TRFrameResolverRelease(TRFrameResolverRef resolver);

TR_EXTERN_C_END

#endif
