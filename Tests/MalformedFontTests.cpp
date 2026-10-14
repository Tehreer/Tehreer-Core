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
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Tehreer/TRFontFile.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypeface.h>

extern "C" {
#include <Core/Mutex.h>
#include <Font/FaceMetadata.h>
#include <Font/FontData.h>
#include <Graphics/FreeType.h>
#include <Graphics/RenderableFace.h>
#include <Graphics/ShapableFace.h>
}

#include "TestFonts.h"

#include "MalformedFontTests.h"

using namespace std;
using namespace Tehreer;

typedef vector<uint8_t> Bytes;

constexpr size_t HeaderSize = 12;
constexpr size_t RecordSize = 16;
constexpr size_t MutationRounds = 400;

static const char *const StaticFont = "Roboto-Regular.abc.ttf";
static const char *const VariableFont = "Roboto-Variable.abc.ttf";
static const char *const ColorFont = "RocherColorGX.abc.ttf";
static const char *const PaletteFont = "COLRv0.extents.ttf";
static const char *const NamesFont = "nameID.dup.expected.ttf";
static const char *const BitmapFont = "NotoColorEmoji-CBDT.flags.ttf";

static const char *const AllFonts[] = {
    StaticFont, VariableFont, ColorFont, PaletteFont, NamesFont, BitmapFont
};

static const char *const TableTags[] = {
    "head", "hhea", "maxp", "hmtx", "cmap", "loca", "glyf", "name", "OS/2", "post",
    "fvar", "avar", "gvar", "STAT", "CPAL", "COLR", "CBDT", "CBLC", "GSUB", "GPOS", "GDEF"
};

void MalformedFontTests::run() {
    testIntactFonts();
    testTruncatedFiles();
    testBadTableDirectory();
    testBadTableRecords();
    testEmptyTables();
    testZeroAndHugeCounts();
    testBadNameTable();
    testBadVariationTable();
    testBadPaletteTable();
    testGarbageData();
    testCollectionHeader();
    testByteCorruption();
}

static Bytes readFont(const char *fontName) {
    ifstream stream(testFontPath(fontName), ios::binary);
    assert(stream.good());

    return Bytes(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());
}

static uint32_t readUInt16(const Bytes &data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 8) | data[offset + 1];
}

static uint32_t readUInt32(const Bytes &data, size_t offset) {
    return (readUInt16(data, offset) << 16) | readUInt16(data, offset + 2);
}

static void writeUInt16(Bytes &data, size_t offset, uint32_t value) {
    assert(offset + 2 <= data.size());

    data[offset] = static_cast<uint8_t>(value >> 8);
    data[offset + 1] = static_cast<uint8_t>(value);
}

static void writeUInt32(Bytes &data, size_t offset, uint32_t value) {
    writeUInt16(data, offset, value >> 16);
    writeUInt16(data, offset + 2, value & 0xFFFF);
}

/* Returns the offset of the table record, or zero if the font does not have the table. */
static size_t findRecord(const Bytes &data, const char *tag) {
    size_t tableCount = readUInt16(data, 4);

    for (size_t i = 0; i < tableCount; i++) {
        size_t record = HeaderSize + (i * RecordSize);

        if (memcmp(&data[record], tag, 4) == 0) {
            return record;
        }
    }

    return 0;
}

/* Returns the offset of the table data, or zero if the font does not have the table. */
static size_t findTable(const Bytes &data, const char *tag) {
    size_t record = findRecord(data, tag);

    return record != 0 ? readUInt32(data, record + 8) : 0;
}

static size_t directoryEnd(const Bytes &data) {
    return HeaderSize + (readUInt16(data, 4) * RecordSize);
}

static void touch(const TRStringView *view) {
    if (view) {
        assert(view->encoding == TRStringEncodingUTF16);
        assert(view->length == 0 || view->buffer != nullptr);

        const auto *units = static_cast<const char16_t *>(view->buffer);
        char16_t sum = 0;
        for (size_t i = 0; i < view->length; i++) {
            sum = static_cast<char16_t>(sum + units[i]);
        }
        (void)sum;
    }
}

