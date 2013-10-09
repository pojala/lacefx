/*
 *  LXPool_surface_priv.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 11.10.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#pragma pack (push, 4)
typedef struct {
    uint32_t version;  // set to 0
    
    LXSurfaceRef    (*getSurface)(LXPoolRef pool, uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint32_t flags, void *userData);
    
    void            (*returnSurface)(LXPoolRef pool, LXSurfaceRef surface, void *userData);
    
    void            (*poolWillBeDestroyed)(LXPoolRef pool, void *userData);
    
} LXSurfacePoolCallbacks;
#pragma pack (pop)



#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT void LXPoolSetSurfacePoolCallbacks_(LXPoolRef pool, LXSurfacePoolCallbacks *callbacks, void *userData);

LXEXPORT LXSurfaceRef LXPoolGetSurface_(LXPoolRef pool, uint32_t w, uint32_t h, LXPixelFormat pxFormat, uint32_t flags);

LXEXPORT void LXPoolReturnSurface_(LXPoolRef pool, LXSurfaceRef surface);


#ifdef __cplusplus
}
#endif