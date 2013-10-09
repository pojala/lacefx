/*
 *  LXImageFunctions.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 5.5.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXBasicTypes.h"
#include "LXImageFunctions.h"
#include "LXFixedPoint.h"
#include "LXPixelBuffer.h"
#include <math.h>


#if defined(__APPLE__) && defined(LXPLATFORM_MAC)
#define HAVE_VIMAGE 1
#import <Accelerate/Accelerate.h>   // contains vImage
#endif


#if defined (__SSE2__)

 
#define ISALIGNED(p_)  (((unsigned long)(p_) & 15) == 0)


/*
  Important note about MinGW compatibility:
  to ensure stack alignment, the -mstackrealign should be included in gcc flags.
  
  (Functions that contain SSE code should be marked LXFUNCATTR_SSE,
  but they are currently not, and this has not been tested.
  -mstackrealign does this stack realignment for all functions.)
*/

#include "LXVecInline_SSE2.h"

#ifndef _LX_MM_SHUFFLE_BIGENDIAN_STYLE
#define _LX_MM_SHUFFLE_BIGENDIAN_STYLE(fp0, fp1, fp2, fp3) \
                (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | (fp0))
#endif


LXFUNCATTR_SSE static
void LXPxConvert_int32_to_float32_withRange_inplace_SSE2(int32_t * LXRESTRICT buf, const size_t count,
                                                         const int32_t intMin, const int32_t intMax)
{
    const float fscale = 1.0 / (intMax - intMin);
    const vsint32_t v_bias = { intMin, intMin, intMin, intMin };
    const vfloat_t v_scale = { fscale, fscale, fscale, fscale };
    
    const LXUInteger n = count / 4;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vsint32_t v = _mm_load_si128((__m128i *)buf);
        v = _mm_sub_epi32(v, v_bias);
        
        vfloat_t vf = _mm_cvtepi32_ps(v);
        vf = _mm_mul_ps(vf, v_scale);
        
        _mm_store_ps((float *)buf, vf);
        buf += 4;
    }
}

void LXPxConvert_int32_to_float32_withRange_inplace_VEC(int32_t * LXRESTRICT buf, const size_t count,
                                                         const int32_t intMin, const int32_t intMax)
{
    LXPxConvert_int32_to_float32_withRange_inplace_SSE2(buf, count, intMin, intMax);
}

LXFUNCATTR_SSE static
void LXPxConvert_int8_to_float32_withRange_SSE2(uint8_t * LXRESTRICT srcBuf, float * LXRESTRICT dstBuf, const size_t count,
                                                const int32_t intMin, const int32_t intMax)
{
    const float fscale = 1.0 / (intMax - intMin);
    const vsint8_t v_zero = { 0, 0, 0, 0,  0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0 };
    const vsint32_t v_bias = { intMin, intMin, intMin, intMin };
    const vfloat_t v_scale = { fscale, fscale, fscale, fscale };

    const LXUInteger n = count / 16;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vuint8_t vb0 = _mm_load_si128((__m128i *)srcBuf);
        srcBuf += 16;
    
        vsint16_t vs0 = _mm_unpacklo_epi8(vb0, v_zero);
        vsint16_t vs1 = _mm_unpackhi_epi8(vb0, v_zero);
        
        vsint32_t v0 = _mm_unpacklo_epi16(vs0, v_zero);
        v0 = _mm_sub_epi32(v0, v_bias);
        v0 = (vsint32_t) _mm_cvtepi32_ps(v0);
        v0 = (vsint32_t) _mm_mul_ps((vfloat_t)v0, v_scale);
        
        vsint32_t v1 = _mm_unpackhi_epi16(vs0, v_zero);
        v1 = _mm_sub_epi32(v1, v_bias);
        v1 = (vsint32_t) _mm_cvtepi32_ps(v1);
        v1 = (vsint32_t) _mm_mul_ps((vfloat_t)v1, v_scale);
        
        vsint32_t v2 = _mm_unpacklo_epi16(vs1, v_zero);
        v2 = _mm_sub_epi32(v2, v_bias);
        v2 = (vsint32_t) _mm_cvtepi32_ps(v2);
        v2 = (vsint32_t) _mm_mul_ps((vfloat_t)v2, v_scale);

        vsint32_t v3 = _mm_unpackhi_epi16(vs1, v_zero);        
        v3 = _mm_sub_epi32(v3, v_bias);
        v3 = (vsint32_t) _mm_cvtepi32_ps(v3);
        v3 = (vsint32_t) _mm_mul_ps((vfloat_t)v3, v_scale);
        
        _mm_store_si128((__m128i *)dstBuf, v0);
        _mm_store_si128((__m128i *)(dstBuf + 4), v1);
        _mm_store_si128((__m128i *)(dstBuf + 8), v2);
        _mm_store_si128((__m128i *)(dstBuf + 12), v3);
        dstBuf += 16;
    }
}

void LXPxConvert_int8_to_float32_withRange_VEC(uint8_t * LXRESTRICT srcBuf, float * LXRESTRICT dstBuf, const size_t count,
                                                const int32_t intMin, const int32_t intMax)
{
    LXPxConvert_int8_to_float32_withRange_SSE2(srcBuf, dstBuf, count, intMin, intMax);
}


LXFUNCATTR_SSE static
void LXPxConvert_float32_to_int32_withRange_inplace_SSE2(float * LXRESTRICT buf, const size_t count,
                                                         const int32_t intMin, const int32_t intMax)
{
    const float fscale = (intMax - intMin);
    const vsint32_t v_bias = { intMin, intMin, intMin, intMin };
    const vfloat_t v_scale = { fscale, fscale, fscale, fscale };
    
    const LXUInteger n = count / 4;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vfloat_t vf = _mm_load_ps((float *)buf);
        vf = _mm_mul_ps(vf, v_scale);
        
        vsint32_t v = _lx_vec_cts(vf); // lx_vec_cts handles overflow smarter (Altivec-style) than the plain conversion intrinsic:  _mm_cvtps_epi32(vf);
        v = _mm_add_epi32(v, v_bias);
        
        _mm_store_si128((__m128i *)buf, v);
        buf += 4;
    }
}

void LXPxConvert_float32_to_int32_withRange_inplace_VEC(float * LXRESTRICT buf, const size_t count,
                                                         const int32_t intMin, const int32_t intMax)
{
    LXPxConvert_float32_to_int32_withRange_inplace_SSE2(buf, count, intMin, intMax);
}


LXFUNCATTR_SSE static
void LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2(float * LXRESTRICT buf, const size_t count,
                                                            const int32_t intMax)
{
    const float fscale = intMax;
    const vfloat_t v_scale = { fscale, fscale, fscale, fscale };
    const vfloat_t v_zero = { 0.0f, 0.0f, 0.0f, 0.0f };
    const vfloat_t v_one  = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    const LXUInteger n = count / 4;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vfloat_t vf = _mm_load_ps((float *)buf);
        
        vf = _mm_min_ps(vf, v_one);
        vf = _mm_max_ps(vf, v_zero);
        vf = _mm_mul_ps(vf, v_scale);
        
        vsint32_t v = _lx_vec_cts(vf); // lx_vec_cts handles overflow smarter (Altivec-style) than the plain conversion intrinsic:  _mm_cvtps_epi32(vf);
        
        _mm_store_si128((__m128i *)buf, v);
        buf += 4;
    }
}

void LXPxConvert_float32_to_int32_withSaturate_inplace_VEC(float * LXRESTRICT buf, const size_t count,
                                                         const int32_t intMax)
{
    LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2(buf, count, intMax);
}

LXFUNCATTR_SSE static
void LXPxConvert_int32_to_int8_withSaturate_SSE2(int32_t * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count)
{    
    const LXUInteger n = count / 16;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vsint32_t v0 = _mm_load_si128((__m128i *)srcBuf);
        vsint32_t v1 = _mm_load_si128((__m128i *)(srcBuf + 4));
        vsint32_t v2 = _mm_load_si128((__m128i *)(srcBuf + 8));
        vsint32_t v3 = _mm_load_si128((__m128i *)(srcBuf + 12));
        srcBuf += 16;
        
        vsint16_t vs0 = _mm_packs_epi32(v0, v1);
        vsint16_t vs1 = _mm_packs_epi32(v2, v3);
        
        vuint8_t vb0 = _mm_packus_epi16(vs0, vs1);
        _mm_store_si128((__m128i *)dstBuf, vb0);
        dstBuf += 16;
    }
}

void LXPxConvert_int32_to_int8_withSaturate_VEC(int32_t * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count)
{
    LXPxConvert_int32_to_int8_withSaturate_SSE2(srcBuf, dstBuf, count);
}


LXFUNCATTR_SSE static
void LXPxConvert_int32_to_int16_withSaturate_SSE2(int32_t * LXRESTRICT srcBuf, uint16_t * LXRESTRICT dstBuf, const size_t count)
{    
    const LXUInteger n = count / 8;
    LXUInteger i;
    for (i = 0; i < n; i++) {
        vsint32_t v0 = _mm_load_si128((__m128i *)srcBuf);
        vsint32_t v1 = _mm_load_si128((__m128i *)(srcBuf + 4));
        srcBuf += 8;
        
        vsint16_t vs0 = _mm_packs_epi32(v0, v1);
        _mm_store_si128((__m128i *)dstBuf, vs0);
        dstBuf += 8;
    }
}

void LXPxConvert_int32_to_int16_withSaturate_VEC(int32_t * LXRESTRICT srcBuf, uint16_t * LXRESTRICT dstBuf, const size_t count)
{
    LXPxConvert_int32_to_int16_withSaturate_SSE2(srcBuf, dstBuf, count);
}

#endif  // __SSE2__




#define CLAMP_FLOAT_PX_TO_UINT8(v_)   (uint8_t)(255.0f * CLAMP_SAT_F(v_))

// endian-independent uint32 byte operations
#ifdef __BIG_ENDIAN__
 #define GETBYTE0(u_)  (((u_) >> 24)  & 0xff)
 #define GETBYTE1(u_)  (((u_) >> 16)  & 0xff)
 #define GETBYTE2(u_)  (((u_) >> 8)   & 0xff)
 #define GETBYTE3(u_)  ((u_)          & 0xff)
 
 #define SETBYTE0(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 24))
 #define SETBYTE1(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 16))
 #define SETBYTE2(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 8))
 #define SETBYTE3(u_, c_)   u_ = (u_ | ((uint32_t)(c_)))

 #define MAKEPX_2VUY(y1_, y2_, cb_, cr_)   (cb_ << 24) | (y1_ << 16) | (cr_ << 8) | (y2_)
 
