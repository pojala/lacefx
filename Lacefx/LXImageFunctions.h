/*
 *  LXImageFunctions.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 5.5.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXIMAGEFUNCTIONS_H_
#define _LXIMAGEFUNCTIONS_H_

#include "LXBasicTypes.h"
#include "LXHalfFloat.h"


// for convenience of browing through functions,
// constants are located at the end of this file


#if defined(__SSE2__) && !defined(LXVEC)
#define LXVEC 1
#endif


#ifdef __cplusplus
extern "C" {
#endif

// -- scaling

LXEXPORT void LXImageScale_RGBA_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                     uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                     LXError *outError);

LXEXPORT void LXImageScale_RGBA_int16(uint16_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                      uint16_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                      LXError *outError);
                           
LXEXPORT void LXImageScale_RGBA_float32(float * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                        float * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                        LXError *outError);

LXEXPORT void LXImageScale_RGBAWithDepth(void * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                         void * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                         LXUInteger depth,
                                         LXError *outError);

LXEXPORT void LXImageScale_Luminance_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                          uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                          LXError *outError);

LXEXPORT void LXImageScale_Luminance_float32(float * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                             float * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                             LXError *outError);

// -- basic pixel format conversions.

// having so many of these plain-jane pixel component order swizzler functions is boring but unavoidable --
// better have them in one place here (rather than all over the plugins, etc.), so at least they are properly optimized.
LXEXPORT void LXPxConvert_ARGB_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_int8(const LXInteger w, const LXInteger h,        // can be used for RGBA<->ABGR and ARGB<->BGRA
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_BGRA_int8(const LXInteger w, const LXInteger h,   // can be used for RGBA<->BGRA and ARGB<->GRAB
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

// int16 versions (works for float16 as well, of course)
LXEXPORT void LXPxConvert_ARGB_to_RGBA_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_ARGB_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_int16(const LXInteger w, const LXInteger h,        // can be used for RGBA<->ABGR and ARGB<->BGRA
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_BGRA_int16(const LXInteger w, const LXInteger h,   // can be used for RGBA<->BGRA and ARGB<->GRAB
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

// float32 versions
LXEXPORT void LXPxConvert_ARGB_to_RGBA_float32(const LXInteger w, const LXInteger h, 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_ARGB_float32(const LXInteger w, const LXInteger h, 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_float32(const LXInteger w, const LXInteger h,        // can be used for RGBA<->ABGR and ARGB<->BGRA
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_reverse_BGRA_float32(const LXInteger w, const LXInteger h,   // can be used for RGBA<->BGRA and ARGB<->GRAB 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes);

// functions that don't convert pixels, but just copy between different rowbytes values
LXEXPORT void LXPxCopy_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxCopy_RGBA_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxCopy_RGBA_float32(const LXInteger w, const LXInteger h, 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes);

// non-4-component formats
LXEXPORT void LXPxConvert_RGB_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGB_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGB_to_RGBA_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const uint16_t dstCodingWhite);

LXEXPORT void LXPxConvert_RGBA_to_RGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const LXInteger dstByteStride);

LXEXPORT void LXPxConvert_lum_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_lum_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

LXEXPORT void LXPxConvert_RGBA_to_monochrome_lum_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, 
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const LXInteger dstByteStride,
                                  const float rw, const float gw, const float bw); // typical weight factors are provided as constants below, e.g. "kLX_601_toY__R" for Rec.601 red

// bit depth conversions
LXEXPORT void LXPxConvert_int8_to_float32(uint8_t * LXRESTRICT srcBuf, float * LXRESTRICT dstBuf, const size_t count);
LXEXPORT void LXPxConvert_int8_to_float16(uint8_t * LXRESTRICT srcBuf, LXHalf * LXRESTRICT dstBuf, const size_t count);
LXEXPORT void LXPxConvert_int16_to_float16(uint16_t * LXRESTRICT srcBuf, LXHalf * LXRESTRICT dstBuf, const size_t count, const uint16_t srcCodingWhite);

LXEXPORT void LXPxConvert_float32_to_int8(float * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count);
LXEXPORT void LXPxConvert_float16_to_int8(LXHalf * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count);
LXEXPORT void LXPxConvert_float16_to_int16(LXHalf * LXRESTRICT srcBuf, uint16_t * LXRESTRICT dstBuf, const size_t count, const uint16_t dstCodingWhite);

LXEXPORT void LXPxConvert_RGBA_float16_rawYUV_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);  // input must be already transformed to YUV in [0,1] range for all components

LXEXPORT void LXPxConvert_YCbCr422_to_RGBA_float16_rawYUV(const LXInteger w, const LXInteger h,
                                                uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                                LXHalf * LXRESTRICT dstBuf, const size_t dstRowBytes);  // output is raw YUV with [0,1] range for all components

LXEXPORT void LXPxConvert_RGBA_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                         LXColorSpaceEncoding cspace);  // cspace must be kLX_YCbCr_Rec601 or kLX_YCbCr_Rec709
                                         
LXEXPORT void LXPxConvert_ARGB_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                         LXColorSpaceEncoding cspace);  // cspace must be kLX_YCbCr_Rec601 or kLX_YCbCr_Rec709

LXEXPORT void LXPxConvert_YCbCr422_to_RGBA_int8(const LXInteger w, const LXInteger h,
                                              uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                              uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                              LXColorSpaceEncoding cspace);  // cspace must be kLX_YCbCr_Rec601 or kLX_YCbCr_Rec709

LXEXPORT void LXPxConvert_YCbCr422_YUY2_to_RGBA_int8(const LXInteger w, const LXInteger h,
                                                   uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                                   uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                                   LXColorSpaceEncoding cspace);  // cspace must be kLX_YCbCr_Rec601 or kLX_YCbCr_Rec709

LXEXPORT void LXPxConvert_RGB_float16_to_RGB_int10(const LXInteger w, const LXInteger h, 
                                        LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite);  // packing format for 10-bit pixels is "align to high byte" (aka. left-justified in big-endian)

LXEXPORT void LXPxConvert_RGB_float32_to_RGB_int10(const LXInteger w, const LXInteger h, 
                                        float * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite);

LXEXPORT void LXPxConvert_RGB_float16_to_RGB_int16(const LXInteger w, const LXInteger h, 
                                        LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite);

LXEXPORT void LXPxConvert_RGB_float32_to_RGB_int16(const LXInteger w, const LXInteger h, 
                                        float * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite);

// -- utilities --

// multiplies RGB components by alpha
LXEXPORT void LXImagePremultiply_RGBA_int8_inplace(const LXInteger w, const LXInteger h, 
                                        uint8_t * LXRESTRICT buf, const size_t rowBytes);
LXEXPORT void LXImagePremultiply_RGBA_int8(const LXInteger w, const LXInteger h, 
                                        uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                        uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes);

// blends two YCbCr 4:2:2 scanlines
LXEXPORT void LXImageBlend_YCbCr422(uint8_t * LXRESTRICT outBuf,
                                    uint8_t * LXRESTRICT buf1, uint8_t * LXRESTRICT buf2,
                                    LXInteger w);

// computes 256-entry histogram
LXEXPORT void LXImageCalcHistogram_RGBA_int8(const LXInteger w, const LXInteger h,
                                             uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                             uint32_t * LXRESTRICT histBuf);  // size of histBuf must be 4*256 uint32_t


#if defined(LXVEC)
// --- vectorized utilities ---
LXEXPORT void LXPxConvert_int32_to_float32_withRange_inplace_VEC(int32_t * LXRESTRICT buf, const size_t count,
                                                                 const int32_t srcMinCoding, const int32_t srcMaxCoding);
LXEXPORT void LXPxConvert_float32_to_int32_withRange_inplace_VEC(float * LXRESTRICT buf, const size_t count,
                                                                 const int32_t dstMinCoding, const int32_t dstMaxCoding);
LXEXPORT void LXPxConvert_float32_to_int32_withSaturate_inplace_VEC(float * LXRESTRICT buf, const size_t count,
                                                                 const int32_t dstMaxCoding);
                                                                 
LXEXPORT void LXPxConvert_int8_to_float32_withRange_VEC(uint8_t * LXRESTRICT srcBuf, float * LXRESTRICT dstBuf, const size_t count,
                                                        const int32_t intMin, const int32_t intMax);
#endif


#ifdef __cplusplus
}
#endif


// ---------------------------------
// RGB<->YUV color matrix constants:

// Rec.601, Y'PbPr
//  Y =   0.299    · R + 0.587    · G + 0.114    · B 
//  Pb = -0.168736 · R - 0.331264 · G + 0.5      · B 
//  Pr =  0.5      · R - 0.418688 · G - 0.081312 · B 
#define kLX_601_toY__R   0.299f
#define kLX_601_toY__G   0.587f
#define kLX_601_toY__B   0.114f
#define kLX_601_toPb_R  -0.168736f
#define kLX_601_toPb_G  -0.331264f
#define kLX_601_toPb_B  0.5f
#define kLX_601_toPr_R  0.5f
#define kLX_601_toPr_G  -0.418688f
#define kLX_601_toPr_B  -0.081312f

// for YCbCr 8-bit (aka "601.219" or "709.219"), scale YPbPr's Y row by 219 and chroma rows by 224, and add offsets of 16 / 128.
#define kLX_Y219_scale     219
#define kLX_C219_scale     224
#define kLX_Y219_offset    16
#define kLX_C219_offset    128

// inverse:
//  R = 1.0 * Y + 0        * Pb + 1.402    * Pr
//  G = 1.0 * Y - 0.344136 * Pb - 0.714136 * Pr
//  B = 1.0 * Y + 1.772    * Pb + 0        * Pr
#define kLX_601_toR__Pr  1.402f
#define kLX_601_toG__Pb  -0.344136f
#define kLX_601_toG__Pr  -0.714136f
#define kLX_601_toB__Pb  1.772f

// Rec.709, Y'PbPr
//  Y =   0.2126   · R + 0.7152   · G + 0.0722   · B 
//  Pb = -0.114572 · R - 0.385428 · G + 0.5      · B 
//  Pr =  0.5      · R - 0.454153 · G - 0.045847 · B 
#define kLX_709_toY__R   0.2126f
#define kLX_709_toY__G   0.7152f
#define kLX_709_toY__B   0.0722f
#define kLX_709_toPb_R  -0.114572f
#define kLX_709_toPb_G  -0.385428f
#define kLX_709_toPb_B  0.5f
#define kLX_709_toPr_R  0.5f
#define kLX_709_toPr_G  -0.454153f
#define kLX_709_toPr_B  -0.045847f

// inverse:
//  R = 1.0 * Y + 0        * Pb + 1.5748   * Pr
//  G = 1.0 * Y - 0.187324 * Pb - 0.468124 * Pr
//  B = 1.0 * Y + 1.8556   * Pb + 0        * Pr
#define kLX_709_toR__Pr  1.5748f
#define kLX_709_toG__Pb  -0.187324f
#define kLX_709_toG__Pr  -0.468124f
#define kLX_709_toB__Pb  1.8556f

// ---------------------------------

#endif
