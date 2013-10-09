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

#import "LXPlatform_d3d.h"
#import "LXRandomGen.h"
#import "LXMutex.h"
#import "LXMutexAtomic.h"
#import "LXStringUtils.h"

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
/*
 this file uses some of the Foundation types and functions like NSString and NSLog.
 these references could be wrapped in __OBJC__ checks too for compiling on something
 else than Cocotron.
 */


// globals
static HWND g_displayHwnd = NULL;
static IDirect3D9 *g_sharedDX9 = NULL;
static IDirect3DDevice9 *g_sharedDXDev = NULL;
static D3DCAPS9 g_d3dCaps;
static char *g_d3dPixelShaderVersionStr = NULL;

static LXPoolRef g_lxPoolForMainThread = NULL;
static NSAutoreleasePool *g_nsPoolForMainThread = NULL;

static NSThread *g_mainThread = nil;


// private function in LXPool.c
extern LXPoolRef LXPoolCreateForThreadWithAssociatedCleanUpCallbacks_(LXGenericCleanupFuncPtr purgeFunc, LXGenericCleanupFuncPtr destroyFunc, void *cleanUpObj);



#pragma mark --- platform utils ---

D3DFORMAT LXD3DFormatFromLXPixelFormat(LXPixelFormat pxFormat)
{
	D3DFORMAT texFormat = (D3DFORMAT)0;
	
	switch (pxFormat) {
        case kLX_ARGB_INT8:
        case kLX_BGRA_INT8:
		case kLX_RGBA_INT8:			texFormat = D3DFMT_A8R8G8B8;  break;
        
		case kLX_RGBA_FLOAT16:		texFormat = D3DFMT_A16B16G16R16F;  break;
		case kLX_RGBA_FLOAT32:		texFormat = D3DFMT_A32B32G32R32F;  break;
        
        case kLX_Luminance_INT8:    texFormat = D3DFMT_L8;  break;
        case kLX_Luminance_FLOAT16: texFormat = D3DFMT_R16F;  break;
        case kLX_Luminance_FLOAT32: texFormat = D3DFMT_R32F;  break;
        
        case kLX_YCbCr422_INT8:     texFormat = D3DFMT_UYVY;  break;
        
        default:  LXPrintf("** %s: unsupported pixel format (%ld)\n", __func__, pxFormat);
	}

    return texFormat;
}


#pragma mark --- threading ---

static LXMutex g_lxCtxMutex;
static LXMutex s_lxAtomicMutex;
static LXMutex s_lxStdoutMutex;

LXMutexPtr g_lxAtomicLock = NULL;
LXMutexPtr g_lxStdoutLock = NULL;

LXBool g_lxLocksInited = NO;


LXVersion LXLibraryVersion()
{
    LXVersion ver = { LXLIBVERSION_LATEST_MAJOR, LXLIBVERSION_LATEST_MINOR, LXLIBVERSION_LATEST_MILLI, 0 };
    return ver;
}

const char *LXPlatformGetID()
{
    return "Windows-i386-Direct3D";
}


void LXPlatformCreateLocks_()
{
    if ( !g_lxLocksInited) {
        LXMutexInit(&g_lxCtxMutex);
                
        LXMutexInit(&s_lxAtomicMutex);
        g_lxAtomicLock = &s_lxAtomicMutex;

        LXMutexInit(&s_lxStdoutMutex);
        g_lxStdoutLock = &s_lxStdoutMutex;

        g_lxLocksInited = YES;
        
        ///printf("%s finished\n", __func__);
    }
}

LXSuccess LXSurfaceBeginAccessOnThread(LXUInteger flags)
{
    LXMutexLock(&g_lxCtxMutex);
    return YES;
}

void LXSurfaceEndAccessOnThread()
{
    LXMutexUnlock(&g_lxCtxMutex);
}


#pragma mark --- native context --- 

void *LXPlatformSharedNativeGraphicsContext()
{
    return (void *)g_sharedDXDev;
}

IDirect3DDevice9 *LXPlatformGetSharedD3D9Device()
{
    return g_sharedDXDev;
}

void LXPlatformSetSharedD3D9Device(IDirect3DDevice9 *dev)
{
    if (dev == g_sharedDXDev)
        return;

    LXPlatformCreateLocks_();

    if (g_sharedDXDev)
        IDirect3DDevice9_Release(g_sharedDXDev);
        
    g_sharedDXDev = dev;
    IDirect3DDevice9_AddRef(g_sharedDXDev);
}

