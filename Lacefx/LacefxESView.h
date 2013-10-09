/*
 *  LacefxESView.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#import <UIKit/UIKit.h>
#import <Lacefx/Lacefx.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

// requires OpenGL ES 2
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>



@interface LacefxESView : UIView {

    BOOL isBeingRotated;

    LXSurfaceRef _lxSurface;
    
    id _displayLink;
    
    EAGLContext *_context;
    // The pixel dimensions of the backbuffer
    GLint _backingWidth;
    GLint _backingHeight;
    // OpenGL names for the renderbuffer and framebuffer used to render to this view
    GLuint _viewRenderbuffer, _viewFramebuffer;	
}

@property (nonatomic, assign) BOOL isBeingRotated;

- (EAGLContext *)EAGLContext;

- (void)drawView:(id)sender;

// subclasses can override
- (void)didInitGraphics;

- (BOOL)resizeFromLayer:(CAEAGLLayer *)layer;

- (void)drawInLXSurface:(LXSurfaceRef)lxSurface;


// this method calls -drawInLXSurface
- (void)renderGL;

@end
