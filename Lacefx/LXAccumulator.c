/*
 *  LXAccumulator.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 11.10.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXAccumulator.h"
#import "LXRef_Impl.h"
#import "LXSurface.h"
#import "LXShader.h"


typedef struct {
    LXREF_STRUCT_HEADER
    
    LXPoolRef surfPool;
    LXSize size;
    LXPixelFormat pxFormat;
    LXInteger numSamples;
    LXFloat opacity;
    
    LXInteger currentSample;
    LXBool isComplete;
    
    LXSurfaceRef surf1;
    LXSurfaceRef surf2;
    
    LXShaderRef firstPassShader;
    LXShaderRef accShader;
    
} LXAccumulatorImpl;



const char *LXAccumulatorTypeID()
{
    static const char *s = "LXAccumulator";
    return s;
}

LXAccumulatorRef LXAccumulatorRetain(LXAccumulatorRef r)
{
    if ( !r) return NULL;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXAccumulatorRelease(LXAccumulatorRef r)
{
    if ( !r) return;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);
        
        imp->surfPool = NULL;
        LXSurfaceRelease(imp->surf1);
        LXSurfaceRelease(imp->surf2);
        
        LXShaderRelease(imp->firstPassShader);
        LXShaderRelease(imp->accShader);
        
        _lx_free(imp);
    }
}

LXAccumulatorRef LXAccumulatorCreateWithSize(LXPoolRef pool, LXSize size, LXUInteger flags, LXError *outError)
{
    LXAccumulatorImpl *imp = _lx_calloc(sizeof(LXAccumulatorImpl), 1);
    LXREF_INIT(imp, LXAccumulatorTypeID(), LXAccumulatorRetain, LXAccumulatorRelease);
    
    imp->surfPool = pool;
    imp->size = size;
    imp->pxFormat = kLX_RGBA_FLOAT16;
    imp->numSamples = 4;   // default
    
    static const char *ss_firstPass = "!!ARBfp1.0"
                     "TEMP t0;  "
                     "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                     "MUL t0, t0, program.local[0];  "
                     "MOV result.color, t0;  "
                     "END";

    imp->firstPassShader = LXShaderCreateWithString(ss_firstPass, strlen(ss_firstPass), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, outError);
    if ( !imp->firstPassShader) {
        LXAccumulatorRelease((LXAccumulatorRef)imp);
        return NULL;
    }
    
    static const char *ss_acc = "!!ARBfp1.0"
                     "TEMP t0, t1;  "
                     "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                     "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                     "MAD t0, t1, program.local[0], t0;  "
                     "MOV result.color, t0;  "
                     "END";

    imp->accShader = LXShaderCreateWithString(ss_acc, strlen(ss_acc), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, outError);
    if ( !imp->accShader) {
        LXAccumulatorRelease((LXAccumulatorRef)imp);
        return NULL;
    }
    
    return (LXAccumulatorRef)imp;
}


void LXAccumulatorSetNumberOfSamples(LXAccumulatorRef r, LXInteger numSamples)
{
    if ( !r) return;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    if (numSamples != imp->numSamples) {
        // for high sample counts, we must use a 32-bit pixel format, or there will be nasty artifacts from the accumulation
        if (numSamples > 16 && imp->numSamples <= 16) {
            imp->pxFormat = kLX_RGBA_FLOAT32;
            if (imp->surf1) LXSurfaceRelease(imp->surf1);
            if (imp->surf2) LXSurfaceRelease(imp->surf2);
            imp->surf1 = NULL;
            imp->surf2 = NULL;
        }
        else if (numSamples <= 16 && imp->numSamples > 16) {
            imp->pxFormat = kLX_RGBA_FLOAT16;
            if (imp->surf1) LXSurfaceRelease(imp->surf1);
            if (imp->surf2) LXSurfaceRelease(imp->surf2);
            imp->surf1 = NULL;
            imp->surf2 = NULL;        
        }
        
        imp->numSamples = numSamples;
    }
}

LXInteger LXAccumulatorGetNumberOfSamples(LXAccumulatorRef r)
{
    if ( !r) return 0;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    return imp->numSamples;
}

LXSize LXAccumulatorGetSize(LXAccumulatorRef r)
{
    if ( !r) return LXZeroSize;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    return imp->size;
}

LXBool LXAccumulatorMatchesSize(LXAccumulatorRef r, LXSize size)
{
    if ( !r) return NO;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    
    return (imp->size.w == size.w && imp->size.h == size.h) ? YES : NO;
}


#pragma mark --- accumulation ---

LXBool LXAccumulatorStartAccumulation(LXAccumulatorRef r, LXError *outError)
{
    if ( !r) return NO;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;

    imp->currentSample = 0;
    
    if ( !imp->surf1) {
        imp->surf1 = LXSurfaceCreate(imp->surfPool, imp->size.w, imp->size.h, imp->pxFormat, 0, outError);
        if ( !imp->surf1) return NO;
    }
    
    if ( !imp->surf2) {
        imp->surf2 = LXSurfaceCreate(imp->surfPool, imp->size.w, imp->size.h, imp->pxFormat, 0, outError);
        if ( !imp->surf2) return NO;
    }
    
    return YES;
}

void LXAccumulatorAccumulateTexture(LXAccumulatorRef r, LXTextureRef texture)
{
    if ( !r) return;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    
    LXFloat opacity = (imp->numSamples <= 0) ? 1.0
                                             : (1.0 / imp->numSamples);
    
    LXAccumulatorAccumulateTextureWithOpacity(r, texture, opacity);
}

void LXAccumulatorAccumulateTextureWithOpacity(LXAccumulatorRef r, LXTextureRef texture, LXFloat op)
{
    if ( !r) return;
    if ( !texture) return;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    
    const LXBool isFirstPass = (imp->currentSample == 0);
    const LXBool isEvenPass = (imp->currentSample % 2 == 0);
    LXSurfaceRef surf = (isEvenPass) ? imp->surf1 : imp->surf2;
    LXSurfaceRef prevSurf = (isFirstPass) ? NULL : ((isEvenPass) ? imp->surf2 : imp->surf1);

    LXDrawContextRef drawCtx = LXDrawContextCreate();
    LXTextureArrayRef texArray = NULL;

    LXDrawContextSetShader(drawCtx, (isFirstPass) ? imp->firstPassShader : imp->accShader);
    LXDrawContextSetShaderParameter(drawCtx, 0, LXMakeRGBA(op, op, op, op));
    
    if (isFirstPass) {
        //LXPrintf("first pass: copying into surf %p, tex %p\n", surf, texture);
        LXSurfaceCopyTexture(surf, texture, drawCtx);
    } else {
        //LXPrintf("pass %i: copying into surf %p from surf %p, tex %p\n", (int)imp->currentSample, surf, prevSurf, texture);
        texArray = LXTextureArrayCreateWithTextures(2, LXSurfaceGetTexture(prevSurf), texture);
        LXDrawContextSetTextureArray(drawCtx, texArray);
        
        LXSurfaceCopyTexture(surf, texArray, drawCtx);
    }
    
    imp->currentSample++;
    
    LXSurfaceFlush(surf);
    LXTextureArrayRelease(texArray);
    LXDrawContextRelease(drawCtx);
}

LXSurfaceRef LXAccumulatorFinishAccumulation(LXAccumulatorRef r)
{
    if ( !r) return NULL;
    LXAccumulatorImpl *imp = (LXAccumulatorImpl *)r;
    
    if (imp->currentSample <= 0) {
        return NULL;
    }
    
    const LXBool isEvenPass = (imp->currentSample % 2 == 0);
    LXSurfaceRef lastSurf = (isEvenPass) ? imp->surf2 : imp->surf1;
    
    return LXSurfaceRetain(lastSurf);
}


