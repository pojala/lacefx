/*
 *  LXPixelBuffer_tiff.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 1.8.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPixelBuffer.h"
#include "LXPixelBuffer_priv.h"

#include "LXStringUtils.h"
#include "LXImageFunctions.h"
#include "LXHalfFloat.h"
#include "LXColorFunctions.h"

#include <tiff.h>
#include <tiffio.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif



#pragma mark --- I/O helpers ---

static TIFF *lxTiffOpenUnipath(LXUnibuffer unibuf, const char *mode, LXError *outError)
{
    TIFF *tiff = NULL;

#if defined(LXPLATFORM_WIN)
    // libTIFF provides a wchar version of its open function on Win32
    char16_t *tempStr = LXStrUnibufferCopyAppendZero(unibuf);
    
    tiff = TIFFOpenW(tempStr, mode);
    _lx_free(tempStr);
#else

    char *utf8Str = LXStrCreateUTF8_from_UTF16(unibuf.unistr, unibuf.numOfChar16, NULL);
    
    tiff = TIFFOpen(utf8Str, mode);
    _lx_free(utf8Str);
#endif

    if( !tiff) {
        LXErrorSet(outError, 1808, (mode[0] == 'w') ? "unable to open file for writing" : "unable to open file");
    }
    return tiff;
}



#pragma mark --- read API ---

LXPixelBufferRef LXPixelBufferCreateFromTIFFImageAtPath(LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
    ///NSLog(@"%s...", __func__);
    
    TIFF *tiff;
    if ( !(tiff = lxTiffOpenUnipath(unipath, "r", outError))) {
        return NULL;
    }
    
    // only a handful of possible TIFF formats are supported here.
    // the caller (in LXPixelBuffer.c) will use the native API to try to open a file that we can't open.
    uint16_t bps = 0, spp = 0;
    uint32_t w, h;
    uint32_t rowsPerStrip;
    LXBool ok = YES;
    
    if (TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bps) == 0 || (bps != 8 && bps != 16)) {
        LXErrorSet(outError, 1862, "unsupported bit depth");
        ok = NO;
    }
    if (ok && (TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &spp) == 0 || (spp != 3 && spp != 4))) {
        LXErrorSet(outError, 1862, "unsupported sample count");
        ok = NO;
    }
    if (ok && (TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w) == 0 || w < 1)) {
        LXErrorSet(outError, 1862, "invalid image width");
        ok = NO;
    }
    if (ok && (TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h) == 0 || h < 1)) {
        LXErrorSet(outError, 1862, "invalid image height");
        ok = NO;
    }
    if (ok && (TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) == 0 || rowsPerStrip < 1)) {
        LXErrorSet(outError, 1862, "invalid rows per strip value");
        ok = NO;
    }
    
    if ( !ok) {
        TIFFClose(tiff);
        return NULL;
    }
    
    LXPixelFormat pxFormat = (bps == 16) ? kLX_RGBA_FLOAT16 : kLX_RGBA_INT8;
    
    LXSuccess success = NO;
    uint8_t *tempBuf = NULL;
    LXSuccess didLock = NO;
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, pxFormat, outError);
    if ( !newPixbuf)
        goto bail;
        
    size_t dstRowBytes = 0;
    uint8_t *dstData = LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, outError);
    if ( !dstData)
        goto bail;

    didLock = YES;

    // libTIFF's strip reading will return 16-bit pixels in the native byte order, so swapping not needed here    
    LXInteger stripSize = TIFFStripSize(tiff);
    LXInteger stripCount = TIFFNumberOfStrips(tiff);
    LXInteger stripRowBytes = stripSize / rowsPerStrip;
    
    tempBuf = _lx_malloc(stripSize);
    
    uint16_t *tempHalfBuf = (pxFormat == kLX_RGBA_FLOAT16) ? _lx_malloc(dstRowBytes * rowsPerStrip) : NULL;

    ///NSLog(@"%s: image size %i * %i -- strip size is %i; strip count %i; rows per strip %i -> stripRowBytes %i (dst rb %i)", 
    ///        __func__, w, h, stripSize, stripCount, rowsPerStrip, stripRowBytes, dstRowBytes);

    
    LXInteger y = 0;
    
    LXInteger i;
    for (i = 0; i < stripCount; i++) {
        LXInteger readBytes = TIFFReadEncodedStrip(tiff, i, tempBuf, stripSize);
        if (readBytes <= 0) {
            printf("** TIFF reading failed at strip %ld / %ld\n", (long)i, (long)stripCount);
            goto bail;
        }
        LXInteger numRowsRead = (readBytes / stripRowBytes);
        if ((numRowsRead * stripRowBytes) != readBytes) {
            printf("** TIFF read bytes value differs from expected (%ld / %ld): is %ld, rows %ld, striprb %ld\n", (long)i, (long)stripCount, (long)readBytes, (long)numRowsRead, (long)stripRowBytes);
            goto bail;
        }
        
        uint8_t *src = tempBuf;
        uint8_t *dst = dstData + dstRowBytes * y;
        
        if (pxFormat == kLX_RGBA_INT8) {
            if (spp == 4) {
                LXPxCopy_RGBA_int8(w, numRowsRead,  src, stripRowBytes,  dst, dstRowBytes);
            } else {
                LXPxConvert_RGB_to_RGBA_int8(w, numRowsRead,  src, stripRowBytes, 3,  dst, dstRowBytes);
            }
        }
        // for 16-bit formats, need to use the second temp buffer for int->float16 conversion
        else if (pxFormat == kLX_RGBA_FLOAT16) {
            if (spp == 4) {
                LXPxCopy_RGBA_int16(w, numRowsRead,  (uint16_t *)src, stripRowBytes,  tempHalfBuf, dstRowBytes);
            } else {
                LXPxConvert_RGB_to_RGBA_int16(w, numRowsRead,  (uint16_t *)src, stripRowBytes, 3,  tempHalfBuf, dstRowBytes, 65535);
            }
            
            ///NSLog(@"... strip %i: num rows read: %i", i, numRowsRead);
            LXPxConvert_int16_to_float16(tempHalfBuf, (LXHalf *)dst, numRowsRead * dstRowBytes / sizeof(LXHalf), 65535);
        }
        
        y += numRowsRead;
    }
    success = YES;
    
    if (tempHalfBuf) _lx_free(tempHalfBuf);
        
bail:
    ///NSLog(@"%s finished, ok: %i", __func__, success);
    
    if (didLock) LXPixelBufferUnlockPixels(newPixbuf);
    _lx_free(tempBuf);
    TIFFClose(tiff);
    if ( !success) {
        LXPixelBufferRelease(newPixbuf);
        newPixbuf = NULL;
    }
    return newPixbuf;
}


#pragma mark --- write API ---


static LXSuccess writeTIFFImage_RGBA_float16_to_int16(LXPixelBufferRef pixbuf,
                                                            LXUnibuffer unipath,
                                                            LXBool includeAlpha,
                                                            uint8_t *iccData, size_t iccDataLen,
                                                            LXError *outError)
{
    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);

    if (pxFormat != kLX_RGBA_FLOAT16) {
        LXErrorSet(outError, 1812, "invalid pixel format specified for native API writer (float16 expected)");  // this shouldn't happen (data should be converted before)
        return NO;
    }

    LXBool retVal = NO;
    
    size_t srcRowBytes = 0;
    LXHalf *srcData = (LXHalf *) LXPixelBufferLockPixels(pixbuf, &srcRowBytes, NULL, outError);
    if ( !srcData) return NO;
    
    size_t dstRowBytes = w * (includeAlpha ? 8 : 6);
    uint16_t *tempRowData = _lx_malloc(dstRowBytes);

    const uint16_t dstWhite = 65535;
    

    TIFF *tiff;
    if ((tiff = lxTiffOpenUnipath(unipath, "w", outError))) {
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, (uint32_t)w);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, (uint32_t)h);
    
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, (uint16_t)16);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16_t)((includeAlpha) ? 4 : 3));
        
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);

        TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

        TIFFSetField(tiff, TIFFTAG_XRESOLUTION, (float)72.0);
        TIFFSetField(tiff, TIFFTAG_YRESOLUTION, (float)72.0);
        TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
  
        if (includeAlpha) {
            uint16_t alphaInfo = EXTRASAMPLE_ASSOCALPHA;
            TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, (uint32_t)1, &alphaInfo);
        }
    
        if (iccData) {
            TIFFSetField(tiff, TIFFTAG_ICCPROFILE, (uint32_t)iccDataLen, iccData);
        }
  
        /*
        tsize_t bytesWritten = TIFFWriteEncodedStrip(tiff, 0, dstData, dstRowBytes * h);
        //NSLog(@"libtiff bytes written: %i", bytesWritten);
        */
        // write out each scanline as a strip
        LXUInteger y;
        for (y = 0; y < h; y++) {
            uint16_t *src = (uint16_t *)((uint8_t *)srcData + srcRowBytes * y);
            uint16_t *dataToWrite = tempRowData;
            
            if (includeAlpha) {
                LXPxConvert_float16_to_int16(src, dataToWrite, w * 4, dstWhite);
            } else {
                LXPxConvert_RGB_float16_to_RGB_int16(w, 1,
                                                     src, srcRowBytes, 4,
                                                     dataToWrite, dstRowBytes,
                                                     dstWhite);
            }
            
            tsize_t bytesWritten = TIFFWriteEncodedStrip(tiff, y, dataToWrite, dstRowBytes);
            
            if (bytesWritten <= 0) {
                LXErrorSet(outError, 1813, "unable to write file (disk may be full)");
                break;
            }
        }
        retVal = (y == h) ? YES : NO;
        
        TIFFClose(tiff);
    }

    _lx_free(tempRowData);

    LXPixelBufferUnlockPixels(pixbuf);    
    return retVal;
}


