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

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H

#include <string.h>

#include <Tehreer/TRTypeface.h>
#include <API/TRAssert.h>
#include <API/TRBase.h>
#include <API/TRTypeface.h>
#include <Core/Memory.h>
#include <Core/NameWriter.h>
#include <Graphics/FreeType.h>
#include <SFNT/Utilities.h>

#include "FaceMetadata.h"

typedef struct _VariationAxis {
    TRTag tag;
    NameString name;
    FT_Fixed minValue;
    FT_Fixed defaultValue;
    FT_Fixed maxValue;
    TRUInt32 flags;
} VariationAxis;

typedef struct _NamedStyle {
    FT_Fixed *coordinates;
    NameString subfamilyName;
    NameString postScriptName;
} NamedStyle;

typedef struct _ColorPalette {
    FT_Color *colors;
    NameString name;
    TRUInt16 flags;
} ColorPalette;

typedef struct _RawMetadata {
    NameString familyName;
    NameString subfamilyName;
    NameString fullName;

    TRWeight weight;
    TRWidth width;
    TRSlope slope;

    TRBoolean isScalable;

    TRBitmapStrike *bitmapStrikes;
    TRUInteger bitmapStrikeCount;

    VariationAxis *variationAxes;
    TRUInteger variationAxisCount;

    NamedStyle *namedStyles;
    TRUInteger namedStyleCount;

    NameString *paletteEntryNames;
    TRUInteger paletteEntryCount;

    ColorPalette *predefinedPalettes;
    TRUInteger predefinedPaletteCount;

    TRUInteger nameCount;
    TRUInteger nameBytes;
} RawMetadata;


#define RAW_METADATA        0
#define VARIATION_AXES      1
#define NAMED_STYLES        2
#define PALETTE_ENTRY_NAMES 3
#define PREDEFINED_PALETTES 4
#define COORDINATES         5
#define COLORS              6
#define BITMAP_STRIKES      7
#define COUNT               8

static RawMetadata *AllocateRawMetadata(MemoryRef memory, TRUInteger variationAxisCount,
    TRUInteger namedStyleCount, TRUInteger paletteEntryCount, TRUInteger predefinedPaletteCount,
    TRUInteger bitmapStrikeCount)
{
    void *pointers[COUNT] = { NULL };
    TRUInteger sizes[COUNT] = { 0 };
    RawMetadata *rawMetadata = NULL;

    sizes[RAW_METADATA]        = sizeof(RawMetadata);
    sizes[VARIATION_AXES]      = sizeof(VariationAxis) * variationAxisCount;
    sizes[NAMED_STYLES]        = sizeof(NamedStyle) * namedStyleCount;
    sizes[PALETTE_ENTRY_NAMES] = sizeof(NameString) * paletteEntryCount;
    sizes[PREDEFINED_PALETTES] = sizeof(ColorPalette) * predefinedPaletteCount;
    sizes[COORDINATES]         = (sizeof(FT_Fixed) * variationAxisCount) * namedStyleCount;
    sizes[COLORS]              = (sizeof(FT_Color) * paletteEntryCount) * predefinedPaletteCount;
    sizes[BITMAP_STRIKES]      = sizeof(TRBitmapStrike) * bitmapStrikeCount;

    if (MemoryAllocateChunks(memory, sizes, COUNT, pointers)) {
        rawMetadata = pointers[RAW_METADATA];
        rawMetadata->isScalable = TRTrue;
        rawMetadata->bitmapStrikes = pointers[BITMAP_STRIKES];
        rawMetadata->bitmapStrikeCount = bitmapStrikeCount;
        rawMetadata->variationAxes = pointers[VARIATION_AXES];
        rawMetadata->variationAxisCount = variationAxisCount;
        rawMetadata->namedStyles = pointers[NAMED_STYLES];
        rawMetadata->namedStyleCount = namedStyleCount;
        rawMetadata->paletteEntryNames = pointers[PALETTE_ENTRY_NAMES];
        rawMetadata->paletteEntryCount = paletteEntryCount;
        rawMetadata->predefinedPalettes = pointers[PREDEFINED_PALETTES];
        rawMetadata->predefinedPaletteCount = predefinedPaletteCount;

        if (namedStyleCount > 0) {
            NamedStyle *namedStyles = rawMetadata->namedStyles;
            FT_Fixed *coordinates = pointers[COORDINATES];
            TRUInteger styleIndex;

            for (styleIndex = 0; styleIndex < namedStyleCount; styleIndex++) {
                namedStyles[styleIndex].coordinates = coordinates;
                coordinates += variationAxisCount;
            }
        }

        if (predefinedPaletteCount > 0) {
            ColorPalette *palettes = rawMetadata->predefinedPalettes;
            FT_Color *colors = pointers[COLORS];
            TRUInteger paletteIndex;

            for (paletteIndex = 0; paletteIndex < predefinedPaletteCount; paletteIndex++) {
                palettes[paletteIndex].colors = colors;
                colors += paletteEntryCount;
            }
        }
    }

    return rawMetadata;
}

