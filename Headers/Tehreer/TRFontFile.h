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

TR_EXTERN_C_BEGIN

/**
 * Opaque reference to a font file object.
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
 *      A reference to a font file object, or `NULL` if the path is `NULL` or the file does not
 *      contain a usable font.
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
 *      A reference to a font file object, or `NULL` if the data is empty or does not contain a
 *      usable font.
 */
TR_PUBLIC TRFontFileRef TRFontFileCreateFromMemory(const void *memory, TRUInteger size);

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
