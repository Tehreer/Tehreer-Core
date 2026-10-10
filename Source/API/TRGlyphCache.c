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

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRTypeface.h>

#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRGlyphImage.h>
#include <API/TRPath.h>
#include <API/TRTypeface.h>
#include <Core/Allocator.h>
#include <Core/Array.h>
#include <Core/Mutex.h>
#include <Core/Object.h>
#include <Core/Once.h>
#include <Font/FaceMetadata.h>
#include <Graphics/GlyphBitmap.h>
#include <Graphics/RenderableFace.h>

#include "TRGlyphCache.h"

#define InitialBucketCount  64
#define DefaultCapacity     (8 * 1024 * 1024)

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

/* The cache MUST be locked by the caller for all of the functions below, until the rendering ones. */

static void UnlinkFromList(TRGlyphCacheRef cache, GlyphCacheEntry *entry)
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

static void LinkAsFirst(TRGlyphCacheRef cache, GlyphCacheEntry *entry)
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

static GlyphCacheEntry **GetBucket(TRGlyphCacheRef cache, TRUInt32 hash)
{
    return ArrayGetItem(&cache->buckets, hash % ArrayGetCount(&cache->buckets));
}

static void RemoveEntry(TRGlyphCacheRef cache, GlyphCacheEntry *entry)
{
    GlyphCacheEntry **link = GetBucket(cache, entry->hash);

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

    AllocatorDeallocateBlock(entry);
}

static void TrimCache(TRGlyphCacheRef cache)
{
    while (cache->size > cache->capacity && cache->lastEntry) {
        RemoveEntry(cache, cache->lastEntry);
    }
}

static void GrowTable(TRGlyphCacheRef cache)
{
    TRUInteger oldCount = ArrayGetCount(&cache->buckets);
    Array newBuckets;

    ArrayInitialize(&newBuckets, sizeof(GlyphCacheEntry *));

    /* If there is no memory, the table just stays crowded. */
    if (ArrayResize(&newBuckets, oldCount * 2)) {
        TRUInteger index;

        for (index = 0; index < oldCount; index++) {
            GlyphCacheEntry *entry = *(GlyphCacheEntry **)ArrayGetItem(&cache->buckets, index);

            while (entry) {
                GlyphCacheEntry *next = entry->bucketNext;
                GlyphCacheEntry **bucket = ArrayGetItem(&newBuckets,
                    entry->hash % ArrayGetCount(&newBuckets));

                entry->bucketNext = *bucket;
                *bucket = entry;
                entry = next;
            }
        }

        ArrayFinalize(&cache->buckets);
        cache->buckets = newBuckets;
    } else {
        ArrayFinalize(&newBuckets);
    }
}

/* Finds the entry and makes it the most recently used one. */
static GlyphCacheEntry *FindEntry(TRGlyphCacheRef cache, const CacheKey *key, TRUInt32 hash)
{
    GlyphCacheEntry *foundEntry = NULL;
    GlyphCacheEntry *entry = *GetBucket(cache, hash);

    while (entry) {
        if (entry->hash == hash && EqualKeys(&entry->key, key)) {
            if (cache->firstEntry != entry) {
                UnlinkFromList(cache, entry);
                LinkAsFirst(cache, entry);
            }

            foundEntry = entry;
            break;
        }

        entry = entry->bucketNext;
    }

    return foundEntry;
}

/* The new entry is the most recently used one. */
static GlyphCacheEntry *CreateEntry(TRGlyphCacheRef cache, const CacheKey *key, TRUInt32 hash)
{
    GlyphCacheEntry *entry = AllocatorAllocateZeroedBlock(sizeof(GlyphCacheEntry));

    if (entry) {
        GlyphCacheEntry **bucket;

        if (cache->entryCount >= ArrayGetCount(&cache->buckets)) {
            GrowTable(cache);
        }

        entry->key = *key;
        entry->hash = hash;
        entry->size = sizeof(GlyphCacheEntry);
        TRTypefaceRetain(key->typeface);

        bucket = GetBucket(cache, hash);
        entry->bucketNext = *bucket;
        *bucket = entry;
        LinkAsFirst(cache, entry);

        cache->size += entry->size;
        cache->entryCount += 1;
    }

    return entry;
}

static GlyphCacheEntry *GetEntry(TRGlyphCacheRef cache, const CacheKey *key)
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
static void StoreImage(TRGlyphCacheRef cache, GlyphCacheEntry *entry, TRGlyphImageRef image)
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

static void StorePath(TRGlyphCacheRef cache, GlyphCacheEntry *entry, TRPathRef path)
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

/* Rendering must not be done with the cache locked. The functions after it lock it on their own. */

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

