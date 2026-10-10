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
#include <string.h>

#include <SheenBidi/SBCodepoint.h>

#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>
#include <Tehreer/TRTypefaceManager.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <Core/Array.h>
#include <Core/Mutex.h>
#include <Core/Once.h>

#include "TRTypefaceManager.h"

#define KeyMax          0x7FFFFFFF
#define MaxStepCount    4

enum {
    CandidateKeyWidth = 0,
    CandidateKeySlope = 1,
    CandidateKeyWeight = 2
};
typedef TRUInt32 CandidateKey;

/*
 * A step of the matching algorithm: the keys from low to high, both included, from which the
 * lowest or the highest one is taken.
 */
typedef struct _MatchStep {
    TRInt32 low;
    TRInt32 high;
    TRBoolean picksHighest;
} MatchStep;

static const TRStringView EmptyName = { NULL, 0, TRStringEncodingUTF8 };

static TRTypefaceManager DefaultManager;

/* ---------- Manager ---------- */

static void InitDefaultManager(void)
{
    MutexInit(&DefaultManager.mutex);
    ArrayInitialize(&DefaultManager.entries, sizeof(TypefaceEntry));
}

static TRTypefaceManager *GetDefaultManager(void)
{
    static Once once = OnceMake();

    OnceExecute(&once, InitDefaultManager);

    return &DefaultManager;
}

/* ---------- Names ---------- */

static TRUInt32 DecodeFoldedCodePoint(const TRStringView *name, TRUInteger *index)
{
    TRUInt32 codePoint = SBCodepointFaulty;

    /* Index MUST be less than the length. */
    TRAssert(*index < name->length);

    switch (name->encoding) {
    case TRStringEncodingUTF8:
        codePoint = SBCodepointDecodeNextFromUTF8(name->buffer, name->length, index);
        break;

    case TRStringEncodingUTF16:
        codePoint = SBCodepointDecodeNextFromUTF16(name->buffer, name->length, index);
        break;

    case TRStringEncodingUTF32:
        codePoint = ((const TRUInt32 *)name->buffer)[(*index)++];
        break;

    default:
        (*index)++;
        break;
    }

    /* Only the ASCII letters are folded. */
    if (codePoint >= 'A' && codePoint <= 'Z') {
        codePoint += 'a' - 'A';
    }

    return codePoint;
}

static int CompareNames(const TRStringView *first, const TRStringView *second)
{
    int order = 0;
    TRUInteger firstIndex = 0;
    TRUInteger secondIndex = 0;

    while (firstIndex < first->length && secondIndex < second->length) {
        TRUInt32 firstCodePoint = DecodeFoldedCodePoint(first, &firstIndex);
        TRUInt32 secondCodePoint = DecodeFoldedCodePoint(second, &secondIndex);

        if (firstCodePoint != secondCodePoint) {
            order = (firstCodePoint < secondCodePoint ? -1 : 1);
            break;
        }
    }

    if (order == 0) {
        if (firstIndex < first->length) {
            order = 1;
        } else if (secondIndex < second->length) {
            order = -1;
        }
    }

    return order;
}

static const TRStringView *GetFamilyName(TRTypefaceRef typeface)
{
    const TRStringView *familyName = TRTypefaceGetFamilyName(typeface);

    return (familyName ? familyName : &EmptyName);
}

static const TRStringView *GetSubfamilyName(TRTypefaceRef typeface)
{
    const TRStringView *subfamilyName = TRTypefaceGetSubfamilyName(typeface);

    return (subfamilyName ? subfamilyName : &EmptyName);
}

/* ---------- Entries ---------- */

static TRUInteger FindEntryIndexByTypeface(const Array *entries, TRTypefaceRef typeface)
{
    TRUInteger entryIndex = TRInvalidIndex;
    TRUInteger entryCount = ArrayGetCount(entries);
    TRUInteger index;

    for (index = 0; index < entryCount; index++) {
        const TypefaceEntry *entry = ArrayGetItem(entries, index);

        if (entry->typeface == typeface) {
            entryIndex = index;
            break;
        }
    }

    return entryIndex;
}

static TRUInteger FindEntryIndexByTag(const Array *entries, TRUInteger tag)
{
    TRUInteger entryIndex = TRInvalidIndex;
    TRUInteger entryCount = ArrayGetCount(entries);
    TRUInteger index;

    for (index = 0; index < entryCount; index++) {
        const TypefaceEntry *entry = ArrayGetItem(entries, index);

        if (entry->tag == tag) {
            entryIndex = index;
            break;
        }
    }

    return entryIndex;
}

