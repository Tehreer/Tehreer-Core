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

#ifndef _TEHREER_API_COMPOSED_LINE_H
#define _TEHREER_API_COMPOSED_LINE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRGeometry.h>

#include <API/TRBase.h>
#include <API/TRGlyphRun.h>
#include <Core/Object.h>
#include <Layout/TextBuffer.h>

typedef struct _TRComposedLine {
    ObjectBase _base;
    TRUInteger codeUnitStart;
    TRUInteger codeUnitEnd;
    TRUInt8 paragraphLevel;
    TRPoint origin;
    TRFloat ascent;
    TRFloat descent;
    TRFloat leading;
    TRFloat extent;
    TRFloat trailingWhitespaceExtent;
    TRBoolean isBlock;
    TRBoolean isTruncated;
    GlyphRunRef *runs;
    TRUInteger runCount;

    /* Set by the frame that the line is in. */
    TRFloat flushFactor;
    TRFloat intrinsicMargin;
} TRComposedLine, *ComposedLineRef;

/*
 * Creates a line from its runs, which are in visual order. The line takes the references of the
 * runs, and the array that holds them is the caller's. The origins of the runs are set here, and
 * the metrics of the line are the greatest of those of its runs. Returns NULL on failure, in which
 * case the runs are released as well.
 */
TR_INTERNAL ComposedLineRef ComposedLineCreate(const TextBuffer *buffer, TRUInteger start,
    TRUInteger end, GlyphRunRef *runs, TRUInteger runCount, TRUInt8 paragraphLevel);

TR_INTERNAL TRFloat ComposedLineGetTop(ComposedLineRef line);
TR_INTERNAL TRFloat ComposedLineGetBottom(ComposedLineRef line);

#endif
