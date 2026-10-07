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

#ifndef _TEHREER_LAYOUT_BREAK_RESOLVER_H
#define _TEHREER_LAYOUT_BREAK_RESOLVER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRBase.h>
#include <API/TRTypesetter.h>

/*
 * Suggests the end of a line that starts at the start of the range and has to fit in the extent.
 * The range MUST NOT be empty and MUST be within the text. At least one character is taken, and the
 * suggestion never goes past the paragraph that the range starts in. A block replacement ends a
 * line before it, and a line that starts with one has it and the whitespace that follows.
 */
TR_INTERNAL TRUInteger BreakResolverSuggestForwardBreak(TRTypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode);

/*
 * Suggests the start of a line that ends at the end of the range and has to fit in the extent. The
 * range MUST NOT be empty and MUST be within the text. At least one character is taken, and the
 * suggestion never goes before the paragraph that the range ends in.
 */
TR_INTERNAL TRUInteger BreakResolverSuggestBackwardBreak(TRTypesetterRef typesetter, TRFloat extent,
    TRUInteger start, TRUInteger end, TRBreakMode breakMode);

#endif
