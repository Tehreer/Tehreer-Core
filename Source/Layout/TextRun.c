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
#include <string.h>

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRShapingEngine.h>
#include <Tehreer/TRShapingResult.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Object.h>
#include <Layout/CaretUtils.h>

#include "TextRun.h"

#define RUN             0
#define GLYPH_IDS       1
#define GLYPH_OFFSETS   2
#define GLYPH_ADVANCES  3
#define CLUSTER_MAP     4
#define CARET_EDGES     5
#define COUNT           6

static void FinalizeTextRun(ObjectRef object)
{
    TextRunRef textRun = object;

    TRTypefaceRelease(textRun->typeface);

    if (textRun->replacement) {
        TRReplacementRelease(textRun->replacement);
    }
}

static TextRunRef AllocateTextRun(TextRunKind kind, TRUInteger codeUnitStart,
    TRUInteger codeUnitEnd, TRUInteger glyphCount, TRUInt8 bidiLevel, TRTypefaceRef typeface,
    TRFloat typeSize)
{
    TRUInteger codeUnitCount = codeUnitEnd - codeUnitStart;
    TRUInteger sizes[COUNT] = { 0 };
    void *pointers[COUNT] = { NULL };
    TextRunRef textRun;

    sizes[RUN] = sizeof(TextRun);
    sizes[GLYPH_IDS] = sizeof(TRGlyphID) * glyphCount;
    sizes[GLYPH_OFFSETS] = sizeof(TRPoint) * glyphCount;
    sizes[GLYPH_ADVANCES] = sizeof(TRFloat) * glyphCount;
    sizes[CLUSTER_MAP] = sizeof(TRUInteger) * codeUnitCount;
    sizes[CARET_EDGES] = sizeof(TRFloat) * (codeUnitCount + 1);

    textRun = ObjectCreate(sizes, COUNT, pointers, FinalizeTextRun);

    if (textRun) {
        textRun->kind = kind;
        textRun->codeUnitStart = codeUnitStart;
        textRun->codeUnitEnd = codeUnitEnd;
        textRun->isBackward = TRFalse;
        textRun->bidiLevel = bidiLevel;
        textRun->writingDirection = TRWritingDirectionLeftToRight;
        textRun->typeface = TRTypefaceRetain(typeface);
        textRun->typeSize = typeSize;
        textRun->scaleX = 1.0f;
        textRun->scaleY = 1.0f;
        textRun->ascent = 0.0f;
        textRun->descent = 0.0f;
        textRun->leading = 0.0f;
        textRun->glyphCount = glyphCount;
        textRun->glyphIDs = pointers[GLYPH_IDS];
        textRun->glyphOffsets = pointers[GLYPH_OFFSETS];
        textRun->glyphAdvances = pointers[GLYPH_ADVANCES];
        textRun->clusterMap = pointers[CLUSTER_MAP];
        textRun->caretEdges = pointers[CARET_EDGES];
        textRun->replacement = NULL;
        textRun->extent = 0.0f;
    }

    return textRun;
}

#undef RUN
#undef GLYPH_IDS
#undef GLYPH_OFFSETS
#undef GLYPH_ADVANCES
#undef CLUSTER_MAP
#undef CARET_EDGES
#undef COUNT

TR_INTERNAL TextRunRef TextRunCreateIntrinsic(TRUInteger codeUnitStart, TRUInteger codeUnitEnd,
    TRUInt8 bidiLevel, TRTypefaceRef typeface, TRFloat typeSize, TRFloat scaleX, TRFloat scaleY,
    TRFloat baselineOffset, TRShapingResultRef shapingResult, TRWritingDirection writingDirection)
{
    TRUInteger glyphCount = TRShapingResultGetGlyphCount(shapingResult);
    TextRunRef textRun;

    textRun = AllocateTextRun(TextRunKindIntrinsic, codeUnitStart, codeUnitEnd, glyphCount,
        bidiLevel, typeface, typeSize);

    if (textRun) {
        const TRPoint *offsets = TRShapingResultGetGlyphOffsetsPtr(shapingResult);
        const TRFloat *advances = TRShapingResultGetGlyphAdvancesPtr(shapingResult);
        TRUInteger unitsPerEM = TRTypefaceGetUnitsPerEM(typeface);
        TRFloat sizeByEm = (unitsPerEM > 0 ? typeSize / (TRFloat)unitsPerEM : 0.0f);
        TRFloat sizeScale = sizeByEm * scaleY;
        TRUInteger index;

        textRun->isBackward = TRShapingResultIsBackward(shapingResult);
        textRun->writingDirection = writingDirection;
        textRun->scaleX = scaleX;
        textRun->scaleY = scaleY;
        textRun->ascent = (TRFloat)TRTypefaceGetAscent(typeface) * sizeScale;
        textRun->descent = (TRFloat)TRTypefaceGetDescent(typeface) * sizeScale;
        textRun->leading = (TRFloat)TRTypefaceGetLeading(typeface) * sizeScale;

        if (glyphCount > 0) {
            memcpy(textRun->glyphIDs, TRShapingResultGetGlyphIDsPtr(shapingResult),
                sizeof(TRGlyphID) * glyphCount);
        }

        for (index = 0; index < glyphCount; index++) {
            textRun->glyphOffsets[index].x = (offsets[index].x * scaleX);
            textRun->glyphOffsets[index].y = (offsets[index].y * scaleY) + baselineOffset;
            textRun->glyphAdvances[index] = advances[index] * scaleX;
        }

        if (codeUnitEnd > codeUnitStart) {
            memcpy(textRun->clusterMap, TRShapingResultGetClusterMapPtr(shapingResult),
                sizeof(TRUInteger) * (codeUnitEnd - codeUnitStart));
        }

        TRShapingResultGetCaretEdges(shapingResult, NULL, textRun->caretEdges);

        /* The glyphs are scaled horizontally, so the distances along the run are as well. */
        if (scaleX != 1.0f) {
            for (index = 0; index <= codeUnitEnd - codeUnitStart; index++) {
                textRun->caretEdges[index] *= scaleX;
            }
        }
    }

    return textRun;
}

