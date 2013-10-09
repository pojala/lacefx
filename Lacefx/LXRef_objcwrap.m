/*
 *  LXRef_objcwrap.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/24/10.
 *  Copyright 2010 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXRef_Impl.h"


typedef struct {
    LXREF_STRUCT_HEADER
    
    id nsObj;
} LXObjCWrapImpl;



LXObjCRef LXObjCRetain(LXObjCRef r)
{
    if ( !r) return NULL;
    LXObjCWrapImpl *imp = (LXObjCWrapImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}


void LXObjCRelease(LXObjCRef r)
{
    if ( !r) return;
    LXObjCWrapImpl *imp = (LXObjCWrapImpl *)r;
    
    LXInteger refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        [imp->nsObj release];

        _lx_free(imp);
    }
}


const char *LXObjCTypeID()
{
    static const char *s = "LXObjCWrapper";
    return s;
}

LXObjCRef LXObjCCreateWithObject(id nsObj)
{
    LXObjCWrapImpl *imp = (LXObjCWrapImpl *) _lx_calloc(sizeof(LXObjCWrapImpl), 1);

    LXREF_INIT(imp, LXObjCTypeID(), (LXRefRetainFuncPtr)LXObjCRetain, (LXRefReleaseFuncPtr)LXObjCRelease);

    imp->nsObj = [nsObj retain];
        
    return (LXObjCRef)imp;
}

id LXObjCGetObject(LXObjCRef r)
{
    if ( !r) return nil;
    LXObjCWrapImpl *imp = (LXObjCWrapImpl *)r;

    return imp->nsObj;
}