/* Reads everything that a typeface tells without assuming anything about the font. */
static void exerciseTypeface(TRTypefaceRef typeface) {
    touch(TRTypefaceGetFamilyName(typeface));
    touch(TRTypefaceGetSubfamilyName(typeface));
    touch(TRTypefaceGetFullName(typeface));

    (void)TRTypefaceGetWeight(typeface);
    (void)TRTypefaceGetWidth(typeface);
    (void)TRTypefaceGetSlope(typeface);
    (void)TRTypefaceGetUnitsPerEM(typeface);
    (void)TRTypefaceGetAscent(typeface);
    (void)TRTypefaceGetDescent(typeface);
    (void)TRTypefaceGetLeading(typeface);
    (void)TRTypefaceGetBoundingBox(typeface);
    (void)TRTypefaceGetUnderlinePosition(typeface);
    (void)TRTypefaceGetUnderlineThickness(typeface);
    (void)TRTypefaceGetStrikeoutPosition(typeface);
    (void)TRTypefaceGetStrikeoutThickness(typeface);
    (void)TRTypefaceIsScalable(typeface);

    TRUInteger axisCount = TRTypefaceGetVariationAxisCount(typeface);
    const TRVariationAxis *axes = TRTypefaceGetVariationAxesPtr(typeface);
    assert(axisCount == 0 || axes != nullptr);
    for (TRUInteger i = 0; i < axisCount; i++) {
        touch(axes[i].name);
    }

    TRUInteger styleCount = TRTypefaceGetNamedStyleCount(typeface);
    const TRNamedStyle *styles = TRTypefaceGetNamedStylesPtr(typeface);
    assert(styleCount == 0 || styles != nullptr);
    for (TRUInteger i = 0; i < styleCount; i++) {
        touch(styles[i].subfamilyName);
        touch(styles[i].postScriptName);
        assert(styles[i].coordinateCount == axisCount);
    }

    TRUInteger entryCount = TRTypefaceGetPaletteEntryCount(typeface);
    const TRPaletteEntry *entries = TRTypefaceGetPaletteEntriesPtr(typeface);
    assert(entryCount == 0 || entries != nullptr);
    for (TRUInteger i = 0; i < entryCount; i++) {
        touch(entries[i].name);
    }

    TRUInteger paletteCount = TRTypefaceGetPredefinedPaletteCount(typeface);
    const TRPredefinedPalette *palettes = TRTypefaceGetPredefinedPalettesPtr(typeface);
    assert(paletteCount == 0 || palettes != nullptr);
    for (TRUInteger i = 0; i < paletteCount; i++) {
        touch(palettes[i].name);
        assert(palettes[i].colorCount == entryCount);

        TRColor sum = 0;
        for (TRUInteger j = 0; j < palettes[i].colorCount; j++) {
            sum += palettes[i].colorsPtr[j];
        }
        (void)sum;
    }

    TRUInteger strikeCount = TRTypefaceGetBitmapStrikeCount(typeface);
    const TRBitmapStrike *strikes = TRTypefaceGetBitmapStrikesPtr(typeface);
    assert(strikeCount == 0 || strikes != nullptr);
    for (TRUInteger i = 0; i < strikeCount; i++) {
        (void)strikes[i].pixelWidth;
    }

    TRUInteger glyphCount = TRTypefaceGetGlyphCount(typeface);
    TRUInteger glyphLimit = glyphCount < 8 ? glyphCount : 8;
    for (TRUInteger i = 0; i <= glyphLimit; i++) {
        TRGlyphID glyphID = static_cast<TRGlyphID>(i);
        char name[32];

        (void)TRTypefaceGetGlyphAdvance(typeface, glyphID, 16.0f, TRFalse);
        (void)TRTypefaceGetGlyphAdvance(typeface, glyphID, 16.0f, TRTrue);
        (void)TRTypefaceGetGlyphName(typeface, glyphID, name, sizeof(name));

        TRPathRef path = TRTypefaceCreateGlyphPath(typeface, glyphID, 16.0f);
        if (path) {
            TRPathRelease(path);
        }
    }

    for (TRUInt32 codePoint = 'a'; codePoint <= 'c'; codePoint++) {
        TRGlyphID glyphID = TRTypefaceGetGlyphID(typeface, codePoint);
        (void)TRTypefaceGetVariantGlyphID(typeface, codePoint, 0xFE0F);
        (void)TRTypefaceGetGlyphAdvance(typeface, glyphID, 16.0f, TRFalse);
    }

    char buffer[64];
    for (const char *tag : TableTags) {
        TRTag tableTag = TRTagMake(tag[0], tag[1], tag[2], tag[3]);
        TRUInteger size = TRTypefaceGetTableSize(typeface, tableTag);
        TRUInteger length = TRTypefaceGetTableData(typeface, tableTag, 0, buffer, sizeof(buffer));
        assert(length <= size);
        assert(length <= sizeof(buffer));
    }
}

