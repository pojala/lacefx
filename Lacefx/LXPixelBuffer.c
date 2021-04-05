/*
 *  LXPixelBuffer_objc.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.8.2007.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPixelBuffer.h"
#include "LXBasicTypes.h"
#include "LXRef_Impl.h"
#include "LXPixelBuffer_priv.h"
#include "LXFileHandlers.h"
#include "LXPool_pixelbuffer_priv.h"
#include "LXTexture.h"
#include "LXMap.h"

#include "LXStringUtils.h"
#include "LXBinaryUtils.h"
#include "LXImageFunctions.h"
#include "LXHalfFloat.h"

#include <math.h>
#include <ctype.h>

#if defined(__APPLE__)
#include <libkern/OSAtomic.h>
static volatile int64_t s_createCount = 0;
static volatile int64_t s_liveCount = 0;
static volatile int64_t s_liveMemBytesEstimate = 0;
// write to same log as LXSurface
extern LXLogFuncPtr g_lxSurfaceLogFuncCb;
extern void *g_lxSurfaceLogFuncCbUserData;
#endif



const char * const kLXPixelBufferAttachmentKey_ColorSpaceEncoding = "lxEnum_ColorSpaceEncoding";
const char * const kLXPixelBufferAttachmentKey_YCbCrPixelFormatID = "lxEnum_YCbCrPixelFormatID";

const char * const kLXPixelBufferFormatRequestKey_AllowAlpha = "allowAlpha";
const char * const kLXPixelBufferFormatRequestKey_AllowYUV = "allowYUV";
const char * const kLXPixelBufferFormatRequestKey_FileFormatID = "fileFormatID";
const char * const kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel = "preferredBitsPerChannel";
const char * const kLXPixelBufferFormatRequestKey_CompressionQuality = "compressionQuality";


//#define DEBUGLOG(format, args...) LXPrintf(format, ## args);
#define DEBUGLOG(format, args...)



// private LXTexture functions
LXEXTERN LXSuccess LXTextureRefreshWithData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint);
LXEXTERN LXSuccess LXTextureWillModifyData(LXTextureRef r, uint8_t *buffer, size_t rowBytes, LXUInteger storageHint);



#pragma pack(push, 4)

typedef struct {
    LXREF_STRUCT_HEADER
    
    uint32_t w;
    uint32_t h;
    LXPixelFormat pf;
    uint32_t bytesPerPixel;

    size_t rowBytes;
    uint8_t *buffer;
    LXUInteger storageHint;
    LXBool prefersCaching;
    
    // should add a (platform-dependent) lock here too, so the lock/unlock methods actually lock

    LXTextureRef texture;
    LXUInteger textureStorageHint;
    
    LXIntegerMapPtr attachmentMap;
    
    LXPixelBufferLockCallbacks lockCallbacks;
    void *lockCallbacksUserData;
} LXPixelBufferImpl;

#pragma pack(pop)



// file wrapper functions
int _lx_fclose(LXFilePtr file)
{
    return fclose((FILE *)file);
}

size_t _lx_fread(void * LXRESTRICT data, size_t len, size_t lenB, LXFilePtr LXRESTRICT file)
{
    return fread(data, len, lenB, (FILE *)file);
}

size_t _lx_fwrite(const void * LXRESTRICT data, size_t len, size_t lenB, LXFilePtr LXRESTRICT file)
{
    return fwrite(data, len, lenB, (FILE *)file);
}

int _lx_fseek64(LXFilePtr file, int64_t pos, int flags)
{
#if defined(LXPLATFORM_WIN)
    return fseeko64((FILE *)file, pos, flags);
#else
    return fseeko((FILE *)file, pos, flags);
#endif
}

int64_t _lx_ftell64(LXFilePtr file)
{
#if defined(LXPLATFORM_WIN)
    return ftello64((FILE *)file);
#else
    return ftello((FILE *)file);
#endif
}



// plugin state
static LXUInteger g_fileHandlerPluginCount = 0;
static LXPixelBufferFileHandlerSuite_v1 *g_fileHandlerPluginSuites = NULL;
static void **g_fileHandlerPluginUserDatas = NULL;


// utility functions
LXUInteger LXBytesPerPixelForPixelFormat(LXPixelFormat pf)
{
    switch (pf) {
        case kLX_ARGB_INT8:
        case kLX_BGRA_INT8:
        case kLX_RGBA_INT8:          return 4;
        
        case kLX_RGBA_FLOAT16:       return 8;
        case kLX_RGBA_FLOAT32:       return 16;
    
        case kLX_Luminance_INT8:     return 1;
        case kLX_Luminance_FLOAT16:  return 2;
        case kLX_Luminance_FLOAT32:  return 4;
        
        case kLX_YCbCr422_INT8:      return 2;
    }
    printf("** %s: unknown pixel format (%lu)\n", __func__, (unsigned long)pf);
    return 0;
}

size_t LXAlignedRowBytes(size_t rowBytes)
{
#if defined(LXPLATFORM_IOS)
    return (rowBytes + 3) & ~3;     // 4-byte alignment
#else
    return (rowBytes + 15) & ~15;   // 16-byte alignment
#endif
}


LXPixelBufferRef LXPixelBufferRetain(LXPixelBufferRef r)
{
    if ( !r) return NULL;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXPixelBufferRelease(LXPixelBufferRef r)
{
    if ( !r) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);
        
        if (imp->lockCallbacks.willDestroy) {
            imp->lockCallbacks.willDestroy(r, imp->lockCallbacksUserData);
        }

        if (imp->texture) {
            LXTextureRelease(imp->texture);
            imp->texture = NULL;
        }
        ///printf("lxpixbuf finishing (%p); is client storage: %lu\n", r, (imp->storageHint & kLXStorageHint_ClientStorage) ? 1 : 0);
        
        if (imp->buffer && !(imp->storageHint & kLXStorageHint_ClientStorage)) {
            _lx_free(imp->buffer);
        }
        imp->buffer = NULL;
        
        if (imp->attachmentMap) {
            LXIntegerMapDestroy(imp->attachmentMap);
            imp->attachmentMap = NULL;
        }

#if defined(__APPLE__)
        int64_t numLive = OSAtomicDecrement64(&s_liveCount);
        int64_t numLiveBytes = OSAtomicAdd64(-(int)(imp->w * imp->h * imp->bytesPerPixel), &s_liveMemBytesEstimate);
        if (g_lxSurfaceLogFuncCb) {
            char text[512];
            snprintf(text, 512, "%s: %p = %d*%d, live now %ld, estimated live bytes %ld (%ld MB)", __func__, imp, imp->w, imp->h, (long)numLive, (long)numLiveBytes, (long)numLiveBytes/(1024*1024));
            g_lxSurfaceLogFuncCb(text, g_lxSurfaceLogFuncCbUserData);
        }
#endif

        _lx_free(imp);
    }
}

LXBool LXPixelBufferReleaseToPool(LXPixelBufferRef r, LXPoolRef pool, LXUInteger releaseHint)
{
    if ( !r) return NO;
    
    return NO;
    // the pool hasn't been implemented so far
    //return LXPixelBufferPoolStore_(r);
}


const char *LXPixelBufferTypeID()
{
    static const char *s = "LXPixelBuffer";
    return s;
}


LXPixelBufferRef LXPixelBufferCreateForData(uint32_t w, uint32_t h,
                                            LXPixelFormat pixelFormat,
                                            size_t rowBytes,
                                            uint8_t *data,
                                            LXUInteger storageHint,
                                            LXError *outError)
{
    if (w < 1 || h < 1 || pixelFormat == 0 || rowBytes < 1 || data == NULL) {
        char str[256];
        sprintf(str, "invalid pixel buffer creation params (%u, %u, %lu, %p)", w, h, (unsigned long)pixelFormat, data);
        LXErrorSet(outError, 1001, str);
        return NULL;
    }
    
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)_lx_calloc(sizeof(LXPixelBufferImpl), 1);

    LXREF_INIT(imp, LXPixelBufferTypeID(), LXPixelBufferRetain, LXPixelBufferRelease);
    
    imp->w = w;
    imp->h = h;
    imp->pf = pixelFormat;
    imp->bytesPerPixel = (int)LXBytesPerPixelForPixelFormat(pixelFormat);
    
    imp->rowBytes = rowBytes;
    imp->buffer = data;
    imp->storageHint = storageHint;
    
#if defined(__APPLE__)
    int64_t numTotal = OSAtomicIncrement64(&s_createCount);
    int64_t numLive = OSAtomicIncrement64(&s_liveCount);
    int64_t numLiveBytes = OSAtomicAdd64(w * h * imp->bytesPerPixel, &s_liveMemBytesEstimate);
    if (g_lxSurfaceLogFuncCb) {
        char text[512];
        snprintf(text, 512, "%s: %p = %d*%d, total %ld, live %ld, estimated live bytes %ld (%ld MB)", __func__, imp, w, h, (long)numTotal, (long)numLive, (long)numLiveBytes, (long)numLiveBytes/(1024*1024));
        g_lxSurfaceLogFuncCb(text, g_lxSurfaceLogFuncCbUserData);
    }
#endif
    
    return (LXPixelBufferRef)imp;

}

LXPixelBufferRef LXPixelBufferCreateWithRowBytes(LXPoolRef pool,
                                     uint32_t w,
                                     uint32_t h,
                                     LXPixelFormat pixelFormat,
                                     size_t rowBytes,
                                     LXError *outError)
{
    if (w < 1 || h < 1 || pixelFormat == 0 || rowBytes < 1) {
        char str[256];
        sprintf(str, "invalid pixel buffer creation params (%u, %u, %lu)", w, h, (unsigned long)pixelFormat);
        LXErrorSet(outError, 1001, str);
        return NULL;
    }

    if (pool) {
        // the pool hasn't been implemented so far
        return NULL;
        /*
        LXPixelBufferRef pbFromPool = LXPixelBufferPoolGet_(w, h, pixelFormat);
        if (pbFromPool)
            return LXPixelBufferRetain(pbFromPool);
         */
    }
    
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)_lx_calloc(sizeof(LXPixelBufferImpl), 1);

    LXREF_INIT(imp, LXPixelBufferTypeID(), LXPixelBufferRetain, LXPixelBufferRelease);
    
    imp->privPoolFunc = LXPixelBufferReleaseToPool;
    imp->pool = pool;
    imp->w = w;
    imp->h = h;
    imp->pf = pixelFormat;
    imp->bytesPerPixel = (int)LXBytesPerPixelForPixelFormat(pixelFormat);
    
    imp->rowBytes = rowBytes;
    imp->buffer = (uint8_t *)_lx_malloc(imp->rowBytes * h);

