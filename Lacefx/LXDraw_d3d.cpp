/*
 *  LXDraw_d3d.cpp
 *  Lacefx
 *
 *  Created by Pauli Ojala on 17.11.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXDrawContext.h"
#include "LXRef_Impl.h"
#include "LXDraw_Impl.h"
#include "LXPlatform_d3d.h"
#include "LXShader.h"
#include "LXShader_Impl.h"
#include "LXTransform3D.h"
#include <d3dx9.h>


#define GETDXDEVICE(_dev_)  \
    IDirect3DDevice9 *_dev_ = state->dev;  \
    if ( !_dev_) {  \
        _dev_ = LXPlatformGetSharedD3D9Device();  \
        state->dev = _dev_;  }


static void convertGLStyleMatrixToD3DMatrix_(LX4x4Matrix *mat, D3DMATRIX *dstMat)
{
    // convert GL-style right hand coords to D3D left hand coords.
    //
    // !! assumes that D3D matrix and LX matrix element size is same --
    //    check is in the public function below
    
    memcpy(dstMat, mat, sizeof(D3DMATRIX));
    dstMat->_31 *= -1.0f;
    dstMat->_32 *= -1.0f;
    dstMat->_33 *= -1.0f;
    dstMat->_34 *= -1.0f;
    
    dstMat->_41 = mat->m14;
    dstMat->_42 = mat->m24;
    dstMat->_43 = mat->m34;
    dstMat->_14 = mat->m41;
    dstMat->_24 = mat->m42;
    dstMat->_34 = mat->m43;
}

void LXTransform3DConvertToD3DMatrix_(LXTransform3DRef r, D3DMATRIX *dstMat)
{
    LX4x4Matrix mat;

    if (sizeof(LX4x4Matrix) != sizeof(D3DMATRIX)) {
        LXPrintf("*** %s: matrix size is invalid on this platform\n", __func__);
        return;
    }
    LXTransform3DGetMatrix(r, &mat);
    convertGLStyleMatrixToD3DMatrix_(&mat, dstMat);
}

void LXTransform3DBeginForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    if ( !state) return;
    GETDXDEVICE(dev)
    D3DMATRIX d3dMat;
    
    if (proj) {
        dev->GetTransform(D3DTS_PROJECTION, &(state->prevProjMatrix));    
        LXTransform3DConvertToD3DMatrix_(proj, &d3dMat);
        dev->SetTransform(D3DTS_PROJECTION, &d3dMat);
    }
    
    if (1) {
        dev->GetTransform(D3DTS_VIEW, &(state->prevMVMatrix));
        
        if (mv) {
            LXTransform3DConvertToD3DMatrix_(mv, &d3dMat);
        } else {
            LX4x4Matrix ident = *LXIdentity4x4Matrix;
            convertGLStyleMatrixToD3DMatrix_(&ident, &d3dMat);
        }
    
        d3dMat._44 = 1.0f;
    
        if (dev->SetTransform(D3DTS_VIEW, &d3dMat) != D3D_OK) {
            LXPrintf("*** %s: modelview set failed\n", __func__);
        }
    }
}


void LXTransform3DFinishForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    if ( !state) return;

    IDirect3DDevice9 *dev = state->dev;
    dev->SetTransform(D3DTS_PROJECTION, &(state->prevProjMatrix));
    dev->SetTransform(D3DTS_VIEW, &(state->prevMVMatrix));
}


void LXShaderApplyWithConstants_(LXShaderRef shader, float *localParams, int paramCount, LXDrawContextActiveState *state)
{
    GETDXDEVICE(dev)
    IDirect3DPixelShader9 *dxShader = (IDirect3DPixelShader9 *)LXShaderLockPlatformNativeObj(shader);
    
    ///LXPrintf("%s: shader is %p; local params %p (%i)\n", __func__, dxShader, localParams, paramCount);
    
    dev->SetPixelShader(dxShader);
    
    LXShaderUnlockPlatformNativeObj(shader);
    
    if ( !dxShader)
        return;
        
    LXUInteger shaderNumLocals = 0;
    LXUInteger shaderLocalsFirstReg = 0;
    LXShaderGetD3DShaderConstantsInfo_(shader, &shaderNumLocals, &shaderLocalsFirstReg, NULL);
    
    ///LXPrintf(".... shader numlocals: %i, first reg %i\n", shaderNumLocals, shaderLocalsFirstReg);
                
    float params[16 * 4];
    memset(params, 0, 16*4*sizeof(float));
    
    if ( !localParams) {
        paramCount = MIN(16, shaderNumLocals);
        int i;
        for (i = 0; i < paramCount; i++) {
            LXShaderGetParameter4fv(shader, i, params + (i * 4));
        }
        localParams = params;
    } else {
        paramCount = MIN(paramCount, shaderNumLocals);
    }
    
    if (localParams) {
        ///LXPrintf("..setting localparms, first (%i) - %f, %f, %f, %f\n", shaderLocalsFirstReg, localParams[0], localParams[1], localParams[2], localParams[3]);
        dev->SetPixelShaderConstantF(shaderLocalsFirstReg, localParams, paramCount);
    }
}

void LXTextureArrayApply_(LXTextureArrayRef array, LXDrawContextActiveState *state)
{
    GETDXDEVICE(dev)
    int texCount = LXTextureArrayInUseCount(array);
    
    if (texCount > 0) {
        int i;
        for (i = 0; i < texCount; i++) {
            LXTextureRef texture = LXTextureArrayAt(array, i);
            IDirect3DTexture9 *dxTex = (IDirect3DTexture9 *)LXTextureLockPlatformNativeObj(texture);
            if ( !dxTex) {
                dev->SetTexture(i, NULL);
            } else {
                dev->SetTexture(i, dxTex);
                
                LXUInteger samplingOverride = LXTextureArrayGetSamplingAt(array, i);
                LXUInteger sampling = (samplingOverride == kLXNotFound) ? LXTextureGetSampling(texture) : samplingOverride;
                
                LXUInteger d3dSampling = (sampling == kLXLinearSampling) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
                
                dev->SetSamplerState(i, D3DSAMP_MAGFILTER, d3dSampling);
                dev->SetSamplerState(i, D3DSAMP_MINFILTER, d3dSampling);
                
                ///LXPrintf("..d3d sampler %i: mode is linear %i\n", i, (sampling == kLXLinearSampling));
            }
            LXTextureUnlockPlatformNativeObj(texture);
        }
    } else {
        dev->SetTexture(0, NULL);
    }
    
    state->texInUseCount = texCount;
}

void LXShaderFinishCurrent_(LXDrawContextActiveState *state)
{
    IDirect3DDevice9 *dev = state->dev;

    dev->SetPixelShader(NULL);
}

void LXTextureArrayFinishCurrent_(LXDrawContextActiveState *state)
{
    IDirect3DDevice9 *dev = state->dev;
    
    int texCount = state->texInUseCount;
    if (texCount > 0) {
        int i;
        for (i = 0; i < texCount; i++) {
            dev->SetTexture(i, NULL);
        }
    }
}