/* Opens the font through the whole pipeline, and returns the number of typefaces it gave. */
static size_t openAndExercise(const Bytes &data) {
    size_t typefaceCount = 0;

    TRFontFileRef fontFile = TRFontFileCreateFromMemory(data.data(), data.size());
    if (fontFile) {
        TRUInteger count = TRFontFileGetTypefaceCount(fontFile);
        assert(TRFontFileGetTypeface(fontFile, count) == nullptr);

        for (TRUInteger i = 0; i < count; i++) {
            TRTypefaceRef typeface = TRFontFileGetTypeface(fontFile, i);
            if (typeface) {
                exerciseTypeface(typeface);
                typefaceCount += 1;
            }
        }

        TRFontFileRelease(fontFile);
    }

    return typefaceCount;
}

/* Opens the font through the internal modules, which each have to fail cleanly on their own. */
static void openInternally(const Bytes &data) {
    FontDataRef fontData = FontDataCreateFromMemory(data.data(), data.size());
    if (fontData) {
        FT_Face ftFace = FontDataCreateFTFace(fontData, 0);
        if (ftFace) {
            FaceMetadataRef metadata = FaceMetadataCreate(ftFace);
            if (metadata) {
                FaceMetadataRelease(metadata);
            }

            FreeTypeRef freetype = FreeTypeGetDefault();
            MutexLock(&freetype->mutex);
            FT_Done_Face(ftFace);
            MutexUnlock(&freetype->mutex);
        }

        RenderableFaceRef renderableFace = RenderableFaceCreate(fontData, 0);
        if (renderableFace) {
            ShapableFaceRef shapableFace = ShapableFaceCreate(renderableFace);
            if (shapableFace) {
                ShapableFaceRelease(shapableFace);
            }

            RenderableFaceRelease(renderableFace);
        }

        FontDataRelease(fontData);
    }
}

static void openFully(const Bytes &data) {
    openAndExercise(data);
    openInternally(data);
}

void MalformedFontTests::testIntactFonts() {
    /* The helpers have to work on the untouched fonts, or the other tests prove nothing. */
    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);

        assert(openAndExercise(data) >= 1);
        openInternally(data);
    }

    assert(findTable(readFont(StaticFont), "head") != 0);
    assert(findTable(readFont(VariableFont), "fvar") != 0);
    assert(findTable(readFont(ColorFont), "CPAL") != 0);
    assert(findTable(readFont(StaticFont), "fvar") == 0);
}

void MalformedFontTests::testTruncatedFiles() {
    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);
        size_t headerEnd = directoryEnd(data);
        size_t step = data.size() > 4096 ? 61 : 1;

        /* A file that ends inside the table directory has no tables to work with. */
        for (size_t length = 1; length < headerEnd; length++) {
            Bytes truncated(data.begin(), data.begin() + length);

            assert(TRFontFileCreateFromMemory(truncated.data(), truncated.size()) == nullptr);
            assert(FontDataCreateFromMemory(truncated.data(), truncated.size()) == nullptr);
        }

        /* A file that loses the end of its tables may open, but then it must stay usable. */
        for (size_t length = headerEnd; length < data.size(); length += step) {
            Bytes truncated(data.begin(), data.begin() + length);

            openFully(truncated);
        }

        openFully(Bytes(data.begin(), data.end() - 1));
    }
}

void MalformedFontTests::testBadTableDirectory() {
    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);

        /* No tables at all. */
        Bytes noTables = data;
        writeUInt16(noTables, 4, 0);
        assert(TRFontFileCreateFromMemory(noTables.data(), noTables.size()) == nullptr);

        /* The directory would be longer than the whole file. */
        Bytes hugeCount = data;
        writeUInt16(hugeCount, 4, 0xFFFF);
        openFully(hugeCount);

        /* One table more than the directory holds, so the extra record reads the first table. */
        Bytes extraTable = data;
        writeUInt16(extraTable, 4, readUInt16(data, 4) + 1);
        openFully(extraTable);

        /* The search values of the directory do not match the count. */
        Bytes badSearch = data;
        writeUInt16(badSearch, 6, 0xFFFF);
        writeUInt16(badSearch, 8, 0xFFFF);
        writeUInt16(badSearch, 10, 0xFFFF);
        openFully(badSearch);

        /* Unknown sfnt versions. */
        const uint32_t versions[] = { 0, 0xFFFFFFFF, 0x00020000, 0x4F54544F, 0x74727565, 0x74797031 };
        for (uint32_t version : versions) {
            Bytes versioned = data;
            writeUInt32(versioned, 0, version);
            openFully(versioned);
        }
    }
}

