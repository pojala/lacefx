/*
 *  LXColorFunctions.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 31.7.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXCOLORFUNCTIONS_H_
#define _LXCOLORFUNCTIONS_H_

#include "LXBasicTypes.h"
#include "LXTransform3D.h"  // for LX4x4Matrix


enum {
	kLXWhitePoint_D65 = 0,  // the default for sRGB and video
	kLXWhitePoint_D40,
	kLXWhitePoint_D45,
	kLXWhitePoint_D50,
	kLXWhitePoint_D55,
	kLXWhitePoint_D60,
	kLXWhitePoint_D70,
	kLXWhitePoint_A,
	kLXWhitePoint_B,
	kLXWhitePoint_C,
	kLXWhitePoint_E
};
typedef LXUInteger LXColorXYZWhitePoint;

// x/y chromaticities for standard whitepoints (A - E)
#define kLXWhitePoint_A_x 0.44757
#define kLXWhitePoint_A_y 0.40745
#define kLXWhitePoint_B_x 0.34842
#define kLXWhitePoint_B_y 0.05161
#define kLXWhitePoint_C_x 0.31006
#define kLXWhitePoint_C_y 0.31616
#define kLXWhitePoint_D40_x 0.382344
#define kLXWhitePoint_D40_y 0.383766
#define kLXWhitePoint_D45_x 0.362089
#define kLXWhitePoint_D45_y 0.370870
#define kLXWhitePoint_D50_x 0.345741
#define kLXWhitePoint_D50_y 0.358666
#define kLXWhitePoint_D55_x 0.332502
#define kLXWhitePoint_D55_y 0.347608
#define kLXWhitePoint_D60_x 0.321692
#define kLXWhitePoint_D60_y 0.337798
#define kLXWhitePoint_D65_x 0.312779
#define kLXWhitePoint_D65_y 0.329183
#define kLXWhitePoint_D70_x 0.305357
#define kLXWhitePoint_D70_y 0.321646
#define kLXWhitePoint_D75_x 0.299091
#define kLXWhitePoint_D75_y 0.315025
#define kLXWhitePoint_E_x (1.0/3.0)
#define kLXWhitePoint_E_y (1.0/3.0)
				

#ifdef __cplusplus
extern "C" {
#endif
				
LXEXPORT void LXColorXYZfromYxy(float *outXYZ, float Y, float x, float y);

// -- color matrix utilities --
// returned matrices use LXTransform3D compatible layout,
// (i.e. transposed compared to OpenGL layout)

LXEXPORT void LXColorRGBtoXYZTransformMatrixFromChromaticities(LX4x4Matrix *mOut,
													  float xr, float xg, float xb,
													  float yr, float yg, float yb,
													  float RY, float GY, float BY );
                                                      
LXEXPORT void LXColorXYZtoRGBTransformMatrixFromChromaticities(LX4x4Matrix *mOut,
													  float xr, float xg, float xb,
													  float yr, float yg, float yb,
													  float RY, float GY, float BY );

LXEXPORT void LXColorConvertChromaticityWhitePoint_xy_to_RGB(float *rgbOut,
                                                      float xr, float xg, float xb, float xw,
                                                      float yr, float yg, float yb, float yw );

LXEXPORT void LXColorGetChromaticAdaptationMatrix(LX4x4Matrix *mOut,
                                                  LXColorXYZWhitePoint srcWhite, LXColorXYZWhitePoint dstWhite);
// ( more information on chromatic adaptation:
//   http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html )


LXEXPORT LXSuccess LXCopyICCProfileDataForColorSpaceEncoding(LXColorSpaceEncoding colorSpaceID, uint8_t **outData, size_t *outDataLen);


#ifdef __cplusplus
}
#endif

#endif  // _LXCOLORFUNCTIONS_H_

