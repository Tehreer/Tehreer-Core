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
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Once.h>

#include "AttributeRegistry.h"

#define AttributeCount  20

typedef struct _AttributeDescription {
    const char *name;
    SBAttributeScope scope;
} AttributeDescription;

/* The descriptions are in the order of the attribute types, which start from one. */
static const AttributeDescription AttributeDescriptions[AttributeCount] = {
    { "Typeface",               SBAttributeScopeCharacter },
    { "PointSize",              SBAttributeScopeCharacter },
    { "ScaleX",                 SBAttributeScopeCharacter },
    { "ScaleY",                 SBAttributeScopeCharacter },
    { "BaselineOffset",         SBAttributeScopeCharacter },
    { "Obliqueness",            SBAttributeScopeCharacter },
    { "Replacement",            SBAttributeScopeCharacter },
    { "TextAlignment",          SBAttributeScopeParagraph },
    { "FirstLineHeadIndent",    SBAttributeScopeParagraph },
    { "HeadIndent",             SBAttributeScopeParagraph },
    { "TailIndent",             SBAttributeScopeParagraph },
    { "FirstIndentLineCount",   SBAttributeScopeParagraph },
    { "ParagraphSpacingBefore", SBAttributeScopeParagraph },
    { "ParagraphSpacing",       SBAttributeScopeParagraph },
    { "LineHeightMultiple",     SBAttributeScopeParagraph },
    { "MinimumLineHeight",      SBAttributeScopeParagraph },
    { "MaximumLineHeight",      SBAttributeScopeParagraph },
    { "LineSpacing",            SBAttributeScopeParagraph },
    { "ForegroundColor",        SBAttributeScopeCharacter },
    { "UserData",               SBAttributeScopeCharacter }
};

static AttributeRegistry GlobalAttributeRegistry;
static SBTextConfigRef DefaultTextConfig;
static SBAttributeID AttributeIDs[AttributeCount + 1];

static SBBoolean EqualAttributeItem(const void *firstPtr, const void *secondPtr)
{
    const TRAttribute *firstItem = firstPtr;
    const TRAttribute *secondItem = secondPtr;

    if (firstItem == secondItem) {
        return SBTrue;
    }
    if (firstItem->type != secondItem->type) {
        return SBFalse;
    }

    switch (firstItem->type) {
    case TRAttributeTypeface:
        return firstItem->value.typeface == secondItem->value.typeface;
    case TRAttributePointSize:
        return firstItem->value.pointSize == secondItem->value.pointSize;
    case TRAttributeScaleX:
        return firstItem->value.scaleX == secondItem->value.scaleX;
    case TRAttributeScaleY:
        return firstItem->value.scaleY == secondItem->value.scaleY;
    case TRAttributeBaselineOffset:
        return firstItem->value.baselineOffset == secondItem->value.baselineOffset;
    case TRAttributeObliqueness:
        return firstItem->value.obliqueness == secondItem->value.obliqueness;
    case TRAttributeReplacement:
        return firstItem->value.replacement == secondItem->value.replacement;
    case TRAttributeTextAlignment:
        return firstItem->value.textAlignment == secondItem->value.textAlignment;
    case TRAttributeFirstLineHeadIndent:
        return firstItem->value.firstLineHeadIndent == secondItem->value.firstLineHeadIndent;
    case TRAttributeHeadIndent:
        return firstItem->value.headIndent == secondItem->value.headIndent;
    case TRAttributeTailIndent:
        return firstItem->value.tailIndent == secondItem->value.tailIndent;
    case TRAttributeFirstIndentLineCount:
        return firstItem->value.firstIndentLineCount == secondItem->value.firstIndentLineCount;
    case TRAttributeParagraphSpacingBefore:
        return firstItem->value.paragraphSpacingBefore == secondItem->value.paragraphSpacingBefore;
    case TRAttributeParagraphSpacing:
        return firstItem->value.paragraphSpacing == secondItem->value.paragraphSpacing;
    case TRAttributeLineHeightMultiple:
        return firstItem->value.lineHeightMultiple == secondItem->value.lineHeightMultiple;
    case TRAttributeMinimumLineHeight:
        return firstItem->value.minimumLineHeight == secondItem->value.minimumLineHeight;
    case TRAttributeMaximumLineHeight:
        return firstItem->value.maximumLineHeight == secondItem->value.maximumLineHeight;
    case TRAttributeLineSpacing:
        return firstItem->value.lineSpacing == secondItem->value.lineSpacing;
    case TRAttributeForegroundColor:
        return firstItem->value.foregroundColor == secondItem->value.foregroundColor;
    case TRAttributeUserData:
        return firstItem->value.userData == secondItem->value.userData;
    }

    return SBFalse;
}

static const void *RetainAttributeItem(const void *pointer)
{
    const TRAttribute *item = pointer;

    if (item->type == TRAttributeTypeface) {
        TRTypefaceRetain(item->value.typeface);
    } else if (item->type == TRAttributeReplacement) {
        TRReplacementRetain(item->value.replacement);
    }

    return pointer;
}

static void ReleaseAttributeItem(const void *pointer)
{
    const TRAttribute *item = pointer;

    if (item->type == TRAttributeTypeface) {
        TRTypefaceRelease(item->value.typeface);
    } else if (item->type == TRAttributeReplacement) {
        TRReplacementRelease(item->value.replacement);
    }
}

static SBAttributeValueCallbacks AttributeItemCallbacks = {
    EqualAttributeItem,
    RetainAttributeItem,
    ReleaseAttributeItem
};

static void InitializeAttributeRegistry(void)
{
    SBAttributeInfo attributeInfos[AttributeCount];
    SBAttributeRegistryRef internalRegistry;
    unsigned int index;

    for (index = 0; index < AttributeCount; index++) {
        attributeInfos[index].name = AttributeDescriptions[index].name;
        attributeInfos[index].group = SBAttributeGroupNone;
        attributeInfos[index].scope = AttributeDescriptions[index].scope;
    }

    internalRegistry = SBAttributeRegistryCreate(attributeInfos, AttributeCount,
        sizeof(TRAttribute), &AttributeItemCallbacks);

    if (internalRegistry) {
        /* Cache the IDs once so TRTextSetAttribute/RemoveAttribute can route without a name lookup. */
        for (index = 0; index < AttributeCount; index++) {
            AttributeIDs[index + 1] = SBAttributeRegistryGetAttributeID(internalRegistry,
                AttributeDescriptions[index].name);
        }

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

    if (type >= 1 && type <= AttributeCount) {
        return AttributeIDs[type];
    }

    return SBAttributeIDNone;
}

TR_INTERNAL SBTextConfigRef AttributeRegistryGetDefaultConfig(void)
{
    TryLazyInitializeAttributeRegistry();

    return DefaultTextConfig;
}
