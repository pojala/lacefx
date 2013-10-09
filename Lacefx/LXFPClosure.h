/*
 *  LXFPClosure.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 17.2.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXFPCLOSURE_H_
#define _LXFPCLOSURE_H_

#include <stdlib.h>
#include "LXBasicTypes.h"
#include "LXBasicTypeFunctions.h"
#include "LXFPClosureContext.h"

/*
  This is a tiny ARBfp parser + execution engine originally built for CPU-executed closures within a Conduit node graph
  (e.g. slider values manipulated by arithmetic nodes).
  
  Note that the input semantics differ slightly from ARBfp, which has a single bank of vector inputs ("program local" vars)
  whereas the inputs for FPClosure programs come in two banks: scalars and vectors (this mirrors Conduit's slider/color picker model).
  
  Also, there is no special output register. The last evaluation result is considered the closure's result.
  
  To encapsulate the input data, use LXFPClosureContext.
*/



#define FPCLOSURE_NUM_REGS			96

// these are the magic keys in a shader program used as FPClosure:
// registers as "r0", scalars as "s1", vectors as "v1" -- note that scalars and vectors are 1-based!
// the returned result of the shader program is the last instruction's dst value.
#define FPCLOSURE_VARID_REGISTER  	'r'
#define FPCLOSURE_VARID_OPENSCALAR	's'
#define FPCLOSURE_VARID_OPENVECTOR	'v'



typedef struct _FPClosure *LXFPClosurePtr;



#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT: program string is expected to start with the header "!!LCQfp1.0" (in contrast to "!!ARBfp1.0" used for regular OpenGL ARB fragment programs).
// this is because of differing input semantics, see note above.
LXEXPORT LXFPClosurePtr LXFPClosureCreateWithString(const char *fpStr, size_t fpStrLen);

LXEXPORT void LXFPClosureDestroy(LXFPClosurePtr fp);


// context can be NULL
LXEXPORT LXSuccess LXFPClosureExecuteWithContext(LXFPClosurePtr fp, LXRGBA *outRGBA, LXFPClosureContextPtr ctx, LXError *error);

// returns 0 for success (or an internal error value otherwise), inputVals can be NULL
LXEXPORT int LXFPClosureExecute_f(LXFPClosurePtr fp, float * LXRESTRICT outVector,
                                  const float * LXRESTRICT inputScalars, LXInteger inputScalarCount,
                                  const float * LXRESTRICT inputVectors, LXInteger inputVectorCount);


#ifdef __cplusplus
}
#endif



#endif  // _LXFPCLOSURE_H_
