/*
 *  LXHalfFloat.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.4.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXHALFFLOAT_H_
#define _LXHALFFLOAT_H_

#include "LXBasicTypes.h"


typedef union _LXUif {
    uint32_t    i;
    float       f;
} LXUif;


typedef uint16_t LXHalf;


#ifdef __cplusplus
extern "C" {
#endif


// lookup tables for fast conversions
LXEXPORT_CONSTVAR LXUif	  s_LXHalf_toFloat[1 << 16];
LXEXPORT_CONSTVAR LXHalf  s_LXHalf_eLut[1 << 9];


// array functions
LXEXPORT void LXConvertHalfToFloatArray(const LXHalf * LXRESTRICT ph, float * LXRESTRICT pf, const size_t n);
LXEXPORT void LXConvertFloatToHalfArray(const float * LXRESTRICT pf, LXHalf * LXRESTRICT ph, const size_t n);


// the general case conversion function that handles overflows, etc.
LXEXPORT LXFUNCATTR_PURE short LXFloatToHalfGeneral(int i);


#ifdef __cplusplus
}
#endif


LXINLINE LXFUNCATTR_PURE float LXFloatFromHalf(const LXHalf h)
{
    return s_LXHalf_toFloat[h].f;
}

LXINLINE LXFUNCATTR_PURE LXHalf LXHalfFromFloat(const float f)
{
    if (f == 0.0f) {
        return 0; ///h = (x.i >> 16);
    }
    else {
        LXUif x;
        uint16_t h;

        x.f = f;
        
        LXUInteger e = (x.i >> 23) & 0x000001ff;
        LXUInteger el = s_LXHalf_eLut[e];

        if (el) {
            LXUInteger m = x.i & 0x007fffff;
            h = el + ((m + 0x00000fff + ((m >> 13) & 1)) >> 13);
        }
        else {
            h = LXFloatToHalfGeneral(x.i);
        }
        return (LXHalf)h;
    }
}


#endif