#else
 // little-endian versions

 #define GETBYTE3(u_)  (((u_) >> 24)  & 0xff)
 #define GETBYTE2(u_)  (((u_) >> 16)  & 0xff)
 #define GETBYTE1(u_)  (((u_) >> 8)   & 0xff)
 #define GETBYTE0(u_)  ((u_)        & 0xff)
 
 #define SETBYTE3(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 24))
 #define SETBYTE2(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 16))
 #define SETBYTE1(u_, c_)   u_ = (u_ | ((uint32_t)(c_) << 8))
 #define SETBYTE0(u_, c_)   u_ = (u_ | ((uint32_t)(c_))) 
 
 #define MAKEPX_2VUY(y1_, y2_, cb_, cr_)   (cb_) | (y1_ << 8) | (cr_ << 16) | (y2_ << 24)
 
#endif



void LXImageScale_RGBAWithDepth(void * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                void * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                LXUInteger depth,
                                LXError *outError)
{
    switch (depth) {
        case 8:
            LXImageScale_RGBA_int8((uint8_t *)srcBuf, srcW, srcH, srcRowBytes,  (uint8_t *)dstBuf, dstW, dstH, dstRowBytes, outError);
            break;
            
        case 16:
            LXImageScale_RGBA_int16((uint16_t *)srcBuf, srcW, srcH, srcRowBytes,  (uint16_t *)dstBuf, dstW, dstH, dstRowBytes, outError);
            break;
            
        case 32:
            LXImageScale_RGBA_float32((float *)srcBuf, srcW, srcH, srcRowBytes,  (float *)dstBuf, dstW, dstH, dstRowBytes, outError);
            break;
            
        default:
            LXErrorSet(outError, 1999, "unsupported sample depth for LXImageScaleRGBA");
            break;
    }
}

#define SCALING_FIXD_Q  8

static void scale_nearest_lum_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                        uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes)
{
    LXInteger x, y;
    const float xinc = (float)srcW / dstW;
    
    // precompute sampling positions in fixed point
    uint32_t scalePosArr[dstW-1];
    for (x = 0; x < dstW - 1; x++) {
        float pos =  xinc * (float)x;
        scalePosArr[x] = FLOORF(pos * (1<<SCALING_FIXD_Q));
    }
    
    for (y = 0; y < dstH; y++) {
        float sy = floor( ((float)y / dstH) * (float)srcH );
        
        uint8_t * LXRESTRICT srcRow = (uint8_t *) (srcBuf + (LXInteger)sy * srcRowBytes);
        uint8_t * LXRESTRICT dstRow = (uint8_t *) (dstBuf + y * dstRowBytes);
        
        for (x = 0; x < dstW-1; x++) {
            uint32_t pos = scalePosArr[x];
            uint32_t sx = pos >> SCALING_FIXD_Q;
            *dstRow++ = srcRow[sx];
        }
        *dstRow = srcRow[srcW-1];
    }
}

static void scale_nearest_lum_float32(float * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                      float * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes)
{
    LXInteger x, y;
    const float xinc = (float)srcW / dstW;
    
    // precompute sampling positions in fixed point
    uint32_t scalePosArr[dstW-1];
    for (x = 0; x < dstW - 1; x++) {
        float pos =  xinc * (float)x;
        scalePosArr[x] = FLOORF(pos * (1<<SCALING_FIXD_Q));
    }
    
    for (y = 0; y < dstH; y++) {
        float sy = floor( ((float)y / dstH) * (float)srcH );
        
        float * LXRESTRICT srcRow = (float *) (srcBuf + (LXInteger)sy * srcRowBytes);
        float * LXRESTRICT dstRow = (float *) (dstBuf + y * dstRowBytes);
        
        for (x = 0; x < dstW-1; x++) {
            uint32_t pos = scalePosArr[x];
            uint32_t sx = pos >> SCALING_FIXD_Q;
            *dstRow++ = srcRow[sx];
        }
        *dstRow = srcRow[srcW-1];
    }
}

static void scale_nearest_RGBA_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                        uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes)
{
    LXInteger x, y;
    const float xinc = (float)srcW / dstW;

    // precompute sampling positions in fixed point
    uint32_t scalePosArr[dstW-1];
    for (x = 0; x < dstW - 1; x++) {
        float pos =  xinc * (float)x;
        scalePosArr[x] = FLOORF(pos * (1<<SCALING_FIXD_Q));
    }
    
    for (y = 0; y < dstH; y++) {
        float sy = FLOORF( ((float)y / dstH) * (float)srcH );
        
        uint32_t * LXRESTRICT srcRow = (uint32_t *) (srcBuf + (LXInteger)sy * srcRowBytes);
        uint32_t * LXRESTRICT dstRow = (uint32_t *) (dstBuf + y * dstRowBytes);
        
        for (x = 0; x < dstW-1; x++) {
            uint32_t pos = scalePosArr[x];
            uint32_t sx = pos >> SCALING_FIXD_Q;
            *dstRow++ = srcRow[sx];
        }
        *dstRow = srcRow[srcW-1];
    }
}

