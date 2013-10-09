/*
 *  LQGLContext.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.4.2008.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import <Foundation/Foundation.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/OpenGL.h>
#import "LXRefTypes.h"


#ifndef __nsfunc__
 // useful as the caller argument to the -activateWithTimeout:caller: method
 #define __nsfunc__ [NSString stringWithUTF8String:__func__]
#endif


@interface LQGLContext : NSObject {

    NSString                    *_name;

    CGLContextObj               _ctx;
    
    NSRecursiveLock             *_lock;
    //NSString                    *_lockHolderName;
    NSMutableArray              *_lockHolderNameStack;
    NSMutableArray              *_lockHoldRefTimeStack;

    NSLock                      *_callDepthLock;
    long                        _callDepth;

    CGLContextObj               _prevCtx;
    
    // deferred delete / update of GL objects
    NSLock                      *_deferLock;
    
    NSMutableSet                *_progKillList;
    NSMutableSet                *_textureKillList;
    NSMutableSet                *_fboKillList;
    NSMutableSet                *_resourceKillList;
    
    NSMutableArray              *_deferredUpdates;
}

- (id)initWithCGLContext:(CGLContextObj)ctx name:(NSString *)name;

- (void)setCGLContext:(CGLContextObj)ctx;
- (CGLContextObj)CGLContext;

// these methods will serialise access to the context through lock/unlock
- (BOOL)activate;
- (BOOL)activateWithTimeout:(double)timeoutSecs caller:(NSString *)callerName;
- (void)deactivate;

// returns -1 on lock fail; call depth value otherwise
- (long)lockContextWithTimeout:(double)timeoutSecs caller:(NSString *)callerName errorInfo:(NSDictionary **)errInfo;
- (void)unlockContext;
- (NSRecursiveLock *)contextLock;

- (BOOL)isActive;

- (void)deferDeleteOfGLTextureID:(GLuint)glid;
- (void)deferDeleteOfARBProgramID:(GLuint)glid;
- (void)deferDeleteOfGLFrameBufferID:(GLuint)glid;
- (void)deferReleaseOfGLResource:(id)obj;

- (void)deferUpdateOfLXRef:(LXRef)object callback:(LXRefGenericUserDataFuncPtr)callback userData:(void *)data isMalloced:(BOOL)userDataIsMalloced;
- (void)cancelDeferredUpdatesOfLXRef:(LXRef)object;

- (void)performCleanup;

@end