void MalformedFontTests::testBadTableRecords() {
    const uint32_t badOffsets[] = { 0, 4, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000 };
    const uint32_t badLengths[] = { 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0x01000000 };

    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);
        size_t tableCount = readUInt16(data, 4);

        for (size_t i = 0; i < tableCount; i++) {
            size_t record = HeaderSize + (i * RecordSize);

            for (uint32_t offset : badOffsets) {
                Bytes patched = data;
                writeUInt32(patched, record + 8, offset);
                openFully(patched);
            }

            for (uint32_t length : badLengths) {
                Bytes patched = data;
                writeUInt32(patched, record + 12, length);
                openFully(patched);
            }

            /* The offset is valid but the end of the table wraps around 32 bits. */
            Bytes wrapped = data;
            writeUInt32(wrapped, record + 8, static_cast<uint32_t>(data.size() - 2));
            writeUInt32(wrapped, record + 12, 0xFFFFFFFF);
            openFully(wrapped);

            /* The table starts exactly at the end of the file. */
            Bytes atEnd = data;
            writeUInt32(atEnd, record + 8, static_cast<uint32_t>(data.size()));
            openFully(atEnd);
        }
    }
}

void MalformedFontTests::testEmptyTables() {
    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);

        for (const char *tag : TableTags) {
            size_t record = findRecord(data, tag);

            if (record != 0) {
                Bytes patched = data;
                writeUInt32(patched, record + 12, 0);
                openFully(patched);
            }
        }
    }
}

void MalformedFontTests::testZeroAndHugeCounts() {
    const uint32_t values[] = { 0, 1, 0x7FFF, 0xFFFF };

    for (uint32_t value : values) {
        Bytes data = readFont(StaticFont);
        size_t head = findTable(data, "head");
        size_t hhea = findTable(data, "hhea");
        size_t maxp = findTable(data, "maxp");
        assert(head != 0 && hhea != 0 && maxp != 0);

        /* The glyph count, the unit scale and the number of metrics are lies. */
        Bytes glyphs = data;
        writeUInt16(glyphs, maxp + 4, value);
        openFully(glyphs);

        Bytes metrics = data;
        writeUInt16(metrics, hhea + 34, value);
        openFully(metrics);

        Bytes scale = data;
        writeUInt16(scale, head + 18, value);
        openFully(scale);

        Bytes locations = data;
        writeUInt16(locations, head + 50, value);
        openFully(locations);
    }

    /* A zero units-per-em must not turn into a division by zero. */
    Bytes zeroScale = readFont(StaticFont);
    writeUInt16(zeroScale, findTable(zeroScale, "head") + 18, 0);
    TRFontFileRef fontFile = TRFontFileCreateFromMemory(zeroScale.data(), zeroScale.size());
    if (fontFile) {
        TRTypefaceRef typeface = TRFontFileGetTypeface(fontFile, 0);
        if (typeface) {
            (void)TRTypefaceGetGlyphAdvance(typeface, 1, 16.0f, TRFalse);
        }

        TRFontFileRelease(fontFile);
    }
}

void MalformedFontTests::testBadNameTable() {
    const uint32_t values[] = { 0, 1, 0x7FFF, 0xFFFF };
    const char *const fontNames[] = { StaticFont, NamesFont, VariableFont };

    for (const char *fontName : fontNames) {
        Bytes data = readFont(fontName);
        size_t name = findTable(data, "name");
        assert(name != 0);

        for (uint32_t value : values) {
            /* The record count and the offset of the strings are out of the table. */
            Bytes count = data;
            writeUInt16(count, name + 2, value);
            openFully(count);

            Bytes storage = data;
            writeUInt16(storage, name + 4, value);
            openFully(storage);
        }

        /* Every record gets a string that lies outside of the table. */
        size_t recordCount = readUInt16(data, name + 2);
        for (uint32_t length : values) {
            Bytes strings = data;
            for (size_t i = 0; i < recordCount; i++) {
                size_t record = name + 6 + (i * 12);

                writeUInt16(strings, record + 8, length);
                writeUInt16(strings, record + 10, 0xFFFF);
            }

            openFully(strings);
        }
    }
}

