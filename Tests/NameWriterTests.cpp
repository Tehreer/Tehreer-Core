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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Tehreer/TRString.h>

extern "C" {
#include <Core/NameWriter.h>
#include <SFNT/Utilities.h>
}

#include "NameWriterTests.h"

using namespace std;
using namespace Tehreer;

void NameWriterTests::run() {
    testWriteName();
    testWriteMultipleNames();
    testSkipEmptyName();
    testSkipUnsupportedEncoding();
    testSkipOddByte();
    testBuffersAreContiguous();
}

static NameString makeName(const vector<uint8_t> &bytes, SFNTEncoding encoding = SFNTEncodingUTF16BE) {
    return NameString{ bytes.data(), bytes.size(), encoding };
}

void NameWriterTests::testWriteName() {
    const vector<uint8_t> bytes = { 0x00, 0x52, 0x06, 0x2F };
    NameString name = makeName(bytes);
    TRStringView views[1];
    uint16_t codeUnits[2] = { 0 };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);
    TRStringView *view = NameWriterWrite(&writer, &name);

    assert(view == &views[0]);
    assert(view->buffer == codeUnits);
    assert(view->length == 2);
    assert(view->encoding == TRStringEncodingUTF16);
    assert(codeUnits[0] == 0x0052);
    assert(codeUnits[1] == 0x062F);
}

void NameWriterTests::testWriteMultipleNames() {
    const vector<uint8_t> first = { 0x00, 0x41, 0x00, 0x42 };
    const vector<uint8_t> second = { 0x00, 0x43 };
    NameString firstName = makeName(first);
    NameString secondName = makeName(second);
    TRStringView views[2];
    uint16_t codeUnits[3] = { 0 };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);

    assert(NameWriterWrite(&writer, &firstName) == &views[0]);
    assert(NameWriterWrite(&writer, &secondName) == &views[1]);

    assert(views[0].length == 2);
    assert(views[1].length == 1);
    assert(codeUnits[0] == 'A');
    assert(codeUnits[1] == 'B');
    assert(codeUnits[2] == 'C');
}

void NameWriterTests::testSkipEmptyName() {
    const vector<uint8_t> bytes = { 0x00, 0x41 };
    NameString empty = { nullptr, 0, SFNTEncodingUnknown };
    NameString name = makeName(bytes);
    TRStringView views[1];
    uint16_t codeUnits[1] = { 0 };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);

    assert(NameWriterWrite(&writer, &empty) == nullptr);
    assert(NameWriterWrite(&writer, &name) == &views[0]);
    assert(codeUnits[0] == 'A');
}

void NameWriterTests::testSkipUnsupportedEncoding() {
    const vector<uint8_t> legacy = { 'R', 'o', 'b', 'o' };
    const vector<uint8_t> bytes = { 0x00, 0x41 };
    NameString legacyName = makeName(legacy, SFNTEncodingUnknown);
    NameString name = makeName(bytes);
    TRStringView views[1];
    uint16_t codeUnits[1] = { 0 };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);

    assert(NameWriterWrite(&writer, &legacyName) == nullptr);
    assert(NameWriterWrite(&writer, &name) == &views[0]);
    assert(codeUnits[0] == 'A');
}

void NameWriterTests::testSkipOddByte() {
    const vector<uint8_t> single = { 0x00 };
    const vector<uint8_t> odd = { 0x00, 0x41, 0x00 };
    NameString singleName = makeName(single);
    NameString oddName = makeName(odd);
    TRStringView views[1];
    uint16_t codeUnits[2] = { 0xFFFF, 0xFFFF };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);

    assert(NameWriterWrite(&writer, &singleName) == nullptr);
    assert(NameWriterWrite(&writer, &oddName) == &views[0]);
    assert(views[0].length == 1);
    assert(codeUnits[0] == 'A');
    assert(codeUnits[1] == 0xFFFF);
}

void NameWriterTests::testBuffersAreContiguous() {
    const vector<uint8_t> bytes = { 0x00, 0x41, 0x00, 0x42 };
    NameString name = makeName(bytes);
    TRStringView views[3];
    uint16_t codeUnits[6] = { 0 };
    NameWriter writer;

    NameWriterInitialize(&writer, views, codeUnits);

    for (size_t i = 0; i < 3; i++) {
        TRStringView *view = NameWriterWrite(&writer, &name);

        assert(view == &views[i]);
        assert(view->buffer == codeUnits + i * 2);
    }
}

#ifdef STANDALONE_TESTING

int main() {
    NameWriterTests tests;
    tests.run();

    return 0;
}

#endif
