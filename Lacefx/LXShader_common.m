/*
 *  LXShader_eval.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.3.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import <Foundation/Foundation.h>

#include "LXShader.h"
#include "LXRef_Impl.h"
#include "LXShader_Impl.h"
#include "LXFPClosure.h"  // Conduit's ARBfp execution engine
#include "LXMutex.h"



static NSMutableDictionary *g_staticShadersMap = nil;

extern LXMutexPtr g_lxAtomicLock;



const char *LXShaderTypeID()
{
    static const char *s = "LXShader";
    return s;
}


// empty LXShader; useful for storing parameter values or other metadata
LXShaderRef LXShaderCreateEmpty()
{
    LXShaderImpl *imp = _lx_calloc(sizeof(LXShaderImpl), 1);
    
    LXREF_INIT(imp, LXShaderTypeID(), LXShaderRetain, LXShaderRelease);
    
    return (LXShaderRef)imp;
}




#pragma mark --- shader parameters ---

void LXShaderCopyParameters(LXShaderRef shader, LXShaderRef otherShader)
{
    if (!shader || !otherShader)
        return;
        
    LXShaderImpl *impDst = (LXShaderImpl *)shader;
    LXShaderImpl *impSrc = (LXShaderImpl *)otherShader;

    memcpy(impDst->shaderParams,  impSrc->shaderParams,  LXSHADERPARAMCOUNT * 4 * sizeof(float));
}

LXInteger LXShaderGetMaxNumberOfParameters(LXShaderRef r)
{
    return LXSHADERPARAMCOUNT;
}

void LXShaderSetParameter(LXShaderRef r, LXInteger paramIndex, LXRGBA value)
{
    float vv[4] = { value.r, value.g, value.b, value.a };
    LXShaderSetParameter4fv(r, paramIndex, vv);
}


void LXShaderSetParameter4f(LXShaderRef r,
                                 LXInteger paramIndex,
                                 float v1, float v2, float v3, float v4)
{
    float vv[4] = { v1, v2, v3, v4 };
    LXShaderSetParameter4fv(r, paramIndex, vv);
}

void LXShaderSetParameter4fv(LXShaderRef r,
                                  LXInteger paramIndex,
                                  float *pv)
{
    if ( !r || !pv) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    if (paramIndex >= LXSHADERPARAMCOUNT)
        return;
        
    memcpy(imp->shaderParams + 4*paramIndex, pv, 4*sizeof(float));
}


LXRGBA LXShaderGetParameter(LXShaderRef shader, LXInteger paramIndex)
{
    float vv[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    LXShaderGetParameter4fv(shader, paramIndex, vv);
    
    return LXMakeRGBA(vv[0], vv[1], vv[2], vv[3]);
}

void LXShaderGetParameter4fv(LXShaderRef r,
                                  LXInteger paramIndex,
                                  float *outV)
{
    if ( !r || !outV) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;

    if (paramIndex >= LXSHADERPARAMCOUNT)
        return;
        
    memcpy(outV, imp->shaderParams + 4*paramIndex, 4*sizeof(float));
}


#pragma mark --- common init ---


void _LXShaderSetProgramString(LXShaderImpl *imp, const char *prog, size_t progLen, LXUInteger progType)
{
    if (imp->programStr) {
        _lx_free(imp->programStr);
        imp->programStr = NULL;
    }
        
    if (prog && progLen > 0) {
        imp->programStr = _lx_malloc(progLen + 1);
        imp->programStr[progLen] = 0;
        
        memcpy(imp->programStr, prog, progLen);
    }   
        
    imp->programStrLen = progLen;
    imp->programType = progType;
}

void _LXShaderCommonDataFree(LXShaderImpl *imp)
{
    if (imp->storageHint != kLXStorageHint_Final) {
        if (imp->fpClsObj)
            LXFPClosureDestroy(imp->fpClsObj);

        if (imp->programStr)
            _lx_free(imp->programStr);
    }
    imp->fpClsObj = NULL;
    imp->programStr = NULL;
}

LXShaderRef _LXShaderCommonCopy(LXShaderRef r)
{
    if ( !r) return NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;

    LXShaderImpl *newImp = _lx_calloc(sizeof(LXShaderImpl), 1);

    LXREF_INIT(newImp, LXShaderTypeID(), LXShaderRetain, LXShaderRelease);

    newImp->storageHint = imp->storageHint;
    newImp->programType = imp->programType;

    if (imp->storageHint & kLXStorageHint_Final) {
        // for static shaders, we can copy only the pointers to the cached program string and closure object
        newImp->programStr = imp->programStr;
        newImp->programStrLen = imp->programStrLen;
        
        newImp->fpClsObj = imp->fpClsObj;
    }
    else {
        _LXShaderSetProgramString(newImp, imp->programStr, imp->programStrLen, imp->programType);
    }
    
    LXShaderCopyParameters((LXShaderRef)newImp, (LXShaderRef)imp);
    
    return (LXShaderRef)newImp;
}


LXShaderRef _LXCachedShaderForStaticString(const char *asciiStr)
{
    LXShaderRef obj = NULL;
    
    LXMutexLock(g_lxAtomicLock);

    if (g_staticShadersMap && asciiStr) {
        // the input program string is known to be a static pointer, so check the map if we already have a cached shader
        NSValue *mapKey = [NSValue valueWithPointer:asciiStr];
    
        obj = [[g_staticShadersMap objectForKey:mapKey] pointerValue];
    }
    LXMutexUnlock(g_lxAtomicLock);
    return obj;
}

void _LXSetCachedShaderForStaticString(LXShaderRef shader, const char *asciiStr)
{
    if ( !asciiStr) return;
    
    NSValue *mapKey = [NSValue valueWithPointer:asciiStr];

    // increment the retain count so the shader never gets released
    LXShaderRetain(shader);

    NSCAssert(g_lxAtomicLock, @"atomic lock mutex doesn't exist");
    LXMutexLock(g_lxAtomicLock);
    
    if ( !g_staticShadersMap)
        g_staticShadersMap = [[NSMutableDictionary alloc] init];
    
    [g_staticShadersMap setObject:[NSValue valueWithPointer:shader] forKey:mapKey];
    
    LXMutexUnlock(g_lxAtomicLock);
}


LXShaderRef _LXShaderCommonCreateWithString(const char *asciiStr,
                                                      size_t strLen,
                                                      LXUInteger programFormat,
                                                      LXUInteger storageHint,
                                                      LXError *outError)
{
    if ( !asciiStr || strLen < 5) {
        LXErrorSet(outError, kLXErrorID_Shader_EmptyArg, "empty or too short program string given as argument");
        return NULL;
    }

    if (storageHint == kLXStorageHint_Final) {
        LXShaderRef obj = _LXCachedShaderForStaticString(asciiStr);
        if (obj) {
            return LXShaderRetain(obj);
        }
    }

    LXShaderImpl *imp = _lx_calloc(sizeof(LXShaderImpl), 1);
    LXREF_INIT(imp, LXShaderTypeID(), LXShaderRetain, LXShaderRelease);
    
    imp->storageHint = storageHint;
    
    _LXShaderSetProgramString(imp, asciiStr, strLen, programFormat);
    
    // cache static shader
    if (storageHint == kLXStorageHint_Final) {
        // create the cached shader's GL/D3D object now, so it can be shared by future copies of this same shader
        LXUnidentifiedNativeObj nativeObj = LXShaderLockPlatformNativeObj((LXShaderRef)imp);
        #pragma unused (nativeObj)
        ///NSLog(@"will cache shader %p, nativeobj %i, stringptr %p", imp, nativeObj, asciiStr);
        LXShaderUnlockPlatformNativeObj((LXShaderRef)imp);        
        
        _LXSetCachedShaderForStaticString((LXShaderRef)imp, asciiStr);
    }

    if (outError) memset(outError, 0, sizeof(LXError));
    
    return (LXShaderRef)imp;
}



#pragma mark --- evaluation through FPClosure ---


static LXSuccess createCachedClosure(LXShaderImpl *imp)
{
    if ( !imp->programStr) return NO;
    if (imp->programStrLen < 1) return NO;
    if (imp->programType != kLXShaderFormat_FPClosure) return NO;
    
    
    LXFPClosurePtr cls = (imp->fpClsObj) ? (LXFPClosurePtr)(imp->fpClsObj) :
                                      LXFPClosureCreateWithString(imp->programStr, strlen(imp->programStr));
    if ( !cls) return NO;

    // cache the closure object
    if ( !imp->fpClsObj)
        imp->fpClsObj = cls;
    
    return YES;
}


LXSuccess LXShaderEvaluate1DArray4f(LXShaderRef r,
                                        LXInteger arrayCount,
                                        const float * LXRESTRICT inArray,
                                        float * LXRESTRICT outArray)
{
    if ( !r) return NO;
    if (arrayCount < 1 || !inArray || !outArray) return NO;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    if ( !createCachedClosure(imp))
        return NO;
        
    //LXFloat mul = 1.0 / arrayCount;

    int i;
    for (i = 0; i < arrayCount; i++) {    
        LXFPClosureExecute_f(imp->fpClsObj, outArray,  NULL, 0,  inArray, 1);  // no scalar inputs, 1 vector
        
        inArray += 4;
        outArray += 4;
    }
        
    return YES;
}



void LXShaderEvaluateVectorWith1SInput(LXShaderRef r, const float * LXRESTRICT inData, float * LXRESTRICT outData)
{
    if ( !r || !inData || !outData) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    if ( !createCachedClosure(imp))
        return;

    LXFPClosureExecute_f(imp->fpClsObj, outData,  inData, 1,  NULL, 0);  // one scalar input, no vectors    
}