void LXImageScale_Luminance_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                 uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                 LXError *outError)
{
#ifdef HAVE_VIMAGE
    vImage_Buffer viSrc = { srcBuf, srcH, srcW, srcRowBytes };  // vImage struct has H before W
    vImage_Buffer viDst = { dstBuf, dstH, dstW, dstRowBytes };

    vImage_Error err = vImageScale_Planar8 (&viSrc, &viDst, NULL, kvImageEdgeExtend);
    if (err != kvImageNoError) {
        LXErrorSet(outError, (long)err + 2000, "vImageScale(1i8) failed");
    }

#else
    /*if (dstW > srcW || dstH > srcH) {
        magnify_bilinear_RGBA_int8(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    } else {*/
        scale_nearest_lum_int8(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    //}
#endif
}

void LXImageScale_Luminance_float32(float * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                             float * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                                             LXError *outError)
{
#ifdef HAVE_VIMAGE
    vImage_Buffer viSrc = { srcBuf, srcH, srcW, srcRowBytes };  // vImage struct has H before W
    vImage_Buffer viDst = { dstBuf, dstH, dstW, dstRowBytes };

    vImage_Error err = vImageScale_PlanarF (&viSrc, &viDst, NULL, kvImageEdgeExtend);
    if (err != kvImageNoError) {
        LXErrorSet(outError, (long)err + 2000, "vImageScale(1f) failed");
    }

#else
    /*if (dstW > srcW || dstH > srcH) {
        magnify_bilinear_RGBA_int8(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    } else {*/
        scale_nearest_lum_float32(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    //}
#endif
}

static void magnify_bilinear_RGBA_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                                        uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes)
{
    size_t tempBufRowBytes = dstW * 4;
    uint8_t *tempBuf = _lx_malloc(tempBufRowBytes * srcH);
    
    LXInteger x, y;
    
    // first pass: horizontal
    const float xinc = (float)(srcW - 1) / (dstW - 1);
    
    // precompute sampling positions in fixed point
    uint32_t scalePosArr[dstW-1];
    for (x = 0; x < dstW - 1; x++) {
        float pos =  xinc * (float)x;
        scalePosArr[x] = FLOORF(pos * (1<<SCALING_FIXD_Q));
    }
    
    for (y = 0; y < srcH; y++) {
        uint32_t * LXRESTRICT src = (uint32_t *) (srcBuf + y * srcRowBytes);
        uint32_t * LXRESTRICT dst = (uint32_t *) (tempBuf + y * tempBufRowBytes);
        
        for (x = 0; x < dstW - 1; x++) {
            uint32_t pos = scalePosArr[x];
            uint32_t sx = pos >> SCALING_FIXD_Q;
            uint32_t pos_fxd = pos & ((1<<SCALING_FIXD_Q) - 1);
            uint32_t ipos_fxd = (1<<SCALING_FIXD_Q) - pos_fxd;
            
            uint32_t s1 = src[sx];
            uint32_t s2 = src[sx+1];
            
            uint32_t v1 = ((s1 >> 24)*ipos_fxd + (s2 >> 24)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v2 = (((s1 >> 16) & 0xff)*ipos_fxd + ((s2 >> 16) & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v3 = (((s1 >> 8)  & 0xff)*ipos_fxd + ((s2 >> 8)  & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v4 = ((s1 & 0xff)*ipos_fxd + (s2 & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            
            *dst = (v1 << 24) | (v2 << 16) | (v3 << 8) | v4;
            dst++;
        }
        *dst = src[srcW-1];
    }
    
    // second pass: vertical
    const float yinc = (float)(srcH - 1) / (dstH - 1);
    
    for (y = 0; y < dstH - 1; y++) {
        uint32_t * LXRESTRICT dst = (uint32_t *) (dstBuf + y * dstRowBytes);
        
        float pos = yinc * (float)y;
        float posfloor = FLOORF(pos);
        float posfr = pos - posfloor;
        //float iposfr = 1.0f - posfr;
        LXInteger sy = posfloor;
        
        uint32_t pos_fxd = FLOORF(posfr * (1<<SCALING_FIXD_Q));
        uint32_t ipos_fxd = (1<<SCALING_FIXD_Q) - pos_fxd;

        uint32_t * LXRESTRICT src1 = (uint32_t *) (tempBuf + sy * tempBufRowBytes);
        uint32_t * LXRESTRICT src2 = (uint32_t *) (tempBuf + (sy + 1) * tempBufRowBytes);
        
        for (x = 0; x < dstW; x++) {
            uint32_t s1 = src1[x];
            uint32_t s2 = src2[x];
            
            /*uint32_t v1 = (s1 >> 24)*iposfr + (s2 >> 24)*posfr;
            uint32_t v2 = ((s1 >> 16) & 0xff)*iposfr + ((s2 >> 16) & 0xff)*posfr;
            uint32_t v3 = ((s1 >> 8)  & 0xff)*iposfr + ((s2 >> 8)  & 0xff)*posfr;
            uint32_t v4 = (s1 & 0xff)*iposfr + (s2 & 0xff)*posfr;
            */
            uint32_t v1 = ((s1 >> 24)*ipos_fxd + (s2 >> 24)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v2 = (((s1 >> 16) & 0xff)*ipos_fxd + ((s2 >> 16) & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v3 = (((s1 >> 8)  & 0xff)*ipos_fxd + ((s2 >> 8)  & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            uint32_t v4 = ((s1 & 0xff)*ipos_fxd + (s2 & 0xff)*pos_fxd) >> SCALING_FIXD_Q;
            
            *dst = (v1 << 24) | (v2 << 16) | (v3 << 8) | v4;
            dst++;
        }
    }
    memcpy(dstBuf + (dstH - 1) * dstRowBytes,  tempBuf + (srcH - 1) * tempBufRowBytes,  MIN(tempBufRowBytes, dstRowBytes));
    
    _lx_free(tempBuf);
}

void LXImageScale_RGBA_int8(uint8_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                            uint8_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                            LXError *outError)
{
#if defined(HAVE_VIMAGE)
    vImage_Buffer viSrc = { srcBuf, srcH, srcW, srcRowBytes };  // vImage struct has H before W
    vImage_Buffer viDst = { dstBuf, dstH, dstW, dstRowBytes };

    vImage_Error err = vImageScale_ARGB8888 (&viSrc, &viDst, NULL, kvImageEdgeExtend);
    if (err != kvImageNoError) {
        LXErrorSet(outError, (long)err + 2000, "vImageScale(4i8) failed");
    }

#else
    if (dstW > srcW || dstH > srcH) {
        magnify_bilinear_RGBA_int8(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    } else {
        scale_nearest_RGBA_int8(srcBuf, srcW, srcH, srcRowBytes,
                                   dstBuf, dstW, dstH, dstRowBytes);
    }
#endif
}

void LXImageScale_RGBA_int16(uint16_t * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                             uint16_t * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                             LXError *outError)
{
    // vImage version for this sample depth is not available on Mac, just do nearest-neighbor sampling
    
    LXInteger x, y;
    const float xinc = (float)srcW / (float)dstW;
    
    for (y = 0; y < dstH; y++) {
        float sy = FLOORF( ((float)y / dstH) * (float)srcH );
        
        int * LXRESTRICT srcRow = (int *) ( (uint8_t *)srcBuf + (int)sy * srcRowBytes);
        int * LXRESTRICT dstRow = (int *) ( (uint8_t *)dstBuf + y * dstRowBytes);
        
        for (x = 0; x < dstW; x++) {
            int sx = (int) (xinc * (float)x - 0.5);
            
            dstRow[x*2 + 0] = srcRow[sx*2 + 0];
            dstRow[x*2 + 1] = srcRow[sx*2 + 1];
        }
    }
}


void LXImageScale_RGBA_float32(float * LXRESTRICT srcBuf, LXInteger srcW, LXInteger srcH, size_t srcRowBytes,
                               float * LXRESTRICT dstBuf, LXInteger dstW, LXInteger dstH, size_t dstRowBytes,
                               LXError *outError)
{
#ifdef HAVE_VIMAGE
    /*
    void        *data;  
   uint32_t    height; 
   uint32_t    width;  
   uint32_t    rowBytes;
    */
    vImage_Buffer viSrc, viDst;
    memset(&viSrc, 0, sizeof(vImage_Buffer));
    viSrc.data = srcBuf;
    viSrc.height = srcH;
    viSrc.width = srcW;
    viSrc.rowBytes = srcRowBytes;
    
    memset(&viDst, 0, sizeof(vImage_Buffer));
    viDst.data = dstBuf;
    viDst.height = dstH;
    viDst.width = dstW;
    viDst.rowBytes = dstRowBytes;

    ///printf("%s: %i / %i / %i --- %i / %i / %i\n", __func__, srcW, srcH, srcRowBytes, dstW, dstH, dstRowBytes);

    vImage_Error err = vImageScale_ARGBFFFF (&viSrc, &viDst, NULL, kvImageEdgeExtend);
    if (err != kvImageNoError) {
        LXErrorSet(outError, (long)err + 2000, "vImageScale(4f) failed");
    }

#else
    // ugly and slow nearest-neighbor sampling
    
    LXInteger x, y;
    const float xinc = (float)srcW / (float)dstW;
    
    for (y = 0; y < dstH; y++) {
        float sy = FLOORF( ((float)y / dstH) * (float)srcH );
        
        float * LXRESTRICT srcRow = (float *) ( (uint8_t *)srcBuf + (int)sy * srcRowBytes);
        float * LXRESTRICT dstRow = (float *) ( (uint8_t *)dstBuf + y * dstRowBytes);
        
        for (x = 0; x < dstW; x++) {
            int sx = (int) (xinc * (float)x - 0.5);
            
            dstRow[x] = srcRow[sx];
        }
    }
#endif
}



void LXImageBlend_YCbCr422(uint8_t * LXRESTRICT outBuf,
                           uint8_t * LXRESTRICT buf1,
                           uint8_t * LXRESTRICT buf2,
                           LXInteger w)
{
	LXInteger j;
    uint32_t * LXRESTRICT yuvBuf1 = (uint32_t *)buf1;
    uint32_t * LXRESTRICT yuvBuf2 = (uint32_t *)buf2;
    uint32_t * LXRESTRICT dstYuvBuf = (uint32_t *)outBuf;

    #define CRAM_U8(v)  (uint8_t)(v & 0xff)
	
	for (j = w/2; j; j--) {
		register uint32_t v1 = yuvBuf1[0];
		yuvBuf1++;
		register uint32_t v2 = yuvBuf2[0];
		yuvBuf2++;
			
		register uint32_t iy2_1 = ((v1 >> 24)  & 0xff);
		register uint32_t iCr_1 = ((v1 >> 16)  & 0xff);
		register uint32_t iy1_1 = ((v1 >> 8)   & 0xff);
		register uint32_t iCb_1 = ((v1 >> 0)   & 0xff);

		register uint32_t iy2_2 = ((v2 >> 24)  & 0xff);
		register uint32_t iCr_2 = ((v2 >> 16)  & 0xff);
		register uint32_t iy1_2 = ((v2 >> 8)   & 0xff);
		register uint32_t iCb_2 = ((v2 >> 0)   & 0xff);

		iy1_1 = CRAM_U8( (iy1_1 + iy1_2) >> 1 );
		iy2_1 = CRAM_U8( (iy2_1 + iy2_2) >> 1 );
		iCb_1 = CRAM_U8( (iCb_1 + iCb_2) >> 1 );
		iCr_1 = CRAM_U8( (iCr_1 + iCr_2) >> 1 );
		
		dstYuvBuf[0] = MAKEPX_2VUY(iy1_1, iy2_1, iCb_1, iCr_1);
		dstYuvBuf++;
	}
    
    #undef CRAM_U8
}

void LXImagePremultiply_RGBA_int8_inplace(const LXInteger w, const LXInteger h, 
                                          uint8_t * LXRESTRICT buf, const size_t rowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        uint32_t * LXRESTRICT src = (uint32_t *) (buf + rowBytes * y);
    
        for (x = 0; x < w; x++) {
            uint32_t v = *src;
            // alpha is in range 0-255; for multiplication, convert to fixed point range 0 - 0x100FE (==1.00387, closest equal to 256/255)
            uint32_t a = GETBYTE3(v);
            uint32_t fa = a * ((1 << 8) + 2);
            uint32_t r = (GETBYTE0(v) * fa) >> 16;
            uint32_t g = (GETBYTE1(v) * fa) >> 16;
            uint32_t b = (GETBYTE2(v) * fa) >> 16;
            
            uint32_t ov = 0;
            SETBYTE0(ov, r & 0xff);
            SETBYTE1(ov, g & 0xff);
            SETBYTE2(ov, b & 0xff);
            SETBYTE3(ov, a);
            *src++ = ov;
        }
    }
}

void LXImagePremultiply_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        uint32_t * LXRESTRICT src = (uint32_t *) (srcBuf + srcRowBytes * y);
        uint32_t * LXRESTRICT dst = (uint32_t *) (dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
            uint32_t v = *src++;
            // alpha is in range 0-255; for multiplication, convert to fixed point range 0 - 0x100FE (==1.00387, closest equal to 256/255)
            uint32_t a = GETBYTE3(v);
            uint32_t fa = a * ((1 << 8) + 2);
            uint32_t r = (GETBYTE0(v) * fa) >> 16;
            uint32_t g = (GETBYTE1(v) * fa) >> 16;
            uint32_t b = (GETBYTE2(v) * fa) >> 16;
            
            uint32_t ov = 0;
            SETBYTE0(ov, r & 0xff);
            SETBYTE1(ov, g & 0xff);
            SETBYTE2(ov, b & 0xff);
            SETBYTE3(ov, a);
            *dst++ = ov;
        }
    }
}


#pragma mark --- pixel utils ---

#define COEFF_Y          (1.0f / kLX_Y219_scale)  //0.00456621f

#define R_COEFF_CR_601   (kLX_601_toR__Pr / kLX_C219_scale)  //0.00625893f
#define G_COEFF_CB_601   (kLX_601_toG__Pb / kLX_C219_scale)  //-0.00153632f
#define G_COEFF_CR_601   (kLX_601_toG__Pr / kLX_C219_scale)  //-0.00318811f
#define B_COEFF_CB_601   (kLX_601_toB__Pb / kLX_C219_scale)  //0.00791071f

#define R_COEFF_CR_709   (kLX_709_toR__Pr / kLX_C219_scale)  //0.00625893f
#define G_COEFF_CB_709   (kLX_709_toG__Pb / kLX_C219_scale)  //-0.00153632f
#define G_COEFF_CR_709   (kLX_709_toG__Pr / kLX_C219_scale)  //-0.00318811f
#define B_COEFF_CB_709   (kLX_709_toB__Pb / kLX_C219_scale)  //0.00791071f


LXINLINE LXFUNCATTR_PURE void rgbFromYCbCr_rec601_fv(float rgb[3], float y, float cb, float cr)
{
    //rgb[0] =  0.00456621f * yuv[0]                         +  0.00625893f * yuv[2];
    //rgb[1] =  0.00456621f * yuv[0] + -0.00152632f * yuv[1] + -0.00318811f * yuv[2];
    //rgb[2] =  0.00456621f * yuv[0] +  0.00791071f * yuv[1];
    rgb[0] =  COEFF_Y * y                       + R_COEFF_CR_601 * cr;
    rgb[1] =  COEFF_Y * y + G_COEFF_CB_601 * cb + G_COEFF_CR_601 * cr;
    rgb[2] =  COEFF_Y * y + B_COEFF_CB_601 * cb;
}

LXINLINE LXFUNCATTR_PURE void rgbFromYCbCr_rec709_fv(float rgb[3], float y, float cb, float cr)
{
    //rgb[0] =  0.00456621f * yuv[0]                         +  0.00625893f * yuv[2];
    //rgb[1] =  0.00456621f * yuv[0] + -0.00152632f * yuv[1] + -0.00318811f * yuv[2];
    //rgb[2] =  0.00456621f * yuv[0] +  0.00791071f * yuv[1];
    rgb[0] =  COEFF_Y * y                       + R_COEFF_CR_709 * cr;
    rgb[1] =  COEFF_Y * y + G_COEFF_CB_709 * cb + G_COEFF_CR_709 * cr;
    rgb[2] =  COEFF_Y * y + B_COEFF_CB_709 * cb;
}

LXINLINE LXFUNCATTR_PURE float yFromRGB_rec601_f(const float r, const float g, const float b) {
    return  kLX_Y219_offset + ((kLX_601_toY__R * kLX_Y219_scale) * r + (kLX_601_toY__G * kLX_Y219_scale) * g +  (kLX_601_toY__B * kLX_Y219_scale) * b);
}

LXINLINE LXFUNCATTR_PURE float cbFromRGB_rec601_f(const float r, const float g, const float b) {
    //return  128.0f + (-37.797f * r + -74.203f * g +   112.0f * b);
    return  kLX_C219_offset + ((kLX_601_toPb_R * kLX_C219_scale) * r + (kLX_601_toPb_G * kLX_C219_scale) * g +  (kLX_601_toPb_B * kLX_C219_scale) * b);
}

LXINLINE LXFUNCATTR_PURE float crFromRGB_rec601_f(const float r, const float g, const float b) {
    //return  128.0f + (  112.0f * r + -93.786f * g + -18.214f * b);
    return  kLX_C219_offset + ((kLX_601_toPr_R * kLX_C219_scale) * r + (kLX_601_toPr_G * kLX_C219_scale) * g +  (kLX_601_toPr_B * kLX_C219_scale) * b);
}


LXINLINE LXFUNCATTR_PURE float yFromRGB_rec709_f(const float r, const float g, const float b) {
    return  kLX_Y219_offset + ((kLX_709_toY__R * kLX_Y219_scale) * r + (kLX_709_toY__G * kLX_Y219_scale) * g +  (kLX_709_toY__B * kLX_Y219_scale) * b);
}

LXINLINE LXFUNCATTR_PURE float cbFromRGB_rec709_f(const float r, const float g, const float b) {
    return  kLX_C219_offset + ((kLX_709_toPb_R * kLX_C219_scale) * r + (kLX_709_toPb_G * kLX_C219_scale) * g +  (kLX_709_toPb_B * kLX_C219_scale) * b);
}

LXINLINE LXFUNCATTR_PURE float crFromRGB_rec709_f(const float r, const float g, const float b) {
    return  kLX_C219_offset + ((kLX_709_toPr_R * kLX_C219_scale) * r + (kLX_709_toPr_G * kLX_C219_scale) * g +  (kLX_709_toPr_B * kLX_C219_scale) * b);
}



#pragma mark --- pixel format conversions ---


// luma shouldn't be clamped in range 0-1 because we want to preserve superbrights in Y.
// instead the real max coding value is: (254-16)/219 = 1.08675
#define CLAMP_SAT_F_LUMA(y_)   MAX(0.0f, MIN(1.08675f, y_))

            
void LXPxConvert_RGBA_float16_rawYUV_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger i, j;
    const float yScale = kLX_Y219_scale;
    const float cScale = kLX_C219_scale;
    const float yOff = kLX_Y219_offset;
    const float cOff = kLX_Y219_offset;  // this is the right offset for chroma as well

    const LXInteger n = w/2;
    if (n <= 0) return;

    for (i = 0; i < h; i++) {
        uint32_t * LXRESTRICT yuvBuf = (uint32_t *)((uint8_t *)dstBuf + dstRowBytes * i);
        LXHalf * LXRESTRICT rgbaBuf = (LXHalf *)((uint8_t *)srcBuf + srcRowBytes * i);
        
        for (j = n; j; j--) {
            register uint32_t iy1, iy2, iCb, iCr;
            register float y1, y2, Cb, Cr;
            
            y1 = LXFloatFromHalf(rgbaBuf[0]);
            y2 = LXFloatFromHalf(rgbaBuf[0 + 4]);            
            y1 = CLAMP_SAT_F_LUMA(y1) * yScale + yOff;  // clamp luma in higher range to preserve superbrights
            y2 = CLAMP_SAT_F_LUMA(y2) * yScale + yOff;
            
            Cb = LXFloatFromHalf(rgbaBuf[1]);
            Cr = LXFloatFromHalf(rgbaBuf[2]);
            Cb = CLAMP_SAT_F(Cb) * cScale + cOff;
            Cr = CLAMP_SAT_F(Cr) * cScale + cOff;
            rgbaBuf += 8;
            
            iy1 = ((uint32_t)y1) & 0xff;
            iy2 = ((uint32_t)y2) & 0xff;
            iCb = ((uint32_t)Cb) & 0xff;
            iCr = ((uint32_t)Cr) & 0xff;
            
            yuvBuf[0] = MAKEPX_2VUY(iy1, iy2, iCb, iCr);
            yuvBuf++;
        }
    }
}

#ifdef __BIG_ENDIAN__
 #define PACK_LXHALF_TO_UINT32(h1_, h2_)   (((uint32_t)(h1_) & 0xFFFF) << 16) | (((uint32_t)(h2_) & 0xFFFF))
#else
 #define PACK_LXHALF_TO_UINT32(h1_, h2_)   (((uint32_t)(h2_) & 0xFFFF) << 16) | (((uint32_t)(h1_) & 0xFFFF))
#endif


void LXPxConvert_YCbCr422_to_RGBA_float16_rawYUV(const LXInteger w, const LXInteger h,
                                                uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                                LXHalf * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger i, j;
    const float yScale = 1.0f / kLX_Y219_scale;
    const float cScale = 1.0f / kLX_C219_scale;
    const int yOff = kLX_Y219_offset;
    const int cOff = kLX_Y219_offset;  // this is the right offset for chroma as well
    const LXHalf one = LXHalfFromFloat(1.0f);

    const LXInteger n = w/2;
    if (n <= 0) return;
    
    ///printf("%s: %i * %i, rb %i / %i\n", __func__, w, h, srcRowBytes, dstRowBytes);

    for (i = 0; i < h; i++) {
        uint32_t * LXRESTRICT yuvBuf = (uint32_t *)((uint8_t *)srcBuf + srcRowBytes * i);
        uint32_t * LXRESTRICT halfBuf = (uint32_t *)((uint8_t *)dstBuf + dstRowBytes * i);
        
        for (j = n; j; j--) {
            uint32_t v = *yuvBuf;
            yuvBuf++;
            int iCb = (int)GETBYTE0(v) - cOff;
            int iY0 = (int)GETBYTE1(v) - yOff;
            int iCr = (int)GETBYTE2(v) - cOff;
            int iY1 = (int)GETBYTE3(v) - yOff;
            
            float cb = (float)iCb * cScale;
            float cr = (float)iCr * cScale;            
            uint32_t hCb = LXHalfFromFloat(cb);
            uint32_t hCr = LXHalfFromFloat(cr);
            float y0 = (float)iY0 * yScale;
            float y1 = (float)iY1 * yScale;
            uint32_t hY0 = LXHalfFromFloat(y0);
            uint32_t hY1 = LXHalfFromFloat(y1);
            
            uint32_t packedHCrAndOne = PACK_LXHALF_TO_UINT32(hCr, one);
            
            // instead of writing out in slow 16-bit chunks, pack two half floats into uint32 here
            halfBuf[0] = PACK_LXHALF_TO_UINT32(hY0, hCb);
            halfBuf[1] = packedHCrAndOne;
            halfBuf[2] = PACK_LXHALF_TO_UINT32(hY1, hCb);
            halfBuf[3] = packedHCrAndOne;
            halfBuf += 4;
            
            /*rgbaBuf[0] = LXHalfFromFloat(y0);
            rgbaBuf[1] = hCb;
            rgbaBuf[2] = hCr;
            rgbaBuf[3] = one;
            rgbaBuf[4] = LXHalfFromFloat(y1);
            rgbaBuf[5] = hCb;
            rgbaBuf[6] = hCr;
            rgbaBuf[7] = one;
            rgbaBuf += 8; */
        }
    }
}



void LXPxConvert_RGBA_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                         LXColorSpaceEncoding cspace)
{
    LXInteger i, j;
    const float toFloatMult = (1.0f / 255.0f);
    const float mat11 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__R : kLX_601_toY__R) * kLX_Y219_scale;
    const float mat12 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__G : kLX_601_toY__G) * kLX_Y219_scale;
    const float mat13 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__B : kLX_601_toY__B) * kLX_Y219_scale;
    const float mat21 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_R : kLX_601_toPb_R) * kLX_C219_scale;
    const float mat22 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_G : kLX_601_toPb_G) * kLX_C219_scale;
    const float mat23 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_B : kLX_601_toPb_B) * kLX_C219_scale;
    const float mat31 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_R : kLX_601_toPr_R) * kLX_C219_scale;
    const float mat32 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_G : kLX_601_toPr_G) * kLX_C219_scale;
    const float mat33 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_B : kLX_601_toPr_B) * kLX_C219_scale;
    
    for (i = 0; i < h; i++) {
        uint32_t * LXRESTRICT yuvBuf = (uint32_t *)(dstBuf + dstRowBytes * i);
        uint32_t * LXRESTRICT rgbaBuf = (uint32_t *)(srcBuf + srcRowBytes * i);
        for (j = w/2; j; j--) {
            register uint32_t iy1, iy2, iCb, iCr;
            register float y1, y2, Cb, Cr;
            register float r1, r2, g1, g2, b1, b2;
            
            r1 = (GETBYTE0(rgbaBuf[0])) * toFloatMult;
            r2 = (GETBYTE0(rgbaBuf[1])) * toFloatMult;
            g1 = (GETBYTE1(rgbaBuf[0])) * toFloatMult;
            g2 = (GETBYTE1(rgbaBuf[1])) * toFloatMult;
            b1 = (GETBYTE2(rgbaBuf[0])) * toFloatMult;
            b2 = (GETBYTE2(rgbaBuf[1])) * toFloatMult;
            rgbaBuf += 2;
            
            y1 = (float)kLX_Y219_offset + mat11*r1 + mat12*g1 + mat13*b1;
            iy1 = ((uint32_t)y1) & 0xff;

            //y2 = yFromRGB_rec601_f(r2, g2, b2);            
            y2 = (float)kLX_Y219_offset + mat11*r2 + mat12*g2 + mat13*b2;
            iy2 = ((uint32_t)y2) & 0xff;

            //Cb = cbFromRGB_rec601_f(r1, g1, b1);
            Cb = (float)kLX_C219_offset + mat21*r1 + mat22*g1 + mat23*b1;
            iCb = ((uint32_t)Cb) & 0xff;

            //Cr = crFromRGB_rec601_f(r1, g1, b1);
            Cr = (float)kLX_C219_offset + mat31*r1 + mat32*g1 + mat33*b1;
            iCr = ((uint32_t)Cr) & 0xff;
            
            yuvBuf[0] = MAKEPX_2VUY(iy1, iy2, iCb, iCr);
            yuvBuf++;
        }
    }
}

