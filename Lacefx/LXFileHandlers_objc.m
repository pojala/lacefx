/*
 *  LXFileHandlers_objc.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10/18/10.
 *  Copyright 2010 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#import "LXBasicTypes.h"
#import "LXRefTypes.h"
#import "LXFileHandlers.h"


char *LXStrCreateUTF8_from_UTF16(const char16_t *utf16Str, size_t utf16Len, size_t *outUTF8Len)
{
    if (outUTF8Len) *outUTF8Len = 0;

    NSString *nsstr = [NSString stringWithCharacters:utf16Str length:utf16Len];
    const char *ustr = [nsstr UTF8String];
    
    if (ustr) {
        size_t ustrlen = strlen(ustr);
        char *newBuf = _lx_malloc(ustrlen + 1);
        
        memcpy(newBuf, ustr, ustrlen);
        newBuf[ustrlen] = 0;
        
        if (outUTF8Len) *outUTF8Len = ustrlen;
        
        return newBuf;
    }
    else
        return NULL;
}

char16_t *LXStrCreateUTF16_from_UTF8(const char *utf8Str, size_t utf8Len, size_t *outUTF16Len)
{
    if (outUTF16Len) *outUTF16Len = 0;
    
    NSString *nsstr = [NSString stringWithUTF8String:utf8Str];
    
    if (nsstr) {
        size_t len = [nsstr length];
        char16_t *newBuf = _lx_malloc((len + 1) * 2);
        
        [nsstr getCharacters:newBuf];
        newBuf[len] = 0;
        
        if (outUTF16Len) *outUTF16Len = len;
        
        return newBuf;
    }
    else
        return NULL;
}


LXSuccess LXOpenFileForReadingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile)
{
	FILE *file = NULL;
    LXSuccess retVal = NO;
    NSString *nsstr = [NSString stringWithCharacters:pathBuf length:pathLen];
    const char *utf8Str = [nsstr UTF8String];

    if (utf8Str && NULL != (file = fopen(utf8Str, "rb"))) {
        retVal = YES;
    }
    if (outFile) *outFile = file;
    return retVal;
}

LXSuccess LXOpenFileForWritingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile)
{
	FILE *file = NULL;
    LXSuccess retVal = NO;
    NSString *nsstr = [NSString stringWithCharacters:pathBuf length:pathLen];
    const char *utf8Str = [nsstr UTF8String];

    if (utf8Str && NULL != (file = fopen(utf8Str, "wb"))) {
        retVal = YES;
    }
    if (outFile) *outFile = file;
    return retVal;
}

