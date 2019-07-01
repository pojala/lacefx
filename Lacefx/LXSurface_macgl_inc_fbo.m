/*
 *  LXSurface_macgl_fbo.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/6/10.
 *  Copyright 2010 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#define BEGINSURFACEDRAW \
    GLint prevViewport_[4]; \
    glGetIntegerv(GL_VIEWPORT, prevViewport_); \
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, imp->fbo); \
    glViewport(0, 0, imp->w, imp->h);


#define ENDSURFACEDRAW \
    glViewport(prevViewport_[0], prevViewport_[1], prevViewport_[2], prevViewport_[3]); \
    if (imp->fbo) glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);



static BOOL g_didEnterGLContextForFBO = NO;
static NSString *g_callerForGLContext = nil;


LXSuccess LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(LXUInteger flags, double timeOut, NSString *caller, NSDictionary **errorInfo)
{
    // this call acquires the lock but doesn't change the current context
    if ( !ENTERSHAREDCTXLOCK(timeOut, caller, errorInfo)) {
        return NO;
    }
    
    if ( !g_userSharedCtxLockStateStack)
        g_userSharedCtxLockStateStack = [[NSMutableArray alloc] initWithCapacity:16];
    
    
    // if there's an active context already, we don't need to actually switch to the shared context,
    // this context will do, so enable the flag to that effect
    if (g_activeNSView) {
        flags |= kLXDoNotSwitchContextOnLock;
        ///NSLog(@"%s: no need to activate shared ctx (view %p)", __func__, g_activeNSView);
    }
#if 1
    else if ( !g_didEnterGLContextForFBO) {
        // the 'do not switch GL context' flag is being passed by Conduit Live in ways that don't really make sense (circumventing some old bug, I think).
        // if we haven't actually entered the context yet, clear out this flag.
        flags &= ~kLXDoNotSwitchContextOnLock; 
        ///NSLog(@"lxbeginaccess: will switch context despite flag ('%@')", caller);
    }
#endif
    
    if (flags & kLXDoNotSwitchContextOnLock) {
        g_userSharedCtxLockCount++;
        [g_userSharedCtxLockStateStack addObject:[NSNumber numberWithUnsignedLong:flags]];
        ///NSLog(@"lxbeginaccess: will not switch context, flags %ld, lock count %ld, '%@'", flags, g_userSharedCtxLockCount, caller);
        return YES;
    }
    else if (g_activeNSView) {
        g_userSharedCtxLockCount++;
        [g_userSharedCtxLockStateStack addObject:[NSNumber numberWithUnsignedLong:1]];
        ///NSLog(@"lxbeginaccess: is in view, flags %ld, lock count %ld, '%@'", flags, g_userSharedCtxLockCount, caller);
        return YES;
    }
    
    else {
        if ( ![SHAREDCTX activateWithTimeout:timeOut caller:(caller) ? caller : __nsfunc__]) {
            EXITSHAREDCTXLOCK;
            return NO;
        } else {
            g_userSharedCtxLockCount++;
            [g_userSharedCtxLockStateStack addObject:[NSNumber numberWithUnsignedLong:flags]];
            ///NSLog(@"lxbeginaccess: did enter context, flags %ld, lock count %ld, '%@'", flags, g_userSharedCtxLockCount, caller);
            
            g_didEnterGLContextForFBO = YES;
            [g_callerForGLContext release];
            g_callerForGLContext = [caller retain];
            
            return YES;
        }
    }
}


void LXSurfaceEndAccessOnThread()
{
    NSCAssert1(g_userSharedCtxLockCount > 0, @"invalid lock count (%ld)",  (long)g_userSharedCtxLockCount);
    
    g_userSharedCtxLockCount--;
    LXUInteger lastLockFlags = [[g_userSharedCtxLockStateStack lastObject] unsignedLongValue];
    BOOL didActivateSharedCGLOnLock = (lastLockFlags & 1) ? NO : YES;
    
    [g_userSharedCtxLockStateStack removeLastObject];
    
    if (g_didEnterGLContextForFBO && g_userSharedCtxLockCount < 1) {
        g_didEnterGLContextForFBO = NO;
        ///NSLog(@"%s -- returning from context access", __func__);
    }

    ///NSLog(@"%s: last lock flags %ld, lockCount %ld (activePbuf %p)", __func__, lastLockFlags, g_userSharedCtxLockCount, g_activePbuffer);

    if ( !didActivateSharedCGLOnLock) {
        // nothing to do
    } else {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    
        //DTIME(t1)
        glFlush();
        //DTIME(t2)
        
        /*if (1000*(t2-t1) > 2.0) {
            printf("lxendaccessonthread: flush took %.3f ms (caller '%s')\n", 1000*(t2-t1), [g_callerForGLContext UTF8String]);
        }*/
        
        [SHAREDCTX deactivate];
    }
    EXITSHAREDCTXLOCK;
}

