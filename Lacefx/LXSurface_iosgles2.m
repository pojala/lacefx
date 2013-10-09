/*
 *  LXSurface_iosgles.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import "LXSurface.h"
#import "LXTexture.h"
#import "LXPixelBuffer.h"
#import "LXPixelBuffer_priv.h"
#import "LXRef_Impl.h"
#import "LXDraw_Impl.h"
#import "LXPool_surface_priv.h"

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import "LacefxESView.h"
#import "shaderCompileUtils_iosgles2.h"


typedef struct {
    LXREF_STRUCT_HEADER
    
    LXPoolRef sourcePool;
    
    int w;
    int h;
    LXPixelFormat pixelFormat;
    LXBool hasZBuffer;
    
    LXTextureRef lxTexture;
    
    // render properties
    LXRect imageRegion;
    
    // backing drawable is either an FBO or a view
    LacefxESView *view;
    GLuint fbo;
    GLuint fboColorTex;
    GLuint fboDepthRBO;
    
} LXSurface_iOSImpl;



// platform-independent parts of the class
#define LXSURFACEIMPL LXSurface_iOSImpl
#include "LXSurface_baseinc.c"



LXSurfaceRef LXSurfaceCreateFromLacefxESView_(LacefxESView *view, int w, int h, GLuint fbo)
{
    if ( !view) return NULL;
    
    LXSurface_iOSImpl *imp = _lx_calloc(sizeof(LXSurface_iOSImpl), 1);
    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);

    imp->w = w;
    imp->h = h;
    imp->pixelFormat = 0;
    imp->hasZBuffer = NO;        
    imp->view = view;
    imp->fbo = fbo;
    
    ///NSLog(@"created LXSurface %p from view (%@); %i * %i", imp, view, imp->w, imp->h);
    
    return (LXSurfaceRef)imp;
}

LXSurfaceRef LXSurfaceRetain(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    
    return r;
}

void LXSurfaceRelease(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);

        if (imp->lxTexture) {
            LXTextureRelease(imp->lxTexture);
            imp->lxTexture = NULL;
        }
            
        if (imp->view) {
            imp->view = nil;
            imp->fbo = 0;
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            ///NSLog(@"%s, %p: fbo %i, texture %i, size %i * %i", __func__, r, imp->fbo, imp->fboColorTex, imp->w, imp->h);
            
            if (imp->fboColorTex) {
                glDeleteTextures(1, &(imp->fboColorTex));
                imp->fboColorTex = 0;
            }
            if (imp->fboDepthRBO) {
                glDeleteRenderbuffers(1, &(imp->fboDepthRBO));
                imp->fboDepthRBO = 0;
            }
            glDeleteFramebuffers(1, &(imp->fbo));
            imp->fbo = 0;
        }
        
        _lx_free(imp);
    }
}


void LXSurfaceFlush(LXSurfaceRef r)
{
    if ( !r) return;
    
    glFlush();

    //glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}


LXSurfaceRef LXSurfaceCreate(LXPoolRef pool,
                             uint32_t w, uint32_t h, LXPixelFormat pxFormat,
                             uint32_t flags,
                             LXError *outError)
{
    LXBool hasZBuffer = (flags & kLXSurfaceHasZBuffer) ? YES : NO;
    
    if (hasZBuffer) {
        NSLog(@"*** warning: zbuffer not supported yet with LXSurface FBO impl");
    }

    if (w < 1 || h < 1) {
        LXErrorSet(outError, 4001, "width or height is zero");
        return NULL;
    }

    int bitsPerPixel = 0;
    switch (pxFormat) {
        default:
        case kLX_RGBA_INT8:       bitsPerPixel = 32;  break;
    }

    if (bitsPerPixel == 0) {
        char str[256];
        sprintf(str, "unsupported pixel format (%i)", (int)pxFormat);
        LXErrorSet(outError, 4002, str);
        return NULL;
    }
    
    EAGLContext *prevCtx = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];
    
    LXSurface_iOSImpl *imp = _lx_calloc(sizeof(LXSurface_iOSImpl), 1);
    LXREF_INIT(imp, LXSurfaceTypeID(), LXSurfaceRetain, LXSurfaceRelease);
    
    imp->w = w;
    imp->h = h;
    
    glGenFramebuffers(1, &(imp->fbo));
    
    // setup texture attachment for fbo
    glBindFramebuffer(GL_FRAMEBUFFER, imp->fbo);
    
    glGenTextures(1, &(imp->fboColorTex));
    glBindTexture(GL_TEXTURE_2D, imp->fboColorTex);

    GLenum glPxf = 0;
    GLenum glPxfType = 0;
    GLenum glPxfComp = 0;
    switch (pxFormat) {
        default:
        case kLX_RGBA_INT8:
            glPxf = GL_RGBA;
            glPxfType = GL_RGBA;
            glPxfComp = GL_UNSIGNED_BYTE;
            break;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, glPxf, w, h, 0, glPxfType, glPxfComp, NULL);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, imp->fboColorTex, 0);
                
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_UNSUPPORTED) {
        // according to OpenGL FAQ, this is not really an error. go figure
    }
    else if (status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"*** %s: could not create FBO, status: 0x%x", __func__, status);
        return NULL;
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    ///NSLog(@"%s: created fbo %i, texture %i", __func__, imp->fbo, imp->fboColorTex);
    
    [EAGLContext setCurrentContext:prevCtx];

    if (imp) {
        imp->pixelFormat = pxFormat;
    }
    return (LXSurfaceRef)imp;
}


extern LXTextureRef LXTextureCreateWithSurface_(LXSurfaceRef surf);


LXTextureRef LXSurfaceGetTexture(LXSurfaceRef r)
{
    if ( !r) return NULL;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    
    if (NULL == imp->lxTexture && imp->fboColorTex) {
        imp->lxTexture = LXTextureCreateWithSurface_(r);
        
        LXTextureSetImagePlaneRegion(imp->lxTexture, LXSurfaceGetImagePlaneRegion(r));
        
        ///NSLog(@"surface %p: created LXTexture (%p) for fbo texture %i, pxformat %i", r, imp->lxTexture, imp->fboColorTex, imp->pixelFormat);
    }
    
    return imp->lxTexture;
}


LXPixelFormat LXSurfaceGetPixelFormat(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    return imp->pixelFormat;
}


LXBool LXSurfaceNativeYAxisIsFlipped(LXSurfaceRef r)
{
    if ( !r) return NO;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    return (imp->view) ? YES : NO;  // ??? is this right ???
}

LXBool LXSurfaceHasDefaultPixelOrthoProjection(LXSurfaceRef r)
{
    return YES;
}


LXSuccess LXSurfaceBeginAccessOnThread(LXUInteger flags)
{
    return YES;  // multithreading not implemented
}

void LXSurfaceEndAccessOnThread()
{
    // multithreading not implemented
}




static LXSurface_iOSImpl *g_activeViewSurface = NULL;


void LXSurfaceStartUIViewDrawing_(LXSurfaceRef r)
{
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    g_activeViewSurface = imp;
}

void LXSurfaceEndUIViewDrawing_(LXSurfaceRef r)
{
    g_activeViewSurface = NULL;
}


#define BEGINSURFACEDRAW \
    GLint prevViewport[4]; \
    if ( !imp->view) { glBindFramebuffer(GL_FRAMEBUFFER, imp->fbo); \
                       glGetIntegerv(GL_VIEWPORT, prevViewport); \
                       glViewport(0, 0, imp->w, imp->h); } \

#define ENDSURFACEDRAW \
    if ( !imp->view) { glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]); \
                       glBindFramebuffer(GL_FRAMEBUFFER, (g_activeViewSurface) ? g_activeViewSurface->fbo : 0); }


LXUnidentifiedNativeObj LXSurfaceLockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)0;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    
    return (LXUnidentifiedNativeObj)(imp->fbo);
}

void LXSurfaceUnlockPlatformNativeObj(LXSurfaceRef r)
{
    if ( !r) return;
}

GLuint LXSurfaceGetGLColorTexName_(LXSurfaceRef r)
{
    if ( !r) return 0;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    
    return imp->fboColorTex;
}



#pragma mark --- rendering ---


void LXSurfaceClear(LXSurfaceRef r)
{
    if ( !r) return;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    BEGINSURFACEDRAW
    
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | ((imp->hasZBuffer) ? GL_DEPTH_BUFFER_BIT : 0));
    
    ENDSURFACEDRAW
}

void LXSurfaceClearRegionWithRGBA(LXSurfaceRef r, LXRect rect, LXRGBA c)
{
    if ( !r) return;
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    LXBool isFlipped = LXSurfaceNativeYAxisIsFlipped(r);
    
    if (isFlipped) {
        rect.y = imp->h - rect.y - rect.h;
    }
    
    BEGINSURFACEDRAW
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.x, rect.y,  rect.w, rect.h);
    
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glDisable(GL_SCISSOR_TEST);
    
    ENDSURFACEDRAW
}


// drawing state
#import "LXDraw_iosgles2.h"


static BOOL loadBasicShaders()
{
	// create shader program
    if ( !g_basicGLProgramID) {
        g_basicGLProgramID = glCreateProgram();
    }
    GLuint program = g_basicGLProgramID;
    
    GLuint vertShader, fragShader;
	
	// create and compile vertex shader
	NSString *vertShaderString = @""
"attribute vec4 a_position;"
"attribute vec4 a_color;"
"attribute vec2 a_texCoord0;"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"uniform mat4 modelViewProjectionMatrix;"
"void main() {"
"	gl_Position = modelViewProjectionMatrix * a_position;"
//"   gl_Position.w = 1.0;"
"	v_color = a_color;"
"   v_texCoord0 = a_texCoord0;"
"}";
    
	if ( !compileShaderFromString(&vertShader, GL_VERTEX_SHADER, 1, vertShaderString)) {
		destroyShaders(vertShader, fragShader, program);
		return NO;
	}


	NSString *fragShaderString = @""
"/*#ifdef GL_ES*/\n"
"// define default precision for 'float', 'vec', 'mat'\n"
"precision highp float;\n"
"/*#endif*/\n"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"uniform sampler2D s_texture0;"
"void main() {"
"	gl_FragColor = texture2D(s_texture0, v_texCoord0)  /*v_color*/ ;"
"}";
	    
	// create and compile fragment shader
	if ( !compileShaderFromString(&fragShader, GL_FRAGMENT_SHADER, 1, fragShaderString)) {
		destroyShaders(vertShader, fragShader, program);
		return NO;
	}
	
	// attach vertex shader to program
	glAttachShader(program, vertShader);
	
	// attach fragment shader to program
	glAttachShader(program, fragShader);
	
	// bind attribute locations
	// this needs to be done prior to linking
	glBindAttribLocation(program, ATTRIB_VERTEX, "a_position");
	glBindAttribLocation(program, ATTRIB_COLOR, "a_color");
    glBindAttribLocation(program, ATTRIB_TEXCOORD0, "a_texCoord0");
	
	// link program
	if ( !linkProgram(program)) {
		destroyShaders(vertShader, fragShader, program);
		return NO;
	}
	
	// get uniform locations
	g_basicGLProgramUniforms[UNIFORM_MODELVIEW_PROJECTION_MATRIX] = glGetUniformLocation(program, "modelViewProjectionMatrix");
    
    ///NSLog(@"%s -- uniform %i", __func__, g_basicGLProgramUniforms[UNIFORM_MODELVIEW_PROJECTION_MATRIX]);
	
	// release vertex and fragment shaders
	if (vertShader) {
		glDeleteShader(vertShader);
	}
	if (fragShader) {
		glDeleteShader(fragShader);
	}
	
	return YES;
}


