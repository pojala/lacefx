
/*
 *  Created by Pauli Ojala on 15.9.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXSurface.h"
#include "LXTexture.h"
#include "LXTextureArray.h"
#include "LXShader.h"
#include "LXDrawContext.h"
#include "LXPixelBuffer.h"

#include "LXRef_Impl.h"
#include "LXDraw_Impl.h"
#include "LXPlatform_d3d.h"
#include "LXPool_surface_priv.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>



typedef struct {
    LXREF_STRUCT_HEADER
    
    LXPoolRef sourcePool;
    
    int w;
    int h;
    LXPixelFormat pixelFormat;
    LXBool hasZBuffer;

    LXBool surfaceIsTextureBacked;
    IDirect3DTexture9 *dxTex;           // retained
    IDirect3DSwapChain9 *dxSwapChain;   // not retained
	
    IDirect3DSurface9 *dxSurf;
    IDirect3DDevice9 *dxDev;
    
    LXTextureRef lxTexture;
    
    // render properties
    LXRect imageRegion;
    
    // main memory texture for readback
    IDirect3DTexture9 *systemMemTexture;
} LXSurfaceDXImpl;


// platform-independent parts of the class
#define LXSURFACEIMPL LXSurfaceDXImpl
#include "LXSurface_baseinc.c"

/*
typedef struct LXBasicVertex_d3d_xyzrhw
{
    FLOAT x, y, z, rhw;
    FLOAT u0, v0;
    FLOAT u1, v1;
} LXBasicVertex_d3d_xyzrhw;

#define LXBASICVERTEX_D3D_FORMAT  (D3DFVF_XYZRHW | D3DFVF_TEX2)
*/
typedef struct LXBasicVertex_d3d
{
    FLOAT x, y, z;
    FLOAT u0, v0;
    FLOAT u1, v1;
} LXBasicVertex_d3d;

#define LXBASICVERTEX_D3D_FORMAT  (D3DFVF_XYZ | D3DFVF_TEX2)




LXSurfaceRef LXSurfaceRetain(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));

    return r;
}

void LXSurfaceRelease(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        if (imp->lxTexture) {
            LXTextureRelease(imp->lxTexture);
            imp->lxTexture = NULL;
        }

        if (imp->dxSurf) {
			imp->dxSurf->Release();
            imp->dxSurf = NULL;
        }

        if (imp->dxTex) {
			imp->dxTex->Release();
            imp->dxTex = NULL;
        }
        
        if (imp->systemMemTexture) imp->systemMemTexture->Release();
        
        imp->dxSwapChain = NULL;
        imp->dxDev = NULL;
            
        _lx_free(imp);
    }
}

LXSurfaceRef LXSurfaceCreateWithD3DSwapChain_(IDirect3DSwapChain9 *swapChain, int w, int h)
{
    if ( !swapChain || w < 1 || h < 1)
        return NULL;
    
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *) _lx_calloc(sizeof(LXSurfaceDXImpl), 1);

    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);
    
    imp->w = w;
    imp->h = h;
    imp->pixelFormat = (LXPixelFormat)0;
    imp->hasZBuffer = NO;
    
    imp->dxDev = LXPlatformGetSharedD3D9Device();
    
    imp->surfaceIsTextureBacked = NO;
    imp->dxSwapChain = swapChain;
    
    imp->lxTexture = NULL;
    
    return (LXSurfaceRef)imp;
}


LXSurfaceRef LXSurfaceCreateWithD3DTexture_(IDirect3DTexture9 *dxTex, int w, int h, LXPixelFormat pxFormat, LXBool hasZBuffer, IDirect3DDevice9 *dxDev)
{
    if ( !dxTex || w < 1 || h < 1)
        return NULL;
    
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *) _lx_calloc(sizeof(LXSurfaceDXImpl), 1);

    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);
    
    imp->w = w;
    imp->h = h;
    imp->pixelFormat = pxFormat;
    imp->hasZBuffer = hasZBuffer;
    
    imp->dxDev = dxDev;
    
    imp->surfaceIsTextureBacked = YES;
    imp->dxTex = dxTex;
    imp->dxTex->AddRef();
    
    // create the LXTexture wrapper
    imp->lxTexture = LXTextureCreateWithD3DTextureAndLXSurface_(dxTex, w, h, pxFormat, (LXSurfaceRef)imp);
    
    return (LXSurfaceRef)imp;
}

