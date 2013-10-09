/*
 *  LXShaderUtils.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 8.5.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXShaderUtils.h"

LXShaderRef LXCreateShader_SolidColor()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0"
                     "MOV result.color, program.local[0];  "
                     "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateMaskShader_MaskWithRed()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0"
                     "TEMP t1, t2;  "
                     "TEX t1, fragment.texcoord[0], texture[0], RECT;  "
                     "TEX t2, fragment.texcoord[1], texture[1], RECT;  "
                     //"ADD result.color, t1, t2;"
                     //"ADD result.color, t1, { 0.5, 0.3, 0.0, 0.0 };"
                     "MOV_SAT t2.r, t2.r;  "
                     "MUL_SAT result.color.rgb, t1, t2.r;  "     // premultiply
                     "MOV result.color.a, t2.r;  "
                     "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateMaskShader_MaskWithAlpha()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0"
                     "TEMP t1, t2;  "
                     "TEX t1, fragment.texcoord[0], texture[0], RECT;  "
                     "TEX t2, fragment.texcoord[1], texture[1], RECT;  "
                     //"ADD result.color, t1, t2;  "
                     //"ADD result.color, t1, { 0.5, 0.3, 0.0, 0.0 };  "
                     "MOV_SAT t2.a, t2.a;  "
                     "MUL_SAT result.color.rgb, t1, t2.a;  "     // premultiply
                     "MOV result.color.a, t2.a;  "
                     "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateMaskShader_Param()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0"
                     "TEMP t1;  "
                     "TEX t1, fragment.texcoord[0], texture[0], RECT;  "
                     "MUL t1, t1, program.local[0];  "
                     "MOV result.color, t1;  "
                     "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}


LXShaderRef LXCreateCompositeShader_OverOp_Premult()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            
                            "SUB_SAT s.a, 1.0, t1.a;  "
                            //"LRP c.rgb, s.a, t1, t0;"
                            //"MOV c.a, 1.0;"
                            "MAD c.rgb, t0, s.a, t1;  "
                            
                            //"MOV c.a, 1.0;  "
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "
                            
                            "MOV result.color, c;  "
                            "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateCompositeShader_OverOp_Premult_Param()
{
    LXDECLERROR(err)
    
#if defined(LXPLATFORM_IOS)
    static const char *fs = ""
"precision highp float;\n"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"varying vec2 v_texCoord1;"
"uniform sampler2D s_texture0;"
"uniform sampler2D s_texture1;"
"uniform vec4 param0;"
"void main() {"
"   vec4 t0 = texture2D(s_texture0, v_texCoord0);"
"   vec4 t1 = texture2D(s_texture1, v_texCoord1);"
"   vec4 o;"
"   t1 = t1 * param0;"
"   float s = 1.0 - t1.a;"
"   o = (t0 * s) + t1;"
"   o.a = 1.0;"
"   gl_FragColor = o;"
"}";
    
    LXShaderRef shader = LXShaderCreateWithString(fs, strlen(fs), kLXShaderFormat_OpenGLES2FragmentShader, kLXStorageHint_Final, &err);
    
#else
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            
                            "MUL t1, t1, program.local[0];  "
                            
                            "SUB_SAT s.a, 1.0, t1.a;  "
                            "MAD c.rgb, t0, s.a, t1;  "
                            
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "

                            "MOV result.color, c;  "
                            "END";
                            
    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
#endif

    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateCompositeShader_OverOp_Unpremult_Param()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            
                            "MUL t1.a, t1.a, program.local[0];  "
                            "MUL t1.rgb, t1, t1.a; "
                            
                            "SUB_SAT s.a, 1.0, t1.a;  "
                            "MAD c.rgb, t0, s.a, t1;  "
                            
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "

                            "MOV result.color, c;  "
                            "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateCompositeShader_OverOp_Premult_MaskWithRed()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, t2, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            "TEX t2, fragment.texcoord[2], texture[2], RECT;  "
                            
                            "MUL t1, t1, t2.r;  "
                            
                            "SUB_SAT s.a, 1.0, t1.a;  "
                            "MAD c.rgb, t0, s.a, t1;  "
                            
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "

                            "MOV result.color, c;  "
                            "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateCompositeShader_OverOp_Unpremult_MaskWithRed()
{
    LXDECLERROR(err)
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, t2, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            "TEX t2, fragment.texcoord[2], texture[2], RECT;  "
                            
                            "MUL t1.a, t1.a, t2.r;  "
                            "MUL t1.rgb, t1, t1.a; "
                            
                            "SUB_SAT s.a, 1.0, t1.a;  "
                            "MAD c.rgb, t0, s.a, t1;  "
                            
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "

                            "MOV result.color, c;  "
                            "END";

    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
    
    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}


LXShaderRef LXCreateCompositeShader_Add_Premult_Param()
{
    LXDECLERROR(err)
    
#if defined(LXPLATFORM_IOS)
    static const char *fs = ""
"precision highp float;\n"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"varying vec2 v_texCoord1;"
"uniform sampler2D s_texture0;"
"uniform sampler2D s_texture1;"
"uniform vec4 param0;"
"void main() {"
"   vec4 t0 = texture2D(s_texture0, v_texCoord0);"
"   vec4 t1 = texture2D(s_texture1, v_texCoord1);"
"   vec4 o;"
"   t1 = t1 * param0;"
"   o = t0 + t1;"
"   o.a = 1.0;"
"   gl_FragColor = o;"
"}";
    
    LXShaderRef shader = LXShaderCreateWithString(fs, strlen(fs), kLXShaderFormat_OpenGLES2FragmentShader, kLXStorageHint_Final, &err);
    
#else
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            
                            "MUL t1, t1, program.local[0];  "
                            
                            "ADD c.rgb, t0, t1;  "
                            
                            "SUB_SAT s.a, 1.0, t0.a;  "
                            "MAD c.a, s.a, t1.a, t0.a;  "
                            
                            "MOV result.color, c;  "
                            "END";
                            
    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
#endif

    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}

LXShaderRef LXCreateCompositeShader_Multiply_Premult_Param()
{
    LXDECLERROR(err)
    
#if defined(LXPLATFORM_IOS)
    static const char *fs = ""
"precision highp float;\n"
"varying vec4 v_color;"
"varying vec2 v_texCoord0;"
"varying vec2 v_texCoord1;"
"uniform sampler2D s_texture0;"
"uniform sampler2D s_texture1;"
"uniform vec4 param0;"
"void main() {"
"   vec4 t0 = texture2D(s_texture0, v_texCoord0);"
"   vec4 t1 = texture2D(s_texture1, v_texCoord1);"
"   vec4 o;"
"   t1 = t1 * param0;"
"   o = t0 * t1;"
"   o.a = 1.0;"
"   gl_FragColor = o;"
"}";
    
    LXShaderRef shader = LXShaderCreateWithString(fs, strlen(fs), kLXShaderFormat_OpenGLES2FragmentShader, kLXStorageHint_Final, &err);
    
#else
    static const char *ss = "!!ARBfp1.0\n"
                            "TEMP t0, t1, c, s;  "
                            "TEX t0, fragment.texcoord[0], texture[0], RECT;  "
                            "TEX t1, fragment.texcoord[1], texture[1], RECT;  "
                            
                            "SUB s, {1.0, 1.0, 1.0, 1.0}, t1; "
                            "MUL s, program.local[0]; "
                            "MUL s.rgb, s.rgb, t1.a; "
                            "SUB s, {1.0, 1.0, 1.0, 1.0}, s; "
                            
                            "MUL c.rgb, t0, s;  "
                            "MOV c.a, t0.a;  "
                            
                            "MOV result.color, c;  "
                            "END";
                            
    LXShaderRef shader = LXShaderCreateWithString(ss, strlen(ss), kLXShaderFormat_OpenGLARBfp,  kLXStorageHint_Final, &err);
#endif

    if ( !shader) {
        LXWARN("** %s: can't create shader, err %i (%s)", __func__, err.errorID, err.description);
        LXErrorDestroyOnStack(err);
    }
    return shader;
}