#if defined(__APPLE__)
    int64_t numTotal = OSAtomicIncrement64(&s_createCount);
    int64_t numLive = OSAtomicIncrement64(&s_liveCount);
    int64_t numLiveBytes = OSAtomicAdd64(w * h * imp->bytesPerPixel, &s_liveMemBytesEstimate);
    if (g_lxSurfaceLogFuncCb) {
        char text[512];
        snprintf(text, 512, "%s: %p = %d*%d, total %ld, live %ld, estimated live bytes %ld (%ld MB)", __func__, imp, w, h, (long)numTotal, (long)numLive, (long)numLiveBytes, (long)numLiveBytes/(1024*1024));
        g_lxSurfaceLogFuncCb(text, g_lxSurfaceLogFuncCbUserData);
    }
#endif

    return (LXPixelBufferRef)imp;
}

LXPixelBufferRef LXPixelBufferCreate(LXPoolRef pool,
                                     uint32_t w,
                                     uint32_t h,
                                     LXPixelFormat pixelFormat,
                                     LXError *outError)
{
    int bytesPerPixel = (int)LXBytesPerPixelForPixelFormat(pixelFormat);
    //size_t rowBytes = ((w * bytesPerPixel) + 15) & ~15;  // 16-byte alignment
    size_t rowBytes = LXAlignedRowBytes(w * bytesPerPixel);

    ///printf("%s: size %i * %i, rowbytes %i \n", __func__, w, h, rowBytes);

    return LXPixelBufferCreateWithRowBytes(pool, w, h, pixelFormat, rowBytes, outError);
}


LXSize LXPixelBufferGetSize(LXPixelBufferRef r)
{
    if ( !r) return LXMakeSize(0, 0);
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    return LXMakeSize(imp->w, imp->h);
}

LXPixelFormat LXPixelBufferGetPixelFormat(LXPixelBufferRef r)
{
	if ( !r) return kLX_RGBA_INT8;
	LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;

	return imp->pf;
}

uint32_t LXPixelBufferGetWidth(LXPixelBufferRef r)
{
    if ( !r) return 0;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    return imp->w;
}

uint32_t LXPixelBufferGetHeight(LXPixelBufferRef r)
{
    if ( !r) return 0;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    return imp->h;
}

LXBool LXPixelBufferMatchesSize(LXPixelBufferRef r, LXSize size)
{
    if ( !r) return NO;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    return (imp->w == size.w && imp->h == size.h) ? YES : NO;
}

void LXPixelBufferSetPrefersHWCaching(LXPixelBufferRef r, LXBool b)
{
    if ( !r) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    imp->prefersCaching = b;
}

LXBool LXPixelBufferGetPrefersHWCaching(LXPixelBufferRef r)
{
    if ( !r) return NO;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    return imp->prefersCaching;
}

void LXPixelBufferSetIntegerAttachment(LXPixelBufferRef r, const char *key, LXInteger value)
{
    if ( !r) return;
    if ( !key) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    if ( !imp->attachmentMap) {
        imp->attachmentMap = LXIntegerMapCreateMutable();
    }
    LXIntegerMapInsert(imp->attachmentMap, key, value);
}

LXInteger LXPixelBufferGetIntegerAttachment(LXPixelBufferRef r, const char *key)
{
    if ( !r) return 0;
    if ( !key) return 0;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;

    if ( !imp->attachmentMap) return 0;
    
    return LXIntegerMapGet(imp->attachmentMap, key);
}

void LXPixelBufferSetFloatAttachment(LXPixelBufferRef r, const char *key, LXFloat value)
{
  #if !defined(LX64BIT)
    LXFloat32CharUnion u;
    u.f = (float)value;
    LXPixelBufferSetIntegerAttachment(r, key, u.i);
  #else
    LXDouble64CharUnion u;
    u.f = value;
    LXPixelBufferSetIntegerAttachment(r, key, u.ll);
  #endif
}

LXFloat LXPixelBufferGetFloatAttachment(LXPixelBufferRef r, const char *key)
{
  #if !defined(LX64BIT)
    LXFloat32CharUnion u;
    u.i = LXPixelBufferGetIntegerAttachment(r, key);
    return u.f;
  #else
    LXDouble64CharUnion u;
    u.ll = LXPixelBufferGetIntegerAttachment(r, key);
    return u.f;
  #endif
}


LXTextureRef LXPixelBufferGetTexture(LXPixelBufferRef r, LXError *outError)
{
    if ( !r) return NULL;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;

    if (imp->texture == NULL) {
        LXError error;
        memset(&error, 0, sizeof(LXError));
    
        LXUInteger storageHint = 0;

        // 2008.10.27: client storage with GL doesn't seem to work correctly for float32 textures on Mac, so exercise caution with enabling it
        if (//(imp->pf == kLX_RGBA_INT8 || imp->pf == kLX_ARGB_INT8 || imp->pf == kLX_BGRA_INT8 || imp->pf == kLX_YCbCr422_INT8)
            (imp->pf != kLX_RGBA_FLOAT32 && imp->pf != kLX_Luminance_FLOAT32)
               ) {
            storageHint = kLXStorageHint_ClientStorage;
        }
        
        if (imp->prefersCaching) {
            storageHint |= kLXStorageHint_PreferCaching;
        }
        else {        
            //if (imp->storageHint & kLXStorageHint_PreferDMAToCaching) {
                storageHint |= kLXStorageHint_PreferDMAToCaching;
                storageHint |= kLXStorageHint_ClientStorage;  // DMA flag makes no sense without client storage
            //}
        }
        //printf("%s, %p: storagehint %i (size %i*%i, pxf %i)\n", __func__, r, (int)storageHint, (int)imp->w, (int)imp->h, (int)imp->pf);
    
        imp->texture = LXTextureCreateWithData(imp->w, imp->h, imp->pf,
                                               imp->buffer, imp->rowBytes,
                                               storageHint, 
                                               &error);

        imp->textureStorageHint = storageHint;    
                                               
///        printf("lxpixbuf get texture, create: %p, retc %i\n", imp->texture, LXRefGetRetainCount(imp->texture));
    }
///    else printf("lxpixbuf get texture: %p, retc %i\n", imp->texture, LXRefGetRetainCount(imp->texture));
        
    return imp->texture;
}

void LXPixelBufferSetLockCallbacks(LXPixelBufferRef r, LXPixelBufferLockCallbacks *callbacks, void *userData)
{
    if ( !r || !callbacks) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    imp->lockCallbacks = *callbacks;
    imp->lockCallbacksUserData = userData;
}


///#ifdef __APPLE__
 #define WILLMODIFYTEXTURE  \
    if (imp->texture) {  \
        if (imp->textureStorageHint & kLXStorageHint_ClientStorage) {  \
            LXTextureWillModifyData(imp->texture, imp->buffer, imp->rowBytes, imp->textureStorageHint);  \
        } else {  \
            LXTextureRelease(imp->texture);  /*printf("did release texture %p, pixbuf %p\n", imp->texture, imp);*/  \
            imp->texture = NULL; \
        }  \
    }
    
 #define DIDMODIFYTEXTURE  \
    if (imp->texture) {  \
        LXTextureRefreshWithData(imp->texture, imp->buffer, imp->rowBytes, imp->textureStorageHint);  \
    } 
    
//#else
// #define WILLMODIFYTEXTURE
// #define DIDMODIFYTEXTURE
//#endif


uint8_t *LXPixelBufferLockPixels(LXPixelBufferRef r, size_t *outRowBytes, int32_t *outBytesPerPixel, LXError *outError)
{
    if ( !r) {
        LXErrorSet(outError, 1002, "no pixel buffer for LXPixelBufferLockPixels");
        return NULL;
    }
    
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    if (imp->lockCallbacks.shouldLock) {
        if ( !imp->lockCallbacks.shouldLock(r, imp->lockCallbacksUserData)) {
            return NULL;
        }
    }

    if (outRowBytes) *outRowBytes = imp->rowBytes;
    if (outBytesPerPixel) *outBytesPerPixel = imp->bytesPerPixel;
    
    WILLMODIFYTEXTURE
        
    return imp->buffer;
}


void LXPixelBufferUnlockPixels(LXPixelBufferRef r)
{
    if ( !r) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;

    // currently does nothing, turns out the locking was never needed.
    
    // !!! NOTE: LQBitmap.m will keep the frame buffer data pointer around even after this call,
    // so we shouldn't do anything here that might cause the data pointer to eventually become invalid
    // before the pixelbuffer is actually destroyed
    
    DIDMODIFYTEXTURE
    
    if (imp->lockCallbacks.didUnlock) {
        imp->lockCallbacks.didUnlock(r, imp->lockCallbacksUserData);
    }
}

void LXPixelBufferInvalidateCaches(LXPixelBufferRef r)
{
    if ( !r) return;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    if (imp->texture) {
        LXTextureRelease(imp->texture);
        imp->texture = NULL;
    }
}


#pragma mark --- reading files ---

