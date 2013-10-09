/*
 *  LXDrawContext.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 23.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXDrawContext.h"
#include "LXDrawContext_Impl.h"
#include "LXDraw_Impl.h"
#include "LXRef_Impl.h"
#include "LXTextureArray.h"
#include "LXTransform3D.h"
#include "LXPool.h"
#include "LXShader.h"
#include "LXSurface.h"



LXDrawContextRef LXDrawContextRetain(LXDrawContextRef r)
{
    if ( !r) return NULL;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXDrawContextRelease(LXDrawContextRef r)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        LXRefRelease((LXRef)(imp->texArray));
        LXRefRelease((LXRef)(imp->shader));
        LXRefRelease((LXRef)(imp->projTransform));
        LXRefRelease((LXRef)(imp->mvTransform));

        _lx_free(imp);
    }
}


#pragma mark -- init methods ---

const char *LXDrawContextTypeID()
{
    static const char *s = "LXDrawContext";
    return s;
}

LXDrawContextRef LXDrawContextCreate()
{
    LXDrawContextImpl *imp = (LXDrawContextImpl *) _lx_calloc(sizeof(LXDrawContextImpl), 1);

    LXREF_INIT(imp, LXDrawContextTypeID(), LXDrawContextRetain, LXDrawContextRelease);
    
    imp->shader = NULL;  // created lazily
    
    return (LXDrawContextRef)imp;
}


LXDrawContextRef LXDrawContextWithTexture(LXPoolRef pool, LXTextureRef texture)
{
    LXDrawContextRef r = LXDrawContextCreate();
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    imp->texArray = (texture) ? LXTextureArrayCreateWithTexture(texture) : NULL;
        
    LXPoolAutorelease(pool, r);
    return r;
}

LXDrawContextRef LXDrawContextWithTexturesAndCount(LXPoolRef pool, LXTextureRef *textures, LXInteger texCount)
{
    LXDrawContextRef r = LXDrawContextCreate();
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    imp->texArray = (textures && texCount > 0) ? LXTextureArrayCreateWithTexturesAndCount(textures, texCount) : NULL;
        
    LXPoolAutorelease(pool, r);
    return r;
}

LXDrawContextRef LXDrawContextWithTextureArray(LXPoolRef pool, LXTextureArrayRef array)
{
    LXDrawContextRef r = LXDrawContextCreate();
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    imp->texArray = (LXTextureArrayRef) LXRefRetain((LXRef)array);
        
    LXPoolAutorelease(pool, r);
    return r;
}

LXDrawContextRef LXDrawContextWithTextureArrayAndShader(LXPoolRef pool, LXTextureArrayRef array, LXShaderRef shader)
{
    LXDrawContextRef r = LXDrawContextCreate();
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    imp->texArray = (LXTextureArrayRef) LXRefRetain((LXRef)array);
    
    if (shader) {
        LXShaderRelease(imp->shader);
        imp->shader = (LXShaderRef) LXRefRetain((LXRef)shader);
    }
    
    LXPoolAutorelease(pool, r);
    return r;
}


#pragma mark --- setters ---

void LXDrawContextSetFlags(LXDrawContextRef ctx, LXUInteger flags)
{
    if ( !ctx) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)ctx;
    
    imp->flags = flags;
}

LXUInteger LXDrawContextGetFlags(LXDrawContextRef ctx)
{
    if ( !ctx) return 0;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)ctx;

    return imp->flags;
}



void LXDrawContextSetTextureArray(LXDrawContextRef r, LXTextureArrayRef array)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;    
    
    LXTextureArrayRef prevArr = imp->texArray;
    
    imp->texArray = (LXTextureArrayRef) LXRefRetain((LXRef)array);
    
    if (prevArr)
        LXRefRelease((LXRef)prevArr);
}

void LXDrawContextSetShader(LXDrawContextRef r, LXShaderRef shader)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    if (0) { ///if (imp->shaderIsOwnedByUs) {
        LXShaderRef prevShader = imp->shader;
    
        imp->shader = (shader) ? LXShaderCopy(shader) : LXShaderCreateEmpty();
    
        LXShaderCopyParameters(imp->shader, prevShader);
    
        LXShaderRelease(prevShader);
    }
    else {
        LXShaderRelease(imp->shader);
        imp->shader = LXShaderRetain(shader);
        imp->shaderIsOwnedByUs = NO;
    }
}

LXShaderRef LXDrawContextGetShader_(LXDrawContextRef r)
{
    if ( !r) return NULL;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    return imp->shader;
}

void LXDrawContextSetProjectionTransform(LXDrawContextRef r, LXTransform3DRef transform)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    if (imp->projTransform)
        LXRefRelease((LXRef)(imp->projTransform));
    
    imp->projTransform = (LXTransform3DRef) LXRefRetain((LXRef)transform);
}

void LXDrawContextSetModelViewTransform(LXDrawContextRef r, LXTransform3DRef transform)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    if (imp->mvTransform)
        LXRefRelease((LXRef)(imp->mvTransform));
    
    imp->mvTransform = (LXTransform3DRef) LXRefRetain((LXRef)transform);    
}


#pragma mark --- getters ---

LXTransform3DRef LXDrawContextGetModelViewTransform(LXDrawContextRef r)
{
    if ( !r) return NULL;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    return imp->mvTransform;
}

LXInteger LXDrawContextGetTextureCount(LXDrawContextRef r)
{
    if ( !r) return 0;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    return LXTextureArrayInUseCount(imp->texArray);
}


#pragma mark --- private getters ---

LXTextureArrayRef LXDrawContextGetTextureArray(LXDrawContextRef r)
{
    if ( !r) return NULL;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;

    return imp->texArray;
}


#pragma mark --- private to implementation ---

void LXDrawContextCreateState_(LXDrawContextRef r, LXDrawContextActiveState **outState)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    #pragma unused (imp)

    if ( !outState) {
        printf("** %s: state ptr missing\n", __func__);
        return;
    }
    *outState = _lx_calloc(1, sizeof(LXDrawContextActiveState));
}

void LXDrawContextBeginForSurface_(LXDrawContextRef r, LXSurfaceRef surface, LXDrawContextActiveState **outState)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;

    if ( !outState) {
        printf("** %s: state ptr missing\n", __func__);
        return;
    }
    if (*outState == NULL) {
        LXDrawContextCreateState_(r, outState);
    }

    // on Direct3D we probably don't have a default projection already in place
    LXBool createDefaultProj = (imp->projTransform) ? NO : YES;
    
    if (createDefaultProj && surface) {
        LXBool isFlipped = LXSurfaceNativeYAxisIsFlipped(surface);
            
        LXTransform3DRef projTrs = LXTransform3DCreateIdentity();
        LXTransform3DConcatExactPixelsTransformForSurfaceSize(projTrs, LXSurfaceGetWidth(surface), LXSurfaceGetHeight(surface), isFlipped);
            
        imp->projTransform = projTrs;
        
        (*outState)->didCreateTempProjMatrix = YES;
    }
    
    LXTransform3DBeginForProjectionAndModelView_(imp->projTransform, imp->mvTransform, *outState);

    if (imp->texArray) {
        LXTextureArrayApply_(imp->texArray, *outState);
    }
    
    // must call this even if shader is null, so the native implementation can setup a replacement shader if needed
    LXShaderApplyWithConstants_(imp->shader, NULL, 0, *outState);
}

void LXDrawContextFinish_(LXDrawContextRef r, LXDrawContextActiveState **outState)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;

    if ( !outState) {
        printf("** %s: state missing\n", __func__);
        return;
    }

    LXTransform3DFinishForProjectionAndModelView_(imp->projTransform, imp->mvTransform, *outState);

    LXShaderFinishCurrent_(*outState);
    LXTextureArrayFinishCurrent_(*outState);

    if ((*outState)->didCreateTempProjMatrix) {
        LXTransform3DRelease(imp->projTransform);
        imp->projTransform = NULL;
    }

    if (*outState) {
        _lx_free(*outState);
        *outState = NULL;
    }
}




#pragma mark --- pixel program constants ---

void LXDrawContextSetShaderParameter(LXDrawContextRef r, LXInteger paramIndex, LXRGBA value)
{
   if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    if ( !imp->shaderIsOwnedByUs) {  // copy now so we don't create side effects if shader is used by someone else
        LXShaderRef prevShader = imp->shader;
        imp->shader = (prevShader) ? LXShaderCopy(prevShader) : LXShaderCreateEmpty();
        LXShaderRelease(prevShader);
        imp->shaderIsOwnedByUs = YES;
        ///LXPrintf("%s, %p -- copying shader: %p -> %p\n", __func__, r, prevShader, imp->shader);
    }

    LXShaderSetParameter(imp->shader, paramIndex, value);
}

LXRGBA LXDrawContextGetShaderParameter(LXDrawContextRef r, LXInteger paramIndex)
{
   if ( !r) return LXMakeRGBA(0, 0, 0, 0);
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
 
    return LXShaderGetParameter(imp->shader, paramIndex);
}

void LXDrawContextCopyParametersFromShader(LXDrawContextRef r, LXShaderRef aShader)
{
    if ( !r) return;
    LXDrawContextImpl *imp = (LXDrawContextImpl *)r;
    
    if (imp->shader) {
        if ( !imp->shaderIsOwnedByUs) {  // copy now so we don't create side effects if shader is used by someone else
            LXShaderRef prevShader = imp->shader;
            imp->shader = (prevShader) ? LXShaderCopy(prevShader) : LXShaderCreateEmpty();
            LXShaderRelease(prevShader);
            imp->shaderIsOwnedByUs = YES;
        }

        LXShaderCopyParameters(imp->shader, aShader);
    }
}

