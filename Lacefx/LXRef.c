/*
 *  LXRef.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXREF_C_
#define _LXREF_C_

#include "LXRefTypes.h"
#include "LXRef_Impl.h"


LXBool LXRefIsOfType(LXRef objRef, const char *typeID)
{
    if ( !objRef || !typeID) return NO;
    
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return NO;
                
    return (0 == strcmp(obj->typeID, typeID)) ? YES : NO;
}

const char *LXRefGetType(LXRef objRef)
{
    if ( !objRef) return NULL;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return NULL;

    return obj->typeID;
}

char *LXRefCopyName(LXRef objRef)
{
    if ( !objRef) return NULL;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return NULL;

    if (obj->copyNameFunc) {
        return obj->copyNameFunc(objRef);
    } else {
        size_t len = (obj->name) ? strlen(obj->name) : 0;
        if ( !len) return NULL;
    
        char *buffer = _lx_malloc(len + 1);
        memcpy(buffer, obj->name, len + 1);
        return buffer;
    }
}

void LXRefAttachUserData(LXRef objRef, void *userData)
{
    if ( !objRef) return;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return;

    obj->userData = userData;
}

void *LXRefGetUserData(LXRef objRef)
{
    if ( !objRef) return NULL;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return NULL;

    return obj->userData;
}

void LXRefSetUserDataDestructorFunc(LXRef objRef, LXRefGenericUserDataFuncPtr func)
{
    if ( !objRef) return;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->cookie != LXREF_COOKIE) return;

    obj->userDataDestructorFunc = func;
}


int32_t LXRefGetRetainCount(LXRef objRef)
{
    if ( !objRef) return 0;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;
    if (obj->cookie != LXREF_COOKIE) return 0;

    return obj->retCount;
}


LXRef LXRefRetain(LXRef objRef)
{
    if ( !objRef) return NULL;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;
    if (obj->cookie != LXREF_COOKIE) return NULL;

    if (obj->retainFunc) {
        obj->retainFunc(objRef);
        return objRef;
    } else
        return NULL;
}

void LXRefRelease(LXRef objRef)
{
    if ( !objRef) return;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;    
    if (obj->cookie != LXREF_COOKIE) return;

    if (obj->releaseFunc)
        obj->releaseFunc(objRef);
}

void LXRefWillDestroyItself(LXRef objRef)
{
    if ( !objRef) return;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;

    if (obj->userDataDestructorFunc)
        obj->userDataDestructorFunc(objRef, obj->userData);
        
    obj->userData = NULL;
    obj->userDataDestructorFunc = NULL;
}

LXBool LXRefTryReleaseInPrivatePool(LXRef objRef, int hint)
{
    if ( !objRef) return NO;
    LXRefAbstract *obj = (LXRefAbstract *)objRef;
    if (obj->cookie != LXREF_COOKIE) return NO;

    if (obj->privPoolFunc)
        return obj->privPoolFunc(objRef, obj->pool, hint);
    else
        return NO;
}


LXSuccess LXRefGetProperty(LXRef objRef, const char *propID, LXPropertyType propType, void *outPtr)
{
    if ( !objRef || !propID || !outPtr) return NO;

    LXRefAbstract *obj = (LXRefAbstract *)objRef;
    if (obj->cookie != LXREF_COOKIE) return NO;
    
    if (obj->getPropertyFunc)
        return obj->getPropertyFunc(objRef, propID, propType, outPtr);
    else
        return NO;
}

LXSuccess LXRefSetProperty(LXRef objRef, const char *propID, LXPropertyType propType, void *inPtr)
{
    if ( !objRef || !propID || !inPtr) return NO;

    LXRefAbstract *obj = (LXRefAbstract *)objRef;
    if (obj->cookie != LXREF_COOKIE) return NO;
    
    if (obj->setPropertyFunc)
        return obj->setPropertyFunc(objRef, propID, propType, inPtr);
    else
        return NO;
}

// some convenience methods for setting/getting simple properties

LXFloat LXRefGetFloat(LXRef obj, const char *propID)
{
    LXFloat f;
    if ( !LXRefGetProperty(obj, propID, kLXFloatProperty, &f))
        f = 0.0;
    return f;
}

void LXRefSetFloat(LXRef obj, const char *propID, LXFloat value)
{
    LXRefSetProperty(obj, propID, kLXFloatProperty, &value);
}

LXBool LXRefGetBool(LXRef obj, const char *propID)
{
    LXBool b;
    if ( !LXRefGetProperty(obj, propID, kLXBoolProperty, &b))
        b = NO;
    return b;
}

void LXRefSetBool(LXRef obj, const char *propID, LXBool value)
{
    LXRefSetProperty(obj, propID, kLXBoolProperty, &value);
}




#if defined(__GCC__) && 0
                            //&& !defined(RELEASE)
#pragma mark --- inline function implementations ---

/*
if gcc is on -O0, it doesn't inline, so we need to have these somewhere
*/

LXPoint LXMakePoint(LXFloat x, LXFloat y) {
    LXPoint p = { x, y };
    return p;
}

LXSize LXMakeSize(LXFloat w, LXFloat h) {
    LXSize s = { w, h };
    return s;
}

LXRect LXMakeRect(LXFloat x, LXFloat y, LXFloat w, LXFloat h) {
    LXRect r = { x, y, w, h };
    return r;
}

#endif


#endif