void LXPxConvert_ARGB_to_YCbCr422(const LXInteger w, const LXInteger h,
                                         uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                         uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                         LXColorSpaceEncoding cspace)
{
    LXInteger i, j;
    const float toFloatMult = (1.0f / 255.0f);
    const float mat11 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__R : kLX_601_toY__R) * kLX_Y219_scale;
    const float mat12 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__G : kLX_601_toY__G) * kLX_Y219_scale;
    const float mat13 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toY__B : kLX_601_toY__B) * kLX_Y219_scale;
    const float mat21 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_R : kLX_601_toPb_R) * kLX_C219_scale;
    const float mat22 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_G : kLX_601_toPb_G) * kLX_C219_scale;
    const float mat23 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPb_B : kLX_601_toPb_B) * kLX_C219_scale;
    const float mat31 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_R : kLX_601_toPr_R) * kLX_C219_scale;
    const float mat32 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_G : kLX_601_toPr_G) * kLX_C219_scale;
    const float mat33 = ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toPr_B : kLX_601_toPr_B) * kLX_C219_scale;

    for (i = 0; i < h; i++) {
        uint32_t * LXRESTRICT yuvBuf = (uint32_t *)(dstBuf + dstRowBytes * i);
        uint32_t * LXRESTRICT rgbaBuf = (uint32_t *)(srcBuf + srcRowBytes * i);
        for (j = w/2; j; j--) {
            register uint32_t iy1, iy2, iCb, iCr;
            register float y1, y2, Cb, Cr;
            register float r1, r2, g1, g2, b1, b2;
            
            r1 = (GETBYTE1(rgbaBuf[0])) * toFloatMult;
            r2 = (GETBYTE1(rgbaBuf[1])) * toFloatMult;
            g1 = (GETBYTE2(rgbaBuf[0])) * toFloatMult;
            g2 = (GETBYTE2(rgbaBuf[1])) * toFloatMult;
            b1 = (GETBYTE3(rgbaBuf[0])) * toFloatMult;
            b2 = (GETBYTE3(rgbaBuf[1])) * toFloatMult;
            rgbaBuf += 2;
            
            y1 = (float)kLX_Y219_offset + mat11*r1 + mat12*g1 + mat13*b1;
            iy1 = ((uint32_t)y1) & 0xff;
            y2 = (float)kLX_Y219_offset + mat11*r2 + mat12*g2 + mat13*b2;
            iy2 = ((uint32_t)y2) & 0xff;
            Cb = (float)kLX_C219_offset + mat21*r1 + mat22*g1 + mat23*b1;
            iCb = ((uint32_t)Cb) & 0xff;
            Cr = (float)kLX_C219_offset + mat31*r1 + mat32*g1 + mat33*b1;
            iCr = ((uint32_t)Cr) & 0xff;
            
            yuvBuf[0] = MAKEPX_2VUY(iy1, iy2, iCb, iCr);
            yuvBuf++;
        }
    }
}

