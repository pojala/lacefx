/*
 *  LXSuites.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 19.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSUITES_H_
#define _LXSUITES_H_

#pragma pack (push, 4)

/*
 This file defines the C API for Lacefx plugins.
 
 These are image processing plugins with inputs, outputs, parameters --
 the stuff you find in a Conduit node, in other words.
*/


#include "LXBasicTypes.h"
#include "LXRefTypes.h"


enum {
	kLXNullConnection = 0,
    kLXScalarConnection = 1,
    kLXColorConnection = 2,
};
typedef LXUInteger LXConnectionType;

enum {
    kLXConnectionDataIsImage = 0,
    kLXConnectionDataIsVector,
};
typedef LXUInteger LXConnectionDataType;


// to ensure proper linking on Windows, we need a plugin-specific declaration of dllexport.
//
// all plugin entry points that need to be externally visible must be declared with LXPLUGIN_EXPORT.
// LXPLUGIN_PRIVATE can be used to explicitly prevent (non-static) functions from being exported.
//
#if defined(LXPLATFORM_WIN)
  #if !defined(LXPLUGIN_IMPORTING_DLL)
    #define LXPLUGIN_EXPORT __declspec(dllexport)
  #else
    #define LXPLUGIN_EXPORT __declspec(dllimport)
  #endif
  #define LXPLUGIN_PRIVATE
#elif defined(__APPLE__) || defined(LXPLUGIN_ENABLE_GCCVISIBILITYPATCH)
  #define LXPLUGIN_EXPORT __attribute__ ((visibility("default")))
  #define LXPLUGIN_PRIVATE __attribute__ ((visibility("hidden")))
#else
  #define LXPLUGIN_EXPORT
  #define LXPLUGIN_PRIVATE
#endif



#pragma mark --- plugin main entry points ---

// number of plugins in this binary; must be implemented
typedef uint32_t (*LXGetPluginCountFuncPtr)();

// plugin API version implemented by this binary; must be implemented
typedef void (*LXGetPluginAPIVersionFuncPtr)(LXVersion * /*version return from plugin*/);

// plugin main entry; must be implemented
typedef LXSuccess (*LXPluginEntryFuncPtr)(uint32_t /*pluginNumber*/,
                                          const char * /*selector*/, size_t /*selector strlen*/,
                                          void * /*input context*/, void * /*output context*/,
                                          LXError * /*error return from plugin*/);

// basic selectors; the in/out context parameters need to be cast to appropriate types.
// the use of these parameters in each selector is documented in more detail in the API guide.
// the selector names use simple decoration to indicate the in/out types.
//
#define kLXSel_init                 "init,_su_"          // inCtx is LXInitSuite *
#define kLXSel_finish               "finish,__"

#define kLXSel_getName              "name,_su_lo"          // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getPluginID          "pluginid,_su_lo"      // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getPluginType        "plugintype,_su_lo"    // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getCategoryName      "categoryname,_su_lo"  // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getDescription       "description,_su_lo"   // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getCopyright         "copyright,_su_lo"     // inCtx is LXHostSuite *, outCtx is LXLocString *
#define kLXSel_getVersion           "version,_su_ve"       // inCtx is LXHostSuite *, outCtx is LXVersion *
#define kLXSel_getIcon              "icon,_su_pb"          // inCtx is LXHostSuite *, outCtx is LXPixelBufferRef *

// for render plugins
#define kLXSel_preRender            "prerender,_su_"     // inCtx is LXPreRenderSuite *
#define kLXSel_render               "render,_su_"        // inCtx is LXRenderSuite *

// plugin type names (a plugin's getPluginType selector must return this)
#define kLXPluginTypeID_render      "lxPlugin::render"


#pragma mark --- LXInitSuite ---

typedef struct {
    void *              ctx;
    void *              (*getSuiteByName)(void *ctx, const char *suiteName);
    
    LXMapPtr            (*getPersistentMap)(void *ctx);
    LXMapPtr            (*getTransientMap)(void *ctx);

    LXSuccess           (*createInput)(void *ctx, const char *name, LXConnectionType type);
    LXSuccess           (*createOutput)(void *ctx, const char *name, LXConnectionType type);
    
    LXSuccess           (*createBoolParameter)(void *ctx, const char *name, LXBool defaultValue);
    LXSuccess           (*createDoubleParameter)(void *ctx, const char *name, double defaultValue, double sliderMin, double sliderMax);
    LXSuccess           (*createEnumParameter)(void *ctx, const char *name, LXUInteger defaultItem, LXUInteger itemCount, const char **itemNames);
    LXSuccess           (*createRGBAParameter)(void *ctx, const char *name, LXRGBA defaultValue);
    
    // the following function is for future extension
    LXSuccess           (*createParameterWithNamedType)(void *ctx, const char *name, const char *paramTypeName, void *creationProperties);
    
    void *_reserved[32];
} LXInitSuite_v1;


