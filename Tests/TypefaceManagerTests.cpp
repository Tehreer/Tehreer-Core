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

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <vector>

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>
#include <Tehreer/TRTypefaceManager.h>

extern "C" {
#include <API/TRTypeface.h>
#include <Core/AtomicUInt.h>
}

#include "TestTypeface.h"

#include "TypefaceManagerTests.h"

using namespace std;
using namespace Tehreer;

constexpr size_t NumThreads = 8;
constexpr size_t Iterations = 200;
constexpr size_t ThreadTypefaces = 4;

namespace {

struct Style {
    TRWidth width;
    TRWeight weight;
    TRSlope slope;
};

struct Spec {
    const char *family;
    const char *subfamily;
};

struct Collected {
    vector<TRUInteger> tags;
    vector<TRUInteger> familyIDs;
    vector<string> names;
};

}

void TypefaceManagerTests::run() {
    testRegisterAndGet();
    testRegisterRejections();
    testUnregister();
    testFamilyGrouping();
    testNameComparison();
    testWidthMatching();
    testSlopeMatching();
    testWeightMatching();
    testMatchingPriority();
    testMatchingTieBreak();
    testVariationMatching();
    testEnumerateTypefaces();
    testEnumerateFamilies();
    testEnumerationStop();
    testReentrantEnumeration();
    testConcurrentAccess();
}

static size_t retainCount(TRTypefaceRef typeface) {
    return AtomicUIntLoad(&typeface->_base.retainCount);
}

static string toString(const TRStringView *view) {
    string text;

    for (size_t i = 0; i < view->length; i++) {
        switch (view->encoding) {
        case TRStringEncodingUTF8:
            text.push_back(static_cast<const char *>(view->buffer)[i]);
            break;

        case TRStringEncodingUTF16:
            text.push_back(static_cast<char>(static_cast<const uint16_t *>(view->buffer)[i]));
            break;

        case TRStringEncodingUTF32:
            text.push_back(static_cast<char>(static_cast<const uint32_t *>(view->buffer)[i]));
            break;
        }
    }

    return text;
}

static TRStringView utf8View(const char *text) {
    TRStringView view = { text, strlen(text), TRStringEncodingUTF8 };
    return view;
}

/* Gives a name that stays valid until the end of the process. */
static const TRStringView *makeName(const char *text) {
    static deque<string> texts;
    static deque<TRStringView> names;
    texts.push_back(text);
    names.push_back(utf8View(texts.back().c_str()));

    return &names.back();
}

static void expectRegistered(TRTypefaceRef typeface, TRUInteger tag, TRUInteger familyID) {
    bool isRegistered = TRTypefaceManagerRegisterTypeface(typeface, tag, familyID);
    assert(isRegistered == true);
    (void)isRegistered;
}

static void expectNotRegistered(TRTypefaceRef typeface, TRUInteger tag, TRUInteger familyID) {
    bool isRegistered = TRTypefaceManagerRegisterTypeface(typeface, tag, familyID);
    assert(isRegistered == false);
    (void)isRegistered;
}

static void expectUnregistered(TRTypefaceRef typeface) {
    bool isUnregistered = TRTypefaceManagerUnregisterTypeface(typeface);
    assert(isUnregistered == true);
    (void)isUnregistered;
}

static void expectNotUnregistered(TRTypefaceRef typeface) {
    bool isUnregistered = TRTypefaceManagerUnregisterTypeface(typeface);
    assert(isUnregistered == false);
    (void)isUnregistered;
}

static TRTypefaceRef makeTypeface(const Style &style, const char *family = nullptr,
    const char *subfamily = nullptr) {
    TRTypefaceRef typeface = createTestTypeface("Roboto-Regular.abc.ttf");
    TRTypeface *mutableTypeface = const_cast<TRTypeface *>(typeface);

    mutableTypeface->width = style.width;
    mutableTypeface->weight = style.weight;
    mutableTypeface->slope = style.slope;

    if (family) {
        mutableTypeface->familyName = makeName(family);
    }
    if (subfamily) {
        mutableTypeface->subfamilyName = makeName(subfamily);
    }

    return typeface;
}

static TRTypefaceRef makeTypeface(TRWidth width, TRWeight weight, TRSlope slope) {
    return makeTypeface(Style { width, weight, slope });
}

/* Unregisters the typeface, if it is registered, and releases the reference of the test. */
static void discard(TRTypefaceRef typeface) {
    TRTypefaceManagerUnregisterTypeface(typeface);
    TRTypefaceRelease(typeface);
}

