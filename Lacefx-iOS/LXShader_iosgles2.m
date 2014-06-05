/*
 *  LXShader_iosgles2.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXShader.h"
#include "LXRef_Impl.h"
#include "LXShader_Impl.h"

#import "LacefxESView.h"
#import "shaderCompileUtils_iosgles2.h"



LXShaderRef LXShaderRetain(LXShaderRef r)
{
    if ( !r) return NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    return r;
}

void LXShaderRelease(LXShaderRef r)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);
        
        LXBool wasCopied = (imp->sharedStorageFlags & kLXShader_NativeObjectWasCopied);
        ///NSLog(@"releasing shader %p (programID %i, is static %i, was copied %i)", imp, imp->programID, imp->storageHint == kLXStorageHint_Final, wasCopied);
    
        if (imp->programID && !wasCopied) {
            EAGLContext *prevCtx = [EAGLContext currentContext];
            [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];
            
            glDeleteProgram(imp->programID);
            
            [EAGLContext setCurrentContext:prevCtx];
        }

        _LXShaderCommonDataFree(imp);

        _lx_free(imp);
    }
}


static LXBool createGLES2ProgramForShader(LXShaderImpl *imp)
{
    int err = 0;
    
    [EAGLContext setCurrentContext:(EAGLContext *)LXPlatformSharedNativeGraphicsContext()];
    
    glGetError();
    
    GLuint program = glCreateProgram();
    
    if ( !program) {
        err = glGetError();
        NSLog(@"*** could not create gl program: error %i", err);
        return NO;
    }

    imp->programID = program;

    //NSLog(@"creating shader %p: prog %s", imp, imp->programStr);
    NSLog(@"shader %p: created program id %i", imp, imp->programID);
    
    GLuint vertShader = 0, fragShader = 0;

	// create and compile vertex shader
	NSString *vertShaderString = @""
"attribute vec4 a_position;"
"attribute vec4 a_color;"
"attribute vec2 a_texCoord0;"
//"attribute vec2 a_texCoord1;"
//"attribute vec2 a_texCoord2;"
//"attribute vec2 a_texCoord3;"
"uniform mat4 modelViewProjectionMatrix;"
"varying vec2 v_texCoord0;"
"varying vec2 v_texCoord1;"
"varying vec2 v_texCoord2;"
"varying vec2 v_texCoord3;"
//"varying vec4 v_color;"
"void main() {"
"	gl_Position = modelViewProjectionMatrix * a_position;"
//"   gl_Position.w = 1.0;"
//"	v_color = a_color;"
"   v_texCoord0 = a_texCoord0;"  // setting all texcoords currently to the same
"   v_texCoord1 = a_texCoord0;"
"   v_texCoord2 = a_texCoord0;"
"   v_texCoord3 = a_texCoord0;"
"}";
    
	if ( !compileShaderFromString(&vertShader, GL_VERTEX_SHADER, 1, vertShaderString)) {
		destroyShaders(vertShader, fragShader, program);
		return NO;
	}

	NSString *defaultFragShaderString = @""
"/*#ifdef GL_ES*/\n"
"// define default precision for 'float', 'vec', 'mat'\n"
"precision highp float;\n"
"/*#endif*/\n"
//"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"uniform sampler2D s_texture0;"
"void main() {"
"	gl_FragColor = texture2D(s_texture0, v_texCoord0)  /*v_color*/ ;"
"}";
    
    NSString *fragShaderString = nil;
    if ( !imp->programStr || imp->programStrLen < 1) {
        fragShaderString = defaultFragShaderString;
    }
    else if (imp->programType != kLXShaderFormat_OpenGLES2FragmentShader) {
        NSLog(@"*** unknown LX shader program type (%i), can't compile", imp->programType);
        fragShaderString = defaultFragShaderString;
    }
    else {
        fragShaderString = [NSString stringWithUTF8String:imp->programStr];
    }
    
	// create and compile fragment shader
	if ( !compileShaderFromString(&fragShader, GL_FRAGMENT_SHADER, 1, fragShaderString)) {
		destroyShaders(vertShader, fragShader, program);
		return NO;
	}
	
	// attach shaders to to program
	glAttachShader(program, vertShader);
	glAttachShader(program, fragShader);
	
	// bind attribute locations.
	// this needs to be done prior to linking
	glBindAttribLocation(program, ATTRIB_VERTEX, "a_position");
	//glBindAttribLocation(program, ATTRIB_COLOR, "a_color");
    glBindAttribLocation(program, ATTRIB_TEXCOORD0, "a_texCoord0");
    //glBindAttribLocation(program, ATTRIB_TEXCOORD1, "a_texCoord1");
    //glBindAttribLocation(program, ATTRIB_TEXCOORD2, "a_texCoord2");
    //glBindAttribLocation(program, ATTRIB_TEXCOORD3, "a_texCoord3");
    
    NSLog(@"--- %s (%p): linking program, id %i ---", __func__, imp, program);
	
	// link program
	if ( !linkProgram(program)) {
		destroyShaders(vertShader, fragShader, program);
        NSLog(@"*** %s: link program failed", __func__);
		return NO;
	}
	
	// get uniform locations
    GLint *u = imp->programUniformLocations;
    
    int i;
    for (i = 0; i < NUM_UNIFORMS; i++) {
        u[i] = -1;
    }
    
    u[UNIFORM_MODELVIEW_PROJECTION_MATRIX] = glGetUniformLocation(program, "modelViewProjectionMatrix");
    
    u[UNIFORM_SAMPLER0] = glGetUniformLocation(program, "s_texture0");
    u[UNIFORM_SAMPLER1] = glGetUniformLocation(program, "s_texture1");
    
    u[UNIFORM_PARAM0] = glGetUniformLocation(program, "param0");
    
    for (i = 0; i < NUM_UNIFORMS; i++) {
        if (i > 0) printf(", ");
        printf("uniform %i: location %i", i, u[i]);
    }
    printf("\n");
	
	// release vertex and fragment shaders
	if (vertShader)
		glDeleteShader(vertShader);
	if (fragShader)
		glDeleteShader(fragShader);

    return YES;
}


