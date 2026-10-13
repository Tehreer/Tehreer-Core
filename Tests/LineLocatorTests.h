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


#ifndef _TEHREER__LINE_LOCATOR_TESTS_H
#define _TEHREER__LINE_LOCATOR_TESTS_H

namespace Tehreer {

class LineLocatorTests {
public:
    LineLocatorTests() = default;

    void run();

private:
    void testSingleLine();
    void testManyLines();
    void testBoundaries();
    void testRectsOutside();
    void testOverlappingBoxes();
    void testFirstLineIndexBelow();
    void testStop();
    void testInvalidArguments();
    void testInfiniteRects();
    void testBandLimit();
    void testNegativeBoxes();
    void testConcurrentQueries();
    void testReferenceCounting();
};

}

#endif
