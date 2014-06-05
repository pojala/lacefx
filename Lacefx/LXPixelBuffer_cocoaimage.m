/*
 *  LXPixelBuffer_cocoaimage.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 9.9.2007.
 *  Copyright 207 Lacquer oy/ltd.
 *

 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#import "LXPixelBuffer.h"
#import "LXPixelBuffer_priv.h"
#import "LXHalfFloat.h"
#import "LXImageFunctions.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>


#define LX_HAS_LIBTIFF 1

#if defined(LX_HAS_LIBTIFF)
 #include <tiff.h>
 #include <tiffio.h>
#endif



  // Cocoa image API uses full 0-65535 range for 16-bit pixels (unlike e.g. Photoshop API which uses 0-32768)
static const float PX_16BIT_TO_FLOAT = 1.0f / 65535.0f;


#if !defined(__LP64__)

@interface NSColorSpace (LeopardAdditions)
+ (NSColorSpace *)adobeRGB1998ColorSpace;
+ (NSColorSpace *)sRGBColorSpace;
- (id)initWithCGColorSpace:(CGColorSpaceRef)cgColorSpace;
- (CGColorSpaceRef)CGColorSpace;
@end

@interface NSBitmapImageRep (LeopardAdditions)
- (CGImageRef)CGImage;
@end

#endif





static void copyNSImageIntoCGBitmapContextWithScaling(NSImage *image, CGContextRef cgCtx)
{
    if ( !cgCtx) return;

    CGContextSetInterpolationQuality(cgCtx, kCGInterpolationHigh);
    
    NSRect dstRect = NSMakeRect(0, 0, CGBitmapContextGetWidth(cgCtx), CGBitmapContextGetHeight(cgCtx));
    
    NSGraphicsContext *nsCtx = [NSGraphicsContext graphicsContextWithGraphicsPort:cgCtx flipped:NO];
    NSGraphicsContext *prevCtx = [[NSGraphicsContext currentContext] retain];
    [NSGraphicsContext setCurrentContext:nsCtx];
    
    //[[NSColor yellowColor] set];
    //[[NSBezierPath bezierPathWithRect:dstRect] fill];
    [image drawInRect:dstRect fromRect:NSMakeRect(0, 0, [image size].width, [image size].height)
                              operation:NSCompositeSourceOver fraction:1.0];
    
    [NSGraphicsContext setCurrentContext:prevCtx];
    [prevCtx release];
}


NSBitmapImageRep *LXPixelBufferCopyAsNSBitmapImageRep_(LXPixelBufferRef srcPixbuf, LXError *outError)
{
    if ( !srcPixbuf) return nil;
    
    LXUInteger lxPixelFormat = LXPixelBufferGetPixelFormat(srcPixbuf);
    LXPixelBufferRef tempPixbuf = NULL;
    LXPixelBufferRef pixbuf = srcPixbuf;
    
    if (lxPixelFormat != kLX_RGBA_INT8) {
        tempPixbuf = LXPixelBufferCreate(NULL, LXPixelBufferGetWidth(srcPixbuf), LXPixelBufferGetHeight(srcPixbuf), kLX_RGBA_INT8, outError);
        if ( !tempPixbuf)
            return nil;
        
        if ( !LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, srcPixbuf, outError))
            return nil;
        
        //lxPixelFormat = kLX_RGBA_INT8;
        pixbuf = tempPixbuf;
        /*
        char msg[512];
        snprintf(msg, 512, "unsupported pixel format for conversion (%ld)", lxPixelFormat);
        LXErrorSet(outError, 1602, msg);
        return nil;
        */
    }
    
    const LXInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXInteger h = LXPixelBufferGetHeight(pixbuf);

    size_t rowBytes = 0;
    uint8_t *data = LXPixelBufferLockPixels(pixbuf, &rowBytes, NULL, outError);    
    if ( !data) {
        return nil;  // this shouldn't happen in practice
    }
    
    // create an empty alloced imagerep.
    //
    // the specified colorspace is not correct, but it's the best we can do (CalibratedRGB is more wrong usually, because it's gamma 1.8.)
    // the caller should retag/convert the image to its actual colorspace, if known.
    // 10.6 has a useful -[NSBitmapImageRep bitmapImageRepByRetaggingWithColorSpace:] method for this.
    //
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                pixelsWide:w
                                pixelsHigh:h
                                bitsPerSample:8
                                samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                                colorSpaceName:NSDeviceRGBColorSpace  
                                bitmapFormat:0
                                bytesPerRow:rowBytes
                                bitsPerPixel:32];
                                
    if (rep && [rep bitmapData]) {
        memcpy([rep bitmapData], data, rowBytes * h);
    } else {
        LXErrorSet(outError, 1603, "unable to create NSBitmapRep");
    }
                        
    LXPixelBufferUnlockPixels(pixbuf);
    
    LXPixelBufferRelease(tempPixbuf);
    return rep;
}