static void RemoveTypefaceEntry(Array *entries, TRUInteger entryIndex)
{
    TRUInteger entryCount = ArrayGetCount(entries);
    TRUInteger lastIndex = entryCount - 1;
    TypefaceEntry *items = ArrayGetItems(entries);

    memmove(&items[entryIndex], &items[entryIndex + 1],
        (lastIndex - entryIndex) * sizeof(TypefaceEntry));
    ArrayResize(entries, lastIndex);
}

/*
 * With a family ID, the entry must have the same ID. Without one, the entry must also have no ID
 * and the same family name.
 */
static TRBoolean EntryIsInFamily(const TypefaceEntry *entry, TRUInteger familyID,
    const TRStringView *familyName)
{
    TRBoolean isInFamily;

    if (familyID != 0) {
        isInFamily = (entry->familyID == familyID);
    } else {
        isInFamily = (entry->familyID == 0
                      && CompareNames(GetFamilyName(entry->typeface), familyName) == 0);
    }

    return isInFamily;
}

/* ---------- Matching ---------- */

static TRInt32 GetCandidateKey(TRTypefaceRef typeface, CandidateKey kind)
{
    TRInt32 key = 0;

    switch (kind) {
    case CandidateKeyWidth:
        key = (TRInt32)TRTypefaceGetWidth(typeface);
        break;

    case CandidateKeySlope:
        key = (TRInt32)TRTypefaceGetSlope(typeface);
        break;

    case CandidateKeyWeight:
        key = (TRInt32)TRTypefaceGetWeight(typeface);
        break;
    }

    return key;
}

static void SetStep(MatchStep *step, TRInt32 low, TRInt32 high, TRBoolean picksHighest)
{
    step->low = low;
    step->high = high;
    step->picksHighest = picksHighest;
}

/* The width is matched first, preferring the narrower ones for a normal or narrower request. */
static TRUInteger MakeWidthSteps(MatchStep *steps, TRWidth width)
{
    TRInt32 desired = (TRInt32)width;
    MatchStep *narrower = &steps[1];
    MatchStep *wider = &steps[2];

    if (desired > TRWidthNormal) {
        narrower = &steps[2];
        wider = &steps[1];
    }

    SetStep(&steps[0], desired, desired, TRFalse);
    SetStep(narrower, 0, desired - 1, TRTrue);
    SetStep(wider, desired + 1, KeyMax, TRFalse);

    return 3;
}

static TRUInteger MakeSlopeSteps(MatchStep *steps, TRSlope slope)
{
    TRInt32 first = TRSlopePlain;
    TRInt32 second = TRSlopeOblique;
    TRInt32 third = TRSlopeItalic;

    if (slope == TRSlopeItalic) {
        first = TRSlopeItalic;
        second = TRSlopeOblique;
        third = TRSlopePlain;
    } else if (slope == TRSlopeOblique) {
        first = TRSlopeOblique;
        second = TRSlopeItalic;
        third = TRSlopePlain;
    }

    SetStep(&steps[0], first, first, TRFalse);
    SetStep(&steps[1], second, second, TRFalse);
    SetStep(&steps[2], third, third, TRFalse);

    return 3;
}

static TRUInteger MakeWeightSteps(MatchStep *steps, TRWeight weight)
{
    TRInt32 desired = (TRInt32)weight;
    TRUInteger stepCount;

    if (desired >= TRWeightRegular && desired <= TRWeightMedium) {
        /* The exact weight, up to medium, below the desired weight, and then above medium. */
        SetStep(&steps[0], desired, desired, TRFalse);
        SetStep(&steps[1], desired + 1, TRWeightMedium, TRFalse);
        SetStep(&steps[2], 0, desired - 1, TRTrue);
        SetStep(&steps[3], TRWeightMedium + 1, KeyMax, TRFalse);
        stepCount = 4;
    } else if (desired < TRWeightRegular) {
        SetStep(&steps[0], 0, desired, TRTrue);
        SetStep(&steps[1], desired + 1, KeyMax, TRFalse);
        stepCount = 2;
    } else {
        SetStep(&steps[0], desired, KeyMax, TRFalse);
        SetStep(&steps[1], 0, desired - 1, TRTrue);
        stepCount = 2;
    }

    return stepCount;
}