#if defined(LXPLATFORM_MAC) && !defined(__BIG_ENDIAN__)
#define USE_SSE2_FOR_YUV __SSE2__
#else
#define USE_SSE2_FOR_YUV 0
#endif


void LXPxConvert_YCbCr422_to_RGBA_int8(const LXInteger w, const LXInteger h,
                                              uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                              uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                              LXColorSpaceEncoding cspace)
{
    LXInteger stride = w / 2;
    const LXFixedNum fxd_lumaMul = FIXD_FROM_FLOAT(255.0 / kLX_Y219_scale);  // 1.164
    const LXFixedNum fxd_crMul_r = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toR__Pr : kLX_601_toR__Pr));  // 1.596
    const LXFixedNum fxd_crMul_g = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pr : kLX_601_toG__Pr));  // 0.813
    const LXFixedNum fxd_cbMul_g = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pb : kLX_601_toG__Pb));  // 0.391
    const LXFixedNum fxd_cbMul_b = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toB__Pb : kLX_601_toB__Pb));  // 2.018

#if (USE_SSE2_FOR_YUV)
    const LXBool useVec = ((srcRowBytes & 15) == 0 && (dstRowBytes & 15) == 0);
    const LXUInteger vecN = (useVec) ? (w / 8) : 0;
    float *tempBuf = (useVec) ? _lx_malloc(w * 4 * sizeof(float) + w * 2 * sizeof(float)) : NULL;
    float *tempBuf1 = (useVec) ? tempBuf + w*4 : NULL;
    float *tempBuf2 = (useVec) ? tempBuf : NULL;
    
    const float lumaMul = 255.0 / kLX_Y219_scale;
    const float crMul_r = (255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toR__Pr : kLX_601_toR__Pr);
    const float crMul_g = (255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pr : kLX_601_toG__Pr);
    const float cbMul_g = (255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pb : kLX_601_toG__Pb);
    const float cbMul_b = (255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toB__Pb : kLX_601_toB__Pb);
    
    const vfloat_t v_bias_2vuy = { -128.0, -16.0, -128.0, -16.0 };
#endif

    LXInteger y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)(srcBuf + srcRowBytes * y);
        uint8_t * LXRESTRICT dst = (uint8_t *)(dstBuf + dstRowBytes * y);
        LXInteger n;

#if (USE_SSE2_FOR_YUV)
        LXPxConvert_int8_to_float32_withRange_SSE2(src, tempBuf1, vecN * 16,  0, 1);
        
        float * LXRESTRICT tempSrc = tempBuf1;
        float * LXRESTRICT tempDst = tempBuf2;
        for (n = 0; n < vecN * 4; n++) {
            // TODO: should properly vectorize this algorithm (i.e. load a vector of 8 pixels straight from the source, instead of this temp buffer stuff)
            LXVectorFloat v0;
            v0.v = _mm_load_ps(tempSrc);
            tempSrc += 4;
            
            v0.v = _mm_add_ps(v0.v, v_bias_2vuy);
            float cb = v0.e[0];
            float y0 = v0.e[1];
            float cr = v0.e[2];
            float y1 = v0.e[3];
            
            float sc_R = (crMul_r * cr);
            float sc_G = (crMul_g * cr) + (cbMul_g * cb);
            float sc_B = (cbMul_b * cb);
            
            float sc_y = lumaMul * y0;
            v0.e[0] = sc_y + sc_R;
            v0.e[1] = sc_y + sc_G;
            v0.e[2] = sc_y + sc_B;
            v0.e[3] = 255.0f;
            _mm_store_ps(tempDst, v0.v);
            
            sc_y = lumaMul * y1;
            v0.e[0] = sc_y + sc_R;
            v0.e[1] = sc_y + sc_G;
            v0.e[2] = sc_y + sc_B;
            _mm_store_ps(tempDst + 4, v0.v);
            tempDst += 8;
        }
        
        LXPxConvert_float32_to_int32_withRange_inplace_SSE2(tempBuf2, vecN * 16*2,  0, 1);
        LXPxConvert_int32_to_int8_withSaturate_SSE2((int32_t *)tempBuf2, dst, vecN * 16*2);
        
        stride = (w - (vecN * 8)) / 2;
        src += vecN * 16;
        dst += vecN * 16 * 2;
#endif
        
        for (n = 0; n < stride; n++) {
            uint32_t v = *((uint32_t *)src);
            int cb = GETBYTE0(v);
            int y0 = GETBYTE1(v);
            int cr = GETBYTE2(v);
            int y1 = GETBYTE3(v);

            /*  R = 1.164(Y-16) + 1.596(Cr-128)
                G = 1.164(Y-16) - 0.813(Cr-128) - 0.391(Cb-128)
                B = 1.164(Y-16) + 2.018(Cb-128)
            */
            cr = FIXD_FROM_INT(cr - 128);
            cb = FIXD_FROM_INT(cb - 128);
            y0 = FIXD_FROM_INT(y0 - 16);
            y1 = FIXD_FROM_INT(y1 - 16);
            
            int scaled_cr_r = FIXD_MUL(fxd_crMul_r, cr);
            int scaled_cr_g = FIXD_MUL(fxd_crMul_g, cr);
            int scaled_cb_g = FIXD_MUL(fxd_cbMul_g, cb);
            int scaled_cb_b = FIXD_MUL(fxd_cbMul_b, cb);
            
            int scaled_y = FIXD_MUL(fxd_lumaMul, y0);
            int r = scaled_y + scaled_cr_r;
            int g = scaled_y + scaled_cr_g + scaled_cb_g;
            int b = scaled_y + scaled_cb_b;
            
            v = 0;
            SETBYTE0(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(r)));
            SETBYTE1(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(g)));
            SETBYTE2(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(b)));
            SETBYTE3(v, 255);            
            ((uint32_t *)dst)[0] = v;
            
            scaled_y = FIXD_MUL(fxd_lumaMul, y1);
            r = scaled_y + scaled_cr_r;
            g = scaled_y + scaled_cr_g + scaled_cb_g;
            b = scaled_y + scaled_cb_b;

            v = 0;
            SETBYTE0(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(r)));
            SETBYTE1(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(g)));
            SETBYTE2(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(b)));
            SETBYTE3(v, 255);            
            ((uint32_t *)dst)[1] = v;
            
            src += 4;
            dst += 8;
        }
    }
   
#if (USE_SSE2_FOR_YUV)
    _lx_free(tempBuf);
#endif
}

void LXPxConvert_YCbCr422_YUY2_to_RGBA_int8(const LXInteger w, const LXInteger h,
                                                   uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                                   uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                                   LXColorSpaceEncoding cspace)
{

    const LXInteger stride = w / 2;
    const LXFixedNum fxd_lumaMul = FIXD_FROM_FLOAT(255.0 / kLX_Y219_scale);  // 1.164
    const LXFixedNum fxd_crMul_r = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toR__Pr : kLX_601_toR__Pr));  // 1.596
    const LXFixedNum fxd_crMul_g = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pr : kLX_601_toG__Pr));  // 0.813
    const LXFixedNum fxd_cbMul_g = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toG__Pb : kLX_601_toG__Pb));  // 0.391
    const LXFixedNum fxd_cbMul_b = FIXD_FROM_FLOAT((255.0 / kLX_C219_scale) * ((cspace == kLX_YCbCr_Rec709) ? kLX_709_toB__Pb : kLX_601_toB__Pb));  // 2.018

    LXInteger y;
    for (y = 0; y < h; y++) {
        uint32_t * LXRESTRICT src = (uint32_t *)(srcBuf + srcRowBytes * y);
        uint32_t * LXRESTRICT dst = (uint32_t *)(dstBuf + dstRowBytes * y);
        LXInteger n;
        for (n = 0; n < stride; n++) {
            uint32_t v = *src;
            int y0 = GETBYTE0(v);
            int cb = GETBYTE1(v);
            int y1 = GETBYTE2(v);
            int cr = GETBYTE3(v);

            cr = FIXD_FROM_INT(cr - 128);
            cb = FIXD_FROM_INT(cb - 128);
            y0 = FIXD_FROM_INT(y0 - 16);
            y1 = FIXD_FROM_INT(y1 - 16);
            
            int scaled_cr_r = FIXD_MUL(fxd_crMul_r, cr);
            int scaled_cr_g = FIXD_MUL(fxd_crMul_g, cr);
            int scaled_cb_g = FIXD_MUL(fxd_cbMul_g, cb);
            int scaled_cb_b = FIXD_MUL(fxd_cbMul_b, cb);
            
            int scaled_y = FIXD_MUL(fxd_lumaMul, y0);
            int r = scaled_y + scaled_cr_r;
            int g = scaled_y + scaled_cr_g + scaled_cb_g;
            int b = scaled_y + scaled_cb_b;
            
            v = 0;
            SETBYTE0(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(r)));
            SETBYTE1(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(g)));
            SETBYTE2(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(b)));
            SETBYTE3(v, 255);            
            dst[0] = v;
            
            scaled_y = FIXD_MUL(fxd_lumaMul, y1);
            r = scaled_y + scaled_cr_r;
            g = scaled_y + scaled_cr_g + scaled_cb_g;
            b = scaled_y + scaled_cb_b;

            v = 0;
            SETBYTE0(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(r)));
            SETBYTE1(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(g)));
            SETBYTE2(v, (uint32_t) FIXD_TO_INT(FIXD_CLAMP_255(b)));
            SETBYTE3(v, 255);            
            dst[1] = v;
            
            src++;
            dst += 2;
        }
    }
}


