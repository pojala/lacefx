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

#import <Foundation/Foundation.h>
#import "LXBasicTypes.h"
#import "LXSurface.h"
#import "LXTexture.h"
#import "LXShader.h"

/*
  Internal implementation class used by the LXConvolver functions.
*/

@interface LQLXConvolver : NSObject {

    id                      _delegate;
    
    LXTextureRef            _sourceTex;

    double                  *_weights;
    int                     _weightsCount;    
    BOOL                    _kernelIsDirty;
    BOOL                    _useNearestSampling;

    LXShaderRef             _firstPassShader;
    LXShaderRef             _fullPassShader;
    LXShaderRef             _endPassShader;
    
    NSMutableArray          *_offsetInfos;
}

- (void)setDelegate:(id)delegate;

- (void)invalidateCache;

- (void)setSourceLXTexture:(LXTextureRef)tex;
- (LXTextureRef)sourceLXTexture;

- (LXSurfaceRef)render1DConvolutionWithWidth:(LXFloat)am
                                            destSurface1:(LXSurfaceRef)surf1
                                            destSurface2:(LXSurfaceRef)surf2;
											
@end


@interface NSObject (LQLXConvolverDelegate)

- (void)generateConvolutionWeights:(LQLXConvolver *)conv inDoubleArray:(double *)array totalTaps:(LXInteger)width;
- (BOOL)shouldNormalizeConvolutionWeights:(LQLXConvolver *)conv;

- (int)convolutionSpreadTreshold:(LQLXConvolver *)conv;
- (double)convolutionSpreadMultiplier:(LQLXConvolver *)conv;
- (double)convolutionSampleOffsetMultiplier:(LQLXConvolver *)conv;
- (double)convolutionSampleOffsetRotation:(LQLXConvolver *)conv;
- (BOOL)useNearestNeighborSamplingForConvolution:(LQLXConvolver *)conv;

- (NSString *)combinerARBfpInstructionForConvolution:(LQLXConvolver *)conv;
- (NSString *)finishingARBfpInstructionForConvolution:(LQLXConvolver *)conv sourceVar:(NSString *)v;

- (LXRect)transformConvolutionRenderLXRect:(LXRect)r;

@end


