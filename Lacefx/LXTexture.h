/*
 *  LXTexture.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 19.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXTEXTURE_H_
#define _LXTEXTURE_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"



enum {
    kLXNearestSampling = 0,
    kLXLinearSampling = 1
};

enum {
    kLXWrapMode_ClampToEdge = 0,
    kLXWrapMode_ClampToBorder = 1,
    kLXWrapMode_Repeat = 16
};


#ifdef __cplusplus
extern "C" {
#endif


#pragma mark --- LXTexture public API methods ---

LXEXPORT const char *LXTextureTypeID();

LXEXPORT LXTextureRef LXTextureCreateWithData(uint32_t w, uint32_t h, LXPixelFormat pxFormat,
                                     uint8_t *buffer, size_t rowBytes,
                                     LXUInteger storageHint,
                                     LXError *outError);

LXEXPORT LXTextureRef LXTextureRetain(LXTextureRef texture);
LXEXPORT void LXTextureRelease(LXTextureRef texture);

LXEXPORT LXSize LXTextureGetSize(LXTextureRef texture);
LXEXPORT uint32_t LXTextureGetWidth(LXTextureRef texture);
LXEXPORT uint32_t LXTextureGetHeight(LXTextureRef texture);

// modifying sampling on the texture object is persistent.
// for temporarily changing the sampling mode used during a draw operation, LXTextureArray provides alternative methods.
LXEXPORT LXUInteger LXTextureGetSampling(LXTextureRef texture);
LXEXPORT void LXTextureSetSampling(LXTextureRef texture, LXUInteger samplingEnum);

LXEXPORT LXRect LXTextureGetImagePlaneRegion(LXTextureRef texture);
LXEXPORT void LXTextureSetImagePlaneRegion(LXTextureRef texture, LXRect region);

// the returned value needs to be cast to the native object type
// (e.g. a texture ID on OpenGL, or a IDirect3DTexture9 object on Direct3D)
LXEXPORT LXUnidentifiedNativeObj LXTextureLockPlatformNativeObj(LXTextureRef texture);
LXEXPORT void LXTextureUnlockPlatformNativeObj(LXTextureRef texture);

LXEXPORT LXSuccess LXTextureCopyContentsIntoPixelBuffer(LXTextureRef texture, LXPixelBufferRef pixBuf,
                                                        LXError *outError);

#ifdef __cplusplus
}
#endif

#endif


