/*
 *  LXPixelBuffer_jpeg.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 23.2.2010.
 *  Copyright 2010 Lacquer oy/ltd. 
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

#include <jpeglib.h>


#ifdef __BIG_ENDIAN__
 #define MAKEPX_2VUY(y1_, y2_, cb_, cr_)    (cb_ << 24) | (y1_ << 16) | (cr_ << 8) | (y2_)
 #define Y1_FROM_2VUY(v_)                   ((v_ >> 16) & 0xff)
 #define Y2_FROM_2VUY(v_)                   ((v_) & 0xff)
 #define CB_FROM_2VUY(v_)                   ((v_ >> 24) & 0xff)
 #define CR_FROM_2VUY(v_)                   ((v_ >> 8) & 0xff)
#else
 #define MAKEPX_2VUY(y1_, y2_, cb_, cr_)    (cb_) | (y1_ << 8) | (cr_ << 16) | (y2_ << 24)
 #define Y1_FROM_2VUY(v_)                   ((v_ >> 8) & 0xff)
 #define Y2_FROM_2VUY(v_)                   ((v_ >> 24) & 0xff)
 #define CB_FROM_2VUY(v_)                   ((v_) & 0xff)
 #define CR_FROM_2VUY(v_)                   ((v_ >> 16) & 0xff)
#endif


void LXPxConvert_YCbCr420_planar_to_422_convertLevelsFrom255(
                                                const LXUInteger w, const LXUInteger h,
                                                uint8_t * LXRESTRICT srcBuf_y, const size_t srcRowBytes_y,
                                                uint8_t * LXRESTRICT srcBuf_Cb,
                                                uint8_t * LXRESTRICT srcBuf_Cr,
                                                const size_t srcRowBytes_chroma,
                                                uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                                const LXBool convertLevelsFrom255)
{
    const LXUInteger halfW = w/2;

    // for 255->235 levels conversion (JFIF stores full-range YCbCr data)
    ///const float lumaBase = 16;
    /*const float lumaScale = (219.0 / 255.0);
    const float chromaBase = 16;
    const float chromaScale = (224.0 / 255.0);*/
    // TODO: convertLevelsFrom255 is not implemented yet
    
    
    LXUInteger y;
    for (y = 0; y < h; y++) {
        const LXUInteger chromaY = y/2;

        LXUInteger x;
        uint32_t *dst = (uint32_t *)(dstBuf + dstRowBytes * y);        
        uint8_t *src_y = srcBuf_y + srcRowBytes_y * y;
        uint8_t *src_Cb = srcBuf_Cb + srcRowBytes_chroma * chromaY;
        uint8_t *src_Cr = srcBuf_Cr + srcRowBytes_chroma * chromaY;
        
        for (x = 0; x < halfW; x++) {
            uint32_t y0 = src_y[0];
            uint32_t y1 = src_y[1];
            src_y += 2;
            uint32_t cb = *src_Cb++;
            uint32_t cr = *src_Cr++;
            
            *dst = MAKEPX_2VUY(y0, y1, cb, cr);
            dst++;
        }
    }
}

