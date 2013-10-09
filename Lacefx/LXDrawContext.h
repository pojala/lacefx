/*
 *  LXDrawContext.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 22.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXDRAWCONTEXT_H_
#define _LXDRAWCONTEXT_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"


#ifdef __cplusplus
extern "C" {
#endif

#pragma mark --- LXDrawContext public API methods ---

LXEXPORT LXDrawContextRef LXDrawContextCreate();
LXEXPORT void LXDrawContextRelease(LXDrawContextRef ctx);

// convenience factories that return an autoreleased object
LXEXPORT LXDrawContextRef LXDrawContextWithTexture(LXPoolRef pool, LXTextureRef texture);
LXEXPORT LXDrawContextRef LXDrawContextWithTexturesAndCount(LXPoolRef pool, LXTextureRef *textures, LXInteger texCount);
LXEXPORT LXDrawContextRef LXDrawContextWithTextureArray(LXPoolRef pool, LXTextureArrayRef array);
LXEXPORT LXDrawContextRef LXDrawContextWithTextureArrayAndShader(LXPoolRef pool, LXTextureArrayRef array, LXShaderRef shader);

LXEXPORT void LXDrawContextSetTextureArray(LXDrawContextRef ctx, LXTextureArrayRef array);
LXEXPORT void LXDrawContextSetShader(LXDrawContextRef ctx, LXShaderRef shader);
LXEXPORT void LXDrawContextSetProjectionTransform(LXDrawContextRef ctx, LXTransform3DRef transform);
LXEXPORT void LXDrawContextSetModelViewTransform(LXDrawContextRef ctx, LXTransform3DRef transform);

LXEXPORT LXInteger LXDrawContextGetTextureCount(LXDrawContextRef ctx);

LXEXPORT LXTransform3DRef LXDrawContextGetModelViewTransform(LXDrawContextRef ctx);

LXEXPORT void LXDrawContextSetShaderParameter(LXDrawContextRef ctx, LXInteger paramIndex, LXRGBA value);
LXEXPORT LXRGBA LXDrawContextGetShaderParameter(LXDrawContextRef ctx, LXInteger paramIndex);
LXEXPORT void LXDrawContextCopyParametersFromShader(LXDrawContextRef r, LXShaderRef aShader);

// flags are a convenience to easily set some of the most commonly used hardware options.
// for more elaborate or esoteric extension/specification needs in the future, LXRefGetProperty/SetProperty can be used
// instead of introducing another set of flags or specific accessor functions.
LXEXPORT void LXDrawContextSetFlags(LXDrawContextRef ctx, LXUInteger flags);
LXEXPORT LXUInteger LXDrawContextGetFlags(LXDrawContextRef ctx);

#ifdef __cplusplus
}
#endif


// flags
enum {
    kLXDrawFlag_UseFixedFunctionBlending_SourceIsPremult    = 1 << 0,
    kLXDrawFlag_UseFixedFunctionBlending_SourceIsUnpremult  = 1 << 1,
    
    // on most consumer-level NV/ATI hardware, line antialiasing is not supported in combination with a fragment shader.
    // if a shader is specified together with fragment AA, Lacefx will instead use shader parameter 0 as the fragment color.
    kLXDrawFlag_UseHardwareFragmentAntialiasing             = 1 << 16,
    
    // reserved for internal use
    kLXDrawFlag_private1_                                   = 1 << 25,
    kLXDrawFlag_private2_                                   = 1 << 26,
};



#endif