LXPixelBufferRef LXPixelBufferCreateFromNSBitmapImageRep_(NSBitmapImageRep *rep, LXError *outError)
{
    if ( ![rep respondsToSelector:@selector(bitmapData)]) {
        char msg[512];
        snprintf(msg, 512, "unsupported type of NSImage rep (%p / %s)", rep, [[[rep class] description] UTF8String]);
        LXErrorSet(outError, 1601, msg);
        return NULL;
    }
    
    const LXUInteger bpp = [rep bitsPerPixel];
    const LXUInteger spp = [rep samplesPerPixel];
    const LXUInteger bps = [rep bitsPerSample];
    const LXInteger w = [rep pixelsWide];
    const LXInteger h = [rep pixelsHigh];
    const LXUInteger bitmapFormat = [rep respondsToSelector:@selector(bitmapFormat)] ? [rep bitmapFormat] : 0;
    
    //NSData *iccData = (NSData *)[rep valueForProperty:NSImageColorSyncProfileData];
    //NSLog(@"image rep %i * %i:  colorSyncProfile len is %i", w, h, [iccData length]);
    
    //if (iccData)  [iccData writeToFile:@"/temp_in.icc" options:0 error:NULL];
    
    // check for supported formats
    if ([rep isPlanar] || (spp != 1 && spp != 3 && spp != 4) || (bps != 8 && bps != 16 && bps != 32)) {
        char msg[512];
        sprintf(msg, "unsupported color depth, colorspace or plane layout (planar %i, sppx %i, bitsps %i, bmapformat %i)", (int)[rep isPlanar], (int)spp, (int)bps, (int)bitmapFormat);
        LXErrorSet(outError, 1602, msg);
        return NULL;
    }

    LXUInteger lxPixelFormat = (bps == 8) ? kLX_RGBA_INT8 : kLX_RGBA_FLOAT32;  // 16-bit int pixels are promoted to 32-bit float currently
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, lxPixelFormat, outError);
    if ( !newPixbuf)
        return NULL;

    size_t srcRowBytes = [rep bytesPerRow];
    size_t dstRowBytes;
    uint8_t * LXRESTRICT srcBuf = [rep bitmapData];
    uint8_t * LXRESTRICT dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);
    LXInteger x, y;
    const BOOL isAlphaFirst = (bitmapFormat & NSAlphaFirstBitmapFormat) ? YES : NO;
    const BOOL isPremultAlpha = (bitmapFormat & NSAlphaNonpremultipliedBitmapFormat) ? NO : YES;

    ////NSLog(@"%s: spp %i, bpp %i, bps %i, bitmapformat %i, size %i * %i, rb %i (%i)", __func__, spp, bpp, bps, bitmapFormat,  w, h,  srcRowBytes, dstRowBytes);

    if (bps == 16) {
        if (spp == 4 || bpp == 64) {
            for (y = 0; y < h; y++) {
                uint16_t * LXRESTRICT src = (uint16_t *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    dst[0] = src[0] * PX_16BIT_TO_FLOAT;
                    dst[1] = src[1] * PX_16BIT_TO_FLOAT;
                    dst[2] = src[2] * PX_16BIT_TO_FLOAT;
                    dst[3] = src[3] * PX_16BIT_TO_FLOAT;
                    dst += 4;
                    src += 4;
                }
            }
        }
        else if (spp == 3) {
            for (y = 0; y < h; y++) {
                uint16_t * LXRESTRICT src = (uint16_t *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    dst[0] = src[0] * PX_16BIT_TO_FLOAT;
                    dst[1] = src[1] * PX_16BIT_TO_FLOAT;
                    dst[2] = src[2] * PX_16BIT_TO_FLOAT;
                    dst[3] = 1.0f;
                    dst += 4;
                    src += 3;
                }
            }
        }
        else if (spp == 1) {  // TODO: should use LX_Luminance format instead of RGBA
            for (y = 0; y < h; y++) {
                uint16_t * LXRESTRICT src = (uint16_t *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    float v = src[0] * PX_16BIT_TO_FLOAT; 
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = 1.0f;
                    dst += 4;
                    src += 1;
                }
            }        
        }
    }  // end of bps == 16
    
    else if (bps == 32) {
        if (spp == 4 || bpp == 128) {
            for (y = 0; y < h; y++) {
                float * LXRESTRICT src = (float *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = src[3];
                    dst += 4;
                    src += 4;
                }
            }
        }
        else if (spp == 3) {
            for (y = 0; y < h; y++) {
                float * LXRESTRICT src = (float *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = 1.0f;
                    dst += 4;
                    src += 3;
                }
            }
        }
        else if (spp == 1) {
            for (y = 0; y < h; y++) {
                float * LXRESTRICT src = (float *)(srcBuf + srcRowBytes * y);
                float * LXRESTRICT dst = (float *)(dstBuf + dstRowBytes * y);
                for (x = 0; x < w; x++) {
                    float v = src[0];
                    dst[0] = v;
                    dst[1] = v;
                    dst[2] = v;
                    dst[3] = 1.0f;
                    dst += 4;
                    src += 1;
                }
            }
        }
    }  // end of bps == 32
    
    else if (bps == 8) {
        if (spp == 4 || bpp == 32) {
            for (y = 0; y < h; y++) {
                uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
                uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
                
                // Cocoa reports an spp value of 3 for e.g. TGA images which are originally 24-bit but were decoded as 32-bit
                if (isPremultAlpha && !isAlphaFirst) {
                    memcpy(dst, src, MIN(srcRowBytes, dstRowBytes));
                } else {
                    if (isPremultAlpha && isAlphaFirst) {
                        for (x = 0; x < w; x++) {
                            dst[0] = src[1];
                            dst[1] = src[2];
                            dst[2] = src[3];
                            dst[3] = src[0];
                            dst += 4;
                            src += 4;
                        }
                    } else if ( !isPremultAlpha && !isAlphaFirst) {
                        for (x = 0; x < w; x++) {
                            LXUInteger a = src[3];
                            dst[0] = ((LXUInteger)src[0] * a) >> 8;
                            dst[1] = ((LXUInteger)src[1] * a) >> 8;
                            dst[2] = ((LXUInteger)src[2] * a) >> 8;
                            dst[3] = a;
                            dst += 4;
                            src += 4;
                        }
                    } else if ( !isPremultAlpha && isAlphaFirst) {
                        for (x = 0; x < w; x++) {
                            LXUInteger a = src[0];
                            dst[0] = ((LXUInteger)src[1] * a) >> 8;
                            dst[1] = ((LXUInteger)src[2] * a) >> 8;
                            dst[2] = ((LXUInteger)src[3] * a) >> 8;
                            dst[3] = a;
                            dst += 4;
                            src += 4;
                        }
                    }
                }
            }            
        }
        else if (spp == 3) {
            for (y = 0; y < h; y++) {
                uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
                uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
                for (x = 0; x < w; x++) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = 255;
                    dst += 4;
                    src += 3;
                }
            }        
        }
        else if (spp == 1) {  // TODO: should use LX_Luminance format instead of RGBA
            for (y = 0; y < h; y++) {
                uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
                uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
                for (x = 0; x < w; x++) {
                    dst[0] = src[0];
                    dst[1] = src[0];
                    dst[2] = src[0];
                    dst[3] = 255;
                    dst += 4;
                    src += 1;
                }
            }        
        }
    }  // end of bps == 8
    
    LXPixelBufferUnlockPixels(newPixbuf);
    return newPixbuf;
}

