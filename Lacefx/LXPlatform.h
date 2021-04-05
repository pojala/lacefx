/*
 *  LXPlatform.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 8.1.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPLATFORM_H_
#define _LXPLATFORM_H_

#include "LXBasicTypes.h"

// the most important types of DX9 compatible GPUs identified by vendor.
// only includes models that have relevant differences to previous generations
// (e.g. Geforce 7300 belongs into the Geforce 6600 class)
enum {
    kLXGPUClass_Unknown = 0,
    kLXGPUClass_PreDX9,
    kLXGPUClass_UnknownDX9 = 0x50,
    
    kLXGPUClass_Radeon_9600 = 0x1000,
    kLXGPUClass_Radeon_X1600,
    kLXGPUClass_Radeon_X2600,
    
    kLXGPUClass_Geforce_5200 = 0x2000,
    kLXGPUClass_Geforce_6600,
    kLXGPUClass_Geforce_8600,
    
    kLXGPUClass_Intel_950 = 0x3000,
    kLXGPUClass_Intel_HD3000,
    kLXGPUClass_Intel_HD4000,
    
    LXPlatformGPUClass__end = LXENUMMAX
};
typedef LXUInteger LXPlatformGPUClass;


#define LXGPUCLASS_IS_ATI_DX9(_v_)      ((_v_) >= kLXGPUClass_Radeon_9600 && (_v_) < kLXGPUClass_Geforce_5200)
#define LXGPUCLASS_IS_NVIDIA_DX9(_v_)   ((_v_) >= kLXGPUClass_Geforce_5200 && (_v_) < kLXGPUClass_Intel_950)


#define LXLIBVERSION_LATEST_MAJOR  1
#define LXLIBVERSION_LATEST_MINOR  0
#define LXLIBVERSION_LATEST_MILLI  91


enum {
    kLXDoNotSwitchContextOnLock = 1
};


#ifdef __cplusplus
extern "C" {
#endif

LXEXPORT LXVersion LXLibraryVersion();

// --- device info ---
LXEXPORT const char *LXPlatformGetID();  // returned identifier is formatted e.g. "MacOSX-PowerPC-OpenGL" or "Windows-i386-Direct3D"

LXEXPORT uint32_t LXPlatformGetGPUClass();

LXEXPORT void LXPlatformGetHWMaxTextureSize(int *outW, int *outH);

LXEXPORT LXBool LXPlatformHWSupportsFloatRenderTargets();
LXEXPORT LXBool LXPlatformHWSupportsFilteringForFloatTextures();
LXEXPORT LXBool LXPlatformHWSupportsYCbCrTextures();

// --- threaded drawing ---
LXEXPORT LXSuccess LXSurfaceBeginAccessOnThread(LXUInteger flags);
LXEXPORT void LXSurfaceEndAccessOnThread();

LXEXPORT LXBool LXPlatformCurrentThreadIsMain();

// --- native device ---
LXEXPORT void *LXPlatformSharedNativeGraphicsContext();  // returns an LQGLContext (Obj-C) object on the Mac;
                                                         // an IDirect3DDevice9 object on Windows

LXEXPORT uint32_t LXPlatformMainDisplayIdentifier();     // returns a CGOpenGLDisplayMask on the Mac;
                                                         // a display ordinal (e.g. D3DADAPTER_DEFAULT) on Windows

// --- logging ---
LXEXPORT void LXPlatformLogInfo(void *nativeStr);  // argument is a native string type (NSString on the Mac)
LXEXPORT void LXPlatformLogError(void *nativeStr);

// --- system info ---

// returns a data blob containing the computer's GUID in whatever form the OS provides.
// the buffer in outData must be freed with _lx_free().
LXEXPORT LXBool LXPlatformGetSystemGUID(uint8_t **outData, size_t *outDataSize);

#ifdef __cplusplus
}
#endif


#endif
