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

#ifndef _TEHREER_CORE_ONCE_H
#define _TEHREER_CORE_ONCE_H

#include <API/TRBase.h>
#include <Core/AtomicUInt.h>

#if defined(USE_C11_THREADS)

#include <threads.h>

#define HAS_ONCE_SUPPORT

typedef once_flag Once;
typedef Once *OnceRef;

#define OnceMake()                  ONCE_FLAG_INIT
#define OnceExecute(once, func)     call_once(once, func)

#elif defined(USE_PTHREADS)

#include <pthread.h>

#define HAS_ONCE_SUPPORT

typedef pthread_once_t Once;
typedef Once *OnceRef;

#define OnceMake()                  PTHREAD_ONCE_INIT
#define OnceExecute(once, func)     pthread_once(once, func)

#elif defined(USE_WIN_SYNC)

#include <windows.h>

#define HAS_ONCE_SUPPORT
#define HAS_WIN_ONCE

typedef INIT_ONCE Once;
typedef Once *OnceRef;

TR_PRIVATE BOOL CALLBACK WinOnceCallback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context);

#define OnceMake()                  INIT_ONCE_STATIC_INIT
#define OnceExecute(once, func)     InitOnceExecuteOnce(once, WinOnceCallback, func, NULL)

#elif defined(HAS_ATOMIC_UINT_SUPPORT)

#define HAS_ONCE_SUPPORT
#define HAS_SPIN_ONCE

typedef AtomicUInt Once;
typedef Once *OnceRef;

TR_PRIVATE void ExecuteSpinOnce(OnceRef once, void (*func)(void));

#define OnceMake()                  0
#define OnceExecute(once, func)     ExecuteSpinOnce(once, func)

#else /* Unsafe fallback */

typedef TRBoolean Once;
typedef Once *OnceRef;

#define OnceMake()                  TRFalse
#define OnceExecute(once, func)     \
    do {                            \
        if (!*(once)) {             \
            *(once) = TRTrue;       \
            (func)();               \
        }                           \
    } while (0)

#endif

#endif
