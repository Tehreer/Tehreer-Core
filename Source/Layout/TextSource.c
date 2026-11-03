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

#include <stddef.h>

#include <SheenBidi/SheenBidi.h>
#include <SheenBidi/SBTextConfig.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRAttributeList.h>
#include <Tehreer/TRBase.h>
#include <Tehreer/TRSheenBidi.h>

#include <API/TRAssert.h>
#include <API/TRAttributeList.h>
#include <API/TRBase.h>
#include <API/TRGlyphRun.h>
#include <Core/Array.h>
#include <Layout/ShapeResolver.h>
#include <Layout/TextBuffer.h>
#include <Layout/TextRun.h>
#include <Text/AttributeRegistry.h>
#include <Text/BreakClassifier.h>

#include "TextSource.h"

/*
 * Creates the config of a text, whose paragraphs are shaped by the provider as the defaults say.
 * The config is the caller's to release.
 */
static SBTextConfigRef CreateConfig(ShapeDefaultsRef defaults)
{
    SBTextConfigRef config = AttributeRegistryCreateConfig();

    if (config) {
        SBTextConfigSetParagraphUserInfoCallbacks(config, ShapedParagraphGetUserInfoCallbacks());
        SBTextConfigSetParagraphUserInfoProvider(config, ShapeResolverProvideParagraph, defaults);
    }

    return config;
}

static void SetCursorRun(RunCursor *cursor, TRUInteger runIndex)
{
    TextRunRef textRun = ShapedParagraphGetRun(cursor->paragraph.shaped, runIndex);

    cursor->runIndex = runIndex;
    cursor->textRun = textRun;
    cursor->start = cursor->paragraph.start + textRun->codeUnitStart;
    cursor->end = cursor->paragraph.start + textRun->codeUnitEnd;
}

/* Applies an attribute to what the text is painted with, if the attribute is about painting. */
static void ApplyAttributeToPaint(GlyphRunPaint *paint, const TRAttribute *item)
{
    if (item->type == TRAttributeForegroundColor) {
        paint->flags |= TRGlyphRunFlagForegroundColor;
        paint->foregroundColor = item->value.foregroundColor;
    } else if (item->type == TRAttributeBackgroundColor) {
        paint->flags |= TRGlyphRunFlagBackgroundColor;
        paint->backgroundColor = item->value.backgroundColor;
    } else if (item->type == TRAttributeUnderline) {
        if (item->value.underline) {
            paint->flags |= TRGlyphRunFlagUnderline;
        }
    } else if (item->type == TRAttributeStrikethrough) {
        if (item->value.strikethrough) {
            paint->flags |= TRGlyphRunFlagStrikethrough;
        }
    } else if (item->type == TRAttributeDecorationColor) {
        paint->flags |= TRGlyphRunFlagDecorationColor;
        paint->decorationColor = item->value.decorationColor;
    } else if (item->type == TRAttributeUserData) {
        paint->userData = item->value.userData;
    }
}

TR_INTERNAL TRBoolean TextSourceInitialize(TextSource *source, const void *string,
    TRUInteger length, TRStringEncoding encoding, const TRAttribute *defaultAttributes,
    TRUInteger defaultAttributeCount, TRBoolean isMutable)
{
    ShapeDefaultsRef defaults = ShapeDefaultsCreate(defaultAttributes, defaultAttributeCount);
    TRBoolean isInitialized = TRFalse;

    if (defaults) {
        SBTextConfigRef config = CreateConfig(defaults);

        if (config) {
            if (isMutable) {
                source->_sbText = SBTextCreateMutable(encoding, config);
            } else {
                source->_sbText = (SBMutableTextRef)SBTextCreate(string, length, encoding, config);
            }

            isInitialized = (source->_sbText != NULL);

            SBTextConfigRelease(config);
        }

        if (isInitialized) {
            source->_defaults = defaults;
        } else {
            ShapeDefaultsRelease(defaults);
        }
    }

    return isInitialized;
}

TR_INTERNAL TRBoolean TextSourceInitializeCopy(TextSource *source, TextSourceRef original,
    TRBoolean isMutable)
{
    if (isMutable) {
        source->_sbText = SBTextCreateMutableCopy(original->_sbText);
    } else {
        source->_sbText = (SBMutableTextRef)SBTextCreateCopy(original->_sbText);
    }

    if (source->_sbText) {
        source->_defaults = ShapeDefaultsRetain(original->_defaults);
    }

    return (source->_sbText != NULL);
}

TR_INTERNAL void TextSourceFinalize(TextSource *source)
{
    SBTextRelease(source->_sbText);
    ShapeDefaultsRelease(source->_defaults);
}

TR_INTERNAL SBTextRef TextSourceGetSheenBidiText(TextSourceRef source)
{
    return source->_sbText;
}

