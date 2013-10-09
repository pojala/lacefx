/*
 *  LXShaderD3DTranslation.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 24.8.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSHADERD3DTRANSLATION_H_
#define _LXSHADERD3DTRANSLATION_H_

#include "LXBasicTypes.h"
#include "LXMap.h"


// this struct is used to pass info for the Direct3D converter
// which does something slightly different when a Conduit-generated program
// is passed in. a historical artifact of the renderer's implementation...
typedef struct {
    LXBool shaderIsConduitFormat;
    
    // these apply to conduit-formatted shader
    char *arePickersUsed;
    LXInteger arePickersUsedCount;
    
    // for non-conduit shader, these will be filled out by the converter
    LXInteger numProgramLocalsInUse;
    LXInteger regIndexForFirstProgramLocal;
    
    LXMapPtr *psRegMapPtr;
} LXShaderTranslationInfo;


#ifdef __cplusplus
extern "C" {
#endif
    
// outputs Direct3D 9+ pixel shader assembly
LXEXPORT LXSuccess LXConvertShaderString_OpenGLARBfp_to_D3DPS(const char *str, const char *psFormat,
                                                                char **outStr,
                                                                LXShaderTranslationInfo *shaderInfo);

// outputs OpenGL ES 2.0 fragment shader code.
// the output string is raw code that needs to be wrapped in a function
// and prefixed with variable declarations.
LXEXPORT LXSuccess LXConvertShaderString_OpenGLARBfp_to_ES2_function_body(const char *str,
                                                                char **outStr);

#ifdef __cplusplus
}
#endif

#endif