void MalformedFontTests::testBadVariationTable() {
    const uint32_t values[] = { 0, 1, 0x7FFF, 0xFFFF };

    Bytes data = readFont(VariableFont);
    size_t fvar = findTable(data, "fvar");
    assert(fvar != 0);

    for (uint32_t value : values) {
        /* Offset to the axes, the number of axes, the number of named instances. */
        Bytes offset = data;
        writeUInt16(offset, fvar + 4, value);
        openFully(offset);

        Bytes axes = data;
        writeUInt16(axes, fvar + 8, value);
        openFully(axes);

        Bytes instances = data;
        writeUInt16(instances, fvar + 12, value);
        openFully(instances);

        Bytes sizes = data;
        writeUInt16(sizes, fvar + 10, value);
        writeUInt16(sizes, fvar + 14, value);
        openFully(sizes);
    }

    /* Axis ranges that are reversed or not sensible numbers. */
    size_t axis = fvar + readUInt16(data, fvar + 4);
    const uint32_t fixedValues[] = { 0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    const size_t fields[] = { 4, 8, 12 };
    for (uint32_t value : fixedValues) {
        for (size_t field : fields) {
            Bytes range = data;
            writeUInt32(range, axis + field, value);
            openFully(range);
        }
    }

    /* The variation tables of the glyphs and the design space map. */
    const char *const tags[] = { "gvar", "avar", "STAT", "HVAR" };
    for (const char *tag : tags) {
        size_t record = findRecord(data, tag);

        if (record != 0) {
            size_t table = readUInt32(data, record + 8);
            size_t length = readUInt32(data, record + 12);
            Bytes filled = data;

            for (size_t i = 4; i < length && i < 32; i++) {
                filled[table + i] = 0xFF;
            }
            openFully(filled);
        }
    }

    /* A truncated fvar table. */
    Bytes shortTable = data;
    writeUInt32(shortTable, findRecord(shortTable, "fvar") + 12, 8);
    openFully(shortTable);
}

void MalformedFontTests::testBadPaletteTable() {
    /* The oversized palettes of the table are covered by FaceMetadataTests. */
    const char *const fontNames[] = { ColorFont, PaletteFont };

    for (const char *fontName : fontNames) {
        Bytes data = readFont(fontName);
        size_t record = findRecord(data, "CPAL");
        size_t cpal = findTable(data, "CPAL");
        size_t length = readUInt32(data, record + 12);
        assert(cpal != 0 && length > 14);

        /* The table is cut inside of its header, its palette indices and its colors. */
        const uint32_t cuts[] = { 1, 2, 6, 11, 12, 13, 14, 15, 20, 21, static_cast<uint32_t>(length - 1) };
        for (uint32_t cut : cuts) {
            Bytes shortTable = data;
            writeUInt32(shortTable, record + 12, cut);
            openFully(shortTable);
        }

        /* Zero or extreme counts in every field of the header. */
        const size_t fields[] = { 2, 4, 6 };
        const uint32_t counts[] = { 0, 1, 0x7FFF, 0xFFFF };
        for (size_t field : fields) {
            for (uint32_t value : counts) {
                Bytes patched = data;
                writeUInt16(patched, cpal + field, value);
                openFully(patched);
            }
        }

        /* The colors start outside of the table, or the palette indices point past the colors. */
        const uint32_t offsets[] = { 0, 4, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFF };
        for (uint32_t value : offsets) {
            Bytes colors = data;
            writeUInt32(colors, cpal + 8, value);
            openFully(colors);
        }

        size_t paletteCount = readUInt16(data, cpal + 4);
        Bytes indices = data;
        for (size_t i = 0; i < paletteCount; i++) {
            writeUInt16(indices, cpal + 12 + (i * 2), 0xFFFF);
        }
        openFully(indices);

        /* Version 1 header with offsets to the types, labels and entry labels outside. */
        size_t extra = cpal + 12 + (paletteCount * 2);
        Bytes version1 = data;
        writeUInt16(version1, cpal, 1);
        for (size_t i = 0; i < 3 && extra + (i * 4) + 4 <= data.size(); i++) {
            writeUInt32(version1, extra + (i * 4), 0xFFFFFFFF);
        }
        openFully(version1);
    }

    /* The color tables that refer to the palette. */
    const char *const tags[] = { "COLR", "CBDT", "CBLC", "sbix", "SVG " };
    for (const char *tag : tags) {
        for (const char *fontName : AllFonts) {
            Bytes data = readFont(fontName);
            size_t record = findRecord(data, tag);

            if (record != 0) {
                size_t table = readUInt32(data, record + 8);
                size_t length = readUInt32(data, record + 12);
                Bytes filled = data;

                for (size_t i = 2; i < length && i < 40; i++) {
                    filled[table + i] = 0xFF;
                }
                openFully(filled);
            }
        }
    }
}

/* Deterministic pseudo-random numbers, so a failure can be reproduced. */
static uint32_t nextRandom(uint32_t &state) {
    state = (state * 1664525u) + 1013904223u;
    return state >> 8;
}

void MalformedFontTests::testGarbageData() {
    const size_t sizes[] = { 1, 2, 3, 4, 11, 12, 13, 16, 28, 64, 256, 4096 };
    uint32_t state = 12345;

    for (size_t size : sizes) {
        Bytes zeros(size, 0);
        Bytes ones(size, 0xFF);
        assert(TRFontFileCreateFromMemory(zeros.data(), size) == nullptr);
        assert(TRFontFileCreateFromMemory(ones.data(), size) == nullptr);

        Bytes random(size);
        for (auto &byte : random) {
            byte = static_cast<uint8_t>(nextRandom(state));
        }
        openFully(random);

        /* A valid header with a table directory made of garbage. */
        if (size >= HeaderSize) {
            Bytes header = random;
            writeUInt32(header, 0, 0x00010000);
            writeUInt16(header, 4, static_cast<uint32_t>(size / RecordSize));
            openFully(header);
        }
    }

    /* The body of a real font is replaced by garbage behind a valid table directory. */
    for (const char *fontName : AllFonts) {
        Bytes data = readFont(fontName);

        for (size_t i = directoryEnd(data); i < data.size(); i++) {
            data[i] = static_cast<uint8_t>(nextRandom(state));
        }
        openFully(data);

        for (size_t i = directoryEnd(data); i < data.size(); i++) {
            data[i] = 0;
        }
        openFully(data);
    }

    /* Text and markup, which a user could pick by mistake. */
    string html = "<!DOCTYPE html><html><body>not a font</body></html>";
    assert(TRFontFileCreateFromMemory(html.data(), html.size()) == nullptr);

    string woff = "wOFF";
    woff.append(60, '\0');
    openFully(Bytes(woff.begin(), woff.end()));
}

void MalformedFontTests::testCollectionHeader() {
    /* A collection header that has no fonts, or offsets to fonts that are not in the file. */
    Bytes header(64, 0);
    memcpy(header.data(), "ttcf", 4);
    writeUInt32(header, 4, 0x00010000);

    const uint32_t counts[] = { 0, 1, 2, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFF };
    const uint32_t offsets[] = { 0, 12, 16, 63, 64, 0xFFFFFFFF };
    for (uint32_t count : counts) {
        for (uint32_t offset : offsets) {
            Bytes data = header;
            writeUInt32(data, 8, count);
            writeUInt32(data, 12, offset);

            openFully(data);
        }
    }

    /* A real font behind a collection header that points to it. */
    Bytes font = readFont(StaticFont);
    Bytes collection(16, 0);
    memcpy(collection.data(), "ttcf", 4);
    writeUInt32(collection, 4, 0x00010000);
    writeUInt32(collection, 8, 1);
    writeUInt32(collection, 12, 16);

    size_t tableCount = readUInt16(font, 4);
    for (size_t i = 0; i < tableCount; i++) {
        size_t record = HeaderSize + (i * RecordSize);
        writeUInt32(font, record + 8, readUInt32(font, record + 8) + 16);
    }
    collection.insert(collection.end(), font.begin(), font.end());
    assert(openAndExercise(collection) == 1);

    /* The collection points to itself. */
    Bytes selfReference = collection;
    writeUInt32(selfReference, 12, 0);
    openFully(selfReference);

    /* More fonts than the file holds. */
    Bytes tooMany = collection;
    writeUInt32(tooMany, 8, 1000);
    openFully(tooMany);
}

void MalformedFontTests::testByteCorruption() {
    uint32_t state = 987654321;

    for (const char *fontName : AllFonts) {
        Bytes original = readFont(fontName);
        size_t rounds = original.size() > 4096 ? MutationRounds / 8 : MutationRounds;

        for (size_t round = 0; round < rounds; round++) {
            Bytes data = original;
            size_t flips = 1 + (nextRandom(state) % 8);

            for (size_t i = 0; i < flips; i++) {
                size_t index = nextRandom(state) % data.size();

                data[index] = static_cast<uint8_t>(nextRandom(state));
            }

            openFully(data);
        }
    }
}

#ifdef STANDALONE_TESTING

int main() {
    MalformedFontTests tests;
    tests.run();

    return 0;
}

#endif
