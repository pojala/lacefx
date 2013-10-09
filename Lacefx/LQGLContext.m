/*
 *  LQGLContext.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.4.2008.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LQGLContext.h"
#import "LXBasicTypes.h"


#define DTIME(t_)  double t_ = CFAbsoluteTimeGetCurrent();


@implementation LQGLContext

- (id)initWithCGLContext:(CGLContextObj)ctx name:(NSString *)name
{
    self = [super init];
    
    _name = [name copy];
    
    _ctx = ctx;
    _lock = [[NSRecursiveLock alloc] init];
    _lockHolderNameStack = [[NSMutableArray arrayWithCapacity:32] retain];
    _lockHoldRefTimeStack = [[NSMutableArray arrayWithCapacity:32] retain];

    _callDepthLock = [[NSLock alloc] init];

    _textureKillList =  [[NSMutableSet setWithCapacity:32] retain];
    _progKillList =     [[NSMutableSet setWithCapacity:32] retain];
    _fboKillList =      [[NSMutableSet setWithCapacity:32] retain];
    _resourceKillList = [[NSMutableSet setWithCapacity:32] retain];
    _deferredUpdates =  [[NSMutableArray arrayWithCapacity:100] retain];
    _deferLock =        [[NSLock alloc] init];
    
    return self;
}

- (void)dealloc {
    [self performCleanup];
    
    [_textureKillList release];
    [_progKillList release];
    [_fboKillList release];
    [_resourceKillList release];
    [_deferredUpdates release];
    [_deferLock release];
    
    [_lock release];
    [_lockHolderNameStack release];
    [_lockHoldRefTimeStack release];

    [_callDepthLock release];
        
    _ctx = NULL;
    [_name release];
    
    [super dealloc];
}


- (void)setCGLContext:(CGLContextObj)ctx {
    _ctx = ctx;
}

- (CGLContextObj)CGLContext {
    return _ctx;
}


- (void)_reallyCleanUpKillList
{
    [_deferLock lock];

    id v;   
    NSEnumerator *killListEnum = [_textureKillList objectEnumerator];
    while (v = [killListEnum nextObject]) {
        GLuint texid = [v longValue];
        glDeleteTextures(1, &texid);
        ///LXDEBUGLOG("  context %p: deleted texid %i", self, texid);
    }
    [_textureKillList removeAllObjects];

    killListEnum = [_progKillList objectEnumerator];
    while (v = [killListEnum nextObject]) {
        GLuint progid = [v longValue];
        glDeleteProgramsARB(1, &progid);        
        ///LXDEBUGLOG("%s: context %p: deleted program %i", __func__, self, progid);
    }
    [_progKillList removeAllObjects];

    killListEnum = [_fboKillList objectEnumerator];
    while (v = [killListEnum nextObject]) {
        GLuint fbo = [v longValue];
        glDeleteFramebuffersEXT(1, &fbo);
        ///LXDEBUGLOG("%s: context %p: deleted fbo %i", __func__, self, progid);
    }
    [_progKillList removeAllObjects];


    ///if ([gResourceKillList count] > 0) NSLog(@"removing %i deferred gl objs", [gResourceKillList count]);
    
    // release deferred Obj-C objects (probably pbuffers or other GL resource wrappers)
    [_resourceKillList removeAllObjects];
    
    [_deferLock unlock];
}

- (void)_reallyPerformDeferredUpdates
{
    [_deferLock lock];

    for (id dict in _deferredUpdates) {
        LXRef obj = [[dict objectForKey:@"lxRef"] pointerValue];
        LXRefGenericUserDataFuncPtr callback = [[dict objectForKey:@"callbackFuncPtr"] pointerValue];
        void *userData = [[dict objectForKey:@"userData"] pointerValue];
        
        if (callback) {
            callback(obj, userData);
        }
        
        if ([[dict objectForKey:@"userDataIsMalloced"] boolValue]) {
            if (userData) _lx_free(userData);
        }
    }
    
    ///if ([_deferredUpdates count]) NSLog(@"... performed %i deferred updates", [_deferredUpdates count]);
    
    [_deferredUpdates removeAllObjects];

    [_deferLock unlock];
}



#define DEFAULTTIMEOUT 1.5

- (BOOL)activate
{
    return [self activateWithTimeout:DEFAULTTIMEOUT caller:@"glCtx_activate"];
}

- (NSRecursiveLock *)contextLock
{
    return _lock;
}

- (long)lockContextWithTimeout:(double)timeout caller:(NSString *)callerName errorInfo:(NSDictionary **)errInfo
{
    if ([_lock lockBeforeDate:[NSDate dateWithTimeIntervalSinceNow:timeout]]) {    
        DTIME(tLock)

        [_callDepthLock lock];
        DTIME(tLock2)
        
        [_lockHolderNameStack addObject:[[callerName copy] autorelease]];
        [_lockHoldRefTimeStack addObject:[NSNumber numberWithDouble:tLock]];
    
        long callDepth = _callDepth;
    
        [_callDepthLock unlock];    

        if (tLock2-tLock > (2.0/1000.0)) {
            printf("long time spent on call depth lock: %.2f ms (caller '%s', lock acquired)\n", 1000*(tLock2-tLock), [callerName UTF8String]);
        }
            
        return callDepth;
    }
    else {
        DTIME(tLock)
        if (errInfo) {
            [_callDepthLock lock];
            DTIME(tLock2)
            
            NSString *lockHolderName = [_lockHolderNameStack lastObject];
            id lockHolderStack = [[_lockHolderNameStack copy] autorelease];
            id lockRefTimeStack = [[_lockHoldRefTimeStack copy] autorelease];
            [_callDepthLock unlock];
            
            if (tLock2-tLock > (2.0/1000.0)) {
                printf("long time spent on call depth lock: %.2f ms (caller '%s', lock not acquired)\n", 1000*(tLock2-tLock), [callerName UTF8String]);
            }
        
            *errInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                                        (lockHolderName) ? lockHolderName : @"unknown",  @"lockHolderName",
                                        lockHolderStack, @"lockHolderStack",
                                        lockRefTimeStack, @"lockRefTimeStack",
                                        nil];
        }
        return -1;
    }
}

- (void)unlockContext
{
    [_callDepthLock lock];
    [_lockHolderNameStack removeLastObject];
    [_lockHoldRefTimeStack removeLastObject];
    [_callDepthLock unlock];    
    
    [_lock unlock];
}

- (BOOL)activateWithTimeout:(double)timeoutSec caller:(NSString *)callerName
{
    if ( !_ctx) {
        NSLog(@"** %s (%@): no context (caller %@, t.o. %f)", __func__, _name, callerName, timeoutSec);
        return NO;
    }

    BOOL didLock = [_lock lockBeforeDate:[NSDate dateWithTimeIntervalSinceNow:timeoutSec]];
        
    if (!didLock) {        
        return NO;
    }
    DTIME(tLock)
    
    [_callDepthLock lock];
    _callDepth++;
    long callDepth = _callDepth;
    
    [_lockHolderNameStack addObject:[[callerName copy] autorelease]];
    [_lockHoldRefTimeStack addObject:[NSNumber numberWithDouble:tLock]];
        
    [_callDepthLock unlock];

  #if (DEBUG_CGLLOCK)        
    printf("--cgl lock: timeout %.4f, caller %s, depth %ld\n", (timeoutSec < 20.0 ? timeoutSec : -1.0), [callerName cString], callDepth);
  #endif
    
  #if (DEBUG)
    t = (CFAbsoluteTimeGetCurrent() - t) * 1000.0;  // time in ms
    if (t > 50.0) {
        NSLog(@"long time waiting for sharedCGLContext lock: %.4f ms", t);
    }
  #endif
    
    if (callDepth == 1) {
        _prevCtx = CGLGetCurrentContext();
        CGLSetCurrentContext(_ctx);
    
        [self _reallyCleanUpKillList];
    }
    
    [self _reallyPerformDeferredUpdates];
    
    return YES;
}

- (void)deactivate
{
    [_callDepthLock lock];

  #if (DEBUG_CGLLOCK)
    printf("--cgl unlock: timeout %.4f, caller %s, depth %ld\n", [[_lockHolderNameStack lastObject] cString], _callDepth);
  #endif
    
    if (_callDepth == 0) {
        NSLog(@"** warning: %s called without previous activate, this is probably an error (%@)", __func__, _name);
        
        ///int *blowup = NULL;
        ///int a = *blowup;
        ///printf("%i", a);
        
        [_callDepthLock unlock];
        return;
    }

    _callDepth--;
    long callDepth = _callDepth;
    
    [_lockHolderNameStack removeLastObject];
    [_lockHoldRefTimeStack removeLastObject];
    
    [_callDepthLock unlock];
    
    if (callDepth == 0) {
        [self _reallyCleanUpKillList];
        
        CGLSetCurrentContext(_prevCtx);
        _prevCtx = NULL;
    }

    [_lock unlock];
}


- (BOOL)isActive
{
    BOOL f;
    [_callDepthLock lock];
    f = (_callDepth > 0) ? YES : NO;
    [_callDepthLock unlock];
    return f;
}

- (void)deferDeleteOfGLTextureID:(GLuint)glid
{
    if (glid > 0) {
        [_deferLock lock];
        [_textureKillList addObject:[NSNumber numberWithLong:glid]];
        [_deferLock unlock];
    }
}

- (void)deferDeleteOfARBProgramID:(GLuint)glid
{
    if (glid > 0) {
        [_deferLock lock];
        [_progKillList addObject:[NSNumber numberWithLong:glid]];
        [_deferLock unlock];
    }
}

- (void)deferDeleteOfGLFrameBufferID:(GLuint)glid
{
    if (glid > 0) {
        [_deferLock lock];
        [_fboKillList addObject:[NSNumber numberWithLong:glid]];
        [_deferLock unlock];
    }
}

- (void)deferReleaseOfGLResource:(id)obj
{
    if (obj) {
        ///NSLog(@"deferring %@ / retc %i", [obj description], [obj retainCount]);
        [_deferLock lock];
        [_resourceKillList addObject:obj];
        [_deferLock unlock];
    }
}

- (void)performCleanup
{
    if ([self activateWithTimeout:2.1212 caller:__nsfunc__])  // _reallyCleanUp... is called inside activateWith...
        [self deactivate];
    else
        NSLog(@"** %s failed", __func__);
}


- (void)deferUpdateOfLXRef:(LXRef)object callback:(LXRefGenericUserDataFuncPtr)callback userData:(void *)data isMalloced:(BOOL)userDataIsMalloced
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    [dict setObject:[NSValue valueWithPointer:object] forKey:@"lxRef"];
    [dict setObject:[NSValue valueWithPointer:callback] forKey:@"callbackFuncPtr"];
    [dict setObject:[NSValue valueWithPointer:data] forKey:@"userData"];
    [dict setObject:[NSNumber numberWithBool:userDataIsMalloced] forKey:@"userDataIsMalloced"];

    [_deferLock lock];
    [_deferredUpdates addObject:dict];
    [_deferLock unlock];
}

- (void)cancelDeferredUpdatesOfLXRef:(LXRef)object
{
    [_deferLock lock];
    
    NSMutableArray *killList = nil;
    
    for (id dict in _deferredUpdates) {
        if ([[dict objectForKey:@"lxRef"] pointerValue] == object) {
            if ( !killList) killList = [NSMutableArray array];
            [killList addObject:dict];
            
            if ([[dict objectForKey:@"userDataIsMalloced"] boolValue]) {
                void *userData = [[dict objectForKey:@"userData"] pointerValue];
                if (userData) _lx_free(userData);
            }
            break;
        }
    }
    
    if ([killList count] > 0) [_deferredUpdates removeObjectsInArray:killList];
    
    [_deferLock unlock];
}


@end
