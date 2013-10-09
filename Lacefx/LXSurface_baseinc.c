/*
 *  LXSurface_baseinc.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 4.8.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

/*
 To be included in platform-specific implementation files;
 do not compile
 */


#include "LXTextureArray.h"


const char *LXSurfaceTypeID()
{
    static const char *s = "LXSurface";
    return s;
}


LXSize LXSurfaceGetSize(LXSurfaceRef r)
{
    if ( !r) return LXMakeSize(0, 0);
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return LXMakeSize(imp->w, imp->h);
}

uint32_t LXSurfaceGetWidth(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return imp->w;
}

uint32_t LXSurfaceGetHeight(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return imp->h;
}


LXBool LXSurfaceMatchesSize(LXSurfaceRef r, LXSize size)
{
    if ( !r) return NO;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return (imp->w == size.w && imp->h == size.h) ? YES : NO;
}

void LXSurfaceGetQuadVerticesXYUV(LXSurfaceRef r, LXVertexXYUV *vertices)
{
    if ( !r || !vertices)
        return;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    LXSetQuadVerticesXYUV(vertices, LXMakeRect(0, 0, imp->w, imp->h),
                                    LXMakeRect(0, 0, 1, 1) );
}

LXRect LXSurfaceGetBounds(LXSurfaceRef r)
{
    if ( !r) return LXZeroRect;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return LXMakeRect(0, 0, imp->w, imp->h);
}

LXRect LXSurfaceGetImagePlaneRegion(LXSurfaceRef r)
{
    if ( !r) return LXZeroRect;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    return imp->imageRegion;
}

void LXSurfaceSetImagePlaneRegion(LXSurfaceRef r, LXRect region)
{
    if ( !r) return;
    LXSURFACEIMPL *imp = (LXSURFACEIMPL *)r;
    
    imp->imageRegion = region;
    
    if (imp->lxTexture) {
        LXTextureSetImagePlaneRegion(imp->lxTexture, region);
    }
}


void LXSurfaceCopyTexture(LXSurfaceRef dst, LXTextureRef texture, LXDrawContextRef drawCtx)
{
    LXVertexXYUV vertices[4];    
    LXSurfaceGetQuadVerticesXYUV(dst, vertices);
    
    LXTextureArrayRef texArray;
    LXTextureRef firstTexture;
    if (texture && LXRefGetType(texture) == LXTextureArrayTypeID()) {
        texArray = (LXTextureArrayRef)LXRefRetain(texture);
        firstTexture = LXTextureArrayAt(texArray, 0);
    } else {
        texArray = (texture) ? LXTextureArrayCreateWithTexture(texture) : NULL;
        firstTexture = texture;
    }
    
    // if the source texture is same size as dst surface, we can specify nearest sampling
    ///LXInteger prevSamplingMode = -1;
    if (LXSurfaceMatchesSize(dst, LXTextureGetSize(firstTexture))) {
        ///prevSamplingMode = LXTextureGetSampling(srcTexture);
        ///LXTextureSetSampling(srcTexture, kLXNearestSampling);
        LXTextureArraySetSamplingAt(texArray, 0, kLXNearestSampling);
    }

    if (drawCtx && texArray)  LXDrawContextSetTextureArray(drawCtx, texArray);
        
    LXSurfaceDrawTexturedQuad(dst, vertices, kLXVertex_XYUV, texArray, drawCtx);

    if (drawCtx && texArray)  LXDrawContextSetTextureArray(drawCtx, NULL);
        
    LXTextureArrayRelease(texArray);
    
    ///if (prevSamplingMode != -1 && prevSamplingMode != kLXNearestSampling) {
    ///    LXTextureSetSampling(srcTexture, prevSamplingMode);
    ///}
}

void LXSurfaceCopyTextureWithShader(LXSurfaceRef dst, LXTextureRef texture, LXShaderRef pxProg)
{
    LXVertexXYUV vertices[4];    
    LXSurfaceGetQuadVerticesXYUV(dst, vertices);

    LXTextureArrayRef texArray;
    LXTextureRef firstTexture;
    if (texture && LXRefGetType(texture) == LXTextureArrayTypeID()) {
        texArray = (LXTextureArrayRef)LXRefRetain(texture);
        firstTexture = LXTextureArrayAt(texArray, 0);
    } else {
        texArray = (texture) ? LXTextureArrayCreateWithTexture(texture) : NULL;
        firstTexture = texture;
    }

    // if the source texture is same size as dst surface, we can specify nearest sampling
    ///LXInteger prevSamplingMode = -1;
    if (LXSurfaceMatchesSize(dst, LXTextureGetSize(firstTexture))) {
        ///prevSamplingMode = LXTextureGetSampling(srcTexture);
        ///LXTextureSetSampling(srcTexture, kLXNearestSampling);
        LXTextureArraySetSamplingAt(texArray, 0, kLXNearestSampling);
    }
    ///LXTextureArrayRef texArray = (srcTexture) ? LXTextureArrayCreateWithTexture(srcTexture) : NULL;

    LXSurfaceDrawTexturedQuadWithShaderAndTransform(dst, vertices, kLXVertex_XYUV, texArray, pxProg, NULL);
    
    LXRefRelease(texArray);
    
    ///if (prevSamplingMode != -1 && prevSamplingMode != kLXNearestSampling) {
    ///    LXTextureSetSampling(srcTexture, prevSamplingMode);
    ///}
}

void LXSurfaceDrawTexturedQuad(LXSurfaceRef r,
                                    void *vertices,
                                    LXVertexType vertexType,
                                    LXTextureRef texture,
                                    LXDrawContextRef drawCtx)
{
    if ( !drawCtx) {
        LXSurfaceDrawTexturedQuadWithShaderAndTransform(r, vertices, vertexType, texture, NULL, NULL);
    } else {
        LXSurfaceDrawPrimitive(r, kLXQuads, vertices, 4, vertexType, drawCtx);
    }
}


void LXSurfaceDrawTexturedQuadWithShaderAndTransform(LXSurfaceRef r,
                                    void *vertices,
                                    LXVertexType vertexType,
                                    LXTextureArrayRef inTex,
                                    LXShaderRef shader,
                                    LXTransform3DRef transform)
{
    LXTextureArrayRef texArray;
    if (inTex && LXRefGetType(inTex) == LXTextureArrayTypeID()) {
        texArray = LXTextureArrayRetain(inTex);
    } else {
        texArray = (inTex) ? LXTextureArrayCreateWithTexture(inTex) : NULL;
    }

    LXDrawContextRef drawCtx = LXDrawContextCreate();
    
    if (texArray) {
        LXDrawContextSetTextureArray(drawCtx, texArray);
    }
    if (shader) {
        LXDrawContextSetShader(drawCtx, shader);
        LXDrawContextCopyParametersFromShader(drawCtx, shader);
    }
    if (transform) {
        LXDrawContextSetModelViewTransform(drawCtx, transform);
    }
    
    if (transform && !shader) {
        LXDrawContextSetFlags(drawCtx, kLXDrawFlag_UseFixedFunctionBlending_SourceIsPremult);
    }
    
    LXSurfaceDrawPrimitive(r, kLXQuads, vertices, 4,
                              vertexType, drawCtx);
    
    
    LXDrawContextRelease(drawCtx);
        
    LXTextureArrayRelease(texArray);
}