LXUInteger LXImageTypeFromPath(LXUnibuffer uni, LXUInteger *outPluginIndex)
{
    const char16_t *unipath = uni.unistr;

    int n = (int)uni.numOfChar16;
	const char16_t *ptr = unipath + n - 1;
	const int EXTLEN = 15;
	char extbuf[16] = "";
	
	while (*ptr == 0 && (--n) > 0)
		ptr--;

	if (n == 0) return 0;
	
    LXBool foundExt = NO;
    int i;
    int lim = (n > EXTLEN-1) ? 0 : EXTLEN-1-n;
	for (i = EXTLEN-1; i >= lim; i--) {
		extbuf[i] = tolower( (*(ptr--)) & 127 );
		if (extbuf[i] == '.') {
			i++;
            foundExt = YES;
			break;
		}
	}
    
    if ( !foundExt) return 0;

    char *ext = extbuf+i;
	//printf("got ext: '%s'\n", ext);
        
    if (0 == strcmp(ext, "dpx") || 0 == strcmp(ext, "cin"))
        return kLXImage_DPX;
    else if (0 == strcmp(ext, "tga"))
        return kLXImage_TGA;
    else if (0 == strcmp(ext, "jp2") || 0 == strcmp(ext, "jp2k"))
        return kLXImage_JPEG2000;
    else if (0 == strcmp(ext, "jpg") || 0 == strcmp(ext, "jpeg"))
        return kLXImage_JPEG;
    else if (0 == strcmp(ext, "tif") || 0 == strcmp(ext, "tiff"))
        return kLXImage_TIFF;
    else if (0 == strcmp(ext, "bmp"))
        return kLXImage_BMP;
	else if (0 == strcmp(ext, "png"))
		return kLXImage_PNG;
	else if (0 == strcmp(ext, "lxpix") || 0 == strcmp(ext, "lxp"))
		return kLXImage_LXPix;
        
    else if (outPluginIndex) {
        // check if we have a plugin to handle this extension
        LXUInteger plugCount = g_fileHandlerPluginCount;
        if (plugCount > 0) {
            LXUInteger i;
            for (i = 0; i < plugCount; i++) {
                LXPixelBufferFileHandlerSuite_v1 *suite = g_fileHandlerPluginSuites + i;
                void *suiteCtx = g_fileHandlerPluginUserDatas[i];
                LXBool didFindMatch = NO;

                char **supportedExts = NULL;
                size_t supportedExtCount = 0;
                suite->copySupportedFileExtensions(suiteCtx, &supportedExts, &supportedExtCount);
                
                if (supportedExtCount > 0) {
                    LXUInteger j;
                    for (j = 0; j < supportedExtCount; j++) {
                        char *str = supportedExts[j];
                        if (str && 0 == strcmp(ext, str)) {
                            didFindMatch = YES;
                        }
                        _lx_free(str);
                    }
                    _lx_free(supportedExts);
                }
                
                if ( !didFindMatch) {
                    // the given "path" may also be a format UTI such as "com.ilm.openexr-image",
                    // so try to match against the plugin's declared format.
                    size_t utf8Len = 0;
                    char *utf8Str = LXStrCreateUTF8_from_UTF16(uni.unistr, uni.numOfChar16, &utf8Len);

                    if (utf8Str && 0 == strcmp(utf8Str, suite->getFileFormatID(suiteCtx))) {
                        didFindMatch = YES;
                    }
                    _lx_free(utf8Str);
                }
                
                if (didFindMatch) {
                    *outPluginIndex = i;
                    return kLXImage_HandledByPlugin;
                }
            }
        }
    }
    
    return 0;
}

LXUInteger LXImageTypeFromUTI(const char *uti, LXUInteger *outPluginIndex)
{
    if ( !uti) return 0;

    LXUInteger plugCount = g_fileHandlerPluginCount;
    LXUInteger i;
    for (i = 0; i < plugCount; i++) {
        LXPixelBufferFileHandlerSuite_v1 *suite = g_fileHandlerPluginSuites + i;
        void *suiteCtx = g_fileHandlerPluginUserDatas[i];
        
        if (0 == strcmp(uti, suite->getFileFormatID(suiteCtx))) {
            if (outPluginIndex) *outPluginIndex = i;
            return kLXImage_HandledByPlugin;
        }
    }
    
    if (0 == strcmp(uti, "public.dpx"))
        return kLXImage_DPX;
    else if (0 == strcmp(uti, "com.truevision.tga-image"))
        return kLXImage_TGA;
    else if (0 == strcmp(uti, "public.jpeg-2000"))
        return kLXImage_JPEG2000;
    else if (0 == strcmp(uti, "public.jpeg"))
        return kLXImage_JPEG;
    else if (0 == strcmp(uti, "public.tiff"))
        return kLXImage_TIFF;
    else if (0 == strcmp(uti, "com.microsoft.bmp"))
        return kLXImage_BMP;
	else if (0 == strcmp(uti, "public.png"))
		return kLXImage_PNG;
	else if (0 == strcmp(uti, "fi.lacquer.lxpix"))
		return kLXImage_LXPix;
    
    return 0;
}

char *LXCopyDefaultFileExtensionForImageUTI(const char *uti)
{
    if ( !uti) return NULL;

    LXUInteger plugCount = g_fileHandlerPluginCount;
    LXUInteger i;
    for (i = 0; i < plugCount; i++) {
        LXPixelBufferFileHandlerSuite_v1 *suite = g_fileHandlerPluginSuites + i;
        void *suiteCtx = g_fileHandlerPluginUserDatas[i];
        
        if (0 == strcmp(uti, suite->getFileFormatID(suiteCtx))) {
            char **supportedExts = NULL;
            size_t supportedExtCount = 0;
            suite->copySupportedFileExtensions(suiteCtx, &supportedExts, &supportedExtCount);            
            if (supportedExtCount < 1) return NULL;
            
            char *ext = supportedExts[0];
            LXUInteger j;
            for (j = 1; j < supportedExtCount; j++) {
                _lx_free(supportedExts[j]);
            }
            _lx_free(supportedExts);
            
            return ext;
        }
    }

    if (0 == strcmp(uti, "public.dpx"))
        return _lx_strdup("dpx");
    else if (0 == strcmp(uti, "com.truevision.tga-image"))
        return _lx_strdup("tga");
    else if (0 == strcmp(uti, "public.jpeg-2000"))
        return _lx_strdup("jp2");
    else if (0 == strcmp(uti, "public.jpeg"))
        return _lx_strdup("jpg");
    else if (0 == strcmp(uti, "public.tiff"))
        return _lx_strdup("tif");
    else if (0 == strcmp(uti, "com.microsoft.bmp"))
        return _lx_strdup("bmp");
	else if (0 == strcmp(uti, "public.png"))
		return _lx_strdup("png");
	else if (0 == strcmp(uti, "fi.lacquer.lxpix"))
		return _lx_strdup("lxp");

    return NULL;
}


LXPixelBufferRef LXPixelBufferCreateFromFileInMemory(const uint8_t *data, size_t len, const char *formatUTI, LXMapPtr properties, LXError *outError)
{
    if ( !data || len < 1) {
        LXErrorSet(outError, 1899, "no data");
        return NULL;
    }
    
    LXUInteger pluginIndex = kLXNotFound;
    LXUInteger imageType = (formatUTI) ? LXImageTypeFromUTI(formatUTI, &pluginIndex) : 0;
    
    if (imageType == kLXImage_LXPix) {
        return LXPixelBufferCreateFromSerializedData(data, len, outError);
    }
    else if (imageType == kLXImage_HandledByPlugin) {
        LXErrorSet(outError, 1901, "image format is handled by plugin which doesn't support this data representation");
        return NULL;
    }
    else if (imageType == kLXImage_DPX) {
        LXErrorSet(outError, 1901, "DPX loader doesn't support this data representation");
        return NULL;    
    }
    
    LXMapPtr newProps = properties;
    if (properties && formatUTI) {
        LXMapPtr newProps = LXMapCreateMutable();
        LXMapCopyEntriesFromMap(newProps, properties);
    
        LXMapSetUTF8(newProps, kLXPixelBufferFormatRequestKey_FileFormatID, formatUTI);
    }
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreateFromFileDataUsingNativeAPI_(data, len, imageType, newProps, outError);
    
    if (newProps != properties) LXMapDestroy(newProps);
    return newPixbuf;
}

LXPixelBufferRef LXPixelBufferCreateFromFileAtPath(LXUnibuffer uni, LXMapPtr properties,
                                                   LXError *outError)
{
	if ( !uni.unistr || uni.numOfChar16 < 1) {	
        LXErrorSet(outError, 1699, "empty path given");
        return NULL;
    }
    
    DEBUGLOG("%s\n", __func__);
	
    LXUnibuffer tempUni = { 0, NULL };
    LXBool didUseTempUni = LXStrTrimBOMAndSwapToNative(&uni, &tempUni);
    LXUnibuffer thePath = (didUseTempUni) ? tempUni : uni;

    DEBUGLOG("1\n");
    
	// find extension
    LXUInteger pluginIndex = kLXNotFound;
	LXUInteger imageType = LXImageTypeFromPath(thePath, &pluginIndex);
    
    DEBUGLOG("2\n");
    
    // ... could check "kLXPixelBufferFormatRequestKey_FileFormatID" property so that caller can override detection of file extension

    LXPixelBufferRef newPixbuf = NULL;
    
    if (imageType == kLXImage_HandledByPlugin && pluginIndex != kLXNotFound) {
        LXPixelBufferFileHandlerSuite_v1 *pluginSuite = g_fileHandlerPluginSuites + pluginIndex;
        void *suiteCtx = g_fileHandlerPluginUserDatas[pluginIndex];
        
        void *imageRef = NULL;
        if ( !pluginSuite->openImageAtPath(suiteCtx, thePath, &imageRef, outError)) {
            if (outError && !outError->description) {
                LXErrorSet(outError, 1702, "file handler plugin failed to open image file");
            }
            goto bail;
        } else {
            LXUInteger layerCount = pluginSuite->getNumberOfLayers(suiteCtx, imageRef);
            if (layerCount < 1) {
                LXErrorSet(outError, 1703, "image file does not contain any layers");
                goto bail;
            }
        
            newPixbuf = pluginSuite->createPixelBufferForLayer(suiteCtx, imageRef, 0, properties, outError);
            pluginSuite->closeImage(suiteCtx, imageRef);
        }
    }
    else if (imageType == kLXImage_LXPix) {
        LXFilePtr file = NULL;
        if ( !LXOpenFileForReadingWithUnipath(thePath.unistr, thePath.numOfChar16, &file)) {
            LXErrorSet(outError, 1760, "could not open file");
            return NULL;
        }
        
        _lx_fseek64(file, 0, SEEK_END);
        size_t fileLen = (size_t)_lx_ftell64(file);
        uint8_t *fileData = _lx_malloc(fileLen);
        
        _lx_fseek64(file, 0, SEEK_SET);
        size_t bytesRead = _lx_fread(fileData, 1, fileLen, file);
        _lx_fclose(file);
        
        //LXPrintf("reading lxpix file of %i bytes\n", fileLen);
    
        LXPixelBufferRef pixbuf = NULL;
        if (bytesRead != fileLen || bytesRead == 0) {
            LXErrorSet(outError, 1761, "error reading from file");
        } else {
            pixbuf = LXPixelBufferCreateFromSerializedData(fileData, fileLen, outError);
        }
        _lx_free(fileData);
        return pixbuf;
    }
    
#if !defined(LXPLATFORM_IOS)
    else if (imageType == kLXImage_DPX) {
        // read with our own DPX reader
        newPixbuf = LXPixelBufferCreateFromDPXImageAtPath(thePath, properties, outError);
    }
    else if (imageType == kLXImage_TIFF) {
        // try first with our own reader; if it fails, let the native API open the file.
        // if the caller wants a colorspace conversion, we must let the native API handle it.
        LXBool wantsColorConv = LXMapContainsValueForKey(properties, "outputColorSpaceName", NULL);
        LXBool didHandle = NO;
        LXDECLERROR(tiffErr)
        
        if ( !wantsColorConv) {
            if ( !(newPixbuf = LXPixelBufferCreateFromTIFFImageAtPath(thePath, properties, &tiffErr))) {
                printf("*** Lacefx TIFF reader failed: error is %i / '%s'\n", tiffErr.errorID, tiffErr.description);
            } else {
                didHandle = YES;
            }
        }
        if ( !didHandle) {
            newPixbuf = LXPixelBufferCreateFromPathUsingNativeAPI_(thePath, imageType, properties, outError);
        }

        if ( !newPixbuf && outError && !(outError->description)) {
            *outError = tiffErr;
        } else {
            LXErrorDestroyOnStack(tiffErr);
        }
    }
#endif
    
    else {
        // call the platform-dependent implementation (handles a lot of formats on OS X)
        newPixbuf = LXPixelBufferCreateFromPathUsingNativeAPI_(thePath, imageType, properties, outError);
    }

    DEBUGLOG("%s finished, imagetype was %i\n", __func__, (int)imageType);
    
bail:
    if (didUseTempUni)
        LXStrUnibufferDestroy(&tempUni);
        
    return newPixbuf;
}