NSBitmapImageRep *LXPixelBufferCopyAsNSBitmapImageRep(LXPixelBufferRef srcPixbuf, LXError *outError)
{
    return LXPixelBufferCopyAsNSBitmapImageRep_(srcPixbuf, outError);
}

LXPixelBufferRef LXPixelBufferCreateFromNSBitmapImageRep(NSBitmapImageRep *rep, LXError *outError)
{
    return LXPixelBufferCreateFromNSBitmapImageRep_(rep, outError);
}


static CGColorSpaceRef createOutputCGColorSpaceFromLXProps(LXMapPtr properties)
{
    CGColorSpaceRef outputColorSpace = NULL;
    if (properties) {
        char *csName = NULL;
        if (LXMapGetUTF8(properties, "outputColorSpaceName", &csName) && csName) {
            NSString *cgCSName = nil;
            if (0 == strcmp(csName, "sRGB")) {
                cgCSName = @"kCGColorSpaceSRGB";
            } else if (0 == strcmp(csName, "Adobe RGB 1998") || 0 == strcmp(csName, "AdobeRGB1998")) {
                cgCSName = @"kCGColorSpaceAdobeRGB1998";
            } else if (0 == strcmp(csName, "Linear RGB") || 0 == strcmp(csName, "LinearRGB")) {
                cgCSName = @"kCGColorSpaceGenericRGBLinear";
            }
            
            _lx_free(csName);
            
            if (cgCSName) {
                ///NSLog(@"...%s: will convert to output colorspace '%@'", __func__, cgCSName);
                outputColorSpace = CGColorSpaceCreateWithName((CFStringRef)cgCSName);
            }
        }
    }
    return outputColorSpace;
}

