/*
 *  LXPool_pixelbuffer_priv.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#ifndef _LXPOOL_PIXBUF_PRIV_H_
#define _LXPOOL_PIXBUF_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

LXPixelBufferRef LXPixelBufferPoolGet_(int w, int h, LXPixelFormat pf);
LXBool LXPixelBufferPoolStore_(LXPixelBufferRef pb);
void LXPixelBufferPoolPurge_();

#ifdef __cplusplus
}
#endif

#endif