#pragma mark --- writing files ---

LXSuccess LXPixelBufferWriteAsFileToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
	if ( !unipath.unistr || unipath.numOfChar16 < 1) {	
        LXErrorSet(outError, 1699, "empty path given");
        return NO;
    }
	
    LXSuccess retVal = NO;
    LXUInteger pluginIndex = kLXNotFound;
    LXUInteger imageType = 0;
    
    // the format ID property can be an UTI-style string, e.g. "public.png" or "com.ilm.openexr-image"
    ///LXUnibuffer formatIDStr = { 0, NULL };
    char *formatUTI = NULL;
    if (LXMapContainsValueForKey(properties, kLXPixelBufferFormatRequestKey_FileFormatID, NULL)) {
        //LXMapGetUTF16(properties, kLXPixelBufferFormatRequestKey_FileFormatID, &formatIDStr);
        //formatUTI = LXStrCreateUTF8_from_UTF16(formatIDStr.unistr, formatIDStr.numOfChar16, NULL);
        
        LXMapGetUTF8(properties, kLXPixelBufferFormatRequestKey_FileFormatID, &formatUTI);
        
        imageType = LXImageTypeFromUTI(formatUTI, &pluginIndex);

        /*if (pluginIndex != kLXNotFound) {
            LXPrintf("%s: exporting with UTI '%s' -- pluginIndex %u\n", __func__, formatUTI, pluginIndex);
        } else {
            LXPrintf("%s: exporting with UTI '%s' -- will use native API (image type %i)\n", __func__, formatUTI, imageType);
        }*/
        ///LXStrUnibufferDestroy(&formatIDStr);
    }
    
    // if the format ID didn't provide the image type, look at the extension in the output file name
    if ( !imageType) {
        imageType = LXImageTypeFromPath(unipath, &pluginIndex);
    }
    
    if (imageType == kLXImage_HandledByPlugin && pluginIndex != kLXNotFound) {
        LXPixelBufferFileHandlerSuite_v1 *pluginSuite = g_fileHandlerPluginSuites + pluginIndex;
        void *suiteCtx = g_fileHandlerPluginUserDatas[pluginIndex];
        
        if ( !pluginSuite->writePixelBufferToFile) {
            char msg[1024];
            sprintf(msg, "file handler plugin does not support writing (format: %s)",
                                                (pluginSuite->getFileFormatID) ? pluginSuite->getFileFormatID(suiteCtx) : "(invalid plugin)");
            LXErrorSet(outError, 1710, msg);
            retVal = NO;
        } else {
            retVal = pluginSuite->writePixelBufferToFile(suiteCtx, pixbuf, unipath, properties, outError);
        }
    }

    else if (imageType == kLXImage_LXPix) {
        LXFilePtr file = NULL;
        if ( !LXOpenFileForWritingWithUnipath(unipath.unistr, unipath.numOfChar16, &file)) {
            LXErrorSet(outError, 1760, "could not open file");
            retVal = NO;
        } else {
            uint8_t *buf = NULL;
            size_t bufLen = 0;
            if ( !LXPixelBufferSerializeDeflated(pixbuf, &buf, &bufLen)) {
                LXErrorSet(outError, 1770, "could not serialize pixel buffer");
                retVal = NO;
            } else {
                _lx_fwrite(buf, bufLen, 1, file);
                _lx_free(buf);
                retVal = YES;
            }
            _lx_fclose(file);
        }
    }
#if !defined(LXPLATFORM_IOS)
    else if (imageType == kLXImage_DPX) {
        retVal = LXPixelBufferWriteAsDPXImageToPath(pixbuf, unipath, properties, outError);
    }
    else if (imageType == kLXImage_JPEG) {
        retVal = LXPixelBufferWriteAsJPEGImageToPath(pixbuf, unipath, properties, outError);
    }
    else if (imageType == kLXImage_TIFF) {
        retVal = LXPixelBufferWriteAsTIFFImageToPath(pixbuf, unipath, properties, outError);
    }
#endif
    else if (imageType > 0) {
        retVal = LXPixelBufferWriteToPathUsingNativeAPI_(pixbuf, unipath, imageType, properties, outError);
    }
    else {
        LXErrorSet(outError, 1801, "could not detect image format for writing");
    }
    
    _lx_free(formatUTI);
    return retVal;
}


#pragma mark --- file handler plugins ---

LXSuccess LXPixelBufferGetSuggestedFileHandlerForPath(LXUnibuffer path, LXPixelBufferFileHandlerSuite_v1 **outHandlerSuite, void **outHandlerCtx)
{
    LXUInteger pluginIndex = kLXNotFound;
	LXUInteger imageType = LXImageTypeFromPath(path, &pluginIndex);

    if (imageType == kLXImage_HandledByPlugin && pluginIndex != kLXNotFound) {
        if (outHandlerSuite) *outHandlerSuite = g_fileHandlerPluginSuites + pluginIndex;
        if (outHandlerCtx) *outHandlerCtx = g_fileHandlerPluginUserDatas[pluginIndex];
        return YES;
    }
    return NO;
}

LXSuccess LXPixelBufferRegisterFileHandler(LXPixelBufferFileHandlerSuite_v1 *pluginSuite, void *ctx, LXError *outError)
{
    if ( !pluginSuite) return NO;
    
    if (NULL == pluginSuite->getFileFormatID) {
        LXErrorSet(outError, 400, "format ID callback missing");
        return NO;
    }
    if (NULL == pluginSuite->getFileFormatID(ctx) || strlen(pluginSuite->getFileFormatID(ctx)) < 1) {
        LXErrorSet(outError, 400, "file handler plugin does not report a valid format ID");
        return NO;
    }
    if (NULL == pluginSuite->copySupportedFileExtensions) {
        LXErrorSet(outError, 400, "file extensions callback missing");
        return NO;
    }
    if (NULL == pluginSuite->openImageAtPath) {
        LXErrorSet(outError, 400, "openImageAtPath callback missing");
        return NO;
    }
    if (NULL == pluginSuite->createPixelBufferForLayer) {
        LXErrorSet(outError, 400, "createPixelBufferForLayer callback missing");
        return NO;
    }
    
    LXUInteger plugCount = g_fileHandlerPluginCount + 1;
    
    g_fileHandlerPluginSuites = (g_fileHandlerPluginSuites) ? _lx_realloc(g_fileHandlerPluginSuites, plugCount * sizeof(LXPixelBufferFileHandlerSuite_v1))
                                                            : _lx_malloc(                            plugCount * sizeof(LXPixelBufferFileHandlerSuite_v1));

    g_fileHandlerPluginUserDatas = (g_fileHandlerPluginUserDatas) ? _lx_realloc(g_fileHandlerPluginUserDatas, plugCount * sizeof(void *))
                                                                  : _lx_malloc(                               plugCount * sizeof(void *));

    memcpy(g_fileHandlerPluginSuites + (plugCount-1), pluginSuite, sizeof(LXPixelBufferFileHandlerSuite_v1));
    
    g_fileHandlerPluginUserDatas[plugCount-1] = ctx;

    g_fileHandlerPluginCount = plugCount;
    return YES;    
}


#pragma mark --- serialization ---


enum {
    kLXPixBufIsDeflated = 1
};
typedef LXUInteger LXPixBufFlatFlags;


#pragma pack(push, 1)

typedef struct {
    uint32_t cookie;
    uint32_t w;
    uint32_t h;
    uint32_t pf;
    uint32_t rowBytes;
    
    uint32_t imageDataSize;
    uint32_t flags;
    uint32_t metadataSizeInBytes;  // this is effectively the offset to the actual image data counted from the end of the header
    uint32_t _reserved[24];
    
    char buf[256];
} LXPixBufFlat;

#pragma pack(pop)


#define FLATHEADERSIZE (32 * sizeof(uint32_t))

#define FLATCOOKIE              0x3401affa
#define FLATCOOKIE_FLIPPED      0xfaaf0134