static void discardAll(const vector<TRTypefaceRef> &typefaces) {
    for (auto typeface : typefaces) {
        discard(typeface);
    }
}

/* Registers a typeface for each style in a family, with the tags starting from the given one. */
static vector<TRTypefaceRef> registerFamily(TRUInteger familyID, TRUInteger firstTag,
    const vector<Style> &styles, const char *family = nullptr) {
    vector<TRTypefaceRef> typefaces;

    for (size_t i = 0; i < styles.size(); i++) {
        TRTypefaceRef typeface = makeTypeface(styles[i], family);
        expectRegistered(typeface, firstTag + i, familyID);

        typefaces.push_back(typeface);
    }

    return typefaces;
}

static TRUInteger matchTag(TRUInteger familyID, TRWidth width, TRWeight weight, TRSlope slope) {
    TRTypefaceRef match = TRTypefaceManagerGetMatchingTypefaceByFamilyID(familyID, width, weight,
        slope);

    return (match ? TRTypefaceManagerGetTypefaceTag(match) : 0);
}

static void collectTypeface(TRTypefaceRef typeface, void *context, TRBoolean *stop) {
    auto *collected = static_cast<Collected *>(context);
    collected->tags.push_back(TRTypefaceManagerGetTypefaceTag(typeface));
    collected->familyIDs.push_back(TRTypefaceManagerGetTypefaceFamilyID(typeface));
    collected->names.push_back(toString(TRTypefaceGetFamilyName(typeface))
                               + "/" + toString(TRTypefaceGetSubfamilyName(typeface)));
}

static void collectFamily(TRUInteger familyID, const TRStringView *familyName, void *context,
    TRBoolean *stop) {
    auto *collected = static_cast<Collected *>(context);
    collected->familyIDs.push_back(familyID);
    collected->names.push_back(toString(familyName));
}

void TypefaceManagerTests::testRegisterAndGet() {
    TRTypefaceRef typeface = makeTypeface(TRWidthNormal, TRWeightRegular, TRSlopePlain);
    assert(retainCount(typeface) == 1);

    expectRegistered(typeface, 11, 21);
    assert(retainCount(typeface) == 2);

    /* The lookups do not retain. */
    assert(TRTypefaceManagerGetTypeface(11) == typeface);
    assert(retainCount(typeface) == 2);
    assert(TRTypefaceManagerGetTypefaceTag(typeface) == 11);
    assert(TRTypefaceManagerGetTypefaceFamilyID(typeface) == 21);

    assert(TRTypefaceManagerGetTypeface(12) == nullptr);
    assert(TRTypefaceManagerGetTypeface(0) == nullptr);

    /* A typeface that is not registered has nothing. */
    TRTypefaceRef other = makeTypeface(TRWidthNormal, TRWeightBold, TRSlopePlain);
    assert(TRTypefaceManagerGetTypefaceTag(other) == 0);
    assert(TRTypefaceManagerGetTypefaceFamilyID(other) == 0);
    TRTypefaceRelease(other);

    /* A typeface without a tag or an ID reports zeros. */
    TRTypefaceRef untagged = makeTypeface(TRWidthNormal, TRWeightBold, TRSlopePlain);
    expectRegistered(untagged, 0, 0);
    assert(TRTypefaceManagerGetTypefaceTag(untagged) == 0);
    assert(TRTypefaceManagerGetTypefaceFamilyID(untagged) == 0);

    discard(untagged);
    discard(typeface);
}

void TypefaceManagerTests::testRegisterRejections() {
    TRTypefaceRef first = makeTypeface(TRWidthNormal, TRWeightRegular, TRSlopePlain);
    TRTypefaceRef second = makeTypeface(TRWidthNormal, TRWeightBold, TRSlopePlain);

    expectNotRegistered(nullptr, 31, 0);
    assert(TRTypefaceManagerGetTypeface(31) == nullptr);

    expectRegistered(first, 31, 0);
    assert(retainCount(first) == 2);

    /* The same typeface cannot be registered twice. */
    expectNotRegistered(first, 32, 0);
    assert(TRTypefaceManagerGetTypeface(32) == nullptr);
    assert(TRTypefaceManagerGetTypefaceTag(first) == 31);
    assert(retainCount(first) == 2);

    /* A tag that is taken cannot be used again. */
    expectNotRegistered(second, 31, 0);
    assert(TRTypefaceManagerGetTypeface(31) == first);
    assert(TRTypefaceManagerGetTypefaceTag(second) == 0);
    assert(retainCount(second) == 1);

    /* The tag 0 means none, so it can be used by many typefaces. */
    expectRegistered(second, 0, 0);
    TRTypefaceRef third = makeTypeface(TRWidthNormal, TRWeightLight, TRSlopePlain);
    expectRegistered(third, 0, 0);
    assert(TRTypefaceManagerGetTypeface(0) == nullptr);

    discard(third);
    discard(second);
    discard(first);
}

