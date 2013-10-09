/*
 *  LXFPClosureContext.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 23.9.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXFPCLOSURECONTEXT_H_
#define _LXFPCLOSURECONTEXT_H_


#include "LXBasicTypes.h"
#include "LXBasicTypeFunctions.h"


typedef struct _LXFPClosureContext *LXFPClosureContextPtr;


#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT LXFPClosureContextPtr LXFPClosureContextCreate();
LXEXPORT void LXFPClosureContextDestroy(LXFPClosureContextPtr ctx);

LXEXPORT LXFPClosureContextPtr LXFPClosureContextCopy(LXFPClosureContextPtr ctx);

LXEXPORT void LXFPClosureContextSetScalars(LXFPClosureContextPtr ctx, LXFloat *values, LXInteger scalarCount);
LXEXPORT void LXFPClosureContextSetVectors(LXFPClosureContextPtr ctx, LXRGBA *values, LXInteger vectorCount);

LXEXPORT LXInteger LXFPClosureContextGetScalarCount(LXFPClosureContextPtr ctx);
LXEXPORT LXInteger LXFPClosureContextGetVectorCount(LXFPClosureContextPtr ctx);

LXEXPORT LXInteger LXFPClosureContextGetScalarCapacity(LXFPClosureContextPtr ctx);
LXEXPORT LXInteger LXFPClosureContextGetVectorCapacity(LXFPClosureContextPtr ctx);


LXEXPORT LXFloat LXFPClosureContextGetScalarAtIndex(LXFPClosureContextPtr ctx, LXInteger index);
LXEXPORT LXRGBA LXFPClosureContextGetVectorAtIndex(LXFPClosureContextPtr ctx, LXInteger index);


#ifdef __cplusplus
}
#endif

#endif