static TRBoolean FindStepKey(const Array *candidates, CandidateKey kind, const MatchStep *step,
    TRInt32 *stepKey)
{
    TRBoolean isFound = TRFalse;
    TRUInteger candidateCount = ArrayGetCount(candidates);
    TRUInteger index;

    for (index = 0; index < candidateCount; index++) {
        const TRTypefaceRef *typeface = ArrayGetItem(candidates, index);
        TRInt32 key = GetCandidateKey(*typeface, kind);

        if (key >= step->low && key <= step->high) {
            TRInt32 bestKey = *stepKey;

            if (!isFound) {
                bestKey = key;
            } else if (step->picksHighest) {
                bestKey = NumberMax(bestKey, key);
            } else {
                bestKey = NumberMin(bestKey, key);
            }

            *stepKey = bestKey;
            isFound = TRTrue;
        }
    }

    return isFound;
}

static void KeepCandidates(Array *candidates, CandidateKey kind, TRInt32 keptKey)
{
    TRUInteger candidateCount = ArrayGetCount(candidates);
    TRUInteger keptCount = 0;
    TRUInteger index;

    for (index = 0; index < candidateCount; index++) {
        const TRTypefaceRef *typeface = ArrayGetItem(candidates, index);

        if (GetCandidateKey(*typeface, kind) == keptKey) {
            TRTypefaceRef *keptTypeface = ArrayGetItem(candidates, keptCount);
            *keptTypeface = *typeface;
            keptCount += 1;
        }
    }

    ArrayResize(candidates, keptCount);
}

/* The first step that finds a key decides, and only the candidates with that key stay. */
static void NarrowCandidates(Array *candidates, CandidateKey kind, const MatchStep *steps,
    TRUInteger stepCount)
{
    TRUInteger stepIndex;

    for (stepIndex = 0; stepIndex < stepCount; stepIndex++) {
        TRInt32 stepKey = 0;

        if (FindStepKey(candidates, kind, &steps[stepIndex], &stepKey)) {
            KeepCandidates(candidates, kind, stepKey);
            break;
        }
    }
}

/* Follows https://www.w3.org/TR/css-fonts-4/#font-style-matching */
static TRTypefaceRef FindBestCandidate(Array *candidates, TRWidth width, TRWeight weight,
    TRSlope slope)
{
    MatchStep steps[MaxStepCount];
    TRUInteger stepCount;
    const TRTypefaceRef *bestTypeface;

    /* There MUST be at least one candidate. */
    TRAssert(ArrayGetCount(candidates) > 0);

    stepCount = MakeWidthSteps(steps, width);
    NarrowCandidates(candidates, CandidateKeyWidth, steps, stepCount);

    stepCount = MakeSlopeSteps(steps, slope);
    NarrowCandidates(candidates, CandidateKeySlope, steps, stepCount);

    stepCount = MakeWeightSteps(steps, weight);
    NarrowCandidates(candidates, CandidateKeyWeight, steps, stepCount);

    /* The candidates are in the order of registration, so the first one wins a tie. */
    bestTypeface = ArrayGetItem(candidates, 0);

    return *bestTypeface;
}

static TRBoolean CollectCandidates(const Array *entries, TRUInteger familyID,
    const TRStringView *familyName, Array *candidates)
{
    TRBoolean isCollected = TRTrue;
    TRUInteger entryCount = ArrayGetCount(entries);
    TRUInteger index;

    for (index = 0; index < entryCount; index++) {
        const TypefaceEntry *entry = ArrayGetItem(entries, index);

        if (EntryIsInFamily(entry, familyID, familyName)) {
            if (!ArrayAppend(candidates, &entry->typeface)) {
                isCollected = TRFalse;
                break;
            }
        }
    }

    return isCollected;
}

/* The family name is only used if the ID is 0. */
static TRTypefaceRef FindMatchingTypeface(TRUInteger familyID, const TRStringView *familyName,
    TRWidth width, TRWeight weight, TRSlope slope)
{
    TRTypefaceRef matchingTypeface = NULL;
    TRTypefaceManager *manager = GetDefaultManager();
    Array candidates;

    ArrayInitialize(&candidates, sizeof(TRTypefaceRef));

    MutexLock(&manager->mutex);

    if (CollectCandidates(&manager->entries, familyID, familyName, &candidates)
        && ArrayGetCount(&candidates) > 0) {
        matchingTypeface = FindBestCandidate(&candidates, width, weight, slope);
    }

    MutexUnlock(&manager->mutex);

    ArrayFinalize(&candidates);

    return matchingTypeface;
}

/* ---------- Enumeration ---------- */

static int CompareEntries(const TypefaceEntry *first, const TypefaceEntry *second)
{
    int order = CompareNames(GetFamilyName(first->typeface), GetFamilyName(second->typeface));

    if (order == 0) {
        order = CompareNames(GetSubfamilyName(first->typeface),
                             GetSubfamilyName(second->typeface));
    }

    return order;
}

