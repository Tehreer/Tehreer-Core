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

#ifndef _TEHREER_LAYOUT_SHAPE_RESOLVER_H
#define _TEHREER_LAYOUT_SHAPE_RESOLVER_H

#include <SheenBidi/SBText.h>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRBase.h>
#include <Tehreer/TRFontFeatures.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRBase.h>
#include <Core/Object.h>

/* What decides how a part of the text is shaped, beside its bidirectional level and script. */
typedef struct _ShapingStyle {
    TRTypefaceRef typeface;
    TRFloat typeSize;
    TRFloat baselineOffset;
    TRFloat obliqueness;
    TRFloat scaleX;
    TRFloat scaleY;
    TRReplacementRef replacement;
    TRTag languageTag;
    TRFontFeaturesRef fontFeatures;
} ShapingStyle;

/*
 * The style that fills in what the attributes of a text do not set. It keeps its own references,
 * and a typesetter and its copies share it, since it is the context of the provider that shapes
 * the paragraphs.
 */
typedef struct _ShapeDefaults {
    ObjectBase _base;
    ShapingStyle style;
} ShapeDefaults, *ShapeDefaultsRef;

/*
 * Creates the defaults from the attributes that decide the shaping, which are the only ones
 * looked at. Returns `NULL` on failure.
 */
TR_INTERNAL ShapeDefaultsRef ShapeDefaultsCreate(const TRAttribute *attributes, TRUInteger count);

TR_INTERNAL ShapeDefaultsRef ShapeDefaultsRetain(ShapeDefaultsRef defaults);
TR_INTERNAL void ShapeDefaultsRelease(ShapeDefaultsRef defaults);

/*
 * The provider of the paragraphs of a text, which SheenBidi calls when an edit creates or changes
 * a paragraph. Its context MUST be the defaults. It returns a `ShapedParagraph` that has the runs
 * of the paragraph, and which stays unshaped if some of its text has no typeface, or `NULL` on
 * failure.
 */
TR_INTERNAL const void *ShapeResolverProvideParagraph(SBTextRef sbText, SBUInteger index,
    SBUInteger length, void *context);

#endif