static LXPixelBufferRef createLXPixelBufferFromNSImage(NSImage *image, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    NSBitmapImageRep *rep = nil;
    if ( !image || [[image representations] count] < 1) {
        LXErrorSet(outError, 1600, "couldn't load image");
        return NULL;
    }
    rep = [[image representations] objectAtIndex:0];
    
    ///NSLog(@"%s: props %p, rep %@", __func__, properties, rep);

    CGColorSpaceRef outputColorSpace = createOutputCGColorSpaceFromLXProps(properties);
    
    BOOL hasBitmapRep = [rep isKindOfClass:[NSBitmapImageRep class]];
    if ( !hasBitmapRep && !outputColorSpace) {
        outputColorSpace = CGColorSpaceCreateDeviceRGB();
    }
    
    if ( !outputColorSpace) {
        return LXPixelBufferCreateFromNSBitmapImageRep_(rep, outError);    
    }
    
    // to convert the colorspace, we'll draw into a CGBitmapContext with the specified colorspace
    LXPixelBufferRef pixbuf = NULL;
    LXInteger w = [rep pixelsWide];
    LXInteger h = [rep pixelsHigh];
    BOOL isFloat = NO;  // doesn't work correctly at least on OS X 10.5.8
    LXUInteger ctxPxf = (isFloat) ? kLX_RGBA_FLOAT32 : kLX_RGBA_INT8;
    LXUInteger dstPxf = (isFloat) ? kLX_RGBA_FLOAT16 : kLX_RGBA_INT8;
    LXInteger bpc = (isFloat) ? 32 : 8;
    size_t rowBytes = w * 4 * (bpc / 8);
    uint8_t *data = _lx_calloc(rowBytes * h, 1);
    LXUInteger bitmapInfo = kCGImageAlphaPremultipliedLast;
    if (isFloat) bitmapInfo |= kCGBitmapFloatComponents;
    
    CGContextRef cgCtx = CGBitmapContextCreate(data, w, h, bpc, rowBytes, outputColorSpace, bitmapInfo);
    if ( !cgCtx) {
        NSLog(@"** %s: unable to create cg ctx (%d * %d, isfloat %i)", __func__, (int)w, (int)h, isFloat);
        LXErrorSet(outError, 1644, "native API could not create context for image conversion");
    } else {
        copyNSImageIntoCGBitmapContextWithScaling(image, cgCtx);
        
        pixbuf = LXPixelBufferCreate(NULL, w, h, dstPxf, outError);
        if (pixbuf) {
            if ( !LXPixelBufferWriteDataWithPixelFormatConversion(pixbuf,
                                                                  data, w, h, rowBytes, ctxPxf,
                                                                  NULL,
                                                                  outError)) {
                NSLog(@"** %s: unable to write data with lx pixel conversion (%i * %i)", __func__, (int)w, (int)h);
            }
        }
    }
    if (cgCtx) CGContextRelease(cgCtx);
    _lx_free(data);
    
    CGColorSpaceRelease(outputColorSpace);
    
    return pixbuf;
}

LXPixelBufferRef LXPixelBufferCreateFromPathUsingNativeAPI_(LXUnibuffer unipath, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    NSString *path = [NSString stringWithCharacters:unipath.unistr length:unipath.numOfChar16];
    if ( !path || [path length] < 1) {
        LXErrorSet(outError, 1001, "path is empty");
        return NULL;
    }
    
    CGColorSpaceRef outputColorSpace = createOutputCGColorSpaceFromLXProps(properties);
    
    if (outputColorSpace) {
        ///NSLog(@"%s: will use Core Image for output color space conversion", __func__);
        CIImage *ciImage = [CIImage imageWithContentsOfURL:[NSURL fileURLWithPath:path]];
        if ( !ciImage) {
            LXErrorSet(outError, 1600, "couldn't load image");
            CGColorSpaceRelease(outputColorSpace);
            return NULL;
        }
        
        LXPixelBufferRef pixbuf = NULL;
        LXInteger w = [ciImage extent].size.width;
        LXInteger h = [ciImage extent].size.height;
        BOOL isFloat = NO;  // doesn't work correctly at least on OS X 10.5.8
        LXUInteger ctxPxf = (isFloat) ? kLX_RGBA_FLOAT32 : kLX_RGBA_INT8;
        LXUInteger dstPxf = (isFloat) ? kLX_RGBA_FLOAT32 : kLX_RGBA_INT8;
        LXInteger bpc = (isFloat) ? 32 : 8;
        size_t rowBytes = w * 4 * (bpc / 8);
        uint8_t *data = _lx_calloc(rowBytes * h, 1);
        LXUInteger bitmapInfo = kCGImageAlphaPremultipliedLast;
        if (isFloat) bitmapInfo |= kCGBitmapFloatComponents;
            
        CGContextRef cgCtx = CGBitmapContextCreate(data, w, h, bpc, rowBytes, outputColorSpace, bitmapInfo);
        if ( !cgCtx) {
            NSLog(@"** %s: unable to create cg ctx (%i * %i, float %i)", __func__, (int)w, (int)h, isFloat);
            LXErrorSet(outError, 1644, "native API could not create context for image conversion");
        } else {
            CIContext *ciContext = [CIContext contextWithCGContext:cgCtx options:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                                        (id)outputColorSpace, kCIContextOutputColorSpace,
                                                                                        nil]];
            
            [ciContext drawImage:ciImage inRect:CGRectMake(0, 0, w, h) fromRect:[ciImage extent]];
            
            CGContextFlush(cgCtx);
            CGContextRelease(cgCtx);
            
            //NSLog(@"... did draw CIImage into CGContext: %i * %i, isFloat %i", (int)w, (int)h, isFloat);
            
            pixbuf = LXPixelBufferCreate(NULL, w, h, dstPxf, outError);
            if (pixbuf) {
                if ( !LXPixelBufferWriteDataWithPixelFormatConversion(pixbuf,
                                                                data, w, h, rowBytes, ctxPxf,
                                                                NULL,
                                                                outError)) {
                    NSLog(@"** %s: unable to write data with lx pixel conversion (%i * %i)", __func__, (int)w, (int)h);
                }
            }
        }
        _lx_free(data);
        
        CGColorSpaceRelease(outputColorSpace);
        
        return pixbuf;
    }
    
    
    NSImage *image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
    return createLXPixelBufferFromNSImage(image, imageType, properties, outError);
}


