/*
 *  LXSurface.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSURFACE_H_
#define _LXSURFACE_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"
#include "LXPool.h"
#include "LXTextureArray.h"
#include "LXDrawContext.h"


typedef struct {
    LXFloat x;
    LXFloat y;
    LXFloat u;
    LXFloat v;
} LXVertexXYUV;

typedef struct {
    LXFloat x;
    LXFloat y;
    LXFloat z;
    LXFloat w;
} LXVertexXYZW;

typedef struct {
    LXFloat x;
    LXFloat y;
    LXFloat z;
    LXFloat w;
    LXFloat u;
    LXFloat v;
} LXVertexXYZWUV;

// inline utilities for setting xyuv or xyzw vertices
LXINLINE void LXSetVertex4(LXVertexXYZW *pv,  LXFloat x, LXFloat y, LXFloat z, LXFloat w) {
    pv->x = x;  pv->y = y;  pv->z = z;  pv->w = w; }

LXINLINE void LXSetVertex2(LXVertexXYZW *pv,  LXFloat x, LXFloat y) {
    pv->x = x;  pv->y = y;  pv->z = 0.0;  pv->w = 1.0; }


enum {
    kLXTriangleFan = 0,
    kLXTriangleStrip,
    kLXQuads,
    
    kLXPoints = 0x100,
    
    kLXLineStrip = 0x200,
    kLXLineLoop,
};
typedef LXUInteger LXPrimitiveType;


enum {
    kLXVertex_XYUV = 0,
    kLXVertex_XYZW,
    kLXVertex_XYZWUV,
};
typedef LXUInteger LXVertexType;

// flags
enum {
    kLXSurfaceHasZBuffer = 1 << 0
};


#ifdef __cplusplus
extern "C" {
#endif

// utility for easily setting image and texture coords.
// texture coords in Lacefx are normalized -- if you want the entire image, just pass LXUnitRect as the value for "uvRect"
LXEXPORT void LXSetQuadVerticesXYUV(LXVertexXYUV *vertices, LXRect quadRect, LXRect uvRect);


#pragma mark --- LXSurface public API methods ---

LXEXPORT const char *LXSurfaceTypeID(void);

LXEXPORT LXSurfaceRef LXSurfaceCreate(LXPoolRef pool,
                             uint32_t w, uint32_t h, LXPixelFormat pxFormat,
                             uint32_t flags,
                             LXError *outError);

LXEXPORT LXSurfaceRef LXSurfaceRetain(LXSurfaceRef r);
LXEXPORT void LXSurfaceRelease(LXSurfaceRef r);

LXEXPORT LXSize LXSurfaceGetSize(LXSurfaceRef surface);
LXEXPORT uint32_t LXSurfaceGetWidth(LXSurfaceRef surface);
LXEXPORT uint32_t LXSurfaceGetHeight(LXSurfaceRef surface);
LXEXPORT LXRect LXSurfaceGetBounds(LXSurfaceRef surface);
LXEXPORT void LXSurfaceGetQuadVerticesXYUV(LXSurfaceRef surface, LXVertexXYUV *vertices);

// the "image plane region" is metadata that determines final composition of a 2D element on the image plane.
// functions like LXSurfaceCopyTexture will take this into account when drawing
// (but only if the region size is >0 for both the dest surface and the source texture)
LXEXPORT LXRect LXSurfaceGetImagePlaneRegion(LXSurfaceRef surface);
LXEXPORT void LXSurfaceSetImagePlaneRegion(LXSurfaceRef surface, LXRect region);


LXEXPORT LXBool LXSurfaceMatchesSize(LXSurfaceRef surface, LXSize size);

LXEXPORT LXTextureRef LXSurfaceGetTexture(LXSurfaceRef surface);

// this may return 0 in circumstances where the native surface's pixel format can't be determined,
// so check the result value and default to something sensible as necessary
LXEXPORT LXPixelFormat LXSurfaceGetPixelFormat(LXSurfaceRef r);


LXEXPORT void LXSurfaceClear(LXSurfaceRef surface);
LXEXPORT void LXSurfaceClearRegionWithRGBA(LXSurfaceRef r, LXRect rect, LXRGBA c);

// these methods accept either an LXTexture and an LXTextureArray as the texture argument
LXEXPORT void LXSurfaceCopyTexture(LXSurfaceRef surface, LXTextureRef srcTexture, LXDrawContextRef drawCtx);
LXEXPORT void LXSurfaceCopyTextureWithShader(LXSurfaceRef surface, LXTextureRef srcTexture, LXShaderRef pxProg);

LXEXPORT void LXSurfaceDrawPrimitive(LXSurfaceRef surface,
                            LXPrimitiveType primitiveType,
                            void *vertices,
                            LXUInteger vertexCount,
                            LXVertexType vertexType,
                            LXDrawContextRef drawCtx);

// convenience function that takes a texture directly as a parameter,
// so you don't have to put the texture into an LXDrawContext first
LXEXPORT void LXSurfaceDrawTexturedQuad(LXSurfaceRef surface,
                                void *vertices,
                                LXVertexType vertexType,
                                LXTextureRef texture,
                                LXDrawContextRef drawCtx);

// an even more elaborate convenience function
LXEXPORT void LXSurfaceDrawTexturedQuadWithShaderAndTransform(LXSurfaceRef r,
                                    void *vertices,
                                    LXVertexType vertexType,
                                    LXTextureArrayRef textureArray,
                                    LXShaderRef pxProg,
                                    LXTransform3DRef transform);                            


LXEXPORT LXUnidentifiedNativeObj LXSurfaceBeginPlatformNativeDrawing(LXSurfaceRef surface);
LXEXPORT void LXSurfaceEndPlatformNativeDrawing(LXSurfaceRef surface);

LXEXPORT LXUnidentifiedNativeObj LXSurfaceLockPlatformNativeObj(LXSurfaceRef r);
LXEXPORT void LXSurfaceUnlockPlatformNativeObj(LXSurfaceRef r);

// ensures that any open or pending graphics device state is finished
LXEXPORT void LXSurfaceFlush(LXSurfaceRef surface);

// for reading back pixel data from the GPU
LXEXPORT LXSuccess LXSurfaceCopyContentsIntoPixelBuffer(LXSurfaceRef surface, LXPixelBufferRef pixBuf,
                                                        LXError *outError);

LXEXPORT LXSuccess LXSurfaceCopyRegionIntoPixelBuffer(LXSurfaceRef r, LXRect region, LXPixelBufferRef pixBuf,
                                                        LXError *outError);

// determine platform-specific surface flip property; this may be needed when constructing a projection matrix
LXEXPORT LXBool LXSurfaceNativeYAxisIsFlipped(LXSurfaceRef r);


// async readback.
// this exploits CPU/GPU parallelism to do the readback while the CPU continues processing.
// you should call LXSurfacePerformAsyncReadback() once per frame after rendering.
// the returned frame is always one behind.
typedef void *LXSurfaceAsyncReadBufferPtr;

LXEXPORT LXSurfaceAsyncReadBufferPtr LXSurfaceAsyncReadBufferCreateForSurface(LXSurfaceRef r);
LXEXPORT void LXSurfaceAsyncReadBufferDestroy(LXSurfaceAsyncReadBufferPtr aobj);

LXEXPORT LXPixelFormat LXSurfaceAsyncReadBufferGetPixelFormat(LXSurfaceAsyncReadBufferPtr aobj);

LXEXPORT LXSuccess LXSurfacePerformAsyncReadbackIntoPixelBuffer(LXSurfaceRef r, LXSurfaceAsyncReadBufferPtr aobj, LXBool *outHasData,
                                                                LXPixelBufferRef pixBuf,
                                                                LXError *outError);

LXEXPORT LXSuccess LXSurfacePerformAsyncReadback(LXSurfaceRef r, LXSurfaceAsyncReadBufferPtr aobj, LXBool *outHasData,
                                                    uint8_t **outReadbackBuffer, size_t *outReadbackRowBytes,
                                                    LXError *outError);


#ifdef __cplusplus
}
#endif

#endif