LXSurfaceRef LXSurfaceCreate(LXPoolRef pool,
                             uint32_t w, uint32_t h, LXPixelFormat pxFormat,
                             uint32_t flags,
                             LXError *outError)
{
    LXBool hasZBuffer = (flags & kLXSurfaceHasZBuffer) ? YES : NO;

    if (w < 1 || h < 1) {
        LXErrorSet(outError, 4001, "width or height is zero");
        return NULL;
    }

    // TODO: add pool support (not yet done in Obj-C version either)
    
	HRESULT hRes;
    D3DFORMAT texFormat = LXD3DFormatFromLXPixelFormat(pxFormat);

	if (texFormat == 0) {
		LXPrintf("*** %s -- invalid texture format (%i)\n", __func__, pxFormat);
        LXErrorSet(outError, 4802, "invalid texture format");
		return NULL;
	}

	IDirect3DDevice9 *dxDev = LXPlatformGetSharedD3D9Device();
	if ( !dxDev) {
		LXPrintf("*** %s -- no Direct3D device set, can't create texture\n", __func__);
        LXErrorSet(outError, 4803, "no D3D device");
		return NULL;
	}

	IDirect3DTexture9 *newTexture = NULL;
    
    hRes = dxDev->CreateTexture(w, h, 1,
                                D3DUSAGE_RENDERTARGET,
                                texFormat,
                                D3DPOOL_DEFAULT,
                                &newTexture, NULL);
    
    if (hRes != D3D_OK) {
        LXErrorSet(outError, 4002, "D3D texture creation failed");
        return NULL;
    }
    
    LXSurfaceRef newSurf = LXSurfaceCreateWithD3DTexture_(newTexture, w, h, pxFormat, hasZBuffer, dxDev);
    
    newTexture->Release();
    return newSurf;
}



LXTextureRef LXSurfaceGetTexture(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;

    if (NULL == imp->lxTexture && imp->surfaceIsTextureBacked) {
        LXPrintf("*** lxTexture missing for surface %p\n", r);
    }
    
    return imp->lxTexture;
}

LXPixelFormat LXSurfaceGetPixelFormat(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    return imp->pixelFormat;
}


#pragma mark --- native object ---

IDirect3DSurface9 *LXSurfaceLockD3DSurface_(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    
    if (imp->surfaceIsTextureBacked) {
        imp->dxTex->GetSurfaceLevel(0, &imp->dxSurf);  // this call retains the returned surface
    } else {
        imp->dxSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &imp->dxSurf);  // this call retains the returned surface
    }
    
    return imp->dxSurf;
}

void LXSurfaceUnlockD3DSurface_(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;

    imp->dxSurf->Release();
    imp->dxSurf = NULL;
}

LXBool LXSurfaceNativeYAxisIsFlipped(LXSurfaceRef r)
{
    return YES;
}

LXBool LXSurfaceHasDefaultPixelOrthoProjection(LXSurfaceRef r)
{
    return NO;
}

LXUnidentifiedNativeObj LXSurfaceLockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    
    imp->retCount++;
    
    return (LXUnidentifiedNativeObj)LXSurfaceLockD3DSurface_(r);
}

void LXSurfaceUnlockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;

    LXSurfaceUnlockD3DSurface_(r);
    
    imp->retCount--;
}

void LXSurfaceFlush(LXSurfaceRef r)
{
    // this function is meant for more precise control of OpenGL pixelbuffers, so we don't need to do anything here
}



#pragma mark --- drawing ---


#define ENTERRENDERTARGET(_surf_) \
	IDirect3DSurface9 *origBackBuffer = NULL;  \
	dxDev->GetRenderTarget(0, &origBackBuffer);  \
    dxDev->SetRenderTarget(0, _surf_);

#define EXITRENDERTARGET() \
    dxDev->SetRenderTarget(0, origBackBuffer); \
    if (origBackBuffer)  origBackBuffer->Release(); \
    origBackBuffer = NULL;


