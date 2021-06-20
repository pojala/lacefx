/*
 *  LXPixelBuffer_priv.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.11.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPIXELBUFFER_PRIV_H_
#define _LXPIXELBUFFER_PRIV_H_

#include "LXFileHandlers.h"

#if defined(LXPLATFORM_MAC) && defined(__OBJC__)
// on Mac, this file includes functions for converting between LXPixelBufferRef and NSBitmapImageRep
#import <AppKit/AppKit.h>
#endif

#define LX_HAS_LIBTIFF 0


enum {
    kLXImage_PNG = 1,
    kLXImage_BMP,
    kLXImage_TIFF,
    kLXImage_JPEG,
    kLXImage_JPEG2000,
    kLXImage_TGA,
    kLXImage_DPX,
    
    kLXImage_LXPix = 0x9000,  // the LXPixelBuffer serialized format written to disk (file extension ".lxpix" or ".lxp")
    
    kLXImage_HandledByPlugin = 0x10000
};

#ifdef __cplusplus
extern "C" {
#endif

// these functions are useful for file writers e.g. in LacqMediaCore
LXEXPORT char *LXCopyDefaultFileExtensionForImageUTI(const char *uti);

LXEXPORT LXUInteger LXImageTypeFromUTI(const char *uti, LXUInteger *outPluginIndex);


// platform-dependent implementation for common formats
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromPathUsingNativeAPI_(LXUnibuffer unipath, LXInteger imageType, LXMapPtr properties, LXError *outError);

LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromFileDataUsingNativeAPI_(const uint8_t *data, size_t len, LXInteger imageType, LXMapPtr properties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferWriteToPathUsingNativeAPI_(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXUInteger lxImageType, LXMapPtr properties, LXError *outError);

// DPX/Cineon reader and writer
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromDPXImageAtPath(LXUnibuffer unipath, LXMapPtr properties, LXError *outError);
LXEXPORT LXSuccess LXPixelBufferWriteAsDPXImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError);

// TIFF reader and writer
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromTIFFImageAtPath(LXUnibuffer unipath, LXMapPtr properties, LXError *outError);
LXEXPORT LXSuccess LXPixelBufferWriteAsTIFFImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError);

// JPEG reader and writer
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromJPEGImageInMemory(const uint8_t *jpegData, size_t jpegDataLen, LXMapPtr properties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferWriteAsJPEGImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferWriteAsJPEGImageInMemory_raw422_(LXPixelBufferRef pixbuf, uint8_t **outBuffer, size_t *outBufferSize,
                                                        size_t *outBytesWritten,
                                                        LXMapPtr properties, LXError *outError);


// generic pixel format conversion
LXEXPORT LXSuccess LXPxConvert_Any_(
                           const uint8_t * LXRESTRICT aSrcBuffer,
                           const uint32_t srcW, const uint32_t srcH, const size_t srcRowBytes,
                           const LXPixelFormat srcPxFormat,
                           uint8_t * LXRESTRICT aDstBuffer,
                           const uint32_t dstW, const uint32_t dstH, const size_t dstRowBytes,
                           const LXPixelFormat dstPxFormat,
                           LXUInteger srcColorSpaceID,
                           LXUInteger dstColorSpaceID,
                           LXUInteger srcYCbCrFormatID,
                           LXUInteger dstYCbCrFormatID,
                           LXError *outError);

// utility used by CopyRegion/GetRegion functions.
// modifies the region and its data pointer to fit within the source w/h, and clears the outside area to zero.
// regionW or regionH may become zero if the region is entirely outside the source.
LXEXPORT void LXFitRegionToBufferAndClearOutside(int32_t *pRegionX, int32_t *pRegionY, int32_t *pRegionW, int32_t *pRegionH,
                                                 uint8_t **pDstRegionBuf,
                                                 const size_t dstRowBytes, const size_t dstBytesPerPixel,
                                                 const uint32_t srcW, const uint32_t srcH);


#if defined(LXPLATFORM_MAC) && defined(__OBJC__)
LXEXPORT NSBitmapImageRep *LXPixelBufferCopyAsNSBitmapImageRep(LXPixelBufferRef srcPixbuf, LXError *outError);
    
LXEXPORT LXPixelBufferRef LXPixelBufferCreateFromNSBitmapImageRep(NSBitmapImageRep *rep, LXError *outError);
#endif
    
    
#ifdef __cplusplus
}
#endif

#endif
