/*
 *  LXTexture_macgl.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXTexture.h"
#include "LXRef_Impl.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>

#import "LXPlatform.h"
#import "LXPlatform_mac.h"
#import "LQGLContext.h"


#define DTIME(t_)  double t_ = CFAbsoluteTimeGetCurrent();


#define USE_FBO 1


#if ( !USE_FBO)
#import "LQGLPbuffer.h"
#endif


#define SHAREDCTX  ( (LQGLContext *)LXPlatformSharedNativeGraphicsContext() )
#define CTXTIMEOUT 3.505


typedef struct {
    LXREF_STRUCT_HEADER
    
    uint32_t w;
    uint32_t h;
    LXPixelFormat pf;

    GLuint texID;
#if (USE_FBO)
    LXSurfaceRef surf; 
#else
    LQGLPbuffer *pb;  // weak reference -- owned by the LXSurface that created us
#endif

    CGLContextObj creationCtx;
    LXBool isWrapper;
    
    // original spec for textures created from data
    void *buffer;
    size_t rowBytes;
    LXUInteger storageHint;

    // render properties
    LXUInteger sampling;
    LXRect imageRegion;
} LXTextureOSXImpl;


// platform-independent parts of the class
#define LXTEXTUREIMPL LXTextureOSXImpl
#include "LXTexture_baseinc.c"


#if ( !USE_FBO)
// implemented in LXSurface_objc.m
extern LXSuccess LXCopyGLPbufferRegionIntoPixelBuffer_(LQGLPbuffer *pb, LXPixelBufferRef pixBuf,
                                                        GLint x, GLint y,
                                                        GLint w, GLint h,
                                                        LXError *outError);
#endif


// redirect context
static CGLContextObj g_cglContext = NULL;
static NSRecursiveLock *g_cglContextLock = NULL;

#define INITCTXLOCK  if ( !g_cglContextLock)  { g_cglContextLock = [[NSRecursiveLock alloc] init]; }
#define ENTERCTXLOCK [g_cglContextLock lock];
#define EXITCTXLOCK  [g_cglContextLock unlock];


// LXTextures can be created in another CGL context than the shared Lacefx context.
// this functionality is used by e.g. PixMathScopesView to render text labels using LQCGBitmap -> LXTexture

void LXTextureBeginRedirectToCGLContext_(CGLContextObj ctx)
{
    INITCTXLOCK
    ENTERCTXLOCK
    g_cglContext = ctx;
}

void LXTextureEndRedirectToCGLContext_(CGLContextObj obj)
{
    g_cglContext = NULL;
    EXITCTXLOCK
}


LXTextureRef LXTextureRetain(LXTextureRef r)
{
    if ( !r) return NULL;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));

    return r;
}

void LXTextureRelease(LXTextureRef r)
{
    if ( !r) return;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        if (imp->texID != 0 && !imp->isWrapper) {
            [SHAREDCTX cancelDeferredUpdatesOfLXRef:r];
        
            if (imp->creationCtx) {
                ENTERCTXLOCK
                CGLContextObj prevCtx = CGLGetCurrentContext();
                if (prevCtx != imp->creationCtx) CGLSetCurrentContext(imp->creationCtx);
                
                //LXDEBUGLOG("releasing non-main context texture %p (GL id %i, in context %p)", imp, imp->texID, imp->creationCtx);
                
                glDeleteTextures(1, &(imp->texID));
                
                if (prevCtx != imp->creationCtx) CGLSetCurrentContext(prevCtx);
                EXITCTXLOCK
            } else {
                //NSLog(@"deferring delete of lx texid %i in main context", imp->texID);
                ///LXDEBUGLOG("deferring texture delete %p (%i)", imp, imp->texID);
                
                [SHAREDCTX deferDeleteOfGLTextureID:imp->texID];
            }
        }
        _lx_free(imp);
    }
}


#if ( !USE_FBO)

LXTextureRef LXTextureCreateWithGLPbuffer_(LQGLPbuffer *pb, LXPixelFormat pxFormat)
{
    if ( !pb)
        return NULL;

    LXTextureOSXImpl *imp = _lx_calloc(sizeof(LXTextureOSXImpl), 1);

    LXREF_INIT(imp, LXTextureTypeID(), LXTextureRetain, LXTextureRelease);
    
    imp->w = [pb size].width;
    imp->h = [pb size].height;
    imp->pf = pxFormat;
    imp->sampling = kLXNearestSampling;
    imp->pb = pb;
    
    return (LXTextureRef)imp;
}
#else

extern GLuint LXSurfaceGetGLColorTexName_(LXSurfaceRef r);

LXTextureRef LXTextureCreateWithSurface_(LXSurfaceRef surf)
{
    if ( !surf)
        return NULL;
        
    LXTextureOSXImpl *imp = _lx_calloc(sizeof(LXTextureOSXImpl), 1);

    LXREF_INIT(imp, LXTextureTypeID(), LXTextureRetain, LXTextureRelease);
    
    imp->w = LXSurfaceGetWidth(surf);
    imp->h = LXSurfaceGetHeight(surf);
    imp->pf = LXSurfaceGetPixelFormat(surf);
    imp->sampling = kLXNearestSampling;
    imp->surf = surf;
    imp->texID = LXSurfaceGetGLColorTexName_(surf);
    imp->isWrapper = YES;
    
    return (LXTextureRef)imp;
}

#endif  // USE_FBO



LXTextureRef LXTextureCreateWithGLTextureInternal_(GLuint texID, int w, int h, LXPixelFormat pxFormat,
                                                         uint8_t *buffer, size_t rowBytes,
                                                         LXUInteger storageHint,
                                                         LXBool isWrapper)
{
    if (texID == 0 || w < 1 || h < 1)
        return NULL;

    LXTextureOSXImpl *imp = _lx_calloc(sizeof(LXTextureOSXImpl), 1);

    LXREF_INIT(imp, LXTextureTypeID(), LXTextureRetain, LXTextureRelease);
    
    imp->w = w;
    imp->h = h;
    imp->texID = texID;
    imp->pf = pxFormat;
    imp->sampling = kLXNearestSampling;
    
    imp->creationCtx = g_cglContext;
    imp->isWrapper = isWrapper;
    
    imp->storageHint = storageHint;
    
    // it's safe to store the data pointer only if client storage was specified
    const BOOL useClientStorage = (storageHint & kLXStorageHint_ClientStorage) ? YES : NO;
    if (useClientStorage) {
        imp->buffer = buffer;
        imp->rowBytes = rowBytes;
    }
    
    return (LXTextureRef)imp;
}

LXTextureRef LXTextureCreateWithGLTexture_(GLuint texID, int w, int h, LXPixelFormat pxFormat,
                                                         uint8_t *buffer, size_t rowBytes,
                                                         LXUInteger storageHint)
{
    return LXTextureCreateWithGLTextureInternal_(texID, w, h, pxFormat, buffer, rowBytes, storageHint, NO);
}

LXTextureRef LXTextureCreateWrapperForGLTexture_(unsigned int texID, int w, int h, LXPixelFormat pxFormat)
{
    return LXTextureCreateWithGLTextureInternal_((GLuint)texID, w, h, pxFormat, NULL, 0, 0, YES);
}



static LXSuccess getGLTexFormatFromLXPixelFormat(LXPixelFormat pxFormat, GLuint *outTexFormat, GLuint *outTexInternalFormat, GLuint *outTexType, int *outBytesPerPixel)
{
    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;
    
    switch (pxFormat) {
        case kLX_Luminance_INT8:
            bytesPerPixel = 1;
            texFormat = GL_LUMINANCE;
            texInternalFormat = GL_LUMINANCE;
            texType = GL_UNSIGNED_BYTE;
            break;
    
        case kLX_RGBA_INT8:
			bytesPerPixel = 4;
            texFormat = GL_RGBA;
            texInternalFormat = GL_RGBA;
            #ifdef __BIG_ENDIAN__
            texType = GL_UNSIGNED_INT_8_8_8_8;
            #else
            texType = GL_UNSIGNED_INT_8_8_8_8_REV;
            #endif
            break;

        case kLX_ARGB_INT8:
			bytesPerPixel = 4;
            texFormat = GL_BGRA;
            texInternalFormat = GL_RGBA;
            #ifdef __BIG_ENDIAN__
            texType = GL_UNSIGNED_INT_8_8_8_8_REV;
            #else
            texType = GL_UNSIGNED_INT_8_8_8_8;
            #endif
            break;
        
        case kLX_BGRA_INT8:
			bytesPerPixel = 4;
            texFormat = GL_BGRA;
            texInternalFormat = GL_RGBA;
            #ifdef __BIG_ENDIAN__
            texType = GL_UNSIGNED_INT_8_8_8_8;
            #else
            texType = GL_UNSIGNED_INT_8_8_8_8_REV;
            #endif
            break;
        
        case kLX_YCbCr422_INT8:
			bytesPerPixel = 2;
            texFormat = GL_YCBCR_422_APPLE;
            texInternalFormat = GL_RGBA;
			#ifdef __BIG_ENDIAN__
            texType = GL_UNSIGNED_SHORT_8_8_REV_APPLE;
			#else
			texType = GL_UNSIGNED_SHORT_8_8_APPLE;
			#endif
            break;
			
        case kLX_RGBA_FLOAT32:
            bytesPerPixel = 16;
            texFormat = GL_RGBA;
            texInternalFormat = GL_RGBA_FLOAT32_APPLE;
            texType = GL_FLOAT;
            break;
        
        case kLX_RGBA_FLOAT16:
            bytesPerPixel = 8;
            texFormat = GL_RGBA;
            texInternalFormat = GL_RGBA_FLOAT16_APPLE;
            texType = GL_HALF_APPLE;
            break;
            
        case kLX_Luminance_FLOAT32:
            bytesPerPixel = 4;
            texFormat = GL_LUMINANCE;
            texInternalFormat = GL_LUMINANCE_FLOAT32_APPLE;
            texType = GL_FLOAT;
            break;
        
        case kLX_Luminance_FLOAT16:
            bytesPerPixel = 2;
            texFormat = GL_LUMINANCE;
            texInternalFormat = GL_LUMINANCE_FLOAT16_APPLE;
            texType = GL_HALF_APPLE;
            break;
        
        default:
            NSLog(@"*** %s: pixel format not yet supported (%i)", __func__, (int)pxFormat);
            return NO;
    }

    *outTexFormat = texFormat;
    *outTexInternalFormat = texInternalFormat;
    *outTexType = texType;
    *outBytesPerPixel = bytesPerPixel;
    return YES;
}


LXTextureRef LXTextureCreateWithData(uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint8_t *buffer, size_t rowBytes,
                                                    LXUInteger storageHint,
                                                    LXError *outError)
{
    const BOOL preferCaching = (storageHint & kLXStorageHint_PreferCaching) ? YES : NO;
    const BOOL useClientStorage = preferCaching ? NO : ((storageHint & kLXStorageHint_ClientStorage) ? YES : NO);
    const BOOL useDMATexturing =  preferCaching ? NO : ((storageHint & kLXStorageHint_PreferDMAToCaching) ? YES : NO);
    GLuint texID;
    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;

    if ( !getGLTexFormatFromLXPixelFormat(pxFormat, &texFormat, &texInternalFormat, &texType, &bytesPerPixel)) {
        printf("** can't create texture, invalid pxformat\n");
        return NULL;
    }

	GLuint textureHint = (useDMATexturing) ? GL_STORAGE_SHARED_APPLE : GL_STORAGE_CACHED_APPLE;   // cached == VRAM, shared == AGP texturing
	GLuint clientStorage = (useClientStorage) ? GL_TRUE : GL_FALSE;
	GLfloat texturePriority = (useDMATexturing) ? 0.0 : 1.0;    // traditionally on Mac OS X, texture priority of 0.0 combined with client storage worked as an "AGP hint"

    LXTextureRef retObj = NULL;
    INITCTXLOCK
    ENTERCTXLOCK

    if ( !g_cglContext) {
        if ( ![SHAREDCTX activateWithTimeout:CTXTIMEOUT caller:__nsfunc__]) {
            LXPlatformLogInfo([NSString stringWithFormat:@"** %s: couldn't lock main GL context, unable to continue", __func__]);
            goto bail;
        }
    } else {
        ///NSLog(@"%s: creating in non-main context %p, buffer %p", __func__, g_cglContext, buffer);
    }

    // clear any previous error
    glGetError();
    
	glGenTextures(1, &texID);
    
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_EXT);

	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, texID);

    GLint prevTexStorageHint = 0;
    glGetTexParameteriv(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, &prevTexStorageHint);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, textureHint);
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, texturePriority);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, clientStorage);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowBytes / bytesPerPixel);
    
    GLint prevAlign = 1;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    
    if (preferCaching) {
        //NSLog(@"creating cached texture: %i * %i, rowbytes %i, bpp %i, clientstorage %i, priority %.1f; alignment was %i",
        //            w, h, rowBytes, bytesPerPixel, clientStorage, texturePriority, prevAlign);
    }
    
	glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, texInternalFormat,
								w, h, 0,
								texFormat, texType, buffer);

	int err = glGetError();
    
	if (err != GL_NO_ERROR) {
		LXPlatformLogInfo([NSString stringWithFormat:@"** %s: error loading texture (error %i; internalformat %i, type %i)", __func__, (int)err, texInternalFormat, texType]);
        glDeleteTextures(1, &texID);
    } else {
        ///NSLog(@"%s: %i * %i, storage hint is %u", __func__, w, h, storageHint);
        retObj = LXTextureCreateWithGLTexture_(texID, w, h, pxFormat, buffer, rowBytes, storageHint);
    }
	
	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);	
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
    if (texturePriority != 1.0) glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, 1.0);
    if (prevTexStorageHint != 0) glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, prevTexStorageHint);

    if ( !g_cglContext)    
        [SHAREDCTX deactivate];

bail:
    EXITCTXLOCK
    return retObj;
}

void _LXTextureRefreshWarnAboutLockFailure()
{
    LXPlatformLogInfo([NSString stringWithFormat:@"** texture refresh: couldn't lock main GL context, unable to continue (break on %s to debug)", __func__]);
}

typedef struct {
    uint8_t *buffer;
    size_t rowBytes;
    LXUInteger storageHint;
} LXTextureRefreshInfo;

static void LXTextureReallyRefreshGLInsideContextLock_(LXTextureRef r, LXTextureRefreshInfo *info)
{
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;
    if ( !getGLTexFormatFromLXPixelFormat(imp->pf, &texFormat, &texInternalFormat, &texType, &bytesPerPixel)) {
        printf("** can't refresh texture, invalid pxformat (%i)\n", (int)imp->pf);
        return;
    }

    const BOOL useClientStorage = (info->storageHint & kLXStorageHint_ClientStorage) ? YES : NO;
    const BOOL useDMATexturing =  (info->storageHint & kLXStorageHint_PreferDMAToCaching) ? YES : NO;
    GLuint textureHint = (useDMATexturing) ? GL_STORAGE_SHARED_APPLE : GL_STORAGE_CACHED_APPLE;   // cached == VRAM, shared == AGP texturing
    GLuint clientStorage = (useClientStorage) ? GL_TRUE : GL_FALSE;
    GLfloat texturePriority = (useDMATexturing) ? 0.0 : 1.0;

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, imp->texID);

    GLint prevTexStorageHint = 0;
    glGetTexParameteriv(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, &prevTexStorageHint);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, textureHint);
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, texturePriority);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, clientStorage);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, info->rowBytes / bytesPerPixel);

    glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, imp->w, imp->h,
                    texFormat, texType, info->buffer);
                    
    //DTIME(t3)
    //LXPrintf("texture refresh (%i * %i): lock %.3f ms, texsubimage %.3f ms\n", imp->w, imp->h, 1000*(t2-t1), 1000*(t3-t2));
                    
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
    if (texturePriority != 1.0) glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, 1.0);
    if (prevTexStorageHint != 0) glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, prevTexStorageHint);
}

static void textureDeferredUpdateCallback(LXRef object, void *userData)
{
    LXTextureRef r = (LXTextureRef)object;
    LXTextureRefreshInfo *info = (LXTextureRefreshInfo *)userData;
    
    LXTextureReallyRefreshGLInsideContextLock_(r, info);
    //NSLog(@"performed deferred texture update (%p)", r);
    
    // no need to free userData here, it was declared as malloced so the caller will clean it up
}

static LXSuccess LXTextureReallyRefreshGL_(LXTextureRef r, uint8_t *buffer, size_t rowBytes, unsigned int storageHint)
{
    if ( !r) return NO;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;

    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;
    if ( !getGLTexFormatFromLXPixelFormat(imp->pf, &texFormat, &texInternalFormat, &texType, &bytesPerPixel)) {
        printf("** can't refresh texture, invalid pxformat (%i)\n", (int)imp->pf);
        return NO;
    }

    ///NSLog(@"refreshing texid %i, buf %p, bpp %i", imp->texID, buffer, bytesPerPixel);

    LXTextureRefreshInfo info;
    info.buffer = buffer;
    info.rowBytes = rowBytes;
    info.storageHint = storageHint;
    
	if (imp->texID != 0 && buffer && texFormat && bytesPerPixel) {	
        CGLContextObj prevCtx = NULL;
        //DTIME(t1)

        if (imp->creationCtx) {
            prevCtx = CGLGetCurrentContext();
            CGLSetCurrentContext(imp->creationCtx);
        }
        
        // 2011.07.06 -- instead of waiting for lock, just try it briefly, and then defer if the lock can't be acquired
        else if ( ![SHAREDCTX activateWithTimeout:1.0/1000.0 caller:__nsfunc__]) {
            void *userData = _lx_malloc(sizeof(LXTextureRefreshInfo));
            memcpy(userData, &info, sizeof(LXTextureRefreshInfo));
            [SHAREDCTX deferUpdateOfLXRef:r callback:textureDeferredUpdateCallback userData:userData isMalloced:YES];
            //NSLog(@"%s -- texture update was deferred (%p, data %p)", __func__, r, userData);
            return YES;
        }
        
        //DTIME(t2)
        
        LXTextureReallyRefreshGLInsideContextLock_(r, &info);

        if (imp->creationCtx)
            CGLSetCurrentContext(prevCtx);
        else
            [SHAREDCTX deactivate];
        return YES;
	}
    else return NO;
}



enum {
    kLXStorageHint_PostponeTextureUpdates = 1 << 29,
};


LXSuccess LXTextureWillModifyData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    const LXBool clientStorage = (storageHint & kLXStorageHint_ClientStorage) ? YES : NO;
    const LXBool postpone = (storageHint & kLXStorageHint_PostponeTextureUpdates) ? YES : NO;
    
    // cancel any pending deferred updates for our buffer, so it doesn't accidentally happen multiple times
    [SHAREDCTX cancelDeferredUpdatesOfLXRef:r];
    
    if (clientStorage && !postpone) {
        return LXTextureReallyRefreshGL_(r, buffer, rowBytes, storageHint);        
    }
    return YES;
}

LXSuccess LXTextureRefreshWithData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    const LXBool clientStorage = (storageHint & kLXStorageHint_ClientStorage) ? YES : NO;
    const LXBool postpone = (storageHint & kLXStorageHint_PostponeTextureUpdates) ? YES : NO;

    if ( !clientStorage || postpone) {
        return LXTextureReallyRefreshGL_(r, buffer, rowBytes, storageHint);
    }
    return YES;
}



LXSuccess LXTextureCopyContentsIntoPixelBuffer(LXTextureRef r, LXPixelBufferRef pixBuf,
                                                                     LXError *outError)
{
    if ( !r) return NO;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;

#if ( !USE_FBO)
    if (imp->pb) {
        return LXCopyGLPbufferRegionIntoPixelBuffer_(imp->pb, pixBuf, 0, 0, LXPixelBufferGetWidth(pixBuf), LXPixelBufferGetHeight(pixBuf), outError);
    }
#else
    if (imp->surf) {
        return LXSurfaceCopyContentsIntoPixelBuffer(imp->surf, pixBuf, outError);
    }
#endif
    else {
        if (imp->buffer && imp->rowBytes && imp->pf) {
            return LXPixelBufferWriteDataWithPixelFormatConversion(pixBuf,
                                                                  (unsigned char *)imp->buffer,
                                                                  imp->w, imp->h,
                                                                  imp->rowBytes,
                                                                  imp->pf,
                                                                  NULL, outError);
        }
        else {
            NSLog(@"** %s: can't copy, original buffer is unavailable (%p, rb %i, pf %i)", __func__, imp->buffer, (int)imp->rowBytes, (int)imp->pf);
            LXErrorSet(outError, 4891, "no buffer available for texture copy");
            return NO;
        }
    }
}


#pragma mark --- implementation-specific accessors ---

#if ( !USE_FBO)
LQGLPbuffer *LXTextureGetGLPbuffer_(LXTextureRef r)
{
    if ( !r) return nil;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    
    return imp->pb;
}
#else
id LXTextureGetGLPbuffer_(LXTextureRef r)
{
    return nil;
}
#endif


GLuint LXTextureGetGLTexName_(LXTextureRef r)
{
    if ( !r) return 0;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    
    return imp->texID;
}


LXUnidentifiedNativeObj LXTextureLockPlatformNativeObj(LXTextureRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)NULL;
    LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    
    ///imp->retCount++;
    
    return (LXUnidentifiedNativeObj)((unsigned long)imp->texID);
}

void LXTextureUnlockPlatformNativeObj(LXTextureRef r)
{
    ///if ( !r) return;
    ///LXTextureOSXImpl *imp = (LXTextureOSXImpl *)r;
    
    ///imp->retCount--;
}

