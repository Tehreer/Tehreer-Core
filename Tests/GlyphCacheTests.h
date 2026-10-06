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

#ifndef _TEHREER__GLYPH_CACHE_TESTS_H
#define _TEHREER__GLYPH_CACHE_TESTS_H

namespace Tehreer {

class GlyphCacheTests {
public:
    GlyphCacheTests() = default;

    void run();

private:
    void testCreate();
    void testDefaultCache();
    void testImageIsCached();
    void testKeysDistinguishSettings();
    void testForegroundColorDoesNotSplitMaskGlyphs();
    void testPathIsCached();
    void testStrokeImages();
    void testMissingGlyphs();
    void testEvictionByCapacity();
    void testLeastRecentlyUsedGoesFirst();
    void testEvictedImagesStayValid();
    void testTypefaceRetention();
    void testClearAndCapacity();
    void testTableGrowth();
    void testNativeDataFollowsImage();
    void testConcurrentLookups();
    void testSeparateCaches();
};

}

#endif
