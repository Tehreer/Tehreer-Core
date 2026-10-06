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

#ifndef _TEHREER_API_SHAPING_RESULT_H
#define _TEHREER_API_SHAPING_RESULT_H

#include <hb.h>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRShapingResult.h>

#include <API/TRBase.h>
#include <Core/Object.h>

typedef struct _TRShapingResult {
    ObjectBase _base;
    TRUInteger codeUnitCount;
    TRBoolean isBackward;
    TRBoolean isRTL;
    TRUInteger glyphCount;
    TRGlyphID *glyphIDs;
    TRPoint *glyphOffsets;
    TRFloat *glyphAdvances;
    TRUInteger *clusterMap;
} TRShapingResult;

/*
 * Creates a result from a shaped HarfBuzz buffer. The glyphs are stored in the order of the
 * writing direction, and their metrics are scaled by `sizeByEm`. `codeUnitCount` MUST be the
 * number of code units that were added to the buffer, so that every cluster is below it.
 */
TR_INTERNAL TRShapingResultRef TRShapingResultCreate(hb_buffer_t *hbBuffer,
    TRUInteger codeUnitCount, TRFloat sizeByEm, TRBoolean isBackward, TRBoolean isRTL);

#endif