TR_INTERNAL TRStringEncoding TextSourceGetEncoding(TextSourceRef source)
{
    return SBTextGetEncoding(source->_sbText);
}

TR_INTERNAL TRUInteger TextSourceGetLength(TextSourceRef source)
{
    return SBTextGetLength(source->_sbText);
}

TR_INTERNAL void TextSourceReadCodeUnits(TextSourceRef source, TRUInteger index, TRUInteger length,
    void *buffer)
{
    if (length > 0) {
        SBTextGetCodeUnits(source->_sbText, index, length, buffer);
    }
}

TR_INTERNAL TRAttributeListRef TextSourceCopyAttributes(TextSourceRef source, TRUInteger index,
    TRUInteger *length)
{
    SBAttributeListRef sbList;

    /* The code unit MUST be within the text. */
    TRAssert(index < SBTextGetLength(source->_sbText));

    sbList = SBTextGetAttributes(source->_sbText, SBAttributeFilterMakeAny(), index, length);

    return TRAttributeListMake(sbList);
}

TR_INTERNAL void TextSourceBeginEditing(TextSource *source)
{
    SBTextBeginEditing(source->_sbText);
}

TR_INTERNAL void TextSourceEndEditing(TextSource *source)
{
    SBTextEndEditing(source->_sbText);
}

TR_INTERNAL void TextSourceAppendCodeUnits(TextSource *source, const void *buffer,
    TRUInteger count)
{
    if (count > 0) {
        SBTextAppendCodeUnits(source->_sbText, buffer, count);
    }
}

TR_INTERNAL void TextSourceInsertCodeUnits(TextSource *source, TRUInteger index,
    const void *buffer, TRUInteger count)
{
    if (count > 0) {
        SBTextInsertCodeUnits(source->_sbText, index, buffer, count);
    }
}

TR_INTERNAL void TextSourceDeleteCodeUnits(TextSource *source, TRUInteger index,
    TRUInteger length)
{
    if (length > 0) {
        SBTextDeleteCodeUnits(source->_sbText, index, length);
    }
}

TR_INTERNAL void TextSourceSetCodeUnits(TextSource *source, const void *buffer, TRUInteger count)
{
    SBTextSetCodeUnits(source->_sbText, buffer, count);
}

TR_INTERNAL void TextSourceReplaceCodeUnits(TextSource *source, TRUInteger index,
    TRUInteger length, const void *buffer, TRUInteger count)
{
    if (count == 0) {
        TextSourceDeleteCodeUnits(source, index, length);
    } else {
        SBTextReplaceCodeUnits(source->_sbText, index, length, buffer, count);
    }
}

TR_INTERNAL TRBoolean TextSourceSetAttribute(TextSource *source, TRUInteger index,
    TRUInteger length, const TRAttribute *attribute)
{
    SBAttributeID attributeID = AttributeRegistryGetAttributeID(attribute->type);
    TRBoolean isSet = (attributeID != SBAttributeIDNone);

    if (isSet && length > 0) {
        SBTextSetAttribute(source->_sbText, index, length, attributeID, attribute);
    }

    return isSet;
}

TR_INTERNAL TRBoolean TextSourceRemoveAttribute(TextSource *source, TRUInteger index,
    TRUInteger length, TRAttributeType type)
{
    SBAttributeID attributeID = AttributeRegistryGetAttributeID(type);
    TRBoolean isRemoved = (attributeID != SBAttributeIDNone);

    if (isRemoved && length > 0) {
        SBTextRemoveAttribute(source->_sbText, index, length, attributeID);
    }

    return isRemoved;
}

TR_INTERNAL void TextSourceGetParagraph(TextSourceRef source, TRUInteger index,
    ParagraphInfo *paragraph)
{
    SBParagraphInfo sbParagraph;

    /* The code unit MUST be within the text. */
    TRAssert(index < SBTextGetLength(source->_sbText));

    SBTextGetCodeUnitParagraphInfo(source->_sbText, index, &sbParagraph);

    paragraph->start = sbParagraph.index;
    paragraph->end = sbParagraph.index + sbParagraph.length;
    paragraph->baseLevel = sbParagraph.baseLevel;
    paragraph->shaped = (ShapedParagraphRef)sbParagraph.userInfo;
}

TR_INTERNAL TRBoolean TextSourceIsShaped(TextSourceRef source, TRUInteger start,
    TRUInteger end)
{
    TRBoolean isShaped = TRTrue;
    TRUInteger index = start;

    while (isShaped && index < end) {
        ParagraphInfo paragraph;

        TextSourceGetParagraph(source, index, &paragraph);

        isShaped = (paragraph.shaped && paragraph.shaped->isShaped);
        index = paragraph.end;
    }

    return isShaped;
}