// --- RGBA 4-unit conversions ---

void LXPxConvert_RGBA_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 4;
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
    
        for (x = 0; x < vecLeft; x++) {
            vuint32_t v = _mm_load_si128((__m128i *)src);
            v = _mm_or_si128(_mm_srli_epi32(v, 24), _mm_slli_epi32(v, 8));            
            _mm_store_si128((__m128i *)dst, v);
            dst += 16;
            src += 16;
        }
        pxLeft = w - (vecLeft * 4);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[1] = src[0];
            dst[2] = src[1];
            dst[3] = src[2];
            dst[0] = src[3];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_ARGB_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                   uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 4;
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint32_t v = _mm_load_si128((__m128i *)src);
            v = _mm_or_si128(_mm_srli_epi32(v, 8), _mm_slli_epi32(v, 24));
            _mm_store_si128((__m128i *)dst, v);
            dst += 16;
            src += 16;
        }
        pxLeft = w - (vecLeft * 4);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_int8(const LXInteger w, const LXInteger h, 
                                   uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(__SSE2__)
    const LXUInteger vecN = w / 4;
    const vuint32_t vmask1 = { 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };
    const vuint32_t vmask2 = { 0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00 };
    const vuint32_t vmask3 = { 0x00ff0000, 0x00ff0000, 0x00ff0000, 0x00ff0000 };
    const vuint32_t vmask4 = { 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint32_t v = _mm_load_si128((__m128i *)src);
            vuint32_t v1 = _mm_and_si128(_mm_srli_epi32(v, 24), vmask1);
            vuint32_t v2 = _mm_and_si128(_mm_srli_epi32(v, 8), vmask2);
            vuint32_t v3 = _mm_and_si128(_mm_slli_epi32(v, 8), vmask3);
            vuint32_t v4 = _mm_and_si128(_mm_slli_epi32(v, 24), vmask4);
            
            v = _mm_or_si128(v1, _mm_or_si128(v2, _mm_or_si128(v3, v4)));
            _mm_store_si128((__m128i *)dst, v);
            dst += 16;
            src += 16;
        }
        pxLeft = w - (vecLeft * 4);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_BGRA_int8(const LXInteger w, const LXInteger h, 
                                        uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                        uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(__SSE2__)
    const LXUInteger vecN = w / 4;
    const vuint32_t vmask1 =   { 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };
    const vuint32_t vmask3 =   { 0x00ff0000, 0x00ff0000, 0x00ff0000, 0x00ff0000 };
    const vuint32_t vmask2_4 = { 0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00 };
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
    
        for (x = 0; x < vecLeft; x++) {
            vuint32_t v = _mm_load_si128((__m128i *)src);
            vuint32_t v1 = _mm_and_si128(_mm_srli_epi32(v, 16), vmask1);
            vuint32_t v3 = _mm_and_si128(_mm_slli_epi32(v, 16), vmask3);
            
            v = _mm_or_si128(v1, _mm_or_si128(v3, _mm_and_si128(v, vmask2_4)));
            _mm_store_si128((__m128i *)dst, v);
            dst += 16;
            src += 16;
        }
        pxLeft = w - (vecLeft * 4);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_ARGB_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 2;
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint16_t * LXRESTRICT src = (uint16_t *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint16_t v = _mm_load_si128((__m128i *)src);
            v = _mm_shufflelo_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 0, 1, 2));
            v = _mm_shufflehi_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 0, 1, 2));
            _mm_store_si128((__m128i *)dst, v);
            dst += 8;
            src += 8;
        }
        pxLeft = w - (vecLeft * 2);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[1] = src[0];
            dst[2] = src[1];
            dst[3] = src[2];
            dst[0] = src[3];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_ARGB_to_RGBA_int16(const LXInteger w, const LXInteger h, 
                                   uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 2;
#endif

    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint16_t * LXRESTRICT src = (uint16_t *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint16_t v = _mm_load_si128((__m128i *)src);
            v = _mm_shufflelo_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(1, 2, 3, 0));
            v = _mm_shufflehi_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(1, 2, 3, 0));
            _mm_store_si128((__m128i *)dst, v);
            dst += 8;
            src += 8;
        }
        pxLeft = w - (vecLeft * 2);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_int16(const LXInteger w, const LXInteger h, 
                                   uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 2;
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint16_t * LXRESTRICT src = (uint16_t *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint16_t v = _mm_load_si128((__m128i *)src);
            v = _mm_shufflelo_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 2, 1, 0));
            v = _mm_shufflehi_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 2, 1, 0));
            _mm_store_si128((__m128i *)dst, v);
            dst += 8;
            src += 8;
        }
        pxLeft = w - (vecLeft * 2);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_BGRA_int16(const LXInteger w, const LXInteger h, 
                                        uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                        uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
#if defined(LXVEC)
    const LXUInteger vecN = w / 2;
#endif
    
    LXUInteger x, y;
    for (y = 0; y < h; y++) {
        uint16_t * LXRESTRICT src = (uint16_t *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        LXUInteger pxLeft = w;
        
    #if defined(__SSE2__)
        LXUInteger vecLeft = (ISALIGNED(src) && ISALIGNED(dst)) ? vecN : 0;
        
        for (x = 0; x < vecLeft; x++) {
            vuint16_t v = _mm_load_si128((__m128i *)src);
            v = _mm_shufflelo_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(2, 1, 0, 3));
            v = _mm_shufflehi_epi16(v, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(2, 1, 0, 3));
            _mm_store_si128((__m128i *)dst, v);
            dst += 8;
            src += 8;
        }
        pxLeft = w - (vecLeft * 2);
    #endif
        
        for (x = 0; x < pxLeft; x++) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_ARGB_float32(const LXInteger w, const LXInteger h, 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  float * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        float * LXRESTRICT dst = (float *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
        #if defined(__SSE2__)
            vfloat_t vf = _mm_load_ps(src);
            vf = _mm_shuffle_ps(vf, vf, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 0, 1, 2));
            _mm_store_ps(dst, vf);
        #else
            dst[1] = src[0];
            dst[2] = src[1];
            dst[3] = src[2];
            dst[0] = src[3];
        #endif
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_ARGB_to_RGBA_float32(const LXInteger w, const LXInteger h, 
                                   float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   float * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        float * LXRESTRICT dst = (float *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
        #if defined(__SSE2__)
            vfloat_t vf = _mm_load_ps(src);
            vf = _mm_shuffle_ps(vf, vf, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(1, 2, 3, 0));
            _mm_store_ps(dst, vf);
        #else
            dst[0] = src[1];
            dst[1] = src[2];
            dst[2] = src[3];
            dst[3] = src[0];
        #endif
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_float32(const LXInteger w, const LXInteger h, 
                                        float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                        float * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        float * LXRESTRICT dst = (float *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
        #if defined(__SSE2__)
            vfloat_t vf = _mm_load_ps(src);
            vf = _mm_shuffle_ps(vf, vf, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(3, 2, 1, 0));
            _mm_store_ps(dst, vf);
        #else
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        #endif
            dst += 4;
            src += 4;
        }
    }
}

void LXPxConvert_RGBA_to_reverse_BGRA_float32(const LXInteger w, const LXInteger h, 
                                            float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                            float * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        float * LXRESTRICT dst = (float *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
        #if defined(__SSE2__)
            vfloat_t vf = _mm_load_ps(src);
            vf = _mm_shuffle_ps(vf, vf, _LX_MM_SHUFFLE_BIGENDIAN_STYLE(2, 1, 0, 3));
            _mm_store_ps(dst, vf);
        #else
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
        #endif
            dst += 4;
            src += 4;
        }
    }
}


// --- rowbyte swizzlers ---

void LXPxCopy_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    const size_t realRowBytes = w * 4;
    LXUInteger y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        memcpy(dst, src, realRowBytes);
    }
}

void LXPxCopy_RGBA_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    const size_t realRowBytes = w * 4 * sizeof(uint16_t);
    LXUInteger y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        memcpy(dst, src, realRowBytes);
    }
}

void LXPxCopy_RGBA_float32(const LXInteger w, const LXInteger h, 
                                   float * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                   float * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    const size_t realRowBytes = w * 4 * sizeof(float);
    LXUInteger y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = (uint8_t *)srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = (uint8_t *)dstBuf + dstRowBytes * y;
        memcpy(dst, src, realRowBytes);
    }
}


// --- non-4-unit conversions ---

void LXPxConvert_RGB_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;

    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            dst[1] = src[0];
            dst[2] = src[1];
            dst[3] = src[2];
            dst[0] = 255;
            dst += 4;
            src += srcByteStride;
        }
    }
}

void LXPxConvert_RGB_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;

    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            dst += 4;
            src += srcByteStride;
        }
    }
}

void LXPxConvert_RGB_to_RGBA_int16(const LXInteger w, const LXInteger h, 
                                  uint16_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                  uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const uint16_t dstCodingWhite)
{
    LXInteger x, y;

    for (y = 0; y < h; y++) {
        uint16_t * LXRESTRICT src = (uint16_t *)((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *)((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = dstCodingWhite;
            dst += 4;
            src += srcValueStride;
        }
    }
}


void LXPxConvert_RGBA_to_RGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const LXInteger dstByteStride)
{
    LXInteger x, y;

    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst += dstByteStride;
            src += srcByteStride;
        }
    }
}

void LXPxConvert_lum_to_ARGB_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            uint8_t s = src[0];
            dst[1] = s;
            dst[2] = s;
            dst[3] = s;
            dst[0] = 255;
            dst += 4;
            src += srcByteStride;
        }
    }
}

void LXPxConvert_lum_to_RGBA_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcByteStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes)
{
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        uint8_t * LXRESTRICT src = srcBuf + srcRowBytes * y;
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            uint8_t s = src[0];
            dst[0] = s;
            dst[1] = s;
            dst[2] = s;
            dst[3] = 255;
            dst += 4;
            src += srcByteStride;
        }
    }
}