void LXPxConvert_YCbCr422_to_420_planar_padY_convertLevelsTo255(
                                                const LXUInteger w, 
                                                uint8_t * LXRESTRICT srcBuf, const LXUInteger srcH, const size_t srcRowBytes,
                                                uint8_t * LXRESTRICT dstBuf_y, const size_t dstRowBytes_y,
                                                uint8_t * LXRESTRICT dstBuf_Cb,
                                                uint8_t * LXRESTRICT dstBuf_Cr,
                                                const size_t dstRowBytes_chroma,
                                                const LXUInteger dstH,
                                                const LXBool convertLevelsTo255)
{
    const LXUInteger halfW = w/2;
    
    uint8_t *lastYRow = NULL;
    uint8_t *lastCbRow = NULL;
    uint8_t *lastCrRow = NULL;
    
    // for 235->255 levels conversion (JFIF stores full-range YCbCr data)
    const float lumaBase = 16;
    const float lumaScale = 1.0 / (219.0 / 255.0);
    const float chromaBase = 16;
    const float chromaScale = 1.0 / (224.0 / 255.0);
    
    
    LXUInteger y;
    for (y = 0; y < srcH; y++) {
        const LXUInteger chromaY = y/2;
        LXUInteger x;
        uint32_t *src = (uint32_t *)(srcBuf + srcRowBytes * y);
        uint8_t *dst_y = dstBuf_y + dstRowBytes_y * y;
        lastYRow = dst_y;
        
        if (y == chromaY*2) {
            uint8_t *dst_Cb = dstBuf_Cb + dstRowBytes_chroma * chromaY;
            uint8_t *dst_Cr = dstBuf_Cr + dstRowBytes_chroma * chromaY;
            lastCbRow = dst_Cb;
            lastCrRow = dst_Cr;
            
            if (convertLevelsTo255) {
                for (x = 0; x < halfW; x++) {
                    uint32_t v = *src++;
                    float y0 = ((float)Y1_FROM_2VUY(v) - lumaBase) * lumaScale;
                    float y1 = ((float)Y2_FROM_2VUY(v) - lumaBase) * lumaScale;
                    float cb = ((float)CB_FROM_2VUY(v) - chromaBase) * chromaScale;
                    float cr = ((float)CR_FROM_2VUY(v) - chromaBase) * chromaScale;
                    y0 = MIN(255.0f, MAX(0.0f, y0));
                    y1 = MIN(255.0f, MAX(0.0f, y1));
                    cb = MIN(255.0f, MAX(0.0f, cb));
                    cr = MIN(255.0f, MAX(0.0f, cr));
                    dst_y[0] = (uint8_t)y0;
                    dst_y[1] = (uint8_t)y1;
                    dst_y += 2;
                    *dst_Cb++ = (uint8_t)cb;
                    *dst_Cr++ = (uint8_t)cr;
                }            
            } else {
                for (x = 0; x < halfW; x++) {
                    uint32_t v = *src++;
                    dst_y[0] = Y1_FROM_2VUY(v);
                    dst_y[1] = Y2_FROM_2VUY(v);
                    dst_y += 2;
                    *dst_Cb++ = CB_FROM_2VUY(v);
                    *dst_Cr++ = CR_FROM_2VUY(v);
                }
            }
        }
        else { // this is a 4:2:0 row without chroma
            if (convertLevelsTo255) {
                for (x = 0; x < halfW; x++) {
                    uint32_t v = *src++;
                    float y0 = ((float)Y1_FROM_2VUY(v) - lumaBase) * lumaScale;
                    float y1 = ((float)Y2_FROM_2VUY(v) - lumaBase) * lumaScale;
                    y0 = MIN(255.0f, MAX(0.0f, y0));
                    y1 = MIN(255.0f, MAX(0.0f, y1));
                    dst_y[0] = (uint8_t)y0;
                    dst_y[1] = (uint8_t)y1;
                    dst_y += 2;
                }            
            } else {
                for (x = 0; x < halfW; x++) {
                    uint32_t v = *src++;
                    dst_y[0] = Y1_FROM_2VUY(v);
                    dst_y[1] = Y2_FROM_2VUY(v);
                    dst_y += 2;
                }
            }
        }
    }
    
    // do padding with last row's data
    if (dstH > srcH && lastYRow && lastCbRow && lastCrRow) {
        for (y = srcH; y < dstH; y++) {
            const LXUInteger chromaY = y/2;
            uint8_t *dst_y = dstBuf_y + dstRowBytes_y * y;
            
            memcpy(dst_y, lastYRow, dstRowBytes_y);
            
            if (y == chromaY*2) {
                uint8_t *dst_Cb = dstBuf_Cb + dstRowBytes_chroma * chromaY;
                uint8_t *dst_Cr = dstBuf_Cr + dstRowBytes_chroma * chromaY;
                
                memcpy(dst_Cb, lastCbRow, dstRowBytes_chroma);
                memcpy(dst_Cr, lastCrRow, dstRowBytes_chroma);
            }
        }
    }
}


