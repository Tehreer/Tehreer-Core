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

#ifndef _TEHREER_CORE_MUTEX_H
#define _TEHREER_CORE_MUTEX_H

#include <API/TRBase.h>

#ifdef USE_C11_THREADS

#include <threads.h>

#define HAS_MUTEX_SUPPORT

typedef mtx_t Mutex;
typedef Mutex *MutexRef;

#define MutexInit(mutex)        mtx_init((mutex), mtx_plain)
#define MutexDestroy(mutex)     mtx_destroy((mutex))
#define MutexLock(mutex)        mtx_lock((mutex))
#define MutexTryLock(mutex)     (mtx_trylock((mutex)) == thrd_success)
#define MutexUnlock(mutex)      mtx_unlock((mutex))

#elif defined(USE_PTHREADS)

#include <pthread.h>

#define HAS_MUTEX_SUPPORT

typedef pthread_mutex_t Mutex;
typedef Mutex *MutexRef;

#define MutexInit(mutex)        pthread_mutex_init((mutex), NULL)
#define MutexDestroy(mutex)     pthread_mutex_destroy((mutex))
#define MutexLock(mutex)        pthread_mutex_lock((mutex))
#define MutexTryLock(mutex)     (pthread_mutex_trylock((mutex)) == 0)
#define MutexUnlock(mutex)      pthread_mutex_unlock((mutex))

#elif defined(USE_WIN_SYNC)

#include <windows.h>

#define HAS_MUTEX_SUPPORT

typedef CRITICAL_SECTION Mutex;
typedef Mutex *MutexRef;

#define MutexInit(mutex)        InitializeCriticalSection((mutex))
#define MutexDestroy(mutex)     DeleteCriticalSection((mutex))
#define MutexLock(mutex)        EnterCriticalSection((mutex))
#define MutexTryLock(mutex)     TryEnterCriticalSection((mutex))
#define MutexUnlock(mutex)      LeaveCriticalSection((mutex))

#else /* Unsafe fallback */

typedef TRUInt32 Mutex;
typedef Mutex *MutexRef;

#define MutexInit(mutex)        ((void)((*(mutex)) = 0))
#define MutexDestroy(mutex)     ((void)(mutex))
#define MutexLock(mutex)        ((void)(mutex))
#define MutexTryLock(mutex)     TRTrue
#define MutexUnlock(mutex)      ((void)(mutex))

#endif

#endif