LXPixelBufferRef LXPixelBufferCreateFromFileDataUsingNativeAPI_(const uint8_t *bytes, size_t len, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    NSData *data = [NSData dataWithBytesNoCopy:(uint8_t *)bytes length:len freeWhenDone:NO];
    NSImage *image = [[[NSImage alloc] initWithData:data] autorelease];
    
    return createLXPixelBufferFromNSImage(image, imageType, properties, outError);
}


#pragma mark --- writing ---

static LXSuccess writeImageUsingNSBitmapImageRep_RGBA_float16_to_int16(LXPixelBufferRef pixbuf,
                                                            NSString *path, LXUInteger nsBitmapType,
                                                            LXBool includeAlpha,
                                                            NSDictionary *nsBitmapImageRepProps,
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

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                    pixelsWide:w pixelsHigh:h
                                    bitsPerSample:16 
                                    samplesPerPixel:(includeAlpha) ? 4 : 3
                                    hasAlpha:(includeAlpha) ? YES : NO
                                    isPlanar:NO
                                    colorSpaceName:NSDeviceRGBColorSpace
                                    bitmapFormat:0
                                    bytesPerRow:dstRowBytes
                                    bitsPerPixel:(includeAlpha) ? 64 : 48
                                ];
                                
    NSCAssert(rep, @"unable to create NSBitmapImageRep (this shouldn't happen on Mac OS X)");

    uint16_t *dstData = (uint16_t *)[rep bitmapData];

    const uint16_t dstWhite = 65535;  // Cocoa image API uses 0-65535 range for 16-bit pixels (unlike e.g. Photoshop API which uses 0-32768)
    
    if ( !includeAlpha) {
        LXPxConvert_RGB_float16_to_RGB_int16(w, h, srcData, srcRowBytes, 4,
                                             dstData, dstRowBytes,
                                             dstWhite);
    } else {
        LXUInteger y;
        for (y = 0; y < h; y++) {
            LXHalf *src = (LXHalf *)((uint8_t *)srcData + srcRowBytes * y);
            uint16_t *dst = (uint16_t *)((uint8_t *)dstData + dstRowBytes * y);
            LXPxConvert_float16_to_int16(src, dst, w * 4, dstWhite);
        }
    }

#if defined(LX_HAS_LIBTIFF)
    TIFF *tiff;

    if( !(tiff = TIFFOpen([path UTF8String], "w"))) {
        LXErrorSet(outError, 1808, "unable to open file for writing through libTIFF");
    }

    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, h);
    
    
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (includeAlpha) ? 4 : 3);
    TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, h);

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
    
    NSData *iccData = [nsBitmapImageRepProps objectForKey:NSImageColorSyncProfileData];
    if (iccData) {
        TIFFSetField(tiff, TIFFTAG_ICCPROFILE, [iccData length], [iccData bytes]);
    }
  
    tsize_t bytesWritten = TIFFWriteEncodedStrip(tiff, 0, [rep bitmapData], dstRowBytes * h);
    //NSLog(@"libtiff bytes written: %i", bytesWritten);

    TIFFClose(tiff);

    retVal = (bytesWritten > 0) ? YES : NO;

#else    
    if (rep) {
        NSData *data;
        // 2009.07.31 -- this color space conversion doesn't seem to work currently in Mac OS X 10.5
        // using neither the Cocoa API nor CGImageDestination. the only tiffs produced are in the display color space. darn.
        /*
        if ([rep respondsToSelector:@selector(CGImageRef)]) {
            CGImageRef cgImage = [rep CGImage];
            NSLog(@"... writing as cgimage: bits per pixel is %i; cg color space is %p", CGImageGetBitsPerPixel(cgImage), cgColorSpace);
            
            CGImageRef tempCGImage = NULL;
            if (cgColorSpace) {
                tempCGImage = CGImageCreateCopyWithColorSpace(cgImage, cgColorSpace);
            }
        
            data = [NSMutableData data];
            CGImageDestinationRef imgDst = CGImageDestinationCreateWithData((CFMutableDataRef)data, kUTTypeTIFF, 1, NULL);  // <<-- hardcoded as TIFF currently!
            
            CGImageDestinationAddImage(imgDst, (tempCGImage) ? tempCGImage : cgImage, NULL);
            CGImageDestinationFinalize(imgDst);
            CFRelease(imgDst);
            
            if (tempCGImage) CGImageRelease(tempCGImage);
        } else {
        */
        {
            data = [rep representationUsingType:nsBitmapType properties:nil];
        }
                
        if ( !data) {
            LXErrorSet(outError, 1806, "native API did not produce a suitable representation");
        } else {
            NSError *nsError = nil;
            retVal = [data writeToFile:path options:0 error:&nsError];
            if ( !retVal) {
                char msg[512];
                snprintf(msg, 512, "file write failed at native API: %s", [[nsError localizedDescription] UTF8String]);
                LXErrorSet(outError, 1807, msg);
            }
        }
    }