#undef RAW_METADATA
#undef VARIATION_AXES
#undef NAMED_STYLES
#undef PALETTE_ENTRY_NAMES
#undef PREDEFINED_PALETTES
#undef COORDINATES
#undef COLORS
#undef BITMAP_STRIKES
#undef COUNT

/* The `fvar` table starts with a header whose instance count is a big endian 16-bit number. */
#define FvarHeaderSize              16
#define FvarInstanceCountOffset     12

#define ReadUInt16BE(bytes_, offset_)   \
    (((TRUInteger)(bytes_)[offset_] << 8) | (bytes_)[(offset_) + 1])

/* Loads the header of the `fvar` table, and returns whether it was loaded completely. */
static TRBoolean LoadFvarHeader(FT_Face ftFace, FT_Byte *header)
{
    FT_ULong length = FvarHeaderSize;
    FT_Error error = FT_Load_Sfnt_Table(ftFace, FT_MAKE_TAG('f', 'v', 'a', 'r'), 0, header,
        &length);

    return (error == 0 && length == FvarHeaderSize);
}

/*
 * FreeType adds the default instance as the last named style if the font has no record for it,
 * while it is meant to come first. Returns TRTrue if the last style has been added that way, which
 * is found by comparing the style count with the instance count of the `fvar` table.
 */
static TRBoolean HasAppendedDefaultStyle(FT_Face ftFace, TRUInteger namedStyleCount)
{
    TRBoolean hasAppended = TRFalse;
    FT_Byte header[FvarHeaderSize];

    if (namedStyleCount > 0 && LoadFvarHeader(ftFace, header)) {
        TRUInteger instanceCount = ReadUInt16BE(header, FvarInstanceCountOffset);

        hasAppended = (namedStyleCount == instanceCount + 1);
    }

    return hasAppended;
}

#undef FvarHeaderSize
#undef FvarInstanceCountOffset
#undef ReadUInt16BE

/* Counts a name that has to be written, along with the code units that it takes. */
static void TallyName(const NameString *name, TRUInteger *nameCount, TRUInteger *nameBytes)
{
    *nameCount += 1;
    *nameBytes += NameStringGetCapacity(name);
}

/* Counts the name that an id refers to, unless the id says that there is none. */
static void SearchOptionalName(FT_Face ftFace, FT_UShort nameID, NameString *name,
    TRUInteger *nameCount, TRUInteger *nameBytes)
{
    if (nameID == 0xFFFF) {
        static const NameString emptyName = { NULL, 0, SFNTEncodingUnknown };

        *name = emptyName;
    } else {
        SearchEnglishName(ftFace, nameID, name);
        TallyName(name, nameCount, nameBytes);
    }
}

static void ReadFontNames(FT_Face ftFace, const TT_OS2 *os2Table, RawMetadata *rawMetadata,
    TRUInteger *nameCount, TRUInteger *nameBytes)
{
    SearchFamilyName(ftFace, os2Table, &rawMetadata->familyName);
    TallyName(&rawMetadata->familyName, nameCount, nameBytes);

    SearchSubfamilyName(ftFace, os2Table, &rawMetadata->subfamilyName);
    TallyName(&rawMetadata->subfamilyName, nameCount, nameBytes);

    SearchFullName(ftFace, &rawMetadata->fullName);
    TallyName(&rawMetadata->fullName, nameCount, nameBytes);
}

