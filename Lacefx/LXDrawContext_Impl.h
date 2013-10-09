/*
 *  LXDrawContext_Impl.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 15.4.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXDRAWCONTEXT_IMPL_H_
#define _LXDRAWCONTEXT_IMPL_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"
#include "LXRef_Impl.h"


typedef struct {
    LXREF_STRUCT_HEADER

    LXTextureArrayRef       texArray;
    LXShaderRef             shader;
    LXTransform3DRef        projTransform;
    LXTransform3DRef        mvTransform;
    
    LXUInteger              flags;
    
    LXBool                  shaderIsOwnedByUs;
} LXDrawContextImpl;



#endif