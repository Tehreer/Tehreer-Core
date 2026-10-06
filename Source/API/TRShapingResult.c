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

#include <hb.h>

#include <API/TRBase.h>
#include <Core/Object.h>
#include <Text/CaretEdgesBuilder.h>

#include "TRShapingResult.h"

#define RESULT          0
#define GLYPH_IDS       1
#define GLYPH_OFFSETS   2
#define GLYPH_ADVANCES  3
#define CLUSTER_MAP     4
#define COUNT           5

/* Returns the index in the buffer of the glyph at the given index in the writing direction. */
#define BufferIndex(result_, index_)    \
    ((result_)->isRTL ? (result_)->glyphCount - (index_) - 1 : (index_))

static void BuildClusterMap(TRShapingResult *result, const hb_glyph_info_t *infos)
{
    TRUInteger codeUnitCount = result->codeUnitCount;
    TRUInteger glyphCount = result->glyphCount;
    TRUInteger *map = result->clusterMap;
    TRUInteger association = 0;
    TRUInteger index;

    for (index = 0; index < codeUnitCount; index++) {
        map[index] = TRInvalidIndex;
    }

    /* Traverse in reverse order so that the first glyph takes priority in case of multiple
     * substitution. */
    for (index = glyphCount; index > 0; index--) {
        TRUInteger cluster = infos[BufferIndex(result, index - 1)].cluster;

        /* A cluster MUST be less than the code unit count. */
        if (cluster < codeUnitCount) {
            association = cluster;
            map[association] = index - 1;
        }
    }

    if (result->isBackward) {
        /* Assign the same glyph index to the preceding code units. */
        for (index = codeUnitCount; index > 0; index--) {
            if (map[index - 1] == TRInvalidIndex) {
                map[index - 1] = association;
            }

            association = map[index - 1];
        }
    } else {
        /* Assign the same glyph index to the subsequent code units. */
        for (index = 0; index < codeUnitCount; index++) {
            if (map[index] == TRInvalidIndex) {
                map[index] = association;
            }

            association = map[index];
        }
    }
}

TR_INTERNAL TRShapingResultRef TRShapingResultCreate(hb_buffer_t *hbBuffer,
    TRUInteger codeUnitCount, TRFloat sizeByEm, TRBoolean isBackward, TRBoolean isRTL)
{
    const hb_glyph_info_t *infos;
    const hb_glyph_position_t *positions;
    unsigned int glyphCount = 0;
    TRUInteger sizes[COUNT] = { 0 };
    void *pointers[COUNT] = { NULL };
    TRShapingResult *result;

    infos = hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
    positions = hb_buffer_get_glyph_positions(hbBuffer, NULL);

    sizes[RESULT] = sizeof(TRShapingResult);
    sizes[GLYPH_IDS] = sizeof(TRGlyphID) * glyphCount;
    sizes[GLYPH_OFFSETS] = sizeof(TRPoint) * glyphCount;
    sizes[GLYPH_ADVANCES] = sizeof(TRFloat) * glyphCount;
    sizes[CLUSTER_MAP] = sizeof(TRUInteger) * codeUnitCount;

    result = ObjectCreate(sizes, COUNT, pointers, NULL);

    if (result) {
        TRUInteger index;

        result->codeUnitCount = codeUnitCount;
        result->isBackward = isBackward;
        result->isRTL = isRTL;
        result->glyphCount = glyphCount;
        result->glyphIDs = pointers[GLYPH_IDS];
        result->glyphOffsets = pointers[GLYPH_OFFSETS];
        result->glyphAdvances = pointers[GLYPH_ADVANCES];
        result->clusterMap = pointers[CLUSTER_MAP];

        for (index = 0; index < glyphCount; index++) {
            TRUInteger bufferIndex = BufferIndex(result, index);

            result->glyphIDs[index] = (TRGlyphID)infos[bufferIndex].codepoint;
            result->glyphOffsets[index].x = (TRFloat)positions[bufferIndex].x_offset * sizeByEm;
            result->glyphOffsets[index].y = (TRFloat)positions[bufferIndex].y_offset * sizeByEm;
            result->glyphAdvances[index] = (TRFloat)positions[bufferIndex].x_advance * sizeByEm;
        }

        BuildClusterMap(result, infos);
    }

    return result;
}

#undef RESULT
#undef GLYPH_IDS
#undef GLYPH_OFFSETS
#undef GLYPH_ADVANCES
#undef CLUSTER_MAP
#undef COUNT

TRUInteger TRShapingResultGetCodeUnitCount(TRShapingResultRef result)
{
    return result->codeUnitCount;
}

TRBoolean TRShapingResultIsBackward(TRShapingResultRef result)
{
    return result->isBackward;
}

TRBoolean TRShapingResultIsRTL(TRShapingResultRef result)
{
    return result->isRTL;
}

TRUInteger TRShapingResultGetGlyphCount(TRShapingResultRef result)
{
    return result->glyphCount;
}

const TRGlyphID *TRShapingResultGetGlyphIDsPtr(TRShapingResultRef result)
{
    return result->glyphIDs;
}

const TRPoint *TRShapingResultGetGlyphOffsetsPtr(TRShapingResultRef result)
{
    return result->glyphOffsets;
}

const TRFloat *TRShapingResultGetGlyphAdvancesPtr(TRShapingResultRef result)
{
    return result->glyphAdvances;
}

const TRUInteger *TRShapingResultGetClusterMapPtr(TRShapingResultRef result)
{
    return result->clusterMap;
}

void TRShapingResultGetCaretEdges(TRShapingResultRef result, const TRBoolean *caretStops,
    TRFloat *caretEdges)
{
    CaretEdgesBuild(result->isBackward, result->isRTL, result->glyphAdvances, result->glyphCount,
        result->clusterMap, result->codeUnitCount, caretStops, caretEdges);
}

TRShapingResultRef TRShapingResultRetain(TRShapingResultRef result)
{
    return ObjectRetain((ObjectRef)result);
}

void TRShapingResultRelease(TRShapingResultRef result)
{
    ObjectRelease((ObjectRef)result);
}