static void ReadDescription(RawMetadata *rawMetadata, const TT_OS2 *os2Table,
    const TT_Header *headTable)
{
    rawMetadata->weight = TRWeightRegular;
    rawMetadata->width = TRWidthNormal;
    rawMetadata->slope = TRSlopePlain;

    if (os2Table) {
        rawMetadata->weight = os2Table->usWeightClass;
        rawMetadata->width = os2Table->usWidthClass;

        if (os2Table->fsSelection & FSSelectionOblique) {
            rawMetadata->slope = TRSlopeOblique;
        } else if (os2Table->fsSelection & FSSelectionItalic) {
            rawMetadata->slope = TRSlopeItalic;
        }
    } else if (headTable) {
        if (headTable->Mac_Style & MacStyleBold) {
            rawMetadata->weight = TRWeightBold;
        }

        if (headTable->Mac_Style & MacStyleCondensed) {
            rawMetadata->width = TRWidthCondensed;
        } else if (headTable->Mac_Style & MacStyleExtended) {
            rawMetadata->width = TRWidthExpanded;
        }

        if (headTable->Mac_Style & MacStyleItalic) {
            rawMetadata->slope = TRSlopeItalic;
        }
    }
}

static void ReadVariationAxes(FT_Face ftFace, FT_MM_Var *ftVariations, RawMetadata *rawMetadata,
    TRUInteger *nameCount, TRUInteger *nameBytes)
{
    TRUInteger index;

    for (index = 0; index < rawMetadata->variationAxisCount; index++) {
        const FT_Var_Axis *ftAxis = &ftVariations->axis[index];
        VariationAxis *variationAxis = &rawMetadata->variationAxes[index];
        FT_UInt flags = 0;

        SearchEnglishName(ftFace, ftAxis->strid, &variationAxis->name);
        TallyName(&variationAxis->name, nameCount, nameBytes);

        variationAxis->tag = ftAxis->tag;
        variationAxis->minValue = ftAxis->minimum;
        variationAxis->defaultValue = ftAxis->def;
        variationAxis->maxValue = ftAxis->maximum;
        variationAxis->flags = 0;

        if (FT_Get_Var_Axis_Flags(ftVariations, (FT_UInt)index, &flags) == 0) {
            variationAxis->flags = flags;
        }
    }
}

static void ReadNamedStyles(FT_Face ftFace, FT_MM_Var *ftVariations, RawMetadata *rawMetadata,
    TRUInteger *nameCount, TRUInteger *nameBytes)
{
    static const NameString emptyName = { NULL, 0, SFNTEncodingUnknown };
    TRUInteger namedStyleCount = rawMetadata->namedStyleCount;
    TRBoolean hasAppendedDefault = HasAppendedDefaultStyle(ftFace, namedStyleCount);
    TRUInteger index;

    for (index = 0; index < namedStyleCount; index++) {
        const FT_Var_Named_Style *ftStyle = &ftVariations->namedstyle[index];
        TRUInteger styleIndex = (hasAppendedDefault
                                 ? (index == namedStyleCount - 1 ? 0 : index + 1)
                                 : index);
        NamedStyle *namedStyle = &rawMetadata->namedStyles[styleIndex];

        memcpy(namedStyle->coordinates, ftStyle->coords,
            rawMetadata->variationAxisCount * sizeof(FT_Fixed));

        if (hasAppendedDefault && styleIndex == 0) {
            /* The default instance takes the style name of the font, and has no record. */
            namedStyle->subfamilyName = rawMetadata->subfamilyName;
            TallyName(&namedStyle->subfamilyName, nameCount, nameBytes);

            namedStyle->postScriptName = emptyName;
        } else {
            SearchEnglishName(ftFace, ftStyle->strid, &namedStyle->subfamilyName);
            TallyName(&namedStyle->subfamilyName, nameCount, nameBytes);

            SearchOptionalName(ftFace, ftStyle->psid, &namedStyle->postScriptName, nameCount,
                nameBytes);
        }
    }
}