/* It is a stable sort, so the entries that are equal stay in the order of their registration. */
static void SortEntries(Array *entries)
{
    TRUInteger entryCount = ArrayGetCount(entries);
    TypefaceEntry *items = ArrayGetItems(entries);
    TRUInteger index;

    for (index = 1; index < entryCount; index++) {
        TypefaceEntry entry = items[index];
        TRUInteger position = index;

        while (position > 0 && CompareEntries(&items[position - 1], &entry) > 0) {
            items[position] = items[position - 1];
            position -= 1;
        }

        items[position] = entry;
    }
}

static void ReleaseSnapshot(Array *snapshot)
{
    TRUInteger entryCount = ArrayGetCount(snapshot);
    TRUInteger index;

    for (index = 0; index < entryCount; index++) {
        const TypefaceEntry *entry = ArrayGetItem(snapshot, index);
        TRTypefaceRelease(entry->typeface);
    }

    ArrayFinalize(snapshot);
}

/* The snapshot retains the typefaces, and has to be released even if the copy failed. */
static TRBoolean CopySortedSnapshot(Array *snapshot)
{
    TRBoolean isCopied = TRTrue;
    TRTypefaceManager *manager = GetDefaultManager();
    TRUInteger entryCount;
    TRUInteger index;

    MutexLock(&manager->mutex);

    entryCount = ArrayGetCount(&manager->entries);

    for (index = 0; index < entryCount; index++) {
        const TypefaceEntry *entry = ArrayGetItem(&manager->entries, index);

        if (ArrayAppend(snapshot, entry)) {
            TRTypefaceRetain(entry->typeface);
        } else {
            isCopied = TRFalse;
            break;
        }
    }

    MutexUnlock(&manager->mutex);

    SortEntries(snapshot);

    return isCopied;
}

static TRBoolean SnapshotHasFamilyBefore(const Array *snapshot, TRUInteger entryIndex)
{
    TRBoolean hasFamily = TRFalse;
    const TypefaceEntry *entry = ArrayGetItem(snapshot, entryIndex);
    const TRStringView *familyName = GetFamilyName(entry->typeface);
    TRUInteger index;

    for (index = 0; index < entryIndex; index++) {
        const TypefaceEntry *previous = ArrayGetItem(snapshot, index);

        if (EntryIsInFamily(previous, entry->familyID, familyName)) {
            hasFamily = TRTrue;
            break;
        }
    }

    return hasFamily;
}

static void EnumerateSnapshotFamilies(const Array *snapshot,
    TRTypefaceManagerFamilyCallback callback, void *context)
{
    TRUInteger entryCount = ArrayGetCount(snapshot);
    TRBoolean stop = TRFalse;
    TRUInteger index;

    for (index = 0; index < entryCount && !stop; index++) {
        if (!SnapshotHasFamilyBefore(snapshot, index)) {
            const TypefaceEntry *entry = ArrayGetItem(snapshot, index);
            const TRStringView *familyName = GetFamilyName(entry->typeface);

            callback(entry->familyID, familyName, context, &stop);
        }
    }
}

/* ---------- Public Functions ---------- */

TRBoolean TRTypefaceManagerRegisterTypeface(TRTypefaceRef typeface, TRUInteger tag,
    TRUInteger familyID)
{
    TRBoolean isRegistered = TRFalse;

    if (typeface) {
        TRTypefaceManager *manager = GetDefaultManager();
        TRUInteger tagIndex = TRInvalidIndex;
        TRUInteger typefaceIndex;

        MutexLock(&manager->mutex);

        typefaceIndex = FindEntryIndexByTypeface(&manager->entries, typeface);
        if (tag != 0) {
            tagIndex = FindEntryIndexByTag(&manager->entries, tag);
        }

        if (typefaceIndex == TRInvalidIndex && tagIndex == TRInvalidIndex) {
            TypefaceEntry entry;

            entry.typeface = TRTypefaceRetain(typeface);
            entry.tag = tag;
            entry.familyID = familyID;

            if (ArrayAppend(&manager->entries, &entry)) {
                isRegistered = TRTrue;
            } else {
                TRTypefaceRelease(entry.typeface);
            }
        }

        MutexUnlock(&manager->mutex);
    }

    return isRegistered;
}