#endif

    [rep release];

    LXPixelBufferUnlockPixels(pixbuf);
    return retVal;
}


static LXSuccess writeImageUsingNSBitmapImageRep_RGBA_int8(LXPixelBufferRef pixbuf,
                                                           NSString *path, LXUInteger nsBitmapType,
                                                           LXBool includeRGB, LXBool includeAlpha,
                                                           NSDictionary *nsBitmapImageRepProps,
                                                           LXError *outError)
{
    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);

    if (pxFormat != kLX_RGBA_INT8 && pxFormat != kLX_ARGB_INT8) {
        LXErrorSet(outError, 1812, "invalid pixel format specified for native API writer (int8 expected)");  // this shouldn't happen (data should be converted before)
        return NO;
    }

    LXBool retVal = NO;
    uint8_t *tempBuf = NULL;
    NSBitmapImageRep *rep = nil;
    
    size_t rowBytes = 0;
    uint8_t *pbData = LXPixelBufferLockPixels(pixbuf, &rowBytes, NULL, outError);
    if ( !pbData) return NO;
    
    
    if (includeRGB == includeAlpha) {
        rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&pbData
                                    pixelsWide:w pixelsHigh:h
                                    bitsPerSample:8 samplesPerPixel:4
                                    hasAlpha:YES isPlanar:NO
                                    colorSpaceName:NSDeviceRGBColorSpace
                                    bitmapFormat:(pxFormat == kLX_ARGB_INT8) ? NSAlphaFirstBitmapFormat : 0
                                    bytesPerRow:rowBytes
                                    bitsPerPixel:32];                                        
    }
    else if (includeRGB) {
        size_t tempRowBytes = LXAlignedRowBytes(w * 3);
        tempBuf = _lx_malloc(tempRowBytes * h);
        
        LXPxConvert_RGBA_to_RGB_int8(w, h, pbData, rowBytes, 4,  tempBuf, tempRowBytes, 3);

        rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&tempBuf
                                    pixelsWide:w pixelsHigh:h
                                    bitsPerSample:8 samplesPerPixel:3
                                    hasAlpha:NO isPlanar:NO
                                    colorSpaceName:NSDeviceRGBColorSpace
                                    bitmapFormat:0
                                    bytesPerRow:tempRowBytes
                                    bitsPerPixel:24];
    }
    else {  // copy the alpha channel only
        size_t tempRowBytes = LXAlignedRowBytes(w);
        tempBuf = _lx_malloc(tempRowBytes * h);
        
        LXInteger x, y;
        for (y = 0; y < h; y++) {
            uint8_t *src = pbData + rowBytes * y;
            uint8_t *dst = tempBuf + tempRowBytes * y;
            
            if (pxFormat == kLX_RGBA_INT8) {
                for (x = 0; x < w; x++) {
                    *dst = src[3];
                    dst++;  src += 4;
                }
            } else {
                for (x = 0; x < w; x++) {
                    *dst = src[0];
                    dst++;  src += 4;                    
                }
            }
        }
        
        rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&tempBuf
                                    pixelsWide:w pixelsHigh:h
                                    bitsPerSample:8 samplesPerPixel:1
                                    hasAlpha:NO isPlanar:NO
                                    colorSpaceName:NSDeviceWhiteColorSpace
                                    bitmapFormat:0
                                    bytesPerRow:tempRowBytes
                                    bitsPerPixel:8];
    }
    
    if (rep) {
        NSData *data = [rep representationUsingType:nsBitmapType properties:nsBitmapImageRepProps];
        if ( !data) {
            LXErrorSet(outError, 1806, "native API did not produce a suitable representation");
        } else {
            NSError *nsError = nil;
            retVal = [data writeToFile:path options:0 error:&nsError];
            if ( !retVal) {
                char msg[512];
                snprintf(msg, 512, "file write failed at native API: %s", [[nsError localizedDescription] UTF8String]);
                LXErrorSet(outError, 1807, msg);
            }
        }
        [rep release];    
    }
    
    LXPixelBufferUnlockPixels(pixbuf);
    
    if (tempBuf) _lx_free(tempBuf);
    return retVal;
}

#define LACEFX_NSBUNDLE   [NSBundle bundleWithIdentifier:@"fi.lacquer.Lacefx"]