LXShaderRef LXShaderCreateWithString(const char *asciiStr,
                                                      size_t strLen,
                                                      LXUInteger programFormat,
                                                      LXUInteger storageHint,
                                                      LXError *outError)
{
    LXShaderRef shader = _LXShaderCommonCreateWithString(asciiStr, strLen, programFormat, storageHint, outError);
    
    if ( !shader)
        return NULL;
    
    return shader;
}


LXShaderRef LXShaderCopy(LXShaderRef orig)
{
    LXShaderRef newShader = _LXShaderCommonCopy(orig);
    
    if (newShader == orig)
        return newShader;
    
    if (newShader) {
        LXShaderImpl *imp = (LXShaderImpl *)newShader;
        
        if (imp->programType == kLXShaderFormat_OpenGLES2FragmentShader) {
            if ((imp->storageHint & kLXStorageHint_Final) && ((LXShaderImpl *)orig)->programID != 0) {
                // for static shader string, we can reuse the same fragment program id
                imp->programID = ((LXShaderImpl *)orig)->programID;
                
                memcpy(imp->programUniformLocations, ((LXShaderImpl *)orig)->programUniformLocations, NUM_UNIFORMS*sizeof(GLint));
                
                // this flag keeps track of whether we should destroy the fragProg in LXShaderRelease()
                imp->sharedStorageFlags |= kLXShader_NativeObjectWasCopied;
            }
        }
    }
    return newShader;
}

GLint *LXShaderGetGLUniformLocations_(LXShaderRef r)
{
    if ( !r) return NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;

    return imp->programUniformLocations;
}

LXUnidentifiedNativeObj LXShaderLockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    imp->retCount++;
    
    if ( !imp->programID) {
        NSLog(@"shader %p, will link (no program id)", r);
        
        LXBool ok = createGLES2ProgramForShader(imp);
        if ( !ok) {
            NSLog(@"*** could not create program for shader (%p)", r);
        }
    }
    
    return (LXUnidentifiedNativeObj)((unsigned long)imp->programID);
}

void LXShaderUnlockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    imp->retCount--;
}
