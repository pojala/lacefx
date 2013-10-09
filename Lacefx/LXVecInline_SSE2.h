/*
 *  LXVecInline_SSE2.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 7.7.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include <xmmintrin.h>
#include <emmintrin.h>


typedef union _LXVectorFloat {
    vfloat_t v;
    float e[4];
} LXVectorFloat;


// SSE utils from Apple docs,
// source:  http://developer.apple.com/documentation/performance/Conceptual/Accelerate_sse_migration/migration_sse_translation/migration_sse_translation.html#//apple_ref/doc/uid/TP40002729-CH248-312317


// --- vector select ---

LXINLINE LXFUNCATTR_PURE vfloat_t 
_lx_vec_sel_ps(vfloat_t a, vfloat_t b, vfloat_t mask)
{
    b = _mm_and_ps(b, mask);
    a = _mm_andnot_ps(mask, a);
    return _mm_or_ps(a, b);
}

LXINLINE LXFUNCATTR_PURE vsint16_t 
_lx_vec_sel_epi16(vsint16_t a, vsint16_t b, vsint16_t mask)
{
    b = _mm_and_si128(b, mask);
    a = _mm_andnot_si128(mask, a);
    return _mm_or_si128(a, b);
}


// --- conversion ---

// convert float to signed int, with AltiVec style overflow
// (i.e. large float -> 0x7fffffff instead of 0x80000000)
LXINLINE LXFUNCATTR_PURE vsint32_t
_lx_vec_cts(vfloat_t v)
{
    const vfloat_t vec_two31 = (const vfloat_t) { 0x1.0p31f, 0x1.0p31f, 0x1.0p31f, 0x1.0p31f };
    
    vfloat_t overflow = _mm_cmpge_ps(v, vec_two31);
    vsint32_t result = (vsint32_t)_mm_cvtps_epi32(v);
    return _mm_xor_si128(result, (vsint32_t)overflow);
}


// -- non-saturating integer packing --

// pack 16-bit integers to 8-bit integers without saturation
LXINLINE LXFUNCATTR_PURE vuint8_t
_lx_vec_pack_epu16(vuint16_t hi, vuint16_t lo)
{
    const vuint16_t mask = (const vuint16_t) { 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff };
    // mask off high byte
    hi = _mm_and_si128(hi, mask);
    lo = _mm_and_si128(lo, mask);
    return _mm_packus_epi16(hi, lo);
}

// pack 32-bit integers to 16-bit integers without saturation
LXINLINE LXFUNCATTR_PURE vsint16_t
_lx_vec_pack_epi32(vsint32_t hi, vsint32_t lo)
{
    const vuint32_t mask = (const vuint32_t) { 0xffff, 0xffff, 0xffff, 0xffff };
    // mask off high byte
    hi = _mm_and_si128(hi, mask);
    lo = _mm_and_si128(lo, mask);
    return (vsint16_t) _mm_packs_epi32((vsint32_t)hi, (vsint32_t)lo);
}

LXINLINE LXFUNCATTR_PURE vuint16_t
_lx_vec_pack_epu32_hiwords(vuint32_t i1, vuint32_t i2)
{
    const vuint16_t loMaskA = { 0xffff, 0xffff, 0, 0,   0, 0, 0, 0 };
    const vuint16_t loMaskB = { 0, 0, 0xffff, 0xffff,   0, 0, 0, 0 };
    const vuint16_t hiMaskA = { 0, 0, 0, 0,             0xffff, 0xffff, 0, 0 };
    const vuint16_t hiMaskB = { 0, 0, 0, 0,             0, 0, 0xffff, 0xffff };

    vuint16_t shuffled1A = _mm_shufflelo_epi16(i1, _MM_SHUFFLE(0, 0, 3, 1));
    vuint16_t shuffled1B = _mm_shufflelo_epi16(_mm_srli_si128(i1, 8), _MM_SHUFFLE(3, 1, 0, 0));        
    
    vuint16_t shuffled2A = _mm_slli_si128(_mm_shufflelo_epi16(i2, _MM_SHUFFLE(0, 0, 3, 1)), 8);
    vuint16_t shuffled2B = _mm_slli_si128(_mm_shufflelo_epi16(_mm_srli_si128(i2, 8), _MM_SHUFFLE(3, 1, 0, 0)), 8);

    vuint16_t packed = _mm_or_si128(_mm_or_si128(_mm_and_si128(shuffled1A, loMaskA), _mm_and_si128(shuffled1B, loMaskB)),
                                    _mm_or_si128(_mm_and_si128(shuffled2A, hiMaskA), _mm_and_si128(shuffled2B, hiMaskB)));
    return packed;
}

LXINLINE LXFUNCATTR_PURE vuint16_t
_lx_vec_pack_epu32_lowords(vuint32_t i1, vuint32_t i2)
{
    const vuint16_t loMaskA = { 0xffff, 0xffff, 0, 0,   0, 0, 0, 0 };
    const vuint16_t loMaskB = { 0, 0, 0xffff, 0xffff,   0, 0, 0, 0 };
    const vuint16_t hiMaskA = { 0, 0, 0, 0,             0xffff, 0xffff, 0, 0 };
    const vuint16_t hiMaskB = { 0, 0, 0, 0,             0, 0, 0xffff, 0xffff };

    vuint16_t shuffled1A = _mm_shufflelo_epi16(i1, _MM_SHUFFLE(0, 0, 2, 0));
    vuint16_t shuffled1B = _mm_shufflelo_epi16(_mm_srli_si128(i1, 8), _MM_SHUFFLE(2, 0, 0, 0));        
    
    vuint16_t shuffled2A = _mm_slli_si128(_mm_shufflelo_epi16(i2, _MM_SHUFFLE(0, 0, 2, 0)), 8);
    vuint16_t shuffled2B = _mm_slli_si128(_mm_shufflelo_epi16(_mm_srli_si128(i2, 8), _MM_SHUFFLE(2, 0, 0, 0)), 8);

    vuint16_t packed = _mm_or_si128(_mm_or_si128(_mm_and_si128(shuffled1A, loMaskA), _mm_and_si128(shuffled1B, loMaskB)),
                                    _mm_or_si128(_mm_and_si128(shuffled2A, hiMaskA), _mm_and_si128(shuffled2B, hiMaskB)));
    return packed;
}