TR_INTERNAL TRBoolean TextSourceFindRun(TextSourceRef source, TRUInteger index,
    RunCursor *cursor)
{
    TRBoolean isFound = TRFalse;

    if (index < SBTextGetLength(source->_sbText)) {
        TRUInteger runIndex = TRInvalidIndex;

        TextSourceGetParagraph(source, index, &cursor->paragraph);

        if (cursor->paragraph.shaped) {
            runIndex = ShapedParagraphFindRun(cursor->paragraph.shaped,
                index - cursor->paragraph.start);
        }
        if (runIndex != TRInvalidIndex) {
            SetCursorRun(cursor, runIndex);
            isFound = TRTrue;
        }
    }

    return isFound;
}

TR_INTERNAL TRBoolean TextSourceMoveToNextRun(TextSourceRef source, RunCursor *cursor)
{
    TRBoolean isMoved = TRFalse;

    if (cursor->runIndex + 1 < ArrayGetCount(&cursor->paragraph.shaped->runs)) {
        SetCursorRun(cursor, cursor->runIndex + 1);
        isMoved = TRTrue;
    } else if (cursor->paragraph.end < SBTextGetLength(source->_sbText)) {
        TextSourceGetParagraph(source, cursor->paragraph.end, &cursor->paragraph);

        if (cursor->paragraph.shaped && ArrayGetCount(&cursor->paragraph.shaped->runs) > 0) {
            SetCursorRun(cursor, 0);
            isMoved = TRTrue;
        }
    }

    return isMoved;
}

TR_INTERNAL TRBoolean TextSourceMoveToPreviousRun(TextSourceRef source,
    RunCursor *cursor)
{
    TRBoolean isMoved = TRFalse;

    if (cursor->runIndex > 0) {
        SetCursorRun(cursor, cursor->runIndex - 1);
        isMoved = TRTrue;
    } else if (cursor->paragraph.start > 0) {
        TextSourceGetParagraph(source, cursor->paragraph.start - 1, &cursor->paragraph);

        if (cursor->paragraph.shaped && ArrayGetCount(&cursor->paragraph.shaped->runs) > 0) {
            SetCursorRun(cursor, ArrayGetCount(&cursor->paragraph.shaped->runs) - 1);
            isMoved = TRTrue;
        }
    }

    return isMoved;
}

TR_INTERNAL TRFloat TextSourceMeasureRange(TextSourceRef source, TRUInteger start,
    TRUInteger end)
{
    TRFloat extent = 0.0f;
    RunCursor cursor;

    /* The range MUST be within shaped paragraphs. */
    if (start < end && TextSourceFindRun(source, start, &cursor)) {
        do {
            TRUInteger base = cursor.paragraph.start;
            TRUInteger segmentEnd = NumberMin(end, cursor.end);

            extent += TextRunGetDistance(cursor.textRun, start - base, segmentEnd - base);

            start = segmentEnd;
        } while (start < end && TextSourceMoveToNextRun(source, &cursor));
    }

    return extent;
}

TR_INTERNAL TRUInteger TextSourceGetForwardBreak(TextSourceRef source, BreakType type,
    TRUInteger from, TRUInteger to)
{
    ParagraphInfo paragraph;
    TRUInteger limit;

    TextSourceGetParagraph(source, from, &paragraph);
    limit = NumberMin(to, paragraph.end);

    /* The paragraph MUST be shaped. */
    TRAssert(paragraph.shaped);

    return paragraph.start + BreakClassifierGetForwardBreak(paragraph.shaped->breaks, type,
        from - paragraph.start, limit - paragraph.start);
}

TR_INTERNAL TRUInteger TextSourceGetBackwardBreak(TextSourceRef source, BreakType type,
    TRUInteger from, TRUInteger to)
{
    ParagraphInfo paragraph;
    TRUInteger limit;

    TextSourceGetParagraph(source, from - 1, &paragraph);
    limit = NumberMax(to, paragraph.start);

    /* The paragraph MUST be shaped. */
    TRAssert(paragraph.shaped);

    return paragraph.start + BreakClassifierGetBackwardBreak(paragraph.shaped->breaks, type,
        from - paragraph.start, limit - paragraph.start);
}

TR_INTERNAL TRUInt32 TextSourceGetCodeUnit(TextSourceRef source, TRUInteger index)
{
    ParagraphInfo paragraph;

    TextSourceGetParagraph(source, index, &paragraph);

    /* The paragraph MUST be shaped. */
    TRAssert(paragraph.shaped);

    return TextBufferGetCodeUnit(&paragraph.shaped->buffer, index - paragraph.start);
}

