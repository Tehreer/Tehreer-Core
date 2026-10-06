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

#ifndef _TEHREER_CORE_ATOMIC_PTR_H
#define _TEHREER_CORE_ATOMIC_PTR_H

#include <API/TRBase.h>

#ifdef USE_C11_ATOMICS

#include <stdatomic.h>

#define HAS_ATOMIC_PTR_SUPPORT

typedef _Atomic(void *) AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             _Atomic(type *)
#define AtomicPtrLoad(aptr)             atomic_load(aptr)
#define AtomicPtrStore(aptr, value)     atomic_store(aptr, value)
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    atomic_compare_exchange_strong(aptr, expected, desired)

#elif defined(USE_ATOMIC_BUILTINS)

#define HAS_ATOMIC_PTR_SUPPORT

typedef void *AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             type *
#define AtomicPtrLoad(aptr)             __atomic_load_n(aptr, __ATOMIC_SEQ_CST)
#define AtomicPtrStore(aptr, value)     __atomic_store_n(aptr, value, __ATOMIC_SEQ_CST)
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    __atomic_compare_exchange_n(aptr, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#elif defined(USE_SYNC_BUILTINS)

#define HAS_ATOMIC_PTR_SUPPORT

typedef void *AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             type *
#define AtomicPtrLoad(aptr)             __sync_fetch_and_add(aptr, 0)
#define AtomicPtrStore(aptr, value)     __sync_lock_test_and_set(aptr, value)
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    __sync_bool_compare_and_swap(aptr, *(expected), desired)

#elif defined(USE_WIN_INTRINSICS)

#include <intrin.h>

#define HAS_ATOMIC_PTR_SUPPORT
#pragma intrinsic(_InterlockedExchangePointer, _InterlockedCompareExchangePointer)

typedef void * volatile AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             type * volatile
#define AtomicPtrLoad(aptr)             \
    _InterlockedCompareExchangePointer((AtomicPtrRef)(aptr), NULL, NULL)
#define AtomicPtrStore(aptr, value)     \
    _InterlockedExchangePointer((AtomicPtrRef)(aptr), (void *)(value))
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    (_InterlockedCompareExchangePointer((AtomicPtrRef)(aptr), (void *)(desired), (void *)(*(expected))) == *(expected))

#elif defined(USE_WIN_INTERLOCKED)

#include <windows.h>

#define HAS_ATOMIC_PTR_SUPPORT

typedef void * volatile AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             type * volatile

#ifdef _WIN64

#define AtomicPtrLoad(aptr)             \
    ((void *)InterlockedCompareExchange64((LONG64 volatile *)(aptr), 0, 0))
#define AtomicPtrStore(aptr, value)     \
    InterlockedExchange64((LONG64 volatile *)(aptr), (LONG64)(value))
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    (((void *)InterlockedCompareExchange64((LONG64 volatile *)(aptr), (LONG64)(desired), (LONG64)(*(expected)))) == *(expected))

#else

#define AtomicPtrLoad(aptr)             \
    ((void *)InterlockedCompareExchange((LONG volatile *)(aptr), 0, 0))
#define AtomicPtrStore(aptr, value)     \
    InterlockedExchange((LONG volatile *)(aptr), (LONG)(value))
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    (((void *)InterlockedCompareExchange((LONG volatile *)(aptr), (LONG)(desired), (LONG)(*(expected)))) == *(expected))

#endif

#else /* Non-atomic fallback */

typedef void *AtomicPtr;
typedef AtomicPtr *AtomicPtrRef;

#define AtomicPtrType(type)             type *
#define AtomicPtrLoad(aptr)             (*(aptr))
#define AtomicPtrStore(aptr, value)     (*(aptr) = (value))
#define AtomicPtrCompareAndSet(aptr, expected, desired) \
    ((*(aptr) == *(expected)) ? ((*(aptr) = (desired)), TRTrue) : TRFalse)

#endif

#endif
