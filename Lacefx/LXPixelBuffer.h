/*
 *  LXPixelBuffer.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPIXELBUFFER_H_
#define _LXPIXELBUFFER_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"


typedef struct {
    uint32_t version;  // set to 0 or 1
    LXBool (*shouldLock)(LXPixelBufferRef pixbuf, void *);
    void (*didUnlock)(LXPixelBufferRef pixbuf, void *);
    void (*willDestroy)(LXPixelBufferRef pixbuf, void *);
} LXPixelBufferLockCallbacks;


typedef void (*LXGenericImageDataCallbackPtr)(uint8_t *buf, uint32_t w, uint32_t h, size_t rowBytes, LXPixelFormat pxf, LXMapPtr props, void *userData);



#ifdef __cplusplus
extern "C" {
#endif

// convenience function.
// all LXPixelFormat values have an integer bytes-per-pixel value,
// funky padded formats are best dealt when read/written.
LXEXPORT LXUInteger LXBytesPerPixelForPixelFormat(LXPixelFormat pf);

// rounds the given value up to a platform-specific boundary (usually 16-byte alignment)
LXEXPORT size_t LXAlignedRowBytes(size_t rowBytes);


#pragma mark --- LXPixelBuffer public API methods ---

// the returned object is retained.
// giving a pool is not necessary (the argument can be NULL),
// but it allows the pool to recycle an existing memory space when possible.
LXEXPORT LXPixelBufferRef LXPixelBufferCreate(LXPoolRef pool,
                                              uint32_t w, uint32_t h,
                                              LXPixelFormat pixelFormat,
                                              LXError *outError);

// returned object is retained.
// kLXStorageHint_ClientStorage can be used to indicate that the pixel buffer shouldn't free
// the data buffer when destroyed. (without this hint, the pixel buffer owns the data and will free it using _lx_free!)
// other storage hint flags like "prefer DMA to caching" may be used as hints for LXTexture creation.
LXEXPORT LXPixelBufferRef LXPixelBufferCreateForData(uint32_t w, uint32_t h,
                                                     LXPixelFormat pixelFormat,
                                                     size_t rowBytes,
                                                     uint8_t *data,
                                                     LXUInteger storageHint,
                                                     LXError *outError);

// this create method will automatically call installed file handler plugins to open custom file types.
// the given path can begin with an UTF-16 byte order marker (BOM) to indicate its endianness
// (this is useful for e.g. storing image paths in a cross-platform plugin).
// properties can contain hints about how to treat the image data (but these are file format dependent); see "format request keys" below.
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromFileAtPath(LXUnibuffer path, LXMapPtr properties, LXError *outError);

LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromFileInMemory(const uint8_t *data, size_t len, const char *formatUTI, LXMapPtr properties, LXError *outError);

LXEXPORT LXPixelBufferRef LXPixelBufferRetain(LXPixelBufferRef buffer);
LXEXPORT void LXPixelBufferRelease(LXPixelBufferRef buffer);

LXEXPORT LXSize LXPixelBufferGetSize(LXPixelBufferRef buffer);
LXEXPORT LXPixelFormat LXPixelBufferGetPixelFormat(LXPixelBufferRef buffer);
LXEXPORT uint32_t LXPixelBufferGetWidth(LXPixelBufferRef buffer);
LXEXPORT uint32_t LXPixelBufferGetHeight(LXPixelBufferRef buffer);

LXEXPORT LXBool LXPixelBufferMatchesSize(LXPixelBufferRef surface, LXSize size);

LXEXPORT void LXPixelBufferSetPrefersHWCaching(LXPixelBufferRef buffer, LXBool b);  // if true, created texture prefers GPU VRAM caching
LXEXPORT LXBool LXPixelBufferGetPrefersHWCaching(LXPixelBufferRef buffer);

LXEXPORT LXTextureRef LXPixelBufferGetTexture(LXPixelBufferRef buffer, LXError *outError);

LXEXPORT uint8_t *LXPixelBufferLockPixels(LXPixelBufferRef buffer, size_t *outRowBytes, int32_t *outBytesPerPixel, LXError *outError);
LXEXPORT void LXPixelBufferUnlockPixels(LXPixelBufferRef buffer);

LXEXPORT void LXPixelBufferSetLockCallbacks(LXPixelBufferRef buffer, LXPixelBufferLockCallbacks *callbacks, void *userData);

LXEXPORT void LXPixelBufferInvalidateCaches(LXPixelBufferRef buffer);

// -- simple custom metadata --
LXEXPORT void LXPixelBufferSetIntegerAttachment(LXPixelBufferRef buffer, const char *key, LXInteger value);
LXEXPORT void LXPixelBufferSetFloatAttachment(LXPixelBufferRef buffer, const char *key, LXFloat value);
LXEXPORT LXInteger LXPixelBufferGetIntegerAttachment(LXPixelBufferRef buffer, const char *key);
LXEXPORT LXFloat LXPixelBufferGetFloatAttachment(LXPixelBufferRef buffer, const char *key);

// -- serialisation --
LXEXPORT size_t LXPixelBufferGetSerializedDataSize(LXPixelBufferRef r);
LXEXPORT LXSuccess LXPixelBufferSerialize(LXPixelBufferRef r, uint8_t *buf, size_t bufLen);
LXEXPORT LXSuccess LXPixelBufferSerializeDeflated(LXPixelBufferRef r, uint8_t **outBuf, size_t *outBufLen);

LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromSerializedData(const uint8_t *buf, size_t dataLen, LXError *outError);

//  -- transform utils --
LXEXPORT LXPixelBufferRef LXPixelBufferCreateScaled(LXPixelBufferRef srcPixbuf, uint32_t dstW, uint32_t dstH, LXError *outError);

// -- pixel format conversion utils --

// easiest way to convert pixels: copy from a buffer to another with conversion applied as necessary
//
LXEXPORT LXSuccess LXPixelBufferCopyPixelBufferWithPixelFormatConversion(LXPixelBufferRef dstPixbuf, LXPixelBufferRef srcPixbuf, LXError *outError);

// functions for writing from / reading into a custom buffer.
// "region" versions are useful for e.g. tiled rendering.
// the 'properties' argument is intended to provide an extension mechanism (e.g. for specifying the colorspace of the data).
//
LXEXPORT LXSuccess LXPixelBufferWriteDataWithPixelFormatConversion(LXPixelBufferRef dstPixbuf,
                                                                const uint8_t *srcBuf,
                                                                const uint32_t srcW, const uint32_t srcH,
                                                                const size_t srcRowBytes,
                                                                const LXPixelFormat srcPxFormat, LXMapPtr srcProperties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferGetDataWithPixelFormatConversion(LXPixelBufferRef srcPixbuf,
                                                                uint8_t *dstBuffer,
                                                                const uint32_t dstW, const uint32_t dstH,
                                                                const size_t dstRowBytes,
                                                                const LXPixelFormat dstPxFormat, LXMapPtr dstProperties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferWriteRegionWithPixelFormatConversion(LXPixelBufferRef dstPixbuf,
                                                                LXRect region,
                                                                const uint8_t *srcRegionBuf,
                                                                const size_t srcRowBytes,
                                                                const LXPixelFormat srcPxFormat, LXMapPtr srcProperties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferGetRegionWithPixelFormatConversion(LXPixelBufferRef srcPixbuf,
                                                                LXRect region,
                                                                uint8_t *dstRegionBuf,
                                                                const size_t dstRowBytes,
                                                                const LXPixelFormat dstPxFormat, LXMapPtr dstProperties, LXError *outError);

// file export
//
LXEXPORT LXSuccess LXPixelBufferWriteAsFileToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError);


// attachment keys; these are primarily for internal use
//
LXEXPORT_CONSTVAR char * const kLXPixelBufferAttachmentKey_ColorSpaceEncoding;  // an LXColorSpaceEncoding value
LXEXPORT_CONSTVAR char * const kLXPixelBufferAttachmentKey_YCbCrPixelFormatID;

// format request keys (can be used for export)
//
LXEXPORT_CONSTVAR char * const kLXPixelBufferFormatRequestKey_AllowAlpha;
LXEXPORT_CONSTVAR char * const kLXPixelBufferFormatRequestKey_AllowYUV;
LXEXPORT_CONSTVAR char * const kLXPixelBufferFormatRequestKey_FileFormatID;
LXEXPORT_CONSTVAR char * const kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel;
LXEXPORT_CONSTVAR char * const kLXPixelBufferFormatRequestKey_CompressionQuality;

#ifdef __cplusplus
}
#endif

#endif