static TRGlyphImageRef CreateGlyphImage(const GlyphDataKey *key, TRGlyphID glyphID,
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

static TRPathRef CreateGlyphPath(const GlyphDataKey *key, TRGlyphID glyphID)
{
    FontParams fontParams;

    SetupGlyphFontParams(key, &fontParams);

    return RenderableFaceCreateGlyphPath(key->typeface->renderableFace, &fontParams, glyphID);
}

/* Looks for the image of a color or a stroke entry, which has nothing else in it. */
static TRGlyphImageRef CopyCachedImage(TRGlyphCacheRef cache, const CacheKey *cacheKey)
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

static void SavePath(TRGlyphCacheRef cache, const CacheKey *cacheKey, TRPathRef path)
{
    GlyphCacheEntry *entry;

    MutexLock(&cache->mutex);

    entry = GetEntry(cache, cacheKey);
    if (entry && !entry->path) {
        StorePath(cache, entry, TRPathRetain(path));
        TrimCache(cache);
    }

    MutexUnlock(&cache->mutex);
}

static void SaveImage(TRGlyphCacheRef cache, const CacheKey *cacheKey, TRGlyphImageRef image)
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

/* The stroke is made out of the outline of the glyph, which is taken from the cache. */
static TRGlyphImageRef CreateStrokeImage(TRGlyphCacheRef cache, const GlyphDataKey *key,
    const GlyphStrokeKey *strokeKey, TRGlyphID glyphID)
{
    TRGlyphImageRef image = NULL;
    TRPathRef path = TRGlyphCacheCopyPath(cache, key, glyphID);

    if (path) {
        GlyphBitmapRef bitmap;

        bitmap = GlyphBitmapCreateFromStroke(&path->outline, strokeKey->lineRadius,
            (FT_Stroker_LineCap)strokeKey->lineCap, (FT_Stroker_LineJoin)strokeKey->lineJoin,
            strokeKey->miterLimit);

        if (bitmap) {
            image = TRGlyphImageCreate(bitmap);
        }

        TRPathRelease(path);
    }

    return image;
}

static TRGlyphImageRef CopyColorImage(TRGlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID, TRColor foregroundColor)
{
    CacheKey cacheKey;
    TRGlyphImageRef image;

    SetupKey(&cacheKey, EntryKindColor, key, glyphID);
    cacheKey.foregroundColor = foregroundColor;

    image = CopyCachedImage(cache, &cacheKey);

    if (!image) {
        image = CreateGlyphImage(key, glyphID, foregroundColor);

        if (image) {
            SaveImage(cache, &cacheKey, image);
        }
    }

    return image;
}

/* The public part. */

static TRGlyphCacheRef DefaultCache;

static void FinalizeGlyphCache(ObjectRef object)
{
    TRGlyphCacheFinalize(object);
}

static void InitDefaultCache(void)
{
    DefaultCache = TRGlyphCacheCreate(DefaultCapacity);
}

TR_INTERNAL TRBoolean TRGlyphCacheInitialize(TRGlyphCacheRef cache, TRUInteger capacity)
{
    TRBoolean isInitialized = TRFalse;


    ArrayInitialize(&cache->buckets, sizeof(GlyphCacheEntry *));

    if (ArrayResize(&cache->buckets, InitialBucketCount)) {
        MutexInit(&cache->mutex);
        cache->capacity = capacity;
        cache->size = 0;
        cache->entryCount = 0;
        cache->firstEntry = NULL;
        cache->lastEntry = NULL;

        isInitialized = TRTrue;
    }

    return isInitialized;
}

TR_INTERNAL void TRGlyphCacheFinalize(TRGlyphCacheRef cache)
{
    TRGlyphCacheClear(cache);

    ArrayFinalize(&cache->buckets);
    MutexDestroy(&cache->mutex);
}

TR_INTERNAL TRGlyphImageRef TRGlyphCacheCopyImage(TRGlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID, TRColor foregroundColor)
{
    TRGlyphImageRef glyphImage = NULL;

    if (IsRenderable(key)) {
        TRGlyphImageRef image = NULL;
        GlyphType type = GlyphTypeUnknown;
        CacheKey cacheKey;
        GlyphCacheEntry *entry;

        SetupKey(&cacheKey, EntryKindGlyph, key, glyphID);

        MutexLock(&cache->mutex);

        entry = GetEntry(cache, &cacheKey);
        if (entry && entry->type != GlyphTypeUnknown) {
            type = entry->type;

            if (entry->image) {
                image = TRGlyphImageRetain(entry->image);
            }
        }

        MutexUnlock(&cache->mutex);

        if (type == GlyphTypeUnknown) {
            type = RenderableFaceGetGlyphType(key->typeface->renderableFace, glyphID);

            /* The image of a mixed glyph depends on the foreground color, so it is not shared. */
            if (type != GlyphTypeMixed) {
                image = CreateGlyphImage(key, glyphID, foregroundColor);
            }

            MutexLock(&cache->mutex);

            entry = GetEntry(cache, &cacheKey);
            if (entry && entry->type == GlyphTypeUnknown) {
                entry->type = type;

                StoreImage(cache, entry, (image ? TRGlyphImageRetain(image) : NULL));
                TrimCache(cache);
            }

            MutexUnlock(&cache->mutex);
        }

        if (type == GlyphTypeMixed) {
            glyphImage = CopyColorImage(cache, key, glyphID, foregroundColor);
        } else {
            glyphImage = image;
        }
    }

    return glyphImage;
}

TR_INTERNAL TRGlyphImageRef TRGlyphCacheCopyStrokeImage(TRGlyphCacheRef cache,
    const GlyphDataKey *key, const GlyphStrokeKey *strokeKey, TRGlyphID glyphID)
{
    TRGlyphImageRef strokeImage = NULL;

    if (IsRenderable(key)) {
        CacheKey cacheKey;

        SetupKey(&cacheKey, EntryKindStroke, key, glyphID);
        cacheKey.stroke = *strokeKey;

        strokeImage = CopyCachedImage(cache, &cacheKey);

        if (!strokeImage) {
            strokeImage = CreateStrokeImage(cache, key, strokeKey, glyphID);

            if (strokeImage) {
                SaveImage(cache, &cacheKey, strokeImage);
            }
        }
    }

    return strokeImage;
}

TR_INTERNAL TRPathRef TRGlyphCacheCopyPath(TRGlyphCacheRef cache, const GlyphDataKey *key,
    TRGlyphID glyphID)
{
    TRPathRef path = NULL;

    if (IsRenderable(key)) {
        CacheKey cacheKey;
        GlyphCacheEntry *entry;

        SetupKey(&cacheKey, EntryKindGlyph, key, glyphID);

        MutexLock(&cache->mutex);
        entry = GetEntry(cache, &cacheKey);
        if (entry && entry->path) {
            path = TRPathRetain(entry->path);
        }
        MutexUnlock(&cache->mutex);

        if (!path) {
            path = CreateGlyphPath(key, glyphID);

            if (path) {
                SavePath(cache, &cacheKey, path);
            }
        }
    }

    return path;
}

TRGlyphCacheRef TRGlyphCacheCreate(TRUInteger capacity)
{
    const TRUInteger size = sizeof(TRGlyphCache);
    void *pointer = NULL;
    TRGlyphCacheRef cache;

    cache = ObjectCreate(&size, 1, &pointer, FinalizeGlyphCache);

    if (cache && !TRGlyphCacheInitialize(cache, capacity)) {
        /* Nothing was initialized, so it must not be finalized as a cache. */
        cache->_base.finalize = NULL;
        ObjectRelease(cache);
        cache = NULL;
    }

    return cache;
}

TRGlyphCacheRef TRGlyphCacheGetDefault(void)
{
    static Once once = OnceMake();

    OnceExecute(&once, InitDefaultCache);

    return DefaultCache;
}

TRUInteger TRGlyphCacheGetCapacity(TRGlyphCacheRef cache)
{
    TRUInteger capacity;

    MutexLock(&cache->mutex);
    capacity = cache->capacity;
    MutexUnlock(&cache->mutex);

    return capacity;
}

void TRGlyphCacheSetCapacity(TRGlyphCacheRef cache, TRUInteger capacity)
{
    MutexLock(&cache->mutex);

    cache->capacity = capacity;
    TrimCache(cache);

    MutexUnlock(&cache->mutex);
}

TRUInteger TRGlyphCacheGetSize(TRGlyphCacheRef cache)
{
    TRUInteger size;

    MutexLock(&cache->mutex);
    size = cache->size;
    MutexUnlock(&cache->mutex);

    return size;
}

void TRGlyphCacheClear(TRGlyphCacheRef cache)
{
    MutexLock(&cache->mutex);

    while (cache->lastEntry) {
        RemoveEntry(cache, cache->lastEntry);
    }

    MutexUnlock(&cache->mutex);
}

TRGlyphCacheRef TRGlyphCacheRetain(TRGlyphCacheRef cache)
{
    return ObjectRetain((ObjectRef)cache);
}

void TRGlyphCacheRelease(TRGlyphCacheRef cache)
{
    ObjectRelease((ObjectRef)cache);
}