static void ReadPaletteEntries(FT_Face ftFace, const FT_Palette_Data *ftPalette,
    RawMetadata *rawMetadata, TRUInteger *nameCount, TRUInteger *nameBytes)
{
    const FT_UShort *entryNameIDs = ftPalette->palette_entry_name_ids;
    NameString *entryNames = rawMetadata->paletteEntryNames;
    TRUInteger index;

    for (index = 0; index < rawMetadata->paletteEntryCount; index++) {
        FT_UShort nameID = (entryNameIDs ? entryNameIDs[index] : 0xFFFF);

        SearchOptionalName(ftFace, nameID, &entryNames[index], nameCount, nameBytes);
    }
}

static void ReadPredefinedPalettes(FT_Face ftFace, const FT_Palette_Data *ftPalette,
    RawMetadata *rawMetadata, TRUInteger *nameCount, TRUInteger *nameBytes)
{
    const FT_UShort *paletteNameIDs = ftPalette->palette_name_ids;
    const FT_UShort *paletteFlags = ftPalette->palette_flags;
    TRUInteger paletteEntryCount = rawMetadata->paletteEntryCount;
    TRUInteger index;

    for (index = 0; index < rawMetadata->predefinedPaletteCount; index++) {
        ColorPalette *palette = &rawMetadata->predefinedPalettes[index];
        FT_Color *colors = NULL;
        FT_UShort nameID;

        FT_Palette_Select(ftFace, index, &colors);

        if (colors) {
            memcpy(palette->colors, colors, paletteEntryCount * sizeof(FT_Color));
        } else {
            FT_Color blackColor = { 0x00, 0x00, 0x00, 0xFF };
            TRUInteger colorIndex;

            for (colorIndex = 0; colorIndex < paletteEntryCount; colorIndex++) {
                palette->colors[colorIndex] = blackColor;
            }
        }

        nameID = (paletteNameIDs ? paletteNameIDs[index] : 0xFFFF);
        SearchOptionalName(ftFace, nameID, &palette->name, nameCount, nameBytes);

        palette->flags = (paletteFlags ? paletteFlags[index] : 0);
    }
}

static void ReadBitmapStrikes(FT_Face ftFace, RawMetadata *rawMetadata)
{
    TRUInteger index;

    rawMetadata->isScalable = (FT_IS_SCALABLE(ftFace) != 0);

    for (index = 0; index < rawMetadata->bitmapStrikeCount; index++) {
        const FT_Bitmap_Size *ftSize = &ftFace->available_sizes[index];
        TRBitmapStrike *strike = &rawMetadata->bitmapStrikes[index];

        strike->pixelWidth = (TRFloat)ftSize->x_ppem / 64.0f;
        strike->pixelHeight = (TRFloat)ftSize->y_ppem / 64.0f;
    }
}

static RawMetadata *CreateRawMetadata(MemoryRef memory, FT_Face ftFace)
{
    RawMetadata *rawMetadata = NULL;
    TRUInteger variationAxisCount = 0;
    TRUInteger namedStyleCount = 0;
    TRUInteger paletteEntryCount = 0;
    TRUInteger predefinedPaletteCount = 0;
    FT_MM_Var *ftVariations = NULL;
    TRBoolean hasPalettes = TRFalse;
    FT_Palette_Data ftPalette;

    if (ftFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
        FT_Get_MM_Var(ftFace, &ftVariations);
    }
    if (ftFace->face_flags & FT_FACE_FLAG_COLOR) {
        hasPalettes = (FT_Palette_Data_Get(ftFace, &ftPalette) == FT_Err_Ok);
    }

    if (ftVariations) {
        variationAxisCount = ftVariations->num_axis;
        namedStyleCount = ftVariations->num_namedstyles;
    }
    if (hasPalettes) {
        paletteEntryCount = ftPalette.num_palette_entries;
        predefinedPaletteCount = ftPalette.num_palettes;
    }

    rawMetadata = AllocateRawMetadata(memory, variationAxisCount, namedStyleCount,
        paletteEntryCount, predefinedPaletteCount,
        (FT_HAS_FIXED_SIZES(ftFace) ? (TRUInteger)ftFace->num_fixed_sizes : 0));

    if (rawMetadata) {
        TRUInteger nameCount = 0;
        TRUInteger nameBytes = 0;
        const TT_OS2 *os2Table;
        const TT_Header *headTable;

        os2Table = FT_Get_Sfnt_Table(ftFace, FT_SFNT_OS2);
        headTable = FT_Get_Sfnt_Table(ftFace, FT_SFNT_HEAD);

        ReadFontNames(ftFace, os2Table, rawMetadata, &nameCount, &nameBytes);
        ReadDescription(rawMetadata, os2Table, headTable);
        ReadBitmapStrikes(ftFace, rawMetadata);

        if (ftVariations) {
            ReadVariationAxes(ftFace, ftVariations, rawMetadata, &nameCount, &nameBytes);
            ReadNamedStyles(ftFace, ftVariations, rawMetadata, &nameCount, &nameBytes);
        }

        if (hasPalettes) {
            ReadPaletteEntries(ftFace, &ftPalette, rawMetadata, &nameCount, &nameBytes);
            ReadPredefinedPalettes(ftFace, &ftPalette, rawMetadata, &nameCount, &nameBytes);
        }

        rawMetadata->nameBytes = nameBytes;
        rawMetadata->nameCount = nameCount;
    }

    if (ftVariations) {
        FreeTypeRef freetype = FreeTypeGetDefault();

        FT_Done_MM_Var(freetype->library, ftVariations);
    }

    return rawMetadata;
}


