/*
 *  LXBinaryUtils.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 15.6.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXBinaryUtils.h"
#include <zlib.h>


LXSuccess LXSimpleDeflate(uint8_t *srcBuf, size_t srcLen,
                                   uint8_t *dstBuf, size_t dstLen,
                                   size_t *outCompressedLen)
{
    const int deflateQuality = Z_BEST_SPEED;  // was: 7
    int zret;
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));

    if (Z_OK != deflateInit(&stream, deflateQuality))
        return NO;
        
    stream.avail_in = srcLen;    
    stream.avail_out = dstLen;
    stream.next_in = (uint8_t *)srcBuf;    
    stream.next_out =  (uint8_t *)dstBuf;
    
    zret = deflate(&stream, Z_FINISH);
    
    //printf("did deflate pxbuf, inbytes %i --> outbytes %i\n", srcLen, stream.total_out);
        
    deflateEnd(&stream);
    
    *outCompressedLen = stream.total_out;    
    return (zret == Z_STREAM_END) ? YES : NO;
}
                                   
LXSuccess LXSimpleInflate(uint8_t *srcBuf, size_t srcLen,
                                   uint8_t *dstBuf, size_t dstLen,
                                   size_t *outDecompressedLen)
{
    int zret;
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));

    if (Z_OK != inflateInit(&stream))
        return NO;
        
    stream.avail_in = srcLen;    
    stream.avail_out = dstLen;
    stream.next_in = (uint8_t *)srcBuf;
    stream.next_out =  (uint8_t *)dstBuf;
    
    zret = inflate(&stream, Z_FINISH);
    
    ///printf("did inflate pxbuf, inbytes %i --> outbytes %i\n", srcLen, stream.total_out);
        
    inflateEnd(&stream);
    
    *outDecompressedLen = stream.total_out;    
    return (zret == Z_STREAM_END) ? YES : NO;
}

