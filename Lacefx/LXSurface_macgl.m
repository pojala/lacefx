/*
 *  LXSurface_macgl.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 29.8.2007.
 *  Copyright 207 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXSurface.h"
#import "LXTexture.h"
#import "LXPixelBuffer.h"
#import "LXPixelBuffer_priv.h"
#import "LXRef_Impl.h"
#import "LXDraw_Impl.h"
#import "LXPool_surface_priv.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>

#import "LXPlatform.h"
#import "LQGLContext.h"


#define DTIME(t_)  double t_ = CFAbsoluteTimeGetCurrent();


// the old drawing method used pbuffers, which was the best choice on OS X 10.4.
// it's now been updated for FBOs (frame buffer objects) on all builds.
#define USE_FBO 1


#if ( !USE_FBO)
 #import "LQGLPbuffer.h"
#endif


extern LXShaderRef LXDrawContextGetShader_(LXDrawContextRef r);


// forward declarations
LXEXPORT NSString *LXSurfaceBuildThreadAccessErrorString(NSDictionary *errorInfo);
LXEXPORT LXSuccess LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(LXUInteger flags, double timeOut, NSString *caller, NSDictionary **errorInfo);

LXBool getGLFormatInfoForLXPixelFormat(LXPixelFormat pxFormat, GLenum *outGLPxf, GLenum *outGLPxfType, size_t *outBPP, LXError *outError);



#define SHAREDCTX  ( (LQGLContext *)LXPlatformSharedNativeGraphicsContext() )
#define CTXTIMEOUT 2.555



static const GLuint NATIVE_GL_RGBA_8888FORMAT =
#ifdef __BIG_ENDIAN__
									GL_UNSIGNED_INT_8_8_8_8;
#else
									GL_UNSIGNED_INT_8_8_8_8_REV;
#endif				

static const GLuint NATIVE_GL_RGBA_8888FORMAT_REV =
#ifdef __BIG_ENDIAN__
									GL_UNSIGNED_INT_8_8_8_8_REV;
#else
									GL_UNSIGNED_INT_8_8_8_8;
#endif



typedef struct {
    LXREF_STRUCT_HEADER
    
    LXPoolRef sourcePool;
    
    int w;
    int h;
    LXPixelFormat pixelFormat;
    LXBool hasZBuffer;
    
    LXTextureRef lxTexture;
    
    // render properties
    LXRect imageRegion;
    
    // backing drawable is either the pbuffer/FBO or an nsview    
    NSOpenGLView *view;
    
#if (USE_FBO)
    GLuint fbo;
    GLuint fboColorTex;
    GLuint fboDepthRBO;
    GLint prevViewport[4];
#else
    LQGLPbuffer *pb;
    LXInteger activeDrawableMode;
#endif
    
} LXSurfaceOSXImpl;


typedef struct {
    // async readback using two PBOs
    GLuint readbackPBOs[2];
    int readbackIndex_vram2main;
    int readbackIndex_surface2vram;
    uint8_t *readbackBuffer;
    unsigned long readbackCount;
    
    int w;
    int h;
    int pixelFormat;
    size_t rowBytes;
} LXSurfaceAsyncReadBuffer;


// platform-independent parts of the class
#define LXSURFACEIMPL LXSurfaceOSXImpl
#include "LXSurface_baseinc.c"



static NSOpenGLView *g_activeNSView = nil;
static LXInteger g_userSharedCtxLockCount = 0;
static NSMutableArray *g_userSharedCtxLockStateStack = nil;



LXLogFuncPtr g_lxSurfaceLogFuncCb = NULL;
void *g_lxSurfaceLogFuncCbUserData = NULL;


// ---- exported functions used by host implementation ----

LXEXPORT void LXSurfaceSetLogCallback(LXLogFuncPtr cb, void *data)
{
    g_lxSurfaceLogFuncCb = cb;
    g_lxSurfaceLogFuncCbUserData = data;
}

NSString *LXBuildLockHolderTraceStringFromNamesAndRefTimes(id holderStack, id refTimeStack)
{
    if ([holderStack count] != [refTimeStack count]) {
        NSLog(@"** %s: stack counts differ (%ld / %ld)", __func__, (long)[holderStack count], (long)[refTimeStack count]);
        return nil;
    }
    DTIME(t0)
    
    NSMutableString *str = [NSMutableString string];
    LXInteger count = [holderStack count];
    LXInteger i;
    for (i = 0; i < count; i++) {
        id name = [holderStack objectAtIndex:i];
        double t = [[refTimeStack objectAtIndex:i] doubleValue];
        double tdiff = t0 - t;
        
        [str appendFormat:@"%@ (%.2fms ago)", name, 1000.0*tdiff];
        
        if (i < count-1)
            [str appendString:@" / "];
    }
    return str;
}

NSString *LXSurfaceBuildThreadAccessErrorString(NSDictionary *errorInfo)
{
    return LXBuildLockHolderTraceStringFromNamesAndRefTimes([errorInfo objectForKey:@"lockHolderStack"], [errorInfo objectForKey:@"lockRefTimeStack"]);
}


#define ENTERSHAREDCTXLOCK(timeout_, caller_, errinfo_)     ((-1 == [SHAREDCTX lockContextWithTimeout:timeout_ caller:caller_ errorInfo:errinfo_]) ? NO : YES)

#define ENTERSHAREDCTXLOCK_NOTIMEOUT(caller_)               [SHAREDCTX lockContextWithTimeout:2.0 caller:caller_ errorInfo:NULL]

#define EXITSHAREDCTXLOCK                                   [SHAREDCTX unlockContext]



#if (USE_FBO)
 #include "LXSurface_macgl_inc_fbo.m"
#else
 #include "LXSurface_macgl_inc_pbuffer.m"
#endif



LXSuccess LXSurfaceBeginAccessOnThreadBeforeTimeIntervalSinceNow_(double timeOut, LXUInteger flags)
{
    return LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(flags, timeOut, @"accessOnThread_timeout", NULL);
}

LXSuccess LXSurfaceBeginAccessOnThread(LXUInteger flags)
{
    return LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(flags, 500.0/1000.0, (flags) ? @"accessOnThread(N)" : @"accessOnThread(0)", NULL);
}



LXSurfaceRef LXSurfaceRetain(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    
    return r;
}


void LXSurfaceStartNSViewDrawing_(LXSurfaceRef r)
{
    ENTERSHAREDCTXLOCK_NOTIMEOUT(@"LXSurface_NSViewDrawing");
    if ( !r) {
        NSLog(@"*** %s: null surface", __func__);
        return;
    }
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    g_activeNSView = imp->view;
}

void LXSurfaceEndNSViewDrawing_(LXSurfaceRef r)
{
    g_activeNSView = nil;
    EXITSHAREDCTXLOCK;
}




// ------------

LXSurfaceRef LXSurfaceCreateFromNSView_(NSOpenGLView *view)
{
    if ( !view) return NULL;
    
    LXSurfaceOSXImpl *imp = _lx_calloc(sizeof(LXSurfaceOSXImpl), 1);
    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);
    
    NSSize size = [view bounds].size;
    if ([view respondsToSelector:@selector(convertRectToBacking:)]) {
        size = [view convertRectToBacking:[view bounds]].size;
    }

    imp->w = size.width;
    imp->h = size.height;
    imp->pixelFormat = 0;
    imp->hasZBuffer = NO;
        
    imp->view = view;
    
    //NSLog(@"created LXSurface %p from view (%@); %i * %i", imp, view, imp->w, imp->h);
    
    return (LXSurfaceRef)imp;
}


LXPixelFormat LXSurfaceGetPixelFormat(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    return imp->pixelFormat;
}


#pragma mark --- native object ---

LXBool LXSurfaceNativeYAxisIsFlipped(LXSurfaceRef r)
{
    if ( !r) return NO;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    return (imp->view) ? YES : NO;
}

LXBool LXSurfaceHasDefaultPixelOrthoProjection(LXSurfaceRef r)
{
    return YES;
}




#pragma mark --- drawing ---

void LXSurfaceClear(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    BEGINSURFACEDRAW
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | ((imp->hasZBuffer) ? GL_DEPTH_BUFFER_BIT : 0));
    
    ENDSURFACEDRAW
}

void LXSurfaceClearRegionWithRGBA(LXSurfaceRef r, LXRect rect, LXRGBA c)
{
    if ( !r) return;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    BEGINSURFACEDRAW
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.x, rect.y,  rect.w, rect.h);
    
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glDisable(GL_SCISSOR_TEST);
    
    ENDSURFACEDRAW
}


#pragma mark --- primitives ---

// private drawContext flags.
// Conduit's ARBfp generation uses texcoord[7] for normalized texcoords,
// so this flag ensures that the texcoord gets passed
enum {
    kLXDrawFlag_PutNormalizedTexCoordsInUnit7 = 1 << 25,                // defined as kLXDrawFlag_private1_ in the public LXDrawContext API
    
    kLXDrawFlag_UseFragmentProgramForBilinearFiltering = 1 << 26,       // defined as kLXDrawFlag_private2_ in the public LXDrawContext API
};


static inline void setGLTexCoords(LXFloat u, LXFloat v, LXInteger texCount, LXTextureArrayRef texArray, LXBool haveNormalizedTexCoordsInUnit7)
{
    if (texCount > 0) {
        int ti;
        for (ti = 0; ti < texCount; ti++) {
            LXSize texSize = LXTextureGetSize( LXTextureArrayAt(texArray, ti) );
            //NSLog(@"  %s -- incoords (%.3f, %.3f) --> (%.3f, %.3f)", __func__, u, v, u*texSize.w, v*texSize.h);
            glMultiTexCoord2f(GL_TEXTURE0 + ti,  u * texSize.w, v * texSize.h);
        }
    } else {
        // if no textures, put normalized texcoords in first and last units
#if defined(LXFLOAT_IS_DOUBLE)
        glMultiTexCoord2d(GL_TEXTURE0, u, v);
#else
        glMultiTexCoord2f(GL_TEXTURE0, u, v);
#endif
    }
    
    if (texCount == 0 || haveNormalizedTexCoordsInUnit7) {
        // if no textures, put normalized texcoords in first and last units
#if defined(LXFLOAT_IS_DOUBLE)
        glMultiTexCoord2d(GL_TEXTURE7, u, v);                    
#else
        glMultiTexCoord2f(GL_TEXTURE7, u, v);
#endif
    }
}


// filtering utility
GLuint LXOpenGL_BilinearFilteringFragmentProgramID()
{
    static GLuint fp = 0;
    if ( !fp) {
		// bilinear filtering in ARBfp, from Pete Warden's GPU Notes blog
		const char blerpStr[] = "!!ARBfp1.0"
			"ATTRIB inputCoords = fragment.texcoord[0];"
			"TEMP sourceCoords;"
			"TEMP sourceCoordsTopLeft, sourceCoordsTopRight, sourceCoordsBottomLeft, sourceCoordsBottomRight, fraction;"
			"TEMP sourceTopLeft,sourceTopRight,sourceBottomLeft,sourceBottomRight;"
			"SUB sourceCoords, inputCoords, {0.5, 0.5, 0.0, 0.0};"
			"FRC fraction.x, sourceCoords.x;"
			"FRC fraction.y, sourceCoords.y;"
			"SUB sourceCoords, sourceCoords, fraction;"
			"ADD sourceCoords, sourceCoords, {0.5, 0.5, 0.0, 0.0};"
			"ADD sourceCoordsTopLeft, sourceCoords, {1,1,0,0};"
			"ADD sourceCoordsTopRight, sourceCoords, {0,1,0,0};"
			"ADD sourceCoordsBottomLeft, sourceCoords, {1,0,0,0};"
			"ADD sourceCoordsBottomRight, sourceCoords, {0,0,0,0};"
			"TEX sourceTopLeft, sourceCoordsTopLeft, texture[0], RECT;"
			"TEX sourceTopRight, sourceCoordsTopRight, texture[0], RECT;"
			"TEX sourceBottomLeft, sourceCoordsBottomLeft, texture[0], RECT;"
			"TEX sourceBottomRight, sourceCoordsBottomRight, texture[0], RECT;"
			"LRP sourceTopLeft, fraction.x, sourceTopLeft, sourceTopRight;"
			"LRP sourceBottomLeft, fraction.x, sourceBottomLeft, sourceBottomRight;"
			"LRP result.color, fraction.y, sourceTopLeft, sourceBottomLeft;"
			"END";
			
		glGenProgramsARB(1, &fp);
		
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp);
        glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(blerpStr), blerpStr);
        
        int err = glGetError();
        if (err != GL_NO_ERROR)
            NSLog(@"** error creating blerp fragment program (%i)", err);
		
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	}
	return fp;
}



void LXSurfaceDrawPrimitive(LXSurfaceRef r,
                            LXPrimitiveType primitiveType,
                            void *vertices,
                            LXUInteger vertexCount,
                            LXVertexType vertexType,
                            LXDrawContextRef drawCtx)
{
    if ( !r || !vertices || vertexCount < 1) return;
    
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    BEGINSURFACEDRAW

    LXDrawContextActiveState *drawState = NULL;
    
    LXDrawContextBeginForSurface_(drawCtx, r, &drawState);
    
    LXUInteger drawFlags = LXDrawContextGetFlags(drawCtx);

    // need textures here because Lacefx uses normalized texcoords
    LXTextureArrayRef texArray = LXDrawContextGetTextureArray(drawCtx);
    LXInteger texCount = LXDrawContextGetTextureCount(drawCtx);

    glColor4f(1, 1, 1, 1);    

    if (drawFlags & kLXDrawFlag_UseHardwareFragmentAntialiasing) {
        if (primitiveType == kLXPoints) {
            glEnable(GL_POINT_SMOOTH);
        } else if (primitiveType == kLXLineStrip || primitiveType == kLXLineLoop) {
            glEnable(GL_LINE_SMOOTH);
        } else {
            glEnable(GL_POLYGON_SMOOTH);
        }
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(drawCtx);
    }    
    else if (drawFlags & kLXDrawFlag_UseFixedFunctionBlending_SourceIsPremult) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        
        LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(drawCtx);
    } 
    else if (drawFlags & kLXDrawFlag_UseFixedFunctionBlending_SourceIsUnpremult) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  GL_SRC_ALPHA, GL_ONE);
        
        LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(drawCtx);
    }
    else {
        glDisable(GL_BLEND);
    }
    
    LXBool useBlerpFP = (drawFlags & kLXDrawFlag_UseFragmentProgramForBilinearFiltering) ? YES : NO;
    
    if ( !useBlerpFP) {
        // if the hardware doesn't support bilinear filtering, we'll use a custom fragment program to do it.
        // enable that mode only if there's a single texture and no shader
        if ( !LXPlatformHWSupportsFilteringForFloatTextures()) {
            if (LXDrawContextGetShader_(drawCtx) == NULL && LXTextureArrayInUseCount(texArray) == 1) {
                useBlerpFP = YES;
            }
        }
    }
    
    if (useBlerpFP) {
        GLenum filteringMode = GL_NEAREST;
        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, filteringMode);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, filteringMode);
        
        ///glDisable(GL_BLEND);
        
        GLuint blerpProgID = LXOpenGL_BilinearFilteringFragmentProgramID();
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, blerpProgID);
        ///printf("   .. enabled blerp FP, %i\n", blerpProgID);
    }
    
    
    const LXBool haveNormalizedTexCoordsInUnit7 = (drawFlags & kLXDrawFlag_PutNormalizedTexCoordsInUnit7) ? YES : NO;

    GLenum glPrimType;
    switch (primitiveType) {
        default:
        case kLXTriangleFan:    glPrimType = GL_TRIANGLE_FAN;  break;
        case kLXTriangleStrip:  glPrimType = GL_TRIANGLE_STRIP;  break;
        case kLXQuads:          glPrimType = GL_QUADS;  break;

        case kLXPoints:         glPrimType = GL_POINTS;  break;

        case kLXLineStrip:      glPrimType = GL_LINE_STRIP;  break;
        case kLXLineLoop:       glPrimType = GL_LINE_LOOP;  break;
    }
    
    // start drawing
    glBegin(glPrimType);
    
    LXUInteger i;
    for (i = 0; i < vertexCount; i++) {
        switch (vertexType) {
        
            case kLXVertex_XYUV: {
                LXVertexXYUV *vp = (LXVertexXYUV *) (((LXFloat *)vertices) + 4*i);
                
                setGLTexCoords(vp->u, vp->v, texCount, texArray, haveNormalizedTexCoordsInUnit7);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex2d(vp->x, vp->y);
                #else
                glVertex2f(vp->x, vp->y);
                #endif
                break;
            }
            
            case kLXVertex_XYZW: {
                LXVertexXYZW *vp = (LXVertexXYZW *) (((LXFloat *)vertices) + 4*i);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex4dv((double *)vp);
                #else
                glVertex4fv((float *)vp);
                //glVertex3f(vp->x, vp->y, vp->z);
                #endif
                break;
            }
            
            case kLXVertex_XYZWUV: {
                LXVertexXYZWUV *vp = (LXVertexXYZWUV *) (((LXFloat *)vertices) + 6*i);
                
                setGLTexCoords(vp->u, vp->v, texCount, texArray, haveNormalizedTexCoordsInUnit7);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex4dv((double *)vp);
                #else
                glVertex4fv((float *)vp);
                #endif
                break;
            }
            
            default:
                NSLog(@"*** unsupported vertex format (%ld)", (long)vertexType);
                break;
        }
    }
    
    glEnd();
    
    if (useBlerpFP) {
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    }

    if (drawFlags != 0) {
        glDisable(GL_BLEND);
        
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POINT_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
        
        glColor4f(1, 1, 1, 1);
    }
        
    LXDrawContextFinish_(drawCtx, &drawState);
    
    ENDSURFACEDRAW
}




#pragma mark --- readback ---

LXBool getGLFormatInfoForLXPixelFormat(LXPixelFormat pxFormat, GLenum *outGLPxf, GLenum *outGLPxfType, size_t *outBPP, LXError *outError)
{
    GLenum glPixelFormat;
    GLenum glPixelFormatType;
    size_t dstBytesPerPixel;
    
    switch (pxFormat) {
        case kLX_RGBA_INT8:
            glPixelFormat = GL_RGBA;
            glPixelFormatType = NATIVE_GL_RGBA_8888FORMAT;
            dstBytesPerPixel = 4;
            break;
            
        case kLX_ARGB_INT8:
            glPixelFormat = GL_BGRA;
            glPixelFormatType = NATIVE_GL_RGBA_8888FORMAT_REV;
            dstBytesPerPixel = 4;
            break;

        case kLX_BGRA_INT8:
            glPixelFormat = GL_BGRA;
            glPixelFormatType = NATIVE_GL_RGBA_8888FORMAT;
            dstBytesPerPixel = 4;
            break;
            
        case kLX_RGBA_FLOAT16:
            glPixelFormat = GL_RGBA;
            glPixelFormatType = GL_HALF_APPLE;
            dstBytesPerPixel = 4*2;
            break;
    
        case kLX_RGBA_FLOAT32:
            glPixelFormat = GL_RGBA;
            glPixelFormatType = GL_FLOAT;
            dstBytesPerPixel = 4*4;
            break;
    
        default:
            NSLog(@"** %s: unsupported pxformat (%ld)", __func__, (long)pxFormat);
            LXErrorSet(outError, 4831, "unsupported pixel format for readback");
            return NO;
    }

    *outGLPxf = glPixelFormat;
    *outGLPxfType = glPixelFormatType;
    *outBPP = dstBytesPerPixel;
    return YES;
}



#pragma mark --- async readback ---


LXSurfaceAsyncReadBufferPtr LXSurfaceAsyncReadBufferCreateForSurface(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceOSXImpl *surf = (LXSurfaceOSXImpl *)r;
    
    LXSurfaceAsyncReadBuffer *asyncBuf = _lx_calloc(sizeof(LXSurfaceAsyncReadBuffer), 1);
    
    if ( !LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(0, 2.0, @"LXSurface_createAsyncReadback", NULL)) {
        NSLog(@"*** %s: unable to get lxLock", __func__);
    } else {
        asyncBuf->w = surf->w;
        asyncBuf->h = surf->h;
        asyncBuf->pixelFormat = surf->pixelFormat;
        asyncBuf->rowBytes = surf->w * LXBytesPerPixelForPixelFormat(surf->pixelFormat);

        // 2010.03.03 -- in 8-bpc, the only fast path for glReadPixels seems to be "BGRA"! (tested on OS X 10.5.8, Geforce 8600)
        if (asyncBuf->pixelFormat == kLX_RGBA_INT8 || asyncBuf->pixelFormat == kLX_ARGB_INT8) {
            asyncBuf->pixelFormat = kLX_BGRA_INT8;
        }
        
        size_t bufSize = asyncBuf->rowBytes * surf->h;
        
        ///NSLog(@"%s (%p): creating buffer with size %i * %i -> %i", __func__, asyncBuf, surf->w, surf->h, bufSize);
        
        glGenBuffers(2, asyncBuf->readbackPBOs);
        
        int i;
        for (i = 0; i < 2; i++) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, asyncBuf->readbackPBOs[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER_ARB, bufSize, NULL, GL_STATIC_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
        
        asyncBuf->readbackIndex_surface2vram = 0;
        asyncBuf->readbackIndex_vram2main = 1;
        
        asyncBuf->readbackBuffer = _lx_calloc(bufSize, 1);
        asyncBuf->readbackCount = 0;
        
        LXSurfaceEndAccessOnThread();
    }
    return asyncBuf;
}

LXPixelFormat LXSurfaceAsyncReadBufferGetPixelFormat(LXSurfaceAsyncReadBufferPtr aobj)
{
    if ( !aobj) return 0;
    LXSurfaceAsyncReadBuffer *asyncBuf = (LXSurfaceAsyncReadBuffer *)aobj;
    return asyncBuf->pixelFormat;
}

void LXSurfaceAsyncReadBufferDestroy(LXSurfaceAsyncReadBufferPtr aobj)
{
    if ( !aobj) return;
    LXSurfaceAsyncReadBuffer *asyncBuf = (LXSurfaceAsyncReadBuffer *)aobj;
    
    if ( !LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(0, 2.0, @"LXSurfaceAsyncReadBuffer_destroy", NULL)) {
        NSLog(@"*** %s: unable to get lxLock", __func__);
    } else {
        ///NSLog(@"%s (%p): destroying readback buffer", __func__, asyncBuf);
        
        glDeleteBuffers(2, asyncBuf->readbackPBOs);
        
        _lx_free(asyncBuf->readbackBuffer);
        asyncBuf->readbackBuffer = NULL;
        
        LXSurfaceEndAccessOnThread();
    }
    _lx_free(asyncBuf);
}
    

static LXBool performAsyncReadbackIntoBuffer(LXSurfaceOSXImpl *surf, LXSurfaceAsyncReadBuffer *asyncBuf,
                                                uint8_t *buffer, size_t rowBytes,
                                                LXError *outError)
{
#if (USE_FBO)
    NSCAssert(surf->fbo, @"no FBO for this surface");
#else
    LQGLPbuffer *pb = surf->pb;
    NSCAssert(pb, @"no GL pbuffer for this surface");
#endif

    if (asyncBuf->w != surf->w || asyncBuf->h != surf->h) {
        NSLog(@"** %s: mismatched buffer size", __func__);
        LXErrorSet(outError, 4832, "mismatched buffer size for readback");
        return NO;
    }

    LXPixelFormat pxFormat = asyncBuf->pixelFormat;    
    GLenum glPixelFormat;
    GLenum glPixelFormatType;
    size_t dstBytesPerPixel;    
    if ( !getGLFormatInfoForLXPixelFormat(pxFormat, &glPixelFormat, &glPixelFormatType, &dstBytesPerPixel, outError))
        return NO;

    ///NSLog(@"%s: readback count %i, %i*%i, pxf %i", __func__, asyncBuf->readbackCount, asyncBuf->w, asyncBuf->h, asyncBuf->pixelFormat);
    
    
#if (USE_FBO)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, surf->fbo);
    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, surf->w, surf->h);
#else
    activatePbuffer(pb);
#endif
    {
        // first, read from surface into PBO 1
        glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, asyncBuf->readbackPBOs[asyncBuf->readbackIndex_surface2vram]);
        
        ///double t0 = CFAbsoluteTimeGetCurrent();
        glReadPixels(0, 0, asyncBuf->w, asyncBuf->h, glPixelFormat, glPixelFormatType, 0);
        
        ///double t1 = CFAbsoluteTimeGetCurrent();
        ///printf("... buffer %i: glReadPixels took %.3f ms\n", asyncBuf->readbackIndex_surface2vram, 1000*(t1-t0));
        
        if (asyncBuf->readbackCount > 0) {
            // next, copy the previously read frame from VRAM into main memory
            glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, asyncBuf->readbackPBOs[asyncBuf->readbackIndex_vram2main]);
            
            ///double t2 = CFAbsoluteTimeGetCurrent();
            void *pboData = glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
            
            ///double t3 = CFAbsoluteTimeGetCurrent();
            if (pboData) {
                if (rowBytes == asyncBuf->rowBytes) {
                    _lx_memcpy_aligned(buffer, pboData, rowBytes * asyncBuf->h);
                } else {
                    const size_t srcRowBytes = asyncBuf->rowBytes;
                    const size_t realRowBytes = MIN(rowBytes, srcRowBytes);
                    const LXInteger h = asyncBuf->h;
                    LXInteger y;
                    for (y = 0; y < h; y++) {
                        _lx_memcpy_aligned(buffer + y*rowBytes,  pboData + y*srcRowBytes,  realRowBytes);
                    }
                }
            } else {
                int err = glGetError();
                NSLog(@"** async readback: glMapBuffer() returned error %i (buffer ID: %i)", err, asyncBuf->readbackPBOs[asyncBuf->readbackIndex_vram2main]);
            }
            ///double t4 = CFAbsoluteTimeGetCurrent();
            
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
            
            //printf("    ... buffer %i: copy took %.3f ms (glMap %.3f, memcpy %.3f); data %p, value 0x%x\n",
            //                asyncBuf->readbackIndex_vram2main, 1000*(t4-t2),
            //                1000*(t3-t2), 1000*(t4-t3), pboData, d);
        }
            
        glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
        
        asyncBuf->readbackIndex_surface2vram = (asyncBuf->readbackIndex_surface2vram == 1) ? 0 : 1;
        asyncBuf->readbackIndex_vram2main    = (asyncBuf->readbackIndex_vram2main == 1) ? 0 : 1;
    }
#if (USE_FBO)
    glPopAttrib();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif

    
    return YES;
}


LXSuccess LXSurfacePerformAsyncReadback(LXSurfaceRef r, LXSurfaceAsyncReadBufferPtr aobj, LXBool *outHasData,
                                        uint8_t **outReadbackBuffer, size_t *outReadbackRowBytes,
                                        LXError *outError)
{
    if ( !r || !aobj) return NO;
    LXSurfaceAsyncReadBuffer *asyncBuf = (LXSurfaceAsyncReadBuffer *)aobj;
    LXSurfaceOSXImpl *surf = (LXSurfaceOSXImpl *)r;
    
    BOOL ok = performAsyncReadbackIntoBuffer(surf, asyncBuf,
                                             asyncBuf->readbackBuffer, asyncBuf->rowBytes,
                                             outError);

    if (ok) {
        if (outReadbackBuffer && outReadbackRowBytes) {
            *outReadbackBuffer = asyncBuf->readbackBuffer;
            *outReadbackRowBytes = asyncBuf->rowBytes;
        }
        if (outHasData) *outHasData = (asyncBuf->readbackCount > 0) ? YES : NO;
        
        asyncBuf->readbackCount++;
    }
    return ok;
}

LXSuccess LXSurfacePerformAsyncReadbackIntoPixelBuffer(LXSurfaceRef r, LXSurfaceAsyncReadBufferPtr aobj, LXBool *outHasData,
                                                                LXPixelBufferRef pixBuf,
                                                                LXError *outError)
{
    if ( !r || !aobj) return NO;
    LXSurfaceAsyncReadBuffer *asyncBuf = (LXSurfaceAsyncReadBuffer *)aobj;
    LXSurfaceOSXImpl *surf = (LXSurfaceOSXImpl *)r;

    if (asyncBuf->w != LXPixelBufferGetWidth(pixBuf) || asyncBuf->h != LXPixelBufferGetHeight(pixBuf)) {
        NSLog(@"** %s: mismatched pixel buffer size", __func__);
        LXErrorSet(outError, 4832, "mismatched pixel buffer size for readback");
        return NO;
    }

    size_t dstRowBytes = 0;
    uint8_t *dstBuf = LXPixelBufferLockPixels(pixBuf, &dstRowBytes, NULL, outError);
    if ( !dstBuf) return NO;
    
    BOOL ok = performAsyncReadbackIntoBuffer(surf, asyncBuf,
                                             dstBuf, dstRowBytes,
                                             outError);

    LXPixelBufferUnlockPixels(pixBuf);

    if (ok) {
        if (outHasData) *outHasData = (asyncBuf->readbackCount > 0) ? YES : NO;
        
        asyncBuf->readbackCount++;
    }
    return ok;
}

