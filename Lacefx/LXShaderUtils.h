/*
 *  LQShaderUtils.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 8.5.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSHADERUTILS_H_
#define _LXSHADERUTILS_H_

#include "LXBasicTypes.h"
#include "LXShader.h"

/*
  Some useful basic shaders.
  
  Returned shaders must be released with LXShaderRelease(), but they use the "final" storage hint
  so creation is lightweight (the GPU shader object is created only once and reused for subsequent calls).
*/


#ifdef __cplusplus
extern "C" {
#endif

// solid color shader: simply applies the color specified as shader parameter 0
LXEXPORT LXShaderRef LXCreateShader_SolidColor();

// takes two textures (A/B), and applies B's first channel as the mask for A.
// resulting image is premultiplied and clipped to 0-1.
LXEXPORT LXShaderRef LXCreateMaskShader_MaskWithRed();

// same as above, but uses the alpha channel from image B.
LXEXPORT LXShaderRef LXCreateMaskShader_MaskWithAlpha();

// same as above, but takes the alpha channel from shader param 0. (this is effectively just multiplying the image with the param value)
LXEXPORT LXShaderRef LXCreateMaskShader_Param();

// takes two textures (A/B), and composites B over A using "over" operation -- i.e. regular alpha blending.
// assumes B is premultiplied.
LXEXPORT LXShaderRef LXCreateCompositeShader_OverOp_Premult();
LXEXPORT LXShaderRef LXCreateCompositeShader_OverOp_Premult_Param();    // same, with shader param 0 controlling opacity
LXEXPORT LXShaderRef LXCreateCompositeShader_OverOp_Premult_MaskWithRed();     // same, with texture C's red channel controlling opacity

// unpremult versions
LXEXPORT LXShaderRef LXCreateCompositeShader_OverOp_Unpremult_Param();
LXEXPORT LXShaderRef LXCreateCompositeShader_OverOp_Unpremult_MaskWithRed();

// other blending modes
LXEXPORT LXShaderRef LXCreateCompositeShader_Add_Premult_Param();
LXEXPORT LXShaderRef LXCreateCompositeShader_Multiply_Premult_Param();


#ifdef __cplusplus
}
#endif

#endif