NSData *LXDataForICCProfileWideGamutRGB_()
{
    static NSData *s_data = nil;
    if ( !s_data) {
        s_data = [[NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"WideGamutRGB" ofType:@"icc"]] retain];
    }
    return s_data;
}

NSData *LXDataForICCProfileLinearRGB_709_()
{
    static NSData *s_data = nil;
    if ( !s_data) {
        s_data = [[NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"LinearRGB_709" ofType:@"icc"]] retain];
    }
    return s_data;
}

NSData *LXDataForICCProfileProPhotoRGB_()
{
    static NSData *s_data = nil;
    if ( !s_data) {
        s_data = [[NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"ProPhotoRGB" ofType:@"icc"]] retain];
    }
    return s_data;
}

LXSuccess LXCopyICCProfileDataForColorSpaceEncoding(LXColorSpaceEncoding colorSpaceID, uint8_t **outData, size_t *outDataLen)
{
    // these NSColorSpace class methods are only available on 10.5+, so must check for their availability
    CGColorSpaceRef cgColorSpace = NULL;
    ///NSString *cgSpaceName = nil;
    NSData *iccData = nil;
    if (colorSpaceID == kLX_AdobeRGB_1998) {
        if ([NSColorSpace respondsToSelector:@selector(adobeRGB1998ColorSpace)]) {
            //cgSpaceName = @"kCGColorSpaceAdobeRGB1998";
            iccData = [[NSColorSpace adobeRGB1998ColorSpace] ICCProfileData];
        } else {
            iccData = [NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"AdobeRGB1998" ofType:@"icc"]];
        }
    }
    else if (colorSpaceID == kLX_sRGB) {
        if ([NSColorSpace respondsToSelector:@selector(sRGBColorSpace)]) {
            //cgSpaceName = @"kCGColorSpaceSRGB";
            iccData = [[NSColorSpace sRGBColorSpace] ICCProfileData];
        } else {
            iccData = [NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"sRGB" ofType:@"icc"]];
        }
    }
    else if (colorSpaceID == kLX_CanonicalLinearRGB) {
        // instead of using Apple's Generic RGB space, load our own ICC profile which matches the sRGB / HDTV primaries
        iccData = LXDataForICCProfileLinearRGB_709_();
        
        /*if ([NSColorSpace respondsToSelector:@selector(sRGBColorSpace)]) {
            cgSpaceName = @"kCGColorSpaceGenericRGBLinear";
            cgColorSpace = CGColorSpaceCreateWithName((CFStringRef)cgSpaceName);
            
            id nsColorSpace = [[[NSColorSpace alloc] initWithCGColorSpace:cgColorSpace] autorelease];
            iccData = [nsColorSpace ICCProfileData];
            //if ( !iccData)
            //    NSLog(@"*** %s: failed to get ICC data for '%@' (%p, %@)", __func__, cgSpaceName, cgColorSpace, nsColorSpace);
        } else {
            iccData = [NSData dataWithContentsOfFile:[LACEFX_NSBUNDLE pathForResource:@"LinearRGB" ofType:@"icc"]];
        }*/
    }
    else if (colorSpaceID == kLX_ProPhotoRGB) {
        iccData = LXDataForICCProfileProPhotoRGB_();
    }
    
    if (cgColorSpace) CGColorSpaceRelease(cgColorSpace);
    
    if ( !iccData) {
        return NO;
    } else {
        if (outData && outDataLen) {
            *outDataLen = [iccData length];
            *outData = _lx_malloc(*outDataLen);
            memcpy(*outData, [iccData bytes], *outDataLen);
        }
        return YES;
    }
}


