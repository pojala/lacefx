/*
 *  LXRef_Impl.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXREF_IMPL_H_
#define _LXREF_IMPL_H_

#include "LXBasicTypeFunctions.h"
#include "LXMutexAtomic.h"
#include "LXStringUtils.h"


#define LXREF_COOKIE    0xbeba2607


#define LXREF_STRUCT_HEADER \
    uint32_t cookie;                 \
    volatile int32_t retCount;       \
    const char *typeID;              \
    \
    LXRefRetainFuncPtr retainFunc;          \
    LXRefReleaseFuncPtr releaseFunc;        \
    LXRefPrivatePoolFuncPtr privPoolFunc;   \
    LXRefCopyFuncPtr copyFunc;  \
    void *propertyMap;  \
    LXRefPropertyAccessorFuncPtr setPropertyFunc;  \
    LXRefPropertyAccessorFuncPtr getPropertyFunc;  \
    char *name; \
    LXRefCopyNameFuncPtr copyNameFunc;  \
    void *pool;  \
    void *userData;  \
    LXRefGenericUserDataFuncPtr userDataDestructorFunc; \
    void * _res[8];
    // a total of 20 pointers



#pragma pack(push, 4)

typedef struct _LXRefAbstract {
    LXREF_STRUCT_HEADER    
} LXRefAbstract;

#pragma pack(pop)


#define LXREF_INIT(_obj, _typeName, _retnFunc, _releFunc) \
    _obj->cookie = LXREF_COOKIE;                                \
    _obj->typeID = _typeName;                                   \
    _obj->retainFunc = (LXRefRetainFuncPtr)_retnFunc;           \
    _obj->releaseFunc = (LXRefReleaseFuncPtr)_releFunc;         \
    _obj->retCount = 1;


#ifdef __cplusplus
extern "C" {
#endif

// all release implementations should call this before they free their implementation object
// (this function will call the userDataDestructor)
LXEXPORT void LXRefWillDestroyItself(LXRef objRef);

#ifdef __cplusplus
}
#endif


#define LXREF_INITPROPERTYMAP(_obj) \
    _obj->propertyMap = LXMapCreateMutable();
    
#define LXREF_FREEPROPERTYMAP(_obj) \
    LXMapDestroy(_obj->propertyMap);


// --- utilities for writing accessors and other common methods ---

#define LX_IVAR_ASSIGN_RET(_impType_, _obj_, _ivarName_, _newValue_) {  \
            _impType_ *imp = (_impType_ *)_obj_; \
            if (_ivarName_) LXRefRelease(_ivarName_); \
            _ivarName_ = (_newValue_ != NULL) ? LXRefRetain(_newValue_) : NULL;  }


#endif
