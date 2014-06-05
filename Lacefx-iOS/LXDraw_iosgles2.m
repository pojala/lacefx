/*
 *  LXDraw_iosgles.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXDraw_Impl.h"
#include "LXDrawContext_Impl.h"
#include "LXTransform3D.h"
#include "LXTexture.h"
#include "LXTextureArray.h"
#include "LXPlatform_ios.h"

#import <GLKit/GLKit.h>

#import "LacefxESView.h"

#import "LXDraw_iosgles2.h"


/*
  --- the LXDrawContext implementation for OpenGL ES ---
*/


GLint g_basicGLProgramUniforms[NUM_UNIFORMS] = { 0 };
GLuint g_basicGLProgramID = 0;

LXShaderRef g_basicTexturingShader = NULL;



void LXTransform3DBeginForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    LXTransform3DRef trs;
    if (mv) {
        trs = LXTransform3DCopy(mv);
    } else {
        trs = LXTransform3DCreateIdentity();
    }
    
    if (proj) {
        LXTransform3DConcat(trs, proj);
    }
    
    NSCAssert(sizeof(GLfloat) == sizeof(LXFloat), @"invalid glfloat size");
    
    LX4x4Matrix modelviewProj;
    LXTransform3DGetMatrix(trs, &modelviewProj);
    
    // OpenGL uses opposite matrix row/column order than Lacefx
    LX4x4MatrixTranspose((LX4x4Matrix *)state->projModelviewGLMatrix, &modelviewProj);
    
    /*
    char *str = LX4x4MatrixCreateString(&modelviewProj, YES);
    NSLog(@"-- %s --\n%s", __func__, str);
    _lx_free(str);
	*/
        
    LXTransform3DRelease(trs);
}

void LXTransform3DFinishForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    // nothing to do here
}


void LXTextureOpenGL_EnableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData)
{
    NSCAssert1(i < 8, @"invalid texUnit: %ld", (long)i);
    
    glActiveTexture(GL_TEXTURE0 + i);

    {
        LXUInteger texID = (LXUInteger) LXTextureLockPlatformNativeObj(texture);
        
        if (texID) {            
            ///LXDEBUGLOG("%s: enabling texID %i in unit %i", __func__, texID, i);
            ///NSLog(@"...enabling GL texID %i in unit %i; w %i (%p)", texID, i, LXTextureGetWidth(texture), texture);
        
            //glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texID);
            
            //glEnable(GL_BLEND);
            //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            //glDisable(GL_BLEND);            
        } else {
            //glDisable(GL_TEXTURE_2D);
        }
        
        LXTextureUnlockPlatformNativeObj(texture);
    }
    
    LXTextureArrayRef texArray = (userData && LXRefIsOfType(userData, LXTextureArrayTypeID())) ? (LXTextureArrayRef)userData : NULL;

    LXUInteger samplingOverride = (texArray) ? LXTextureArrayGetSamplingAt(texArray, i) : kLXNotFound;
    
    LXUInteger sampling = (samplingOverride == kLXNotFound) ? LXTextureGetSampling(texture) : samplingOverride;
    
    GLenum glSampling = (sampling == kLXLinearSampling) ? GL_LINEAR : GL_NEAREST;    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glSampling);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glSampling);

    LXUInteger wrapping = (texArray) ? LXTextureArrayGetWrapModeAt(texArray, i) : kLXWrapMode_ClampToEdge;
    GLenum glWrapMode;
    switch (wrapping) {
        default:  glWrapMode = GL_CLAMP_TO_EDGE;  break;
        
        // we can't support anything else on iOS,
        // because the GL_APPLE_texture_2D_limited_npot extension doesn't allow it
        //
        ///case kLXWrapMode_ClampToBorder:  glWrapMode = GL_CLAMP_TO_BORDER;  break;
        ///case kLXWrapMode_Repeat:  glWrapMode = GL_REPEAT;  break;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapMode);
    
    if (i != 0)  glActiveTexture(GL_TEXTURE0);
}

void LXTextureOpenGL_DisableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData)
{
    NSCAssert1(i < 8, @"invalid texUnit: %ld", (long)i);
    
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    
    ///NSLog(@"...disabling GL texture for unit %i (%p)", i, texture);
    
    if (i != 0)  glActiveTexture(GL_TEXTURE0);
}


