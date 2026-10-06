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

#include <stddef.h>

#include <API/TRBase.h>

#include "CaretEdgesBuilder.h"

/*
 * Glyph indexes are one-based here, so that a cluster boundary before the first glyph can be told
 * apart from the one at the start of the first cluster.
 */
static void BuildCaretAdvances(TRBoolean isBackward, const TRFloat *glyphAdvances,
    TRUInteger glyphCount, const TRUInteger *clusterMap, TRUInteger codeUnitCount,
    const TRBoolean *caretStops, TRFloat *caretAdvances)
{
    TRUInteger glyphIndex = clusterMap[0] + 1;
    TRUInteger refIndex = glyphIndex;
    TRUInteger totalStops = 0;
    TRUInteger clusterStart = 0;
    TRUInteger codeUnitIndex;

    for (codeUnitIndex = 0; codeUnitIndex <= codeUnitCount; codeUnitIndex++) {
        caretAdvances[codeUnitIndex] = 0.0f;
    }

    for (codeUnitIndex = 1; codeUnitIndex <= codeUnitCount; codeUnitIndex++) {
        TRUInteger oldIndex = glyphIndex;

        if (codeUnitIndex != codeUnitCount) {
            glyphIndex = clusterMap[codeUnitIndex] + 1;

            if (caretStops && !caretStops[codeUnitIndex - 1]) {
                continue;
            }

            totalStops += 1;
        } else {
            totalStops += 1;
            glyphIndex = (isBackward ? 0 : glyphCount + 1);
        }

        if (glyphIndex != oldIndex) {
            TRFloat clusterAdvance = 0.0f;
            TRFloat distance = 0.0f;
            TRUInteger counter = 1;

            /* Find out the advance of the current cluster. */
            if (isBackward) {
                while (refIndex > glyphIndex) {
                    clusterAdvance += glyphAdvances[refIndex - 1];
                    refIndex -= 1;
                }
            } else {
                while (refIndex < glyphIndex) {
                    clusterAdvance += glyphAdvances[refIndex - 1];
                    refIndex += 1;
                }
            }

            /* Divide the advance evenly between the caret stops of the cluster. */
            while (clusterStart < codeUnitIndex) {
                TRFloat advance = 0.0f;

                if (!caretStops || caretStops[clusterStart] || clusterStart == codeUnitCount - 1) {
                    TRFloat previous = distance;

                    distance = (clusterAdvance * (TRFloat)counter) / (TRFloat)totalStops;
                    advance = distance - previous;
                    counter += 1;
                }

                caretAdvances[clusterStart] = advance;
                clusterStart += 1;
            }

            totalStops = 0;
        }
    }
}

TR_INTERNAL void CaretEdgesBuild(TRBoolean isBackward, TRBoolean isRTL,
    const TRFloat *glyphAdvances, TRUInteger glyphCount, const TRUInteger *clusterMap,
    TRUInteger codeUnitCount, const TRBoolean *caretStops, TRFloat *caretEdges)
{
    TRFloat distance = 0.0f;

    if (codeUnitCount == 0) {
        caretEdges[0] = 0.0f;
        return;
    }

    BuildCaretAdvances(isBackward, glyphAdvances, glyphCount, clusterMap, codeUnitCount,
        caretStops, caretEdges);

    if (isRTL) {
        TRUInteger index = codeUnitCount;

        /* The last edge is zero, and the edges grow toward the first code unit. */
        caretEdges[codeUnitCount] = 0.0f;

        while (index > 0) {
            index -= 1;

            distance += caretEdges[index];
            caretEdges[index] = distance;
        }
    } else {
        TRFloat advance = caretEdges[0];
        TRUInteger index;

        /* The first edge is zero, and the edges grow toward the last code unit. */
        caretEdges[0] = 0.0f;

        for (index = 1; index <= codeUnitCount; index++) {
            distance += advance;
            advance = caretEdges[index];
            caretEdges[index] = distance;
        }
    }
}
