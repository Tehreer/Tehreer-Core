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

#ifndef _TEHREER_COMPOSED_LINE_H
#define _TEHREER_COMPOSED_LINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRGlyphRun.h>
#include <Tehreer/TRRenderer.h>

TR_EXTERN_C_BEGIN

/**
 * A line of text: glyph runs in the order they appear on the screen, which is not the order of the
 * text where it has directions that are mixed. The y axis of all of its positions points downward,
 * and the baseline of the line is at the y of its origin.
 *
 * A line that comes from a frame has its origin in the coordinates of the frame. One that was
 * made by a typesetter has its origin at zero. A line never changes once it is handed out, so it
 * can be used from multiple threads.
 */
typedef const struct _TRComposedLine *TRComposedLineRef;

/**
 * A function that receives the left and right edges of a part of a line.
 */
typedef void (*TREdgeFunc)(void *userData, TRFloat left, TRFloat right);

/**
 * Returns the code units that the line covers.
 */
TR_PUBLIC TRRange TRComposedLineGetCodeUnitRange(TRComposedLineRef line);

/**
 * Returns the base level of the paragraph of the line: even for left-to-right and odd for
 * right-to-left.
 */
TR_PUBLIC TRUInt8 TRComposedLineGetParagraphLevel(TRComposedLineRef line);

/**
 * Returns the position of the start of the line on its baseline.
 */
TR_PUBLIC TRPoint TRComposedLineGetOrigin(TRComposedLineRef line);

/**
 * Returns the distance from the baseline to the top of the line, to its bottom, and the extra
 * space below it.
 */
TR_PUBLIC TRFloat TRComposedLineGetAscent(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetDescent(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetLeading(TRComposedLineRef line);

/**
 * Returns the extent of the line along its direction, and its height with the leading.
 */
TR_PUBLIC TRFloat TRComposedLineGetWidth(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetHeight(TRComposedLineRef line);

/**
 * Returns the edges of the line in the coordinates of its frame: the top without the ascent
 * above the baseline, the bottom with the descent and the leading, and the left and right.
 */
TR_PUBLIC TRFloat TRComposedLineGetTop(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetBottom(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetLeft(TRComposedLineRef line);
TR_PUBLIC TRFloat TRComposedLineGetRight(TRComposedLineRef line);

/**
 * Returns the extent of the whitespace at the end of the line, which is part of its width.
 */
TR_PUBLIC TRFloat TRComposedLineGetTrailingWhitespaceExtent(TRComposedLineRef line);

/**
 * Returns whether the line holds a block replacement, which is as tall as the replacement and has
 * nothing else.
 */
TR_PUBLIC TRBoolean TRComposedLineIsBlock(TRComposedLineRef line);

/**
 * Returns whether the line shows a token in place of some of its text. Such a line cannot be
 * made again from its range, as that would show the text that was cut out.
 */
TR_PUBLIC TRBoolean TRComposedLineIsTruncated(TRComposedLineRef line);

/**
 * Returns the number of glyph runs, and a glyph run by its index. The run is not retained for the
 * caller, and stays valid as long as the line is alive.
 */
TR_PUBLIC TRUInteger TRComposedLineGetGlyphRunCount(TRComposedLineRef line);
TR_PUBLIC TRGlyphRunRef TRComposedLineGetGlyphRun(TRComposedLineRef line, TRUInteger index);

/**
 * Returns the distance from the start of the line to the boundary before a code unit, which can be
 * the end of the line too.
 */
TR_PUBLIC TRFloat TRComposedLineGetCodeUnitDistance(TRComposedLineRef line, TRUInteger index);

/**
 * Passes the parts of the line that a range of code units covers to the function, as pairs of
 * their left and right edges, measured from the start of the line. There is a part for each glyph
 * run that the range touches, in the order of the runs.
 */
TR_PUBLIC void TRComposedLineEnumerateEdges(TRComposedLineRef line, TRRange range,
    TREdgeFunc func, void *userData);

/**
 * Returns the code unit boundary that is closest to a distance from the start of the line.
 */
TR_PUBLIC TRUInteger TRComposedLineGetCodeUnitIndex(TRComposedLineRef line, TRFloat distance);

/**
 * Returns how far to move the start of the line so that it is aligned. The flush factor is 0 for
 * the start of the extent and 1 for its end, and anything between them, such as 0.5 for the
 * center, is possible. The extent that is not taken by the line, without its trailing whitespace,
 * is divided by the factor.
 */
TR_PUBLIC TRFloat TRComposedLineGetPenOffset(TRComposedLineRef line, TRFloat flushFactor,
    TRFloat flushExtent);

/**
 * Returns the box around the glyphs of the line, relative to its origin. The renderer is set up
 * for each run as `TRGlyphRunGetBoundingBox()` does.
 */
TR_PUBLIC TRRect TRComposedLineGetBoundingBox(TRComposedLineRef line, TRRendererRef renderer);

/**
 * Retains the line.
 */
TR_PUBLIC TRComposedLineRef TRComposedLineRetain(TRComposedLineRef line);

/**
 * Releases the line.
 */
TR_PUBLIC void TRComposedLineRelease(TRComposedLineRef line);

TR_EXTERN_C_END

#endif
