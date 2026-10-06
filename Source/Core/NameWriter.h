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

#ifndef _TEHREER_CORE_NAME_WRITER_H
#define _TEHREER_CORE_NAME_WRITER_H

#include <Tehreer/TRString.h>

#include <API/TRBase.h>
#include <SFNT/Utilities.h>

typedef struct _NameWriter {
    TRStringView *views;
    TRUInt16 *codeUnits;
} NameWriter, *NameWriterRef;

/**
 * Prepares a writer that stores converted names into the given string views and code unit buffer.
 * Both must be large enough to hold every name that will be written.
 */
TR_INTERNAL void NameWriterInitialize(NameWriterRef writer, TRStringView *views, void *codeUnits);

/**
 * Converts a name into the next string view and returns it, or returns `NULL` without consuming
 * any space if the name is empty or its encoding is not supported.
 */
TR_INTERNAL TRStringView *NameWriterWrite(NameWriterRef writer, NameString *name);

#endif
