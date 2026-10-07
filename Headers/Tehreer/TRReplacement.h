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

#ifndef _TEHREER_REPLACEMENT_H
#define _TEHREER_REPLACEMENT_H

#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * The room that a replacement takes in a line.
 */
typedef struct _TRReplacementRoom {
    TRFloat ascent;     /**< Distance from the baseline to the top. */
    TRFloat descent;    /**< Distance from the baseline to the bottom. */
    TRFloat extent;     /**< Extent along the line. */
} TRReplacementRoom;

/**
 * The functions that a replacement calls on its owner. Both are optional.
 */
typedef struct _TRReplacementCallbacks {
    /**
     * Computes the room of the replacement. It is called with a layout width of 0 while the text is
     * being shaped, and with the width of the frame while a frame is being laid out. A block
     * replacement usually takes the whole layout width as its extent.
     *
     * It may be called from any thread. Without it, the room is zero.
     */
    void (*computeRoom)(void *userData, TRFloat layoutWidth, TRReplacementRoom *room);

    /**
     * Called when the replacement is destroyed, so that the owner can free its `userData`.
     */
    void (*finalize)(void *userData);
} TRReplacementCallbacks;

/**
 * How a replacement is placed in the text.
 */
enum {
    TRReplacementKindInline = 0,    /**< The replacement stays on the line of the text around it. */
    TRReplacementKindBlock = 1      /**< The replacement has a line of its own. */
};
typedef TRUInt32 TRReplacementKind;

/**
 * Content that replaces a range of text, such as an image or a view. The wrapper that creates it
 * draws it, and Core only uses its room to lay out the line.
 */
typedef const struct _TRReplacement *TRReplacementRef;

/**
 * Creates a replacement.
 *
 * @param callbacks
 *      The callbacks, which are copied.
 * @param userData
 *      An opaque pointer that is passed to the callbacks.
 * @param kind
 *      How the replacement is placed in the text.
 * @return
 *      New replacement, or `NULL` if the kind is not valid or on failure.
 */
TR_PUBLIC TRReplacementRef TRReplacementCreate(const TRReplacementCallbacks *callbacks,
    void *userData, TRReplacementKind kind);

/**
 * Returns how the replacement is placed in the text.
 */
TR_PUBLIC TRReplacementKind TRReplacementGetKind(TRReplacementRef replacement);

/**
 * Computes the room of the replacement for the given layout width.
 */
TR_PUBLIC void TRReplacementComputeRoom(TRReplacementRef replacement, TRFloat layoutWidth,
    TRReplacementRoom *room);

/**
 * Retains the replacement.
 */
TR_PUBLIC TRReplacementRef TRReplacementRetain(TRReplacementRef replacement);

/**
 * Releases the replacement.
 */
TR_PUBLIC void TRReplacementRelease(TRReplacementRef replacement);

TR_EXTERN_C_END

#endif
