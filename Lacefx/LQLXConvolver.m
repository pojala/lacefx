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

#import "LQLXConvolver.h"
#import "LXBasicTypeFunctions.h"
#import "LXStringUtils.h"
#import "LXTextureArray.h"
#import "LXSurface.h"
#import "LXTexture.h"

#if defined(LXPLATFORM_WIN)
#import "LXPlatform_d3d.h"
#endif


#if 0
#define DEBUGLOG(format, args...)   NSLog(@"%s:  %@", __func__, [NSString stringWithFormat:format , ## args]);
#else
#define DEBUGLOG(format, args...)
#endif


@implementation LQLXConvolver


- (id)init
{
    self = [super init];
    
    return self;
}

- (void)dealloc
{
    [_offsetInfos release];
    
    LXShaderRelease(_firstPassShader);
    LXShaderRelease(_fullPassShader);
    LXShaderRelease(_endPassShader);

    _lx_free(_weights);

    [super dealloc];
}


- (void)setDelegate:(id)delegate {
    _delegate = delegate; }

- (void)invalidateCache
{
    _weightsCount = 0;
    
    _lx_free(_weights);
    _weights = nil;
}

- (void)setSourceLXTexture:(LXTextureRef)tex {
    _sourceTex = tex;
}
    
- (LXTextureRef)sourceLXTexture {
    return _sourceTex; }



#pragma mark --- LX rendering ---

- (LXShaderRef)createShaderForTapCount:(int)tapCount
                        useAccumulationTexture:(BOOL)useAcc
{
    NSMutableString *prog = [NSMutableString stringWithString:@"!!ARBfp1.0  TEMP tcoord, offc, v, acc;\n"];
    int i;
    
    // get weight & offset parameters
    for (i = 0; i < tapCount; i++) {
        NSString *s = [NSString stringWithFormat:@"PARAM w%i = program.local[%i];\n", i, i];
        [prog appendString:s];
    }
    
    // get base texture coordinate
    [prog appendString://@"PARAM base = { 0.5, 0.5, 0.5, 1.0 };\n"
                       @"MOV tcoord, fragment.texcoord[0];\n"
                    ];
    
    // do first tap
    if (!useAcc)
        [prog appendString:@"TEX v, tcoord, texture[0], RECT;\n"];
    else {
        [prog appendString:@"TEX v, fragment.texcoord[1], texture[1], RECT;\n"];
        //[prog appendString:@"MOV acc, base;"];
    }

    BOOL delegateDoesCombiner = [_delegate respondsToSelector:@selector(combinerARBfpInstructionForConvolution:)];
    BOOL delegateDoesWeights =  [_delegate respondsToSelector:@selector(generateConvolutionWeights:inArray:totalTaps:)];
    BOOL delegateDoesFinish =   [_delegate respondsToSelector:@selector(finishingARBfpInstructionForConvolution:sourceVar:)];

    NSString *combinerInstr = (delegateDoesCombiner) ? [_delegate combinerARBfpInstructionForConvolution:self] : nil;
    if (delegateDoesCombiner && [combinerInstr length] < 1)
        delegateDoesCombiner = NO;

    DEBUGLOG(@"delegate combiner %i ('%@') / %i / %i (is: %@)", delegateDoesCombiner, combinerInstr, delegateDoesWeights, delegateDoesFinish, _delegate);

    if ( !delegateDoesCombiner) {
        [prog appendString:@"MUL acc, v, w0.z;\n"];  // if other combiner than MAD is used, assume that the weights don't matter
                                                   // (in practice, the combiner instruction can be MIN/MAX/etc.)
    } else {
        [prog appendString:@"MOV acc, v;\n"];
    }

    // do additional taps
    for (i = 1; i < tapCount; i++) {
        NSString *s = [NSString stringWithFormat:
                        @"ADD offc, tcoord, w%i;\n", i];  // this creates the offset texcoords for this tap
                        
        NSString *s2 = @"TEX v, offc, texture[0], RECT;\n";
        NSString *s3;
        
        if ( !delegateDoesCombiner) {
            s3 = [NSString stringWithFormat:@"MAD acc, v, w%i.z, acc;\n", i];
        } else {
            //NSString *combinerInstr = [_delegate combinerARBfpInstructionForConvolution:self];
            
            if ( !delegateDoesWeights)
                s3 = [NSString stringWithFormat:@"%@ acc, v, acc;\n", combinerInstr];
            else
                s3 = [NSString stringWithFormat:@"MUL v, v, w%i.z;  \n%@ acc, v, acc;\n", i, combinerInstr];
        }
                        
        [prog appendString:s];
        [prog appendString:s2];
        [prog appendString:s3];
    }
   
    // finish up
    NSString *delFinishStr = (delegateDoesFinish) ? [_delegate finishingARBfpInstructionForConvolution:self sourceVar:@"acc"] : nil;
    if ( !delFinishStr || [delFinishStr length] < 1)
        [prog appendString:@"MOV result.color, acc;  END"];
    else {
        [prog appendString:delFinishStr];
        [prog appendString:@"END"];
    }
    
    //if (useAcc) { DEBUGLOG(@"acc program with %i taps is: %@", tapCount, prog); }
    /*if (useAcc) {
        prog = [NSMutableString stringWithString:@"!!ARBfp1.0  TEMP tcoord, offc, v, acc;"];
        [prog appendString:@"TEX v, fragment.texcoord[0], texture[0], RECT;  "];
        [prog appendString:@"MOV result.color, v;  END"];
        DEBUGLOG(@"temp acc program with %i taps is: %@", tapCount, prog);
    }*/
    
    
    LXDECLERROR(lxErr)
    const char *cstring = [prog UTF8String];
    LXShaderRef shader = LXShaderCreateWithString(cstring, strlen(cstring), kLXShaderFormat_OpenGLARBfp, 0, &lxErr);
    if ( !shader) {
        LXPrintf("*** %s: error creating shader (%i, %s)\n", __func__, lxErr.errorID, lxErr.description);
        LXErrorDestroyOnStack(lxErr);
    }
    
    return shader;
}



