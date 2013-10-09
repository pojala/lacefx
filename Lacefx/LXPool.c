/*
 *  LXPool.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 19.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPool.h"
#include "LXStringUtils.h"
#include "LXRef_Impl.h"
#include "LXPool_surface_priv.h"


extern LXBool LXRefTryReleaseInPrivatePool(LXRef objRef, LXUInteger hint);

extern void LXPlatformCreateLocks_();


#if (LX_BUILD_SINGLETHREADED)
 // this define allows us to build without pthread support.
 // instead of per-thread storage, a regular global stores the current arpool.
 void *g_currentAutoreleasePool = NULL;

 #define GET_CURRENT_POOLFORTHREAD          g_currentAutoreleasePool
 #define SET_CURRENT_POOLFORTHREAD(_p_)     g_currentAutoreleasePool = _p_;
 #define RESTORE_PREV_POOLFORTHREAD(_imp_)  g_currentAutoreleasePool = _imp_->prevARPool;


#elif defined(LXPLATFORM_WIN)
 #include <windows.h>
 
 DWORD g_TLSIndex_CurrentLXPool = 0;
 
 #define TLS_EXISTS                         (g_TLSIndex_CurrentLXPool != 0)
 #define TLS_CREATE                         g_TLSIndex_CurrentLXPool = TlsAlloc(); \
                                            do { if ( !g_TLSIndex_CurrentLXPool)  LXPrintf("*** Lacefx API: failed to create thread local storage (%i)\n", (int)GetCurrentThreadId); \
                                               } while(0);
 
 #define GET_CURRENT_POOLFORTHREAD          TlsGetValue(g_TLSIndex_CurrentLXPool)
 #define SET_CURRENT_POOLFORTHREAD(_p_)     TlsSetValue(g_TLSIndex_CurrentLXPool, _p_);
 #define RESTORE_PREV_POOLFORTHREAD(_imp_)  TlsSetValue(g_TLSIndex_CurrentLXPool, _imp_->prevARPool);

#else
 #include <pthread.h>

 // per-thread storage: the currently active autorelease pool
 pthread_key_t g_TLSKey_CurrentLXPool = 0;

 #define TLS_EXISTS                         (g_TLSKey_CurrentLXPool != 0)
 #define TLS_CREATE                         pthread_key_create(&g_TLSKey_CurrentLXPool, LXPoolRelease_TLSDestructor);

 #define GET_CURRENT_POOLFORTHREAD          pthread_getspecific(g_TLSKey_CurrentLXPool)
 #define SET_CURRENT_POOLFORTHREAD(_p_)     pthread_setspecific(g_TLSKey_CurrentLXPool, _p_);
 #define RESTORE_PREV_POOLFORTHREAD(_imp_)  pthread_setspecific(g_TLSKey_CurrentLXPool, _imp_->prevARPool);

#endif



typedef struct _LXListNode {
    LXRef obj;
    LXUInteger flags;
    struct _LXListNode *next;
} LXListNode;


typedef struct {
    LXREF_STRUCT_HEADER
    
    LXInteger count;
    LXListNode *first;
    
    void *prevARPool;
    LXBool doDebug;
    
    // associated cleanup data (can be used to e.g. keep an NSAutoreleasePool that's purged at the same time)
    LXGenericCleanupFuncPtr assocPurgeFunc;
    LXGenericCleanupFuncPtr assocDestroyFunc;
    void *assocObj;
    
    // surface pool callbacks
    LXSurfacePoolCallbacks surfacePoolCallbacks;
    void *surfacePoolCbData;
} LXPoolImpl;



static void LXListInsertAfter(LXListNode *node, LXRef obj) {
    LXListNode *newNode = (LXListNode *)_lx_malloc(sizeof(LXListNode));
    newNode->obj = obj;
    newNode->next = node->next;
    node->next = newNode;
}


static LXListNode *LXListLast(LXListNode *node) {
    while (node->next != NULL) {
        node = node->next;
    }
    return node;
}


void LXPoolPurge(LXPoolRef poolRef)
{
    if ( !poolRef)  
        return;

    LXPoolImpl *imp = (LXPoolImpl *)poolRef;
    
    ///printf("%s (%p): count is %i, assoc cleanup func is %p\n", __func__, imp, imp->count, imp->assocPurgeFunc);

    if (imp->assocPurgeFunc)
        imp->assocPurgeFunc(poolRef, imp->assocObj);
    
    if (imp->count < 1 || !(imp->first))
        return;

    
    LXListNode *node = imp->first;
    
    LXInteger n = 0;
    while (node) {
        LXRef obj = (LXRef)(node->obj);
        
        if (obj) {
            if (node->flags & kLXPoolHint_ObjectIsMalloced) {
                _lx_free(obj);
            }
            else {
                ///LXDEBUGLOG("..pool %p: releasing obj %p (%i / %i;  next %p;  retc %i)", imp, obj, n, imp->count, node->next, LXRefGetRetainCount(obj));
                
                LXBool wasHandled = LXRefTryReleaseInPrivatePool(obj, 0);  // last argument is the autorelease hint, which isn't currently used
            
                if ( !wasHandled) LXRefRelease(obj);
            }
            //node->obj = NULL;
        }
        else {
            LXWARN("empty node in LXPool (%p; %i / %i)", imp, (int)n, (int)imp->count);
        }
            
        LXListNode *nd = node;
            
        node = node->next;
        
        _lx_free(nd);
        n++;
    }
    
    imp->count = 0;
    imp->first = NULL;    
}



LXPoolRef LXPoolRetain(LXPoolRef poolRef)
{
    if ( !poolRef) return NULL;
    LXPoolImpl *imp = (LXPoolImpl *)poolRef;

    LXAtomicInc_int32(&(imp->retCount));
    return poolRef;
}


void LXPoolRelease(LXPoolRef poolRef)
{
    if ( !poolRef) return;
    LXPoolImpl *imp = (LXPoolImpl *)poolRef;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)poolRef);

        if (imp->doDebug) LXDEBUGLOG("LXPoolRelease: pool %p, count %i", poolRef, (int)imp->count);

        LXPoolPurge(poolRef);
        
        if (GET_CURRENT_POOLFORTHREAD == poolRef) {
            if (imp->doDebug) LXDEBUGLOG("  ...resetting prev autorelease pool: %p", imp->prevARPool);
            
            ///pthread_setspecific(g_TLSKey_CurrentLXPool, imp->prevARPool);
            RESTORE_PREV_POOLFORTHREAD(imp);
        }

        if (imp->assocDestroyFunc)
            imp->assocDestroyFunc(poolRef, imp->assocObj);

        _lx_free(imp);
    }
}


const char *LXPoolTypeID()
{
    static const char *s = "LXPool";
    return s;
}

LXPoolRef LXPoolCreate()
{
    LXPoolImpl *pool = (LXPoolImpl *)_lx_calloc(sizeof(LXPoolImpl), 1);

    LXREF_INIT(pool, LXPoolTypeID(), LXPoolRetain, LXPoolRelease);

    return (LXPoolRef)pool;
}


void LXPoolRelease_TLSDestructor(void *ref)
{
    LXPoolImpl *imp = (LXPoolImpl *)ref;
    
    ///printf("%s: thread %lu; pool is %p, prevpool is %p\n", __func__, (unsigned long)pthread_self(), ref, imp->prevARPool);
    
    // reset previous autorelease pool as active
    ///pthread_setspecific(g_TLSKey_CurrentLXPool, imp->prevARPool);
    RESTORE_PREV_POOLFORTHREAD(imp);
    
    LXPoolRelease(ref);
}


LXPoolRef LXPoolCreateForThreadWithAssociatedCleanUpCallbacks_(LXGenericCleanupFuncPtr purgeFunc, LXGenericCleanupFuncPtr destroyFunc, void *cleanUpObj)
{
    LXPlatformCreateLocks_();  // to ensure that the platform locks get created as early as possible, do it here

    LXPoolRef pool = LXPoolCreate();
    LXPoolImpl *imp = (LXPoolImpl *)pool;

    imp->assocPurgeFunc = purgeFunc;
    imp->assocDestroyFunc = destroyFunc;
    imp->assocObj = cleanUpObj;

#if ( !LX_BUILD_SINGLETHREADED)
    if ( !TLS_EXISTS) {
        TLS_CREATE;
    } else
#endif    
    {
        // get the current autorelease pool, so we can reset it when we are destroyed
        // (this is effectively a stack of nested pools)
        imp->prevARPool = GET_CURRENT_POOLFORTHREAD; //pthread_getspecific(g_TLSKey_CurrentLXPool);
    }
    
    SET_CURRENT_POOLFORTHREAD(pool);
    ///pthread_setspecific(g_TLSKey_CurrentLXPool, pool);

    ///printf("-- thread %lu; created pool %p, prevpool is %p\n", (unsigned long)pthread_self(), imp, imp->prevARPool);    
    
    return pool;
}

LXPoolRef LXPoolCreateForThread()
{
    return LXPoolCreateForThreadWithAssociatedCleanUpCallbacks_(NULL, NULL, NULL);
}


LXPoolRef LXPoolCurrentForThread()
{
    return GET_CURRENT_POOLFORTHREAD; //return pthread_getspecific(g_TLSKey_CurrentLXPool);
}


LXUInteger LXPoolCount(LXPoolRef poolRef)
{
    if ( !poolRef)
        return 0;

    LXPoolImpl *pool = (LXPoolImpl *)poolRef;
    return pool->count;
}

void _LXPoolAutoreleaseWarnAboutNoPool(LXRef obj, LXUInteger releaseHint)
{
    LXBool objIsPlainC = (releaseHint & kLXPoolHint_ObjectIsMalloced) ? YES : NO;
    const char *objType = "<not an LXType>";
    if ( !objIsPlainC)
        objType = LXRefGetType(obj);        
    LXWARN("attempt to autorelease without a pool, will leak (%p -- type '%s'; break on %s to debug)", obj, objType, __func__);
}

LXRef LXPoolAutoreleaseWithHint(LXPoolRef poolRef, LXRef obj, LXUInteger releaseHint)
{
    if ( !obj)
        return NULL;
        
    LXBool objIsPlainC = (releaseHint & kLXPoolHint_ObjectIsMalloced) ? YES : NO;
        
    if ( !poolRef) {
        _LXPoolAutoreleaseWarnAboutNoPool(obj, releaseHint);
        return obj;
    }

    LXPoolImpl *pool = (LXPoolImpl *)poolRef;
    
    if (pool->doDebug) {
        if (objIsPlainC) {
            LXDEBUGLOG("LXPool %p: adding malloced ptr %p", pool, obj);
        } else {
            //const char *objType = LXRefGetType(obj);
            //LXDEBUGLOG("LXPool %p: adding object %p (type %s)", pool, obj,  (objType) ? objType : "");
        }
    }
    
    // add object to pool's linked list
    LXListNode *prevNode = pool->first;    
    pool->first = (LXListNode *)_lx_malloc(sizeof(LXListNode));
    pool->first->obj = obj;
    pool->first->next = prevNode;
    pool->first->flags = releaseHint;
    
    (pool->count)++;
    
    return obj;
}


LXRef LXPoolAutorelease(LXPoolRef pool, LXRef obj)
{
    if ( !pool)
        pool = LXPoolCurrentForThread();
    return LXPoolAutoreleaseWithHint(pool, obj, kLXPoolHint_TemporaryObject);
}


void LXPoolEnableDebug(LXPoolRef ref)
{
    if ( !ref) return;
    LXPoolImpl *imp = (LXPoolImpl *)ref;
    
    imp->doDebug = YES;
}



void LXPoolSetSurfacePoolCallbacks_(LXPoolRef pool, LXSurfacePoolCallbacks *callbacks, void *userData)
{
    if ( !pool) return;
    LXPoolImpl *imp = (LXPoolImpl *)pool;
    
    if (callbacks) {
        memcpy(&(imp->surfacePoolCallbacks), callbacks, sizeof(imp->surfacePoolCallbacks));
        imp->surfacePoolCbData = userData;
    } else {
        memset(&(imp->surfacePoolCallbacks), 0, sizeof(imp->surfacePoolCallbacks));
        imp->surfacePoolCbData = NULL;
    }
}

LXSurfaceRef LXPoolGetSurface_(LXPoolRef pool, uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint32_t flags)
{
    if ( !pool) return NULL;
    LXPoolImpl *imp = (LXPoolImpl *)pool;

    if (imp->surfacePoolCallbacks.getSurface) {
        return imp->surfacePoolCallbacks.getSurface(pool, w, h, pxFormat, flags, imp->surfacePoolCbData);
    } else
        return NULL;
}

void LXPoolReturnSurface_(LXPoolRef pool, LXSurfaceRef surface)
{
    if ( !pool) return;
    LXPoolImpl *imp = (LXPoolImpl *)pool;

    if (imp->surfacePoolCallbacks.returnSurface) {
        imp->surfacePoolCallbacks.returnSurface(pool, surface, imp->surfacePoolCbData);
    }
}