NSString *NSStringFromD3DHRes(HRESULT hRes)
{
    NSString *str = nil;
    switch (hRes) {
        case D3DERR_NOTAVAILABLE:
            str = @"Not available";  break;
        case D3DERR_INVALIDDEVICE:
            str = @"Invalid device";  break;
        case D3DERR_INVALIDCALL:
            str = @"Invalid call";  break;
        case D3DERR_OUTOFVIDEOMEMORY:
            str = @"Out of video memory";  break;
            
        default:
            str = [NSString stringWithFormat:@"Error %i (code %i)", hRes, ((unsigned int)hRes & ((1 << 16) - 1))];  break;
    }
    return str;
}


uint32_t LXPlatformMainDisplayIdentifier()
{
    // TODO: should support setting this value so that a different GPU can be used
    // (default is the primary adapter)
    return D3DADAPTER_DEFAULT;
}


LXSuccess _LXPlatformRecreateD3D9Device()
{
    if ( !g_sharedDX9 || !g_displayHwnd) {
        NSLog(@"** %s: no dx9 api object or hwnd (%p, %p)", __func__, g_sharedDX9, g_displayHwnd);
        return NO;
    }
    
    HRESULT hRes;
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    
    // this is the size for the default back buffer that's automatically created along with the device
    // (aka the "implicit swap chain" of the device).
    // any views that wish to render on-screen must create a D3D swap chain of their own.
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed = TRUE;
    d3dpp.EnableAutoDepthStencil = FALSE;
    //d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
  
    IDirect3DDevice9 *dxDev = NULL;
    
    DWORD devFlags = D3DCREATE_MIXED_VERTEXPROCESSING;
        
    hRes = IDirect3D9_CreateDevice(g_sharedDX9,
                                   LXPlatformMainDisplayIdentifier(),
                                   D3DDEVTYPE_HAL,
                                   g_displayHwnd,
                                   //D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED, 
                                   (devFlags | D3DCREATE_FPU_PRESERVE),
                                   &d3dpp,
                                   &dxDev);

    if (hRes != D3D_OK) {
        NSLog(@"** %s device creation failed: '%@' (hwnd is %p, dx9 obj is %p)", __func__, NSStringFromD3DHRes(hRes), g_displayHwnd, g_sharedDX9);
        return NO;
    } else {
        LXPlatformSetSharedD3D9Device(dxDev);
        /*
        LXInteger texFormats[8] = { D3DFMT_A8R8G8B8, D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F,
                                    D3DFMT_L8, D3DFMT_R16F, D3DFMT_R32F,
                                    D3DFMT_UYVY, D3DFMT_YUY2 };
        LXInteger i;
        for (i = 0; i < 8; i++) {
            LXBool isValid = (S_OK == IDirect3D9_CheckDeviceFormat(g_sharedDX9, LXPlatformMainDisplayIdentifier(),
                                          D3DDEVTYPE_HAL,
                                          D3DFMT_X8R8G8B8,
                                          0,
                                          D3DRTYPE_TEXTURE,
                                          texFormats[i]));
            //LXBool isValid = isTextureFormatOk(g_sharedDX9, texFormats[i], D3DFMT_X8R8G8B8);
            //printf("  %i: texformat %i: %s\n", i, texFormats[i], (isValid) ? "supported" : "unsupported");
        }*/
        
        return YES;
    }
}


IDirect3DSwapChain9 *_LXPlatformCreateD3DSwapChainForView(void *sender, int w, int h)
{
    IDirect3DDevice9 *dxDev = LXPlatformGetSharedD3D9Device();
    if ( !dxDev) {
        NSLog(@"%s: no device exists, will create now (%p, %p)", __func__, g_sharedDX9, g_displayHwnd);
        
        _LXPlatformRecreateD3D9Device();
        if ( !dxDev) {
            NSLog(@"** %s: failed to create D3D9 device", __func__);
            return NO;
        }
    }
    
    HRESULT hRes;
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    
    d3dpp.BackBufferWidth = w;
    d3dpp.BackBufferHeight = h;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed = TRUE;
    d3dpp.EnableAutoDepthStencil = FALSE;

    IDirect3DSwapChain9 *newSwapChain = NULL;

    hRes = IDirect3DDevice9_CreateAdditionalSwapChain(dxDev, &d3dpp, &newSwapChain);
    
    if (hRes != D3D_OK) {
        NSLog(@"%s failed: %@", __func__, NSStringFromD3DHRes(hRes));
        return NULL;
    } else {
        return newSwapChain;
    }
}


/*
static LXBool isTextureFormatOk(IDirect3D9 *d3d, D3DFORMAT textureFormat, D3DFORMAT adapterFormat)
{
    HRESULT hr = IDirect3D9_CheckDeviceFormat(d3d, LXPlatformMainDisplayIdentifier(),
                                          D3DDEVTYPE_HAL,
                                          adapterFormat,
                                          0,
                                          D3DRTYPE_TEXTURE,
                                          textureFormat);
    
    return (SUCCEEDED(hr)) ? YES : NO;
}
*/

