/*
 *  LXConvolver_lx.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 25.8.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXRef_Impl.h"
#import "LXSurface.h"
#import "LXPlatform.h"
#import "LXConvolver.h"
#import "LXDraw_Impl.h"
#import "LXStringUtils.h"

#import "LQLXConvolver.h"


#if !defined(LXPLATFORM_WIN)
extern LXSuccess LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(LXUInteger flags, double timeOut, NSString *caller, NSDictionary **errorInfo);
extern NSString *LXSurfaceBuildThreadAccessErrorString(NSDictionary *errorInfo);
#endif


// this private stub class acts as the delegate for the LQLXConvolver
@interface LXConvolverObjCInner : NSObject {
    @public
        LXConvolverRef owner;
        LXUInteger samplingType;
        LXTextureRef texture;
        LXRect outputRect;
        double sampleDistance;
        double sampleRotation;
        BOOL doNormalize;
        
        LXConvolverCallbacks_v1 callbacks;
        void *callbackUserData;
}
@end


@implementation LXConvolverObjCInner

- (void)generateConvolutionWeights:(LQLXConvolver *)conv inDoubleArray:(double *)array totalTaps:(LXInteger)width
{
    if (callbacks.generateWeights) {
        callbacks.generateWeights(owner, array, width, callbackUserData);
    } else {
        // fill with linear values
        double v = 1.0; // / width;
        int i;
        for (i = 0; i < width; i++)
            array[i] = v;
    }
}

- (BOOL)shouldNormalizeConvolutionWeights:(LQLXConvolver *)conv {
    return doNormalize;
}

- (BOOL)useNearestNeighborSamplingForConvolution:(LQLXConvolver *)conv {
    ///NSLog(@"%s -- sampling is nearest %i", __func__, (samplingType == kLXNearestSampling));
    return (samplingType == kLXNearestSampling) ? YES : NO;
}


- (double)convolutionSampleOffsetMultiplier:(LQLXConvolver *)conv {
    return (fabs(sampleDistance) > 0.0) ? sampleDistance : 1.0; }

- (double)convolutionSampleOffsetRotation:(LQLXConvolver *)conv {
    return sampleRotation; }


- (NSString *)combinerARBfpInstructionForConvolution:(LQLXConvolver *)conv
{
    if (callbacks.getCustomCombinerInstruction) {
        char str[32];
        memset(str, 0, 32);
        callbacks.getCustomCombinerInstruction(owner, str, 31, callbackUserData);
        
        return (strlen(str) > 0) ? [NSString stringWithUTF8String:str] : nil;
    }
    else
        return nil;
}

- (NSString *)finishingARBfpInstructionForConvolution:(LQLXConvolver *)conv sourceVar:(NSString *)v
{
    if (callbacks.getCustomFinisherInstruction) {
        char str[256];
        memset(str, 0, 256);
        
        const char *varStr = (v) ? [v UTF8String] : NULL;
        
        callbacks.getCustomFinisherInstruction(owner, str, 255,  varStr, (varStr) ? strlen(varStr) : 0, callbackUserData);
        
        return (strlen(str) > 0) ? [NSString stringWithUTF8String:str] : nil;
    }
    else
        return nil;
}

@end



typedef struct {
    LXREF_STRUCT_HEADER
    
    LXConvolverObjCInner *inner;
    
    LQLXConvolver *lxConv;
} LXConvolverImpl;



LXConvolverRef LXConvolverRetain(LXConvolverRef r)
{
    if ( !r) return NULL;
    LXConvolverImpl *imp = (LXConvolverImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXConvolverRelease(LXConvolverRef r)
{
    if ( !r) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        [imp->lxConv release];
        imp->lxConv = nil;
            
        [imp->inner release];
        imp->inner = nil;
        
        _lx_free(imp);
    }
}

const char *LXConvolverTypeID()
{
    static const char *s = "LXConvolver";
    return s;
}

LXConvolverRef LXConvolverCreate(LXPoolRef pool, LXUInteger flags, LXError *outError)
{
    LXConvolverImpl *imp = _lx_calloc(sizeof(LXConvolverImpl), 1);
    LXREF_INIT(imp, LXConvolverTypeID(), LXConvolverRetain, LXConvolverRelease);
    
    imp->inner = [[LXConvolverObjCInner alloc] init];
    imp->lxConv = [[LQLXConvolver alloc] init];
    
    [imp->lxConv setDelegate:imp->inner];
    
    imp->inner->owner = (LXConvolverRef)imp;
    imp->inner->doNormalize = YES;
    
    return (LXConvolverRef)imp;
}

void LXConvolverSetCallbacks(LXConvolverRef conv, LXConvolverCallbacks_v1 *callbacks, void *userData)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;
    
    memcpy(&(imp->inner->callbacks), callbacks, sizeof(LXConvolverCallbacks_v1));
    
    imp->inner->callbackUserData = userData;
}

void LXConvolverSetSampling(LXConvolverRef conv, LXUInteger samplingEnum)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->samplingType = samplingEnum;
}

void LXConvolverSetSourceTexture(LXConvolverRef conv, LXTextureRef texture)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->texture = texture;
}

void LXConvolverSetOutputRect(LXConvolverRef conv, LXRect rect)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->outputRect = rect;
}

void LXConvolverSetSampleDistance(LXConvolverRef conv, LXFloat sampleDistance)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->sampleDistance = sampleDistance;
}

void LXConvolverSetSampleRotation(LXConvolverRef conv, LXFloat sampleRot)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->sampleRotation = sampleRot;
}

void LXConvolverSetNormalizesWeights(LXConvolverRef conv, LXBool doNormalize)
{
    if ( !conv) return;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;

    imp->inner->doNormalize = doNormalize;
}

LXSurfaceRef LXConvolverRender1DWithWidthUsingSurfaces(LXConvolverRef conv,
                                                              LXInteger width,
                                                              LXSurfaceRef surface1,
                                                              LXSurfaceRef surface2,
                                                              int *outPassCount,  // number of passes rendered is returned here, can be NULL
                                                              LXError *outError)
{
    if ( !conv) return NULL;
    LXConvolverImpl *imp = (LXConvolverImpl *)conv;
    LXSurfaceRef resultSurf;
    
    if ( !imp->inner->texture) NSLog(@"*** %s: no texture", __func__);
    
#if defined(LXPLATFORM_WIN)
    BOOL didLock = LXSurfaceBeginAccessOnThread(0);
#else
    NSDictionary *lockErrorDict = nil;
    //DTIME(t0)
    BOOL didLock = LXSurfaceBeginAccessOnThreadWithTimeOutAndNSCaller_(0, 200.0/1000.0,
                                                                       [NSString stringWithFormat:@"LXConvolverRender1D(%p)", conv],
                                                                       &lockErrorDict);
    if ( !didLock) {
        NSString *traceStr = LXSurfaceBuildThreadAccessErrorString(lockErrorDict);
        LXPrintf("*** LXConvolverRender1D: failed to get lock; trace: %s\n", [traceStr UTF8String]);
        return NULL;
    }
#endif
    
    [imp->lxConv setSourceLXTexture:imp->inner->texture];

    // actual rendering happens here.
    // the lxConvolver calls its delegate (our "inner object") for all the other stuff
    resultSurf = [imp->lxConv render1DConvolutionWithWidth:width
                                                    destSurface1:surface1
                                                    destSurface2:surface2];
    

#if 0
    // testing:
    LXSurfaceCopyTexture(surface1, imp->inner->texture, NULL);
    LXSurfaceFlush(surface1);
    resultSurf = surface1;
#endif

    
    if (didLock) LXSurfaceEndAccessOnThread();
    
    // TODO: outPassCount isn't actually filled
    
    return resultSurf;
}


