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
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRGlyphCache.h>
#include <API/TRGlyphImage.h>
#include <API/TRPath.h>
#include <API/TRTypeface.h>
#include <Core/Mutex.h>
#include <Font/FaceMetadata.h>
#include <Graphics/GlyphBitmap.h>
#include <Graphics/RenderableFace.h>

#include "GlyphCache.h"

#define InitialBucketCount  64

/* The least size that FreeType can set is one pixel in each direction, which is 64 in 26.6 format. */
#define MinimumPixelSize    64

enum {
    EntryKindGlyph = 0,     /* The default image, the type and the outline of a glyph. */
    EntryKindColor = 1,     /* The image of a mixed glyph for a foreground color. */
    EntryKindStroke = 2     /* The stroked image of a glyph. */
};

/* Every field that is not used by a kind of entry MUST be zero. */
typedef struct _CacheKey {
    TRUInt32 kind;
    TRTypefaceRef typeface;
    TRInt32 pixelWidth;
    TRInt32 pixelHeight;
    TRInt32 skewX;
    TRUInt32 glyphID;
    TRUInt32 foregroundColor;
    GlyphStrokeKey stroke;
} CacheKey;

typedef struct _GlyphCacheEntry {
    CacheKey key;
    TRUInt32 hash;
    struct _GlyphCacheEntry *bucketNext;
    struct _GlyphCacheEntry *lruPrevious;
    struct _GlyphCacheEntry *lruNext;
    TRUInteger size;

    /* The members below are those of a glyph entry; the other entries only have an image. */
    TRBoolean hasType;
    GlyphType type;
    TRGlyphImageRef image;
    TRPathRef path;
} GlyphCacheEntry;

/*
 * A smaller size is refused by FreeType without an error that reaches the callers, and the face
 * would then keep the size that was set last, so nothing is rendered for it.
 */
static TRBoolean IsRenderable(const GlyphDataKey *key)
{
    return key->pixelWidth >= MinimumPixelSize && key->pixelHeight >= MinimumPixelSize;
}

static TRUInt32 HashKey(const CacheKey *key)
{
    /* FNV-1a over the fields. */
    TRUInt32 hash = 2166136261u;
    TRUInt32 values[11];
    TRUInteger index;

    values[0] = key->kind;
    values[1] = (TRUInt32)((TRUInteger)key->typeface >> 4);
    values[2] = (TRUInt32)key->pixelWidth;
    values[3] = (TRUInt32)key->pixelHeight;
    values[4] = (TRUInt32)key->skewX;
    values[5] = key->glyphID;
    values[6] = key->foregroundColor;
    values[7] = (TRUInt32)key->stroke.lineRadius;
    values[8] = key->stroke.lineCap;
    values[9] = key->stroke.lineJoin;
    values[10] = (TRUInt32)key->stroke.miterLimit;

    for (index = 0; index < 11; index++) {
        hash = (hash ^ values[index]) * 16777619u;
    }

    return hash;
}

static TRBoolean EqualKeys(const CacheKey *first, const CacheKey *second)
{
    return first->kind == second->kind
        && first->typeface == second->typeface
        && first->pixelWidth == second->pixelWidth
        && first->pixelHeight == second->pixelHeight
        && first->skewX == second->skewX
        && first->glyphID == second->glyphID
        && first->foregroundColor == second->foregroundColor
        && first->stroke.lineRadius == second->stroke.lineRadius
        && first->stroke.lineCap == second->stroke.lineCap
        && first->stroke.lineJoin == second->stroke.lineJoin
        && first->stroke.miterLimit == second->stroke.miterLimit;
}

static void SetupKey(CacheKey *key, TRUInt32 kind, const GlyphDataKey *dataKey, TRGlyphID glyphID)
{
    memset(key, 0, sizeof(CacheKey));

    key->kind = kind;
    key->typeface = dataKey->typeface;
    key->pixelWidth = dataKey->pixelWidth;
    key->pixelHeight = dataKey->pixelHeight;
    key->skewX = dataKey->skewX;
    key->glyphID = glyphID;
}

/* The cache MUST be locked by the caller for all of the functions below. */

static void UnlinkFromList(GlyphCacheRef cache, GlyphCacheEntry *entry)
{
    if (entry->lruPrevious) {
        entry->lruPrevious->lruNext = entry->lruNext;
    } else {
        cache->firstEntry = entry->lruNext;
    }

    if (entry->lruNext) {
        entry->lruNext->lruPrevious = entry->lruPrevious;
    } else {
        cache->lastEntry = entry->lruPrevious;
    }

    entry->lruPrevious = NULL;
    entry->lruNext = NULL;
}