#define FACE_METADATA       0
#define VARIATION_AXES      1
#define NAMED_STYLES        2
#define PALETTE_ENTRIES     3
#define PREDEFINED_PALETTES 4
#define NAME_STRINGS        5
#define COORDINATES         6
#define COLORS              7
#define NAME_DATA           8
#define BITMAP_STRIKES      9
#define COUNT               10

static void WriteVariationAxes(FaceMetadata *faceMetadata, const RawMetadata *rawMetadata,
    NameWriterRef writer, TRVariationAxis *axes)
{
    TRUInteger count = rawMetadata->variationAxisCount;

    if (count > 0) {
        TRUInteger index;

        faceMetadata->variationAxesPtr = axes;
        faceMetadata->variationAxisCount = count;

        for (index = 0; index < count; index++) {
            VariationAxis *rawAxis = &rawMetadata->variationAxes[index];
            TRVariationAxis *faceAxis = &faceMetadata->variationAxesPtr[index];

            faceAxis->name = NameWriterWrite(writer, &rawAxis->name);

            faceAxis->tag = rawAxis->tag;
            faceAxis->minValue = rawAxis->minValue / 65536.0;
            faceAxis->maxValue = rawAxis->maxValue / 65536.0;
            faceAxis->defaultValue = rawAxis->defaultValue / 65536.0;
            faceAxis->flags = rawAxis->flags;
        }
    } else {
        faceMetadata->variationAxesPtr = NULL;
        faceMetadata->variationAxisCount = 0;
    }
}

static void WriteNamedStyle(TRNamedStyle *faceStyle, NamedStyle *rawStyle, TRUInteger coordCount,
    NameWriterRef writer, TRFloat *coordinates)
{
    TRUInteger coordIndex;

    faceStyle->subfamilyName = NameWriterWrite(writer, &rawStyle->subfamilyName);

    for (coordIndex = 0; coordIndex < coordCount; coordIndex++) {
        coordinates[coordIndex] = rawStyle->coordinates[coordIndex] / 65536.0;
    }

    faceStyle->coordinatesPtr = coordinates;
    faceStyle->coordinateCount = coordCount;

    faceStyle->postScriptName = NameWriterWrite(writer, &rawStyle->postScriptName);
}

static void WriteNamedStyles(FaceMetadata *faceMetadata, const RawMetadata *rawMetadata,
    NameWriterRef writer, TRNamedStyle *styles, TRFloat *coordinates)
{
    TRUInteger count = rawMetadata->namedStyleCount;

    if (count > 0) {
        TRUInteger coordCount = rawMetadata->variationAxisCount;
        TRUInteger index;

        faceMetadata->namedStylesPtr = styles;
        faceMetadata->namedStyleCount = count;

        for (index = 0; index < count; index++) {
            WriteNamedStyle(&faceMetadata->namedStylesPtr[index], &rawMetadata->namedStyles[index],
                coordCount, writer, coordinates);

            coordinates += coordCount;
        }
    } else {
        faceMetadata->namedStylesPtr = NULL;
        faceMetadata->namedStyleCount = 0;
    }
}

