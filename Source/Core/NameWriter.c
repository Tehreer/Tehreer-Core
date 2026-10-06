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

#include <stddef.h>

#include <API/TRBase.h>
#include <SFNT/Utilities.h>

#include "NameWriter.h"

TR_INTERNAL void NameWriterInitialize(NameWriterRef writer, TRStringView *views, void *codeUnits)
{
    writer->views = views;
    writer->codeUnits = codeUnits;
}

TR_INTERNAL TRStringView *NameWriterWrite(NameWriterRef writer, NameString *name)
{
    TRStringView *view = writer->views;

    if (name->length == 0) {
        return NULL;
    }

    view->buffer = writer->codeUnits;

    if (!NameStringToStringView(name, view)) {
        return NULL;
    }

    writer->views += 1;
    writer->codeUnits += view->length;

    return view;
}