static void LinkAsFirst(GlyphCacheRef cache, GlyphCacheEntry *entry)
{
    entry->lruPrevious = NULL;
    entry->lruNext = cache->firstEntry;

    if (cache->firstEntry) {
        cache->firstEntry->lruPrevious = entry;
    } else {
        cache->lastEntry = entry;
    }

    cache->firstEntry = entry;
}

static void RemoveEntry(GlyphCacheRef cache, GlyphCacheEntry *entry)
{
    GlyphCacheEntry **link = &cache->buckets[entry->hash % cache->bucketCount];

    while (*link != entry) {
        link = &(*link)->bucketNext;
    }
    *link = entry->bucketNext;

    UnlinkFromList(cache, entry);

    cache->size -= entry->size;
    cache->entryCount -= 1;

    if (entry->image) {
        TRGlyphImageRelease(entry->image);
    }
    if (entry->path) {
        TRPathRelease(entry->path);
    }
    TRTypefaceRelease(entry->key.typeface);

    free(entry);
}

static void TrimCache(GlyphCacheRef cache)
{
    while (cache->size > cache->capacity && cache->lastEntry) {
        RemoveEntry(cache, cache->lastEntry);
    }
}

static void GrowTable(GlyphCacheRef cache)
{
    TRUInteger newCount = cache->bucketCount * 2;
    GlyphCacheEntry **newBuckets = calloc(newCount, sizeof(GlyphCacheEntry *));
    TRUInteger index;

    /* If there is no memory, the table just stays crowded. */
    if (!newBuckets) {
        return;
    }

    for (index = 0; index < cache->bucketCount; index++) {
        GlyphCacheEntry *entry = cache->buckets[index];

        while (entry) {
            GlyphCacheEntry *next = entry->bucketNext;
            TRUInteger slot = entry->hash % newCount;

            entry->bucketNext = newBuckets[slot];
            newBuckets[slot] = entry;
            entry = next;
        }
    }

    free(cache->buckets);
    cache->buckets = newBuckets;
    cache->bucketCount = newCount;
}

/* Finds the entry and makes it the most recently used one. */
static GlyphCacheEntry *FindEntry(GlyphCacheRef cache, const CacheKey *key, TRUInt32 hash)
{
    GlyphCacheEntry *entry = cache->buckets[hash % cache->bucketCount];

    while (entry) {
        if (entry->hash == hash && EqualKeys(&entry->key, key)) {
            if (cache->firstEntry != entry) {
                UnlinkFromList(cache, entry);
                LinkAsFirst(cache, entry);
            }

            return entry;
        }

        entry = entry->bucketNext;
    }

    return NULL;
}

/* The new entry is the most recently used one. */
static GlyphCacheEntry *CreateEntry(GlyphCacheRef cache, const CacheKey *key, TRUInt32 hash)
{
    GlyphCacheEntry *entry = calloc(1, sizeof(GlyphCacheEntry));
    TRUInteger slot;

    if (!entry) {
        return NULL;
    }

    if (cache->entryCount >= cache->bucketCount) {
        GrowTable(cache);
    }

    entry->key = *key;
    entry->hash = hash;
    entry->size = sizeof(GlyphCacheEntry);
    TRTypefaceRetain(key->typeface);

    slot = hash % cache->bucketCount;
    entry->bucketNext = cache->buckets[slot];
    cache->buckets[slot] = entry;
    LinkAsFirst(cache, entry);

    cache->size += entry->size;
    cache->entryCount += 1;

    return entry;
}

static GlyphCacheEntry *GetEntry(GlyphCacheRef cache, const CacheKey *key)
{
    TRUInt32 hash = HashKey(key);
    GlyphCacheEntry *entry = FindEntry(cache, key, hash);

    return (entry ? entry : CreateEntry(cache, key, hash));
}

static TRUInteger GetPathSize(TRPathRef path)
{
    const FT_Outline *outline = &path->outline;

    return (TRUInteger)outline->n_points * (sizeof(FT_Vector) + sizeof(char))
         + (TRUInteger)outline->n_contours * sizeof(short);
}

/* Replaces the values of an entry, taking over the references that it is given. */
static void StoreImage(GlyphCacheRef cache, GlyphCacheEntry *entry, TRGlyphImageRef image)
{
    cache->size -= entry->size;

    if (entry->image) {
        TRGlyphImageRelease(entry->image);
    }

    entry->image = image;
    entry->size = sizeof(GlyphCacheEntry)
                + (entry->image ? TRGlyphImageGetByteCount(entry->image) : 0)
                + (entry->path ? GetPathSize(entry->path) : 0);

    cache->size += entry->size;
}

