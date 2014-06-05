/*
 *  LacefxESView.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LacefxESView.h"
#import "LXPlatform_ios.h"
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGLDrawable.h>


extern LXSurfaceRef LXSurfaceCreateFromLacefxESView_(LacefxESView *view, int w, int h, GLuint fbo);
extern void LXSurfaceStartUIViewDrawing_(LXSurfaceRef r);
extern void LXSurfaceEndUIViewDrawing_(LXSurfaceRef r);


@implementation LacefxESView

@synthesize isBeingRotated;


+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

+ (NSUInteger)EAGLRenderingAPIType
{
    return kEAGLRenderingAPIOpenGLES2;
}

- (id)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        // get our backing CoreAnimation layer
        CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
        
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                                        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
                                                        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                                                        nil];
        
        NSUInteger apiType = [[self class] EAGLRenderingAPIType];
        
        EAGLContext *existingSharedContext = LXPlatformSharedNativeGraphicsContext();
        
        if (existingSharedContext) {
            _context = [[EAGLContext alloc] initWithAPI:apiType sharegroup:existingSharedContext.sharegroup];
        } else {
            _context = [[EAGLContext alloc] initWithAPI:apiType];
        }
        
        if ( !_context || ![EAGLContext setCurrentContext:_context]) {
            NSLog(@"*** %s: unable to init OpenGL ES context (api %ld, existing ctx %p)", __func__, (long)apiType, existingSharedContext);
        } else {
            // Create system framebuffer object. The backing will be allocated in -reshapeFramebuffer
            glGenFramebuffers(1, &_viewFramebuffer);
            glGenRenderbuffers(1, &_viewRenderbuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, _viewFramebuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, _viewRenderbuffer);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _viewRenderbuffer);
            
            NSLog(@"%s - created gl context %p, framebufferid=%i, renderbufferid=%i", __func__, _context, (int)_viewFramebuffer, (int)_viewRenderbuffer);
            
            if ( !existingSharedContext) {
                LXPlatformSetSharedEAGLContext(_context);
            }
        }
		
		// Perform additional one-time GL initialization
        [self didInitGraphics];

    }
    return self;
}

- (void)dealloc
{
    if ([EAGLContext currentContext] == _context)
        [EAGLContext setCurrentContext:nil];
    
    [_context release];
    _context = nil;
    
    [super dealloc];
}

- (EAGLContext *)EAGLContext {
    return _context; }


#pragma mark --- Lacefx drawing ---

- (void)drawInLXSurface:(LXSurfaceRef)lxSurface
{

}


#pragma mark --- GL setup and drawing ---

- (void)didInitGraphics
{
    // subclasses can override
}

- (void)renderGL
{
    [EAGLContext setCurrentContext:_context];
    
    glBindFramebuffer(GL_FRAMEBUFFER, _viewFramebuffer);
    glViewport(0, 0, _backingWidth, _backingHeight);
    
    //glClearColor(0.33f, 0.33f, 0.33f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);
    
    if (1) { // !self.isBeingRotated) {
        LXPoolRef lxPool = LXPoolCreateForThread();
    
        LXSurfaceStartUIViewDrawing_(_lxSurface);
    
        [self drawInLXSurface:_lxSurface];
    
        LXSurfaceEndUIViewDrawing_(_lxSurface);
        
        LXPoolRelease(lxPool);
    }
    
    glBindRenderbuffer(GL_RENDERBUFFER, _viewRenderbuffer);
    [_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)drawView:(id)sender
{
    [self renderGL];
}

- (BOOL)resizeFromLayer:(CAEAGLLayer *)layer
{
    [EAGLContext setCurrentContext:_context];
    
    glBindFramebuffer(GL_FRAMEBUFFER, _viewFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _viewRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _viewRenderbuffer);
            
	// Allocate color buffer backing based on the current layer size
    ///glBindRenderbuffer(GL_RENDERBUFFER, _viewRenderbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_backingHeight);
	
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"** %s: failed to make complete framebuffer object, status 0x%x (size %i * %i)", __func__, glCheckFramebufferStatus(GL_FRAMEBUFFER), _backingWidth, _backingHeight);
        return NO;
    }
	
    //NSLog(@"%s -- size %i * %i", __func__, _backingWidth, _backingHeight);
    
    LXSurfaceRelease(_lxSurface);
    _lxSurface = LXSurfaceCreateFromLacefxESView_(self, _backingWidth, _backingHeight, _viewFramebuffer);
    
    return YES;
}


#pragma mark --- overrides ---

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    [self resizeFromLayer:(CAEAGLLayer *)self.layer];
    [self drawView:nil];
}


@end