void LXPxConvert_RGBA_to_monochrome_lum_int8(const LXInteger w, const LXInteger h, 
                                  uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes, const LXInteger dstByteStride,
                                  const float rw, const float gw, const float bw)
{
    const LXFixedNum fixedRw = FIXD_FROM_FLOAT(rw);
    const LXFixedNum fixedGw = FIXD_FROM_FLOAT(gw);
    const LXFixedNum fixedBw = FIXD_FROM_FLOAT(bw);
    
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        uint32_t * LXRESTRICT src = (uint32_t *)(srcBuf + srcRowBytes * y);
        uint8_t * LXRESTRICT dst = dstBuf + dstRowBytes * y;
        
        for (x = 0; x < w; x++) {
            uint32_t v = src[0];
            src++;
            LXFixedNum r = FIXD_FROM_INT(GETBYTE0(v));
            LXFixedNum g = FIXD_FROM_INT(GETBYTE1(v));
            LXFixedNum b = FIXD_FROM_INT(GETBYTE2(v));
            LXFixedNum mf = FIXD_MUL(r, fixedRw) + FIXD_MUL(g, fixedGw) + FIXD_MUL(b, fixedBw);
            int32_t m = FIXD_TO_INT(mf);
            
            dst[0] = MIN(255, m);
            dst += dstByteStride;
        }
    }
}


void LXPxConvert_int8_to_float32(uint8_t * LXRESTRICT srcBuf, float * LXRESTRICT dstBuf, const size_t count)
{
    const float mul = 1.0 / 255.0;
    LXUInteger i;    

#if defined(LXVEC)
    const size_t unrolledCount = (count / 16) * 4;

    // first expand 8-bit -> 32-bit ints in the output buffer
    {
    uint32_t * LXRESTRICT ubuf = (uint32_t *)srcBuf;
    int32_t * LXRESTRICT intbuf = (int32_t *)dstBuf;    
    for (i = 0; i < unrolledCount; i++) {
        uint32_t u = *ubuf++;
        intbuf[0] = GETBYTE0(u);
        intbuf[1] = GETBYTE1(u);
        intbuf[2] = GETBYTE2(u);
        intbuf[3] = GETBYTE3(u);
        intbuf += 4;
    }
    }
    // then do int->float conversion in-place
    LXPxConvert_int32_to_float32_withRange_inplace_SSE2((int32_t *)dstBuf, unrolledCount*4,  0, 255);
    dstBuf += unrolledCount * 4;
    
#else
    const size_t unrolledCount = count / 4;
    {
    uint32_t * LXRESTRICT ubuf = (uint32_t *)srcBuf;
    for (i = 0; i < unrolledCount; i++) {
        uint32_t u = *ubuf++;
        dstBuf[0] = (float)GETBYTE0(u) * mul;
        dstBuf[1] = (float)GETBYTE1(u) * mul;
        dstBuf[2] = (float)GETBYTE2(u) * mul;
        dstBuf[3] = (float)GETBYTE3(u) * mul;
        dstBuf += 4;
    }
    }
#endif
    
    const size_t leftCount = count - unrolledCount*4;
    if (leftCount > 0) {
        uint8_t * LXRESTRICT leftBuf = srcBuf + unrolledCount*4;
    
        for (i = 0; i < leftCount; i++) {
            dstBuf[0] = (float)(leftBuf[0]) * mul;
            leftBuf++;
            dstBuf++;
        }
    }
}

void LXPxConvert_int8_to_float16(uint8_t * LXRESTRICT srcBuf, LXHalf * LXRESTRICT dstBuf, const size_t count)
{
    if (count < 1) return;
    size_t leftCount = count;
    const float mul = 1.0 / 255.0;
    LXUInteger i;

#if defined(LXVEC)
    if (count >= 16) {
        // first expand 8-bit -> 32-bit ints into a temp buffer
        int32_t *tempBuf = _lx_malloc(count * sizeof(int32_t));
        
        const size_t unrolledCount = count / 4;
        {
        uint32_t * LXRESTRICT ubuf = (uint32_t *)srcBuf;
        int32_t * LXRESTRICT int32Buf = tempBuf;
        for (i = 0; i < unrolledCount; i++) {
            uint32_t u = *ubuf++;
            int32Buf[0] = GETBYTE0(u);
            int32Buf[1] = GETBYTE1(u);
            int32Buf[2] = GETBYTE2(u);
            int32Buf[3] = GETBYTE3(u);
            int32Buf += 4;
        }
        }
        
        // then do int->float conversion in-place
        LXPxConvert_int32_to_float32_withRange_inplace_SSE2(tempBuf, unrolledCount*4,  0, 255);
        
        // finally convert float32 -> float16
        LXConvertFloatToHalfArray((float *)tempBuf, dstBuf, unrolledCount*4);
        
        _lx_free(tempBuf);
        
        srcBuf += unrolledCount * 4;
        dstBuf += unrolledCount * 4;
        leftCount = count - unrolledCount * 4;
    }

#else
    const size_t unrolledCount = count / 4;
    {
    uint32_t * LXRESTRICT ubuf = (uint32_t *)srcBuf;
    
    for (i = 0; i < unrolledCount; i++) {
        uint32_t u = *ubuf++;
        float f0 = (float)GETBYTE0(u) * mul;
        float f1 = (float)GETBYTE1(u) * mul;
        float f2 = (float)GETBYTE2(u) * mul;
        float f3 = (float)GETBYTE3(u) * mul;
        dstBuf[0] = LXHalfFromFloat(f0);
        dstBuf[1] = LXHalfFromFloat(f1);
        dstBuf[2] = LXHalfFromFloat(f2);
        dstBuf[3] = LXHalfFromFloat(f3);
        dstBuf += 4;
    }
    }
    srcBuf += unrolledCount * 4;
    leftCount = count - unrolledCount * 4;
#endif
    
    if (leftCount > 0) {
        uint8_t * LXRESTRICT leftBuf = srcBuf;
    
        for (i = 0; i < leftCount; i++) {
            float f0 = (float)(leftBuf[0]) * mul;
            dstBuf[0] = LXHalfFromFloat(f0);
            leftBuf++;
            dstBuf++;
        }
    }
}

void LXPxConvert_int16_to_float16(uint16_t * LXRESTRICT srcBuf, LXHalf * LXRESTRICT dstBuf, const size_t count, const uint16_t srcCodingWhite)
{
    if (count < 1) return;
    size_t leftCount = count;
    const float mul = 1.0 / srcCodingWhite;
    LXUInteger i;

#if defined(LXVEC)
    if (count >= 16 && ((LXUInteger)srcBuf & 15) == 0 && ((LXUInteger)dstBuf & 15) == 0) {
        // first expand 16-bit -> 32-bit ints into a temp buffer
        int32_t *tempBuf = _lx_malloc(count * sizeof(int32_t));
        
        const size_t unrolledCount = count / 4;
        {
        uint16_t * LXRESTRICT int16Buf = srcBuf;
        int32_t * LXRESTRICT int32Buf = tempBuf;
        for (i = 0; i < unrolledCount; i++) {
            int32Buf[0] = int16Buf[0];
            int32Buf[1] = int16Buf[1];
            int32Buf[2] = int16Buf[2];
            int32Buf[3] = int16Buf[3];
            int16Buf += 4;
            int32Buf += 4;
        }
        }
        
        // then do int->float conversion in-place
        LXPxConvert_int32_to_float32_withRange_inplace_SSE2(tempBuf, unrolledCount*4,  0, srcCodingWhite);
        
        // finally convert float32 -> float16
        LXConvertFloatToHalfArray((float *)tempBuf, dstBuf, unrolledCount*4);
        
        _lx_free(tempBuf);
        
        srcBuf += unrolledCount * 4;
        dstBuf += unrolledCount * 4;
        leftCount = count - unrolledCount * 4;
    }
#endif
    
    for (i = 0; i < leftCount; i++) {
        float f0 = (float)(srcBuf[0]) * mul;
        dstBuf[0] = LXHalfFromFloat(f0);
        srcBuf++;
        dstBuf++;
    }
}


void LXPxConvert_float32_to_int8(float * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count)
{
    const float mul = 255.0;
    LXUInteger leftCount = count;
    LXInteger i;
    
#if defined(LXVEC)
    if (count >= 16) {
        const size_t unrolledCount = count / 16;
        int32_t *tempBuf = _lx_malloc(count * sizeof(int32_t));
    
        memcpy(srcBuf, tempBuf, unrolledCount*16*sizeof(int32_t));

        LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2((float *)tempBuf, unrolledCount*16,  255);
        
        LXPxConvert_int32_to_int8_withSaturate_SSE2(tempBuf, dstBuf, unrolledCount*16);
        
        _lx_free(tempBuf);
        
        srcBuf += unrolledCount*16;
        dstBuf += unrolledCount*16;
        leftCount = count - unrolledCount*16;
    }
#else
    const size_t unrolledCount = count / 4;
    {
    uint32_t * LXRESTRICT ubuf = (uint32_t *)dstBuf;
    
    for (i = 0; i < unrolledCount; i++) {
        uint32_t u = 0;
        float s0 = CLAMP_SAT_F(srcBuf[0]) * mul;
        float s1 = CLAMP_SAT_F(srcBuf[1]) * mul;
        float s2 = CLAMP_SAT_F(srcBuf[2]) * mul;
        float s3 = CLAMP_SAT_F(srcBuf[3]) * mul;
        srcBuf += 4; 
    
        SETBYTE0(u, (uint32_t)s0);
        SETBYTE1(u, (uint32_t)s1);
        SETBYTE2(u, (uint32_t)s2);
        SETBYTE3(u, (uint32_t)s3);
        *ubuf = u;
        ubuf++;
    }
    }
    dstBuf += unrolledCount*4;
    leftCount = count - unrolledCount*4;
#endif
    
    for (i = 0; i < leftCount; i++) {
        float s = CLAMP_SAT_F(srcBuf[0]) * mul;
        srcBuf++;
        dstBuf[0] = (uint8_t)s;
        dstBuf++;
    }
}