void LXSurfaceDrawPrimitive(LXSurfaceRef r,
                            LXPrimitiveType primitiveType,
                            void *vertices,
                            LXUInteger vertexCount,
                            LXVertexType vertexType,
                            LXDrawContextRef drawCtx)
{
    if ( !r || !vertices || vertexCount < 1) return;
    
    LXSurface_iOSImpl *imp = (LXSurface_iOSImpl *)r;
    BEGINSURFACEDRAW
    
    /*if ( !g_basicGLProgramID) {
        loadBasicShaders();
    }
    glUseProgram(g_basicGLProgramID);
    */
    
    LXDrawContextActiveState *drawState = NULL;
    
    LXDrawContextBeginForSurface_(drawCtx, r, &drawState);
    
    LXUInteger drawFlags = LXDrawContextGetFlags(drawCtx);

    // need textures here because Lacefx uses normalized texcoords
    LXTextureArrayRef texArray = LXDrawContextGetTextureArray(drawCtx);
    LXInteger texCount = LXDrawContextGetTextureCount(drawCtx);

    GLsizei glVertexStride = 4 * sizeof(LXFloat);
    GLenum glVertexType = GL_FLOAT;
#if defined(LXFLOAT_IS_DOUBLE)
    vertexType = GL_DOUBLE;
#endif
        
    // !!!--- this code currently draws just a single quad with no textures ---!!!
    /*
    LXTransform3DRef trs = LXAutorelease(LXTransform3DCreateIdentity());
    //LXTransform3DScale(trs, 0.5, 0.5, 1.0);
    //LXTransform3DTranslate(trs, 0.5, 0.07, 0);
    LXTransform3DConcatExactPixelsTransformForSurfaceSize(trs, 768, 748, YES);

    LX4x4Matrix modelviewProj;
    
    LXTransform3DGetMatrix(trs, &modelviewProj);
    
    LX4x4MatrixTranspose(&modelviewProj, &modelviewProj);
    
    char *str = LX4x4MatrixCreateString(&modelviewProj, YES);
    NSLog(@"\n%s", str);
    _lx_free(str);
	
	// update uniform values
	glUniformMatrix4fv(g_basicGLProgramUniforms[UNIFORM_MODELVIEW_PROJECTION_MATRIX], 1, GL_FALSE, (GLfloat *)&modelviewProj);
      */  
      
	glVertexAttribPointer(ATTRIB_VERTEX, 2, glVertexType, 0, glVertexStride, vertices);
	glEnableVertexAttribArray(ATTRIB_VERTEX);

#if 0
    const GLfloat squareVertices[] = {
        /*-0.5f, -0.5f,
        0.5f,  -0.5f,
        -0.5f,  0.5f,
        0.5f,   0.5f,*/
        100, 100,
        300, 100,
        100, 300,
        300, 300,
    };
#endif

    const GLubyte squareColors[] = {
        255, 255,   0, 255,
        0,   255, 255, 255,
        0,     0,   0,   0,
        255,   0, 255, 255,
    };
    
    //glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
	//glEnableVertexAttribArray(ATTRIB_VERTEX);
    
	glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, 1, 0, squareColors); //enable the normalized flag
    glEnableVertexAttribArray(ATTRIB_COLOR);
    
    
    if (vertexType == kLXVertex_XYUV) {
        glVertexAttribPointer(ATTRIB_TEXCOORD0, 2, glVertexType, 0, glVertexStride, (LXFloat *)vertices + 2); //enable the normalized flag
        glEnableVertexAttribArray(ATTRIB_TEXCOORD0);

        ///NSLog(@"enabling texcoord vertex array");
    }
    

    switch (primitiveType) {
        case kLXQuads: {
            // FIXME: need to draw the rest of the quads too, not just the first one :)
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            break;
        }
            
        case kLXTriangleFan:
            glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
            break;

        case kLXTriangleStrip:
            glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount);
            break;
            
        case kLXPoints:
            glDrawArrays(GL_POINTS, 0, vertexCount);
            break;

        case kLXLineStrip:
            glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
            break;

        case kLXLineLoop:
            glDrawArrays(GL_LINE_LOOP, 0, vertexCount);
            break;
    }
    