LXPixelBufferRef LXPixelBufferCreateFromSerializedData(const uint8_t *buf, size_t dataLen, LXError *outError)
{
    if ( !buf || dataLen < FLATHEADERSIZE) {
	    LXErrorSet(outError, 1010, "no input buffer provided or data size is too short");
	    return NULL;
    }

    const LXPixBufFlat *flat = (const LXPixBufFlat *)buf;
    
    if (flat->cookie != FLATCOOKIE && flat->cookie != FLATCOOKIE_FLIPPED) {
        printf("*** %s: invalid cookie in flat data (size %i)\n", __func__, (int)dataLen);
	    LXErrorSet(outError, 1011, "provider data doesn't have the correct identifier, file may be corrupted");
        return NULL;
    }
    
    const LXBool dataIsFlipped = (flat->cookie == FLATCOOKIE_FLIPPED);
    const LXUInteger w =  (dataIsFlipped) ? LXEndianSwap_uint32(flat->w) : flat->w;
    const LXUInteger h =  (dataIsFlipped) ? LXEndianSwap_uint32(flat->h) : flat->h;
    const LXUInteger pf = (dataIsFlipped) ? LXEndianSwap_uint32(flat->pf) : flat->pf;
    const size_t rowBytes = (dataIsFlipped) ? LXEndianSwap_uint32(flat->rowBytes) : flat->rowBytes;
    const size_t imageDataSize = (dataIsFlipped) ? LXEndianSwap_uint32(flat->imageDataSize) : flat->imageDataSize;
    const LXUInteger flags = (dataIsFlipped) ? LXEndianSwap_uint32(flat->flags) : flat->flags;
    const LXUInteger mdSize = (dataIsFlipped) ? LXEndianSwap_uint32(flat->metadataSizeInBytes) : flat->metadataSizeInBytes;
    if (mdSize > 64*1024*1024) {  // sanity check
        printf("*** %s: invalid metadata size (%i)\n", __func__, (int)mdSize);
	    LXErrorSet(outError, 1012, "serialized data has excessive metadata size, may indicate corruption");
        return NULL;
    }
    
    uint8_t *imageBuffer = (uint8_t *)flat->buf + mdSize;
    uint8_t *mdBuffer = (mdSize > 0) ? (uint8_t *)flat->buf : NULL;
    #pragma unused(mdBuffer)
    
    // TODO: process metadata
        
    LXPixelBufferRef newPixbuf = LXPixelBufferCreateWithRowBytes(NULL, w, h, (LXPixelFormat)pf, rowBytes, outError);
    if (newPixbuf) {
        uint8_t *srcBuf = imageBuffer;
        LXBool srcNeedsFree = NO;
        uint8_t *dstBuf = LXPixelBufferLockPixels(newPixbuf, NULL, NULL, NULL);

        // if data is zlib compressed, must decompress (=inflate)
        if (flags & kLXPixBufIsDeflated) {
            size_t inflateBufSize = h * rowBytes + 1024;
            srcBuf = (uint8_t *) _lx_malloc(inflateBufSize);
            srcNeedsFree = YES;
            size_t inflatedLen = 0;
            
            if (NO == LXSimpleInflate(imageBuffer, imageDataSize, srcBuf, inflateBufSize, &inflatedLen)) {
                printf("*** %s: inflate failed\n", __func__);
            } else {
                ///printf("inflate successful: %i --> %i\n", imageDataSize, inflatedLen);
            }
        }
        
        // for 8-bit rgba pixels, we may need to flip
        if (dataIsFlipped && (pf == kLX_RGBA_INT8 || pf == kLX_ARGB_INT8 || pf == kLX_BGRA_INT8)) {
            int x, y;
            for (y = 0; y < h; y++) {
                unsigned int *src = (unsigned int *)(srcBuf + rowBytes * y);
                unsigned int *dst = (unsigned int *)(dstBuf + rowBytes * y);
                for (x = 0; x < w; x++) {
                    unsigned int v = src[x];
                    dst[x] = LXEndianSwap_uint32(v);
                }
            }
        }
        else {
            _lx_memcpy_aligned(dstBuf, srcBuf, rowBytes * h);
        }
        
        LXPixelBufferUnlockPixels(newPixbuf);
        
        if (srcNeedsFree) _lx_free(srcBuf);
    }
    
    return newPixbuf;
}

size_t LXPixelBufferGetSerializedDataSize(LXPixelBufferRef r)
{
    if ( !r) return 0;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    size_t bufSize = FLATHEADERSIZE + (imp->rowBytes * imp->h);
    
    return bufSize;
}


// TODO: should write attachments as metadata

LXSuccess LXPixelBufferSerialize(LXPixelBufferRef r, uint8_t *buf, size_t bufLen)
{
    if ( !r) return NO;
    if (bufLen < LXPixelBufferGetSerializedDataSize(r)) return NO;
    
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    LXPixBufFlat *flat = (LXPixBufFlat *)buf;
    
    memset(flat, 0, FLATHEADERSIZE);
    
    flat->cookie = FLATCOOKIE;  // the cookie determines endianness
    flat->w = imp->w;
    flat->h = imp->h;
    flat->pf = imp->pf;
    flat->rowBytes = (unsigned int)imp->rowBytes;
    flat->imageDataSize = imp->rowBytes * imp->h;
    flat->metadataSizeInBytes = 0;
    
    memcpy(buf + FLATHEADERSIZE,  imp->buffer,  flat->imageDataSize);

    return YES;
}


LXSuccess LXPixelBufferSerializeDeflated(LXPixelBufferRef r, uint8_t **outBuf, size_t *outBufLen)
{
    if ( !r) return NO;
    LXPixelBufferImpl *imp = (LXPixelBufferImpl *)r;
    
    // create deflated data.
    // we just assume that the compressed data won't grow substantially in size.
    size_t srcBufSize = imp->rowBytes * imp->h;
    size_t dstBufSize = srcBufSize + 512;
    size_t deflDataSize = 0;
    uint8_t *deflBuf = (uint8_t *) _lx_malloc(dstBufSize);
    
    if (NO == LXSimpleDeflate((uint8_t *)imp->buffer, srcBufSize,  deflBuf, dstBufSize, &deflDataSize)) {
        _lx_free(deflBuf);
        return NO;
    }
    
    LXPixBufFlat *flat = (LXPixBufFlat *) _lx_malloc(FLATHEADERSIZE + deflDataSize);
    
    memset(flat, 0, FLATHEADERSIZE);
    
    flat->cookie = FLATCOOKIE;  // the cookie determines endianness
    flat->w = imp->w;
    flat->h = imp->h;
    flat->pf = imp->pf;
    flat->rowBytes = (unsigned int)imp->rowBytes;
    flat->imageDataSize = deflDataSize;
    flat->flags = kLXPixBufIsDeflated;
    flat->metadataSizeInBytes = 0;
    
    memcpy((uint8_t *)flat + FLATHEADERSIZE,  deflBuf,  deflDataSize);
    
    _lx_free(deflBuf);
    
    *outBuf = (uint8_t *)flat;
    *outBufLen = FLATHEADERSIZE + deflDataSize;
    return YES;
}



#pragma mark --- transform utils ---

LXPixelBufferRef LXPixelBufferCreateScaled(LXPixelBufferRef srcPixbuf, uint32_t dstW, uint32_t dstH, LXError *outError)
{
    if ( !srcPixbuf) {
        LXErrorSet(outError, 2001, "no source image");
        return NULL;
    }
    if (dstW < 1 || dstH < 1) {
        LXErrorSet(outError, 2002, "dst size is zero");
        return NULL;
    }

	LXPixelFormat pf = LXPixelBufferGetPixelFormat(srcPixbuf);
    
	LXUInteger sampleDepth = 0;
	switch (pf) { // only RGBA formats and lum8 are supported for scaling currently
        case kLX_Luminance_INT8:
        case kLX_ARGB_INT8:
        case kLX_BGRA_INT8:
        case kLX_RGBA_INT8:          sampleDepth = 8;   break;
        case kLX_RGBA_FLOAT16:       sampleDepth = 16;  break;
        case kLX_RGBA_FLOAT32:       sampleDepth = 32;  break;
    }

	if (sampleDepth == 0) {
        char msg[512];
        sprintf(msg, "source pixelformat is unsupported for scaling (%ld)", (long)pf);
		LXErrorSet(outError, 2003, msg);
		return NULL;
	}

    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, dstW, dstH, pf, outError);
    if ( !newPixbuf)
        return NULL;

    int srcW = LXPixelBufferGetWidth(srcPixbuf);
    int srcH = LXPixelBufferGetHeight(srcPixbuf);
    size_t srcRowBytes = 0;
    size_t dstRowBytes = 0;
    uint8_t *srcBuf = (uint8_t *) LXPixelBufferLockPixels(srcPixbuf, &srcRowBytes, NULL, NULL);
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);

    if (pf == kLX_Luminance_INT8) {
        LXImageScale_Luminance_int8(srcBuf, srcW, srcH, srcRowBytes,
                                    dstBuf, dstW, dstH, dstRowBytes,
                                    outError);
    } else {
        LXImageScale_RGBAWithDepth(srcBuf, srcW, srcH, srcRowBytes,
                                    dstBuf, dstW, dstH, dstRowBytes,
                                    sampleDepth, 
                                    outError);
    }
    LXPixelBufferUnlockPixels(srcPixbuf);
    LXPixelBufferUnlockPixels(newPixbuf);
    
    return newPixbuf;
}


#pragma mark --- pixel format utils ---

// the following pxReorder.. functions are implemented separately this way so that they can be Altivec/SSE optimized
// easily if necessary

#define REORDERSRC(a, b, c, d)  dstBuf[0] = srcBuf[a];  dstBuf[1] = srcBuf[b];  dstBuf[2] = srcBuf[c];  dstBuf[3] = srcBuf[d];

static void pxReorder_uint8_as_1230(uint8_t * LXRESTRICT dstBuf, const uint8_t * LXRESTRICT srcBuf, const uint32_t w) {
    LXUInteger x;
    for (x = 0; x < w; x++) {
        REORDERSRC(1, 2, 3, 0);
        srcBuf += 4;  dstBuf += 4;
    }
}

static void pxReorder_uint8_as_3012(uint8_t * LXRESTRICT dstBuf, const uint8_t * LXRESTRICT srcBuf, const uint32_t w) {
    LXUInteger x;
    for (x = 0; x < w; x++) {
        REORDERSRC(3, 0, 1, 2);
        srcBuf += 4;  dstBuf += 4;
    }
}

static void pxReorder_uint8_as_3210(uint8_t * LXRESTRICT dstBuf, const uint8_t * LXRESTRICT srcBuf, const uint32_t w) {
    LXUInteger x;
    for (x = 0; x < w; x++) {
        REORDERSRC(3, 2, 1, 0);
        srcBuf += 4;  dstBuf += 4;
    }
}


// "srcYCbCrFormatID" specifies the YUV pixel layout.
// this is a hack to allow us to put YUY2 format data into LXPixelBuffers on Win32 where it's a common format from DirectShow.
// YUY2 is specified with a value of 1.
// Lacefx's default YCbCr layout is called '2vuy' on Mac, 'UYVY' on Win32.
// 
enum {
    kLX_YCbCrFormat_2vuy = 0,
    kLX_YCbCrFormat_YUY2 = 1
};

#define LXPXF_ISRGBA(pxf_)       ((pxf_) == kLX_RGBA_INT8 || (pxf_) == RGBA_FLOAT16 || (pxf_) == kLX_RGBA_FLOAT32 || (pxf_) == kLX_ARGB_INT8 || (pxf_) == kLX_BGRA_INT8)
#define LXPXF_ISLUMINANCE(pxf_)  ((pxf_) == kLX_Luminance_INT8 || (pxf_) == kLX_Luminance_FLOAT16 || (pxf_) == kLX_Luminance_FLOAT32)