TR_INTERNAL TextRunRef TextRunCreateReplacement(TRUInteger codeUnitStart, TRUInteger codeUnitEnd,
    TRUInt8 bidiLevel, TRReplacementRef replacement, TRTypefaceRef typeface, TRFloat typeSize,
    TRFloat layoutWidth)
{
    TextRunRef textRun;

    textRun = AllocateTextRun(TextRunKindReplacement, codeUnitStart, codeUnitEnd, 1, bidiLevel,
        typeface, typeSize);

    if (textRun) {
        TRReplacementRoom room;
        TRUInteger length = codeUnitEnd - codeUnitStart;
        TRUInteger index;

        TRReplacementComputeRoom(replacement, layoutWidth, &room);

        textRun->replacement = TRReplacementRetain(replacement);
        textRun->extent = room.extent;
        textRun->ascent = room.ascent;
        textRun->descent = room.descent;
        textRun->leading = TRReplacementGetLeading(replacement);

        /* The replacement takes the place of a single space. */
        textRun->glyphIDs[0] = TRTypefaceGetGlyphID(typeface, 0x20);
        textRun->glyphOffsets[0].x = 0.0f;
        textRun->glyphOffsets[0].y = 0.0f;
        textRun->glyphAdvances[0] = room.extent;

        for (index = 0; index < length; index++) {
            textRun->clusterMap[index] = 0;
            textRun->caretEdges[index] = 0.0f;
        }

        /* The whole extent is on the side where the run ends, which depends on its direction. */
        textRun->caretEdges[length] = 0.0f;
        if ((bidiLevel & 1) == 0) {
            textRun->caretEdges[length] = room.extent;
        } else {
            textRun->caretEdges[0] = room.extent;
        }
    }

    return textRun;
}

TR_INTERNAL TextRunRef TextRunCreateForLayoutWidth(TextRunRef textRun, TRFloat layoutWidth)
{
    TextRunRef forFrame;

    if (textRun->kind != TextRunKindReplacement) {
        return TextRunRetain(textRun);
    }

    forFrame = TextRunCreateReplacement(textRun->codeUnitStart, textRun->codeUnitEnd,
        textRun->bidiLevel, textRun->replacement, textRun->typeface, textRun->typeSize,
        layoutWidth);

    /* The room that a frame decides includes the space around the replacement. */
    if (forFrame) {
        forFrame->leading = 0.0f;
    }

    return forFrame;
}

TR_INTERNAL TRBoolean TextRunIsBlock(TextRunRef textRun)
{
    return (textRun->kind == TextRunKindReplacement
            && TRReplacementIsBlock(textRun->replacement));
}

TR_INTERNAL TRBoolean TextRunIsRTL(TextRunRef textRun)
{
    return (textRun->bidiLevel & 1) == 1;
}

static TRUInteger GetForwardGlyphIndex(TextRunRef textRun, TRUInteger mappingIndex)
{
    TRUInteger common = textRun->clusterMap[mappingIndex];
    TRUInteger length = textRun->codeUnitEnd - textRun->codeUnitStart;
    TRUInteger index;

    for (index = mappingIndex + 1; index < length; index++) {
        TRUInteger mapping = textRun->clusterMap[index];

        if (mapping != common) {
            return mapping - 1;
        }
    }

    return textRun->glyphCount - 1;
}

