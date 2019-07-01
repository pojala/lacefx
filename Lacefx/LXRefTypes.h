/*
 *  LXRefTypes.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXREFTYPES_H_
#define _LXREFTYPES_H_

#include "LXBasicTypes.h"
#include "LXMap.h"  // LXPropertyType is defined here


// all these pseudo-OO LXRef types are currently defined as void * (which sucks for error checking of course -- sorry!)


// basic types
typedef LXRef LXPoolRef;
typedef LXRef LXTransform3DRef;

// graphics types
typedef LXRef LXAccumulatorRef;
typedef LXRef LXConvolverRef;
typedef LXRef LXDrawContextRef;
typedef LXRef LXPixelBufferRef;
typedef LXRef LXShaderRef;
typedef LXRef LXSurfaceRef;
typedef LXRef LXTextureRef;
typedef LXRef LXTextureArrayRef;


typedef void(*LXRefGenericUserDataFuncPtr)(LXRef, void *);


#ifdef __cplusplus
extern "C" {
#endif

// basic functions for LXRefs
LXEXPORT LXRef LXRefRetain(LXRef obj);
LXEXPORT void LXRefRelease(LXRef obj);
LXEXPORT int32_t LXRefGetRetainCount(LXRef obj);

LXEXPORT const char *LXRefGetType(LXRef objRef);
LXEXPORT LXBool LXRefIsOfType(LXRef objRef, const char *typeID);

LXEXPORT char *LXRefCopyName(LXRef obj);  // returned string must be destroyed with _lx_free()

LXEXPORT void LXRefAttachUserData(LXRef obj, void *userData);
LXEXPORT void *LXRefGetUserData(LXRef obj);
LXEXPORT void LXRefSetUserDataDestructorFunc(LXRef obj, LXRefGenericUserDataFuncPtr func);  // func will be called when the object is about to be destroyed

LXEXPORT LXSuccess LXRefGetProperty(LXRef obj, const char *propID, LXPropertyType propType, void *outPtr);
LXEXPORT LXSuccess LXRefSetProperty(LXRef obj, const char *propID, LXPropertyType propType, void *inPtr);

LXEXPORT LXFloat LXRefGetFloat(LXRef obj, const char *propID);
LXEXPORT void LXRefSetFloat(LXRef obj, const char *propID, LXFloat value);

LXEXPORT LXBool LXRefGetBool(LXRef obj, const char *propID);
LXEXPORT void LXRefSetBool(LXRef obj, const char *propID, LXBool value);


#ifdef __OBJC__
// where Objective-C is available, a wrapper for making an NSObject into an LXRef
typedef LXRef LXObjCRef;
LXEXPORT const char *LXObjCTypeID();

LXEXPORT LXObjCRef LXObjCCreateWithObject(id nsObj);
LXEXPORT id LXObjCGetObject(LXObjCRef r);
#endif


#ifdef __cplusplus
}
#endif


// callback function type for enumerating LXRefs
typedef void(*LXRefEnumerationFuncPtr)(LXRef obj, LXInteger index, void *userData);

// reference counting and copying callbacks
typedef LXRef(*LXRefRetainFuncPtr)(LXRef);
typedef void(*LXRefReleaseFuncPtr)(LXRef);
typedef LXRef(*LXRefCopyFuncPtr)(LXRef);

// other LXRef memory management callbacks
typedef LXBool(*LXRefPrivatePoolFuncPtr)(LXRef, LXPoolRef, LXUInteger);
typedef void(*LXGenericCleanupFuncPtr)(void *ctx, void *userData);

// useful common callbacks
typedef char *(*LXRefCopyNameFuncPtr)(LXRef);

typedef LXSuccess(*LXRefPropertyAccessorFuncPtr)(LXRef, const char *, LXPropertyType, void *);

typedef void(*LXRefGenericVoidFuncPtr)(LXRef ref);
typedef LXBool(*LXRefBoolGetterFuncPtr)(LXRef ref);
typedef LXInteger(*LXRefIntegerGetterFuncPtr)(LXRef ref);
typedef LXFloat(*LXRefFloatGetterFuncPtr)(LXRef ref);

// combiner function types
typedef LXFloat(*LXFloatCombinerFuncPtr)(LXFloat v1, LXFloat v2);

// utility callbacks
typedef void(*LXLogFuncPtr)(const char *str, void *userData);


#endif

