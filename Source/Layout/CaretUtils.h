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

#ifndef _TEHREER_LAYOUT_CARET_UTILS_H
#define _TEHREER_LAYOUT_CARET_UTILS_H

#include <API/TRBase.h>

/*
 * Caret edges give the distance of each code unit boundary of a run from its first edge, along the
 * direction of the writing. For right-to-left runs the first code unit is at the right, so the
 * edges go down toward the end. The functions below take the indexes of the edges, which are
 * relative to the first edge of the array, and inclusive ranges of them.
 */

/* Returns the edge that is at the left side of a range of edges. */
TR_INTERNAL TRFloat CaretUtilsGetLeftMargin(const TRFloat *caretEdges, TRBoolean isRTL,
    TRUInteger first, TRUInteger last);

/* Returns the distance between the first and the last edge of a range. */
TR_INTERNAL TRFloat CaretUtilsGetDistance(const TRFloat *caretEdges, TRBoolean isRTL,
    TRUInteger first, TRUInteger last);

/*
 * Returns the index of the edge that is closest to the distance from the left margin of the range,
 * which MUST NOT be empty. A distance before all of the edges gives the first index of the range,
 * and one after them gives the last index.
 */
TR_INTERNAL TRUInteger CaretUtilsGetIndexOfEdge(const TRFloat *caretEdges, TRBoolean isRTL,
    TRFloat distance, TRUInteger first, TRUInteger last);

#endif