static TRUInteger GetBackwardGlyphIndex(TextRunRef textRun, TRUInteger mappingIndex)
{
    TRUInteger common = textRun->clusterMap[mappingIndex];
    TRUInteger index;

    for (index = mappingIndex; index > 0; index--) {
        TRUInteger mapping = textRun->clusterMap[index - 1];

        if (mapping != common) {
            return mapping - 1;
        }
    }

    return textRun->glyphCount - 1;
}

TR_INTERNAL TRUInteger TextRunGetClusterStart(TextRunRef textRun, TRUInteger index)
{
    TRUInteger mappingIndex = index - textRun->codeUnitStart;
    TRUInteger common;
    TRUInteger i;

    if (textRun->kind == TextRunKindReplacement) {
        return textRun->codeUnitStart;
    }

    common = textRun->clusterMap[mappingIndex];

    for (i = mappingIndex; i > 0; i--) {
        if (textRun->clusterMap[i - 1] != common) {
            return i + textRun->codeUnitStart;
        }
    }

    return textRun->codeUnitStart;
}

TR_INTERNAL TRUInteger TextRunGetClusterEnd(TextRunRef textRun, TRUInteger index)
{
    TRUInteger mappingIndex = index - textRun->codeUnitStart;
    TRUInteger length = textRun->codeUnitEnd - textRun->codeUnitStart;
    TRUInteger common;
    TRUInteger i;

    if (textRun->kind == TextRunKindReplacement) {
        return textRun->codeUnitEnd;
    }

    common = textRun->clusterMap[mappingIndex];

    for (i = mappingIndex + 1; i < length; i++) {
        if (textRun->clusterMap[i] != common) {
            return i + textRun->codeUnitStart;
        }
    }

    return length + textRun->codeUnitStart;
}

TR_INTERNAL TRUInteger TextRunGetLeadingGlyphIndex(TextRunRef textRun, TRUInteger index)
{
    TRUInteger mappingIndex = index - textRun->codeUnitStart;

    if (textRun->kind == TextRunKindReplacement) {
        return 0;
    }

    if (textRun->isBackward) {
        return GetBackwardGlyphIndex(textRun, mappingIndex);
    }

    return textRun->clusterMap[mappingIndex];
}

TR_INTERNAL TRUInteger TextRunGetTrailingGlyphIndex(TextRunRef textRun, TRUInteger index)
{
    TRUInteger mappingIndex = index - textRun->codeUnitStart;

    if (textRun->kind == TextRunKindReplacement) {
        return 0;
    }

    if (textRun->isBackward) {
        return textRun->clusterMap[mappingIndex];
    }

    return GetForwardGlyphIndex(textRun, mappingIndex);
}

TR_INTERNAL void TextRunGetGlyphRange(TextRunRef textRun, TRUInteger start, TRUInteger end,
    TRUInteger *glyphStart, TRUInteger *glyphEnd)
{
    TRUInteger firstIndex = start - textRun->codeUnitStart;
    TRUInteger lastIndex = end - 1 - textRun->codeUnitStart;

    /* The range MUST NOT be empty. */
    TRAssert(start < end);

    if (textRun->kind == TextRunKindReplacement) {
        *glyphStart = 0;
        *glyphEnd = 1;
    } else if (textRun->isBackward) {
        *glyphStart = textRun->clusterMap[lastIndex];
        *glyphEnd = GetBackwardGlyphIndex(textRun, firstIndex) + 1;
    } else {
        *glyphStart = textRun->clusterMap[firstIndex];
        *glyphEnd = GetForwardGlyphIndex(textRun, lastIndex) + 1;
    }
}

TR_INTERNAL TRFloat TextRunGetCaretBoundary(TextRunRef textRun, TRUInteger start, TRUInteger end)
{
    return CaretUtilsGetLeftMargin(textRun->caretEdges, TextRunIsRTL(textRun),
        start - textRun->codeUnitStart, end - textRun->codeUnitStart);
}

TR_INTERNAL TRFloat TextRunGetCaretEdge(TextRunRef textRun, TRUInteger index)
{
    return textRun->caretEdges[index - textRun->codeUnitStart];
}

TR_INTERNAL TRFloat TextRunGetDistance(TextRunRef textRun, TRUInteger start, TRUInteger end)
{
    return CaretUtilsGetDistance(textRun->caretEdges, TextRunIsRTL(textRun),
        start - textRun->codeUnitStart, end - textRun->codeUnitStart);
}

TR_INTERNAL TRFloat TextRunGetWidth(TextRunRef textRun)
{
    return TextRunGetDistance(textRun, textRun->codeUnitStart, textRun->codeUnitEnd);
}

TR_INTERNAL TextRunRef TextRunRetain(TextRunRef textRun)
{
    return ObjectRetain((ObjectRef)textRun);
}

TR_INTERNAL void TextRunRelease(TextRunRef textRun)
{
    ObjectRelease((ObjectRef)textRun);
}
