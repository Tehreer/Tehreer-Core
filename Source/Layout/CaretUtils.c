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

#include <API/TRBase.h>

#include "CaretUtils.h"

/* Moves the index to the next edge in the direction, and tells if there is one to move to. */
static TRBoolean MoveToNextEdge(TRUInteger *index, TRBoolean isRTL, TRUInteger first,
    TRUInteger last)
{
    TRBoolean hasNext = TRTrue;

    if (isRTL) {
        if (*index == first) {
            hasNext = TRFalse;
        } else {
            *index -= 1;
        }
    } else if (*index == last) {
        hasNext = TRFalse;
    } else {
        *index += 1;
    }

    return hasNext;
}

TR_INTERNAL TRFloat CaretUtilsGetLeftMargin(const TRFloat *caretEdges, TRBoolean isRTL,
    TRUInteger first, TRUInteger last)
{
    return caretEdges[isRTL ? last : first];
}

TR_INTERNAL TRFloat CaretUtilsGetDistance(const TRFloat *caretEdges, TRBoolean isRTL,
    TRUInteger first, TRUInteger last)
{
    TRFloat firstEdge = caretEdges[first];
    TRFloat lastEdge = caretEdges[last];

    return (isRTL ? firstEdge - lastEdge : lastEdge - firstEdge);
}

TR_INTERNAL TRUInteger CaretUtilsGetIndexOfEdge(const TRFloat *caretEdges, TRBoolean isRTL,
    TRFloat distance, TRUInteger first, TRUInteger last)
{
    TRFloat leftMargin = CaretUtilsGetLeftMargin(caretEdges, isRTL, first, last);
    TRBoolean hasLeading = TRFalse;
    TRBoolean hasTrailing = TRFalse;
    TRUInteger leadingIndex = first;
    TRUInteger trailingIndex = first;
    TRFloat leadingEdge = 0.0f;
    TRFloat trailingEdge = 0.0f;
    TRUInteger index = (isRTL ? last : first);
    TRUInteger edgeIndex;

    for (;;) {
        TRFloat caretEdge = caretEdges[index] - leftMargin;

        if (caretEdge > distance) {
            hasTrailing = TRTrue;
            trailingIndex = index;
            trailingEdge = caretEdge;
            break;
        }

        hasLeading = TRTrue;
        leadingIndex = index;
        leadingEdge = caretEdge;

        /* Move to the next edge, and stop after the last one. */
        if (!MoveToNextEdge(&index, isRTL, first, last)) {
            break;
        }
    }

    if (!hasLeading) {
        /* Nothing is covered by the distance. */
        edgeIndex = first;
    } else if (!hasTrailing) {
        /* The whole range is covered by the distance. */
        edgeIndex = last;
    } else if (distance <= (leadingEdge + trailingEdge) / 2.0f) {
        /* The distance is closer to the first edge. */
        edgeIndex = leadingIndex;
    } else {
        /* The distance is closer to the second edge. */
        edgeIndex = trailingIndex;
    }

    return edgeIndex;
}