#pragma mark --- LXHostSuite ---

#define kLXHostSuiteName        "lxSuite::host"

#define LXDECLSUITE_HOST(s_)    LXHostSuite_v1 *hostSuite = ((s_)->getSuiteByName) ? ((LXHostSuite_v1 *) (s_)->getSuiteByName((s_)->ctx, kLXHostSuiteName)) : NULL;

typedef struct {
    void *          ctx;
    const char *    (*getHostName)(void *ctx);
    LXVersion       (*getHostVersion)(void *ctx);
    
    LXBool          (*canProvideSuiteNamed)(void *ctx, const char *suiteName);

    // these calls return absolute paths to directories within the plugin bundle's root (e.g. "Contents/MacOS" and "Contents/Resources").
    // returned strings are owned by the API and must not be freed.
    const char *    (*getPluginExecutablePathUTF8)(void *ctx);
    const char *    (*getPluginResourcesPathUTF8)(void *ctx);
    
    void *_reserved[32];
} LXHostSuite_v1;



#pragma mark --- LXInputSuite ---

typedef struct {
    void *          ctx;
    LXBool          (*isInputConnected)(void *ctx, const char *inputName);
    LXBool          (*isParameterConnected)(void *ctx, const char *paramName);
    
    LXBool          (*getParameterBoolValue)(void *ctx, const char *paramName);
    double          (*getParameterDoubleValue)(void *ctx, const char *paramName);
    LXUInteger      (*getParameterEnumValue)(void *ctx, const char *paramName);
    LXRGBA          (*getParameterRGBAValue)(void *ctx, const char *paramName);
    LXSuccess       (*getParameterPtrValue)(void *ctx, const char *paramName, void *outValue);

    LXInteger       (*getInputCount)(void *ctx);
    const char *    (*getNameOfInputAtIndex)(void *ctx, LXInteger inputIndex);

    LXInteger       (*getParameterCount)(void *ctx);
    const char *    (*getNameOfParameterAtIndex)(void *ctx, LXInteger paramIndex);

    LXConnectionDataType (*getInputDataType)(void *ctx, const char *inputName);
    LXConnectionDataType (*getParameterDataType)(void *ctx, const char *paramName);

    void *_reserved[32];
} LXInputSuite_v1;


#pragma mark --- LXPreRenderSuite ---

typedef struct {
    void *              ctx;
    void                (*setError)(void *ctx, LXError *error);    
    void *              (*getSuiteByName)(void *ctx, const char *suiteName);
    LXInputSuite_v1     *inputSuite;
    
    LXPoolRef           (*getPool)(void *ctx, void *poolSpec);  // poolSpec is currently unused
    LXMapPtr            (*getPersistentMap)(void *ctx);
    LXMapPtr            (*getTransientMap)(void *ctx);
    
    LXSize              (*getInputSize)(void *ctx);
    LXBool              (*isPlaybackContinuous)(void *ctx, double *outFrameDeltaInSeconds_optional);

    void                (*setIsNoOp)(void *ctx, LXBool isNoOp);
    ///void                (*setOutputIsUnchanged)();
    
    void                (*setOutputSize)(void *ctx, LXSize size);
    LXMapPtr            (*getOutputProperties)(void *ctx);

    void *_reserved[32];
} LXPreRenderSuite_v1;


#pragma mark --- LXRenderSuite ---

typedef struct {
    void *              ctx;
    void                (*setError)(void *ctx, LXError *error);
    void *              (*getSuiteByName)(void *ctx, const char *suiteName);
    LXInputSuite_v1     *inputSuite;

    LXPoolRef           (*getPool)(void *ctx, void *poolSpec);  // poolSpec is currently unused
    LXMapPtr            (*getPersistentMap)(void *ctx);
    LXMapPtr            (*getTransientMap)(void *ctx);
    
    LXInteger           (*getActiveOutputIndex)(void *ctx);
    LXSurfaceRef        (*getActiveOutputSurface)(void *ctx);
    LXMapPtr            (*getOutputProperties)(void *ctx);

    LXBool              (*isInputConstantForIndex)(void *ctx, LXInteger index);
    LXTextureRef        (*getInputTextureByIndex)(void *ctx, LXInteger index);
    LXRGBA              (*getConstantInputValueByIndex)(void *ctx, LXInteger index);

    void *_reserved[32];
} LXRenderSuite_v1;


#pragma pack(pop)

#endif
