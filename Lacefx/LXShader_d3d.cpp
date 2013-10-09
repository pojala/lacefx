/*
 *  LXShader_d3d.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 23.9.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXShader.h"
#include "LXRef_Impl.h"
#include "LXShader_Impl.h"
#include "LXShaderTranslation.h"
#include "LXPlatform_d3d.h"
#include <d3dx9.h>


LXShaderRef LXShaderRetain(LXShaderRef r)
{
    if ( !r) return NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXShaderRelease(LXShaderRef r)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        _lx_free(imp->dxPixelShaderStr);
        
        if (imp->dxShader) {
            imp->dxShader->Release();
            imp->dxShader = NULL;
        }
        
        _LXShaderCommonDataFree(imp);

        _lx_free(imp);
    }
}

LXShaderRef LXShaderCopy(LXShaderRef orig)
{
    LXShaderRef newShader = _LXShaderCommonCopy(orig);
    
    if (newShader == orig)
        return newShader;
    
    if (newShader) {
        LXShaderImpl *imp = (LXShaderImpl *)newShader;

        const char *psStr = ((LXShaderImpl *)orig)->dxPixelShaderStr;
        if (psStr) {
            imp->dxPixelShaderStr = (char *) _lx_malloc(strlen(psStr) + 1);
            memcpy(imp->dxPixelShaderStr, psStr, strlen(psStr) + 1);
        }
        imp->numProgramLocals = ((LXShaderImpl *)orig)->numProgramLocals;
        imp->regIndexForFirstProgramLocal = ((LXShaderImpl *)orig)->regIndexForFirstProgramLocal;
        
        if (imp->programType == kLXShaderFormat_OpenGLARBfp) {
            if (imp->storageHint == kLXStorageHint_Final) {
                // for static shader string, we can just retain the existing shader
                IDirect3DPixelShader9 *dxShader = ((LXShaderImpl *)orig)->dxShader;
                if (dxShader) {
                    dxShader->AddRef();
                    imp->dxShader = dxShader;
                }                
            }
            else {
                LPD3DXBUFFER pCode = NULL;
                LPD3DXBUFFER pErrorMsgs = NULL;
                HRESULT hRes;
    
                hRes = D3DXAssembleShader(psStr, strlen(psStr), NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &pCode, &pErrorMsgs);
                
                if (FAILED(hRes)) {
                    char *messageFromDX = (char *)pErrorMsgs->GetBufferPointer();
                    LXPrintf("*** %s: shader assembly failed: error is: %s\n", __func__, messageFromDX);
                    LXShaderRelease(newShader);
                    return NULL;
                } else {
                    hRes = LXPlatformGetSharedD3D9Device()->CreatePixelShader((DWORD *)pCode->GetBufferPointer(), &(imp->dxShader));
                    if (hRes != D3D_OK) {
                        LXPrintf("*** %s: dxDevice CreatePixelShader failed (error %i)\n", __func__, hRes);
                        LXShaderRelease(newShader);
                        return NULL;
                    }
                }
            }
        }
    }
    return newShader;
}

LXShaderRef LXShaderCreateWithString(const char *asciiStr,
                                                      size_t strLen,
                                                      LXUInteger programFormat,
                                                      LXUInteger storageHint,
                                                      LXError *outError)
{
    LXShaderRef shader = _LXShaderCommonCreateWithString(asciiStr, strLen, programFormat, storageHint, outError);
    
    if ( !shader)
        return NULL;

    LXShaderImpl *imp = (LXShaderImpl *)shader;
    
    char *psStr = NULL;
    LXShaderTranslationInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    
    if (programFormat == kLXShaderFormat_Direct3DPixelShaderAssembly) {
        psStr = (char *) _lx_calloc(strLen + 1, 1);
        memcpy(psStr, asciiStr, strLen);
        
        imp->numProgramLocals = 16;  // uh, I guess we can't know this
    }
    else if (programFormat == kLXShaderFormat_OpenGLARBfp) {        
        LXConvertShaderString_OpenGLARBfp_to_D3DPS(asciiStr, LXPlatformGetD3DPixelShaderClass(),
                                                    &psStr, &shaderInfo);
                
        if ( !psStr) {
            LXPrintf("*** shader D3D conversion failed\n");
        }
    }
    else {
        LXPrintf("*** %s: unsupported shader format (%i)\n", __func__, programFormat);
    }
    
    if (psStr) {
        LPD3DXBUFFER pCode = NULL;
        LPD3DXBUFFER pErrorMsgs = NULL;
        HRESULT hRes;
        
        ///LXPrintf("%s: going to assemble D3D shader: %s\n", __func__, psStr);

        hRes = D3DXAssembleShader(psStr, strlen(psStr), NULL, NULL, 0, //D3DXSHADER_SKIPVALIDATION,
                                  &pCode, &pErrorMsgs);
        
        if (FAILED(hRes)) {
            char *messageFromDX = (char *)pErrorMsgs->GetBufferPointer();
            LXPrintf("*** shader assembly failed: error is: %s\n", messageFromDX);
        } else {
            hRes = LXPlatformGetSharedD3D9Device()->CreatePixelShader((DWORD *)pCode->GetBufferPointer(), &(imp->dxShader));
            if (hRes != D3D_OK) {
                LXPrintf("*** dxDevice CreatePixelShader failed (error %i)\n", hRes);
            } else {
                imp->dxPixelShaderStr = psStr;
                
                imp->numProgramLocals = shaderInfo.numProgramLocalsInUse;
                imp->regIndexForFirstProgramLocal = shaderInfo.regIndexForFirstProgramLocal;
    
                ///LXPrintf("...lxshader %p: created D3D shader %p: paramcount is %i, first reg index %i\n", imp, imp->dxShader, imp->numProgramLocals, imp->regIndexForFirstProgramLocal);
            }
        }
    }
    
    return shader;
}

void LXShaderSetD3DShaderConstantsInfo_(LXShaderRef r, LXUInteger numProgramLocals, LXUInteger regIndex, void *_reserved)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    imp->numProgramLocals = numProgramLocals;
    imp->regIndexForFirstProgramLocal = regIndex;
}

void LXShaderGetD3DShaderConstantsInfo_(LXShaderRef r, LXUInteger *outNumProgramLocals, LXUInteger *outRegIndex, void *_reserved)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    if (outNumProgramLocals) *outNumProgramLocals = imp->numProgramLocals;
    if (outRegIndex) *outRegIndex = imp->regIndexForFirstProgramLocal;
}


LXUnidentifiedNativeObj LXShaderLockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    ///imp->retCount++;
    
    return (LXUnidentifiedNativeObj)(imp->dxShader);
}

void LXShaderUnlockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    ///imp->retCount--;
}