void TypefaceManagerTests::testUnregister() {
    TRTypefaceRef typeface = makeTypeface(TRWidthNormal, TRWeightRegular, TRSlopePlain);
    expectRegistered(typeface, 41, 51);
    assert(retainCount(typeface) == 2);

    expectUnregistered(typeface);
    assert(retainCount(typeface) == 1);
    assert(TRTypefaceManagerGetTypeface(41) == nullptr);
    assert(TRTypefaceManagerGetTypefaceTag(typeface) == 0);
    assert(TRTypefaceManagerGetTypefaceFamilyID(typeface) == 0);
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyID(51, TRWidthNormal, TRWeightRegular,
        TRSlopePlain) == nullptr);

    expectNotUnregistered(typeface);
    expectNotUnregistered(nullptr);
    assert(retainCount(typeface) == 1);

    /* The tag is free again, and so is the typeface. */
    TRTypefaceRef other = makeTypeface(TRWidthNormal, TRWeightBold, TRSlopePlain);
    expectRegistered(other, 41, 0);
    expectRegistered(typeface, 42, 0);
    assert(TRTypefaceManagerGetTypeface(41) == other);
    assert(TRTypefaceManagerGetTypeface(42) == typeface);

    /* The typeface lives on if the owner releases it while it is registered. */
    TRTypefaceRetain(typeface);
    TRTypefaceRelease(typeface);
    TRTypefaceRelease(typeface);
    assert(retainCount(TRTypefaceManagerGetTypeface(42)) == 1);
    expectUnregistered(TRTypefaceManagerGetTypeface(42));

    discard(other);
}

