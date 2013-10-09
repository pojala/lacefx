/*
 *  LXPatternGen.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.4.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPATTERNGEN_H_
#define _LXPATTERNGEN_H_

#include "LXBasicTypes.h"
#include "LXPixelBuffer.h"

#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT LXPixelBufferRef LXPatternCreateBlank(LXSize size);

LXEXPORT LXPixelBufferRef LXPatternCreateGrid(LXSize size);

// cellsize must be a power of 2
LXEXPORT LXPixelBufferRef LXPatternCreateGridWithCellSizeAndLineWidth(LXSize size, const int cellsize, const int linew);


LXEXPORT LXPixelBufferRef LXPatternCreateCheckerboardWithTileSizeAndColors(LXSize size, const unsigned int tilesize, LXRGBA color1, LXRGBA color2);


LXEXPORT LXPixelBufferRef LXPatternCreate75PercentColorBars(LXSize size);

LXEXPORT LXPixelBufferRef LXPatternCreateGradientColorBars(LXSize size);


#ifdef __cplusplus
}
#endif

#endif