static LXSuccess writeJPEGImage_YCbCr422_int8(LXPixelBufferRef pixbuf,
                                                            LXUnibuffer unipath,
                                                            uint8_t **optionalBuffer, unsigned long *optionalBufferSize,  // size written is returned here
                                                            LXFloat jpegQualityF,
                                                            LXBool convertLevelsTo255,
                                                            LXError *outError)
{
    LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);

    if (pxFormat != kLX_YCbCr422_INT8) {
        LXErrorSet(outError, 1812, "invalid pixel format specified for native API writer (int8 expected)");  // this shouldn't happen (data should be converted before)
        return NO;
    }
    
    // --- note: width of image is required to be divisible by 16! ---
    if (w % 16 != 0) {
        LXPrintf("*** Lacefx API warning (%s): width is not divisible by 16 (is %i)\n", __func__, (int)w);
        w = (w / 16) * 16;
    }
    
    FILE *outfile = NULL;
    if (unipath.unistr) {
        if ( !LXOpenFileForWritingWithUnipath(unipath.unistr, unipath.numOfChar16, (LXFilePtr *)&outfile)) {
            LXErrorSet(outError, 1760, "could not open file");
            return NO;
        }
    }

    size_t srcRowBytes = 0;
    uint8_t *srcData = (uint8_t *) LXPixelBufferLockPixels(pixbuf, &srcRowBytes, NULL, outError);
    if ( !srcData) return NO;


    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = 3;
	jpeg_set_defaults(&cinfo);

	jpeg_set_colorspace(&cinfo, JCS_YCbCr);
    cinfo.do_fancy_downsampling = FALSE;

    jpeg_set_quality(&cinfo, MAX(0, MIN(100, (int)(jpegQualityF * 100))), FALSE);  // last argument indicates baseline compatibility
    
    cinfo.dct_method = JDCT_FLOAT;  // seems to be faster on modern x86

	cinfo.raw_data_in = TRUE;   // raw YCbCr, 4:2:0 sampling
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

    // set up destination and start compress
    if ( !outfile) {
        ///LXPrintf("...setting jpeg mem dest: %p, size %i\n", *optionalBuffer, *optionalBufferSize);
        jpeg_mem_dest(&cinfo, optionalBuffer, optionalBufferSize);
    } else {
        jpeg_stdio_dest(&cinfo, outfile);
    }
    jpeg_start_compress(&cinfo, TRUE);  // last argument indicates "complete interchange JPEG"
    

    size_t dstRowBytes_y = w;
    size_t dstRowBytes_chroma = LXAlignedRowBytes(w / 2);
    uint8_t *tempDataY = _lx_malloc(16 * (dstRowBytes_y + 2*dstRowBytes_chroma));
    uint8_t *tempDataCb = tempDataY + 16*dstRowBytes_y;
    uint8_t *tempDataCr = tempDataCb + 16*dstRowBytes_chroma;
    
    ///LXPrintf("... %s: size %i * %i -- chroma rowbytes %i (using file: %i)\n", __func__, w, h, dstRowBytes_chroma, (outfile != NULL));

    JSAMPROW Y_ptr[16], Cb_ptr[16], Cr_ptr[16];
    LXUInteger y;
    for (y = 0; y < 16; y++) {
        Y_ptr[y] = tempDataY + y*dstRowBytes_y;
        Cb_ptr[y] = tempDataCb + y*dstRowBytes_chroma;
        Cr_ptr[y] = tempDataCr + y*dstRowBytes_chroma;
    }
	JSAMPARRAY planes[3] = { Y_ptr, Cb_ptr, Cr_ptr };
    
    
    for (y = 0; y < h; y += 16) {
        LXUInteger numLines = (y + 16 <= h) ? 16 : (h - y);
        
        uint8_t *src = ((uint8_t *)srcData + srcRowBytes * y);
    
        LXPxConvert_YCbCr422_to_420_planar_padY_convertLevelsTo255(w, 
                                                    src, numLines, srcRowBytes,
                                                    tempDataY, dstRowBytes_y,
                                                    tempDataCb, tempDataCr, dstRowBytes_chroma,
                                                    16,
                                                    convertLevelsTo255);
    
        jpeg_write_raw_data(&cinfo, planes, 16);
    }
    
    jpeg_finish_compress(&cinfo);
    
    if (outfile)
        _lx_fclose(outfile);
    
    jpeg_destroy_compress(&cinfo);

    _lx_free(tempDataY);
    LXPixelBufferUnlockPixels(pixbuf);    
    return YES;
}

