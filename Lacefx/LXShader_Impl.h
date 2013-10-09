/*
 *  LXShader_Impl.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.3.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXBasicTypes.h"


#if defined(LXPLATFORM_WIN)
 #include "LXPlatform_d3d.h"
#elif defined(LXPLATFORM_IOS)
 #include "LacefxESView.h"
 #include "LXDraw_iosgles2.h"
#else
 #include <OpenGL/GL.h>
#endif


#define LXSHADERPARAMCOUNT 24


typedef struct {
    LXREF_STRUCT_HEADER

    char        *programStr;
    LXUInteger  programStrLen;
    LXUInteger  programType;
    
    LXUInteger  programErrorWarningState;

#if defined(LXPLATFORM_WIN)
    char        *dxPixelShaderStr;
    IDirect3DPixelShader9 *dxShader;
#elif defined(LXPLATFORM_IOS)
    GLuint      programID;
    GLint       programUniformLocations[NUM_UNIFORMS];
#else
    GLuint      fragProgID;
#endif
    
    LXUInteger  storageHint;
    
    void        *fpClsObj;    
    float       shaderParams[LXSHADERPARAMCOUNT * 4];
    
    // these are only valid for Direct3D where program locals must go into the same set of registers as temps
    LXUInteger  numProgramLocals;
    LXUInteger  regIndexForFirstProgramLocal;
    
    LXUInteger  sharedStorageFlags;  // used for copies of shaders declared final
    
    void        *_reservedA[7];
} LXShaderImpl;


enum {
    kLXShader_NativeObjectWasCopied = 1
};


#ifdef __cplusplus
extern "C" {
#endif

// implemented along with other non-platform specific methods in LXShader_eval.c
extern void _LXShaderSetProgramString(LXShaderImpl *imp, const char *prog, size_t progLen, LXUInteger progType);

extern void _LXShaderCommonDataFree(LXShaderImpl *imp);

extern LXShaderRef _LXShaderCommonCopy(LXShaderRef r);

extern LXShaderRef _LXShaderCommonCreateWithString(const char *asciiStr,
                                                      size_t strLen,
                                                      LXUInteger programFormat,
                                                      LXUInteger storageHint,
                                                      LXError *outError);


#if defined(LXPLATFORM_WIN)

LXEXPORT void LXShaderGetD3DShaderConstantsInfo_(LXShaderRef r, LXUInteger *outNumProgramLocals, LXUInteger *outRegIndex, void *_reserved);

LXEXPORT void LXShaderSetD3DShaderConstantsInfo_(LXShaderRef r, LXUInteger numProgramLocals, LXUInteger regIndex, void *_reserved);

#endif

#ifdef __cplusplus
}
#endif

