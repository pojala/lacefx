/*
 *  LXPlatform_ios.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/5/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXPlatform_ios.h"
#import "LacefxESView.h"
#import "LXPlatform.h"
#import "LXPlatform_ios.h"
#import "LXMutex.h"


/*
Most functions are in LXPlatform_applebase.m.
This file just contains OpenGL ES specific setup.
*/


extern LXMutex *g_lxAtomicLock;
extern void LXPlatformCreateLocks_();


EAGLContext *g_EAGLCtx = nil;



LXBool LXPlatformHWSupportsFilteringForFloatTextures()
{
    return NO;
}

LXBool LXPlatformHWSupportsFloatRenderTargets()
{
    return NO;
}

LXBool LXPlatformHWSupportsYCbCrTextures()
{
    return NO;
}

void LXPlatformGetHWMaxTextureSize(int *outW, int *outH)
{
    // TODO: implement real check
    int w = 1024;
    int h = 1024;
    if (outW) *outW = w;
    if (outH) *outH = h;
}

uint32_t LXPlatformGetGPUClass()
{
    // TODO: implement real check
    return kLXGPUClass_Unknown;
}


void *LXPlatformSharedNativeGraphicsContext()
{
    return g_EAGLCtx;
}


void LXPlatformSetSharedEAGLContext(void *obj)
{
    EAGLContext *ctx = (EAGLContext *)obj;
    
    if ( !g_lxAtomicLock) {
        LXPlatformCreateLocks_();
    }

    if (ctx != g_EAGLCtx) {
        [g_EAGLCtx release];
        g_EAGLCtx = [ctx retain];
    }
}

uint32_t LXPlatformMainDisplayIdentifier()
{
    return 0;
}
