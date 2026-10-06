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

#ifndef _TEHREER_CORE_ATOMIC_UINT_H
#define _TEHREER_CORE_ATOMIC_UINT_H

#include <API/TRBase.h>

#ifdef USE_C11_ATOMICS

#include <stdatomic.h>

#define HAS_ATOMIC_UINT_SUPPORT

typedef _Atomic(TRUInteger) AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      atomic_init(aui, value)
#define AtomicUIntLoad(aui)             atomic_load(aui)
#define AtomicUIntStore(aui, value)     atomic_store(aui, value)
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    atomic_compare_exchange_strong(aui, expected, desired)
#define AtomicUIntIncrement(aui)        ((TRUInteger)(atomic_fetch_add(aui, 1) + 1))
#define AtomicUIntDecrement(aui)        ((TRUInteger)(atomic_fetch_sub(aui, 1) - 1))

#elif defined(USE_ATOMIC_BUILTINS)

#define HAS_ATOMIC_UINT_SUPPORT

typedef TRUInteger AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (value))
#define AtomicUIntLoad(aui)             __atomic_load_n(aui, __ATOMIC_SEQ_CST)
#define AtomicUIntStore(aui, value)     __atomic_store_n(aui, value, __ATOMIC_SEQ_CST)
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    __atomic_compare_exchange_n(aui, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define AtomicUIntIncrement(aui)        __atomic_add_fetch(aui, 1, __ATOMIC_SEQ_CST)
#define AtomicUIntDecrement(aui)        __atomic_sub_fetch(aui, 1, __ATOMIC_SEQ_CST)

#elif defined(USE_SYNC_BUILTINS)

#define HAS_ATOMIC_UINT_SUPPORT

typedef TRUInteger AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (value))
#define AtomicUIntLoad(aui)             __sync_fetch_and_add(aui, 0)
#define AtomicUIntStore(aui, value)     __sync_lock_test_and_set(aui, value)
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    __sync_bool_compare_and_swap(aui, *(expected), desired)
#define AtomicUIntIncrement(aui)        __sync_add_and_fetch(aui, 1)
#define AtomicUIntDecrement(aui)        __sync_sub_and_fetch(aui, 1)

#elif defined(USE_WIN_INTRINSICS)

#include <intrin.h>

#define HAS_ATOMIC_UINT_SUPPORT

#ifdef _WIN64

#pragma intrinsic(_InterlockedExchange64, _InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedIncrement64, _InterlockedDecrement64)

typedef volatile __int64 AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (__int64)(value))
#define AtomicUIntLoad(aui)             ((TRUInteger)_InterlockedCompareExchange64(aui, 0, 0))
#define AtomicUIntStore(aui, value)     _InterlockedExchange64(aui, (__int64)(value))
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    (((TRUInteger)_InterlockedCompareExchange64(aui, (__int64)(desired), (__int64)(*(expected)))) == *(expected))
#define AtomicUIntIncrement(aui)        ((TRUInteger)_InterlockedIncrement64(aui))
#define AtomicUIntDecrement(aui)        ((TRUInteger)_InterlockedDecrement64(aui))

#else

#pragma intrinsic(_InterlockedExchange, _InterlockedCompareExchange)
#pragma intrinsic(_InterlockedIncrement, _InterlockedDecrement)

typedef volatile long AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (long)(value))
#define AtomicUIntLoad(aui)             ((TRUInteger)_InterlockedCompareExchange(aui, 0, 0))
#define AtomicUIntStore(aui, value)     _InterlockedExchange(aui, (long)(value))
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    (((TRUInteger)_InterlockedCompareExchange(aui, (long)(desired), (long)(*(expected)))) == *(expected))
#define AtomicUIntIncrement(aui)        ((TRUInteger)_InterlockedIncrement(aui))
#define AtomicUIntDecrement(aui)        ((TRUInteger)_InterlockedDecrement(aui))

#endif

#elif defined(USE_WIN_INTERLOCKED)

#include <windows.h>

#define HAS_ATOMIC_UINT_SUPPORT

#ifdef _WIN64

typedef volatile LONG64 AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (LONG64)(value))
#define AtomicUIntLoad(aui)             ((TRUInteger)InterlockedCompareExchange64(aui, 0, 0))
#define AtomicUIntStore(aui, value)     InterlockedExchange64(aui, (LONG64)(value))
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    (((TRUInteger)InterlockedCompareExchange64(aui, (LONG64)(desired), (LONG64)(*(expected)))) == *(expected))
#define AtomicUIntIncrement(aui)        ((TRUInteger)InterlockedIncrement64(aui))
#define AtomicUIntDecrement(aui)        ((TRUInteger)InterlockedDecrement64(aui))

#else

typedef volatile LONG AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (LONG)(value))
#define AtomicUIntLoad(aui)             ((TRUInteger)InterlockedCompareExchange(aui, 0, 0))
#define AtomicUIntStore(aui, value)     InterlockedExchange(aui, (LONG)(value))
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    (((TRUInteger)InterlockedCompareExchange(aui, (LONG)(desired), (LONG)(*(expected)))) == *(expected))
#define AtomicUIntIncrement(aui)        ((TRUInteger)InterlockedIncrement(aui))
#define AtomicUIntDecrement(aui)        ((TRUInteger)InterlockedDecrement(aui))

#endif

#else /* Non-atomic fallback */

typedef TRUInteger AtomicUInt;
typedef AtomicUInt *AtomicUIntRef;

#define AtomicUIntInit(aui, value)      (*(aui) = (value))
#define AtomicUIntLoad(aui)             (*(aui))
#define AtomicUIntCompareAndSet(aui, expected, desired) \
    ((*(aui) == *(expected)) ? ((*(aui) = (desired)), TRTrue) : TRFalse)
#define AtomicUIntStore(aui, value)     (*(aui) = (value))
#define AtomicUIntIncrement(aui)        (++(*(aui)))
#define AtomicUIntDecrement(aui)        (--(*(aui)))

#endif

#endif