void LXSurfaceFlush(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    #pragma unused(imp)

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}



void LXSurfaceRelease(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);
        
        NSDictionary *lockErrorDict = nil;
        LXSuccess didLock = LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(0, 40.0/1000.0, @"LXSurfaceRelease", &lockErrorDict);
        
        ///BOOL didLock = [SHAREDCTX activateWithTimeout:0.02 caller:@"LXSurfaceRelease"];
        
        if ( !didLock) {
            NSString *traceStr = LXSurfaceBuildThreadAccessErrorString(lockErrorDict);
            
            if (imp->fboColorTex)  [SHAREDCTX deferDeleteOfGLTextureID:imp->fboColorTex];
            if (imp->fbo)          [SHAREDCTX deferDeleteOfGLFrameBufferID:imp->fbo];
            imp->fbo = 0;
            imp->fboColorTex = 0;
            
            NSLog(@"%s: no ctx lock, deferred (texture %i, fbo %i, rbo %i); lock trace: %s\n",
                            __func__, (int)imp->fboColorTex, (int)imp->fbo, (int)imp->fboDepthRBO, [traceStr UTF8String]);
        }
        
        if (imp->lxTexture) {
            LXTextureRelease(imp->lxTexture);
            imp->lxTexture = NULL;
        }
            
        ///NSLog(@"%s, %p: fbo %i, texture %i, size %i * %i", __func__, r, imp->fbo, imp->fboColorTex, imp->w, imp->h);

        if (didLock && imp->fbo) {
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            
            if (imp->fboColorTex) {
                glDeleteTextures(1, &(imp->fboColorTex));
                imp->fboColorTex = 0;
            }
            if (imp->fboDepthRBO) {
                glDeleteRenderbuffersEXT(1, &(imp->fboDepthRBO));
                imp->fboDepthRBO = 0;
            }
            glDeleteFramebuffersEXT(1, &(imp->fbo));
            imp->fbo = 0;
        }
        
        if (didLock) {
            //EXITSHAREDCTXLOCK;
            //[SHAREDCTX deactivate];
            LXSurfaceEndAccessOnThread();
        }

        _lx_free(imp);
    }
}

static volatile int64_t s_surfaceCreateCount = 0;

