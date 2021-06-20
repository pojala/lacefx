/*
 *  LXHalfFloat.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.4.2008.
 *
 
 This file is based on half.cpp in OpenEXR library.
 Original license follows:
 -----
 ///////////////////////////////////////////////////////////////////////////
 //
 // Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
 // Digital Ltd. LLC
 //
 // All rights reserved.
 //
 // Redistribution and use in source and binary forms, with or without
 // modification, are permitted provided that the following conditions are
 // met:
 // *       Redistributions of source code must retain the above copyright
 // notice, this list of conditions and the following disclaimer.
 // *       Redistributions in binary form must reproduce the above
 // copyright notice, this list of conditions and the following disclaimer
 // in the documentation and/or other materials provided with the
 // distribution.
 // *       Neither the name of Industrial Light & Magic nor the names of
 // its contributors may be used to endorse or promote products derived
 // from this software without specific prior written permission.
 //
 // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 // "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 // LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 // A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 // OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 // LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 // DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 // (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 //
 ///////////////////////////////////////////////////////////////////////////
 
 // Primary authors:
 //     Florian Kainz <kainz@ilm.com>
 //     Rod Bogart <rgb@ilm.com>
 -----
 */

#include "LXHalfFloat.h"

// vector-optimized versions
#if defined(__SSE2__)
LXFUNCATTR_SSE static void LXConvertFloatToHalfArray_SSE2_blocksOf8(const float * LXRESTRICT pf, LXHalf * LXRESTRICT ph, const size_t n);
#endif


// lookup tables
const LXUif	s_LXHalf_toFloat[1 << 16] =
    #include "LXHalfToFloatLUT.h"

const LXHalf s_LXHalf_eLut[1 << 9] = 
    #include "LXHalfELUT.h"



static float overflow()
{
    volatile float f = 1e10;
    int i;

    for (i = 0; i < 10; i++)	
	f *= f;				// this will overflow before
                        // the for loop terminates
    return f;
}


//-----------------------------------------------------
// Float-to-half conversion -- general case, including
// zeroes, denormalized numbers and exponent overflows.
//-----------------------------------------------------

short LXFloatToHalfGeneral(int i)
{
    //
    // Our floating point number, f, is represented by the bit
    // pattern in integer i.  Disassemble that bit pattern into
    // the sign, s, the exponent, e, and the significand, m.
    // Shift s into the position where it will go in in the
    // resulting half number.
    // Adjust e, accounting for the different exponent bias
    // of float and half (127 versus 15).
    //

    register int s =  (i >> 16) & 0x00008000;
    register int e = ((i >> 23) & 0x000000ff) - (127 - 15);
    register int m =   i        & 0x007fffff;

    //
    // Now reassemble s, e and m into a half:
    //

    if (e <= 0)
    {
	if (e < -10)
	{
	    //
	    // E is less than -10.  The absolute value of f is
	    // less than HALF_MIN (f may be a small normalized
	    // float, a denormalized float or a zero).
	    //
	    // We convert f to a half zero with the same sign as f.
	    //

	    return s;
	}

	//
	// E is between -10 and 0.  F is a normalized float
	// whose magnitude is less than HALF_NRM_MIN.
	//
	// We convert f to a denormalized half.
	//

	//
	// Add an explicit leading 1 to the significand.
	// 

	m = m | 0x00800000;

	//
	// Round to m to the nearest (10+e)-bit value (with e between
	// -10 and 0); in case of a tie, round to the nearest even value.
	//
	// Rounding may cause the significand to overflow and make
	// our number normalized.  Because of the way a half's bits
	// are laid out, we don't have to treat this case separately;
	// the code below will handle it correctly.
	// 

	int t = 14 - e;
	int a = (1 << (t - 1)) - 1;
	int b = (m >> t) & 1;

	m = (m + a + b) >> t;

	//
	// Assemble the half from s, e (zero) and m.
	//

	return s | m;
    }
    else if (e == 0xff - (127 - 15))
    {
	if (m == 0)
	{
	    //
	    // F is an infinity; convert f to a half
	    // infinity with the same sign as f.
	    //

	    return s | 0x7c00;
	}
	else
	{
	    //
	    // F is a NAN; we produce a half NAN that preserves
	    // the sign bit and the 10 leftmost bits of the
	    // significand of f, with one exception: If the 10
	    // leftmost bits are all zero, the NAN would turn 
	    // into an infinity, so we have to set at least one
	    // bit in the significand.
	    //

	    m >>= 13;
	    return s | 0x7c00 | m | (m == 0);
	}
    }
    else
    {
	//
	// E is greater than zero.  F is a normalized float.
	// We try to convert f to a normalized half.
	//

	//
	// Round to m to the nearest 10-bit value.  In case of
	// a tie, round to the nearest even value.
	//

	m = m + 0x00000fff + ((m >> 13) & 1);

	if (m & 0x00800000)
	{
	    m =  0;		// overflow in significand,
	    e += 1;		// adjust exponent
	}

	//
	// Handle exponent overflow
	//

	if (e > 30)
	{
	    overflow ();	// Cause a hardware floating point overflow;
	    return s | 0x7c00;	// if this returns, the half becomes an
	}   			// infinity with the same sign as f.

	//
	// Assemble the half from s, e and m.
	//

	return s | (e << 10) | (m >> 13);
    }
}