void LXSurfaceClear(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    IDirect3DDevice9 *dxDev = imp->dxDev;
    if ( !dxDev) return;
    
    IDirect3DSurface9 *surf = LXSurfaceLockD3DSurface_(r);
    if ( !surf) return;
    
    ENTERRENDERTARGET(surf)
    {
        DWORD flags = D3DCLEAR_TARGET;
        if (imp->hasZBuffer) flags |= D3DCLEAR_ZBUFFER;
        
        dxDev->Clear(0, NULL,  flags,
                     D3DCOLOR_XRGB(0, 0, 0),  1.0,  0);
    }
    EXITRENDERTARGET()
    
    LXSurfaceUnlockD3DSurface_(r);
}

void LXSurfaceClearRegionWithRGBA(LXSurfaceRef r, LXRect rect, LXRGBA c)
{
    if ( !r) return;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    IDirect3DDevice9 *dxDev = imp->dxDev;
    if ( !dxDev) return;

    IDirect3DSurface9 *surf = LXSurfaceLockD3DSurface_(r);
    if ( !surf) return;
    
    ENTERRENDERTARGET(surf)
    {
        DWORD flags = D3DCLEAR_TARGET;
        D3DCOLOR color = D3DCOLOR_RGBA( (int)(255 * c.r), (int)(255 * c.g), (int)(255 * c.b), (int)(255 * c.a) );
        D3DRECT d3dRect;
        d3dRect.x1 = rect.x;
        d3dRect.y1 = rect.y;
        d3dRect.x2 = rect.x + rect.w;
        d3dRect.y2 = rect.y + rect.h;
        
        if (imp->hasZBuffer) flags |= D3DCLEAR_ZBUFFER;
        
        dxDev->Clear(1, &d3dRect, flags,
                     color,  1.0,  0);
    }
    EXITRENDERTARGET()
    
    LXSurfaceUnlockD3DSurface_(r);
}