LXSurfaceRef LXSurfaceCreate(LXPoolRef pool,
                             uint32_t w, uint32_t h, LXPixelFormat pxFormat,
                             uint32_t flags,
                             LXError *outError)
{
    LXBool hasZBuffer = (flags & kLXSurfaceHasZBuffer) ? YES : NO;
    
    if (hasZBuffer) {
        NSLog(@"*** warning: zbuffer not supported yet with LXSurface FBO impl");
    }

    if (w < 1 || h < 1) {
        LXErrorSet(outError, 4001, "width or height is zero");
        return NULL;
    }

    int bitsPerPixel = 0;
    switch (pxFormat) {
        case kLX_RGBA_INT8:       bitsPerPixel = 32;  break;
        case kLX_RGBA_FLOAT16:    bitsPerPixel = 64;  break;
        case kLX_RGBA_FLOAT32:    bitsPerPixel = 128;  break;
    }

    if (bitsPerPixel == 0) {
        char str[256];
        sprintf(str, "unsupported pixel format (%i)", (int)pxFormat);
        LXErrorSet(outError, 4002, str);
        return NULL;
    }

    if ( ![SHAREDCTX activateWithTimeout:CTXTIMEOUT caller:__nsfunc__]) {
        LXErrorSet(outError, 4099, "couldn't lock main GL context");
        return NULL;
    }
    
    LXSurfaceOSXImpl *imp = _lx_calloc(sizeof(LXSurfaceOSXImpl), 1);
    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);
    
    imp->w = w;
    imp->h = h;
    
    glGenFramebuffersEXT(1, &(imp->fbo));
    
    // setup texture attachment for fbo
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, imp->fbo);
    
    glGenTextures(1, &(imp->fboColorTex));
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, imp->fboColorTex);
    
    // for debugging
    int64_t numSurfacesTotal = OSAtomicIncrement64(&s_surfaceCreateCount);
    
    if (g_lxSurfaceLogFuncCb) {
        char text[512];
        snprintf(text, 512, "%s: %p (fbo %ld, tex %ld), total %ld", __func__, imp, (long)imp->fbo, (long)imp->fboColorTex, (long)numSurfacesTotal);
        g_lxSurfaceLogFuncCb(text, g_lxSurfaceLogFuncCbUserData);
    }

    GLenum glPxf = 0;
    GLenum glPxfType = 0;
    GLenum glPxfComp = 0;
    switch (pxFormat) {
        default:
            NSLog(@"*** %s: unsupported pixel format (%d), will default to RGBA_INT8", __func__, (int)pxFormat);
            pxFormat = kLX_RGBA_INT8;
        case kLX_RGBA_INT8:
            glPxf = GL_RGBA8;
            glPxfType = GL_RGBA;
            glPxfComp = GL_UNSIGNED_BYTE;
            break;
        case kLX_RGBA_FLOAT16:
            glPxf = GL_RGBA_FLOAT16_APPLE;
            glPxfType = GL_RGBA;
            glPxfComp = GL_HALF_APPLE;
            break;
        case kLX_RGBA_FLOAT32:
            glPxf = GL_RGBA_FLOAT32_APPLE;
            glPxfType = GL_RGBA;
            glPxfComp = GL_FLOAT;
            break;
    }
    
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, glPxf, w, h, 0, glPxfType, glPxfComp, NULL);
    
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_EXT, imp->fboColorTex, 0);
                
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        NSLog(@"*** %s: could not create FBO, status: %i", __func__, status);
        return NULL;
    }
    
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    
    [SHAREDCTX deactivate];

    if (imp) {
        imp->pixelFormat = pxFormat;
    }
    return (LXSurfaceRef)imp;
}


extern LXTextureRef LXTextureCreateWithSurface_(LXSurfaceRef surf);


LXTextureRef LXSurfaceGetTexture(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    if (NULL == imp->lxTexture && imp->fboColorTex) {
        imp->lxTexture = LXTextureCreateWithSurface_(r);
        
        LXTextureSetImagePlaneRegion(imp->lxTexture, LXSurfaceGetImagePlaneRegion(r));
    }
    
    return imp->lxTexture;
}


LXUnidentifiedNativeObj LXSurfaceLockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)0;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    ///BEGINSURFACEDRAW
    
    return (LXUnidentifiedNativeObj)(imp->fbo);
}

void LXSurfaceUnlockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return;

    ///ENDSURFACEDRAW
}

LXUnidentifiedNativeObj LXSurfaceBeginPlatformNativeDrawing(LXSurfaceRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)NULL;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    LQGLContext *ctx = LXPlatformSharedNativeGraphicsContext();
    
    BEGINSURFACEDRAW
    
    memcpy(imp->prevViewport, prevViewport_, 4*sizeof(GLint));
    
    return (LXUnidentifiedNativeObj)[ctx CGLContext];
}

