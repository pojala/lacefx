/*
 *  LXAccumulator.h
 *  Lacefx
 *
 *  Created by Pauli Ojala.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXACCUMULATOR_H_
#define _LXACCUMULATOR_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"
#include "LXPool.h"


#pragma mark --- LXAccumulator public API methods ---

LXEXPORT const char *LXAccumulatorTypeID();

LXEXPORT LXAccumulatorRef LXAccumulatorCreateWithSize(LXPoolRef pool, LXSize size, LXUInteger flags, LXError *outError);

LXEXPORT LXAccumulatorRef LXAccumulatorRetain(LXAccumulatorRef acc);
LXEXPORT void LXAccumulatorRelease(LXAccumulatorRef acc);

LXEXPORT void LXAccumulatorSetNumberOfSamples(LXAccumulatorRef acc, LXInteger numSamples);
LXEXPORT LXInteger LXAccumulatorGetNumberOfSamples(LXAccumulatorRef acc);

LXEXPORT LXSize LXAccumulatorGetSize(LXAccumulatorRef acc);
LXEXPORT LXBool LXAccumulatorMatchesSize(LXAccumulatorRef acc, LXSize size);


LXEXPORT LXBool LXAccumulatorStartAccumulation(LXAccumulatorRef r, LXError *outError);

LXEXPORT void LXAccumulatorAccumulateTexture(LXAccumulatorRef acc, LXTextureRef texture);
LXEXPORT void LXAccumulatorAccumulateTextureWithOpacity(LXAccumulatorRef acc, LXTextureRef texture, LXFloat opacity);

LXEXPORT LXSurfaceRef LXAccumulatorFinishAccumulation(LXAccumulatorRef acc);  // returned surface is retained


#endif // _LXACCUMULATOR_H_
