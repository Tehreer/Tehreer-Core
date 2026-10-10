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

#ifndef _TEHREER_FONT_FILE_H
#define _TEHREER_FONT_FILE_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRTypeface.h>

TR_EXTERN_C_BEGIN

/**
 * Opaque reference to a font file object, which has the default typefaces of the font: the
 * typefaces of its faces, and of the named styles of the faces that have them, as in the variable
 * fonts. They are made when the font file is created, so they are there for as long as it is.
 */
typedef const struct _TRFontFile *TRFontFileRef;

/**
 * Creates a font file object from a file on disk.
 *
 * The path is copied, so the caller may free it right after the call.
 *
 * @param path
 *      Null-terminated path of the font file.
 * @return
 *      A font file object that the caller owns, or `NULL` if the path is `NULL` or the file does
 *      not contain a usable font.
 */
TR_PUBLIC TRFontFileRef TRFontFileCreateFromPath(const char *path);

/**
 * Creates a font file object from font data in memory.
 *
 * The data is copied, so the caller keeps ownership of the buffer and may free it right after the
 * call.
 *
 * @param memory
 *      Pointer to the font data.
 * @param size
 *      Size of the font data in bytes.
 * @return
 *      A font file object that the caller owns, or `NULL` if the data is empty or does not contain
 *      a usable font.
 */
TR_PUBLIC TRFontFileRef TRFontFileCreateFromMemory(const void *memory, TRUInteger size);

/**
 * Returns the number of default typefaces of the font file. A face that has named styles has a
 * typeface for each of them, which takes their variation coordinates. Any other face has a single
 * one, which uses the default coordinates of the font. The faces that cannot be loaded have none.
 * A font file always has at least one.
 */
TR_PUBLIC TRUInteger TRFontFileGetTypefaceCount(TRFontFileRef fontFile);

/**
 * Returns a default typeface of the font file, which is not retained for the caller, and stays
 * valid as long as the font file does. They are in the order of the faces, and of the named styles
 * of each face. A typeface can be retained to outlive the font file.
 *
 * @param fontFile
 *      The font file.
 * @param index
 *      Index of the typeface, less than `TRFontFileGetTypefaceCount()`.
 * @return
 *      The typeface, or `NULL` if the index is not less than the count.
 */
TR_PUBLIC TRTypefaceRef TRFontFileGetTypeface(TRFontFileRef fontFile, TRUInteger index);

/**
 * Increments the reference count of a font file object.
 *
 * @param fontFile
 *      The font file object whose reference count will be incremented.
 * @return
 *      The same font file object passed in as the parameter.
 */
TR_PUBLIC TRFontFileRef TRFontFileRetain(TRFontFileRef fontFile);

/**
 * Decrements the reference count of a font file object. The object will be deallocated when its
 * reference count reaches zero.
 *
 * @param fontFile
 *      The font file object whose reference count will be decremented.
 */
TR_PUBLIC void TRFontFileRelease(TRFontFileRef fontFile);

TR_EXTERN_C_END

#endif
