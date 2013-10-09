/*
 *  LXMutexAtomic.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 14.1.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXMUTEXATOMIC_H_
#define _LXMUTEXATOMIC_H_

#include "LXBasicTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

LXEXPORT int32_t LXAtomicInc_int32(volatile int32_t *i);
LXEXPORT int32_t LXAtomicDec_int32(volatile int32_t *i);

#ifdef __cplusplus
}
#endif

#endif