static LXSuccess writeTIFFImage_RGBA_int8(LXPixelBufferRef pixbuf,
                                                            LXUnibuffer unipath,
                                                            LXBool includeAlpha,
                                                            uint8_t *iccData, size_t iccDataLen,
                                                            LXError *outError)
{
    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);

    if (pxFormat != kLX_RGBA_INT8) {
        LXErrorSet(outError, 1812, "invalid pixel format specified for native API writer (int8 expected)");  // this shouldn't happen (data should be converted before)
        return NO;
    }

    LXBool retVal = NO;
    
    size_t srcRowBytes = 0;
    uint8_t *srcData = (uint8_t *) LXPixelBufferLockPixels(pixbuf, &srcRowBytes, NULL, outError);
    if ( !srcData) return NO;
    
    size_t dstRowBytes = w * (includeAlpha ? 4 : 3);
    uint8_t *tempRowData = _lx_malloc(dstRowBytes);  // * h);

    TIFF *tiff;
    if ((tiff = lxTiffOpenUnipath(unipath, "w", outError))) {
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, w);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, h);
    
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (includeAlpha) ? 4 : 3);
        
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);

        TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);    
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

        TIFFSetField(tiff, TIFFTAG_XRESOLUTION, 72.0);
        TIFFSetField(tiff, TIFFTAG_YRESOLUTION, 72.0);
        TIFFSetField(tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
  
        if (includeAlpha) {
            uint16_t alphaInfo = EXTRASAMPLE_ASSOCALPHA;
            TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, (int)1, &alphaInfo);
        }
    
        if (iccData) {
            TIFFSetField(tiff, TIFFTAG_ICCPROFILE, (int)iccDataLen, iccData);
        }
        
        // write out each scanline as a strip
        LXUInteger y;
        for (y = 0; y < h; y++) {
            uint8_t *src = ((uint8_t *)srcData + srcRowBytes * y);
            uint8_t *dataToWrite = src;
            
            if ( !includeAlpha) {
                LXPxConvert_RGBA_to_RGB_int8(w, 1,  src, srcRowBytes, 4,  tempRowData, dstRowBytes, 3);
                dataToWrite = tempRowData;
            }
            
            tsize_t bytesWritten = TIFFWriteEncodedStrip(tiff, y, dataToWrite, dstRowBytes);
            
            if (bytesWritten <= 0) {
                LXErrorSet(outError, 1813, "unable to write file (disk may be full)");
                break;
            }
        }
        retVal = (y == h) ? YES : NO;
        
        TIFFClose(tiff);
    }

    _lx_free(tempRowData);

    LXPixelBufferUnlockPixels(pixbuf);    
    return retVal;
}