static void StorePath(GlyphCacheRef cache, GlyphCacheEntry *entry, TRPathRef path)
{
    cache->size -= entry->size;

    if (entry->path) {
        TRPathRelease(entry->path);
    }

    entry->path = path;
    entry->size = sizeof(GlyphCacheEntry)
                + (entry->image ? TRGlyphImageGetByteCount(entry->image) : 0)
                + (entry->path ? GetPathSize(entry->path) : 0);

    cache->size += entry->size;
}

/* Rendering. It must not be done with the cache locked. */

static void SetupGlyphFontParams(const GlyphDataKey *key, FontParams *fontParams)
{
    TRTypefaceRef typeface = key->typeface;
    FaceMetadataRef metadata = typeface->renderableFace->metadata;

    fontParams->coordinatesPtr = typeface->rawCoordinates;
    fontParams->coordinateCount = (FT_UInt)metadata->variationAxisCount;
    fontParams->colorsPtr = typeface->rawColors;
    fontParams->colorCount = (FT_UInt)metadata->paletteEntryCount;
    fontParams->pixelWidth = key->pixelWidth;
    fontParams->pixelHeight = key->pixelHeight;
    fontParams->transform.xx = 0x10000;
    fontParams->transform.xy = -(FT_Fixed)key->skewX;
    fontParams->transform.yx = 0;
    fontParams->transform.yy = 0x10000;
}

static TRGlyphImageRef RenderImage(const GlyphDataKey *key, TRGlyphID glyphID,
    TRColor foregroundColor)
{
    GlyphBitmapRef bitmap;
    FontParams fontParams;
    FT_Color color;

    color.alpha = (FT_Byte)((foregroundColor >> 24) & 0xFF);
    color.red = (FT_Byte)((foregroundColor >> 16) & 0xFF);
    color.green = (FT_Byte)((foregroundColor >> 8) & 0xFF);
    color.blue = (FT_Byte)(foregroundColor & 0xFF);

    SetupGlyphFontParams(key, &fontParams);
    bitmap = RenderableFaceRasterizeGlyph(key->typeface->renderableFace, &fontParams, glyphID,
        color);

    return (bitmap ? TRGlyphImageCreate(bitmap) : NULL);
}

static TRPathRef RenderPath(const GlyphDataKey *key, TRGlyphID glyphID)
{
    FontParams fontParams;

    SetupGlyphFontParams(key, &fontParams);

    return RenderableFaceCreateGlyphPath(key->typeface->renderableFace, &fontParams, glyphID);
}

/* The public part. */

TR_INTERNAL TRBoolean GlyphCacheInitialize(GlyphCacheRef cache, TRUInteger capacity)
{
    cache->buckets = calloc(InitialBucketCount, sizeof(GlyphCacheEntry *));
    if (!cache->buckets) {
        return TRFalse;
    }

    MutexInit(&cache->mutex);
    cache->capacity = capacity;
    cache->size = 0;
    cache->bucketCount = InitialBucketCount;
    cache->entryCount = 0;
    cache->firstEntry = NULL;
    cache->lastEntry = NULL;

    return TRTrue;
}

TR_INTERNAL void GlyphCacheFinalize(GlyphCacheRef cache)
{
    GlyphCacheClear(cache);

    free(cache->buckets);
    MutexDestroy(&cache->mutex);
}

TR_INTERNAL void GlyphCacheClear(GlyphCacheRef cache)
{
    MutexLock(&cache->mutex);

    while (cache->lastEntry) {
        RemoveEntry(cache, cache->lastEntry);
    }

    MutexUnlock(&cache->mutex);
}

TR_INTERNAL void GlyphCacheSetCapacity(GlyphCacheRef cache, TRUInteger capacity)
{
    MutexLock(&cache->mutex);

    cache->capacity = capacity;
    TrimCache(cache);

    MutexUnlock(&cache->mutex);
}

TR_INTERNAL TRPathRef GlyphCacheGetPath(GlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID)
{
    CacheKey cacheKey;
    GlyphCacheEntry *entry;
    TRPathRef path = NULL;

    if (!IsRenderable(key)) {
        return NULL;
    }

    SetupKey(&cacheKey, EntryKindGlyph, key, glyphID);

    MutexLock(&cache->mutex);
    entry = GetEntry(cache, &cacheKey);
    if (entry && entry->path) {
        path = TRPathRetain(entry->path);
    }
    MutexUnlock(&cache->mutex);

    if (!path) {
        path = RenderPath(key, glyphID);

        if (path) {
            MutexLock(&cache->mutex);

            entry = GetEntry(cache, &cacheKey);
            if (entry && !entry->path) {
                StorePath(cache, entry, TRPathRetain(path));
                TrimCache(cache);
            }

            MutexUnlock(&cache->mutex);
        }
    }

    return path;
}

