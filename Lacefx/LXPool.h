/*
 *  LXPool.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPOOL_H_
#define _LXPOOL_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"


enum {
    kLXPoolHint_TemporaryObject = 0,
    kLXPoolHint_LikelyToReuseObject = 1,
    
    kLXPoolHint_ObjectIsMalloced = 1 << 10  // this flag allows plain-C objects to be autoreleased (e.g. C strings)
};


#ifdef __cplusplus
extern "C" {
#endif


#pragma mark --- LXPool public API methods ---

// these functions work like Cocoa's autorelease pools (thread-local, and can be nested by creating a new one).
// most of the time you'll want to use the thread-local pool for autoreleasing temporary objects,
// hence the LXAutorelease macro.
LXEXPORT LXPoolRef LXPoolCreateForThread(void);
LXEXPORT LXPoolRef LXPoolCurrentForThread(void);

// the autorelease functions & macros return the same pointer that was passed in.
// "AutoreleaseCPtr" can be used for any memory object that needs to be freed with _lx_free() -- e.g. strings from Lacefx.
#define LXAutorelease(_obj_)        LXPoolAutorelease(LXPoolCurrentForThread(), _obj_)
#define LXAutoreleaseCPtr(_obj_)    LXPoolAutoreleaseWithHint(LXPoolCurrentForThread(), (LXRef)(_obj_), kLXPoolHint_ObjectIsMalloced)


LXEXPORT LXPoolRef LXPoolCreate(void);

LXEXPORT LXPoolRef LXPoolRetain(LXPoolRef poolRef);
LXEXPORT void LXPoolRelease(LXPoolRef poolRef);

LXEXPORT LXUInteger LXPoolCount(LXPoolRef poolRef);

LXEXPORT LXRef LXPoolAutorelease(LXPoolRef pool, LXRef obj);

LXEXPORT LXRef LXPoolAutoreleaseWithHint(LXPoolRef pool, LXRef obj, LXUInteger releaseHint);

LXEXPORT void LXPoolPurge(LXPoolRef pool);

void LXPoolEnableDebug(LXPoolRef pool);


#ifdef __cplusplus
}
#endif

#endif
