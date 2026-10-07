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

#include <API/TRBase.h>
#include <Core/Allocator.h>
#include <Core/Mutex.h>

#include "AdvanceCache.h"

#define PageBits        8
#define PageSize        ((TRUInteger)1 << PageBits)
#define PageMask        (PageSize - 1)

/* Marks an entry that has no advance yet. */
#define EmptyAdvance    (-2147483647 - 1)

static TRInt32 *CreatePage(void)
{
    TRInt32 *page = AllocatorAllocateBlock(sizeof(TRInt32) * PageSize);

    if (page) {
        TRUInteger index;

        for (index = 0; index < PageSize; index++) {
            page[index] = EmptyAdvance;
        }
    }

    return page;
}

TR_INTERNAL void AdvanceCacheInitialize(AdvanceCacheRef cache, TRUInteger glyphCount)
{
    TRUInteger pageCount = (glyphCount + PageMask) >> PageBits;

    MutexInit(&cache->_mutex);

    cache->_pages = NULL;
    cache->_pageCount = 0;
    cache->_glyphCount = 0;

    if (pageCount > 0) {
        cache->_pages = calloc(pageCount, sizeof(TRInt32 *));

        /* Without the page table, nothing is cached. */
        if (cache->_pages) {
            cache->_pageCount = pageCount;
            cache->_glyphCount = glyphCount;
        }
    }
}

TR_INTERNAL void AdvanceCacheFinalize(AdvanceCacheRef cache)
{
    TRUInteger index;

    for (index = 0; index < cache->_pageCount; index++) {
        AllocatorDeallocateBlock(cache->_pages[index]);
    }

    free(cache->_pages);
    MutexDestroy(&cache->_mutex);
}

TR_INTERNAL TRBoolean AdvanceCacheGet(AdvanceCacheRef cache, TRGlyphID glyphID, TRInt32 *advance)
{
    TRBoolean isFound = TRFalse;

    if (glyphID < cache->_glyphCount) {
        TRInt32 *page;

        MutexLock(&cache->_mutex);

        page = cache->_pages[glyphID >> PageBits];
        if (page && page[glyphID & PageMask] != EmptyAdvance) {
            *advance = page[glyphID & PageMask];
            isFound = TRTrue;
        }

        MutexUnlock(&cache->_mutex);
    }

    return isFound;
}

TR_INTERNAL void AdvanceCachePut(AdvanceCacheRef cache, TRGlyphID glyphID, TRInt32 advance)
{
    if (glyphID < cache->_glyphCount) {
        TRInt32 **pageRef = &cache->_pages[glyphID >> PageBits];

        MutexLock(&cache->_mutex);

        if (!*pageRef) {
            *pageRef = CreatePage();
        }

        if (*pageRef) {
            (*pageRef)[glyphID & PageMask] = advance;
        }

        MutexUnlock(&cache->_mutex);
    }
}
