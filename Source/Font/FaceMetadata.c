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

#include <string.h>

#include <Tehreer/TRTypeface.h>
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

    TRWeight weight;
    TRWidth width;
    TRSlope slope;

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
#define COUNT               7

static RawMetadata *AllocateRawMetadata(MemoryRef memory, TRUInteger variationAxisCount,
    TRUInteger namedStyleCount, TRUInteger paletteEntryCount, TRUInteger predefinedPaletteCount)
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

    if (MemoryAllocateChunks(memory, sizes, COUNT, pointers)) {
        rawMetadata = pointers[RAW_METADATA];
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
#undef COUNT


static RawMetadata *CreateRawMetadata(MemoryRef memory, FT_Face ftFace)
{
    FreeTypeRef freetype = FreeTypeGetDefault();
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
        paletteEntryCount, predefinedPaletteCount);

    if (rawMetadata) {
        NameString emptyName = { NULL, 0, SFNTEncodingUnknown };
        TRUInteger nameCount = 0;
        TRUInteger nameBytes = 0;
        const TT_OS2 *os2Table;
        const TT_Header *headTable;

        os2Table = FT_Get_Sfnt_Table(ftFace, FT_SFNT_OS2);
        headTable = FT_Get_Sfnt_Table(ftFace, FT_SFNT_HEAD);

        SearchFamilyName(ftFace, os2Table, &rawMetadata->familyName);
        nameCount += 1;
        nameBytes += rawMetadata->familyName.length;

        SearchSubfamilyName(ftFace, os2Table, &rawMetadata->subfamilyName);
        nameCount += 1;
        nameBytes += rawMetadata->subfamilyName.length;

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

        if (ftVariations) {
            TRUInteger index;

            for (index = 0; index < variationAxisCount; index++) {
                const FT_Var_Axis *ftAxis = &ftVariations->axis[index];
                VariationAxis *variationAxis = &rawMetadata->variationAxes[index];

                SearchEnglishName(ftFace, ftAxis->strid, &variationAxis->name);
                nameCount += 1;
                nameBytes += variationAxis->name.length;

                variationAxis->tag = ftAxis->tag;
                variationAxis->minValue = ftAxis->minimum;
                variationAxis->defaultValue = ftAxis->def;
                variationAxis->maxValue = ftAxis->maximum;
            }

            for (index = 0; index < namedStyleCount; index++) {
                const FT_Var_Named_Style *ftStyle = &ftVariations->namedstyle[index];
                NamedStyle *namedStyle = &rawMetadata->namedStyles[index];

                memcpy(namedStyle->coordinates, ftStyle->coords, variationAxisCount * sizeof(FT_Fixed));

                SearchEnglishName(ftFace, ftStyle->strid, &namedStyle->subfamilyName);
                nameCount += 1;
                nameBytes += namedStyle->subfamilyName.length;

                if (ftStyle->psid == 0xFFFF) {
                    namedStyle->postScriptName = emptyName;
                } else {
                    SearchEnglishName(ftFace, ftStyle->psid, &namedStyle->postScriptName);
                    nameCount += 1;
                    nameBytes += namedStyle->postScriptName.length;
                }
            }
        }

        if (hasPalettes) {
            const FT_UShort *paletteNameIDs = ftPalette.palette_name_ids;
            const FT_UShort *paletteFlags = ftPalette.palette_flags;
            const FT_UShort *entryNameIDs = ftPalette.palette_entry_name_ids;
            NameString *entryNames = rawMetadata->paletteEntryNames;
            TRUInteger index;

            for (index = 0; index < paletteEntryCount; index++) {
                FT_UShort nameID = (entryNameIDs ? entryNameIDs[index] : 0xFFFF);

                if (nameID == 0xFFFF) {
                    entryNames[index] = emptyName;
                } else {
                    SearchEnglishName(ftFace, nameID, &entryNames[index]);
                    nameCount += 1;
                    nameBytes += entryNames[index].length;
                }
            }

            for (index = 0; index < predefinedPaletteCount; index++) {
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
                if (nameID == 0xFFFF) {
                    palette->name = emptyName;
                } else {
                    SearchEnglishName(ftFace, nameID, &palette->name);
                    nameCount += 1;
                    nameBytes += palette->name.length;
                }

                palette->flags = (paletteFlags ? paletteFlags[index] : 0);
            }
        }

        rawMetadata->nameBytes = nameBytes;
        rawMetadata->nameCount = nameCount;
    }

    if (ftVariations) {
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
#define COUNT               9

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

    faceMetadata = ObjectCreate(sizes, COUNT, pointers, NULL);

    if (faceMetadata) {
        NameWriter writer;

        NameWriterInitialize(&writer, pointers[NAME_STRINGS], pointers[NAME_DATA]);

        faceMetadata = pointers[FACE_METADATA];
        faceMetadata->variationAxesPtr = pointers[VARIATION_AXES];
        faceMetadata->namedStylesPtr = pointers[NAMED_STYLES];
        faceMetadata->paletteEntriesPtr = pointers[PALETTE_ENTRIES];
        faceMetadata->predefinedPalettesPtr = pointers[PREDEFINED_PALETTES];

        faceMetadata->familyName = NameWriterWrite(&writer, &rawMetadata->familyName);

        faceMetadata->subfamilyName = NameWriterWrite(&writer, &rawMetadata->subfamilyName);

        faceMetadata->weight = rawMetadata->weight;
        faceMetadata->width = rawMetadata->width;
        faceMetadata->slope = rawMetadata->slope;

        if (rawMetadata->variationAxisCount > 0) {
            TRUInteger count = rawMetadata->variationAxisCount;
            TRUInteger index;

            faceMetadata->variationAxesPtr = pointers[VARIATION_AXES];
            faceMetadata->variationAxisCount = count;

            for (index = 0; index < count; index++) {
                VariationAxis *rawAxis = &rawMetadata->variationAxes[index];
                TRVariationAxis *faceAxis = &faceMetadata->variationAxesPtr[index];

                faceAxis->name = NameWriterWrite(&writer, &rawAxis->name);

                faceAxis->tag = rawAxis->tag;
                faceAxis->minValue = rawAxis->minValue / 65536.0;
                faceAxis->maxValue = rawAxis->maxValue / 65536.0;
                faceAxis->defaultValue = rawAxis->defaultValue / 65536.0;
            }
        } else {
            faceMetadata->variationAxesPtr = NULL;
            faceMetadata->variationAxisCount = 0;
        }

        if (rawMetadata->namedStyleCount > 0) {
            TRFloat *coordinates = pointers[COORDINATES];
            TRUInteger count = rawMetadata->namedStyleCount;
            TRUInteger index;

            faceMetadata->namedStylesPtr = pointers[NAMED_STYLES];
            faceMetadata->namedStyleCount = count;

            for (index = 0; index < count; index++) {
                NamedStyle *rawStyle = &rawMetadata->namedStyles[index];
                TRNamedStyle *faceStyle = &faceMetadata->namedStylesPtr[index];
                TRUInteger coordCount = rawMetadata->variationAxisCount;
                TRUInteger coordIndex;

                faceStyle->subfamilyName = NameWriterWrite(&writer, &rawStyle->subfamilyName);

                for (coordIndex = 0; coordIndex < coordCount; coordIndex++) {
                    coordinates[coordIndex] = rawStyle->coordinates[coordIndex] / 65536.0;
                }

                faceStyle->coordinatesPtr = coordinates;
                faceStyle->coordinateCount = coordCount;
                coordinates += coordCount;

                faceStyle->postScriptName = NameWriterWrite(&writer, &rawStyle->postScriptName);
            }
        } else {
            faceMetadata->namedStylesPtr = NULL;
            faceMetadata->namedStyleCount = 0;
        }

        if (rawMetadata->paletteEntryCount > 0) {
            TRUInteger count = rawMetadata->paletteEntryCount;
            TRUInteger index;

            faceMetadata->paletteEntriesPtr = pointers[PALETTE_ENTRIES];
            faceMetadata->paletteEntryCount = count;

            for (index = 0; index < count; index++) {
                NameString *rawName = &rawMetadata->paletteEntryNames[index];
                TRPaletteEntry *faceEntry = &faceMetadata->paletteEntriesPtr[index];

                faceEntry->name = NameWriterWrite(&writer, rawName);
            }
        } else {
            faceMetadata->paletteEntriesPtr = NULL;
            faceMetadata->paletteEntryCount = 0;
        }

        if (rawMetadata->predefinedPaletteCount > 0) {
            TRColor *colors = pointers[COLORS];
            TRUInteger count = rawMetadata->predefinedPaletteCount;
            TRUInteger index;

            faceMetadata->predefinedPalettesPtr = pointers[PREDEFINED_PALETTES];
            faceMetadata->predefinedPaletteCount = count;

            for (index = 0; index < count; index++) {
                ColorPalette *rawPalette = &rawMetadata->predefinedPalettes[index];
                FT_Color *rawColors = rawPalette->colors;
                TRPredefinedPalette *facePalette = &faceMetadata->predefinedPalettesPtr[index];
                TRUInteger colorCount = rawMetadata->paletteEntryCount;
                TRUInteger colorIndex;

                facePalette->name = NameWriterWrite(&writer, &rawPalette->name);

                for (colorIndex = 0; colorIndex < colorCount; colorIndex++) {
                    colors[colorIndex] = TRColorMake(rawColors[colorIndex].alpha,
                        rawColors[colorIndex].red, rawColors[colorIndex].green,
                        rawColors[colorIndex].blue);
                }

                facePalette->colorsPtr = colors;
                facePalette->colorCount = colorCount;
                facePalette->flags = rawPalette->flags;

                colors += colorCount;
            }
        } else {
            faceMetadata->predefinedPalettesPtr = NULL;
            faceMetadata->predefinedPaletteCount = 0;
        }
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

TR_INTERNAL FaceMetadataRef FaceMetadataRetain(FaceMetadataRef faceMetadata)
{
    return ObjectRetain((ObjectRef)faceMetadata);
}

TR_INTERNAL void FaceMetadataRelease(FaceMetadataRef faceMetadata)
{
    ObjectRelease((ObjectRef)faceMetadata);
}
