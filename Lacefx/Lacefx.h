/*
 *  Lacefx.h
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#ifndef _LACEFX_H_
#define _LACEFX_H_

#include "LXBasicTypes.h"
#include "LXBasicTypeFunctions.h"
#include "LXRefTypes.h"

#include "LXAccumulator.h"
#include "LXCList.h"
#include "LXConvolver.h"
#include "LXMap.h"
#include "LXPool.h"
#include "LXPixelBuffer.h"
#include "LXShader.h"
#include "LXSurface.h"
#include "LXTexture.h"
#include "LXTextureArray.h"
#include "LXTransform3D.h"

#include "LXHalfFloat.h"
#include "LXMutex.h"
#include "LXPlatform.h"

// LXSuites contains the Lacefx plugin API entry points and function suites
#include "LXSuites.h"

// Lacefx also includes a number of utility functions in separate headers, e.g. LXImageUtils.h and LXPatternGen.h.
// String utils is the most frequently needed.
#include "LXStringUtils.h"

#endif
