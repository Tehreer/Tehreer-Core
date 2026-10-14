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


#ifndef _TEHREER_CORE_NUMBER_H
#define _TEHREER_CORE_NUMBER_H

#include <API/TRBase.h>

/**
 * Converts a value to fixed point with the given unit, rounding it. A value that is out of range is
 * saturated, and one that is not a number is zero, as a conversion of them is not defined.
 *
 * @param value
 *      The value to convert.
 * @param unit
 *      The number of fixed point units that make one, such as 64 for 26.6 fixed point.
 * @return
 *      The rounded fixed point value, which is within the range of `TRInt32`.
 */
TR_INTERNAL TRInt32 NumberToFixed(TRFloat value, TRFloat unit);

#endif