static void WritePaletteEntries(FaceMetadata *faceMetadata, const RawMetadata *rawMetadata,
    NameWriterRef writer, TRPaletteEntry *entries)
{
    TRUInteger count = rawMetadata->paletteEntryCount;

    if (count > 0) {
        TRUInteger index;

        faceMetadata->paletteEntriesPtr = entries;
        faceMetadata->paletteEntryCount = count;

        for (index = 0; index < count; index++) {
            NameString *rawName = &rawMetadata->paletteEntryNames[index];
            TRPaletteEntry *faceEntry = &faceMetadata->paletteEntriesPtr[index];

            faceEntry->name = NameWriterWrite(writer, rawName);
        }
    } else {
        faceMetadata->paletteEntriesPtr = NULL;
        faceMetadata->paletteEntryCount = 0;
    }
}

static void WritePredefinedPalette(TRPredefinedPalette *facePalette, ColorPalette *rawPalette,
    TRUInteger colorCount, NameWriterRef writer, TRColor *colors)
{
    FT_Color *rawColors = rawPalette->colors;
    TRUInteger colorIndex;

    facePalette->name = NameWriterWrite(writer, &rawPalette->name);

    for (colorIndex = 0; colorIndex < colorCount; colorIndex++) {
        colors[colorIndex] = TRColorMake(rawColors[colorIndex].alpha,
            rawColors[colorIndex].red, rawColors[colorIndex].green,
            rawColors[colorIndex].blue);
    }

    facePalette->colorsPtr = colors;
    facePalette->colorCount = colorCount;
    facePalette->flags = rawPalette->flags;
}

static void WritePredefinedPalettes(FaceMetadata *faceMetadata, const RawMetadata *rawMetadata,
    NameWriterRef writer, TRPredefinedPalette *palettes, TRColor *colors)
{
    TRUInteger count = rawMetadata->predefinedPaletteCount;

    if (count > 0) {
        TRUInteger colorCount = rawMetadata->paletteEntryCount;
        TRUInteger index;

        faceMetadata->predefinedPalettesPtr = palettes;
        faceMetadata->predefinedPaletteCount = count;

        for (index = 0; index < count; index++) {
            WritePredefinedPalette(&faceMetadata->predefinedPalettesPtr[index],
                &rawMetadata->predefinedPalettes[index], colorCount, writer, colors);

            colors += colorCount;
        }
    } else {
        faceMetadata->predefinedPalettesPtr = NULL;
        faceMetadata->predefinedPaletteCount = 0;
    }
}

