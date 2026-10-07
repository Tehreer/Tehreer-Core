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
    SBBoolean isEqual = SBFalse;
    const TRAttribute *firstItem = firstPtr;
    const TRAttribute *secondItem = secondPtr;

    if (firstItem == secondItem) {
        isEqual = SBTrue;
    } else if (firstItem->type == secondItem->type) {
        switch (firstItem->type) {
        case TRAttributeTypeface:
            isEqual = firstItem->value.typeface == secondItem->value.typeface;
            break;
        case TRAttributePointSize:
            isEqual = firstItem->value.pointSize == secondItem->value.pointSize;
            break;
        case TRAttributeScaleX:
            isEqual = firstItem->value.scaleX == secondItem->value.scaleX;
            break;
        case TRAttributeScaleY:
            isEqual = firstItem->value.scaleY == secondItem->value.scaleY;
            break;
        case TRAttributeBaselineOffset:
            isEqual = firstItem->value.baselineOffset == secondItem->value.baselineOffset;
            break;
        case TRAttributeObliqueness:
            isEqual = firstItem->value.obliqueness == secondItem->value.obliqueness;
            break;
        case TRAttributeReplacement:
            isEqual = firstItem->value.replacement == secondItem->value.replacement;
            break;
        case TRAttributeTextAlignment:
            isEqual = firstItem->value.textAlignment == secondItem->value.textAlignment;
            break;
        case TRAttributeFirstLineHeadIndent:
            isEqual = firstItem->value.firstLineHeadIndent == secondItem->value.firstLineHeadIndent;
            break;
        case TRAttributeHeadIndent:
            isEqual = firstItem->value.headIndent == secondItem->value.headIndent;
            break;
        case TRAttributeTailIndent:
            isEqual = firstItem->value.tailIndent == secondItem->value.tailIndent;
            break;
        case TRAttributeFirstIndentLineCount:
            isEqual = (firstItem->value.firstIndentLineCount
                      == secondItem->value.firstIndentLineCount);
            break;
        case TRAttributeParagraphSpacingBefore:
            isEqual = (firstItem->value.paragraphSpacingBefore
                      == secondItem->value.paragraphSpacingBefore);
            break;
        case TRAttributeParagraphSpacing:
            isEqual = firstItem->value.paragraphSpacing == secondItem->value.paragraphSpacing;
            break;
        case TRAttributeLineHeightMultiple:
            isEqual = firstItem->value.lineHeightMultiple == secondItem->value.lineHeightMultiple;
            break;
        case TRAttributeMinimumLineHeight:
            isEqual = firstItem->value.minimumLineHeight == secondItem->value.minimumLineHeight;
            break;
        case TRAttributeMaximumLineHeight:
            isEqual = firstItem->value.maximumLineHeight == secondItem->value.maximumLineHeight;
            break;
        case TRAttributeLineSpacing:
            isEqual = firstItem->value.lineSpacing == secondItem->value.lineSpacing;
            break;
        case TRAttributeForegroundColor:
            isEqual = firstItem->value.foregroundColor == secondItem->value.foregroundColor;
            break;
        case TRAttributeUserData:
            isEqual = firstItem->value.userData == secondItem->value.userData;
            break;
        }
    }

    return isEqual;
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
    SBAttributeID attributeID = SBAttributeIDNone;

    TryLazyInitializeAttributeRegistry();

    if (type >= 1 && type <= AttributeCount) {
        attributeID = AttributeIDs[type];
    }

    return attributeID;
}

TR_INTERNAL SBTextConfigRef AttributeRegistryGetDefaultConfig(void)
{
    TryLazyInitializeAttributeRegistry();

    return DefaultTextConfig;
}