void LXSurfaceDrawPrimitive(LXSurfaceRef r,
                            LXPrimitiveType primitiveType,
                            void *vertices,
                            LXUInteger vertexCount,
                            LXVertexType vertexType,
                            LXDrawContextRef drawCtx)
{
    if ( !r || !vertices || vertexCount < 1) return;

    if (vertexType != kLXVertex_XYUV && vertexType != kLXVertex_XYZW) {
        LXPrintf("*** %s: unsupported vertex type (%i)\n", __func__, vertexType);
        return;
    }

    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    IDirect3DDevice9 *dxDev = imp->dxDev;
    if ( !dxDev) return;

    IDirect3DSurface9 *surf = LXSurfaceLockD3DSurface_(r);
    if ( !surf) return;

    ENTERRENDERTARGET(surf)

    LXDrawContextActiveState *drawState = NULL;
    LXDrawContextCreateState_(drawCtx, &drawState);
    drawState->dev = dxDev;
    drawState->surf = surf;
    
    LXDrawContextBeginForSurface_(drawCtx, r, &drawState);

    LXUInteger drawFlags = LXDrawContextGetFlags(drawCtx);
    

    dxDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dxDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dxDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dxDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);

    if (drawFlags & kLXDrawFlag_UseFixedFunctionBlending_SourceIsPremult) {
        dxDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        
        ///dxDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dxDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        dxDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    else if (drawFlags & kLXDrawFlag_UseFixedFunctionBlending_SourceIsUnpremult) {
        dxDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        
        ///dxDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dxDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dxDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    else {
        dxDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }


    D3DPRIMITIVETYPE d3dPrimType;
    switch (primitiveType) {
        default:
        case kLXTriangleFan:    d3dPrimType = D3DPT_TRIANGLEFAN;  break;
        case kLXTriangleStrip:  d3dPrimType = D3DPT_TRIANGLESTRIP;  break;
        case kLXPoints:         d3dPrimType = D3DPT_POINTLIST;    break;
        case kLXLineStrip:      d3dPrimType = D3DPT_LINESTRIP;    break;

        // these don't have a direct d3d equivalent, so they need further handling
        case kLXQuads:          d3dPrimType = D3DPT_TRIANGLEFAN;  break;        
        case kLXLineLoop:       d3dPrimType = D3DPT_LINESTRIP;    break;
    }


    dxDev->SetFVF(LXBASICVERTEX_D3D_FORMAT);

    // pack into d3d vertices
    // (check for supported LXVertexTypes is at start of this func)
    LXBasicVertex_d3d *d3dVerts = (LXBasicVertex_d3d *) _lx_malloc(vertexCount * sizeof(LXBasicVertex_d3d));
    LXInteger i;
    
    switch (vertexType) {
        case kLXVertex_XYUV:
            for (i = 0; i < vertexCount; i++) {
                LXBasicVertex_d3d *dv = d3dVerts + i;
                LXVertexXYUV *sv = ((LXVertexXYUV *)vertices) + i;

                dv->x = sv->x;
                dv->y = sv->y;
                dv->z = 0.0f;
                //dv->w = 1.0f;
                dv->u0 = sv->u;
                dv->v0 = sv->v;
                dv->u1 = 0.0f;
                dv->v1 = 0.0f;
            }
            break;

        case kLXVertex_XYZW:
            for (i = 0; i < vertexCount; i++) {
                LXBasicVertex_d3d *dv = d3dVerts + i;
                LXVertexXYZW *sv = ((LXVertexXYZW *)vertices) + i;
                
                dv->x = sv->x;
                dv->y = sv->y;
                dv->z = sv->z;
                //dv->w = sv->w;
                dv->u0 = 0.0f;
                dv->v0 = 0.0f;
                dv->u1 = 0.0f;
                dv->v1 = 0.0f;
            }
            break;
    }

    D3DMATRIX mmat;
    D3DMATRIX *mat = &mmat;
    dxDev->GetTransform(D3DTS_PROJECTION, mat);
    
    {
        dxDev->BeginScene();
        
        if (primitiveType == kLXQuads) {
            LXInteger quadCount = vertexCount / 4;
            
            for (i = 0; i < quadCount; i++) {
                LXBasicVertex_d3d *quadVerts = d3dVerts + i*4;
                dxDev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, (void *)quadVerts, sizeof(LXBasicVertex_d3d));
            }
        }
        
        else if (d3dPrimType == D3DPT_TRIANGLEFAN || d3dPrimType == D3DPT_TRIANGLESTRIP) {
            LXInteger triangleCount = vertexCount - 2;
            dxDev->DrawPrimitiveUP(d3dPrimType, triangleCount, (void *)d3dVerts, sizeof(LXBasicVertex_d3d));
        }

        else if (d3dPrimType == D3DPT_LINESTRIP) {
            LXInteger lineCount = vertexCount - 1;
            dxDev->DrawPrimitiveUP(d3dPrimType, lineCount, (void *)d3dVerts, sizeof(LXBasicVertex_d3d));
            
            if (primitiveType == kLXLineLoop) {
                // TODO: line loop needs closing line element drawn
            }
        }
        
        else if (d3dPrimType == D3DPT_POINTLIST) {
            dxDev->DrawPrimitiveUP(d3dPrimType, vertexCount, (void *)d3dVerts, sizeof(LXBasicVertex_d3d));
        }
        
        else LXPrintf("** whoops: %s, invalid primtype? %i (%i)", __func__, d3dPrimType, primitiveType);
        
    	dxDev->EndScene();
    }
    _lx_free(d3dVerts);
    d3dVerts = NULL;
    
    LXDrawContextFinish_(drawCtx, &drawState);

    EXITRENDERTARGET()

    LXSurfaceUnlockD3DSurface_(r);    
}



