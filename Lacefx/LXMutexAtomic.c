/*
 *  LXMutexAtomic.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 9.2.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXMutexAtomic.h"
#include "LXMutex.h"


extern LXBool g_lxLocksInited;
extern LXMutexPtr g_lxAtomicLock;
extern void LXPlatformCreateLocks_(void);


#if defined(LXPLATFORM_WIN)

#include <windows.h>

int32_t LXAtomicInc_int32(volatile int32_t *i)
{
    return InterlockedIncrement((volatile LONG *)i);
}

int32_t LXAtomicDec_int32(volatile int32_t *i)
{
    return InterlockedDecrement((volatile LONG *)i);
}


#elif defined(__APPLE__)
// new implementation using iOS-compatible functions
#include <libkern/OSAtomic.h>

int32_t LXAtomicInc_int32(volatile int32_t *i)
{
    int32_t n = OSAtomicIncrement32Barrier(i);
    return n;
}

int32_t LXAtomicDec_int32(volatile int32_t *i)
{
    int32_t n = OSAtomicDecrement32Barrier(i);
    return n;
}


#else
/*
  For cross-platform compatibility, these atomic ops are implemented using a real lock.
  
  (Better solutions inevitably rely on CPU-specific features, and would cause unexpected challenges
  when we need to compile for e.g. an ARM platform.)
*/


int32_t LXAtomicInc_int32(volatile int32_t *i)
{
    int32_t v;
    LXMutexLock(g_lxAtomicLock);
    v = *i + 1;
    *i = v;
    LXMutexUnlock(g_lxAtomicLock);
    return v;
}

int32_t LXAtomicDec_int32(volatile int32_t *i)
{
    int32_t v;
    LXMutexLock(g_lxAtomicLock);
    v = *i - 1;
    *i = v;
    LXMutexUnlock(g_lxAtomicLock);
    return v;
}

#endif

