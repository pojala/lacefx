/*
 *  LXDraw_Impl.h
 *  Lacefx private
 *
 *  Created by Pauli Ojala on 23.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXDRAW_IMPL_H_
#define _LXDRAW_IMPL_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"

#if defined(LXPLATFORM_WIN)
#include <d3d9.h>
#endif


typedef struct _LXDrawContextActiveState {
    LXBool didPushMVMatrix;
    LXBool didPushProjMatrix;
    
    LXBool didCreateTempProjMatrix;
    
#if defined(LXPLATFORM_WIN)
    IDirect3DDevice9 *dev;
    void *surf;
    int32_t texInUseCount;
    
    D3DMATRIX prevMVMatrix;
    D3DMATRIX prevProjMatrix;
#endif

#if defined(LXPLATFORM_IOS)
    float projModelviewGLMatrix[16];
#endif

} LXDrawContextActiveState;


#ifdef __cplusplus
extern "C" {
#endif


/*
following are implemented in LXDrawContext.c and call the platform-dependent implementations
*/
///void LXDrawContextApply_(LXDrawContextRef r);
void LXDrawContextCreateState_(LXDrawContextRef r, LXDrawContextActiveState **outState);
void LXDrawContextBeginForSurface_(LXDrawContextRef r, LXSurfaceRef surface, LXDrawContextActiveState **outState);
void LXDrawContextFinish_(LXDrawContextRef r, LXDrawContextActiveState **outState);

/*
following are considered private
*/
LXTextureArrayRef LXDrawContextGetTextureArray(LXDrawContextRef r);

/*
following functions are the platform-dependent (OpenGL/D3D) implementation of drawing using an LXDrawContext
*/

void LXTransform3DBeginForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state);
void LXTransform3DFinishForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state);

void LXShaderApplyWithConstants_(LXShaderRef r, float *constants, int constCount, LXDrawContextActiveState *state);
void LXTextureArrayApply_(LXTextureArrayRef r, LXDrawContextActiveState *state);

void LXShaderFinishCurrent_(LXDrawContextActiveState *state);
void LXTextureArrayFinishCurrent_(LXDrawContextActiveState *state);

// fallbacks for special case rendering
void LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(LXDrawContextRef drawCtx);

// private platform info
LXBool LXSurfaceHasDefaultPixelOrthoProjection(LXSurfaceRef r);

#ifdef __cplusplus
}
#endif

#endif
