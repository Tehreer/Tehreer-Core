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

#ifndef _TEHREER_TYPEFACE_MANAGER_H
#define _TEHREER_TYPEFACE_MANAGER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/*
 * The typeface manager is a registry that is shared by the whole process. It identifies typefaces
 * by an integer tag, groups them into families, and picks the typeface of a family that is closest
 * to a requested width, weight and slope, as described by CSS Fonts Level 4.
 *
 * A tag of 0 and a family ID of 0 mean none, so valid values start at 1. All functions can be
 * called from multiple threads.
 */

/**
 * The callback that receives the typefaces of the manager.
 *
 * @param typeface
 *      Registered typeface. It stays valid during the call even if it is unregistered meanwhile.
 * @param context
 *      The context that was given to the enumeration.
 * @param stop
 *      Set to `TRTrue` to end the enumeration after the callback returns.
 */
typedef void (*TRTypefaceManagerTypefaceCallback)(TRTypefaceRef typeface, void *context,
    TRBoolean *stop);

/**
 * The callback that receives the families of the manager.
 *
 * @param familyID
 *      ID of the family, or 0 if the typefaces are grouped by the font family name.
 * @param familyName
 *      Name of the family. For a family with an ID, it is the name of its first typeface in the
 *      sorted order. It stays valid during the call only.
 * @param context
 *      The context that was given to the enumeration.
 * @param stop
 *      Set to `TRTrue` to end the enumeration after the callback returns.
 */
typedef void (*TRTypefaceManagerFamilyCallback)(TRUInteger familyID,
    const TRStringView *familyName, void *context, TRBoolean *stop);

/**
 * Registers a typeface. The manager retains it until it is unregistered.
 *
 * @param typeface
 *      Typeface to register.
 * @param tag
 *      Tag that identifies the typeface, or 0 for no tag.
 * @param familyID
 *      ID of the family that the typeface belongs to, or 0 to group it with the typefaces that
 *      have no family ID and the same font family name.
 * @return
 *      `TRTrue` if the typeface was registered; `TRFalse` if it is `NULL`, is already registered,
 *      has a non-zero tag that is already taken, or on failure.
 */
TR_PUBLIC TRBoolean TRTypefaceManagerRegisterTypeface(TRTypefaceRef typeface, TRUInteger tag,
    TRUInteger familyID);

/**
 * Unregisters a typeface and releases the reference of the manager. Its tag and family ID become
 * free again.
 *
 * @param typeface
 *      Typeface to unregister.
 * @return
 *      `TRTrue` if the typeface was unregistered; `TRFalse` if it is not registered.
 */
TR_PUBLIC TRBoolean TRTypefaceManagerUnregisterTypeface(TRTypefaceRef typeface);

/**
 * Returns the typeface that is registered with a tag.
 *
 * @param tag
 *      Tag of the typeface.
 * @return
 *      The typeface, or `NULL` if the tag is 0 or not taken. It is not retained, and stays valid
 *      until the typeface is unregistered.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceManagerGetTypeface(TRUInteger tag);

/**
 * Returns the tag of a registered typeface.
 *
 * @param typeface
 *      Registered typeface.
 * @return
 *      The tag, or 0 if the typeface has none or is not registered.
 */
TR_PUBLIC TRUInteger TRTypefaceManagerGetTypefaceTag(TRTypefaceRef typeface);

/**
 * Returns the family ID of a registered typeface.
 *
 * @param typeface
 *      Registered typeface.
 * @return
 *      The family ID, or 0 if the typeface has none or is not registered.
 */
TR_PUBLIC TRUInteger TRTypefaceManagerGetTypefaceFamilyID(TRTypefaceRef typeface);

/**
 * Returns the typeface of a family that is closest to a width, weight and slope. Ties go to the
 * typeface that was registered first.
 *
 * @param familyID
 *      ID of the family.
 * @param width
 *      Desired width.
 * @param weight
 *      Desired weight.
 * @param slope
 *      Desired slope.
 * @return
 *      The matching typeface, or `NULL` if the ID is 0 or the family has no typefaces. It is not
 *      retained, and stays valid until the typeface is unregistered.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceManagerGetMatchingTypefaceByFamilyID(TRUInteger familyID,
    TRWidth width, TRWeight weight, TRSlope slope);

/**
 * Returns the typeface that is closest to a width, weight and slope among the typefaces that were
 * registered without a family ID. The names are compared ignoring the case of ASCII letters, and a
 * typeface without a font family name is treated as having an empty name, so it is matched by an
 * empty `familyName`. Ties go to the typeface that was registered first.
 *
 * @param familyName
 *      Font family name.
 * @param width
 *      Desired width.
 * @param weight
 *      Desired weight.
 * @param slope
 *      Desired slope.
 * @return
 *      The matching typeface, or `NULL` if the name is `NULL` or no typeface has it. It is not
 *      retained, and stays valid until the typeface is unregistered.
 */
TR_PUBLIC TRTypefaceRef TRTypefaceManagerGetMatchingTypefaceByFamilyName(
    const TRStringView *familyName, TRWidth width, TRWeight weight, TRSlope slope);

/**
 * Enumerates the registered typefaces, ordered by family name and then by style name, ignoring
 * the case of ASCII letters. A typeface without a name is treated as having an empty one. The
 * callback is called without holding any lock, so it may use the manager. Nothing is enumerated if
 * the snapshot of the typefaces cannot be allocated.
 *
 * @param callback
 *      Function that receives each typeface.
 * @param context
 *      Pointer that is passed to the callback.
 */
TR_PUBLIC void TRTypefaceManagerEnumerateTypefaces(TRTypefaceManagerTypefaceCallback callback,
    void *context);

/**
 * Enumerates the families of the registered typefaces, ordered by family name. Typefaces
 * without a family name form a family with an empty name. The callback is called without holding
 * any lock, so it may use the manager. Nothing is enumerated if the snapshot of the typefaces
 * cannot be allocated.
 *
 * @param callback
 *      Function that receives each family.
 * @param context
 *      Pointer that is passed to the callback.
 */
TR_PUBLIC void TRTypefaceManagerEnumerateFamilies(TRTypefaceManagerFamilyCallback callback,
    void *context);

TR_EXTERN_C_END

#endif
