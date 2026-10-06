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

#ifndef _TEHREER_CORE_ATOMIC_FLAG_H
#define _TEHREER_CORE_ATOMIC_FLAG_H

#include <API/TRBase.h>

#ifdef USE_C11_ATOMICS

#include <stdatomic.h>

#define HAS_ATOMIC_FLAG_SUPPORT

typedef atomic_flag AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                ATOMIC_FLAG_INIT
#define AtomicFlagTestAndSet(flag)      ((TRBoolean)atomic_flag_test_and_set(flag))
#define AtomicFlagClear(flag)           atomic_flag_clear(flag)

#elif defined(USE_ATOMIC_BUILTINS)

#define HAS_ATOMIC_FLAG_SUPPORT

typedef TRBoolean AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                TRFalse
#define AtomicFlagTestAndSet(flag)      __atomic_test_and_set(flag, __ATOMIC_SEQ_CST)
#define AtomicFlagClear(flag)           __atomic_clear(flag, __ATOMIC_SEQ_CST)

#elif defined(USE_SYNC_BUILTINS)

#define HAS_ATOMIC_FLAG_SUPPORT

typedef TRBoolean AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                TRFalse
#define AtomicFlagTestAndSet(flag)      __sync_lock_test_and_set(flag, TRTrue)
#define AtomicFlagClear(flag)           __sync_lock_release(flag)

#elif defined(USE_WIN_INTRINSICS)

#include <intrin.h>

#define HAS_ATOMIC_FLAG_SUPPORT
#pragma intrinsic(_InterlockedExchange8)

typedef volatile char AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                0
#define AtomicFlagTestAndSet(flag)      (_InterlockedExchange8(flag, 1) == 1)
#define AtomicFlagClear(flag)           _InterlockedExchange8(flag, 0)

#elif defined(USE_WIN_INTERLOCKED)

#include <windows.h>

#define HAS_ATOMIC_FLAG_SUPPORT

typedef volatile LONG AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                0
#define AtomicFlagTestAndSet(flag)      (InterlockedExchange(flag, 1) == 1)
#define AtomicFlagClear(flag)           InterlockedExchange(flag, 0)

#else /* Non-atomic fallback */

typedef TRBoolean AtomicFlag;
typedef AtomicFlag *AtomicFlagRef;

#define AtomicFlagMake()                TRFalse
#define AtomicFlagTestAndSet(flag)      (*(flag) ? TRTrue : (*(flag) = TRTrue, TRFalse))
#define AtomicFlagClear(flag)           (*(flag) = TRFalse)

#endif

#endif