LXSuccess LXPixelBufferWriteAsTIFFImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf) return NO;

    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);
    
    LXBool includeAlpha = NO;
    LXInteger preferredBitDepth = 8;
    LXInteger colorSpaceID = 0;
    if (properties) {
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowAlpha, &includeAlpha);
        LXMapGetInteger(properties, kLXPixelBufferAttachmentKey_ColorSpaceEncoding, &colorSpaceID);
        LXMapGetInteger(properties, kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel, &preferredBitDepth);
    }
    
    uint8_t *iccDataBuf = NULL;
    size_t iccDataLen = 0;
    LXCopyICCProfileDataForColorSpaceEncoding(colorSpaceID, &iccDataBuf, &iccDataLen);
    
    ///NSLog(@"%s: depth %i, colorspaceid %i -> icc data size is %i", __func__, preferredBitDepth, colorSpaceID, iccDataLen);

    LXPixelBufferRef tempPixbuf = NULL;
    LXSuccess retVal = NO;

    if (preferredBitDepth == 16) {
        if (pxFormat == kLX_RGBA_FLOAT16) {
            retVal = writeTIFFImage_RGBA_float16_to_int16(pixbuf, unipath, includeAlpha, iccDataBuf, iccDataLen, outError);
        }
        else {
            tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_FLOAT16, outError);
            
            if (tempPixbuf && LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError)) {
                retVal = writeTIFFImage_RGBA_float16_to_int16(tempPixbuf, unipath, includeAlpha, iccDataBuf, iccDataLen, outError);
            }
        }        
    }
    else {  // write 8-bit
        if (pxFormat == kLX_RGBA_INT8) {
            retVal = writeTIFFImage_RGBA_int8(pixbuf, unipath, includeAlpha, iccDataBuf, iccDataLen, outError);
        }
        else {
            tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
            
            if (tempPixbuf && LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError)) {
                retVal = writeTIFFImage_RGBA_int8(tempPixbuf, unipath, includeAlpha, iccDataBuf, iccDataLen, outError);
            }            
        }
    }
            
    LXPixelBufferRelease(tempPixbuf);
    _lx_free(iccDataBuf);
    
    return retVal;
}


