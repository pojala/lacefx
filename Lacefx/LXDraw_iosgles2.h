/*
 *  LXDraw_iosgles2.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


// uniform index
enum {
	UNIFORM_MODELVIEW_PROJECTION_MATRIX,
    
    UNIFORM_SAMPLER0,
    UNIFORM_SAMPLER1,
    UNIFORM_SAMPLER2,
    UNIFORM_SAMPLER3,
    
    UNIFORM_PARAM0,
    UNIFORM_PARAM1,
    UNIFORM_PARAM2,
    UNIFORM_PARAM3,
    UNIFORM_PARAM4,
    UNIFORM_PARAM5,
    UNIFORM_PARAM6,
    UNIFORM_PARAM7,
    
	NUM_UNIFORMS
};

extern GLint g_basicGLProgramUniforms[NUM_UNIFORMS];


// attribute index
enum {
	ATTRIB_VERTEX,
	ATTRIB_COLOR,
    ATTRIB_TEXCOORD0,
    ATTRIB_TEXCOORD1,
    ATTRIB_TEXCOORD2,
    ATTRIB_TEXCOORD3,
    ATTRIB_TEXCOORD4,
    ATTRIB_TEXCOORD5,
    ATTRIB_TEXCOORD6,
    ATTRIB_TEXCOORD7,    
	NUM_ATTRIBUTES
};


extern GLuint g_basicGLProgramID;