LXSuccess LXPxConvert_Any_(const uint8_t * LXRESTRICT aSrcBuffer,
                           const uint32_t srcW, const uint32_t srcH, const size_t srcRowBytes,
                           const LXPixelFormat srcPxFormat,
                           uint8_t * LXRESTRICT aDstBuffer,
                           const uint32_t dstW, const uint32_t dstH, const size_t dstRowBytes,
                           const LXPixelFormat dstPxFormat,
                           LXUInteger srcColorSpaceID,
                           LXUInteger dstColorSpaceID,
                           LXUInteger srcYCbCrFormatID,
                           LXUInteger dstYCbCrFormatID,
                           LXError *outError)
{
    const uint32_t realW = MIN(srcW, dstW);
    const uint32_t realH = MIN(srcH, dstH);
    const size_t realRowBytes = MIN(srcRowBytes, dstRowBytes);
    const LXInteger dstBytesPerPixel = LXBytesPerPixelForPixelFormat(dstPxFormat);

    if (realH < 1 || realW < 1 || realRowBytes < 1) {
        LXErrorSet(outError, 2801, "invalid image size or rowBytes");
        return NO;
    }
    
    //printf("lx pxConvert: pf %i / %i, rb %ld / %ld, color %ld / %ld, ycbcrformat %ld\n", (int)srcPxFormat, (int)dstPxFormat, (long)srcRowBytes, (long)dstRowBytes,
    //                            (long)srcColorSpaceID, (long)dstColorSpaceID, (long)srcYCbCrFormatID);

    if (srcPxFormat == dstPxFormat && srcRowBytes == dstRowBytes) {
        // we can only do a one-shot memcpy if the rowbytes actually matches the expected image size.
        // if this condition isn't filled, the code will fallback on the row-by-row memcpy further down in this function.
        if (srcRowBytes == srcW*dstBytesPerPixel) {
            _lx_memcpy_aligned(aDstBuffer, aSrcBuffer, realRowBytes * realH);
            return YES;
        }
    }
    
    if (srcPxFormat == kLX_RGBA_INT8 && dstPxFormat == kLX_ARGB_INT8) {
        LXPxConvert_RGBA_to_ARGB_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes);
        return YES;
    }
    else if (srcPxFormat == kLX_ARGB_INT8 && dstPxFormat == kLX_RGBA_INT8) {
        LXPxConvert_ARGB_to_RGBA_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes);    
        return YES;
    }
    else if (    (srcPxFormat == kLX_ARGB_INT8 && dstPxFormat == kLX_BGRA_INT8)
              || (srcPxFormat == kLX_BGRA_INT8 && dstPxFormat == kLX_ARGB_INT8)) {
        LXPxConvert_RGBA_to_reverse_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes);    
        return YES;
    }
    else if (    (srcPxFormat == kLX_RGBA_INT8 && dstPxFormat == kLX_BGRA_INT8)
              || (srcPxFormat == kLX_BGRA_INT8 && dstPxFormat == kLX_RGBA_INT8)) {
        LXPxConvert_RGBA_to_reverse_BGRA_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes);    
        return YES;
    }
    else if (srcPxFormat == kLX_Luminance_INT8 && dstPxFormat == kLX_RGBA_INT8) {
        LXPxConvert_lum_to_RGBA_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, 1, aDstBuffer, dstRowBytes);
        return YES; 
    }
    else if (srcPxFormat == kLX_Luminance_INT8 && dstPxFormat == kLX_ARGB_INT8) {
        LXPxConvert_lum_to_ARGB_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, 1, aDstBuffer, dstRowBytes); 
        return YES;
    }
    
    else if (srcPxFormat == kLX_RGBA_INT8 && dstPxFormat == kLX_YCbCr422_INT8) {
        LXPxConvert_RGBA_to_YCbCr422(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes, dstColorSpaceID);
        return YES;
    }
    else if (srcPxFormat == kLX_ARGB_INT8 && dstPxFormat == kLX_YCbCr422_INT8) {
        LXPxConvert_ARGB_to_YCbCr422(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes, dstColorSpaceID);
        return YES;
    }
    
    else if (srcPxFormat == kLX_YCbCr422_INT8 && dstPxFormat == kLX_RGBA_INT8) {
        ///LXPrintf("%s -- will convert YUV to RGBA, srcdata %p, srcformat %i\n", __func__, aSrcBuffer, srcYCbCrFormatID);
        switch (srcYCbCrFormatID) {
            default:
                LXPxConvert_YCbCr422_to_RGBA_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes, dstColorSpaceID);
                break;
            case kLX_YCbCrFormat_YUY2:
                LXPxConvert_YCbCr422_YUY2_to_RGBA_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes, dstColorSpaceID);
                break;
        }
        return YES;
    }
    
    else if (srcPxFormat == kLX_YCbCr422_INT8 && dstPxFormat == kLX_RGBA_FLOAT16) {
        LXPxConvert_YCbCr422_to_RGBA_float16_rawYUV(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, (LXHalf *)aDstBuffer, dstRowBytes);
        return YES;
    }
    else if (srcPxFormat == kLX_RGBA_FLOAT16 && dstPxFormat == kLX_YCbCr422_INT8) {
        LXPxConvert_RGBA_float16_rawYUV_to_YCbCr422(realW, realH, (LXHalf *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes);    
        return YES;
    }
    else if (srcPxFormat == kLX_YCbCr422_INT8 && dstPxFormat == kLX_RGBA_FLOAT32) {
        // don't have a direct routine for this, so must go through temp float16 buffer
        size_t tempRowBytes = LXAlignedRowBytes(realW * 4 * sizeof(LXHalf));
        LXHalf *tempBuf = _lx_malloc(tempRowBytes * realH);
        LXPxConvert_YCbCr422_to_RGBA_float16_rawYUV(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, tempBuf, tempRowBytes);
        
        LXUInteger y;
        for (y = 0; y < realH; y++) {
            uint8_t *s = (uint8_t *)tempBuf + tempRowBytes*y;
            uint8_t *d = (uint8_t *)aDstBuffer + dstRowBytes*y;
            LXConvertHalfToFloatArray((LXHalf *)s, (float *)d, realW * 4);
        }
        
        _lx_free(tempBuf);
        return YES;
    }
    
    else if (srcPxFormat == kLX_RGBA_INT8 && dstPxFormat == kLX_Luminance_INT8) {
        LXPxConvert_RGBA_to_monochrome_lum_int8(realW, realH, (uint8_t *)aSrcBuffer, srcRowBytes, aDstBuffer, dstRowBytes, 1,
                                                kLX_709_toY__R, kLX_709_toY__G, kLX_709_toY__B);  // sRGB also uses 709 values, so it's the default choice
        return YES;
    }

    // direct image conversion func not available, so do by row
    
    LXSuccess success = YES;
    uint8_t *tempRowBuf = NULL;
	LXUInteger y;
	for (y = 0; y < realH; y++) {	
        LXUInteger x;
        LXBool didHandle = NO;
		uint8_t *srcBuf = (uint8_t *)aSrcBuffer + srcRowBytes*y;
		uint8_t *dstBuf = (uint8_t *)aDstBuffer + dstRowBytes*y;
		
        if (dstPxFormat == srcPxFormat) {
            _lx_memcpy_aligned(dstBuf, srcBuf, dstBytesPerPixel * MIN(srcW, dstW));
            didHandle = YES;
        }
        
        else if (dstPxFormat == kLX_RGBA_INT8 && srcPxFormat == kLX_RGBA_FLOAT16) {
            LXPxConvert_float16_to_int8((LXHalf *)srcBuf, (uint8_t *)dstBuf, realW * 4);
            didHandle = YES;
        }
        else if (dstPxFormat == kLX_RGBA_INT8 && srcPxFormat == kLX_RGBA_FLOAT32) {
            LXPxConvert_float32_to_int8((float *)srcBuf, (uint8_t *)dstBuf, realW * 4);
            didHandle = YES;
        }

        // --- RGBA, float 16 ---
        else if (dstPxFormat == kLX_RGBA_FLOAT16) {
            if (srcPxFormat == kLX_RGBA_FLOAT32) {
                LXConvertFloatToHalfArray((float *)srcBuf, (LXHalf *)dstBuf, realW * 4);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_RGBA_INT8) {
                LXPxConvert_int8_to_float16((uint8_t *)srcBuf, (LXHalf *)dstBuf, realW * 4);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_ARGB_INT8) {
                LXPxConvert_int8_to_float16((uint8_t *)srcBuf, (LXHalf *)dstBuf, realW * 4);
                LXHalf *dst = (LXHalf *)dstBuf;
                for (x = 0; x < realW; x++) {
                    LXHalf a = dst[0];
                    LXHalf r = dst[1];
                    LXHalf g = dst[2];
                    LXHalf b = dst[3];
                    dst[0] = r;
                    dst[1] = g;
                    dst[2] = b;
                    dst[3] = a;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_BGRA_INT8) {
                LXPxConvert_int8_to_float16((uint8_t *)srcBuf, (LXHalf *)dstBuf, realW * 4);
                LXHalf *dst = (LXHalf *)dstBuf;
                for (x = 0; x < realW; x++) {
                    LXHalf b = dst[0];
                    LXHalf r = dst[2];
                    dst[0] = r;
                    dst[2] = b;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_FLOAT16) {
                LXHalf *src = (LXHalf *)srcBuf;
                LXHalf *dst = (LXHalf *)dstBuf;
                const LXHalf one = LXHalfFromFloat(1.0f);
                for (x = 0; x < realW; x++) {
                    LXHalf v = *src;
                    src++;
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = one;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_FLOAT32) {
                float *src = (float *)srcBuf;
                LXHalf *dst = (LXHalf *)dstBuf;
                const LXHalf one = LXHalfFromFloat(1.0f);
                for (x = 0; x < realW; x++) {
                    LXHalf v = LXHalfFromFloat(*src);
                    src++;
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = one;
                    dst += 4;
                }
                didHandle = YES;
            }
        }
        
        // --- RGBA, float 32 ---
        else if (dstPxFormat == kLX_RGBA_FLOAT32) {
            if (srcPxFormat == kLX_RGBA_FLOAT16) {
                LXConvertHalfToFloatArray((LXHalf *)srcBuf, (float *)dstBuf, realW * 4);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_RGBA_INT8) {
                LXPxConvert_int8_to_float32((uint8_t *)srcBuf, (float *)dstBuf, realW * 4);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_ARGB_INT8) {
                LXPxConvert_int8_to_float32((uint8_t *)srcBuf, (float *)dstBuf, realW * 4);
                float *dst = (float *)dstBuf;
                for (x = 0; x < realW; x++) {
                    float a = dst[0];
                    float r = dst[1];
                    float g = dst[2];
                    float b = dst[3];
                    dst[0] = r;
                    dst[1] = g;
                    dst[2] = b;
                    dst[3] = a;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_BGRA_INT8) {
                LXPxConvert_int8_to_float32((uint8_t *)srcBuf, (float *)dstBuf, realW * 4);
                float *dst = (float *)dstBuf;
                for (x = 0; x < realW; x++) {
                    float b = dst[0];
                    float r = dst[2];
                    dst[0] = r;
                    dst[2] = b;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_FLOAT16) {
                LXHalf *src = (LXHalf *)srcBuf;
                float *dst = (float *)dstBuf;
                const float one = 1.0;
                for (x = 0; x < realW; x++) {
                    float v = LXFloatFromHalf(*src);
                    src++;
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = one;
                    dst += 4;
                }
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_FLOAT32) {
                float *src = (float *)srcBuf;
                float *dst = (float *)dstBuf;
                const float one = 1.0;
                for (x = 0; x < realW; x++) {
                    float v = *src;
                    src++;
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = one;
                    dst += 4;
                }
                didHandle = YES;
            }
        }
        
        // --- luminance depth conversion ---
        else if (dstPxFormat == kLX_Luminance_INT8 && LXPXF_ISLUMINANCE(srcPxFormat)) {
            if (srcPxFormat == kLX_Luminance_FLOAT16) {
                /*if ( !tempRowBuf)  tempRowBuf = _lx_malloc(realW * sizeof(float));
                
                LXConvertHalfToFloatArray((LXHalf *)srcBuf, (float *)tempRowBuf, realW);
                LXPxConvert_float32_to_int8((float *)tempRowBuf, (uint8_t *)dstBuf, realW);
                */
                LXPxConvert_float16_to_int8((LXHalf *)srcBuf, (uint8_t *)dstBuf, realW);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_FLOAT32) {
                LXPxConvert_float32_to_int8((float *)srcBuf, (uint8_t *)dstBuf, realW);
                didHandle = YES;
            }
        }
        else if (dstPxFormat == kLX_Luminance_FLOAT16 && LXPXF_ISLUMINANCE(srcPxFormat)) {
            if (srcPxFormat == kLX_Luminance_FLOAT32) {
                LXConvertFloatToHalfArray((float *)srcBuf, (LXHalf *)dstBuf, realW);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_INT8) {
                /*if ( !tempRowBuf)  tempBuf = _lx_malloc(realW * sizeof(float));
                
                LXPxConvert_int8_to_float32((uint8_t *)srcBuf, (float *)tempRowBuf, realW);
                LXConvertFloatToHalfArray((float *)tempRowBuf, (LXHalf *)dstBuf, realW);
                */
                LXPxConvert_int8_to_float16((uint8_t *)srcBuf, (LXHalf *)dstBuf, realW);
                didHandle = YES;
            }
        }
        else if (dstPxFormat == kLX_Luminance_FLOAT32 && LXPXF_ISLUMINANCE(srcPxFormat)) {
            if (srcPxFormat == kLX_Luminance_FLOAT16) {
                LXConvertHalfToFloatArray((LXHalf *)srcBuf, (float *)dstBuf, realW);
                didHandle = YES;
            }
            else if (srcPxFormat == kLX_Luminance_INT8) {
                LXPxConvert_int8_to_float32((uint8_t *)srcBuf, (float *)dstBuf, realW);
                didHandle = YES;
            }
        }
        
        // --- arbitrary color to monochrome, YUV or BGRA ---
        
        else if (dstPxFormat == kLX_Luminance_INT8 || dstPxFormat == kLX_YCbCr422_INT8 || dstPxFormat == kLX_BGRA_INT8) {
            // at this point we know that the source format isn't RGBA_int8, so convert first to RGBA_int8 through a tempbuf
            const size_t tempRowBytes = realW * 4;
            
            if ( !tempRowBuf)  tempRowBuf = _lx_malloc(tempRowBytes);
            
            LXBool didConvert = LXPxConvert_Any_((uint8_t *)srcBuf, realW, 1, srcRowBytes, srcPxFormat,
                                                 (uint8_t *)tempRowBuf, realW, 1, tempRowBytes, kLX_RGBA_INT8,
                                                 srcColorSpaceID, 0, srcYCbCrFormatID, 0, outError);
            if (didConvert) {
                if (dstPxFormat == kLX_Luminance_INT8) {
                    LXPxConvert_RGBA_to_monochrome_lum_int8(realW, 1,
                                                            (uint8_t *)tempRowBuf, tempRowBytes, (uint8_t *)dstBuf, dstRowBytes, 1,
                                                            kLX_709_toY__R, kLX_709_toY__G, kLX_709_toY__B);  // sRGB also uses 709 values, so it's the default choice
                } else if (dstPxFormat == kLX_YCbCr422_INT8) {
                    LXPxConvert_RGBA_to_YCbCr422(realW, 1,
                                                 (uint8_t *)tempRowBuf, tempRowBytes, (uint8_t *)dstBuf, dstRowBytes, dstColorSpaceID);
                } else {
                    LXPxConvert_RGBA_to_reverse_BGRA_int8(realW, 1,
                                                          (uint8_t *)tempRowBuf, tempRowBytes, (uint8_t *)dstBuf, dstRowBytes);
                }
                didHandle = YES;
            }
        }
                
        if ( !didHandle) {
            ///printf("** %s: unsupported conversion (%i -> %i)\n", __func__, srcPxFormat, dstPxFormat);
            char msg[256];
            memset(msg, 0, 256);
            sprintf(msg, "unsupported conversion (%i -> %i)", (int)srcPxFormat, (int)dstPxFormat);
            LXErrorSet(outError, 2820, msg);
            success = NO;
            break;
        }
	}

    if (tempRowBuf) _lx_free(tempRowBuf);

    return success;
}


LXSuccess LXPixelBufferGetDataWithPixelFormatConversion(LXPixelBufferRef srcPixbuf,
                                                        uint8_t *dstBuf,
                                                        const uint32_t dstW, const uint32_t dstH,
                                                        const size_t dstRowBytes,
                                                        const LXPixelFormat dstPxFormat, LXMapPtr dstProperties, LXError *outError)
{
    if ( !srcPixbuf) {
        LXErrorSet(outError, 2001, "no src image");
        return NO;
    }
    
    const uint32_t dstBytesPerPixel = LXBytesPerPixelForPixelFormat(dstPxFormat);
    
    const uint32_t srcW = LXPixelBufferGetWidth(srcPixbuf);
    const uint32_t srcH = LXPixelBufferGetHeight(srcPixbuf);
    const LXPixelFormat srcPxFormat = LXPixelBufferGetPixelFormat(srcPixbuf);
	size_t srcRowBytes = 0;
	int32_t srcBytesPerPixel = 0;
	uint8_t *srcBuf = (uint8_t *)LXPixelBufferLockPixels(srcPixbuf, &srcRowBytes, &srcBytesPerPixel, NULL);
    #pragma unused (dstBytesPerPixel, srcBytesPerPixel)

    LXUInteger srcColorSpaceID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_ColorSpaceEncoding);
    LXUInteger srcYCbCrFormatID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_YCbCrPixelFormatID);

    LXSuccess success = LXPxConvert_Any_(srcBuf, srcW, srcH, srcRowBytes, srcPxFormat,
                                         dstBuf, dstW, dstH, dstRowBytes, dstPxFormat,
                                         srcColorSpaceID, 0,
                                         srcYCbCrFormatID, 0,
                                         outError);
                     
	LXPixelBufferUnlockPixels(srcPixbuf);

    if ( !success) {
        char msg[512];
        sprintf(msg, "unable to convert pixel data: %lu -> %lu", (unsigned long)srcPxFormat, (unsigned long)dstPxFormat);
        LXErrorSet(outError, 2757, msg);
    }    
    return success;
}


void LXFitRegionToBufferAndClearOutside(int32_t *pRegionX, int32_t *pRegionY, int32_t *pRegionW, int32_t *pRegionH,
                                        uint8_t **pDstRegionBuf,
                                        const size_t dstRowBytes, const size_t dstBytesPerPixel,
                                        const uint32_t srcW, const uint32_t srcH)
{
    int32_t regionX = *pRegionX;
    int32_t regionY = *pRegionY;
    int32_t regionW = *pRegionW;
    int32_t regionH = *pRegionH;
    uint8_t *fittedDstBuf = *pDstRegionBuf;
    LXInteger i, n;
    
    LXInteger leftMarginClearBytes = 0;

    // for parts of the region outside the source pixel buffer, clear the pixels and modify the region's bounds accordingly
    if (regionY < 0) {
        n = MIN(-regionY, regionH);
        for (i = 0; i < n; i++) {
            memset(fittedDstBuf, 0, dstRowBytes);
            fittedDstBuf += dstRowBytes;
        }
        regionY = 0;
        regionH -= n;
    }
    if (regionX < 0) {
        n = MIN(-regionX, regionW);
        leftMarginClearBytes = dstBytesPerPixel*n;
        for (i = 0; i < regionH; i++) {
            memset(fittedDstBuf + dstRowBytes*i,  0,  leftMarginClearBytes);
        }
        fittedDstBuf += leftMarginClearBytes;
        regionX = 0;
        regionW -= n;
    }
    
    if (regionY+regionH > (LXInteger)srcH) {
        n = MIN(regionH, (regionY+regionH) - srcH);
        LXInteger offset = MAX(0, (LXInteger)srcH - regionY);
        ///printf("... y over h: n %i, offset %i (%i, %i)\n", n, offset, srcH, regionY);
        for (i = 0; i < n; i++) {
            memset(fittedDstBuf + dstRowBytes*(i + offset), 0, dstRowBytes - leftMarginClearBytes);
        }
        regionH -= n;
    }
    if (regionX+regionW > (LXInteger)srcW) {
        n = MIN(regionW, (regionX+regionW) - srcW);
        LXInteger offset = MAX(0, (LXInteger)srcW - regionX);
        ///printf("... x over w: n %i, offset %i\n", (int)n, (int)offset);
        for (i = 0; i < regionH; i++) {
            memset(fittedDstBuf + dstRowBytes*i + dstBytesPerPixel*offset,  0,  dstBytesPerPixel*n);
        }
        regionW -= n;
    }
    
    *pRegionX = regionX;
    *pRegionY = regionY;
    *pRegionW = regionW;
    *pRegionH = regionH;
    *pDstRegionBuf = fittedDstBuf;
}

LXSuccess LXPixelBufferGetRegionWithPixelFormatConversion(LXPixelBufferRef srcPixbuf,
                                                        LXRect region,
                                                        uint8_t *dstRegionBuf,
                                                        const size_t dstRowBytes,
                                                        const LXPixelFormat dstPxFormat, LXMapPtr dstProperties, LXError *outError)
{
    if ( !srcPixbuf) {
        LXErrorSet(outError, 2001, "no src image");
        return NO;
    }
    
    const uint32_t dstBytesPerPixel = LXBytesPerPixelForPixelFormat(dstPxFormat);
    
    const uint32_t srcW = LXPixelBufferGetWidth(srcPixbuf);
    const uint32_t srcH = LXPixelBufferGetHeight(srcPixbuf);
    const LXPixelFormat srcPxFormat = LXPixelBufferGetPixelFormat(srcPixbuf);
    
    int32_t regionX = lround(region.x);
    int32_t regionY = lround(region.y);
    int32_t regionW = lround(region.w);
    int32_t regionH = lround(region.h);

    uint8_t *fittedDstBuf = dstRegionBuf;
    
    //memset(dstRegionBuf, 128, regionH*dstRowBytes);  // for testing: fill with 8-bit grey
    
    if (dstProperties) {
        double xoff = 0.0;
        double yoff = 0.0;
        LXMapGetDouble(dstProperties, "offsetX", &xoff);
        LXMapGetDouble(dstProperties, "offsetY", &yoff);
        regionX -= lround(xoff);
        regionY -= lround(yoff);
    }
    
    LXFitRegionToBufferAndClearOutside(&regionX, &regionY, &regionW, &regionH,  &fittedDstBuf,  dstRowBytes, dstBytesPerPixel,  srcW, srcH);

    
    LXSuccess success = YES;

    if (regionW > 0 && regionH > 0) {
        size_t srcRowBytes = 0;
        int32_t srcBytesPerPixel = 0;
        uint8_t *srcBuf = (uint8_t *)LXPixelBufferLockPixels(srcPixbuf, &srcRowBytes, &srcBytesPerPixel, NULL);
        #pragma unused (srcBytesPerPixel)

        uint8_t *srcRegionBuf = srcBuf + (srcRowBytes * regionY) + (srcBytesPerPixel * regionX);

        LXUInteger srcColorSpaceID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_ColorSpaceEncoding);
        LXUInteger srcYCbCrFormatID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_YCbCrPixelFormatID);

        success = LXPxConvert_Any_(srcRegionBuf, regionW, regionH, srcRowBytes, srcPxFormat,
                                             fittedDstBuf, regionW, regionH, dstRowBytes, dstPxFormat,
                                             srcColorSpaceID, 0,
                                             srcYCbCrFormatID, 0,
                                             outError);
                         
        LXPixelBufferUnlockPixels(srcPixbuf);

        if ( !success) {
            char msg[512];
            sprintf(msg, "unable to convert pixel data: %lu -> %lu", (unsigned long)srcPxFormat, (unsigned long)dstPxFormat);
            LXErrorSet(outError, 2757, msg);
        }
    }
    return success;
}

LXSuccess LXPixelBufferWriteRegionWithPixelFormatConversion(LXPixelBufferRef dstPixbuf,
                                                        LXRect region,
                                                        const uint8_t *srcRegionBuf,
                                                        const size_t srcRowBytes,
                                                        const LXPixelFormat srcPxFormat, LXMapPtr srcProperties, LXError *outError)
{
    if ( !dstPixbuf) {
        LXErrorSet(outError, 2001, "no dst image");
        return NO;
    }
    
    const uint32_t srcBytesPerPixel = LXBytesPerPixelForPixelFormat(srcPxFormat);
    
    const uint32_t dstW = LXPixelBufferGetWidth(dstPixbuf);
    const uint32_t dstH = LXPixelBufferGetHeight(dstPixbuf);
    const LXPixelFormat dstPxFormat = LXPixelBufferGetPixelFormat(dstPixbuf);
    
    const int32_t regionW = round(region.w);
    const int32_t regionH = round(region.h);
    const int32_t regionX = round(region.x);
    const int32_t regionY = round(region.y);
    
    if ((regionX + regionW) > dstW || (regionY + regionH) > dstH) {
        char msg[512];
        sprintf(msg, "invalid region (image size %u * %u, region (%u, %u, %u, %u))", dstW, dstH, regionX, regionY, regionW, regionH);
        LXErrorSet(outError, 2755, msg);
        return NO;
    }
    
	size_t dstRowBytes = 0;
	int32_t dstBytesPerPixel = 0;
	uint8_t *dstBuf = (uint8_t *)LXPixelBufferLockPixels(dstPixbuf, &dstRowBytes, &dstBytesPerPixel, NULL);
    #pragma unused (dstBytesPerPixel, srcBytesPerPixel)

    uint8_t *dstRegionBuf = dstBuf + (dstRowBytes * regionY) + (dstBytesPerPixel * regionX);

    LXSuccess success = LXPxConvert_Any_(srcRegionBuf, regionW, regionH, srcRowBytes, srcPxFormat,
                                         dstRegionBuf, regionW, regionH, dstRowBytes, dstPxFormat,
                                         0, 0, 0, 0,  // last arguments are colorspaceID, etc.
                                         outError);
                     
	LXPixelBufferUnlockPixels(dstPixbuf);
    
    if ( !success) {
        char msg[512];
        sprintf(msg, "unable to convert pixel data: %lu -> %lu", (unsigned long)srcPxFormat, (unsigned long)dstPxFormat);
        LXErrorSet(outError, 2757, msg);
    }
    return success;
}

LXSuccess LXPixelBufferWriteDataWithPixelFormatConversion(LXPixelBufferRef dstPixbuf,
                                                        const uint8_t *srcBuf,
                                                        const uint32_t srcW, const uint32_t srcH,
                                                        const size_t srcRowBytes,
                                                        const LXPixelFormat srcPxFormat, LXMapPtr srcProperties, LXError *outError)
{
    if ( !dstPixbuf) {
        LXErrorSet(outError, 2001, "no dst image");
        return NO;
    }
    
    const uint32_t srcBytesPerPixel = LXBytesPerPixelForPixelFormat(srcPxFormat);
    
    const uint32_t dstW = LXPixelBufferGetWidth(dstPixbuf);
    const uint32_t dstH = LXPixelBufferGetHeight(dstPixbuf);
    const LXPixelFormat dstPxFormat = LXPixelBufferGetPixelFormat(dstPixbuf);
	size_t dstRowBytes = 0;
	int32_t dstBytesPerPixel = 0;
	uint8_t *dstBuf = (uint8_t *)LXPixelBufferLockPixels(dstPixbuf, &dstRowBytes, &dstBytesPerPixel, NULL);
    #pragma unused (dstBytesPerPixel, srcBytesPerPixel)

    LXSuccess success = LXPxConvert_Any_(srcBuf, srcW, srcH, srcRowBytes, srcPxFormat,
                                         dstBuf, dstW, dstH, dstRowBytes, dstPxFormat,
                                         0, 0, 0, 0,  // last arguments are colorspaceID, etc.
                                         outError);
                     
	LXPixelBufferUnlockPixels(dstPixbuf);

    if ( !success) {
        char msg[512];
        sprintf(msg, "unable to convert pixel data: %lu -> %lu", (unsigned long)srcPxFormat, (unsigned long)dstPxFormat);
        LXErrorSet(outError, 2757, msg);
    }    
    return success;
}

LXSuccess LXPixelBufferCopyPixelBufferWithPixelFormatConversion(LXPixelBufferRef dstPixbuf, LXPixelBufferRef srcPixbuf, LXError *outError)
{
    if ( !srcPixbuf) {
        LXErrorSet(outError, 2001, "no source image");
        return NO;
    }
    if ( !dstPixbuf) {
        LXErrorSet(outError, 2001, "no dst image");
        return NO;
    }
    
    const int srcW = LXPixelBufferGetWidth(srcPixbuf);
    const int dstW = LXPixelBufferGetWidth(dstPixbuf);
    const int srcH = LXPixelBufferGetHeight(srcPixbuf);
    const int dstH = LXPixelBufferGetHeight(dstPixbuf);
    
    if (srcW != dstW || srcH != dstH) {
        LXErrorSet(outError, 2750, "image sizes do not match (use scale functions instead)");
        return NO;
    }
    
    const LXPixelFormat srcPxFormat = LXPixelBufferGetPixelFormat(srcPixbuf);
    const LXPixelFormat dstPxFormat = LXPixelBufferGetPixelFormat(dstPixbuf);
    
    LXUInteger srcColorSpaceID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_ColorSpaceEncoding);
    LXUInteger srcYCbCrFormatID = LXPixelBufferGetIntegerAttachment(srcPixbuf, kLXPixelBufferAttachmentKey_YCbCrPixelFormatID);
    
    ///LXPrintf("%s -- pixbuf %p -- colorspace %i, yuv format %i\n", __func__, srcPixbuf, srcColorSpaceID, srcYCbCrFormatID);
    
    size_t srcRowBytes = 0;
    size_t dstRowBytes = 0;
    uint8_t *srcBuf = (uint8_t *) LXPixelBufferLockPixels(srcPixbuf, &srcRowBytes, NULL, NULL);
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(dstPixbuf, &dstRowBytes, NULL, NULL);
    
    LXSuccess success = LXPxConvert_Any_(srcBuf, srcW, srcH, srcRowBytes, srcPxFormat,
                                         dstBuf, dstW, dstH, dstRowBytes, dstPxFormat,
                                         srcColorSpaceID, 0,
                                         srcYCbCrFormatID, 0,
                                         outError);
    
    LXPixelBufferUnlockPixels(srcPixbuf);
    LXPixelBufferUnlockPixels(dstPixbuf);
    
    // the convertAny function doesn't do yCbCr format conversion, so we must pass on this flag
    if (dstPxFormat == kLX_YCbCr422_INT8 && srcPxFormat == kLX_YCbCr422_INT8) {
        LXPixelBufferSetIntegerAttachment(dstPixbuf, kLXPixelBufferAttachmentKey_YCbCrPixelFormatID, srcYCbCrFormatID);
    }
    
    ///LXPrintf("-- converted pixbuf %p\n", srcPixbuf);

    return success;
}