#ifdef __BIG_ENDIAN__
 #define PACK_LXHALF_TO_UINT32(h1_, h2_)   (((uint32_t)(h1_) & 0xFFFF) << 16) | (((uint32_t)(h2_) & 0xFFFF))
 #define UNPACK_LXHALF1_FROM_UINT32(u_)    (((u_) >> 16) & 0xFFFF)
 #define UNPACK_LXHALF2_FROM_UINT32(u_)    ((u_) & 0xFFFF)
#else
 #define PACK_LXHALF_TO_UINT32(h1_, h2_)   (((uint32_t)(h2_) & 0xFFFF) << 16) | (((uint32_t)(h1_) & 0xFFFF))
 #define UNPACK_LXHALF1_FROM_UINT32(u_)    ((u_) & 0xFFFF)
 #define UNPACK_LXHALF2_FROM_UINT32(u_)    (((u_) >> 16) & 0xFFFF)
#endif



void LXConvertHalfToFloatArray(const LXHalf * LXRESTRICT ph, float * LXRESTRICT pf, const size_t n)
{
    LXUInteger i;
    
    const size_t vecN = n / 4;
    const size_t restN = (n - vecN * 4);
    
    const uint32_t * LXRESTRICT pu = (const uint32_t *)ph;
    
    for (i = 0; i < vecN; i++) {
        // read four half floats at once
        uint32_t u1 = pu[0];
        uint32_t u2 = pu[1];
        pu += 2;
        
        uint32_t h1 = UNPACK_LXHALF1_FROM_UINT32(u1);
        pf[0] = s_LXHalf_toFloat[h1].f;
        uint32_t h2 = UNPACK_LXHALF2_FROM_UINT32(u1);
        pf[1] = s_LXHalf_toFloat[h2].f;
        uint32_t h3 = UNPACK_LXHALF1_FROM_UINT32(u2);
        pf[2] = s_LXHalf_toFloat[h3].f;
        uint32_t h4 = UNPACK_LXHALF2_FROM_UINT32(u2);
        pf[3] = s_LXHalf_toFloat[h4].f;
        pf += 4;
    }
    
    // finish with non-unrolled loop
    ph += vecN * 4;
    for (i = 0; i < restN; i++) {
        *pf = s_LXHalf_toFloat[*ph].f;
        ph++;
        pf++;
    }
}

void LXConvertFloatToHalfArray(const float * LXRESTRICT pf, LXHalf * LXRESTRICT ph, const size_t n)
{
    LXUInteger i;
    size_t vecN = 0;
    LXBool didHandleVec = NO;
    
    #if defined(__SSE2__)
    if (n >= 8) {
        LXConvertFloatToHalfArray_SSE2_blocksOf8(pf, ph, n);
        vecN = (n / 8) * 2;
        pf += vecN * 4;
        ph += vecN * 4;
        didHandleVec = YES;
    }
    #endif
    
    if ( !didHandleVec) {
        vecN = n / 4;
        uint32_t * LXRESTRICT pu = (uint32_t *)ph;
        
        for (i = 0; i < vecN; i++) {
            float f1 = pf[0];
            float f2 = pf[1];
            float f3 = pf[2];
            float f4 = pf[3];
            pf += 4;
            
            uint32_t h1 = LXHalfFromFloat(f1);  // this is inlined and declared pure
            uint32_t h2 = LXHalfFromFloat(f2);
            pu[0] = PACK_LXHALF_TO_UINT32(h1, h2);
            
            uint32_t h3 = LXHalfFromFloat(f3);
            uint32_t h4 = LXHalfFromFloat(f4);
            pu[1] = PACK_LXHALF_TO_UINT32(h3, h4);
            pu += 2;
        }
        ph += vecN * 4;
    }

    size_t restN = (n - vecN * 4);

    // finish with non-unrolled loop
    for (i = 0; i < restN; i++) {
        *ph = LXHalfFromFloat(*pf);
        pf++;
        ph++;
        /*
        LXUif x;
        register LXHalf h;

        x.f = *pf;
        pf++;

        if (x.f == 0.0f) {
            h = (x.i >> 16);
        }
        else {
            register int e = (x.i >> 23) & 0x000001ff;

            e = s_LXHalf_eLut[e];

            if (e) {
                register int m = x.i & 0x007fffff;
                h = e + ((m + 0x00000fff + ((m >> 13) & 1)) >> 13);
            }
            else {
                h = LXFloatToHalfGeneral(x.i);
            }
        }
        */
    }
}