/* Looks for the image of a color or a stroke entry, which has nothing else in it. */
static TRGlyphImageRef FindImage(GlyphCacheRef cache, const CacheKey *cacheKey)
{
    TRGlyphImageRef image = NULL;
    GlyphCacheEntry *entry;

    MutexLock(&cache->mutex);

    entry = FindEntry(cache, cacheKey, HashKey(cacheKey));
    if (entry && entry->image) {
        image = TRGlyphImageRetain(entry->image);
    }

    MutexUnlock(&cache->mutex);

    return image;
}

static void SaveImage(GlyphCacheRef cache, const CacheKey *cacheKey, TRGlyphImageRef image)
{
    GlyphCacheEntry *entry;

    MutexLock(&cache->mutex);

    entry = GetEntry(cache, cacheKey);
    if (entry && !entry->image) {
        StoreImage(cache, entry, TRGlyphImageRetain(image));
        TrimCache(cache);
    }

    MutexUnlock(&cache->mutex);
}

static TRGlyphImageRef GetColorImage(GlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID, TRColor foregroundColor)
{
    CacheKey cacheKey;
    TRGlyphImageRef image;

    SetupKey(&cacheKey, EntryKindColor, key, glyphID);
    cacheKey.foregroundColor = foregroundColor;

    image = FindImage(cache, &cacheKey);

    if (!image) {
        image = RenderImage(key, glyphID, foregroundColor);

        if (image) {
            SaveImage(cache, &cacheKey, image);
        }
    }

    return image;
}

TR_INTERNAL TRGlyphImageRef GlyphCacheGetImage(GlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID, TRColor foregroundColor)
{
    CacheKey cacheKey;
    GlyphCacheEntry *entry;
    TRGlyphImageRef image = NULL;
    TRBoolean hasType = TRFalse;
    GlyphType type = GlyphTypeMask;

    if (!IsRenderable(key)) {
        return NULL;
    }

    SetupKey(&cacheKey, EntryKindGlyph, key, glyphID);

    MutexLock(&cache->mutex);

    entry = GetEntry(cache, &cacheKey);
    if (entry && entry->hasType) {
        hasType = TRTrue;
        type = entry->type;

        if (entry->image) {
            image = TRGlyphImageRetain(entry->image);
        }
    }

    MutexUnlock(&cache->mutex);

    if (!hasType) {
        type = RenderableFaceGetGlyphType(key->typeface->renderableFace, glyphID);

        /* The image of a mixed glyph depends on the foreground color, so it is not shared. */
        if (type != GlyphTypeMixed) {
            image = RenderImage(key, glyphID, foregroundColor);
        }

        MutexLock(&cache->mutex);

        entry = GetEntry(cache, &cacheKey);
        if (entry && !entry->hasType) {
            entry->hasType = TRTrue;
            entry->type = type;

            StoreImage(cache, entry, (image ? TRGlyphImageRetain(image) : NULL));
            TrimCache(cache);
        }

        MutexUnlock(&cache->mutex);
    }

    if (type == GlyphTypeMixed) {
        return GetColorImage(cache, key, glyphID, foregroundColor);
    }

    return image;
}

TR_INTERNAL TRGlyphImageRef GlyphCacheGetStrokeImage(GlyphCacheRef cache, const GlyphDataKey *key,
    const GlyphStrokeKey *strokeKey, TRGlyphID glyphID)
{
    CacheKey cacheKey;
    TRGlyphImageRef image;

    if (!IsRenderable(key)) {
        return NULL;
    }

    SetupKey(&cacheKey, EntryKindStroke, key, glyphID);
    cacheKey.stroke = *strokeKey;

    image = FindImage(cache, &cacheKey);

    if (!image) {
        TRPathRef path = GlyphCacheGetPath(cache, key, glyphID);

        if (path) {
            GlyphBitmapRef bitmap = GlyphBitmapCreateFromStroke(&path->outline,
                strokeKey->lineRadius, (FT_Stroker_LineCap)strokeKey->lineCap,
                (FT_Stroker_LineJoin)strokeKey->lineJoin, strokeKey->miterLimit);

            if (bitmap) {
                image = TRGlyphImageCreate(bitmap);
            }

            TRPathRelease(path);
        }

        if (image) {
            SaveImage(cache, &cacheKey, image);
        }
    }

    return image;
}
