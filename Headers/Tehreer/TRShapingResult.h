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

#ifndef _TEHREER_SHAPING_RESULT_H
#define _TEHREER_SHAPING_RESULT_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>

TR_EXTERN_C_BEGIN

/**
 * The glyphs that a shaping engine produced for a run of text. The glyphs are in the order of the
 * writing direction: for left-to-right text the pen moves right by the advance of each glyph after
 * it is placed, and for right-to-left text it moves left. All distances are in the unit of the
 * type size that was used for shaping.
 *
 * A shaping result is immutable, so it can be shared between threads.
 */
typedef const struct _TRShapingResult *TRShapingResultRef;

/**
 * Returns the number of code units of the shaped text.
 */
TR_PUBLIC TRUInteger TRShapingResultGetCodeUnitCount(TRShapingResultRef result);

/**
 * Returns whether the text was shaped in backward order.
 */
TR_PUBLIC TRBoolean TRShapingResultIsBackward(TRShapingResultRef result);

/**
 * Returns whether the glyphs flow from right to left.
 */
TR_PUBLIC TRBoolean TRShapingResultIsRTL(TRShapingResultRef result);

/**
 * Returns the number of glyphs.
 */
TR_PUBLIC TRUInteger TRShapingResultGetGlyphCount(TRShapingResultRef result);

/**
 * Returns the glyph IDs. The pointer stays valid as long as the result is alive.
 */
TR_PUBLIC const TRGlyphID *TRShapingResultGetGlyphIDsPtr(TRShapingResultRef result);

/**
 * Returns the offsets of the glyphs from their pen positions, with the y axis pointing up as in
 * the font. The pointer stays valid as long as the result is alive.
 */
TR_PUBLIC const TRPoint *TRShapingResultGetGlyphOffsetsPtr(TRShapingResultRef result);

/**
 * Returns the horizontal advances of the glyphs. The pointer stays valid as long as the result is
 * alive.
 */
TR_PUBLIC const TRFloat *TRShapingResultGetGlyphAdvancesPtr(TRShapingResultRef result);

/**
 * Returns the glyph index of each code unit. If a code unit is a part of a cluster, it maps to the
 * first glyph of that cluster. The pointer stays valid as long as the result is alive.
 */
TR_PUBLIC const TRUInteger *TRShapingResultGetClusterMapPtr(TRShapingResultRef result);

/**
 * Computes the caret edges: the distance from the start of the run to each code unit boundary,
 * measured along the writing direction. The advance of a cluster is divided evenly between the
 * caret stops inside it.
 *
 * @param result
 *      The shaping result.
 * @param caretStops
 *      Optional flags telling for each code unit whether the caret can stop before it, or `NULL`
 *      to allow a stop at every code unit. If given, it MUST have one flag for each code unit.
 * @param caretEdges
 *      Receives the edges; it MUST have room for one more value than the number of code units.
 */
TR_PUBLIC void TRShapingResultGetCaretEdges(TRShapingResultRef result,
    const TRBoolean *caretStops, TRFloat *caretEdges);

/**
 * Retains the shaping result.
 */
TR_PUBLIC TRShapingResultRef TRShapingResultRetain(TRShapingResultRef result);

/**
 * Releases the shaping result.
 */
TR_PUBLIC void TRShapingResultRelease(TRShapingResultRef result);

TR_EXTERN_C_END

#endif
