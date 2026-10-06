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

#include <API/TRBase.h>
#include <Core/AtomicUInt.h>

#include "Once.h"

#if defined(HAS_WIN_ONCE)

TR_PRIVATE BOOL CALLBACK WinOnceCallback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    void (*func)(void) = (void (*)(void))Parameter;
    (void)InitOnce;
    (void)Context;

    func();

    return TRUE;
}

#elif defined(HAS_SPIN_ONCE)

enum {
    OnceIdle = 0,
    OnceBusy = 1,
    OnceDone = 2
};

TR_PRIVATE void ExecuteSpinOnce(OnceRef once, void (*func)(void))
{
    TRUInteger state = AtomicUIntLoad(once);

    if (state != OnceDone) {
        TRUInteger expected = OnceIdle;

        if (AtomicUIntCompareAndSet(once, &expected, OnceBusy)) {
            func();
            AtomicUIntStore(once, OnceDone);
        } else {
            do {
                /* Spin */
            } while (AtomicUIntLoad(once) != OnceDone);
        }
    }
}

#endif
