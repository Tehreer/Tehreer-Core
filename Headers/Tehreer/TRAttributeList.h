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

#ifndef _TEHREER_ATTRIBUTE_LIST_H
#define _TEHREER_ATTRIBUTE_LIST_H

#include <SheenBidi/SBAttributeList.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * Opaque reference to an attribute list, retained/released via `SBAttributeListRetain` and
 * `SBAttributeListRelease`.
 */
typedef SBAttributeListRef TRAttributeListRef;

/**
 * Returns a pointer to the attribute located at the given index in the list.
 *
 * @param list
 *      The attribute list to retrieve from.
 * @param index
 *      The zero-based index of the attribute to retrieve. Must be less than the count returned by
 *      `TRAttributeListGetCount`.
 * @return
 *      A pointer to the attribute at the specified index.
 */
TR_PUBLIC const TRAttribute *TRAttributeListGetItem(TRAttributeListRef list, TRUInteger index);

/**
 * Returns the number of attributes in the list.
 *
 * @param list
 *      The attribute list to query.
 * @return
 *      The total count of attributes currently stored in the list.
 */
TR_PUBLIC TRUInteger TRAttributeListGetCount(TRAttributeListRef list);

TR_EXTERN_C_END

#endif