#if 0
    GLenum glPrimType;
    switch (primitiveType) {
        default:
        case kLXTriangleFan:    glPrimType = GL_TRIANGLE_FAN;  break;
        case kLXTriangleStrip:  glPrimType = GL_TRIANGLE_STRIP;  break;
        case kLXQuads:          glPrimType = GL_TRIANGLE_FAN;  break;

        case kLXPoints:         glPrimType = GL_POINTS;  break;

        case kLXLineStrip:      glPrimType = GL_LINE_STRIP;  break;
        case kLXLineLoop:       glPrimType = GL_LINE_LOOP;  break;
    }
    
    // start drawing
    glBegin(glPrimType);
    
    ///NSLog(@"%s: active nsview is %p, active pbuffer is %p", __func__, g_activeNSView, g_activePbuffer);
    
    /*NSLog(@".... texcount %i", texCount);
    if (texCount > 1) {
        LXSize s = LXTextureGetSize( LXTextureArrayAt(texArray, 1));
        NSLog(@"  .. tex 1 size: %i * %i", (int)s.w, (int)s.h);
    }*/
    
    LXUInteger i;
    for (i = 0; i < vertexCount; i++) {
        switch (vertexType) {
        
            case kLXVertex_XYUV: {
                LXVertexXYUV *vp = (LXVertexXYUV *) (((LXFloat *)vertices) + 4*i);
                
                ///setGLTexCoords(vp->u, vp->v, texCount, texArray, haveNormalizedTexCoordsInUnit7);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex2d(vp->x, vp->y);
                #else
                glVertex2f(vp->x, vp->y);
                #endif
                break;
            }
            
            case kLXVertex_XYZW: {
                LXVertexXYZW *vp = (LXVertexXYZW *) (((LXFloat *)vertices) + 4*i);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex4dv((double *)vp);
                #else
                glVertex4fv((float *)vp);
                //glVertex3f(vp->x, vp->y, vp->z);
                #endif
                break;
            }
            
            case kLXVertex_XYZWUV: {
                LXVertexXYZWUV *vp = (LXVertexXYZWUV *) (((LXFloat *)vertices) + 6*i);
                
                ///setGLTexCoords(vp->u, vp->v, texCount, texArray, haveNormalizedTexCoordsInUnit7);
                
                #if defined(LXFLOAT_IS_DOUBLE)
                glVertex4dv((double *)vp);
                #else
                glVertex4fv((float *)vp);
                #endif
                break;
            }
            
            default:
                NSLog(@"*** unsupported vertex format (%i)", vertexType);
                break;
        }
    }
    
    glEnd();
#endif
    
    LXDrawContextFinish_(drawCtx, &drawState);
    
    ENDSURFACEDRAW
}
