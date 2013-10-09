/*
 *  LXRandomGen.h
 *  Lacefx
 *
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXRANDOMFUNCTIONS_H_
#define _LXRANDOMFUNCTIONS_H_

#include "LXBasicTypes.h"


// forward declaration of randomizer state object
typedef struct _lxrs_state_s *LXRdStatePtr;


#ifdef __cplusplus
extern "C" {
#endif

// initializing the default state (a shared object)
LXEXPORT void LXRdSeed();

// creating a private randomizer state object
LXEXPORT LXRdStatePtr LXRdStateCreateSeeded();
LXEXPORT void LXRdStateDestroy(LXRdStatePtr state);


// --- functions that use a provided state ---
//
LXEXPORT double		LXRdUniform_s(LXRdStatePtr state, double lower, double upper);
LXEXPORT double		LXRdUniform_s_fullPrec(LXRdStatePtr state, double lower, double upper);

LXEXPORT double		LXRdGaussian_s(LXRdStatePtr state, double mean, double sigma);
LXEXPORT double		LXRdGaussian_s_fullPrec(LXRdStatePtr state, double mean, double sigma);

// for LXInteger-sized uniform integer, use LXRdUniform_s_i (defined below)
LXEXPORT int32_t	LXRdUniform_s_i32(LXRdStatePtr state, int32_t lower, int32_t upper);
LXEXPORT int64_t	LXRdUniform_s_i64(LXRdStatePtr state, int64_t lower, int64_t upper);


// --- functions that use randomizer's default state ---
    
// WARNING: shared state, may not be thread-safe.
// Use the _s versions below when multiple threads may access the random generator.

LXEXPORT double		LXRdUniform(double lower, double upper);
LXEXPORT double		LXRdUniform_fullPrec(double lower, double upper);  // full-precision 64-bit random numbers, but slower

// for LXInteger-sized uniform integer, use LXRdUniform_i (defined below)
LXEXPORT int32_t	LXRdUniform_i32(int32_t lower, int32_t upper);
LXEXPORT int64_t	LXRdUniform_i64(int64_t lower, int64_t upper);




// --- macros that return an LXInteger-sized (=native-width integer) value ---
#ifdef LX64BIT
 #define LXRdUniform_i(lower_, upper_)   LXRdUniform_i64(lower_, upper_)
 #define LXRdUniform_s_i(state_, lower_, upper_)   LXRdUniform_s_i64(state_, lower_, upper_)
#else
 #define LXRdUniform_i(lower_, upper_)   LXRdUniform_i32(lower_, upper_)
 #define LXRdUniform_s_i(state_, lower_, upper_)   LXRdUniform_s_i32(state_, lower_, upper_)
#endif


#ifdef __cplusplus
}
#endif

#endif // _LXRANDISTRS_H_
