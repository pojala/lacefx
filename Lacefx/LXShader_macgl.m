/*
 *  LXShader_macgl.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.8.2007.
 *  Copyright 2007 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXShader.h"
#include "LXRef_Impl.h"
#include "LXShader_Impl.h"

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>

#import "LXPlatform.h"
#import "LQGLContext.h"


#define DISABLELXDEBUG 0


#define SHAREDCTX  ( (LQGLContext *)LXPlatformSharedNativeGraphicsContext() )
#define CTXTIMEOUT 9.555



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
        
        //LXDEBUGLOG("releasing shader %p (glfp %i, is static %i)", imp, imp->fragProgID, imp->storageHint == kLXStorageHint_Final);
    
        if (imp->fragProgID && !wasCopied) {
            [SHAREDCTX deferDeleteOfARBProgramID:imp->fragProgID];
        }

        _LXShaderCommonDataFree(imp);

        _lx_free(imp);
    }
}


static int createGLFragProgForShader(LXShaderImpl *imp)
{
    int err = 0;
    
    if ( ![SHAREDCTX activateWithTimeout:CTXTIMEOUT caller:__nsfunc__]) {
        NSLog(@"** %s: could not lock ctx", __func__);
        return 63;
    }
    
    ///NSLog(@"creating shader %p: prog %s", imp, imp->programStr);
    
    glGetError();
    
    glGenProgramsARB(1, &(imp->fragProgID));
    
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, imp->fragProgID);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, imp->programStrLen, imp->programStr);
    
    err = glGetError();
    
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    
    ///NSLog(@"%s: shader %p, fragprog is %i", __func__, imp, imp->fragProgID);

    [SHAREDCTX deactivate];
    return err;
}


LXShaderRef LXShaderCopy(LXShaderRef orig)
{
    LXShaderRef newShader = _LXShaderCommonCopy(orig);
    
    if (newShader == orig)
        return newShader;
    
    if (newShader) {
        LXShaderImpl *imp = (LXShaderImpl *)newShader;
        
        if (imp->programType == kLXShaderFormat_OpenGLARBfp) {
            if ((imp->storageHint & kLXStorageHint_Final) && ((LXShaderImpl *)orig)->fragProgID != 0) {
                // for static shader string, we can reuse the same fragment program id
                imp->fragProgID = ((LXShaderImpl *)orig)->fragProgID;
                
                // this flag keeps track of whether we should destroy the fragProg in LXShaderRelease()
                imp->sharedStorageFlags |= kLXShader_NativeObjectWasCopied;
                
                ///LXPrintf("%s -- %p -> %p, reusing same fpID (%i)\n", __func__, orig, newShader, imp->fragProgID);
            } else {
                // 2008.10.07 - deferred creation to LockPlatformNativeObj, the actual accessor called by implementation
                /*
                int err = createGLFragProgForShader(imp);
            
                if (err != GL_NO_ERROR) {
                    NSLog(@"** %s: GL error while copying shader %p (%i)", __func__, orig, err);
                    LXShaderRelease(newShader);
                    return NULL;
                }
                */
            }
        }
    }
    return newShader;
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

    // 2008.10.07 - deferred creation to LockPlatformNativeObj, the actual accessor called by implementation
    /*  
    if (programFormat == kLXShaderFormat_OpenGLARBfp) {
        int err = createGLFragProgForShader(imp);

        if (err != GL_NO_ERROR) {
            LXErrorSet(outError, kLXErrorID_Shader_ErrorInString, "error in program string");
            _lx_free(imp);
            return NULL;
        }
    }
    */    
    return shader;
}


LXUnidentifiedNativeObj LXShaderLockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return (LXUnidentifiedNativeObj)NULL;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    imp->retCount++;
    
    if (0 == imp->fragProgID) {
        if ( !imp->programStr || imp->programStrLen < 1) {
            imp->fragProgID = 0;
        } else {
            int err = createGLFragProgForShader(imp);
            
            if (err != GL_NO_ERROR && imp->programErrorWarningState == 0) {
                NSLog(@"** %s: GL error while creating fragment program for shader %p (%i)", __func__, r, err);
                NSLog(@"  ... erroneous program is: %s\n", imp->programStr);
                imp->programErrorWarningState = 1;
            }
        }
    }
    
    return (LXUnidentifiedNativeObj)((unsigned long)imp->fragProgID);
}

void LXShaderUnlockPlatformNativeObj(LXShaderRef r)
{
    if ( !r) return;
    LXShaderImpl *imp = (LXShaderImpl *)r;
    
    imp->retCount--;
}

