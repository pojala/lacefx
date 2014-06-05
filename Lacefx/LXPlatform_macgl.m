/*
 *  LXPlatform_macgl.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 8.9.2008.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>
#import "LXPlatform.h"
#import "LXPlatform_mac.h"
#import "GLCheck.h"
#import "LQGLContext.h"
#import "LXMutex.h"



extern LXMutex *g_lxAtomicLock;
extern void LXPlatformCreateLocks_();


#define MEGABYTE (1024*1024)


// shared OpenGL context
static LQGLContext *g_ctx = nil;


static CGDirectDisplayID g_mainDisplayCGID = 0;
static CGOpenGLDisplayMask g_mainDisplayGLMask = 0;

extern void _LXPlatformSetMainDisplayGLMask(CGOpenGLDisplayMask dmask);


void LQUpdateMainDisplayGLMask()
{
    if ( 1) {   //    if (g_mainDisplayGLMask == 0) {
        g_mainDisplayCGID = (CGDirectDisplayID) [[[[NSScreen mainScreen] deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
        
        CGOpenGLDisplayMask glDisplayMask = CGDisplayIDToOpenGLDisplayMask(g_mainDisplayCGID);
        
        if ((CGOpenGLDisplayMask)LXPlatformMainDisplayIdentifier() != glDisplayMask) {
            _LXPlatformSetMainDisplayGLMask(glDisplayMask);
            
#if 0 //!defined(RELEASE)
            NSLog(@"Setting Lacefx main display cgID: %i -> glID %i; devicedesc: %@ --(end)", g_mainDisplayCGID, LXPlatformMainDisplayIdentifier(),
                  [[[NSScreen mainScreen] deviceDescription] description]);
#endif
        }
    }
}



// GL capability globals
GLCaps              *g_glCaps = NULL;
CGDisplayCount      g_displayCount = 0;

static void updateGLCaps()
{
    int change = 0;
	if (g_glCaps && (change = HaveOpenGLCapsChanged(g_glCaps, g_displayCount))) {
		free(g_glCaps);
		g_glCaps = nil;
	}
	if ( !g_glCaps) {
		CheckOpenGLCaps(0, NULL, &g_displayCount); // will just update number of displays
        
        g_glCaps = (GLCaps *)malloc(sizeof(GLCaps) * g_displayCount);
        CheckOpenGLCaps(g_displayCount, g_glCaps, &g_displayCount);
        
        if (g_displayCount > 0) {
            NSString *info = [NSString stringWithFormat:
                        @"(Conduit) Main renderer info: VRAM %i MB, texSize %i px / 2D %i / 3D %i, renderer %s by %s (version %s), CG display id %i, OpenGL display id %i",
                        (int)g_glCaps->deviceVRAM / MEGABYTE,
                        (int)g_glCaps->maxRectTextureSize, (int)g_glCaps->maxTextureSize, (int)g_glCaps->max3DTextureSize,
                        g_glCaps->strRendererName, g_glCaps->strRendererVendor, g_glCaps->strRendererVersion,
                        g_glCaps->cgDisplayID,
                        g_glCaps->cglDisplayMask
                        ];
                        
            LXPlatformLogInfo(info);
        }
	}
}

LXBool LXPlatformHWSupportsFilteringForFloatTextures()
{
    uint32_t g = LXPlatformGetGPUClass();
    
    if (g == kLXGPUClass_Intel_950 || g == kLXGPUClass_Radeon_9600 || g == kLXGPUClass_Radeon_X1600)
        return NO;
    
    return YES;
    
    // TODO: reimplement correct check
    //return [LQGLPbuffer gpuSupportsBilinearFilteringWithFloatTexture];
}

LXBool LXPlatformHWSupportsFloatRenderTargets()
{
    uint32_t g = LXPlatformGetGPUClass();
    
    if (g == kLXGPUClass_Intel_950)
        return NO;
    
    return YES;
    
    // TODO: reimplement correct check
    //return [LQGLPbuffer gpuSupportsFloatPixels];
}

LXBool LXPlatformHWSupportsYCbCrTextures()
{
    return YES;  // all drivers on OS X support this
}

LXInteger LXPlatformGetHWAmountOfMemory()
{
    updateGLCaps();
    
    if (g_displayCount > 0) {
        return g_glCaps->deviceVRAM;
    }
    return -1;
}

void LXPlatformGetHWMaxTextureSize(int *outW, int *outH)
{
    int dim = 2046;  // sensible default, actual values will be read below
    int vram = 64*MEGABYTE;  // sensible default
    static BOOL didPrintInfo = NO;

    updateGLCaps();
    
    int i;
    for (i = 0; i < g_displayCount; i++) {
        if (g_glCaps[i].maxRectTextureSize > dim) {
            dim = g_glCaps[i].maxRectTextureSize;
            vram = g_glCaps[i].deviceVRAM;
        }
    }
    
    // check for limited VRAM and don't allow large textures if we risk running out of memory.
    // 8 bytes per pixel is a sensible compromise (16-bpc RGBA)
    const double maxVRAMPortionToAllow = 1.0 / 3.0;
    
    if (dim*dim*8 > vram*maxVRAMPortionToAllow) {
        dim = MAX(1920, round(sqrt(((double)vram * maxVRAMPortionToAllow) / 8.0)));
        
        if ( !didPrintInfo) {
            NSString *info = [NSString stringWithFormat:
                    @"(Conduit) Available VRAM on this graphics card is %i, so recommended max texture dimension is %i px", vram/MEGABYTE, dim];

            LXPlatformLogInfo((void *)info);

            didPrintInfo = YES;
        }
    }
    
	if (outW) *outW = dim;
	if (outH) *outH = dim;
}

static uint32_t getGPUClass()
{
    updateGLCaps();
    
    BOOL supportsFloatPixels = g_glCaps->fFloatPixels;
    BOOL supportsFragmentProgram = g_glCaps->fFragmentProg;
    
    NSLog(@"fragp %i floatp %i", supportsFragmentProgram, supportsFragmentProgram);
    
    if ( !supportsFragmentProgram)
        return kLXGPUClass_PreDX9;
    
    if ( !supportsFloatPixels)
        return kLXGPUClass_Intel_950;  // the Intels are the only Mac GPUs with ARBfp support but not float pixel formats
    
    
    NSString *rendStr = [[NSString stringWithUTF8String:g_glCaps->strRendererName] lowercaseString];
    if ( !rendStr)
        return kLXGPUClass_UnknownDX9;
    
    if ([rendStr rangeOfString:@"radeon"].location != NSNotFound) {
        if ([rendStr rangeOfString:@"9500"].location != NSNotFound ||
            [rendStr rangeOfString:@"9600"].location != NSNotFound ||
            [rendStr rangeOfString:@"9700"].location != NSNotFound)
            return kLXGPUClass_Radeon_9600;
        
        if ([rendStr rangeOfString:@"1600"].location != NSNotFound)
            return kLXGPUClass_Radeon_X1600;
        
        return kLXGPUClass_Radeon_X2600;
    }
    else if ([rendStr rangeOfString:@"nvidia"].location != NSNotFound) {
        if ([rendStr rangeOfString:@"5200"].location != NSNotFound)
            return kLXGPUClass_Geforce_5200;
        
        if ([rendStr rangeOfString:@"8600"].location != NSNotFound ||
            [rendStr rangeOfString:@"8800"].location != NSNotFound)
            return kLXGPUClass_Geforce_8600;
        
        return kLXGPUClass_Geforce_6600;
    }
    else if ([rendStr rangeOfString:@"intel"].location != NSNotFound) {
        if ([rendStr rangeOfString:@"gma"].location != NSNotFound || [rendStr rangeOfString:@"950"].location != NSNotFound)
            return kLXGPUClass_Intel_950;
        
        if ([rendStr rangeOfString:@"hd graphics 3000"].location != NSNotFound)
            return kLXGPUClass_Intel_HD3000;
        
        return kLXGPUClass_Intel_HD4000;
    }
    
    return kLXGPUClass_UnknownDX9;
}

static uint32_t g_gpuClass = 0;

uint32_t LXPlatformGetGPUClass()
{
    if (g_gpuClass == 0) {
        g_gpuClass = getGPUClass();
    }
    return g_gpuClass;
}


void *LXPlatformSharedNativeGraphicsContext()
{
    return g_ctx;
}

void LXPlatformSetSharedLQGLContext(void *obj)
{
    LQGLContext *ctx = (LQGLContext *)obj;

    if ( !g_lxAtomicLock) {
        LXPlatformCreateLocks_();
    }

    if (ctx != g_ctx) {
        [g_ctx release];
        g_ctx = [ctx retain];
        
        // ensure that these are checked early
        LXPlatformHWSupportsFilteringForFloatTextures();
        LXPlatformHWSupportsFloatRenderTargets();
    }
}

uint32_t LXPlatformMainDisplayIdentifier()
{
    return g_mainDisplayGLMask;
}

void _LXPlatformSetMainDisplayGLMask(CGOpenGLDisplayMask dmask)
{
    g_mainDisplayGLMask = dmask;
}

const char *LXPlatformRendererNameForDisplayIdentifier(LXInteger dmask)
{
    int i;
    for (i = 0; i < g_displayCount; i++) {
        if (g_glCaps[i].cglDisplayMask == dmask) {
            return g_glCaps[i].strRendererName;
        }
    }
    return NULL;
}


// returns a data blob containing the computer's GUID.
// the buffer in outData must be freed with _lx_free()
//
// based on: http://developer.apple.com/library/mac/#releasenotes/General/ValidateAppStoreReceipt/_index.html
//
static NSData *getMACAddressForBSDName(const char *bsdName)
{
    kern_return_t             kernResult;
    mach_port_t               master_port;
    CFMutableDictionaryRef    matchingDict;
    io_iterator_t             iterator;
    io_object_t               service;
    NSData                    *macAddress = nil;
 
    kernResult = IOMasterPort(MACH_PORT_NULL, &master_port);
    if (kernResult != KERN_SUCCESS) {
        printf("IOMasterPort returned %d\n", kernResult);
        return nil;
    }
 
    matchingDict = IOBSDNameMatching(master_port, 0, bsdName);
    if (!matchingDict) {
        printf("IOBSDNameMatching returned empty dictionary\n");
        return nil;
    }
 
    kernResult = IOServiceGetMatchingServices(master_port, matchingDict, &iterator);
    if (kernResult != KERN_SUCCESS) {
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
        return nil;
    }
 
    while((service = IOIteratorNext(iterator)) != 0) {
        io_object_t parentService;
 
        kernResult = IORegistryEntryGetParentEntry(service, kIOServicePlane,
                &parentService);
        if (kernResult == KERN_SUCCESS) {
            if (macAddress) [macAddress release];
 
            macAddress = (NSData *) IORegistryEntryCreateCFProperty(parentService,
                    CFSTR("IOMACAddress"), kCFAllocatorDefault, 0);
            IOObjectRelease(parentService);
        } else {
            printf("IORegistryEntryGetParentEntry returned %d\n", kernResult);
        }
 
        IOObjectRelease(iterator);
        IOObjectRelease(service);
    }
    return [macAddress autorelease];
}

LXBool LXPlatformGetSystemGUID(uint8_t **outData, size_t *outDataSize)
{
    NSData *macAddress = getMACAddressForBSDName("en0");
    if ([macAddress length] < 1) {
        //printf("...trying en1...\n");
        macAddress = getMACAddressForBSDName("en1");
    }
    if ([macAddress length] < 1) {
        //printf("...trying en2...\n");
        macAddress = getMACAddressForBSDName("en2");
    }
    if ([macAddress length] < 1) {
        printf("%s: failed\n", __func__);
        return NO;
    }
    
    if (outData && outDataSize) {
        size_t dataSize = [macAddress length];
        uint8_t *buf = _lx_malloc(dataSize);
        memcpy(buf, [macAddress bytes], dataSize);
        *outData = buf;
        *outDataSize = dataSize;
    }
 
    return YES;
}