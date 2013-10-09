/*
 *  LXShader.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSHADER_H_
#define _LXSHADER_H_

#include "LXRefTypes.h"
#include "LXBasicTypes.h"


enum {
    kLXErrorID_Shader_EmptyArg = 6501,
    kLXErrorID_Shader_ErrorInString,
    kLXErrorID_Shader_InvalidProgramFormat
};

enum {
    kLXShaderFormat_unknown = 0,
    kLXShaderFormat_OpenGLARBfp = 1,
    kLXShaderFormat_FPClosure = 0x20,
    kLXShaderFormat_Direct3DPixelShaderAssembly = 0x40,
    kLXShaderFormat_OpenGLES2FragmentShader = 0x50
};
typedef LXUInteger LXShaderFormat;

enum {
    kLXShaderNeedsNoInput = 0,
    
    kLXShaderExpectsNormalizedX = 1 << 0,
    kLXShaderExpectsNormalizedY = 1 << 1
};
typedef LXUInteger LXShaderInputMask;


#ifdef __cplusplus
extern "C" {
#endif


#pragma mark --- LXShader public API methods ---

LXEXPORT const char *LXShaderTypeID();

LXEXPORT LXShaderRef LXShaderCreateEmpty();

LXEXPORT LXShaderRef LXShaderCopy(LXShaderRef r);

// the returned object is retained.
// it's recommended to always use kLXStorageHint_Final when the program string is known final (e.g. static const char *)
// because it allows shader creation and copying to be optimized.
LXEXPORT LXShaderRef LXShaderCreateWithString(const char *asciiStr,
                                                      size_t strLen,
                                                      LXUInteger programFormat,
                                                      LXUInteger storageHint,
                                                      LXError *outError);

LXEXPORT LXShaderRef LXShaderRetain(LXShaderRef shader);
LXEXPORT void LXShaderRelease(LXShaderRef shader);

LXEXPORT void LXShaderCopyParameters(LXShaderRef shader, LXShaderRef otherShader);

LXEXPORT void LXShaderSetParameter(LXShaderRef shader, LXInteger paramIndex, LXRGBA value);

LXEXPORT void LXShaderSetParameter4f(LXShaderRef shader,
                                     LXInteger paramIndex,
                                     float v1, float v2, float v3, float v4);

LXEXPORT void LXShaderSetParameter4fv(LXShaderRef shader,
                                      LXInteger paramIndex,
                                      float *pv);

LXEXPORT LXRGBA LXShaderGetParameter(LXShaderRef shader, LXInteger paramIndex);

LXEXPORT void LXShaderGetParameter4fv(LXShaderRef shader,
                                      LXInteger paramIndex,
                                      float *outV);
                                      
LXEXPORT LXInteger LXShaderGetMaxNumberOfParameters(LXShaderRef r);


// -- CPU-based evaluation of shaders --

// processes an array of 4-element vector inputs to the shader
LXEXPORT LXSuccess LXShaderEvaluate1DArray4f(LXShaderRef r,
                                             LXInteger arrayCount,
                                             const float * LXRESTRICT inArray,
                                             float * LXRESTRICT outArray);

// takes a single float as input, outputs a vector (can be directly used as a CGShading evaluation callback)
LXEXPORT void LXShaderEvaluateVectorWith1SInput(LXShaderRef r,
                                        const float * LXRESTRICT inData,
                                        float * LXRESTRICT outData);
                                        

// -- accessing the platform-specific object --
    
// the returned value needs to be cast to the native object type
// (e.g. a program ID on OpenGL, or an equivalent Direct3D interface object)
LXEXPORT LXUnidentifiedNativeObj LXShaderLockPlatformNativeObj(LXShaderRef shader);

LXEXPORT void LXShaderUnlockPlatformNativeObj(LXShaderRef shader);


#ifdef __cplusplus
}
#endif

#endif