static void updatePSVersionString()
{
    if ( !g_sharedDX9) {
        NSLog(@"** %s: device has not been initialized", __func__);
        return;
    }

    const char *psname = NULL;
    if (g_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(4, 0))
        psname = "ps_4_0";
    else if (g_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
        psname = "ps_3_0";
    else if (g_d3dCaps.PixelShaderVersion > D3DPS_VERSION(2, 0))
        psname = "ps_2_a";
    else if (g_d3dCaps.PixelShaderVersion == D3DPS_VERSION(2, 0))
        psname = "ps_2_0";
    else
        psname = "";
    
    // TESTING
    psname = "ps_2_0";
    
    _lx_free(g_d3dPixelShaderVersionStr);
    g_d3dPixelShaderVersionStr = (char *)_lx_calloc(strlen(psname) + 1, 1);
    
    memcpy(g_d3dPixelShaderVersionStr, psname, strlen(psname));
}


static void lxPoolAssocPurge(void *lxPool, void *userData)
{
    ///LXPrintf("%s: cleaning main thread autorelease pool\n", __func__);
    [g_nsPoolForMainThread release];
    g_nsPoolForMainThread = [[NSAutoreleasePool alloc] init];
}

static void doBasicInit()
{
    // --- Lacefx standard initialization ---

    g_mainThread = [NSThread currentThread];
    
    if (LXPoolCurrentForThread() == NULL) {
        ///g_nsPoolForMainThread = [[NSAutoreleasePool alloc] init];
        g_lxPoolForMainThread = LXPoolCreateForThreadWithAssociatedCleanUpCallbacks_(lxPoolAssocPurge, NULL, NULL);
        
        LXPrintf("LX platform init: created base pools\n");
    }
    
    LXRdSeed();
}


LXSuccess LXPlatformSetD3D9(HWND hwnd, IDirect3D9 *dx9, IDirect3DDevice9 *dx9Dev)
{
    if ( !dx9 || !dx9Dev) return NO;

    HRESULT hRes;
    memset(&g_d3dCaps, 0, sizeof(D3DCAPS9));
    
    if (g_sharedDX9 && g_sharedDX9 != dx9) {
        NSLog(@"%s: warning -- D3D state was already initialized, any existing LX objects may be incompatible after this call", __func__);
    }

    g_displayHwnd = hwnd;

    hRes = IDirect3D9_GetDeviceCaps(dx9, LXPlatformMainDisplayIdentifier(), D3DDEVTYPE_HAL, &g_d3dCaps);
    
    if (hRes == D3D_OK) {
        NSLog(@"%s: Got Direct3D max tex size: %i * %i; device was supplied by caller\n", __func__, (int)g_d3dCaps.MaxTextureWidth, (int)g_d3dCaps.MaxTextureHeight);
        g_sharedDX9 = dx9;
        
        updatePSVersionString();
    
        LXPlatformSetSharedD3D9Device(dx9Dev);
        return YES;
    }
    else {
        NSLog(@"%s: GetDevCaps failed, can't continue - %@", __func__, NSStringFromD3DHRes(hRes));
        g_sharedDX9 = NULL;
        
        return NO;
    }
}

LXSuccess LXPlatformInitialize()
{
    return LXPlatformInitializeWithHWND(NULL);
}

LXSuccess LXPlatformInitializeWithHWND(HWND hwnd)
{
    if (g_displayHwnd) {
        if (hwnd) {
            LXPrintf("*** %s - warning: D3D state has already been initialized, this call won't do anything (hwnd is %p, new is %p\n)", __func__, g_displayHwnd, hwnd);
        }
        return YES;  // initialization has already been done
    }

    doBasicInit();

    // --- D3D-specific ---
    memset(&g_d3dCaps, 0, sizeof(D3DCAPS9));
    
    g_displayHwnd = hwnd;

    if ( !hwnd) {
        ///LXPrintf("** %s can't create D3D device: a HWND must be supplied", __func__);
        return YES;
    }
    
    IDirect3D9 *dx9 = Direct3DCreate9(D3D_SDK_VERSION);
    HRESULT hRes;
    
    if ( !dx9) {
        LXPrintf("** %s: Couldn't create D3D device, initialization failed", __func__);
        return NO;
    }
    
    hRes = IDirect3D9_GetDeviceCaps(dx9, LXPlatformMainDisplayIdentifier(), D3DDEVTYPE_HAL, &g_d3dCaps);
    
    if (hRes == D3D_OK) {
        NSLog(@"%s: Got Direct3D max tex size: %i * %i; will create device\n", __func__, (int)g_d3dCaps.MaxTextureWidth, (int)g_d3dCaps.MaxTextureHeight);
        g_sharedDX9 = dx9;
        
        updatePSVersionString();
    
        return _LXPlatformRecreateD3D9Device();
    }
    else {
        NSLog(@"%s: GetDevCaps failed - %@", __func__, NSStringFromD3DHRes(hRes));
        g_sharedDX9 = NULL;
        
        return NO;
    }    
}

void LXPlatformDeinitialize()
{
    LXPoolRelease(g_lxPoolForMainThread);
    g_lxPoolForMainThread = NULL;
    
    [g_nsPoolForMainThread release];
    g_nsPoolForMainThread = NULL;
    
    g_displayHwnd = NULL;
    
    if (g_sharedDXDev)
        IDirect3DDevice9_Release(g_sharedDXDev);
    g_sharedDXDev = NULL;
    
    if (g_sharedDX9)
        IDirect3D9_Release(g_sharedDX9);
    g_sharedDX9 = NULL;
}


LXBool LXPlatformCurrentThreadIsMain()
{
    if ( !g_mainThread) {
        NSLog(@"%s: warning: LX platform was not inited", __func__);
    }
    return ([NSThread currentThread] == g_mainThread) ? YES : NO;
}


void LXPlatformDoPeriodicCleanUp()
{
    LXPoolPurge(g_lxPoolForMainThread);
}


void LXPlatformGetHWMaxTextureSize(int *outW, int *outH)
{
    int w = 0, h = 0;
    
    if ( !g_sharedDX9) {
        NSLog(@"** %s: device has not been initialized", __func__);
    } else {
        w = g_d3dCaps.MaxTextureWidth;
        h = g_d3dCaps.MaxTextureHeight;
    }
    
    if (outW) *outW = w;
    if (outH) *outH = h;
}


uint32_t LXPlatformGetGPUClass()
{
    if ( !g_sharedDX9) {
        NSLog(@"** %s: device has not been initialized", __func__);
        return kLXGPUClass_Unknown;
    }

    if (g_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
        return kLXGPUClass_Geforce_6600;
    else if (g_d3dCaps.PixelShaderVersion > D3DPS_VERSION(2, 0))
        return kLXGPUClass_Geforce_5200;
    else if (g_d3dCaps.PixelShaderVersion == D3DPS_VERSION(2, 0))
        return kLXGPUClass_Radeon_9600;
    
    return kLXGPUClass_PreDX9;
}

const char *LXPlatformGetD3DPixelShaderClass()
{
    return g_d3dPixelShaderVersionStr;
}

/*
void LXPlatformGetD3DPixelShaderClass(char *outStr)
{
    if ( !g_sharedDX9) {
        NSLog(@"** %s: device has not been initialized", __func__);
        return;
    }

    const char *psname = NULL;
    if (g_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(4, 0))
        psname = "ps_4_0";
    else if (g_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
        psname = "ps_3_0";
    else if (g_d3dCaps.PixelShaderVersion > D3DPS_VERSION(2, 0))
        psname = "ps_2_a";
    else if (g_d3dCaps.PixelShaderVersion == D3DPS_VERSION(2, 0))
        psname = "ps_2_0";

    if ( !psname)
        outStr[0] = 0;
    else {
        memcpy(outStr, psname, 6);
        outStr[6] = 0;
    }
}
*/

LXBool LXPlatformHWSupportsFilteringForFloatTextures()
{
    uint32_t gpuClass = LXPlatformGetGPUClass();
    
    if (LXGPUCLASS_IS_NVIDIA_DX9(gpuClass) && gpuClass >= kLXGPUClass_Geforce_6600)
        return YES;
    else if (LXGPUCLASS_IS_ATI_DX9(gpuClass) && gpuClass >= kLXGPUClass_Radeon_X2600)
        return YES;
        
    return NO;
}

LXBool LXPlatformHWSupportsFloatRenderTargets()
{
    uint32_t gpuClass = LXPlatformGetGPUClass();
    
    return (LXGPUCLASS_IS_NVIDIA_DX9(gpuClass) || LXGPUCLASS_IS_ATI_DX9(gpuClass)) ? YES : NO;
}

LXBool LXPlatformHWSupportsYCbCrTextures()
{
    if ( !g_sharedDX9) {
        NSLog(@"** %s: device has not been initialized", __func__);
        return NO;
    }

    HRESULT hr = IDirect3D9_CheckDeviceFormat(g_sharedDX9, LXPlatformMainDisplayIdentifier(),
                                                D3DDEVTYPE_HAL,
                                                D3DFMT_X8R8G8B8,
                                                0,
                                                D3DRTYPE_TEXTURE,
                                                D3DFMT_UYVY);
    return (hr == S_OK) ? YES : NO;
}


LXBool LXPlatformGetSystemGUID(uint8_t **outData, size_t *outDataSize)
{
    // TODO: implement system guid on Windows
    return NO;
}
