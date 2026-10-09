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

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>

TR_EXTERN_C_BEGIN

/**
 * Opaque reference to an immutable list of attributes, as the ones that a text has at an index.
 */
typedef const struct _TRAttributeList *TRAttributeListRef;

/**
 * Returns a pointer to the attribute located at the given index in the list.
 *
 * @param list
 *      The attribute list to retrieve from.
 * @param index
 *      The zero-based index of the attribute to retrieve, which should be less than the count
 *      returned by `TRAttributeListGetCount`.
 * @return
 *      A pointer to the attribute at the specified index, or `NULL` if the index is not less than
 *      the count.
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

/**
 * Increments the reference count of an attribute list.
 *
 * @param list
 *      The attribute list whose reference count will be incremented.
 * @return
 *      The same attribute list passed in as the parameter.
 */
TR_PUBLIC TRAttributeListRef TRAttributeListRetain(TRAttributeListRef list);

/**
 * Decrements the reference count of an attribute list. The list will be deallocated when its
 * reference count reaches zero.
 *
 * @param list
 *      The attribute list whose reference count will be decremented.
 */
TR_PUBLIC void TRAttributeListRelease(TRAttributeListRef list);

TR_EXTERN_C_END

#endif
