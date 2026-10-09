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

#ifndef _TEHREER_SHEEN_BIDI_H
#define _TEHREER_SHEEN_BIDI_H

#include <SheenBidi/SBText.h>

#include <Tehreer/TRBase.h>
#include <Tehreer/TRText.h>

TR_EXTERN_C_BEGIN

/**
 * Returns the SheenBidi text that holds the code units and the attributes of a text, for the
 * queries that the text interface does not have, such as the iterators of SheenBidi over the
 * paragraphs, the runs and the attributes.
 *
 * This header is not included by `Tehreer/Tehreer.h`, so that the rest of the interface does not
 * depend on SheenBidi. Including it needs the headers of SheenBidi, and its text interface is
 * available only if `SB_CONFIG_EXPERIMENTAL_TEXT_API` is defined.
 *
 * @param text
 *      The text object.
 * @return
 *      The SheenBidi text of the text object, which is owned by it. It must not be modified, nor
 *      used after the text object is released.
 */
TR_PUBLIC SBTextRef TRTextGetSheenBidiText(TRTextRef text);

TR_EXTERN_C_END

#endif
