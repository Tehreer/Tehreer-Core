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

#ifndef _TEHREER__TYPESETTER_TESTS_H
#define _TEHREER__TYPESETTER_TESTS_H

namespace Tehreer {

class TypesetterTests {
public:
    TypesetterTests() = default;

    void run();

private:
    void testCreateInvalid();
    void testDefaultAttributes();
    void testSingleRun();
    void testEmptyText();
    void testRunsFollowShapingAttributes();
    void testPaintAttributesDoNotSplitRuns();
    void testScaleAndBaselineOffset();
    void testBidirectionalRuns();
    void testParagraphs();
    void testRightToLeftParagraph();
    void testReplacementRuns();
    void testBlockReplacements();
    void testTextIsCopied();
    void testFindRunsAndParagraphs();
    void testMeasureRange();
    void testRunQueries();
    void testEncodings();
    void testUnknownScriptsAndNotdef();
    void testLanguageAndFeatureAttributes();
    void testRangesAreChecked();
};

}

#endif
