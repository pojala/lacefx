/*
 *  LXConvolver.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 7.4.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXCONVOLVER_H_
#define _LXCONVOLVER_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"
#include "LXPool.h"


#pragma pack (push, 4)

typedef struct {
    uint32_t version;  // set to 0 or 1 for this version
    
    void    (*generateWeights)(LXConvolverRef convolver, double *array, LXInteger arraySize, void *);
    
    // if callback doesn't return a string in outStr, convolver will use default combiner
    void    (*getCustomCombinerInstruction)(LXConvolverRef convolver, char *outStr, const size_t outStrLen, void *);
    
    // if callback doesn't return a string in outStr, convolver will use default finisher
    void    (*getCustomFinisherInstruction)(LXConvolverRef convolver, char *outStr, const size_t outStrLen,
                                                                      const char *sourceVarStr, const size_t sourceVarLen, void *);
    
} LXConvolverCallbacks_v1;

#pragma pack (pop)



#pragma mark --- LXConvolver public API methods ---

LXEXPORT const char *LXConvolverTypeID();

// the returned object is retained
LXEXPORT LXConvolverRef LXConvolverCreate(LXPoolRef pool, LXUInteger flags, LXError *outError);

LXEXPORT LXConvolverRef LXConvolverRetain(LXConvolverRef conv);
LXEXPORT void LXConvolverRelease(LXConvolverRef conv);

LXEXPORT void LXConvolverSetCallbacks(LXConvolverRef conv, LXConvolverCallbacks_v1 *callBacks, void *userData);

LXEXPORT void LXConvolverSetSourceTexture(LXConvolverRef conv, LXTextureRef texture);

LXEXPORT void LXConvolverSetOutputRect(LXConvolverRef conv, LXRect rect);

LXEXPORT void LXConvolverSetSampling(LXConvolverRef conv, LXUInteger samplingEnum);
LXEXPORT void LXConvolverSetSampleDistance(LXConvolverRef conv, LXFloat sampleDistance);
LXEXPORT void LXConvolverSetSampleRotation(LXConvolverRef conv, LXFloat sampleRot);
LXEXPORT void LXConvolverSetNormalizesWeights(LXConvolverRef conv, LXBool doNormalize);

// the return value is the surface that contains the final rendering result.
// if surface1 and surface2 are NULL and outPassCount is non-NULL, this function returns the number of passes
// needed to render in outPassCount but doesn't actually render anything.
LXEXPORT LXSurfaceRef LXConvolverRender1DWithWidthUsingSurfaces(LXConvolverRef conv,
                                                                LXInteger width,
                                                                LXSurfaceRef surface1,
                                                                LXSurfaceRef surface2,
                                                                int *outPassCount,  // number of passes rendered is returned here, can be NULL
                                                                LXError *outError);


#endif // _LXCONVOLVER_H_
