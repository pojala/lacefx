
/*
 *  Created by Pauli Ojala on 13.9.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXTexture.h"
#include "LXRef_Impl.h"
#include "LXDraw_Impl.h"
#include "LXPlatform_d3d.h"
#include "LXMutexAtomic.h"
#include "LXImageFunctions.h"
#include "LXPixelBuffer.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>



typedef struct {
    LXREF_STRUCT_HEADER
    
    uint32_t w;
    uint32_t h;
    LXPixelFormat pf;

    LXSurfaceRef surf; 
    
    IDirect3DTexture9 *dxTex;
    IDirect3DDevice9 *device;
    
    // original spec for textures created from data
    void *buffer;
    size_t rowBytes;
    LXUInteger storageHint;

    // render properties
    LXUInteger sampling;
    LXRect imageRegion;
} LXTextureDXImpl;


// platform-independent parts of the class
#define LXTEXTUREIMPL LXTextureDXImpl
#include "LXTexture_baseinc.c"



LXTextureRef LXTextureRetain(LXTextureRef r)
{
    if ( !r) return NULL;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));

    return r;
}

void LXTextureRelease(LXTextureRef r)
{
    if ( !r) return;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        if (imp->dxTex != NULL) {
			imp->dxTex->Release();
            imp->dxTex = NULL;
        }
        _lx_free(imp);
    }
}


LXTextureRef LXTextureCreateWithD3DTextureAndLXSurface_(IDirect3DTexture9 *texture, uint32_t w, uint32_t h, LXPixelFormat pxFormat, LXSurfaceRef surf)
{
    if ( !texture || w < 1 || h < 1)
        return NULL;

    LXTextureDXImpl *imp = (LXTextureDXImpl *) _lx_calloc(sizeof(LXTextureDXImpl), 1);

    LXREF_INIT(imp, LXTextureTypeID(), LXTextureRetain, LXTextureRelease);    
    
    imp->w = w;
    imp->h = h;
    imp->pf = pxFormat;
    imp->sampling = kLXNearestSampling;
    imp->surf = surf;
        
	imp->dxTex = texture;
    imp->dxTex->AddRef();
    
    return (LXTextureRef)imp;
}

static LXBool writeTextureData(IDirect3DTexture9 *texture, uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint8_t *buffer, size_t rowBytes)
{
	HRESULT hRes;
    //D3DFORMAT texFormat = LXD3DFormatFromLXPixelFormat(pxFormat);

	D3DLOCKED_RECT lockRect = { 0, 0 };
	hRes = texture->LockRect(0, &lockRect, NULL, D3DLOCK_DISCARD);
	if (lockRect.pBits == NULL) {
		LXPrintf("** %s -- lockrect failed (%i)\n", __func__, hRes);
		return NO;
	}

	uint8_t *dstBuf = (uint8_t *)lockRect.pBits;
	const size_t dstRowBytes = lockRect.Pitch;
	const size_t minrb = MIN(dstRowBytes, rowBytes);
    LXInteger x, y;

    //LXPrintf("%s -- pxformat %i -- d3d owned dstbuf %p, rowbytes %i -- srcbuf %p, rowbytes %i\n", __func__, pxFormat, dstBuf, dstRowBytes, buffer, rowBytes);
    
    if (pxFormat == kLX_RGBA_INT8) {
        LXPxConvert_RGBA_to_reverse_BGRA_int8(w, h, buffer, rowBytes, dstBuf, dstRowBytes);
    }
    else if (pxFormat == kLX_ARGB_INT8) {  // ARGB->BGRA is reverse
        LXPxConvert_RGBA_to_reverse_int8(w, h, buffer, rowBytes, dstBuf, dstRowBytes);
    }
    else if (pxFormat == kLX_BGRA_INT8 || 
             pxFormat == kLX_Luminance_INT8 ||
             pxFormat == kLX_Luminance_FLOAT16 ||
             pxFormat == kLX_Luminance_FLOAT32) {
        // these formats don't need converting
        if (dstRowBytes == rowBytes) {
            memcpy(dstBuf, buffer, rowBytes*h);
        } else {
            for (y = 0; y < h; y++) {
                uint8_t *srcRowBuf = (uint8_t *)buffer + y*rowBytes;
                uint8_t *dstRowBuf = dstBuf + y*dstRowBytes;
                memcpy(dstRowBuf, srcRowBuf, minrb);
            }
        }
    }
    else {
        LXPrintf("*** %s: unsupported pixel format: %i\n", __func__, pxFormat);
    }
    
	texture->UnlockRect(0);
    return YES;
}

LXTextureRef LXTextureCreateWithData(uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint8_t *buffer, size_t rowBytes,
                                                    LXUInteger storageHint,
                                                    LXError *outError)
{
    const LXBool useClientStorage = (storageHint & kLXStorageHint_ClientStorage) ? YES : NO;
    const LXBool useAGPTexturing =  (storageHint & kLXStorageHint_PreferDMAToCaching) ? YES : NO;

	if ( !buffer || rowBytes < 1)
		return NULL;

	HRESULT hRes;
    D3DFORMAT texFormat = LXD3DFormatFromLXPixelFormat(pxFormat);
    
	if (texFormat == 0) {
		LXPrintf("** %s -- invalid texture format (%i)\n", __func__, pxFormat);
        LXErrorSet(outError, 4802, "invalid texture format");
		return NULL;
	}

	IDirect3DDevice9 *dxDev = LXPlatformGetSharedD3D9Device();
	if ( !dxDev) {
		LXPrintf("** %s -- no d3d device set, can't create texture\n", __func__);
        LXErrorSet(outError, 4803, "no d3d device");
		return NULL;
	}

    IDirect3DTexture9 *newTexture = NULL;

	hRes = dxDev->CreateTexture(w, h, 1,
                                ((useClientStorage || useAGPTexturing) ? D3DUSAGE_DYNAMIC : D3DUSAGE_WRITEONLY),
								texFormat,
                                (useClientStorage || useAGPTexturing) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED,
                                &newTexture, NULL);
                                
	const char *errStr = NULL;
	switch (hRes) {
		case D3D_OK:  break;
		case D3DERR_INVALIDCALL: errStr = "Invalid call"; break;
		case D3DERR_OUTOFVIDEOMEMORY: errStr = "Out of video memory"; break;
		case E_OUTOFMEMORY: errStr = "Out of system memory"; break;
		default: errStr = "unknown"; break;
	}
	if (errStr) {
		LXPrintf("** %s -- D3D error '%s' (%i * %i, pf %lu, cs %i, agp %i)\n", __func__, errStr, w, h, pxFormat, useClientStorage, useAGPTexturing);
            LXErrorSet(outError, 4810, "D3D error");
		return NULL;
	}

    writeTextureData(newTexture, w, h, pxFormat, buffer, rowBytes);

    LXTextureRef newRef = LXTextureCreateWithD3DTextureAndLXSurface_(newTexture, w, h, pxFormat, NULL);
    
    if (newRef) {
        LXTextureDXImpl *imp = (LXTextureDXImpl *)newRef;
        imp->storageHint = storageHint;
    
        // it's safe to store the data pointer only if client storage was specified
        if (useClientStorage) {
            imp->buffer = buffer;
            imp->rowBytes = rowBytes;
        }
    }
    
    newTexture->Release();
    return newRef;
}



#pragma mark --- public API ---


LXUnidentifiedNativeObj LXTextureLockPlatformNativeObj(LXTextureRef r)
{
    if ( !r) return NULL;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;
    
    //imp->retCount++;    
    return (LXUnidentifiedNativeObj)(imp->dxTex);
}

void LXTextureUnlockPlatformNativeObj(LXTextureRef r)
{
    if ( !r) return;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;
    
    //imp->retCount--;
}

LXSuccess LXTextureCopyContentsIntoPixelBuffer(LXTextureRef r, LXPixelBufferRef pixBuf,
                                                                     LXError *outError)
{
    if ( !r) return NO;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;

    if (imp->surf) {
        return LXSurfaceCopyContentsIntoPixelBuffer(imp->surf, pixBuf, outError);
    }
    else {
        ///NSLog(@"%s - doing direct copy of data (%p, rb %i, pf %i)", __func__, imp->buffer, imp->rowBytes, imp->pf);
        if (imp->buffer && imp->rowBytes && imp->pf) {
            return LXPixelBufferWriteDataWithPixelFormatConversion(pixBuf,
                                                                  (unsigned char *)imp->buffer,
                                                                  imp->w, imp->h,
                                                                  imp->rowBytes,
                                                                  imp->pf,
                                                                  NULL, outError);
        }
        else {
            LXPrintf("** %s: can't copy, original buffer is unavailable (%p, rb %i, pf %i)", __func__, imp->buffer, (int)imp->rowBytes, (int)imp->pf);
            LXErrorSet(outError, 4891, "no buffer available for texture copy");
            return NO;
        }
    }
}



#pragma mark --- private API ---

LXSuccess LXTextureWillModifyData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    return YES;
}

LXSuccess LXTextureRefreshWithData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    if ( !r) return NO;
    LXTextureDXImpl *imp = (LXTextureDXImpl *)r;
    
    if ( !imp->dxTex) {
        LXPrintf("** %s: no d3d texture", __func__);
        return NO;
    }

    return writeTextureData(imp->dxTex, imp->w, imp->h, imp->pf, buffer, rowBytes);
}
