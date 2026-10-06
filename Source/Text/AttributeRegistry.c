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

#include <SheenBidi/SheenBidi.h>
#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Once.h>

#include "AttributeRegistry.h"

static const char TypefaceAttributeName[] = "Typeface";
static const char PointSizeAttributeName[] = "PointSize";

static AttributeRegistry GlobalAttributeRegistry;
static SBTextConfigRef DefaultTextConfig;
static SBAttributeID TypefaceAttributeID = SBAttributeIDNone;
static SBAttributeID PointSizeAttributeID = SBAttributeIDNone;

static SBBoolean EqualAttributeItem(const void *firstPtr, const void *secondPtr)
{
    const TRAttribute *firstItem = firstPtr;
    const TRAttribute *secondItem = secondPtr;

    if (firstItem == secondItem) {
        return SBTrue;
    }

    switch (firstItem->type) {
    case TRAttributeTypeface:
        return firstItem->value.typeface == secondItem->value.typeface;
    case TRAttributePointSize:
        return firstItem->value.pointSize == secondItem->value.pointSize;
    }

    return SBFalse;
}

static const void *RetainAttributeItem(const void *pointer)
{
    const TRAttribute *item = pointer;

    if (item->type == TRAttributeTypeface) {
        TRTypefaceRetain(item->value.typeface);
    }

    return pointer;
}

static void ReleaseAttributeItem(const void *pointer)
{
    const TRAttribute *item = pointer;

    if (item->type == TRAttributeTypeface) {
        TRTypefaceRelease(item->value.typeface);
    }
}

static SBAttributeValueCallbacks AttributeItemCallbacks = {
    EqualAttributeItem,
    RetainAttributeItem,
    ReleaseAttributeItem
};

static void InitializeAttributeRegistry(void)
{
    SBAttributeInfo attributeInfos[] = {
        { TypefaceAttributeName, SBAttributeGroupNone, SBAttributeScopeCharacter },
        { PointSizeAttributeName, SBAttributeGroupNone, SBAttributeScopeCharacter }
    };
    SBAttributeRegistryRef internalRegistry;

    internalRegistry = SBAttributeRegistryCreate(attributeInfos, 2, sizeof(TRAttribute),
        &AttributeItemCallbacks);

    if (internalRegistry) {
        /* Cache the IDs once so TRTextSetAttribute/RemoveAttribute can route without a name lookup. */
        TypefaceAttributeID = SBAttributeRegistryGetAttributeID(internalRegistry, TypefaceAttributeName);
        PointSizeAttributeID = SBAttributeRegistryGetAttributeID(internalRegistry, PointSizeAttributeName);

        GlobalAttributeRegistry._registry = internalRegistry;

        DefaultTextConfig = SBTextConfigCreate();
        if (DefaultTextConfig) {
            SBTextConfigSetAttributeRegistry(DefaultTextConfig, internalRegistry);
        }
    }
}

static void TryLazyInitializeAttributeRegistry(void)
{
    static Once once = OnceMake();
    OnceExecute(&once, InitializeAttributeRegistry);
}

TR_INTERNAL SBAttributeID AttributeRegistryGetAttributeID(TRAttributeType type)
{
    TryLazyInitializeAttributeRegistry();

    switch (type) {
    case TRAttributeTypeface:
        return TypefaceAttributeID;
    case TRAttributePointSize:
        return PointSizeAttributeID;
    }

    return SBAttributeIDNone;
}

TR_INTERNAL SBTextConfigRef AttributeRegistryGetDefaultConfig(void)
{
    TryLazyInitializeAttributeRegistry();

    return DefaultTextConfig;
}
