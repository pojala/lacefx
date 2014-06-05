/*
 *  shaderUtils_iosgles2.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "shaderCompileUtils_iosgles2.h"


#define DEBUG 1


GLint compileShaderFromFile(GLuint *shader, GLenum type, GLsizei count, NSString *file)
{
    return compileShaderFromString(shader, type, count, [NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil]);
}


/* Create and compile a shader from the provided source(s) */
GLint compileShaderFromString(GLuint *shader, GLenum type, GLsizei count, NSString *string)
{
	GLint status;
	const GLchar *sources;
	
	// get source code
	sources = (GLchar *)[string UTF8String];
	if (!sources)
	{
		NSLog(@"Failed to load vertex shader");
		return 0;
	}
	
    *shader = glCreateShader(type);				// create shader
    glShaderSource(*shader, 1, &sources, NULL);	// set source code in the shader
    glCompileShader(*shader);					// compile shader
	
#if defined(DEBUG)
	GLint logLength;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(*shader, logLength, &logLength, log);
        NSLog(@"Shader compile log:\n%s", log);
        free(log);
    }
#endif
    
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
		NSLog(@"Failed to compile shader:\n%@", string);
	}
	
	return status;
}


/* Link a program with all currently attached shaders */
GLint linkProgram(GLuint prog)
{
	GLint status;
	
	glLinkProgram(prog);
	
#if defined(DEBUG)
	GLint logLength;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program link log:\n%s", log);
        free(log);
    }
#endif
    
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
		NSLog(@"Failed to link program %d", prog);
	
	return status;
}


/* Validate a program (for i.e. inconsistent samplers) */
GLint validateProgram(GLuint prog)
{
	GLint logLength, status;
	
	glValidateProgram(prog);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program validate log:\n%s", log);
        free(log);
    }
    
    glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE)
		NSLog(@"Failed to validate program %d", prog);
	
	return status;
}

/* delete shader resources */
void destroyShaders(GLuint vertShader, GLuint fragShader, GLuint prog)
{	
	if (vertShader) {
		glDeleteShader(vertShader);
		vertShader = 0;
	}
	if (fragShader) {
		glDeleteShader(fragShader);
		fragShader = 0;
	}
	if (prog) {
		glDeleteProgram(prog);
		prog = 0;
	}
}