static LXPixelBufferRef readJPEGImage_YCbCr422_int8(const uint8_t *jpegData, size_t jpegDataLen, LXBool convertLevelsFrom255,
                                                    LXError *outError)
{
    if ( !jpegData || jpegDataLen < 1) return NULL;
    
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (uint8_t *)jpegData, jpegDataLen);

    jpeg_read_header(&cinfo, TRUE);
    
    cinfo.raw_data_out = TRUE;
    cinfo.out_color_space = JCS_YCbCr;
    cinfo.do_fancy_upsampling = FALSE;
    cinfo.do_block_smoothing = FALSE;
    cinfo.dct_method = JDCT_FLOAT;  // seems to be fastest on modern x86

	/*cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;*/

    jpeg_start_decompress(&cinfo);
    
    LXInteger w = cinfo.output_width;
    LXInteger h = cinfo.output_height;
    LXInteger numChannels = cinfo.output_components;
    LXPixelBufferRef newPixbuf = NULL;
    
    if (w < 1 || h < 1) {
        LXErrorSet(outError, 1622, "could not read JPEG data");
    } else if (numChannels < 3 || cinfo.comp_info[0].h_samp_factor != 2 || cinfo.comp_info[1].h_samp_factor != 1 || cinfo.comp_info[1].v_samp_factor != 1) {
        LXErrorSet(outError, 1622, "could not read JPEG data (component format is not YCC, or sampling is not 4:2:0");
    } else {
        newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_YCbCr422_INT8, outError);
        
        size_t dstRowBytes = 0;
        uint8_t *dstBuf = (newPixbuf) ? LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, outError) : NULL;
        
        if (dstBuf) {
            size_t srcRowBytes_y = LXAlignedRowBytes(w);
            size_t srcRowBytes_chroma = LXAlignedRowBytes(w / 2);
            uint8_t *tempDataY = _lx_malloc(16 * (srcRowBytes_y + 2*srcRowBytes_chroma));
            uint8_t *tempDataCb = tempDataY + 16*srcRowBytes_y;
            uint8_t *tempDataCr = tempDataCb + 16*srcRowBytes_chroma;
    
            ///LXPrintf("... %s: size %i * %i, nc %i -- chroma rowbytes %i -- dst rb %i\n", __func__, w, h, numChannels, srcRowBytes_chroma, dstRowBytes);

            JSAMPROW Y_ptr[16], Cb_ptr[16], Cr_ptr[16];
            LXUInteger y;
            for (y = 0; y < 16; y++) {
                Y_ptr[y] = tempDataY + y*srcRowBytes_y;
                Cb_ptr[y] = tempDataCb + y*srcRowBytes_chroma;
                Cr_ptr[y] = tempDataCr + y*srcRowBytes_chroma;
            }
            JSAMPARRAY planes[3] = { Y_ptr, Cb_ptr, Cr_ptr };
            
            for (y = 0; y < h; y += 16) {
                LXUInteger numLines = (y + 16 <= h) ? 16 : (h - y);
        
                jpeg_read_raw_data(&cinfo, planes, 16);
                                
                uint8_t *dst = dstBuf + dstRowBytes*y;
                
                LXPxConvert_YCbCr420_planar_to_422_convertLevelsFrom255(w, numLines,
                                                                        tempDataY, srcRowBytes_y,
                                                                        tempDataCb, tempDataCr, srcRowBytes_chroma,
                                                                        dst, dstRowBytes,
                                                                        convertLevelsFrom255);
            }
            
            _lx_free(tempDataY);
            LXPixelBufferUnlockPixels(newPixbuf);
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return newPixbuf;
}



