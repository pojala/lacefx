/*
 *  LXPlatform_ios.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/5/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXPLATFORM_IOS_H_
#define _LXPLATFORM_IOS_H_

#include "LXBasicTypes.h"
#include "LXPlatform.h"


#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT LXSuccess LXPlatformInitialize();
LXEXPORT void LXPlatformDeinitialize();

LXEXPORT void LXPlatformDoPeriodicCleanUp();

void LXPlatformSetSharedEAGLContext(void *obj);  // argument is an EAGLContext object


#ifdef __cplusplus
}
#endif



#endif // _LXPLATFORM_MAC_H_
