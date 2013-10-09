/*
 *  LXPixelBuffer_iosimage.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#import "LXPixelBuffer.h"
#import "LXPixelBuffer_priv.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>


#include "LXPixelBuffer_applebase_inc.m"


LXPixelBufferRef LXPixelBufferCreateFromPathUsingNativeAPI_(LXUnibuffer unipath, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    return NULL;
}

LXPixelBufferRef LXPixelBufferCreateFromFileDataUsingNativeAPI_(const uint8_t *bytes, size_t len, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    return NULL;
}


LXSuccess LXPixelBufferWriteToPathUsingNativeAPI_(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXUInteger lxImageType, LXMapPtr properties, LXError *outError)
{
    if ( !pixbuf) return NO;

    return NO;
}