LXSuccess LXSurfaceCopyContentsIntoPixelBuffer(LXSurfaceRef r, LXPixelBufferRef pixBuf,
                                                        LXError *outError)
{
    if ( !r) return NO;
    LXSurfaceDXImpl *imp = (LXSurfaceDXImpl *)r;
    IDirect3DDevice9 *dxDev = imp->dxDev;
    if ( !dxDev) return NO;

    IDirect3DSurface9 *surf = LXSurfaceLockD3DSurface_(r);
    if ( !surf) return NO;
    
    LXSuccess success = NO;
    HRESULT hRes;
    IDirect3DTexture9 *systemMemTexture = imp->systemMemTexture;
    IDirect3DSurface9 *systemMemSurface = NULL;
    
    const uint32_t w = imp->w;
    const uint32_t h = imp->h;
    const LXPixelFormat surfPxFormat = (imp->pixelFormat != 0) ? imp->pixelFormat : kLX_ARGB_INT8;
    const D3DFORMAT systemMemFormat = LXD3DFormatFromLXPixelFormat(imp->pixelFormat);
    
    if ( !systemMemTexture) {    
        hRes = dxDev->CreateTexture(w, h, 1, 0, systemMemFormat, D3DPOOL_SYSTEMMEM, &systemMemTexture, NULL);
        if (hRes != D3D_OK) {
            LXPrintf("*** %s: unable to to create system mem texture, can't continue\n", __func__);
            LXErrorSet(outError, 4901, "unable to create Direct3D systemMem texture for readback");
            LXSurfaceUnlockD3DSurface_(r);
            return NO;
        }
        imp->systemMemTexture = systemMemTexture;
    }

    // do readback into texture in main memory
    {
        systemMemTexture->GetSurfaceLevel(0, &systemMemSurface);
        
        hRes = dxDev->GetRenderTargetData(surf, systemMemSurface);
        if (hRes != D3D_OK) {
            const char *errStr = "readback: unknown error";
            switch (hRes) {
                case D3DERR_DRIVERINTERNALERROR:  errStr = "readback: D3D driver internal error";   break;
                case D3DERR_DEVICELOST:  errStr = "readback: D3D device lost";  break;
                case D3DERR_INVALIDCALL:  errStr = "readback: D3D invalid call";  break; 
            }
            LXErrorSet(outError, 4902, errStr);
        }
        else {
            D3DLOCKED_RECT lockRect;
            ZeroMemory(&lockRect, sizeof(D3DLOCKED_RECT));
					
            hRes = systemMemSurface->LockRect(&lockRect, NULL, D3DLOCK_READONLY);
            if (hRes != D3D_OK || !lockRect.pBits) {
                LXErrorSet(outError, 4903, "unable to lock D3D surface data pointer");
            } else {
                uint8_t *data = (uint8_t *)lockRect.pBits;
                size_t rowBytes = lockRect.Pitch;
                
                success = LXPixelBufferWriteDataWithPixelFormatConversion(pixBuf, data, w, h, rowBytes, surfPxFormat, NULL, outError);
		
                systemMemSurface->UnlockRect();
            }
        }
        
        systemMemSurface->Release();
    }
    
    LXSurfaceUnlockD3DSurface_(r);
    return success;
}

LXSuccess LXSurfaceCopyRegionIntoPixelBuffer(LXSurfaceRef r, LXRect region, LXPixelBufferRef pixbuf,
                                                        LXError *outError)
{
    if ( !pixbuf) return NO;
    LXInteger w = LXSurfaceGetWidth(r);
    LXInteger h = LXSurfaceGetHeight(r);
    LXInteger pxf = LXSurfaceGetPixelFormat(r);
    LXPixelBufferRef tempPixbuf = LXPixelBufferCreate(NULL, w, h, pxf, outError);
    if ( !tempPixbuf) return NO;
    
    LXBool ok = LXSurfaceCopyContentsIntoPixelBuffer(r, tempPixbuf, outError);
    if (ok) {
        LXInteger dstPxf = LXPixelBufferGetPixelFormat(pixbuf);
        size_t dstRowBytes = 0;
        uint8_t *dstBuf = LXPixelBufferLockPixels(pixbuf, &dstRowBytes, NULL, outError);
        if (dstBuf) {
            ok = LXPixelBufferGetRegionWithPixelFormatConversion(tempPixbuf, region, dstBuf, dstRowBytes, dstPxf, NULL, outError);
            LXPixelBufferUnlockPixels(pixbuf);
        }
    }
    
    LXPixelBufferRelease(tempPixbuf);
    return ok;
}

