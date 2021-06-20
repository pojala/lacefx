/*
 *  LXThreadLocal.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 29.6.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXTLS_H_
#define _LXTLS_H_

#include "LXBasicTypes.h"

/*
  LXThreadLocalVar is a wrapper for the platform-native thread-specific variable type.
*/


#ifdef __cplusplus
extern "C" {
#endif


#if defined(LXPLATFORM_WIN)
#include <windows.h>

typedef DWORD LXThreadLocalVar;

#define LXThreadLocalCreate()           TlsAlloc()
#define LXThreadLocalDestroy(t_)        TlsFree(t_)

#define LXThreadLocalGet(t_)            TlsGetValue(t_)
#define LXThreadLocalSet(t_, v_)        TlsSetValue(t_, v_)



#elif defined(__APPLE__) || defined(_HAVE_PTHREAD_H)
#include <pthread.h>

typedef pthread_key_t LXThreadLocalVar;

LXEXPORT LXThreadLocalVar LXThreadLocalCreate(void);
LXEXPORT void LXThreadLocalDestroy(LXThreadLocalVar tls);

#define LXThreadLocalGet(t_)            pthread_getspecific(t_)
#define LXThreadLocalSet(t_, v_)        pthread_setspecific(t_, v_)


#else
#error "No TLS implementation available on this platform (if pthreads are available, define _HAVE_PTHREAD_H)"

#endif


#ifdef __cplusplus
}
#endif

#endif