TRBoolean TRTypefaceManagerUnregisterTypeface(TRTypefaceRef typeface)
{
    TRBoolean isUnregistered = TRFalse;
    TRTypefaceManager *manager = GetDefaultManager();
    TRUInteger entryIndex;

    MutexLock(&manager->mutex);

    entryIndex = FindEntryIndexByTypeface(&manager->entries, typeface);

    if (entryIndex != TRInvalidIndex) {
        RemoveTypefaceEntry(&manager->entries, entryIndex);
        isUnregistered = TRTrue;
    }

    MutexUnlock(&manager->mutex);

    /* The reference of the manager is released outside of the lock. */
    if (isUnregistered) {
        TRTypefaceRelease(typeface);
    }

    return isUnregistered;
}

TRTypefaceRef TRTypefaceManagerGetTypeface(TRUInteger tag)
{
    TRTypefaceRef typeface = NULL;

    if (tag != 0) {
        TRTypefaceManager *manager = GetDefaultManager();
        TRUInteger entryIndex;

        MutexLock(&manager->mutex);

        entryIndex = FindEntryIndexByTag(&manager->entries, tag);

        if (entryIndex != TRInvalidIndex) {
            const TypefaceEntry *entry = ArrayGetItem(&manager->entries, entryIndex);
            typeface = entry->typeface;
        }

        MutexUnlock(&manager->mutex);
    }

    return typeface;
}

TRUInteger TRTypefaceManagerGetTypefaceTag(TRTypefaceRef typeface)
{
    TRUInteger tag = 0;
    TRTypefaceManager *manager = GetDefaultManager();
    TRUInteger entryIndex;

    MutexLock(&manager->mutex);

    entryIndex = FindEntryIndexByTypeface(&manager->entries, typeface);

    if (entryIndex != TRInvalidIndex) {
        const TypefaceEntry *entry = ArrayGetItem(&manager->entries, entryIndex);
        tag = entry->tag;
    }

    MutexUnlock(&manager->mutex);

    return tag;
}

TRUInteger TRTypefaceManagerGetTypefaceFamilyID(TRTypefaceRef typeface)
{
    TRUInteger familyID = 0;
    TRTypefaceManager *manager = GetDefaultManager();
    TRUInteger entryIndex;

    MutexLock(&manager->mutex);

    entryIndex = FindEntryIndexByTypeface(&manager->entries, typeface);

    if (entryIndex != TRInvalidIndex) {
        const TypefaceEntry *entry = ArrayGetItem(&manager->entries, entryIndex);
        familyID = entry->familyID;
    }

    MutexUnlock(&manager->mutex);

    return familyID;
}

TRTypefaceRef TRTypefaceManagerGetMatchingTypefaceByFamilyID(TRUInteger familyID,
    TRWidth width, TRWeight weight, TRSlope slope)
{
    TRTypefaceRef matchingTypeface = NULL;

    if (familyID != 0) {
        matchingTypeface = FindMatchingTypeface(familyID, NULL, width, weight, slope);
    }

    return matchingTypeface;
}

TRTypefaceRef TRTypefaceManagerGetMatchingTypefaceByFamilyName(
    const TRStringView *familyName, TRWidth width, TRWeight weight, TRSlope slope)
{
    TRTypefaceRef matchingTypeface = NULL;

    if (familyName) {
        matchingTypeface = FindMatchingTypeface(0, familyName, width, weight, slope);
    }

    return matchingTypeface;
}

void TRTypefaceManagerEnumerateTypefaces(TRTypefaceManagerTypefaceCallback callback,
    void *context)
{
    if (callback) {
        Array snapshot;
        TRBoolean isCopied;

        ArrayInitialize(&snapshot, sizeof(TypefaceEntry));
        isCopied = CopySortedSnapshot(&snapshot);

        if (isCopied) {
            TRUInteger entryCount = ArrayGetCount(&snapshot);
            TRBoolean stop = TRFalse;
            TRUInteger index;

            for (index = 0; index < entryCount && !stop; index++) {
                const TypefaceEntry *entry = ArrayGetItem(&snapshot, index);
                callback(entry->typeface, context, &stop);
            }
        }

        ReleaseSnapshot(&snapshot);
    }
}

void TRTypefaceManagerEnumerateFamilies(TRTypefaceManagerFamilyCallback callback,
    void *context)
{
    if (callback) {
        Array snapshot;
        TRBoolean isCopied;

        ArrayInitialize(&snapshot, sizeof(TypefaceEntry));
        isCopied = CopySortedSnapshot(&snapshot);

        if (isCopied) {
            EnumerateSnapshotFamilies(&snapshot, callback, context);
        }

        ReleaseSnapshot(&snapshot);
    }
}
