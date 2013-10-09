/*
 *  LXPlatform_applebase.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 8.9.2008.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXPlatform_mac.h"
#import "LXMutex.h"
#import "LXRandomGen.h"
#import "LXColorFunctions.h"
#import "LXImageFunctions.h"
#import "LXStringUtils.h"

#if defined(LXPLATFORM_IOS)
 #import "LacefxESView.h"
#endif


#if 0
extern void LXImplRunTests();
#endif


static LXPoolRef g_lxPoolForMainThread = NULL;

static LXMutex s_lxAtomicMutex;
LXMutexPtr g_lxAtomicLock = NULL;
LXBool g_lxLocksInited = NO;


static NSThread *g_mainThread = nil;

@interface NSThread (LeopardMainThreadAdditions)
+ (BOOL)isMainThread;
+ (NSThread *)mainThread;
@end




LXVersion LXLibraryVersion()
{
    LXVersion ver = { LXLIBVERSION_LATEST_MAJOR, LXLIBVERSION_LATEST_MINOR, LXLIBVERSION_LATEST_MILLI, 0 };
    return ver;
}

const char *LXPlatformGetID()
{
#ifdef __BIG_ENDIAN__
    return "MacOSX-PowerPC-OpenGL";
#else
 #ifdef __LP64__
    return "MacOSX-x86_64-OpenGL"; 
 #else
    return "MacOSX-i386-OpenGL";
 #endif
#endif
}



void LXPlatformCreateLocks_()
{
    if ( !g_lxAtomicLock) {
        LXMutexInit(&s_lxAtomicMutex);
        g_lxAtomicLock = &s_lxAtomicMutex;
        
        g_lxLocksInited = YES;
    }
}


static CGDirectDisplayID g_mainDisplayCGID = 0;

extern void _LXPlatformSetMainDisplayGLMask(CGOpenGLDisplayMask dmask);


void LQUpdateMainDisplayGLMask()
{
    if ( 1) {   //    if (g_mainDisplayGLMask == 0) {
        g_mainDisplayCGID = (CGDirectDisplayID) [[[[NSScreen mainScreen] deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
        
        CGOpenGLDisplayMask glDisplayMask = CGDisplayIDToOpenGLDisplayMask(g_mainDisplayCGID);
        
        if ((CGOpenGLDisplayMask)LXPlatformMainDisplayIdentifier() != glDisplayMask) {
            _LXPlatformSetMainDisplayGLMask(glDisplayMask);
            
#if 0 //!defined(RELEASE)
            NSLog(@"Setting Lacefx main display cgID: %i -> glID %i; devicedesc: %@ --(end)", g_mainDisplayCGID, LXPlatformMainDisplayIdentifier(),
                  [[[NSScreen mainScreen] deviceDescription] description]);
#endif
        }
    }
}



static int g_lxInited = NO;

LXSuccess LXPlatformInitialize()
{
    if (g_lxInited) return YES;

    // --- Lacefx standard initialization ---
    
    if (LXPoolCurrentForThread() == NULL) {
        g_lxPoolForMainThread = LXPoolCreateForThread();
        LXPrintf("LX platform init: created base pool\n");
    }
    
    LXPlatformCreateLocks_();
    
    LXRdSeed();
    
#if !defined(LXPLATFORM_IOS)
    // load colorspace resources early
    LXCopyICCProfileDataForColorSpaceEncoding(kLX_CanonicalLinearRGB, NULL, NULL);
    LXCopyICCProfileDataForColorSpaceEncoding(kLX_ProPhotoRGB, NULL, NULL);

    // OS X specific init
    LQUpdateMainDisplayGLMask();
#endif
    
    if ( ![NSThread respondsToSelector:@selector(isMainThread)]) {
        g_mainThread = [NSThread currentThread];  // on pre-Leopard, get the main thread here
    } else {
        g_mainThread = nil;
    }

#if 0
    LXImplRunTests();
#endif
        
    g_lxInited = YES;
    return YES;
}


void LXPlatformDeinitialize()
{
    LXPoolRelease(g_lxPoolForMainThread);
    g_lxPoolForMainThread = NULL;
}

void LXPlatformDoPeriodicCleanUp()
{
    LXPoolPurge(g_lxPoolForMainThread);
}


#pragma mark --- threading ---

LXBool LXPlatformCurrentThreadIsMain()
{
    if ( !g_lxInited) {
        LXPlatformLogInfo(@"warning: LX platform was not inited");
    }
    if (g_mainThread) {  // for pre-Leopard, this was inited in LXPlatformInitialize()
        return ([NSThread currentThread] == g_mainThread) ? YES : NO;
    } else {
        return [NSThread isMainThread];
    }
}



#pragma mark --- logging ---


static id g_logger = nil;


@interface NSObject (LXLogging)
- (void)logInfo:(NSString *)str;
- (void)logError:(NSString *)str;
@end


void LXPlatformLogInfo(void *a)
{
    id str = (id)a;
    if ( !str) return;
    if ( ![str respondsToSelector:@selector(length)]) return;
    
    if (g_logger)
        [g_logger logInfo:str];
    else
        NSLog(@"%@", str);
}

void LXPlatformLogError(void *a)
{
    id str = (id)a;
    if ( !str) return;
    if ( ![str respondsToSelector:@selector(length)]) return;
    
    if (g_logger)
        [g_logger logError:str];
    else
        NSLog(@"%@", str);
}

void _LXPlatformSetLogger(id logger)
{
    if ([logger respondsToSelector:@selector(logInfo:)])
        g_logger = logger;
    else
        NSLog(@"** %s: invalid logger (%@)", __func__, logger);
}

id _LXPlatformGetLogger()
{
    return g_logger;
}
