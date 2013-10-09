/*
 *  LXTextureArray.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXTextureArray.h"
#include "LXRef_Impl.h"
#include "LXTexture.h"

#include "stdarg.h"


typedef struct {
    LXREF_STRUCT_HEADER
    
    LXInteger count;
    LXTextureRef textures[8];
    
    LXUInteger samplings[8];
    LXUInteger wrapModes[8];
} LXTexArrayImpl;



void LXTextureArrayRemoveAll(LXTextureArrayRef r)
{
    if ( !r) return;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)r;

    LXInteger i;
    for (i = 0; i < 8; i++) {
        LXTextureRef tex = imp->textures[i];
        if (tex) {
            LXTextureRelease(tex);
        }
        imp->textures[i] = NULL;
    }
    
    imp->count = 0;
}


void LXTextureArrayForEach(LXTextureArrayRef r, LXRefEnumerationFuncPtr enumFunc, void *userData)
{
    if ( !r || !enumFunc) return;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)r;

    LXInteger i;
    for (i = 0; i < 8; i++) {
        LXTextureRef tex = imp->textures[i];
        if (tex)
            enumFunc(tex, i, userData);
    }    
}


LXTextureArrayRef LXTextureArrayRetain(LXTextureArrayRef r)
{
    if ( !r) return NULL;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}


void LXTextureArrayRelease(LXTextureArrayRef r)
{
    if ( !r) return;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)r;
    
    LXInteger refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        ///LXPrintf("releasing texarr %p; count %i\n", r, imp->count);
        LXTextureArrayRemoveAll(r);
        _lx_free(imp);
    }
}


const char *LXTextureArrayTypeID()
{
    static const char *s = "LXTextureArray";
    return s;
}

LXTextureArrayRef LXTextureArrayCreate()
{
    LXTexArrayImpl *imp = (LXTexArrayImpl *) _lx_calloc(sizeof(LXTexArrayImpl), 1);

    LXREF_INIT(imp, LXTextureArrayTypeID(), (LXRefRetainFuncPtr)LXTextureArrayRetain, (LXRefReleaseFuncPtr)LXTextureArrayRelease);

    LXInteger i;
    for (i = 0; i < 8; i++) {
        imp->samplings[i] = kLXNotFound;
        imp->wrapModes[i] = 0;
    }
        
    return (LXTextureArrayRef)imp;
}


#pragma mark --- public API methods ---

LXTextureArrayRef LXTextureArrayCreateWithTexture(LXTextureRef texture)
{
    LXTextureArrayRef texArray = LXTextureArrayCreate();
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;
    
    if (imp && texture) {
        imp->textures[0] = LXTextureRetain(texture);
        imp->count = 1;
    }
    
    return texArray;
}

LXTextureArrayRef LXTextureArrayCreateWithTextures(LXInteger optionalCount, ...)
{
    LXTextureArrayRef texArray = LXTextureArrayCreate();
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;
    
    if (imp) {
        va_list args;
        va_start(args, optionalCount);
        
        LXTextureRef tex;
        LXInteger n = 0;
        LXInteger max = (optionalCount > 0 && optionalCount < 8) ? optionalCount : 8;
        
        if (optionalCount > 0) {
            while (n < max) {
                tex = va_arg(args, LXTextureRef);
                imp->textures[n++] = LXTextureRetain(tex);
            }
        } else {
            while (n < max && (tex = va_arg(args, LXTextureRef))) {
                imp->textures[n++] = LXTextureRetain(tex);
            }
        }
        
        va_end(args);
        
        imp->count = n;
        ///LXPrintf("created texture array with varargs (texcount %i; arg %i; max %i)\n", imp->count, optionalCount, max);
    }
    
    return texArray;
}

LXTextureArrayRef LXTextureArrayCreateWithTexturesAndCount(LXTextureRef *textures, LXInteger count)
{
    LXTextureArrayRef texArray = LXTextureArrayCreate();
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;
    
    if (imp) {
        if (count > 8) count = 8;
        LXInteger i;
        LXInteger n = 0;
        for (i = 0; i < count; i++) {
            imp->textures[i] = LXTextureRetain(textures[i]);
            if (imp->textures[i] != NULL) {
                n = i + 1;
            }
        }
        imp->count = n;
    }
    
    return texArray;
}


LXInteger LXTextureArraySlotCount(LXTextureArrayRef texArray)
{
    return 8;
}

LXInteger LXTextureArrayInUseCount(LXTextureArrayRef texArray)
{
    if ( !texArray) return 0;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;
    
    return imp->count;
    
}

LXTextureRef LXTextureArrayAt(LXTextureArrayRef texArray, LXInteger index)
{
    if ( !texArray) return NULL;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;
    
    if (index > 7 || index < 0) {
        LXPrintf("** %s: index out of bounds (%i)\n", __func__, (int)index);
        return NULL;
    }
    
    return imp->textures[index];
}

LXUInteger LXTextureArrayGetSamplingAt(LXTextureArrayRef texArray, LXInteger index)
{
    if ( !texArray) return kLXNotFound;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;

    if (index > 7 || index < 0) {
        LXPrintf("** %s: index out of bounds (%i)\n", __func__, (int)index);
        return kLXNotFound;
    }
    return imp->samplings[index];
}

void LXTextureArraySetSamplingAt(LXTextureArrayRef texArray, LXInteger index, LXUInteger samplingEnum)
{
    if ( !texArray) return;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;

    if (index > 7 || index < 0) {
        LXPrintf("** %s: index out of bounds (%i)\n", __func__, (int)index);
        return;
    }
    imp->samplings[index] = samplingEnum;
}

LXUInteger LXTextureArrayGetWrapModeAt(LXTextureArrayRef texArray, LXInteger index)
{
    if ( !texArray) return 0;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;

    if (index > 7 || index < 0) {
        LXPrintf("** %s: index out of bounds (%i)\n", __func__, (int)index);
        return 0;
    }
    return imp->wrapModes[index];
}

void LXTextureArraySetWrapModeAt(LXTextureArrayRef texArray, LXInteger index, LXUInteger wrapEnum)
{
    if ( !texArray) return;
    LXTexArrayImpl *imp = (LXTexArrayImpl *)texArray;

    if (index > 7 || index < 0) {
        LXPrintf("** %s: index out of bounds (%i)\n", __func__, (int)index);
        return;
    }
    imp->wrapModes[index] = wrapEnum;
}