void LXPxConvert_float16_to_int8(LXHalf * LXRESTRICT srcBuf, uint8_t * LXRESTRICT dstBuf, const size_t count)
{
    if (count < 1) return;
    const float mul = 255.0;
    LXUInteger leftCount = count;
    LXUInteger i;

#if defined(LXVEC)
    if (count >= 16) {
        const size_t unrolledCount = count / 16;
        int32_t *tempBuf = _lx_malloc(count * sizeof(int32_t));
    
        LXConvertHalfToFloatArray(srcBuf, (float *)tempBuf, unrolledCount*16);

        LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2((float *)tempBuf, unrolledCount*16,  255);
        
        LXPxConvert_int32_to_int8_withSaturate_SSE2(tempBuf, dstBuf, unrolledCount*16);
        /*
        {
        int32_t * LXRESTRICT int32Buf = tempBuf;
        uint32_t * LXRESTRICT ubuf = (uint32_t *)dstBuf;
        for (i = 0; i < unrolledCount; i++) {
            int32_t s0 = int32Buf[0];
            int32_t s1 = int32Buf[1];
            int32_t s2 = int32Buf[2];
            int32_t s3 = int32Buf[3];
            int32Buf += 4;
            uint32_t u = 0;
            SETBYTE0(u, s0);
            SETBYTE1(u, s1);
            SETBYTE2(u, s2);
            SETBYTE3(u, s3);
            *ubuf = u;
            ubuf++;
        }
        }
        */
        
        _lx_free(tempBuf);
        
        srcBuf += unrolledCount*16;
        dstBuf += unrolledCount*16;
        leftCount = count - unrolledCount*16;
    }
#endif

    for (i = 0; i < leftCount; i++) {
        float f = LXFloatFromHalf(srcBuf[0]);
        srcBuf++;
        float s = CLAMP_SAT_F(f) * mul;
        dstBuf[0] = (uint8_t)s;
        dstBuf++;
    }
}

void LXPxConvert_float16_to_int16(LXHalf * LXRESTRICT srcBuf, uint16_t * LXRESTRICT dstBuf, const size_t count, const uint16_t dstCodingWhite)
{
    if (count < 1) return;
    const float mul = (float)dstCodingWhite;
    LXUInteger leftCount = count;
    LXInteger i;
    
#if defined(LXVEC)
    if (count >= 8) {
        const size_t unrolledCount = count / 8;
        int32_t *tempBuf = _lx_malloc(count * sizeof(int32_t));
    
        LXConvertHalfToFloatArray(srcBuf, (float *)tempBuf, unrolledCount*8);

        LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2((float *)tempBuf, unrolledCount*8,  dstCodingWhite);
        
        ///LXPxConvert_int32_to_int16_withSaturate_SSE2(tempBuf, dstBuf, unrolledCount*8);
        // ^^^ can't use this, it's for signed ints!
        
#define CLAMP(v_)  ((v_ < 0) ? 0 : ((v_ > dstCodingWhite) ? dstCodingWhite : v_))
        int32_t * LXRESTRICT src = tempBuf;
        uint16_t * LXRESTRICT dst = dstBuf;
        const LXInteger n = unrolledCount*2;
        for (i = 0; i < n; i++) {
            int32_t v1 = src[0];
            int32_t v2 = src[1];
            int32_t v3 = src[2];
            int32_t v4 = src[3];
            src += 4;
            dst[0] = CLAMP(v1);
            dst[1] = CLAMP(v2);
            dst[2] = CLAMP(v3);
            dst[3] = CLAMP(v4);
            dst += 4;
        }
#undef CLAMP

        _lx_free(tempBuf);
        
        srcBuf += unrolledCount*8;
        dstBuf += unrolledCount*8;
        leftCount = count - unrolledCount*8;
    }
#endif
    
    for (i = 0; i < leftCount; i++) {
        float f = LXFloatFromHalf(srcBuf[0]);
        srcBuf++;
        float s = CLAMP_SAT_F(f) * mul;
        dstBuf[0] = (uint16_t)s;
        dstBuf++;
    }
}


void LXPxConvert_RGB_float16_to_RGB_int10(const LXInteger w, const LXInteger h, 
                                  LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                  const uint16_t dstCodingWhite)
{
    const float mul = (float)dstCodingWhite;
    LXInteger x, y;
    
    const size_t temp16RowBytes = LXAlignedRowBytes(w * 3 * sizeof(LXHalf));
    LXHalf *temp16Buf = _lx_malloc(temp16RowBytes);
    
    const size_t temp32RowBytes = LXAlignedRowBytes(w * 3 * sizeof(float));
    float *temp32Buf = _lx_malloc(temp32RowBytes);
    
#if defined(LXVEC)
    const LXUInteger unrolledCount = (w*3) / 4;
#else
    const LXUInteger unrolledCount = 0;
#endif
    const LXUInteger leftCount = (w*3) - unrolledCount*4;
    
    
    for (y = 0; y < h; y++) {        
        LXHalf * LXRESTRICT src = (LXHalf *) ((uint8_t *)srcBuf + srcRowBytes * y);
        LXHalf * LXRESTRICT t16 = temp16Buf;
        for (x = 0; x < w; x++) {
            t16[0] = src[0];
            t16[1] = src[1];
            t16[2] = src[2];
            src += srcValueStride;
            t16 += 3;
        }
        LXConvertHalfToFloatArray(temp16Buf, temp32Buf, w*3);

#if defined(LXVEC)
        LXPxConvert_float32_to_int32_withSaturate_inplace_SSE2(temp32Buf, unrolledCount*4,  dstCodingWhite);
#endif
        for (x = 0; x < leftCount; x++) {
            float *s = temp32Buf + (unrolledCount*4) + x;
            uint32_t *d = (uint32_t *)s;
            float f = *s;
            *d = (uint32_t) (CLAMP_SAT_F(f) * mul);
        }
        
        uint32_t * LXRESTRICT t32 = (uint32_t *) temp32Buf;
        uint32_t * LXRESTRICT dst = (uint32_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        for (x = 0; x < w; x++) {
            uint32_t ir = t32[0] & 0x3ff;
            uint32_t ig = t32[1] & 0x3ff;
            uint32_t ib = t32[2] & 0x3ff;
            t32 += 3;
            uint32_t u = (ir << 22) | (ig << 12) | (ib << 2);  // pixels are packed to "left" (i.e. low 2 bits left empty)
            dst[0] = u;
            dst++;
        }

/*       
        // straight-up single loop version of algorithm:
        for (x = 0; x < w; x++) {
            float r = LXFloatFromHalf(src[0]);
            float g = LXFloatFromHalf(src[1]);
            float b = LXFloatFromHalf(src[2]);
            r = CLAMP_SAT_F(r) * mul;
            g = CLAMP_SAT_F(g) * mul;
            b = CLAMP_SAT_F(b) * mul;
            
            uint32_t ir = (uint32_t)r & 0x3ff;
            uint32_t ig = (uint32_t)g & 0x3ff;
            uint32_t ib = (uint32_t)b & 0x3ff;
            uint32_t u = (ir << 22) | (ig << 12) | (ib << 2);  // pixels are packed to "left" (i.e. low 2 bits left empty)
            
            ///#if !defined(__BIG_ENDIAN__)
            ///u = LXEndianSwap_uint32(u);
            ///#endif
            dst[0] = u;
            
            src += srcValueStride;
            dst += 1;
        }
*/
    }
    _lx_free(temp16Buf);
    _lx_free(temp32Buf);
}

void LXPxConvert_RGB_float32_to_RGB_int10(const LXInteger w, const LXInteger h, 
                                  float * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                  uint8_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                  const uint16_t dstCodingWhite)
{
    const float mul = (float)dstCodingWhite;
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint32_t * LXRESTRICT dst = (uint32_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
            float r = CLAMP_SAT_F(src[0]) * mul;
            float g = CLAMP_SAT_F(src[1]) * mul;
            float b = CLAMP_SAT_F(src[2]) * mul;
            
            uint32_t ir = (uint32_t)r & 0x3ff;
            uint32_t ig = (uint32_t)g & 0x3ff;
            uint32_t ib = (uint32_t)b & 0x3ff;
            uint32_t u = (ir << 22) | (ig << 12) | (ib << 2);  // pixels are packed to "left" (i.e. low 2 bits left empty)
            dst[0] = u;
            
            src += srcValueStride;
            dst += 1;
        }
    }
}

void LXPxConvert_RGB_float16_to_RGB_int16(const LXInteger w, const LXInteger h, 
                                        LXHalf * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite)
{
    const float mul = (float)dstCodingWhite;
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        LXHalf * LXRESTRICT src = (LXHalf *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
            float r = LXFloatFromHalf(src[0]);
            float g = LXFloatFromHalf(src[1]);
            float b = LXFloatFromHalf(src[2]);
            r = CLAMP_SAT_F(r) * mul;
            g = CLAMP_SAT_F(g) * mul;
            b = CLAMP_SAT_F(b) * mul;        
            dst[0] = (uint16_t)r;
            dst[1] = (uint16_t)g;
            dst[2] = (uint16_t)b;
            src += srcValueStride;
            dst += 3;
        }
    }
}

void LXPxConvert_RGB_float32_to_RGB_int16(const LXInteger w, const LXInteger h, 
                                        float * LXRESTRICT srcBuf, const size_t srcRowBytes, const LXInteger srcValueStride,
                                        uint16_t * LXRESTRICT dstBuf, const size_t dstRowBytes,
                                        const uint16_t dstCodingWhite)
{
    const float mul = (float)dstCodingWhite;
    LXInteger x, y;
    for (y = 0; y < h; y++) {
        float * LXRESTRICT src = (float *) ((uint8_t *)srcBuf + srcRowBytes * y);
        uint16_t * LXRESTRICT dst = (uint16_t *) ((uint8_t *)dstBuf + dstRowBytes * y);
        
        for (x = 0; x < w; x++) {
            float r = CLAMP_SAT_F(src[0]) * mul;
            float g = CLAMP_SAT_F(src[1]) * mul;
            float b = CLAMP_SAT_F(src[2]) * mul;
            dst[0] = (uint16_t)r;
            dst[1] = (uint16_t)g;
            dst[2] = (uint16_t)b;
            src += srcValueStride;
            dst += 3;
        }
    }
}



#pragma mark --- histogram ---

void LXImageCalcHistogram_RGBA_int8(const LXInteger w, const LXInteger h,
                                             uint8_t * LXRESTRICT srcBuf, const size_t srcRowBytes,
                                             uint32_t * LXRESTRICT histBuf)
{
    const size_t histEntries = 256;
    
    if ( !histBuf || !srcBuf || srcRowBytes < 1) return;
    
    memset(histBuf, 0, histEntries*4*sizeof(uint32_t));
    
    uint32_t * LXRESTRICT histR = histBuf;
    uint32_t * LXRESTRICT histG = histBuf + 256;
    uint32_t * LXRESTRICT histB = histBuf + 256*2;
    uint32_t * LXRESTRICT histA = histBuf + 256*3;
    
    LXInteger x, y;

    for (y = 0; y < h; y++) {
        uint32_t * LXRESTRICT usrc = (uint32_t *) (srcBuf + srcRowBytes * y);
        
        for (x = 0; x < w; x++) {
            uint32_t u = *usrc++;

            uint32_t r = GETBYTE0(u);
            histR[r]++;
            
            uint32_t g = GETBYTE1(u);
            histG[g]++;

            uint32_t b = GETBYTE2(u);
            histB[b]++;

            uint32_t a = GETBYTE3(u);            
            histA[a]++;
        }
    }
}

