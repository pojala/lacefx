/*
 *  LXFileHandlers.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 21.6.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXFILEHANDLERS_H_
#define _LXFILEHANDLERS_H_

#include "LXBasicTypes.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void *LXFilePtr;

// platform-independent file open functions
LXEXPORT LXSuccess LXOpenFileForReadingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile);
LXEXPORT LXSuccess LXOpenFileForWritingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile);

// exported wrappers for standard file functions
// (this is necessary because on Windows, FILE * may not be compatible across C runtimes).
// the use of the preceding underscore in function names follows the bad example set by
// _lx_malloc and others
LXEXPORT int        _lx_fclose(LXFilePtr file);
LXEXPORT size_t     _lx_fread(void * LXRESTRICT data, size_t len, size_t lenB, LXFilePtr LXRESTRICT file);
LXEXPORT size_t     _lx_fwrite(const void * LXRESTRICT data, size_t len, size_t lenB, LXFilePtr LXRESTRICT file);
LXEXPORT int        _lx_fseek64(LXFilePtr file, int64_t pos, int flags);
LXEXPORT int64_t    _lx_ftell64(LXFilePtr file);



// --- LXPixelBuffer file handler plugin API ---

#pragma pack (push, 4)

typedef struct {
    char *name;
    LXUInteger layerIndex;
} LXPixelBufferFileChannelDescription;

typedef struct {
    uint32_t version;  // set to 0 or 1 for this version
    
    const char *    (*getFileFormatID)(void *ctx);  // this must return an UTI-style format identifier (e.g. "public.png" or "com.xyz.foo-bar").
    
    void            (*copySupportedFileExtensions)(void *ctx, char ***outArray, size_t *outCount);  // outArray and all strings within it will be freed by the API using _lx_free()
    
    // this API supports complex formats that contain multiple channel sets or layers.
    // something like a z-buffer channel is expected to be interpreted as a layer with a single float channel.
    LXSuccess       (*openImageAtPath)(void *ctx, LXUnibuffer path, void **outImageRef, LXError *outError);
    void            (*closeImage)(void *ctx, void *imageRef);
    LXMapPtr        (*copyImageProperties)(void *ctx, void *imageRef);
    LXUInteger      (*getNumberOfChannels)(void *ctx, void *imageRef);
    LXUInteger      (*getNumberOfLayers)(void *ctx, void *imageRef);
    void            (*copyChannelDescriptions)(void *ctx, void *imageRef, LXPixelBufferFileChannelDescription *chDescs, const size_t chDescArraySize);
    void            (*copyLayerNames)(void *ctx, void *imageRef, LXUnibuffer *layerNames, const size_t layerNameArraySize);
    LXPixelBufferRef (*createPixelBufferForLayer)(void *ctx, void *imageRef, LXUInteger layerIndex, LXMapPtr properties, LXError *outError);
    
    // write support. no explicit support for layers (but they could be passed in 'properties' for specific write plugins).
    LXSuccess       (*writePixelBufferToFile)(void *ctx, LXPixelBufferRef pixbuf, LXUnibuffer path, LXMapPtr properties, LXError *outError);
    
} LXPixelBufferFileHandlerSuite_v1;

#pragma pack (pop)


LXEXPORT LXSuccess LXPixelBufferRegisterFileHandler(LXPixelBufferFileHandlerSuite_v1 *pluginSuite, void *ctx, LXError *outError);

LXEXPORT LXSuccess LXPixelBufferGetSuggestedFileHandlerForPath(LXUnibuffer path, LXPixelBufferFileHandlerSuite_v1 **outHandlerSuite, void **outHandlerCtx);

#ifdef __cplusplus
}
#endif

#endif
