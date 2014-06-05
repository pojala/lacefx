/*
 *  LXTexture_iosgles.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXTexture.h"
#include "LXRef_Impl.h"

#import "LacefxESView.h"


typedef struct {
    LXREF_STRUCT_HEADER
    
    uint32_t w;
    uint32_t h;
    LXPixelFormat pf;

    GLuint texID;
    LXSurfaceRef surf; 

    LXBool isWrapper;
    
    // original spec for textures created from data
    void *buffer;
    size_t rowBytes;
    LXUInteger storageHint;

    // render properties
    LXUInteger sampling;
    LXRect imageRegion;
} LXTexture_iOSImpl;


// platform-independent parts of the class
#define LXTEXTUREIMPL LXTexture_iOSImpl
#include "../Lacefx/LXTexture_baseinc.c"


LXTextureRef LXTextureRetain(LXTextureRef r)
{
    if ( !r) return NULL;
    LXTexture_iOSImpl *imp = (LXTexture_iOSImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));

    return r;
}

void LXTextureRelease(LXTextureRef r)
{
    if ( !r) return;
    LXTexture_iOSImpl *imp = (LXTexture_iOSImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        if (imp->texID != 0 && !imp->isWrapper) {
            EAGLContext *prevCtx = [EAGLContext currentContext];
            [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];
            
            //NSLog(@"deleting gl texture %i", imp->texID);
        
            glDeleteTextures(1, &(imp->texID));
            
            [EAGLContext setCurrentContext:prevCtx];
        }
        imp->texID = 0;
        _lx_free(imp);
    }
}


extern GLuint LXSurfaceGetGLColorTexName_(LXSurfaceRef r);

LXTextureRef LXTextureCreateWithSurface_(LXSurfaceRef surf)
{
    if ( !surf)
        return NULL;
        
    LXTexture_iOSImpl *imp = _lx_calloc(sizeof(LXTexture_iOSImpl), 1);

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


LXTextureRef LXTextureCreateWithGLTextureInternal_(GLuint texID, int w, int h, LXPixelFormat pxFormat,
                                                         uint8_t *buffer, size_t rowBytes,
                                                         LXUInteger storageHint,
                                                         LXBool isWrapper)
{
    if (texID == 0 || w < 1 || h < 1)
        return NULL;

    LXTexture_iOSImpl *imp = _lx_calloc(sizeof(LXTexture_iOSImpl), 1);

    LXREF_INIT(imp, LXTextureTypeID(), LXTextureRetain, LXTextureRelease);
    
    imp->w = w;
    imp->h = h;
    imp->texID = texID;
    imp->pf = pxFormat;
    imp->sampling = kLXNearestSampling;
    
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
            texType = GL_UNSIGNED_BYTE;
            break;

        case kLX_BGRA_INT8:
			bytesPerPixel = 4;
            texFormat = GL_BGRA;
            texInternalFormat = GL_RGBA;
            texType = GL_UNSIGNED_BYTE;
            break;
                
        default:
            NSLog(@"*** %s: pixel format not yet supported (%i)", __func__, pxFormat);
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
    GLuint texID;
    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;
    if ( !getGLTexFormatFromLXPixelFormat(pxFormat, &texFormat, &texInternalFormat, &texType, &bytesPerPixel)) {
        printf("** can't create texture, invalid pxformat\n");
        return NULL;
    }

    LXTextureRef retObj = NULL;
    
    EAGLContext *prevCtx = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];

    // clear any previous error
    glGetError();
    int err = 0;
    
	glGenTextures(1, &texID);
    
	glBindTexture(GL_TEXTURE_2D, texID);
        
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // OpenGL ES doesn't have GL_UNPACK_ROW_LENGTH, so we can't tell the driver about rowbytes...
    // just assume 4-byte alignment for now.
    // LXAlignedRowBytes() will return same alignment on iOS.
    int expectedRowBytes = ((w*bytesPerPixel) + 3) & ~3;
    
    if (rowBytes != expectedRowBytes) {
        NSLog(@"*** %s: incorrect rowbytes, can't use as texture (%i, expected %i)", __func__, (int)rowBytes, expectedRowBytes);
    }
    
    GLint prevAlign = 1;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    
	glTexImage2D(GL_TEXTURE_2D, 0, texInternalFormat,
								w, h, 0,
								texFormat, texType, buffer);
    
	err = glGetError();
	if (err != GL_NO_ERROR) {
		LXPlatformLogInfo([NSString stringWithFormat:@"** %s: error loading texture (texid %i; error 0x%x; internalformat 0x%x, texformat 0x%x, type 0x%x, buffer %p, %i*%i)",
                                                __func__, (int)texID, (int)err, texInternalFormat, texFormat, texType, buffer, w, h]);
        glDeleteTextures(1, &texID);
    } else {
        ///NSLog(@"texture created, size %i * %i", w, h);
        retObj = LXTextureCreateWithGLTexture_(texID, w, h, pxFormat, buffer, rowBytes, storageHint);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
	
	glBindTexture(GL_TEXTURE_2D, 0);
    
    [EAGLContext setCurrentContext:prevCtx];

    return retObj;
}


LXSuccess LXTextureRefreshWithData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    if ( !r) return NO;
    LXTexture_iOSImpl *imp = (LXTexture_iOSImpl *)r;

    GLuint texFormat;
    GLuint texInternalFormat;
    GLuint texType;
    int bytesPerPixel;
    if ( !getGLTexFormatFromLXPixelFormat(imp->pf, &texFormat, &texInternalFormat, &texType, &bytesPerPixel)) {
        printf("** can't refresh texture, invalid pxformat\n");
        return NO;
    }

    EAGLContext *prevCtx = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];

	glBindTexture(GL_TEXTURE_2D, imp->texID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imp->w, imp->h,
                                texFormat, texType, buffer);

    glBindTexture(GL_TEXTURE_2D, 0);
    
    [EAGLContext setCurrentContext:prevCtx];
    
    return YES;
}

LXSuccess LXTextureWillModifyData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint)
{
    // TODO
    return YES;
}



GLuint LXTextureGetGLTexName_(LXTextureRef r)
{
    if ( !r) return 0;
    LXTexture_iOSImpl *imp = (LXTexture_iOSImpl *)r;
    
    return imp->texID;
}


LXUnidentifiedNativeObj LXTextureLockPlatformNativeObj(LXTextureRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)NULL;
    LXTexture_iOSImpl *imp = (LXTexture_iOSImpl *)r;
    
    return (LXUnidentifiedNativeObj)((unsigned long)imp->texID);
}

void LXTextureUnlockPlatformNativeObj(LXTextureRef r)
{
}