- (void)generateOffsetsIntoArray:(LXFloat *)offsetsAndWeights
            tapCount:(int)numTaps previousTaps:(int)prevTaps
            xCoeff:(double)xc yCoeff:(double)yc
{
    int c, i;
    int coff = (prevTaps == 0 || prevTaps % 2 != 0) ? (prevTaps/2 + 1) : (prevTaps/2);

    BOOL doNormalize = YES;
    if ([_delegate respondsToSelector:@selector(shouldNormalizeConvolutionWeights:)]) {
        doNormalize = [_delegate shouldNormalizeConvolutionWeights:self];
    } else {
        // this was the old behavior; it doesn't make sense of course, but there may be something in Conduit that relies on it
        doNormalize = ! ([_delegate respondsToSelector:@selector(combinerARBfpInstructionForConvolution:)]);
    }
    
    offsetsAndWeights[0] = offsetsAndWeights[1] = offsetsAndWeights[3] = 0.0;
    
    // weight for center tap
    double cw = 0.0;
    if (prevTaps > 0) {
        for (i = 0; i < prevTaps; i++) {
            cw += _weights[(_weightsCount - prevTaps)/2 + i];
        }
    }
    else  // prevTaps is 0
        cw = _weights[_weightsCount/2];//1.0f;
    
    DEBUGLOG(@"numtaps %i, prevtaps %i, center weight %f, coff %i, numweights %i", numTaps, prevTaps, cw, coff, _weightsCount);
    
    offsetsAndWeights[2] = cw;
        
    // weights & offsets for other taps
    c = 4;
    for (i = 0; i < numTaps / 2; i++) {
        double xoff, yoff;
        xoff = (double)(coff + i) * xc;
        yoff = (double)(coff + i) * yc;
        int leftWOffset = _weightsCount/2 - coff - i;
        int rightWOffset = _weightsCount/2 + coff + i;
        
        if (leftWOffset < 0) {
            NSLog(@"*** left w offset out of bounds: %i", leftWOffset);
            leftWOffset = 0;
        }
        if (rightWOffset > _weightsCount-1) {
            NSLog(@"*** right w offset out of bounds: %i", rightWOffset);
            rightWOffset = _weightsCount-1;
        }
        
        ///NSLog(@"    ..prevtaps %i -- %i, leftW %i, rightW %i", prevTaps, i, leftWOffset, rightWOffset);
        
        offsetsAndWeights[c]   = xoff;     // xoffset
        offsetsAndWeights[c+1] = yoff;     // y offset
        offsetsAndWeights[c+2] = _weights[leftWOffset];
        offsetsAndWeights[c+3] = 0.0;     // not used
        
        offsetsAndWeights[c+4] = -xoff;    // xoffset
        offsetsAndWeights[c+5] = -yoff;     // y offset
        offsetsAndWeights[c+6] = _weights[rightWOffset];
        offsetsAndWeights[c+7] = 0.0;     // not used
        
        c += 8;
    }
    
    if (doNormalize) {
        // calculate sum of weights
        double sum = 0.0;
        for (i = 0; i < numTaps; i++)
            sum += offsetsAndWeights[i*4 + 2];

        // normalize weights (divide by their total sum)
        double m = (sum > 0.0) ? (1.0 / sum) : 1.0;
        for (i = 0; i < numTaps; i++)
            offsetsAndWeights[i*4 + 2] *= m;
    }
}