static FaceMetadataRef CreateFaceMetadata(RawMetadata *rawMetadata)
{
    void *pointers[COUNT] = { NULL };
    TRUInteger sizes[COUNT] = { 0 };
    FaceMetadata *faceMetadata;

    sizes[FACE_METADATA]       = sizeof(FaceMetadata);
    sizes[VARIATION_AXES]      = sizeof(TRVariationAxis) * rawMetadata->variationAxisCount;
    sizes[NAMED_STYLES]        = sizeof(TRNamedStyle) * rawMetadata->namedStyleCount;
    sizes[PALETTE_ENTRIES]     = sizeof(TRStringView) * rawMetadata->paletteEntryCount;
    sizes[PREDEFINED_PALETTES] = sizeof(TRPredefinedPalette) * rawMetadata->predefinedPaletteCount;
    sizes[NAME_STRINGS]        = sizeof(TRStringView) * rawMetadata->nameCount;
    sizes[COORDINATES]         = (sizeof(TRFloat) * rawMetadata->variationAxisCount) * rawMetadata->namedStyleCount;
    sizes[COLORS]              = (sizeof(TRColor) * rawMetadata->paletteEntryCount) * rawMetadata->predefinedPaletteCount;
    sizes[NAME_DATA]           = rawMetadata->nameBytes;
    sizes[BITMAP_STRIKES]      = sizeof(TRBitmapStrike) * rawMetadata->bitmapStrikeCount;

    faceMetadata = ObjectCreate(sizes, COUNT, pointers, NULL);

    if (faceMetadata) {
        NameWriter writer;

        NameWriterInitialize(&writer, pointers[NAME_STRINGS], pointers[NAME_DATA]);

        faceMetadata = pointers[FACE_METADATA];
        faceMetadata->isScalable = rawMetadata->isScalable;
        faceMetadata->bitmapStrikesPtr = pointers[BITMAP_STRIKES];
        faceMetadata->bitmapStrikeCount = rawMetadata->bitmapStrikeCount;
        faceMetadata->variationAxesPtr = pointers[VARIATION_AXES];
        faceMetadata->namedStylesPtr = pointers[NAMED_STYLES];
        faceMetadata->paletteEntriesPtr = pointers[PALETTE_ENTRIES];
        faceMetadata->predefinedPalettesPtr = pointers[PREDEFINED_PALETTES];

        faceMetadata->familyName = NameWriterWrite(&writer, &rawMetadata->familyName);

        faceMetadata->subfamilyName = NameWriterWrite(&writer, &rawMetadata->subfamilyName);

        faceMetadata->fullName = NameWriterWrite(&writer, &rawMetadata->fullName);

        faceMetadata->weight = rawMetadata->weight;
        faceMetadata->width = rawMetadata->width;
        faceMetadata->slope = rawMetadata->slope;

        if (rawMetadata->bitmapStrikeCount > 0) {
            memcpy(faceMetadata->bitmapStrikesPtr, rawMetadata->bitmapStrikes,
                sizeof(TRBitmapStrike) * rawMetadata->bitmapStrikeCount);
        }

        WriteVariationAxes(faceMetadata, rawMetadata, &writer, pointers[VARIATION_AXES]);
        WriteNamedStyles(faceMetadata, rawMetadata, &writer, pointers[NAMED_STYLES],
            pointers[COORDINATES]);
        WritePaletteEntries(faceMetadata, rawMetadata, &writer, pointers[PALETTE_ENTRIES]);
        WritePredefinedPalettes(faceMetadata, rawMetadata, &writer, pointers[PREDEFINED_PALETTES],
            pointers[COLORS]);
    }

    return faceMetadata;
}

#undef FACE_METADATA
#undef VARIATION_AXES
#undef NAMED_STYLES
#undef PALETTE_ENTRIES
#undef PREDEFINED_PALETTES
#undef NAME_STRINGS
#undef COORDINATES
#undef COLORS
#undef NAME_DATA
#undef BITMAP_STRIKES
#undef COUNT


TR_INTERNAL FaceMetadataRef FaceMetadataCreate(FT_Face ftFace)
{
    FaceMetadataRef faceMetadata = NULL;
    Memory memory;
    RawMetadata *rawMetadata;

    MemoryInitialize(&memory);
    rawMetadata = CreateRawMetadata(&memory, ftFace);

    if (rawMetadata) {
        faceMetadata = CreateFaceMetadata(rawMetadata);
    }

    MemoryFinalize(&memory);

    return faceMetadata;
}

TR_INTERNAL TRUInteger FaceMetadataFindBitmapStrike(FaceMetadataRef faceMetadata,
    TRInt32 pixelHeight)
{
    TRUInteger upperIndex = TRInvalidIndex;
    TRUInteger lowerIndex = TRInvalidIndex;
    TRUInteger index;

    /* The face MUST have strikes. */
    TRAssert(faceMetadata->bitmapStrikeCount > 0);

    for (index = 0; index < faceMetadata->bitmapStrikeCount; index++) {
        TRFloat height = faceMetadata->bitmapStrikesPtr[index].pixelHeight;

        if (height * 64.0f >= (TRFloat)pixelHeight) {
            if (upperIndex == TRInvalidIndex
                    || height < faceMetadata->bitmapStrikesPtr[upperIndex].pixelHeight) {
                upperIndex = index;
            }
        } else if (lowerIndex == TRInvalidIndex
                   || height > faceMetadata->bitmapStrikesPtr[lowerIndex].pixelHeight) {
            lowerIndex = index;
        }
    }

    return (upperIndex != TRInvalidIndex ? upperIndex : lowerIndex);
}

TR_INTERNAL FaceMetadataRef FaceMetadataRetain(FaceMetadataRef faceMetadata)
{
    return ObjectRetain((ObjectRef)faceMetadata);
}

TR_INTERNAL void FaceMetadataRelease(FaceMetadataRef faceMetadata)
{
    ObjectRelease((ObjectRef)faceMetadata);
}
