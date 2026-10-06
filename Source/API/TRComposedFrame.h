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


#ifndef _TEHREER_API_COMPOSED_FRAME_H
#define _TEHREER_API_COMPOSED_FRAME_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRComposedFrame.h>

#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <Core/Object.h>

typedef struct _TRComposedFrame {
    ObjectBase _base;
    TRUInteger codeUnitStart;
    TRUInteger codeUnitEnd;
    TRFloat width;
    TRFloat height;
    ComposedLineRef *lines;
    TRUInteger lineCount;
} TRComposedFrame, *ComposedFrameRef;

/*
 * Creates a frame from its lines, which are in the order of the text. The frame takes the
 * references of the lines, and the array that holds them is the caller's. Returns NULL on
 * failure, in which case the lines are released as well.
 */
TR_INTERNAL ComposedFrameRef ComposedFrameCreate(TRUInteger start, TRUInteger end,
    ComposedLineRef *lines, TRUInteger lineCount, TRFloat width, TRFloat height);

#endif
