/*
 *  LXMutex.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 14.1.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXMUTEX_H_
#define _LXMUTEX_H_

#include "LXBasicTypes.h"

/*
  LXMutex is a macro wrapper for the simplest platform-native mutex type.
  
  There are no guarantees that an LXMutex is recursive (Win32 critsec is, but pthread lock isn't).
*/


#ifdef __cplusplus
extern "C" {
#endif


#if (LX_BUILD_SINGLETHREADED)

typedef LXInteger LXMutex;

#define LXDECLMUTEX(m_)         LXMutex m_ = 0;
#define LXMutexInit(m_)         
#define LXMutexLock(m_)
#define LXMutexUnlock(m_)
#define LXMutexDestroy(m_)


#elif defined(LXPLATFORM_WIN)
#include <windows.h>

typedef CRITICAL_SECTION LXMutex;

#define LXDECLMUTEX(m_)         LXMutex m_ = { NULL, 0, 0, NULL, NULL, 0 };
#define LXMutexInit(m_)         InitializeCriticalSection(m_)
#define LXMutexLock(m_)         EnterCriticalSection(m_)
#define LXMutexUnlock(m_)       LeaveCriticalSection(m_)
#define LXMutexDestroy(m_)      DeleteCriticalSection(m_)


#elif defined(__APPLE__) || defined(_HAVE_PTHREAD_H)
#include <pthread.h>

typedef pthread_mutex_t LXMutex;

#define LXDECLMUTEX(m_)         LXMutex m_;  // = PTHREAD_MUTEX_INITIALIZER;  -- removed, instead we'll explicitly call pthread_mutex_init
#define LXMutexInit(m_)         pthread_mutex_init(m_, NULL)
#define LXMutexLock(m_)         pthread_mutex_lock(m_)
#define LXMutexUnlock(m_)       pthread_mutex_unlock(m_)
#define LXMutexDestroy(m_)      pthread_mutex_destroy(m_)


#else
#error "No mutex implementation available on this platform (if pthreads are available, define _HAVE_PTHREAD_H)"

#endif


typedef LXMutex *LXMutexPtr;


#ifdef __cplusplus
}
#endif

#endif