void LXTextureArrayApply_(LXTextureArrayRef array, LXDrawContextActiveState *state)
{
    if (LXTextureArrayInUseCount(array) > 0) {
        ///NSLog(@"enabling textures (count %i, state %p)", LXTextureArrayInUseCount(array), state);
        LXTextureArrayForEach(array, LXTextureOpenGL_EnableInTextureUnit, array);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
}

void LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(LXDrawContextRef drawCtx)
{
    LXDrawContextImpl *imp = (LXDrawContextImpl *)drawCtx;
    if ( !imp) return;
    
    ///glDisable(GL_FRAGMENT_PROGRAM_ARB);
    
    if (imp->shader) {
        float color[4] = { 1, 1, 1, 1 };
        LXShaderGetParameter4fv(imp->shader, 0, color);
        glColor4f(color[0], color[1], color[2], color[3]);
    }

    ///NSLog(@"%s - %f, %f, %f", __func__, color[0], color[1], color[2]);        
}

static LXShaderRef createBasicTexturingShader()
{
    LXDECLERROR(err)
	const char *fs = ""
"precision highp float;\n"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"uniform sampler2D s_texture0;"
"void main() {"
"	gl_FragColor = texture2D(s_texture0, v_texCoord0);"
"}";
    
    LXShaderRef shader = LXShaderCreateWithString(fs, strlen(fs), kLXShaderFormat_OpenGLES2FragmentShader, kLXStorageHint_Final, &err);
    
    NSLog(@"%s: created %p", __func__, shader);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}


extern GLint *LXShaderGetGLUniformLocations_(LXShaderRef r);


void LXShaderApplyWithConstants_(LXShaderRef shader, float *localParams, int paramCount, LXDrawContextActiveState *state)
{
    if ( !shader) {
        if ( !g_basicTexturingShader) {
            g_basicTexturingShader = createBasicTexturingShader();
        }
        shader = g_basicTexturingShader;
    }
    
    GLuint programID = (GLuint) LXShaderLockPlatformNativeObj(shader);
    
    glUseProgram(programID);
    
    ///NSLog(@"%s -- enabling program id %i", __func__, programID);
    
    LXShaderUnlockPlatformNativeObj(shader);
    
    GLint *uniforms = LXShaderGetGLUniformLocations_(shader);
    
    if ( !programID || !uniforms)
        return;
        
    glUniformMatrix4fv(uniforms[UNIFORM_MODELVIEW_PROJECTION_MATRIX], 1, GL_FALSE, state->projModelviewGLMatrix);
    
    if (uniforms[UNIFORM_SAMPLER0] != -1)  glUniform1i(uniforms[UNIFORM_SAMPLER0], 0);
    if (uniforms[UNIFORM_SAMPLER1] != -1)  glUniform1i(uniforms[UNIFORM_SAMPLER1], 1);
    if (uniforms[UNIFORM_SAMPLER2] != -1)  glUniform1i(uniforms[UNIFORM_SAMPLER2], 2);
    if (uniforms[UNIFORM_SAMPLER3] != -1)  glUniform1i(uniforms[UNIFORM_SAMPLER3], 3);

    float params[32 * 4];
    if ( !localParams) {
        paramCount = MIN(32, LXShaderGetMaxNumberOfParameters(shader));
        int i;
        for (i = 0; i < paramCount; i++) {
            LXShaderGetParameter4fv(shader, i, params + (i * 4));
        }
        localParams = params;
    }
    
    if (localParams) {
        int i;
        
        paramCount = MIN(8, paramCount);
        
        for (i = 0; i < paramCount; i++) {
            //printf("gl setting shader param %i - %f, %f, %f, %f\n", i, localParams[i*4], localParams[i*4+1], localParams[i*4+2], localParams[i*4+3]);
            
            GLint uniformLoc = uniforms[UNIFORM_PARAM0 + i];
            
            if (uniformLoc != -1)
                glUniform4f(uniformLoc, localParams[i*4], localParams[i*4+1], localParams[i*4+2], localParams[i*4+3]);
        }
    }
}


void LXShaderFinishCurrent_(LXDrawContextActiveState *state)
{
    /*
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    */
    
    glUseProgram(0);
}

void LXTextureArrayFinishCurrent_(LXDrawContextActiveState *state)
{
    //glBindTexture(GL_TEXTURE_2D, 0);
    //glDisable(GL_TEXTURE_2D);
    int i;
    for (i = 7; i >= 0; i--) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }
}





