/*
 *  LXDraw_objc.m
 *  Lacefx private
 *
 *  Created by Pauli Ojala on 23.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#define DISABLELXDEBUG 1

#include "LXDraw_Impl.h"
#include "LXDrawContext_Impl.h"
#include "LXTransform3D.h"
#include "LXTexture.h"
#include "LXTextureArray.h"
#include "LXPlatform_mac.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>



/*
 this file contains the LXDrawContext implementation for OpenGL
*/


static void LXTransform3DApplyToGLMatrix_(LXTransform3DRef r, GLenum matMode)
{
    if ( !r) return;
    LX4x4Matrix matrix;

    NSCAssert2(sizeof(LX4x4Matrix) == 4*4*sizeof(LXFloat),
                                    @"invalid size for LX4x4Matrix, compiler may be applying padding - this needs to be fixed with pragmas (%i, %i)",
                                    (int)sizeof(LX4x4Matrix), (int)sizeof(LXFloat));
    
    LXTransform3DGetMatrix(r, &matrix);
    
    glMatrixMode(matMode);
    
    // OpenGL uses opposite matrix row/column order than Lacefx
    LX4x4MatrixTranspose(&matrix, &matrix);
    
    if (sizeof(LXFloat) == sizeof(float)) 
        glMultMatrixf((float *)(&matrix));
    else
        glMultMatrixd((double *)(&matrix));
}

void LXTransform3DBeginForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    if ( !state) return;

    if (proj) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        state->didPushProjMatrix = YES;
    
        glLoadIdentity();  // start with identity proj matrix

        LXTransform3DApplyToGLMatrix_(proj, GL_PROJECTION);
    }

    if (1) { //mv) {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        state->didPushMVMatrix = YES;

        glLoadIdentity();

        if (mv) LXTransform3DApplyToGLMatrix_(mv, GL_MODELVIEW);
    }
}

void LXTransform3DFinishForProjectionAndModelView_(LXTransform3DRef proj, LXTransform3DRef mv, LXDrawContextActiveState *state)
{
    if ( !state) return;
        
    if (state->didPushMVMatrix) {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
    
    if (state->didPushProjMatrix) {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();        
    }
}


void LXTextureOpenGL_EnableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData)
{
    NSCAssert1(i < 8, @"invalid texUnit: %ld", (long)i);
    
    glActiveTexture(GL_TEXTURE0 + i);

    LXUInteger texID = (LXUInteger) LXTextureLockPlatformNativeObj(texture);
    if (texID) {
        ///LXDEBUGLOG("%s: enabling texID %i in unit %i", __func__, texID, i);
        ///NSLog(@"...enabling GL texID %i in unit %i; w %i (%p)", texID, i, LXTextureGetWidth(texture), texture);
        
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_RECTANGLE_EXT);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, texID);
        
        //glEnable(GL_BLEND);
        //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    } else {
        //glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_RECTANGLE_EXT);
    }
    LXTextureUnlockPlatformNativeObj(texture);
    
    LXTextureArrayRef texArray = (userData && LXRefIsOfType(userData, LXTextureArrayTypeID())) ? (LXTextureArrayRef)userData : NULL;

    LXUInteger samplingOverride = (texArray) ? LXTextureArrayGetSamplingAt(texArray, i) : kLXNotFound;
    
    LXUInteger sampling = (samplingOverride == kLXNotFound) ? LXTextureGetSampling(texture) : samplingOverride;
    
    GLenum glSampling = (sampling == kLXLinearSampling) ? GL_LINEAR : GL_NEAREST;    
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, glSampling);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, glSampling);

    LXUInteger wrapping = (texArray) ? LXTextureArrayGetWrapModeAt(texArray, i) : kLXWrapMode_ClampToEdge;
    GLenum glWrapMode;
    switch (wrapping) {
        default:  glWrapMode = GL_CLAMP_TO_EDGE;  break;
        case kLXWrapMode_ClampToBorder:  glWrapMode = GL_CLAMP_TO_BORDER;  break;
        case kLXWrapMode_Repeat:  glWrapMode = GL_REPEAT;  break;
    }
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, glWrapMode);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, glWrapMode);
    
    if (i != 0)  glActiveTexture(GL_TEXTURE0);
}

void LXTextureOpenGL_DisableInTextureUnit(LXTextureRef texture, LXInteger i, void *userData)
{
    NSCAssert1(i < 8, @"invalid texUnit: %ld", (long)i);
    
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glDisable(GL_TEXTURE_RECTANGLE_EXT);
    
    ///NSLog(@"...disabling GL texture for unit %i (%p)", i, texture);
    
    if (i != 0)  glActiveTexture(GL_TEXTURE0);
}


void LXTextureArrayApply_(LXTextureArrayRef array, LXDrawContextActiveState *state)
{
    if (LXTextureArrayInUseCount(array) > 0) {
        //NSLog(@"enabling textures (count %i, state %p)", LXTextureArrayInUseCount(array), state);
        LXTextureArrayForEach(array, LXTextureOpenGL_EnableInTextureUnit, array);
    } else {
        glDisable(GL_TEXTURE_RECTANGLE_EXT);
    }
}

void LXDrawContextApplyFixedFunctionBaseColorInsteadOfShader_(LXDrawContextRef drawCtx)
{
    LXDrawContextImpl *imp = (LXDrawContextImpl *)drawCtx;
    if ( !imp) return;
    
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    
    if (imp->shader) {
        float color[4] = { 1, 1, 1, 1 };
        LXShaderGetParameter4fv(imp->shader, 0, color);
        glColor4fv(color);
    }

    ///NSLog(@"%s - %f, %f, %f", __func__, color[0], color[1], color[2]);        
}

void LXShaderApplyWithConstants_(LXShaderRef shader, float *localParams, int paramCount, LXDrawContextActiveState *state)
{
    if ( !shader) {
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        return;
    }
    
    GLuint progID = (GLuint) LXShaderLockPlatformNativeObj(shader);
    
    if (progID) {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, progID);
    } else
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    
    LXShaderUnlockPlatformNativeObj(shader);
    
    if ( !progID)
        return;
    
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
        for (i = 0; i < paramCount; i++) {
            ///printf("gl setting shader param %i - %f, %f, %f, %f\n", i, localParams[i*4], localParams[i*4+1], localParams[i*4+2], localParams[i*4+3]);
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, i,
                                         localParams[i*4], localParams[i*4+1], localParams[i*4+2], localParams[i*4+3] );
        }
    }
}


void LXShaderFinishCurrent_(LXDrawContextActiveState *state)
{
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

void LXTextureArrayFinishCurrent_(LXDrawContextActiveState *state)
{
    //glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    //glDisable(GL_TEXTURE_RECTANGLE_EXT);
    int i;
    for (i = 7; i >= 0; i--) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
        glDisable(GL_TEXTURE_RECTANGLE_EXT);
        glDisable(GL_BLEND);
    }
}