TR_INTERNAL TRUInt32 TextSourceGetCodePoint(TextSourceRef source, TRUInteger index)
{
    ParagraphInfo paragraph;

    TextSourceGetParagraph(source, index, &paragraph);

    /* The paragraph MUST be shaped. */
    TRAssert(paragraph.shaped);

    return TextBufferGetCodePoint(&paragraph.shaped->buffer, index - paragraph.start);
}

TR_INTERNAL TRUInteger TextSourceGetLeadingWhitespaceEnd(TextSourceRef source,
    TRUInteger start, TRUInteger end)
{
    TRUInteger whitespaceEnd = end;
    TRUInteger index = start;

    while (index < end) {
        ParagraphInfo paragraph;
        TRUInteger segmentEnd;
        TRUInteger found;

        TextSourceGetParagraph(source, index, &paragraph);
        segmentEnd = NumberMin(end, paragraph.end);
        found = paragraph.start + TextBufferGetLeadingWhitespaceEnd(&paragraph.shaped->buffer,
            index - paragraph.start, segmentEnd - paragraph.start);

        if (found < segmentEnd) {
            whitespaceEnd = found;
            break;
        }

        index = segmentEnd;
    }

    return whitespaceEnd;
}

TR_INTERNAL TRUInteger TextSourceGetTrailingWhitespaceStart(TextSourceRef source,
    TRUInteger start, TRUInteger end)
{
    TRUInteger whitespaceStart = start;
    TRUInteger index = end;

    while (index > start) {
        ParagraphInfo paragraph;
        TRUInteger segmentStart;
        TRUInteger found;

        TextSourceGetParagraph(source, index - 1, &paragraph);
        segmentStart = NumberMax(start, paragraph.start);
        found = paragraph.start + TextBufferGetTrailingWhitespaceStart(&paragraph.shaped->buffer,
            segmentStart - paragraph.start, index - paragraph.start);

        if (found > segmentStart) {
            whitespaceStart = found;
            break;
        }

        index = segmentStart;
    }

    return whitespaceStart;
}

TR_INTERNAL TRUInteger TextSourceFindWhitespace(TextSourceRef source, TRUInteger start,
    TRUInteger end)
{
    TRUInteger whitespaceStart = end;
    TRUInteger index = start;

    while (index < end) {
        ParagraphInfo paragraph;
        TRUInteger segmentEnd;
        TRUInteger found;

        TextSourceGetParagraph(source, index, &paragraph);
        segmentEnd = NumberMin(end, paragraph.end);
        found = paragraph.start + TextBufferFindWhitespace(&paragraph.shaped->buffer,
            index - paragraph.start, segmentEnd - paragraph.start);

        if (found < segmentEnd) {
            whitespaceStart = found;
            break;
        }

        index = segmentEnd;
    }

    return whitespaceStart;
}

TR_INTERNAL TRBoolean TextSourceAddVisualRuns(TextSourceRef source, TRUInteger start,
    TRUInteger end, Array *visualRuns)
{
    SBVisualRunIteratorRef iterator = SBTextCreateVisualRunIterator(source->_sbText, start,
                                                                    end - start);
    TRBoolean isAdded = TRFalse;

    if (iterator) {
        isAdded = TRTrue;

        while (isAdded && SBVisualRunIteratorMoveNext(iterator)) {
            const SBVisualRun *sbRun = SBVisualRunIteratorGetCurrent(iterator);
            VisualRun visualRun;

            visualRun.start = sbRun->index;
            visualRun.end = sbRun->index + sbRun->length;
            visualRun.level = sbRun->level;

            isAdded = ArrayAppend(visualRuns, &visualRun);
        }

        SBVisualRunIteratorRelease(iterator);
    }

    return isAdded;
}

TR_INTERNAL TRUInteger TextSourceGetPaint(TextSourceRef source, TRUInteger index,
    GlyphRunPaint *paint)
{
    TRUInteger length = 0;
    SBAttributeListRef sbAttributes;
    TRAttributeListRef attributes;

    paint->flags = 0;
    paint->foregroundColor = 0;
    paint->backgroundColor = 0;
    paint->decorationColor = 0;
    paint->userData = NULL;

    sbAttributes = SBTextGetAttributes(source->_sbText,
        SBAttributeFilterMakeCollection(SBAttributeGroupNone, SBAttributeScopeCharacter),
        index, &length);
    attributes = TRAttributeListMake(sbAttributes);

    if (attributes) {
        TRUInteger count = TRAttributeListGetCount(attributes);
        TRUInteger itemIndex;

        for (itemIndex = 0; itemIndex < count; itemIndex++) {
            ApplyAttributeToPaint(paint, TRAttributeListGetItem(attributes, itemIndex));
        }

        TRAttributeListRelease(attributes);
    }

    return length;
}
