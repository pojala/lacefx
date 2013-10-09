/*
 *  LXTexture_base.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 4.8.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

/*
 To be included in platform-specific implementation files;
 do not compile
 */


const char *LXTextureTypeID()
{
    static const char *s = "LXTexture";
    return s;
}

LXSize LXTextureGetSize(LXTextureRef r)
{
    if ( !r) return LXMakeSize(0, 0);
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    
    return LXMakeSize(imp->w, imp->h);
}

uint32_t LXTextureGetWidth(LXTextureRef r)
{
    if ( !r) return 0;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    return imp->w;
}

uint32_t LXTextureGetHeight(LXTextureRef r)
{
    if ( !r) return 0;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    return imp->h;
}

LXUInteger LXTextureGetSampling(LXTextureRef r)
{
    if ( !r) return 0;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    
    return imp->sampling;
}

void LXTextureSetSampling(LXTextureRef r, LXUInteger sv)
{
    if ( !r) return;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    
    imp->sampling = sv;
}

LXRect LXTextureGetImagePlaneRegion(LXTextureRef r)
{
    if ( !r) return LXZeroRect;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    
    return imp->imageRegion;
}

void LXTextureSetImagePlaneRegion(LXTextureRef r, LXRect region)
{
    if ( !r) return;
    LXTEXTUREIMPL *imp = (LXTEXTUREIMPL *)r;
    
    imp->imageRegion = region;
}

