/*
 *  LXPlatform_d3d.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.7.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPLATFORM_D3D_H_
#define _LXPLATFORM_D3D_H_


#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

#include "LXBasicTypes.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  // require WinXP+

#include "LXMutex.h"
#include "LXMutexAtomic.h"
#include "LXPlatform.h"
#include "LXTexture.h"
#include "LXSurface.h"

#include <windows.h>
#include <d3d9.h>
///#include <d3dx9.h>


#ifdef __cplusplus
extern "C" {
#endif


// --- public API ---

// a Win32 window handle is required to connect to the GPU.
// it may be possible to pass the desktop window handle from GetDesktopWindow(), but it's safest to pass the app's own main window handle.
//
// if NULL is passed, the device can be set later using LXPlatformSetD3D9.
// this allows for compatibility with a device created separately, e.g. by another rendering API.
//
LXEXPORT LXSuccess LXPlatformInitializeWithHWND(HWND hwnd);
LXEXPORT LXSuccess LXPlatformInitialize();   // calls initializeWithHWND with NULL argument (provided for compatibility with Mac-side API)
LXEXPORT void LXPlatformDeinitialize();

LXEXPORT LXSuccess LXPlatformSetD3D9(HWND hwnd, IDirect3D9 *dx9, IDirect3DDevice9 *dx9Dev);

LXEXPORT void LXPlatformDoPeriodicCleanUp();

LXEXPORT const char *LXPlatformGetD3DPixelShaderClass();  // returns e.g. "ps_3_0"


// --- private API ---

LXEXPORT D3DFORMAT LXD3DFormatFromLXPixelFormat(LXPixelFormat pxFormat);

LXEXPORT IDirect3DDevice9 *LXPlatformGetSharedD3D9Device();
LXEXPORT void LXPlatformSetSharedD3D9Device(IDirect3DDevice9 *dev);

LXEXPORT LXSuccess LXTextureRefreshWithData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint);
LXEXPORT LXSuccess LXTextureWillModifyData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint);


// --- private for use by views ---
LXEXPORT IDirect3DSwapChain9 *_LXPlatformCreateD3DSwapChainForView(void *sender, int w, int h);
LXEXPORT LXSurfaceRef LXSurfaceCreateWithD3DSwapChain_(IDirect3DSwapChain9 *swapChain, int w, int h);

LXEXPORT LXTextureRef LXTextureCreateWithD3DTextureAndLXSurface_(IDirect3DTexture9 *texture, uint32_t w, uint32_t h, LXPixelFormat pxFormat, LXSurfaceRef surf);

#ifdef __cplusplus
}
#endif

#endif