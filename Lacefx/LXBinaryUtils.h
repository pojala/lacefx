/*
 *  LXBinaryUtils.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 15.6.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSHADERUTILS_H_
#define _LXSHADERUTILS_H_

#include "LXBasicTypes.h"


#ifdef __cplusplus
extern "C" {
#endif


// very minimalist zlib compression functions.
// the maximum size of the compressed/decompressed data needs to be known beforehand.

LXEXPORT LXSuccess LXSimpleDeflate(uint8_t *srcBuf, size_t srcLen,
                                   uint8_t *dstBuf, size_t dstLen,
                                   size_t *outCompressedLen);
                                   
LXEXPORT LXSuccess LXSimpleInflate(uint8_t *srcBuf, size_t srcLen,
                                   uint8_t *dstBuf, size_t dstLen,
                                   size_t *outDecompressedLen);


#ifdef __cplusplus
}
#endif

#endif
