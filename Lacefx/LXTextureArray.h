/*
 *  LXTextureArray.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 19.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXTEXTUREARRAY_H_
#define _LXTEXTUREARRAY_H_

#include "LXRefTypes.h"
#include "LXBasicTypes.h"


#ifdef __cplusplus
extern "C" {
#endif


#pragma mark --- LXTextureArray public API methods ---

LXEXPORT const char *LXTextureArrayTypeID();

LXEXPORT LXTextureArrayRef LXTextureArrayCreateWithTexture(LXTextureRef texture);
LXEXPORT LXTextureArrayRef LXTextureArrayCreateWithTextures(LXInteger optionalCount, ...);
LXEXPORT LXTextureArrayRef LXTextureArrayCreateWithTexturesAndCount(LXTextureRef *textures, LXInteger count);

LXEXPORT LXTextureArrayRef LXTextureArrayRetain(LXTextureArrayRef r);
LXEXPORT void LXTextureArrayRelease(LXTextureArrayRef texArray);

LXEXPORT LXInteger LXTextureArraySlotCount(LXTextureArrayRef texArray);
LXEXPORT LXInteger LXTextureArrayInUseCount(LXTextureArrayRef texArray);
LXEXPORT LXTextureRef LXTextureArrayAt(LXTextureArrayRef texArray, LXInteger index);

LXEXPORT void LXTextureArrayForEach(LXTextureArrayRef, LXRefEnumerationFuncPtr enumFunc, void *userData);

// returns kLXNotFound if a sampling override is not specified for the texture
// (this is the default state for a newly created texture array)
LXEXPORT LXUInteger LXTextureArrayGetSamplingAt(LXTextureArrayRef texArray, LXInteger index);
LXEXPORT void LXTextureArraySetSamplingAt(LXTextureArrayRef texArray, LXInteger index, LXUInteger samplingEnum);

LXEXPORT LXUInteger LXTextureArrayGetWrapModeAt(LXTextureArrayRef texArray, LXInteger index);
LXEXPORT void LXTextureArraySetWrapModeAt(LXTextureArrayRef texArray, LXInteger index, LXUInteger wrapEnum);

#ifdef __cplusplus
}
#endif

#endif