static LXPixelBufferRef readJPEGImage_RGBA_int8(const uint8_t *jpegData, size_t jpegDataLen,
                                                LXError *outError)
{
    if ( !jpegData || jpegDataLen < 1) return NULL;
    
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);    
    jpeg_mem_src(&cinfo, (uint8_t *)jpegData, jpegDataLen);

    cinfo.dct_method = JDCT_FLOAT;  // seems to be faster on modern x86
    
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    
    LXInteger w = cinfo.output_width;
    LXInteger h = cinfo.output_height;
    LXInteger numChannels = cinfo.output_components;
    LXPixelBufferRef newPixbuf = NULL;
    
    ///LXPrintf("...decoding jpeg to rgba: %i * %i, nc %i\n", w, h, numChannels);
    
    if (w < 1 || h < 1 || numChannels < 1) {
        LXErrorSet(outError, 1622, "could not read JPEG data");
    } else {
        newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
        
        size_t dstRowBytes = 0;
        uint8_t *dstBuf = (newPixbuf) ? LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, outError) : NULL;
        
        if (dstBuf) {
            JSAMPROW buffer = _lx_malloc(w * numChannels);
            
            LXInteger y;
            for (y = 0; y < h; y++) {
                uint8_t *dst = dstBuf + dstRowBytes*y;
                
                jpeg_read_scanlines(&cinfo, &buffer, 1);
                
                if (numChannels == 3) {
                    LXPxConvert_RGB_to_RGBA_int8(w, 1, buffer, w*3, 3,
                                                 dst, dstRowBytes);
                } else if (numChannels == 1) {
                    LXPxConvert_lum_to_RGBA_int8(w, 1, buffer, w, 1,
                                                 dst, dstRowBytes);
                } else if (numChannels == 4) {
                    memcpy(dst, buffer, w*4);
                }
            }
            
            _lx_free(buffer);
            LXPixelBufferUnlockPixels(newPixbuf);
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return newPixbuf;
}


static LXSuccess writeJPEGImage_RGBA_int8(LXPixelBufferRef pixbuf,
                                                            LXUnibuffer unipath,
                                                            LXFloat jpegQualityF,
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
    
    FILE *outfile = NULL;
    if ( !LXOpenFileForWritingWithUnipath(unipath.unistr, unipath.numOfChar16, (LXFilePtr *)&outfile)) {
        LXErrorSet(outError, 1760, "could not open file");
        return NO;
    }

    size_t srcRowBytes = 0;
    uint8_t *srcData = (uint8_t *) LXPixelBufferLockPixels(pixbuf, &srcRowBytes, NULL, outError);
    if ( !srcData) return NO;
    
    // -- includeAlpha is currently ignored
    size_t dstRowBytes = w * 3;  //(includeAlpha ? 4 : 3);
    uint8_t *tempRowData = _lx_malloc(dstRowBytes);

        
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    
    jpeg_set_quality(&cinfo, MAX(0, MIN(100, (int)(jpegQualityF * 100))), FALSE);  // last argument indicates baseline compatibility
    
    cinfo.dct_method = JDCT_FLOAT;  // seems to be faster on modern x86

    // set up destination and start compress
    jpeg_stdio_dest(&cinfo, outfile);
    jpeg_start_compress(&cinfo, TRUE);  // last argument indicates "complete interchange JPEG"
    
    LXUInteger y;
    for (y = 0; y < h; y++) {
        JSAMPROW rowptr[1];
        uint8_t *src = ((uint8_t *)srcData + srcRowBytes * y);
        
        LXPxConvert_RGBA_to_RGB_int8(w, 1,  src, srcRowBytes, 4,  tempRowData, dstRowBytes, 3);
        rowptr[0] = tempRowData;
        
        jpeg_write_scanlines(&cinfo, rowptr, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    _lx_fclose(outfile);
    
    jpeg_destroy_compress(&cinfo);
    
    _lx_free(tempRowData);
    LXPixelBufferUnlockPixels(pixbuf);    
    return YES;
}


LXSuccess LXPixelBufferWriteAsJPEGImageToPath(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf) return NO;

    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);
    
    LXBool includeAlpha = NO;
    LXInteger preferredBitDepth = 8;
    LXInteger colorSpaceID = 0;
    double jpegQuality = 0.5;
    if (properties) {
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowAlpha, &includeAlpha);
        LXMapGetInteger(properties, kLXPixelBufferAttachmentKey_ColorSpaceEncoding, &colorSpaceID);
        LXMapGetInteger(properties, kLXPixelBufferFormatRequestKey_PreferredBitsPerChannel, &preferredBitDepth);
        LXMapGetDouble(properties, kLXPixelBufferFormatRequestKey_CompressionQuality, &jpegQuality);
    }

    uint8_t *iccDataBuf = NULL;
    size_t iccDataLen = 0;
    LXCopyICCProfileDataForColorSpaceEncoding(colorSpaceID, &iccDataBuf, &iccDataLen);
    
    LXPixelBufferRef tempPixbuf = NULL;
    LXSuccess retVal = NO;

    ///LXPrintf("writing jpeg file: pxformat %i, quality %.2f, cspace id %i\n", pxFormat, jpegQuality, colorSpaceID);

    {
        if (pxFormat == kLX_RGBA_INT8) {
            retVal = writeJPEGImage_RGBA_int8(pixbuf, unipath, jpegQuality, includeAlpha, iccDataBuf, iccDataLen, outError);
        }
        else if (pxFormat == kLX_YCbCr422_INT8 && (w % 16 == 0)) {
            retVal = writeJPEGImage_YCbCr422_int8(pixbuf, unipath, NULL, NULL, jpegQuality,
                                                  YES /* convert levels to 255 */, outError);
        }
        else {
            tempPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
            
            if (tempPixbuf && LXPixelBufferCopyPixelBufferWithPixelFormatConversion(tempPixbuf, pixbuf, outError)) {
                retVal = writeJPEGImage_RGBA_int8(tempPixbuf, unipath, jpegQuality, includeAlpha, iccDataBuf, iccDataLen, outError);
            }            
        }
    }
            
    LXPixelBufferRelease(tempPixbuf);
    _lx_free(iccDataBuf);
    
    return retVal;
}

