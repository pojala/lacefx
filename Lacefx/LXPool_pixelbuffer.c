/*
 *  LXPool_pixelbuffer.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.8.2007.
 *  Copyright 2007 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPool.h"
#include "LXPixelBuffer.h"


/*
  this file was supposed to implement the pooling mechanism for pixel buffers.
  these functions are private and only used by LXPool/PXPixelBuffer implementation.
 
  !! this functionality was never actually needed, so no implementation !!
*/

LXPixelBufferRef LXPixelBufferPoolGet_(int w, int h, LXPixelFormat pf)
{
    return NULL;
}

LXBool LXPixelBufferPoolStore_(LXPixelBufferRef pb)
{
    return NO;
}

void LXPixelBufferPoolPurge_()
{
}