#if defined(__GCC__) && 0
// must include function definitions here in case inlining is off in compiler (i.e. -O0 flag)

float LXFloatFromHalf(LXHalf h)
{
    return s_LXHalf_toFloat[h].f;
}


// this code is commented in OpenEXR half.h
LXHalf LXHalfFromFloat(float f)
{
    LXUif x;
    unsigned short h;

    x.f = f;

    if (f == 0.0f) {
        h = (x.i >> 16);
    }
    else {
        register int e = (x.i >> 23) & 0x000001ff;

        e = s_LXHalf_eLut[e];

        if (e) {
            register int m = x.i & 0x007fffff;
            h = e + ((m + 0x00000fff + ((m >> 13) & 1)) >> 13);
        }
        else {
            h = LXFloatToHalfGeneral(x.i);
        }
    }
    return (LXHalf)h;
}

#endif



#if !defined(__BIG_ENDIAN__) && !defined(LXPLATFORM_IOS) && !defined(LXPLATFORM_MAC_ARM64) && !defined(__SSE2__)
#warning "Lacefx ought to be compiled with SSE2 on x86 for best performance"
#endif


#if defined(__SSE2__)
#include <emmintrin.h>
#include "LXVecInline_SSE2.h"


LXFUNCATTR_SSE static void LXConvertFloatToHalfArray_SSE2_blocksOf8(const float * LXRESTRICT pf, LXHalf * LXRESTRICT ph, const size_t n)
{
    LXUInteger i;
    
    const size_t vecN = n / 8;

    ///LXPrintf("%s: float ptr is %p, half ptr is %p, n is %i -> vecN %i\n", __func__, pf, ph, (int)n, vecN);
    
//    register int s =  (i >> 16) & 0x00008000;
//    register int e = ((i >> 23) & 0x000000ff) - (127 - 15);
//    register int m =   i        & 0x007fffff;
    
    //const vuint16_t all_ones_u16 = { 0xffff, 0xffff, 0xffff, 0xffff,  0xffff, 0xffff, 0xffff, 0xffff };
    const vsint16_t one_s16 = { 1, 1, 1, 1,  1, 1, 1, 1 };
    
    const vuint16_t s_mask = { 0x8000, 0x8000, 0x8000, 0x8000,  0x8000, 0x8000, 0x8000, 0x8000 };
    ///const vuint16_t us_mask = { 0x7fff, 0x7fff, 0x7fff, 0x7fff,  0x7fff, 0x7fff, 0x7fff, 0x7fff };
    
    const vuint16_t mantissa_leading_one_times_two_u16 = { 0x800, 0x800, 0x800, 0x800,  0x800, 0x800, 0x800, 0x800 };
    
    const vuint16_t inf_value_u16 = { 0x7c00, 0x7c00, 0x7c00, 0x7c00,  0x7c00, 0x7c00, 0x7c00, 0x7c00 };
    const vuint16_t nan_value_u16 = { 0x7e00, 0x7e00, 0x7e00, 0x7e00,  0x7e00, 0x7e00, 0x7e00, 0x7e00 };

    const vsint16_t offset112_u16 = { 112, 112, 112, 112,  112, 112, 112, 112 };
    const vsint16_t twelve_s16 = { 12, 12, 12, 12,  12, 12, 12, 12 };
    const vuint16_t thirty_u16 = { 30, 30, 30, 30,  30, 30, 30, 30 };    
    const vuint16_t nan_mantissa_u16 = { 0x400, 0x400, 0x400, 0x400,  0x400, 0x400, 0x400, 0x400 };    

    for (i = 0; i < vecN; i++) {
        vuint32_t i1 = _mm_load_si128((__m128i *)pf);        
        vuint32_t i2 = _mm_load_si128((__m128i *)(pf + 4));
        pf += 8;

        // vec_sld(i1, i1, 14)
        vuint32_t shifted1, shifted2;
        vuint16_t upper_word_of_floats = _lx_vec_pack_epu32_hiwords(i1, i2);
        /*
        shifted1 = _mm_or_si128(_mm_slli_si128(i1, 14), _mm_srli_si128(i1, 2));
        shifted2 = _mm_or_si128(_mm_slli_si128(i2, 14), _mm_srli_si128(i2, 2));
        
        shifted1 = _mm_sub_epi32(shifted1, unsigned_bias_s32);
        shifted2 = _mm_sub_epi32(shifted2, unsigned_bias_s32);
        upper_word_of_floats = _lx_vec_pack_epi32(shifted1, shifted2);
        upper_word_of_floats = _mm_sub_epi16(upper_word_of_floats, unsigned_bias_s16);
    */

        // --testing
        ///vuint16_t shuffled1 = _mm_shufflehi_epi16(_mm_shufflelo_epi16(i1, shuffIm), shuffIm);
        // ---

        
        // bit 0 contains the sign
        vuint16_t orig_s = _mm_and_si128(upper_word_of_floats, s_mask);
        
        upper_word_of_floats = _mm_andnot_si128(s_mask, upper_word_of_floats);

        //i1 = _mm_and_si128(i1, us_mask_u32);
        //i2 = _mm_and_si128(i2, us_mask_u32);
        
        // bits 8-15 contain the exponent
        vuint16_t orig_e = _mm_srli_epi16(upper_word_of_floats, 7);

        // subtract 127-15 = 112        
        vsint16_t result_e = _mm_sub_epi16(orig_e, offset112_u16);
        
        // bits 5-15 contain the upper 11 bits of the mantissa
        shifted1 = _mm_srli_epi32(i1, 7);
        shifted2 = _mm_srli_epi32(i2, 7);

        /*// SSE2 doesn't have an unsigned 32->16 int pack instruction, so we must bias
        shifted1 = _mm_sub_epi32(shifted1, unsigned_bias_s32);
        shifted2 = _mm_sub_epi32(shifted2, unsigned_bias_s32);
        vuint16_t result_m = _lx_vec_pack_epi32(shifted1, shifted2);  //_mm_srli_epi16(_lx_vec_pack_epu32(shifted1, shifted2), 5);
        result_m = _mm_add_epi16(result_m, unsigned_bias_s16);
        */
        vuint16_t result_m = _lx_vec_pack_epu32_lowords(shifted1, shifted2);

        result_m = _mm_srli_epi16(result_m, 5);
        
        // Round off by adding 1 before shifting.  Note that if the mantissa
        // overflows, then one is added to the exponent and the mantissa becomes
        // zero, so it still works.
        vuint16_t shifted_m = _mm_srli_epi16(_mm_add_epi16(result_m, one_s16), 1);
        vuint16_t shifted_e = _mm_slli_epi16((vuint16_t)result_e, 10);
        
        vuint16_t result = _mm_or_si128(shifted_m, shifted_e);
        
        vsint16_t is_tiny = _mm_cmplt_epi16(result_e, one_s16);

        vuint16_t shift_amount = _mm_min_epi16(_mm_sub_epi16(one_s16, result_e),
                                               twelve_s16);
        
        vuint16_t tiny_result = _mm_srl_epi16(_mm_add_epi16(result_m, mantissa_leading_one_times_two_u16),
                                              shift_amount);
                                              
        tiny_result = _mm_srli_epi16(_mm_add_epi16(tiny_result, one_s16), 1);
        
        result = _lx_vec_sel_epi16(result, tiny_result, is_tiny);
        
        // handle infinity and NaN
        vuint16_t is_inf_or_nan = _mm_cmpgt_epi16(result_e, thirty_u16);
        vuint16_t is_nan = _mm_cmpeq_epi16(result_m, nan_mantissa_u16);

        result = _lx_vec_sel_epi16(result, _lx_vec_sel_epi16(inf_value_u16, nan_value_u16, is_nan), is_inf_or_nan);
        
        // copy sign bit
        result = _mm_or_si128(orig_s, result);
        
        //result = is_nan;  //orig_s; //upper_word_of_floats;
        ///if (i== 0) LXPrintf("should store: %p - ", ph);
        _mm_store_si128((__m128i *)ph, result);
        ph += 8;
    }
}

#endif

