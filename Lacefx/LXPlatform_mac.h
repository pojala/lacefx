/*
 *  LXPlatform_mac.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 8.9.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#ifndef _LXPLATFORM_MAC_H_
#define _LXPLATFORM_MAC_H_

#include "LXBasicTypes.h"
#include "LXPlatform.h"
#include "LXTexture.h"
#include "LXSurface.h"
#include "LXShader.h"
#include "LXPixelBuffer.h"


#ifdef __cplusplus
extern "C" {
#endif


// --- public API ---

LXEXPORT LXSuccess LXPlatformInitialize();      // uses the primary screen GPU device
LXEXPORT void LXPlatformDeinitialize();

LXEXPORT void LXPlatformDoPeriodicCleanUp();

// a plugin that needs to use GL objects from the host app (e.g. the Conduit FxPlug)
// must manage the shared GL context itself by wrapping it into an LQGLContext object.
// (see the FxPlug implementation for specifics)
//
void LXPlatformSetSharedLQGLContext(void *obj);  // argument is an LQGLContext object


// --- implementation API ---

// these may be needed by implementations that render using host-given GL objects, i.e. FxPlug.

LXEXPORT void LXTextureOpenGL_EnableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData);
LXEXPORT void LXTextureOpenGL_DisableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData);

LXEXPORT LXTextureRef LXTextureCreateWrapperForGLTexture_(unsigned int glTexID, int w, int h, LXPixelFormat pxFormat);


// --- other API missing on Windows ---

LXEXPORT LXInteger LXPlatformGetHWAmountOfMemory();

LXEXPORT const char *LXPlatformRendererNameForDisplayIdentifier(LXInteger dmask);
    

#ifdef __cplusplus
}
#endif



#endif // _LXPLATFORM_MAC_H_