LXSuccess LXPixelBufferWriteToPathUsingNativeAPI_(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXUInteger lxImageType, LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf) return NO;
    
    NSString *path = [NSString stringWithCharacters:unipath.unistr length:unipath.numOfChar16];
    if ( !path || [path length] < 1) {
        LXErrorSet(outError, 1001, "path is empty");
        return NO;
    }
    
    LXUInteger nsBitmapType = NSNotFound;
    switch (lxImageType) {
        case kLXImage_PNG:          nsBitmapType = NSPNGFileType;  break;
        case kLXImage_TIFF:         nsBitmapType = NSTIFFFileType;  break;
        case kLXImage_BMP:          nsBitmapType = NSBMPFileType;  break;
        case kLXImage_JPEG:         nsBitmapType = NSJPEGFileType;  break;
        case kLXImage_JPEG2000:     nsBitmapType = NSJPEG2000FileType;  break;
    }
    if (nsBitmapType == NSNotFound) {
        char msg[512];
        sprintf(msg, "format not supported for native writing (format id: %i)", (int)lxImageType);
        LXErrorSet(outError, 1805, msg);
        return NO;
    }

    LXSuccess retVal = NO;
    LXBool includeAlpha = YES;
    LXBool includeRGB = YES;
    LXInteger preferredBitDepth = 8;
    LXInteger colorSpaceID = 0;
    double compressionFactor = 0.0;
    if (properties) {
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowAlpha, &includeAlpha);
        LXMapGetBool(properties, "includeRGB", &includeRGB);
        
        LXMapGetInteger(properties, kLXPixelBufferAttachmentKey_ColorSpaceEncoding, &colorSpaceID);
        LXMapGetDouble(properties, "compressionFactor", &compressionFactor);
        
        if (lxImageType == kLXImage_TIFF) {  // only format that supports >8 bpc
            LXMapGetInteger(properties, kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel, &preferredBitDepth);
        }
    }
    
    NSMutableDictionary *nsBitmapProps = [NSMutableDictionary dictionary];
    [nsBitmapProps setObject:[NSNumber numberWithDouble:compressionFactor] forKey:NSImageCompressionFactor];
    
    /*
    // these NSColorSpace class methods are only available on 10.5+, so must check for their availability
    CGColorSpaceRef cgColorSpace = NULL;
    NSString *cgSpaceName = nil;
    NSData *iccData = nil;
    if (colorSpaceID == kLX_AdobeRGB_1998 && [NSColorSpace respondsToSelector:@selector(adobeRGB1998ColorSpace)]) {
        cgSpaceName = @"kCGColorSpaceAdobeRGB1998";
        iccData = [[NSColorSpace adobeRGB1998ColorSpace] ICCProfileData];
    }
    else if (colorSpaceID == kLX_sRGB && [NSColorSpace respondsToSelector:@selector(sRGBColorSpace)]) {
        cgSpaceName = @"kCGColorSpaceSRGB";
        iccData = [[NSColorSpace sRGBColorSpace] ICCProfileData];
    }
    else if (colorSpaceID == kLX_CanonicalLinearRGB && [NSColorSpace respondsToSelector:@selector(sRGBColorSpace)]) {
        cgSpaceName = @"kCGColorSpaceGenericRGBLinear";
        cgColorSpace = CGColorSpaceCreateWithName((CFStringRef)cgSpaceName);
        id nsColorSpace = [[[NSColorSpace alloc] initWithCGColorSpace:cgColorSpace] autorelease];
        iccData = [nsColorSpace ICCProfileData];
        //if ( !iccData)
        //    NSLog(@"*** %s: failed to get ICC data for '%@' (%p, %@)", __func__, cgSpaceName, cgColorSpace, nsColorSpace);
    }
    else if (colorSpaceID == kLX_WideGamutRGB) {
        iccData = LXDataForICCProfileWideGamutRGB_();
    }
    NSLog(@"%s: cspace id is %i -> icc data len is %i -- cgcolorspace is %p (cspace name: '%@')", __func__, colorSpaceID, [iccData length], cgColorSpace, cgSpaceName);
    */
    uint8_t *iccDataBuf = NULL;
    size_t iccDataLen;
    if (LXCopyICCProfileDataForColorSpaceEncoding(colorSpaceID, &iccDataBuf, &iccDataLen)) {
        [nsBitmapProps setObject:[NSData dataWithBytes:iccDataBuf length:iccDataLen] forKey:NSImageColorSyncProfileData];
        _lx_free(iccDataBuf);
    }
    
    
    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);
    
    // Cocoa image writer supports RGBA and ARGB 8-bit + 16-bit; anything else needs to be converted.
    
    if (preferredBitDepth <= 8) {
        if (pxFormat == kLX_RGBA_INT8 || pxFormat == kLX_ARGB_INT8) {
            retVal = writeImageUsingNSBitmapImageRep_RGBA_int8(pixbuf, path, nsBitmapType, includeRGB, includeAlpha, nsBitmapProps, outError);
        }
        else {
            LXPixelBufferRef tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
            
            if (tempPixbuf && LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError)) {
                retVal = writeImageUsingNSBitmapImageRep_RGBA_int8(tempPixbuf, path, nsBitmapType, includeRGB, includeAlpha, nsBitmapProps, outError);
            }
            
            LXPixelBufferRelease(tempPixbuf);
        }
    }
    else {
        ///NSLog(@"%s: writing 16-bit using Cocoa; pxformat is %ld; has alpha %i", __func__, pxFormat, includeAlpha);
        if (pxFormat == kLX_RGBA_FLOAT16) {
            retVal = writeImageUsingNSBitmapImageRep_RGBA_float16_to_int16(pixbuf, path, nsBitmapType, includeAlpha, nsBitmapProps, outError);
        }
        else {
            LXPixelBufferRef tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_FLOAT16, outError);
            
            if (tempPixbuf && LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError)) {
                retVal = writeImageUsingNSBitmapImageRep_RGBA_float16_to_int16(tempPixbuf, path, nsBitmapType, includeAlpha, nsBitmapProps, outError);
            }
            
            LXPixelBufferRelease(tempPixbuf);
        }
    }
    
    //if (cgColorSpace) CGColorSpaceRelease(cgColorSpace);
    
    return retVal;
}

