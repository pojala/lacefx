/*
 *  shaderUtils_iosgles2.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef SHADERS_H
#define SHADERS_H

#import <Foundation/Foundation.h>

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

/* Shader Utilities */
GLint compileShaderFromFile(GLuint *shader, GLenum type, GLsizei count, NSString *file);
GLint compileShaderFromString(GLuint *shader, GLenum type, GLsizei count, NSString *str);
GLint linkProgram(GLuint prog);
GLint validateProgram(GLuint prog);
void destroyShaders(GLuint vertShader, GLuint fragShader, GLuint prog);

#endif /* SHADERS_H */