void LXSurfaceEndPlatformNativeDrawing(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    GLint prevViewport_[4];
    memcpy(prevViewport_, imp->prevViewport, 4*sizeof(GLint));
    
    ENDSURFACEDRAW
}

GLuint LXSurfaceGetGLColorTexName_(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    
    return imp->fboColorTex;
}



// this is called by both the LXSurface and LXTexture versions of the readback call
LXSuccess LXCopyGLFrameBufferRegionIntoPixelBuffer_(GLuint fbo, GLint srcW, GLint srcH, LXPixelBufferRef pixBuf,
                                                    int32_t regionX, int32_t regionY,
                                                    int32_t regionW, int32_t regionH,
                                                    LXError *outError)
{
    if (fbo == 0) return NO;
    
    if ( !pixBuf) {
        LXErrorSet(outError, 4830, "no pixelbuffer to copy into");
        return NO;
    }
    
    LXPixelFormat pxFormat = LXPixelBufferGetPixelFormat(pixBuf);
    GLenum glPixelFormat;
    GLenum glPixelFormatType;
    size_t dstBytesPerPixel;    
    if ( !getGLFormatInfoForLXPixelFormat(pxFormat, &glPixelFormat, &glPixelFormatType, &dstBytesPerPixel, outError))
        return NO;
    
    size_t dstRowBytes = 0;
    uint8_t *dstBuf = LXPixelBufferLockPixels(pixBuf, &dstRowBytes, NULL, outError);
    
    if ( !dstBuf) {
        if (outError->description == NULL) {
            char msg[512];
            snprintf(msg, 512, "couldn't lock data for pixel buffer (%p)", pixBuf);
            LXErrorSet(outError, 4850, msg);
        }
        return NO;
    }
    
    GLint glAlignment = 4;
#if defined(LX64BIT)
    glAlignment = 8;
#endif

    uint8_t *fittedDstBuf = dstBuf;
    
    LXFitRegionToBufferAndClearOutside(&regionX, &regionY, &regionW, &regionH,  &fittedDstBuf,  dstRowBytes, dstBytesPerPixel,  srcW, srcH);

    ///NSLog(@"%s: region size (%i, %i; %i, %i) -- orig %@; src size %i * %i", __func__, regionX, regionY, regionW, regionH,  NSStringFromRect(origRegion), srcW, srcH);

    if (regionW > 0 && regionH > 0) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
 
        GLint prevAlign = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlign);
        glPixelStorei(GL_PACK_ALIGNMENT, glAlignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, dstRowBytes / dstBytesPerPixel);
    
        glReadBuffer(GL_FRONT);
        glReadPixels(regionX, regionY, regionW, regionH, glPixelFormat, glPixelFormatType, fittedDstBuf);
    
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, prevAlign);

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    }
    
    LXPixelBufferUnlockPixels(pixBuf);
    return YES;
}

LXSuccess LXSurfaceCopyContentsIntoPixelBuffer(LXSurfaceRef r, LXPixelBufferRef pixBuf,
                                                        LXError *outError)
{
    if ( !r) return NO;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    NSCAssert(imp->fbo, @"no FBO for this surface");

    GLint w = LXPixelBufferGetWidth(pixBuf);
    GLint h = LXPixelBufferGetHeight(pixBuf);

    return LXCopyGLFrameBufferRegionIntoPixelBuffer_(imp->fbo, imp->w, imp->h, pixBuf,
                                                     0, 0, w, h, outError);
}

LXSuccess LXSurfaceCopyRegionIntoPixelBuffer(LXSurfaceRef r, LXRect region, LXPixelBufferRef pixBuf,
                                                        LXError *outError)
{
    if ( !r) return NO;
    LXSurfaceOSXImpl *imp = (LXSurfaceOSXImpl *)r;
    NSCAssert(imp->fbo, @"no FBO for this surface");
    
    return LXCopyGLFrameBufferRegionIntoPixelBuffer_(imp->fbo, imp->w, imp->h, pixBuf,
                                                     lround(region.x), lround(region.y), lround(region.w), lround(region.h), outError);
}


