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

#ifndef _TEHREER_LAYOUT_TOKEN_RESOLVER_H
#define _TEHREER_LAYOUT_TOKEN_RESOLVER_H

#include <Tehreer/TRBase.h>
#include <Tehreer/TRString.h>
#include <Tehreer/TRTypesetter.h>

#include <API/TRBase.h>
#include <API/TRComposedLine.h>
#include <API/TRTypesetter.h>

/*
 * Creates the line of a truncation token. It is shaped with the typeface and the size of the text
 * at the place of the truncation in the range, which MUST NOT be empty. If no token string is
 * given, it is an ellipsis, or three dots if the typeface has no ellipsis. Returns NULL on failure.
 */
TR_INTERNAL ComposedLineRef TokenResolverCreateTokenLine(TypesetterRef typesetter,
    TRUInteger start, TRUInteger end, TRTruncationPlace truncationPlace, const void *tokenString,
    TRUInteger tokenLength, TRStringEncoding tokenEncoding);

#endif