LXSuccess LXPixelBufferWriteAsJPEGImageInMemory_raw422_(LXPixelBufferRef pixbuf, uint8_t **outBuffer, size_t *outBufferSize,
                                                        size_t *outBytesWritten,
                                                        LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf || !outBuffer || !outBufferSize || !outBytesWritten) return NO;

    const LXUInteger w = LXPixelBufferGetWidth(pixbuf);
    const LXUInteger h = LXPixelBufferGetHeight(pixbuf);
    LXUInteger pxFormat = LXPixelBufferGetPixelFormat(pixbuf);
    
    if (pxFormat != kLX_YCbCr422_INT8) {
        LXErrorSet(outError, 1812, "invalid pixel format specified for in-memory JPEG writer (ycbcr422 expected)");
        return NO;
    }
    
    double jpegQuality = 0.5;
    if (properties) {
        LXMapGetDouble(properties, kLXPixelBufferFormatRequestKey_CompressionQuality, &jpegQuality);
    }

    // libjpeg API uses unsigned long for byte count.
    // number of bytes written will be returned here.
    unsigned long bufferSize = *outBufferSize;
    uint8_t *buffer = *outBuffer;
    
    if (*outBuffer == NULL) {
        *outBufferSize = bufferSize = w*h*4;  // just alloc enough space right away
        *outBuffer = buffer = _lx_malloc(bufferSize);
    }
    
    LXBool writeRaw601 = NO;
    if (LXMapContainsValueForKey(properties, "writeRaw601VideoLevels", NULL)) {
        LXMapGetBool(properties, "writeRaw601VideoLevels", &writeRaw601);
    }
    //if ( !is601) LXPrintf("%s: data is not 601, will convert levels\n", __func__);
    
    LXBool retVal = writeJPEGImage_YCbCr422_int8(pixbuf, LXMakeUnibuffer(0, NULL),
                                                    &buffer, &bufferSize,
                                                    jpegQuality,
                                                    (writeRaw601) ? NO : YES /* convert levels to 255 */, outError);

    if (buffer != *outBuffer) {
        LXPrintf("** %s: libjpeg did realloc our buffer (this is not always desirable due to potential allocator differences -- new size %ld, prev %ld)\n",
                                __func__, (long)bufferSize, (long)*outBufferSize);
        *outBuffer = buffer;
        *outBufferSize = bufferSize;
    }
    
    *outBytesWritten = bufferSize;
    
    return retVal;
}

LXPixelBufferRef LXPixelBufferCreateFromJPEGImageInMemory(const uint8_t *jpegData, size_t jpegDataLen, LXMapPtr properties, LXError *outError)
{
    LXBool allowYUV = NO;
    if (properties) {
        LXMapGetBool(properties, kLXPixelBufferFormatRequestKey_AllowYUV, &allowYUV);
    }
    
    if (allowYUV) {
        LXBool is601 = NO;
        if (LXMapContainsValueForKey(properties, "uses601VideoLevels", NULL)) {
            LXMapGetBool(properties, "uses601VideoLevels", &is601);
        }
        
        return readJPEGImage_YCbCr422_int8(jpegData, jpegDataLen, (is601) ? NO : YES, outError);
    } else {
        return readJPEGImage_RGBA_int8(jpegData, jpegDataLen, outError);
    }
}

