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

#ifndef _TEHREER__TEST_TEXT_H
#define _TEHREER__TEST_TEXT_H

#include <cassert>
#include <cstddef>
#include <string>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypeface.h>

namespace Tehreer {

/* Creates a mutable UTF-16 text, with the typeface and the point size set on all of it. */
inline TRMutableTextRef makeTestText(const std::u16string &string, TRTypefaceRef typeface = nullptr,
    TRFloat pointSize = 0.0f) {
    TRMutableTextRef text = TRTextCreateMutable(TRStringEncodingUTF16);
    assert(text != nullptr);

    TRTextAppendCodeUnits(text, string.data(), string.size());

    if (!string.empty()) {
        if (typeface) {
            TRAttribute attribute = {};
            attribute.type = TRAttributeTypeface;
            attribute.value.typeface = typeface;
            TRTextSetAttribute(text, 0, string.size(), &attribute);
        }
        if (pointSize > 0.0f) {
            TRAttribute attribute = {};
            attribute.type = TRAttributePointSize;
            attribute.value.pointSize = pointSize;
            TRTextSetAttribute(text, 0, string.size(), &attribute);
        }
    }

    return text;
}

inline void setTestFloat(TRMutableTextRef text, size_t index, size_t length, TRAttributeType type,
    TRFloat value) {
    TRAttribute attribute = {};
    attribute.type = type;

    switch (type) {
    case TRAttributePointSize: attribute.value.pointSize = value; break;
    case TRAttributeScaleX: attribute.value.scaleX = value; break;
    case TRAttributeScaleY: attribute.value.scaleY = value; break;
    case TRAttributeBaselineOffset: attribute.value.baselineOffset = value; break;
    case TRAttributeObliqueness: attribute.value.obliqueness = value; break;
    case TRAttributeParagraphSpacing: attribute.value.paragraphSpacing = value; break;
    case TRAttributeParagraphSpacingBefore: attribute.value.paragraphSpacingBefore = value; break;
    case TRAttributeFirstLineHeadIndent: attribute.value.firstLineHeadIndent = value; break;
    case TRAttributeHeadIndent: attribute.value.headIndent = value; break;
    case TRAttributeTailIndent: attribute.value.tailIndent = value; break;
    case TRAttributeLineHeightMultiple: attribute.value.lineHeightMultiple = value; break;
    case TRAttributeMinimumLineHeight: attribute.value.minimumLineHeight = value; break;
    case TRAttributeMaximumLineHeight: attribute.value.maximumLineHeight = value; break;
    case TRAttributeLineSpacing: attribute.value.lineSpacing = value; break;
    default: assert(false);
    }

    TRTextSetAttribute(text, index, length, &attribute);
}

}

#endif
