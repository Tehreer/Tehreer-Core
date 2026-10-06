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

#include <SheenBidi/SBAttributeInfo.h>
#include <SheenBidi/SBAttributeList.h>

#include <Tehreer/TRAttributeList.h>

const TRAttribute *TRAttributeListGetItem(TRAttributeListRef list, TRUInteger index)
{
    const SBAttributeItem *item = SBAttributeListGetItem(list, index);

    /* The value is stored immediately after the ID, sized as sizeof(TRAttribute). */
    return (const TRAttribute *)(&item->attributeID + 1);
}

TRUInteger TRAttributeListGetCount(TRAttributeListRef list)
{
    return SBAttributeListGetCount(list);
}