void TypefaceManagerTests::testFamilyGrouping() {
    /* Explicit IDs group typefaces whatever their names are. */
    TRTypefaceRef idRegular = makeTypeface(Style { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        "Sample", "Regular");
    TRTypefaceRef idBold = makeTypeface(Style { TRWidthNormal, TRWeightBold, TRSlopePlain },
        "Other", "Bold");
    expectRegistered(idRegular, 61, 100);
    expectRegistered(idBold, 62, 100);

    /* Without an ID, the font family name decides, ignoring the case. */
    TRTypefaceRef nameRegular = makeTypeface(Style { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        "Sample", "Regular");
    TRTypefaceRef nameBold = makeTypeface(Style { TRWidthNormal, TRWeightBold, TRSlopePlain },
        "SAMPLE", "Bold");
    TRTypefaceRef nameOther = makeTypeface(Style { TRWidthNormal, TRWeightBold, TRSlopePlain },
        "Different", "Bold");
    expectRegistered(nameRegular, 63, 0);
    expectRegistered(nameBold, 64, 0);
    expectRegistered(nameOther, 65, 0);

    assert(matchTag(100, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 61);
    assert(matchTag(100, TRWidthNormal, TRWeightBold, TRSlopePlain) == 62);
    assert(matchTag(101, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 0);
    assert(matchTag(0, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 0);

    /* The name lookup ignores the typefaces that have an ID. */
    TRStringView sample = utf8View("sample");
    TRTypefaceRef match = TRTypefaceManagerGetMatchingTypefaceByFamilyName(&sample, TRWidthNormal,
        TRWeightBold, TRSlopePlain);
    assert(match == nameBold);

    match = TRTypefaceManagerGetMatchingTypefaceByFamilyName(&sample, TRWidthNormal,
        TRWeightRegular, TRSlopePlain);
    assert(match == nameRegular);

    TRStringView different = utf8View("DIFFERENT");
    match = TRTypefaceManagerGetMatchingTypefaceByFamilyName(&different, TRWidthNormal,
        TRWeightRegular, TRSlopePlain);
    assert(match == nameOther);

    TRStringView other = utf8View("Other");
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&other, TRWidthNormal,
        TRWeightBold, TRSlopePlain) == nullptr);

    TRStringView unknown = utf8View("Unknown");
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&unknown, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(nullptr, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    /* Unregistering the typefaces leaves the family empty. */
    discard(nameOther);
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&different, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    discard(nameBold);
    discard(nameRegular);
    discard(idBold);
    discard(idRegular);
}

void TypefaceManagerTests::testNameComparison() {
    TRTypefaceRef typeface = makeTypeface(Style { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        "Sample Family", "Regular");
    expectRegistered(typeface, 71, 0);

    const char *utf8 = "sAMPLE fAMILY";
    const uint16_t utf16[] = { 'S', 'a', 'm', 'p', 'l', 'e', ' ', 'F', 'A', 'M', 'I', 'L', 'Y' };
    const uint32_t utf32[] = { 's', 'a', 'm', 'p', 'l', 'e', ' ', 'f', 'a', 'm', 'i', 'l', 'y' };

    TRStringView views[] = {
        { utf8, 13, TRStringEncodingUTF8 },
        { utf16, 13, TRStringEncodingUTF16 },
        { utf32, 13, TRStringEncodingUTF32 }
    };

    for (const auto &view : views) {
        assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&view, TRWidthNormal,
            TRWeightRegular, TRSlopePlain) == typeface);
    }

    /* A prefix or an extension of the name is not the name. */
    TRStringView shorter = { utf8, 12, TRStringEncodingUTF8 };
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&shorter, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    const char *longer = "Sample Familys";
    TRStringView longerView = { longer, 14, TRStringEncodingUTF8 };
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&longerView, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    /* A name that is not ASCII is compared exactly. */
    TRTypefaceRef accented = makeTypeface(Style { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        "\xC3\x89""cole", "Regular");
    expectRegistered(accented, 72, 0);

    TRStringView exact = utf8View("\xC3\x89""cole");
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&exact, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == accented);

    const uint16_t accentedUnits[] = { 0x00C9, 'c', 'o', 'l', 'e' };
    TRStringView exact16 = { accentedUnits, 5, TRStringEncodingUTF16 };
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&exact16, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == accented);

    /* The case of a letter that is not ASCII is not folded. */
    TRStringView lower = utf8View("\xC3\xA9""cole");
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&lower, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    const uint16_t lowerUnits[] = { 0x00E9, 'c', 'o', 'l', 'e' };
    TRStringView lower16 = { lowerUnits, 5, TRStringEncodingUTF16 };
    assert(TRTypefaceManagerGetMatchingTypefaceByFamilyName(&lower16, TRWidthNormal,
        TRWeightRegular, TRSlopePlain) == nullptr);

    discard(accented);
    discard(typeface);
}

void TypefaceManagerTests::testWidthMatching() {
    struct Case {
        vector<TRWidth> widths;
        TRWidth desired;
        TRWidth expected;
    };

    const vector<Case> cases = {
        /* An exact width always wins. */
        { { 3, 5, 7 }, 5, 5 },
        { { 3, 5, 7 }, 7, 7 },
        /* Normal or narrower prefers the closest narrower, then the closest wider. */
        { { 3, 4, 7 }, 5, 4 },
        { { 7, 8 }, 5, 7 },
        { { 2, 3, 8 }, 4, 3 },
        { { 4, 3 }, 1, 3 },
        { { 5, 6 }, 1, 5 },
        /* Wider than normal prefers the closest wider, then the closest narrower. */
        { { 3, 8, 9 }, 6, 8 },
        { { 3, 5 }, 7, 5 },
        { { 3, 4, 5 }, 9, 5 },
        { { 5, 6, 8, 9 }, 7, 8 },
        { { 7, 8 }, 9, 8 }
    };

    for (const auto &item : cases) {
        vector<Style> styles;
        for (auto width : item.widths) {
            styles.push_back(Style { width, TRWeightRegular, TRSlopePlain });
        }

        auto typefaces = registerFamily(200, 300, styles);
        TRUInteger tag = matchTag(200, item.desired, TRWeightRegular, TRSlopePlain);
        assert(tag != 0);
        assert(item.widths[tag - 300] == item.expected);

        discardAll(typefaces);
    }
}

void TypefaceManagerTests::testSlopeMatching() {
    struct Case {
        vector<TRSlope> slopes;
        TRSlope desired;
        TRSlope expected;
    };

    const vector<Case> cases = {
        { { TRSlopePlain, TRSlopeItalic, TRSlopeOblique }, TRSlopeItalic, TRSlopeItalic },
        { { TRSlopePlain, TRSlopeItalic, TRSlopeOblique }, TRSlopeOblique, TRSlopeOblique },
        { { TRSlopePlain, TRSlopeItalic, TRSlopeOblique }, TRSlopePlain, TRSlopePlain },
        /* Italic tries oblique and then plain. */
        { { TRSlopePlain, TRSlopeOblique }, TRSlopeItalic, TRSlopeOblique },
        { { TRSlopePlain }, TRSlopeItalic, TRSlopePlain },
        /* Oblique tries italic and then plain. */
        { { TRSlopePlain, TRSlopeItalic }, TRSlopeOblique, TRSlopeItalic },
        { { TRSlopePlain }, TRSlopeOblique, TRSlopePlain },
        /* Plain tries oblique and then italic. */
        { { TRSlopeItalic, TRSlopeOblique }, TRSlopePlain, TRSlopeOblique },
        { { TRSlopeItalic }, TRSlopePlain, TRSlopeItalic }
    };

    for (const auto &item : cases) {
        vector<Style> styles;
        for (auto slope : item.slopes) {
            styles.push_back(Style { TRWidthNormal, TRWeightRegular, slope });
        }

        auto typefaces = registerFamily(210, 310, styles);
        TRUInteger tag = matchTag(210, TRWidthNormal, TRWeightRegular, item.desired);
        assert(tag != 0);
        assert(item.slopes[tag - 310] == item.expected);

        discardAll(typefaces);
    }
}

void TypefaceManagerTests::testWeightMatching() {
    struct Case {
        vector<TRWeight> weights;
        TRWeight desired;
        TRWeight expected;
    };

    const vector<Case> cases = {
        /* An exact weight always wins. */
        { { 300, 400, 500, 700 }, 400, 400 },
        { { 300, 400, 500, 700 }, 500, 500 },
        { { 300, 400, 500, 700 }, 700, 700 },
        { { 100, 300 }, 300, 300 },
        /* Regular goes up to medium, then down, then above medium. */
        { { 300, 450, 600 }, 400, 450 },
        { { 300, 500, 600 }, 400, 500 },
        { { 200, 300, 600 }, 400, 300 },
        { { 600, 800 }, 400, 600 },
        /* Medium goes up to medium, then down, then above medium. */
        { { 300, 450, 600 }, 500, 450 },
        { { 200, 400, 600 }, 450, 400 },
        { { 300, 600, 700 }, 500, 300 },
        { { 600, 700 }, 500, 600 },
        /* Lighter than regular goes down, then up. */
        { { 100, 200, 400, 500 }, 300, 200 },
        { { 400, 500, 700 }, 300, 400 },
        { { 300, 700 }, 200, 300 },
        { { 100, 700 }, 200, 100 },
        /* Heavier than medium goes up, then down. */
        { { 600, 800, 900 }, 700, 800 },
        { { 400, 600 }, 700, 600 },
        { { 300, 400, 500 }, 800, 500 },
        { { 300, 600, 650 }, 900, 650 }
    };

    for (const auto &item : cases) {
        vector<Style> styles;
        for (auto weight : item.weights) {
            styles.push_back(Style { TRWidthNormal, weight, TRSlopePlain });
        }

        auto typefaces = registerFamily(220, 320, styles);
        TRUInteger tag = matchTag(220, TRWidthNormal, item.desired, TRSlopePlain);
        assert(tag != 0);
        assert(item.weights[tag - 320] == item.expected);

        discardAll(typefaces);
    }
}

void TypefaceManagerTests::testMatchingPriority() {
    /* The width comes first, then the slope, and the weight last. */
    vector<Style> styles = {
        { TRWidthCondensed, TRWeightBold, TRSlopeItalic },      /* 400 */
        { TRWidthNormal, TRWeightRegular, TRSlopePlain },       /* 401 */
        { TRWidthNormal, TRWeightBold, TRSlopeItalic },         /* 402 */
        { TRWidthNormal, TRWeightLight, TRSlopeItalic }         /* 403 */
    };
    auto typefaces = registerFamily(230, 400, styles);

    assert(matchTag(230, TRWidthNormal, TRWeightBold, TRSlopePlain) == 401);
    assert(matchTag(230, TRWidthNormal, TRWeightBold, TRSlopeItalic) == 402);
    assert(matchTag(230, TRWidthNormal, TRWeightLight, TRSlopeItalic) == 403);
    assert(matchTag(230, TRWidthCondensed, TRWeightRegular, TRSlopePlain) == 400);
    assert(matchTag(230, TRWidthExpanded, TRWeightBold, TRSlopeItalic) == 402);

    discardAll(typefaces);
}

void TypefaceManagerTests::testMatchingTieBreak() {
    const vector<Style> styles = {
        { TRWidthNormal, TRWeightBold, TRSlopePlain },
        { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        { TRWidthNormal, TRWeightRegular, TRSlopePlain },
        { TRWidthNormal, TRWeightBold, TRSlopePlain }
    };

    /* The first one that was registered wins, whatever the names say. */
    vector<TRTypefaceRef> typefaces;
    const char *subfamilies[] = { "Zebra", "Yak", "Apple", "Mango" };

    for (size_t i = 0; i < styles.size(); i++) {
        TRTypefaceRef typeface = makeTypeface(styles[i], "Tie", subfamilies[i]);
        expectRegistered(typeface, 500 + i, 240);
        typefaces.push_back(typeface);
    }

    assert(matchTag(240, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 501);
    assert(matchTag(240, TRWidthNormal, TRWeightBold, TRSlopePlain) == 500);

    /* The next one takes over when the first goes away. */
    expectUnregistered(typefaces[1]);
    assert(matchTag(240, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 502);

    discardAll(typefaces);
}

void TypefaceManagerTests::testVariationMatching() {
    TRTypefaceRef base = createTestTypeface("Roboto-Variable.abc.ttf");

    /* The variation coordinates are the weight and the width. */
    const TRFloat lightCondensed[] = { 300.0f, 75.0f };
    const TRFloat boldNormal[] = { 700.0f, 100.0f };
    const TRFloat boldCondensed[] = { 700.0f, 75.0f };

    TRTypefaceRef light = TRTypefaceCreateWithVariation(base, lightCondensed, 2);
    TRTypefaceRef bold = TRTypefaceCreateWithVariation(base, boldNormal, 2);
    TRTypefaceRef condensed = TRTypefaceCreateWithVariation(base, boldCondensed, 2);
    assert(light != nullptr && bold != nullptr && condensed != nullptr);

    /* The derived typefaces report their own traits. */
    assert(TRTypefaceGetWeight(light) == TRWeightLight);
    assert(TRTypefaceGetWidth(light) == TRWidthCondensed);
    assert(TRTypefaceGetWeight(bold) == TRWeightBold);
    assert(TRTypefaceGetWidth(bold) == TRWidthNormal);
    assert(TRTypefaceGetWidth(condensed) == TRWidthCondensed);

    expectRegistered(base, 600, 250);
    expectRegistered(light, 601, 250);
    expectRegistered(bold, 602, 250);
    expectRegistered(condensed, 603, 250);

    assert(matchTag(250, TRWidthNormal, TRWeightRegular, TRSlopePlain) == 600);
    assert(matchTag(250, TRWidthNormal, TRWeightBold, TRSlopePlain) == 602);
    assert(matchTag(250, TRWidthCondensed, TRWeightLight, TRSlopePlain) == 601);
    assert(matchTag(250, TRWidthCondensed, TRWeightBold, TRSlopePlain) == 603);

    /* Nothing is wider, so the closest narrower width stays, and then the closest weight. */
    assert(matchTag(250, TRWidthExpanded, TRWeightLight, TRSlopePlain) == 600);

    discard(condensed);
    discard(bold);
    discard(light);
    discard(base);
}

void TypefaceManagerTests::testEnumerateTypefaces() {
    Collected empty;
    TRTypefaceManagerEnumerateTypefaces(collectTypeface, &empty);
    assert(empty.tags.empty());

    /* The order is by the family name and then by the style name, ignoring the case. */
    const Style style = { TRWidthNormal, TRWeightRegular, TRSlopePlain };
    const vector<Spec> specs = {
        { "banana", "Regular" },
        { "Apple", "regular" },
        { "Apple", "Bold" },
        { "apple", "Italic" },
        { "Cherry", "Bold" }
    };
    vector<TRTypefaceRef> typefaces;

    for (size_t i = 0; i < specs.size(); i++) {
        TRTypefaceRef typeface = makeTypeface(style, specs[i].family, specs[i].subfamily);
        expectRegistered(typeface, 700 + i, i % 2);
        typefaces.push_back(typeface);
    }

    Collected collected;
    TRTypefaceManagerEnumerateTypefaces(collectTypeface, &collected);

    const vector<TRUInteger> expectedTags = { 702, 703, 701, 700, 704 };
    const vector<string> expectedNames = {
        "Apple/Bold", "apple/Italic", "Apple/regular", "banana/Regular", "Cherry/Bold"
    };
    assert(collected.tags == expectedTags);
    assert(collected.names == expectedNames);
    assert(collected.familyIDs == (vector<TRUInteger> { 0, 1, 1, 0, 0 }));

    /* The snapshot does not keep the typefaces after the enumeration. */
    for (auto typeface : typefaces) {
        assert(retainCount(typeface) == 2);
    }

    TRTypefaceManagerEnumerateTypefaces(nullptr, nullptr);
    discardAll(typefaces);

    Collected cleared;
    TRTypefaceManagerEnumerateTypefaces(collectTypeface, &cleared);
    assert(cleared.tags.empty());
}

void TypefaceManagerTests::testEnumerateFamilies() {
    Collected empty;
    TRTypefaceManagerEnumerateFamilies(collectFamily, &empty);
    assert(empty.names.empty());

    const Style style = { TRWidthNormal, TRWeightRegular, TRSlopePlain };
    struct Item {
        const char *family;
        const char *subfamily;
        TRUInteger familyID;
    };
    const vector<Item> items = {
        { "Zulu", "Regular", 7 },
        { "Yankee", "Bold", 7 },
        { "mike", "Regular", 0 },
        { "MIKE", "Bold", 0 },
        { "Mike", "Italic", 9 },
        { "Alpha", "Regular", 0 }
    };
    vector<TRTypefaceRef> typefaces;

    for (size_t i = 0; i < items.size(); i++) {
        TRTypefaceRef typeface = makeTypeface(style, items[i].family, items[i].subfamily);
        expectRegistered(typeface, 800 + i, items[i].familyID);
        typefaces.push_back(typeface);
    }

    /*
     * The families with an ID are named after their first typeface, and the typefaces without an
     * ID are grouped by their names.
     */
    Collected collected;
    TRTypefaceManagerEnumerateFamilies(collectFamily, &collected);

    const vector<string> expectedNames = { "Alpha", "MIKE", "Mike", "Yankee" };
    const vector<TRUInteger> expectedIDs = { 0, 0, 9, 7 };
    assert(collected.names == expectedNames);
    assert(collected.familyIDs == expectedIDs);

    TRTypefaceManagerEnumerateFamilies(nullptr, nullptr);
    discardAll(typefaces);
}

void TypefaceManagerTests::testEnumerationStop() {
    const Style style = { TRWidthNormal, TRWeightRegular, TRSlopePlain };
    const char *names[] = { "A", "B", "C", "D" };
    vector<TRTypefaceRef> typefaces;

    for (size_t i = 0; i < 4; i++) {
        TRTypefaceRef typeface = makeTypeface(style, names[i], "Regular");
        expectRegistered(typeface, 900 + i, 0);
        typefaces.push_back(typeface);
    }

    struct Counter {
        size_t calls;
        size_t limit;
    } counter = { 0, 2 };

    TRTypefaceManagerEnumerateTypefaces([](TRTypefaceRef, void *context, TRBoolean *stop) {
        auto *counter = static_cast<Counter *>(context);
        assert(*stop == false);

        counter->calls += 1;
        if (counter->calls == counter->limit) {
            *stop = true;
        }
    }, &counter);
    assert(counter.calls == 2);

    counter = { 0, 1 };
    TRTypefaceManagerEnumerateFamilies([](TRUInteger, const TRStringView *, void *context,
        TRBoolean *stop) {
        auto *counter = static_cast<Counter *>(context);
        assert(*stop == false);

        counter->calls += 1;
        if (counter->calls == counter->limit) {
            *stop = true;
        }
    }, &counter);
    assert(counter.calls == 1);

    /* The stopped enumeration released everything. */
    for (auto typeface : typefaces) {
        assert(retainCount(typeface) == 2);
    }

    discardAll(typefaces);
}

void TypefaceManagerTests::testReentrantEnumeration() {
    const Style style = { TRWidthNormal, TRWeightBold, TRSlopePlain };
    vector<TRTypefaceRef> typefaces;

    for (size_t i = 0; i < 3; i++) {
        TRTypefaceRef typeface = makeTypeface(style, "Reentrant", i == 0 ? "A" : i == 1 ? "B" : "C");
        expectRegistered(typeface, 1000 + i, 1100);
        typefaces.push_back(typeface);
    }

    /* The callback may use the manager, even to unregister the typeface that it is given. */
    size_t calls = 0;
    TRTypefaceManagerEnumerateTypefaces([](TRTypefaceRef typeface, void *context, TRBoolean *) {
        auto *calls = static_cast<size_t *>(context);
        *calls += 1;

        TRUInteger tag = TRTypefaceManagerGetTypefaceTag(typeface);
        assert(tag >= 1000 && tag <= 1002);
        assert(TRTypefaceManagerGetTypeface(tag) == typeface);

        Collected nested;
        TRTypefaceManagerEnumerateFamilies(collectFamily, &nested);
        assert(nested.names.size() == 1);

        expectUnregistered(typeface);

        /* The snapshot keeps it alive. */
        assert(TRTypefaceGetWeight(typeface) == TRWeightBold);
        assert(retainCount(typeface) == 2);
    }, &calls);
    assert(calls == 3);

    Collected collected;
    TRTypefaceManagerEnumerateTypefaces(collectTypeface, &collected);
    assert(collected.tags.empty());

    for (auto typeface : typefaces) {
        assert(retainCount(typeface) == 1);
        TRTypefaceRelease(typeface);
    }
}

void TypefaceManagerTests::testConcurrentAccess() {
    const Style style = { TRWidthNormal, TRWeightRegular, TRSlopePlain };
    constexpr TRUInteger SharedFamilyID = 2000;

    vector<vector<TRTypefaceRef>> threadTypefaces(NumThreads);
    atomic<size_t> failures(0);

    for (size_t i = 0; i < NumThreads; i++) {
        for (size_t j = 0; j < ThreadTypefaces; j++) {
            Style variant = style;
            variant.weight = static_cast<TRWeight>((j + 1) * 100);
            string family = (j == 3 ? "Concurrent" + to_string(i) : "Concurrent");
            threadTypefaces[i].push_back(makeTypeface(variant, family.c_str(), "Style"));
        }
    }

    vector<thread> threads;

    for (size_t i = 0; i < NumThreads; i++) {
        threads.emplace_back([&, i]() {
            const TRUInteger ownFamilyID = 3000 + i;
            const TRUInteger firstTag = 10000 + i * 100;
            const string ownName = "concurrent" + to_string(i);
            const TRStringView ownFamilyName = utf8View(ownName.c_str());
            const TRStringView sharedName = utf8View("concurrent");
            const auto &typefaces = threadTypefaces[i];

            for (size_t iteration = 0; iteration < Iterations; iteration++) {
                for (size_t j = 0; j < typefaces.size(); j++) {
                    TRUInteger familyID = (j == 3 ? 0 : (j % 2 == 0 ? ownFamilyID : SharedFamilyID));
                    if (!TRTypefaceManagerRegisterTypeface(typefaces[j], firstTag + j, familyID)) {
                        failures++;
                    }
                }

                for (size_t j = 0; j < typefaces.size(); j++) {
                    if (TRTypefaceManagerGetTypeface(firstTag + j) != typefaces[j]) {
                        failures++;
                    }
                    if (TRTypefaceManagerGetTypefaceTag(typefaces[j]) != firstTag + j) {
                        failures++;
                    }
                }

                /* The own family has two typefaces of this thread only. */
                TRTypefaceRef match = TRTypefaceManagerGetMatchingTypefaceByFamilyID(ownFamilyID,
                    TRWidthNormal, 300, TRSlopePlain);
                if (match != typefaces[2]) {
                    failures++;
                }

                /* Only the last typeface of this thread has a family without an ID. */
                match = TRTypefaceManagerGetMatchingTypefaceByFamilyName(&ownFamilyName,
                    TRWidthNormal, TRWeightBold, TRSlopePlain);
                if (match != typefaces[3]) {
                    failures++;
                }

                /* The shared family changes all the time, so its result is not looked at. */
                TRTypefaceManagerGetMatchingTypefaceByFamilyID(SharedFamilyID, TRWidthNormal,
                    TRWeightBold, TRSlopePlain);
                TRTypefaceManagerGetMatchingTypefaceByFamilyName(&sharedName, TRWidthNormal,
                    TRWeightBold, TRSlopePlain);

                atomic<size_t> count(0);
                TRTypefaceManagerEnumerateTypefaces([](TRTypefaceRef typeface, void *context,
                    TRBoolean *) {
                    static_cast<atomic<size_t> *>(context)->fetch_add(1);
                    assert(TRTypefaceGetWeight(typeface) > 0);
                }, &count);
                if (count.load() < typefaces.size()) {
                    failures++;
                }

                TRTypefaceManagerEnumerateFamilies([](TRUInteger, const TRStringView *name,
                    void *context, TRBoolean *) {
                    static_cast<atomic<size_t> *>(context)->fetch_add(1);
                    assert(name != nullptr);
                }, &count);

                for (auto typeface : typefaces) {
                    if (!TRTypefaceManagerUnregisterTypeface(typeface)) {
                        failures++;
                    }
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    assert(failures.load() == 0);

    Collected collected;
    TRTypefaceManagerEnumerateTypefaces(collectTypeface, &collected);
    assert(collected.tags.empty());

    for (const auto &typefaces : threadTypefaces) {
        for (auto typeface : typefaces) {
            assert(retainCount(typeface) == 1);
            TRTypefaceRelease(typeface);
        }
    }
}

#ifdef STANDALONE_TESTING

int main() {
    TypefaceManagerTests tests;
    tests.run();

    return 0;
}

#endif