static LXFloat roundNearlyIntegralFloat(LXFloat f)
{
	LXFloat rf = round(f);
    if (fabs(f - rf) < 0.001)
        f = rf;
	return f;
}

static int convolutionWidthForAmount(LXFloat am)
{
    int roundedAmount = ceil(am);       /// 1 + floor((1.0 + am) * 0.5) * 2;
    while (roundedAmount % 2 != 1 || roundedAmount == 0)
        roundedAmount++;
	return roundedAmount;
}


extern int LXTextureGetGLTexName_(LXTextureRef r);


- (LXSurfaceRef)render1DConvolutionWithWidth:(LXFloat)am
                                            destSurface1:(LXSurfaceRef)surf1
                                            destSurface2:(LXSurfaceRef)surf2
{
    if ( !_sourceTex) {
        NSLog(@"*** %s: no source texture", __func__);
        return NULL;
    }
    
	am = roundNearlyIntegralFloat(am);

    int i;
    int roundedAmount = convolutionWidthForAmount(am);    
    int totalTaps;
    LXFloat offsetScale;
    
    if ([_delegate respondsToSelector:@selector(useNearestNeighborSamplingForConvolution:)])
        _useNearestSampling = [_delegate useNearestNeighborSamplingForConvolution:self];
    
    int treshold = 16;
    if ([_delegate respondsToSelector:@selector(convolutionSpreadTreshold:)])
        treshold = [_delegate convolutionSpreadTreshold:self];
    
    if (roundedAmount <= treshold) {
        totalTaps = roundedAmount;
        offsetScale = 1.0;
    } else {
        double tapScale = 1.0;
        if ([_delegate respondsToSelector:@selector(convolutionSpreadMultiplier:)])
            tapScale = [_delegate convolutionSpreadMultiplier:self];
        
        totalTaps = treshold + (double)(roundedAmount - treshold) / tapScale;
        offsetScale = (double)roundedAmount / (double)totalTaps;
    }
    
    if ([_delegate respondsToSelector:@selector(convolutionSampleOffsetMultiplier:)])
        offsetScale *= [_delegate convolutionSampleOffsetMultiplier:self];

    DEBUGLOG(@"conv offset scale: %f (treshold %i),  taps %i, am %i", offsetScale, treshold, totalTaps, roundedAmount);
    
    // create kernel weights if necessary
    BOOL kernelIsDirty = NO;
    if (_weights == nil || _weightsCount != totalTaps) {
        kernelIsDirty = YES;
        free(_weights);
        _weights = _lx_malloc(totalTaps * sizeof(double) + 32);  // some extra space just in case
        _weightsCount = totalTaps;

        if (totalTaps % 2 != 1)
            NSLog(@"** warning: convolution samplecount should be odd (is %i)", totalTaps);

        if ([_delegate respondsToSelector:@selector(generateConvolutionWeights:inDoubleArray:totalTaps:)]) {
            [_delegate generateConvolutionWeights:self inDoubleArray:_weights totalTaps:totalTaps];
        }
        else {
            // delegate doesn't do the weights, so generate weights for box blur
            for (i = 0; i < totalTaps; i++) {
                _weights[i] = 1.0;
            }
            /////_weights[0] = 1.0f - 0.5f*((totalTaps-1)-am);
        }
        
        // TODO: should scale weights according to fraction of "am", so we can have fractional convolution widths
    }


    // calculate number of passes required
    int maxTapsPerPass = 21;
    
#if defined(LXPLATFORM_WIN)
    const char *psClass = LXPlatformGetD3DPixelShaderClass();
    if (strlen(psClass) < 4 || psClass[3] < '3') {
        maxTapsPerPass = 12;
    }
#endif
    
    int numPasses = ceil((double)totalTaps / (double)maxTapsPerPass);
    
    DEBUGLOG(@"--- rendering convolution (self %p), numpasses %i (taps %i)", self, numPasses, totalTaps);
    
    if (numPasses > 1) {
        NSAssert1(surf2, @"no second drawable provided (for passcount %i)", numPasses);
    }
    
    // create sample offsets
    if (kernelIsDirty || !_offsetInfos || (numPasses != [_offsetInfos count])) {
        double direction = 0.0;
        if ([_delegate respondsToSelector:@selector(convolutionSampleOffsetRotation:)])
            direction = [_delegate convolutionSampleOffsetRotation:self];

        double drad = direction * ( M_PI / 180.0 );
        double xc = sin(M_PI * 0.5 - drad);
        double yc = sin(drad);
        
        DEBUGLOG(@"generating conv sample offsets; rotation %f", direction);

        if (!_offsetInfos) {
            _offsetInfos = [[NSMutableArray arrayWithCapacity:30] retain];
        } else {
            [_offsetInfos removeAllObjects];
        }
        
        for (i = 0; i < numPasses; i++) {
            int tapCount;
            
            if (i == 0) {                       // first pass
                tapCount = (numPasses > 1) ? maxTapsPerPass : totalTaps;
                                    
                LXShaderRelease(_firstPassShader);
                
                _firstPassShader = [self createShaderForTapCount:tapCount
                                            useAccumulationTexture:NO];
                
            }
            else if (i < numPasses-1) {         // full pass
                tapCount = maxTapsPerPass;
                
                LXShaderRelease(_fullPassShader);
                
                _fullPassShader = [self createShaderForTapCount:tapCount
                                            useAccumulationTexture:YES];
            }
            else {                              // last pass of multipass
                tapCount = totalTaps - i * maxTapsPerPass;
                
                LXShaderRelease(_endPassShader);
                
                _endPassShader = [self createShaderForTapCount:tapCount
                                            useAccumulationTexture:YES];
            }

            NSMutableData *offData = [NSMutableData dataWithLength:(tapCount * 4 * sizeof(LXFloat)) + 32];
            
            [self generateOffsetsIntoArray:(LXFloat *)[offData mutableBytes]
                                 tapCount:tapCount
                                 previousTaps:i * maxTapsPerPass
                                 xCoeff:xc * offsetScale
                                 yCoeff:yc * offsetScale
                                 ];
            /*
            if (offsetScale != 1.0) {
                int j;
                for (j = 0; j < tapCount; j++) {
                    NSLog(@"offsets: %i  -- %f, %f (w %f)", j, bytes[j*4], bytes[j*4+1], bytes[j*4+2]);
                }
            }
            */
            
            [_offsetInfos addObject:[NSDictionary dictionaryWithObjectsAndKeys:offData, @"offsetData",
                                                                               [NSNumber numberWithInt:tapCount], @"tapCount",
                                                                               nil]];
        }
    }
    
    LXSurfaceRef currentD = surf1;
    LXSurfaceRef nextD = surf2;
    LXSurfaceRef accD = NULL;
    const LXSize surfSize = LXSurfaceGetSize(surf1);
    ///const LXSize texSize = LXTextureGetSize(_sourceTex);
    
    LXDrawContextRef drawCtx = LXDrawContextCreate();

    LXVertexXYUV vertices[4];

    for (i = 0; i < numPasses; i++) {
        id offsetInfo = [_offsetInfos objectAtIndex:i];
        int tapCount = [[offsetInfo objectForKey:@"tapCount"] intValue];
        NSMutableData *offData = (NSMutableData *)[offsetInfo objectForKey:@"offsetData"];        
        LXFloat *offsetsArray = (LXFloat *)[offData mutableBytes];
        
        LXShaderRef shader = (i == 0) ? _firstPassShader : ((i < numPasses-1) ? _fullPassShader : _endPassShader);
        
        LXTextureRef accTex = (accD) ? LXSurfaceGetTexture(accD) : NULL;
        LXTextureArrayRef texArray = (accD) ? LXTextureArrayCreateWithTextures(2, _sourceTex, accTex)
                                            : LXTextureArrayCreateWithTexture(_sourceTex);
        
        ///NSLog(@"texture sampling for conv: is nearest %i, tapcount %i", _useNearestSampling, tapCount);
        
        LXTextureArraySetSamplingAt(texArray, 0, (_useNearestSampling) ? kLXNearestSampling : kLXLinearSampling);
        LXTextureArraySetSamplingAt(texArray, 1, (_useNearestSampling) ? kLXNearestSampling : kLXLinearSampling);
        
        LXTextureArraySetWrapModeAt(texArray, 0, kLXWrapMode_ClampToEdge);
        LXTextureArraySetWrapModeAt(texArray, 1, kLXWrapMode_ClampToEdge);

        LXDrawContextSetTextureArray(drawCtx, texArray);
        
        /*LXInteger texCount = LXDrawContextGetTextureCount(drawCtx);
        DEBUGLOG(@"---- rendering conv pass %i into %p, acc %p (tex %p); shader %p; texCount %i (%i)", i, currentD, accD, accTex,
                                shader, texCount, LXTextureArrayInUseCount(texArray));*/
    
        int j;
        for (j = 0; j < tapCount; j++) {
            LXFloat *arr = offsetsArray + j*4;
            ///LXPrintf("pass %i: shader param %i: %.4f, %.4f, %.4f, %.4f\n", i, j, arr[0], arr[1], arr[2], arr[3]);
            LXShaderSetParameter4f(shader, j, arr[0], arr[1], arr[2], arr[3]);
        }
        
        LXDrawContextSetShader(drawCtx, shader);
    
        
        LXRect outRect = LXMakeRect(0, 0, surfSize.w, surfSize.h);
        //DEBUGLOG(@"texrect %@; outrect %@", NSStringFromRect(texRect), NSStringFromRect(outRect));

        if ([_delegate respondsToSelector:@selector(transformConvolutionRenderLXRect:)])
            outRect = [_delegate transformConvolutionRenderLXRect:outRect];
            
        LXSetQuadVerticesXYUV(vertices, outRect, LXUnitRect);    
        
        /*if ( !accD)
            LXSurfaceClearRegionWithRGBA(currentD, outRect, LXMakeRGBA(1, 1, 0, 1));
        else*/
        LXSurfaceDrawPrimitive(currentD, kLXQuads, vertices, 4, kLXVertex_XYUV, drawCtx);
        
                
        ///LXSurfaceFlush(currentD);
        LXTextureArrayRelease(texArray);
        
        accD = currentD;
        currentD = nextD;
        nextD = accD;
    }

    DEBUGLOG(@"--- convolution done, self %p, acc %p (surf1 %p / surf2 %p)", self, accD, surf1, surf1);

    LXDrawContextRelease(drawCtx);

    // we're done -- return the drawable that holds the last drawing result
    return accD;
}

@end
