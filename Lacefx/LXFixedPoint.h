/*
 *  LXFixedPoint.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 9.2.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXFIXEDPOINT_H_
#define _LXFIXEDPOINT_H_

#include "LXBasicTypes.h"


// 16.16 fixed point number
typedef int32_t LXFixedNum;

// decimal part size in bits
#define FIXD_Q   16

#define FIXD_ONE            ((int32_t) 65536)
#define FIXD_ONE_FLOAT      ((float)   65536.0f)
#define FIXD_HALF           ((int32_t) 32768)
#define FIXD_TENTH          ((int32_t) 6554)
#define FIXD_MAX            ((int32_t) 0x7fffffff)
#define FIXD_MIN            ((int32_t) 0x80000000)
#define FIXD_255            FIXD_FROM_INT(255)

#define FIXD_TO_FLOAT(x)    ((float) ((long)(x) / FIXD_ONE_FLOAT))
#define FIXD_TO_DOUBLE(x)   ((double) ((long)(x) / 65536.0))

#define FIXD_FROM_FLOAT(x)  ((LXFixedNum) (x * FIXD_ONE_FLOAT))
#define FIXD_FROM_DOUBLE(x) ((LXFixedNum) (x * 65536.0))

#define FIXD_TO_INT(x)      ((x) >> FIXD_Q)

#define FIXD_FROM_INT(x)    ((x) << FIXD_Q)


#define FIXD_MUL(x,y)       ((x) >> 8) * ((y) >> 8)

#define FIXD_DIV(x,y)       ((((x) << 8) / (y)) << 8)

#define FIXD_QMUL(x,y)      (LXFixed_qmul (x,y))
#define FIXD_QDIV(x,y)      (LXFixed_qdiv (x,y))

#define FIXD_CLAMP_255(a_)   ( (a_ > FIXD_255) ? FIXD_255 : ((a_ < 0) ? 0 : a_) )


// --- inline functions ---

LXINLINE LXFUNCATTR_PURE LXFixedNum LXFixed_qmul (LXFixedNum a, LXFixedNum b)
{
#if defined(__armv5te__)
    int res;
    __asm__("smulbb  %0,%1,%2;\n"
              : "=&r" (res) \
              : "%r"(a), "r"(b));
    return (LXFixedNum)res;
    
#elif defined(__arm__)
    int res, temp1;
    __asm__("smull %0, %1, %2, %3     \n"
            "mov   %0, %0,     lsr %4 \n"
            "add   %0, %0, %1, lsl %5 \n"
                    : "=r" (res), "=r" (temp1) \
                    : "r"(a), "r"(b), "i"(FIXD_Q), "i"(32 - FIXD_Q)
            );
    return (LXFixedNum)res;
    
#else
    int64_t r = (int64_t)a * (int64_t)b;
    
    return (LXFixedNum)(r >> FIXD_Q);
#endif
}


LXINLINE LXFUNCATTR_PURE LXFixedNum LXFixed_qdiv (LXFixedNum a, LXFixedNum b)
{
  return (LXFixedNum)( (((int64_t)a) << FIXD_Q) / b );
}



// --- math functions ---

#ifdef __cplusplus
extern "C" {
#endif

// fixed-point square root -- not included in Lacefx
///LXFixedNum LXFixed_sqrt (LXFixedNum x);

#ifdef __cplusplus
}
#endif

#endif  //_LXFIXEDPOINT_H_

