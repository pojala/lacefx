/*
 *  LXShaderD3DTranslation.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 24.8.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXShaderTranslation.h"
#import <Foundation/Foundation.h>
#import "LXBasicTypes.h"


//#define DEBUGLOG(format, args...) NSLog(format, ## args);
#define DEBUGLOG(format, args...)



/*
  This code is from Conduit's PixMathTree.m,
  hence these shader dialect enum values match the definitions in PixMathTree.h.
*/

typedef enum {
	LXShaderDialect_ARBfp = 0,
	LXShaderDialect_FPClosure = 1,

	LXShaderDialect_ECMAScript = 0x100,
    
    LXShaderDialect_PixelBenderBytecode_Flash10 = 0x200,
    LXShaderDialect_PixelBenderBytecode_ENUMMAX = 0x2ff,
	
	LXShaderDialect_D3DPixelShader_2_0 = 0x400,
	LXShaderDialect_D3DPixelShader_2_a = 0x401,
	LXShaderDialect_D3DPixelShader_3_0 = 0x402,
    LXShaderDialect_D3DPixelShader_ENUMMAX = 0x4ff
} LXShaderDialect;



#define MAXSLIDERS 9
#define MAXCOLORPICKERS 5
#define MAXIMAGES 10



typedef struct {
	LXShaderDialect outputDialect;
	int samplersUsedArray[MAXIMAGES];
	int pickersUsedArray[MAXCOLORPICKERS];
	int registerCount;
	int floatConstCount;
	float floatConstArray[64];
	int colorConstCount;
	float colorConstArray[32 * 4];	
	NSMutableDictionary *regMap;  // dict for mapping arbfp variables into dx ps registers
	NSString *curInst;  // current instruction
    
    int psConstRegMaxIndex;
    
    LXShaderTranslationInfo *shaderInfo;
} PixelShaderParseState;




static BOOL is_str_from_charset(const char *str, const char *charSet)
{
	const size_t charSetCount = strlen(charSet);
	const size_t strLen = strlen(str);
    long i, j;	
	BOOL isValidChar = YES;
	
	for (i = 0; i < strLen; i++) {
		char ch = str[i];
		for (j = 0; j < charSetCount; j++) {
			if (charSet[j] == ch)
				break;
		}
		if (j == charSetCount) {  // this char in str was not found in the set
			isValidChar = NO;
			break;
		}
	}
	return isValidChar;
}

	
// this function gets called pretty often, so it uses C strings internally
static NSString *CleanARBFPArgument(NSString *arg, NSString **pRegMask)
{
	char str[256];
	if ( ![arg getCString:str maxLength:256 encoding:NSISOLatin1StringEncoding]) {
        NSLog(@"** can't get cstring for ARBfp argument '%@'", arg);
		return arg;
    }
		
	size_t len = strlen(str);
	if (len < 1)
		return arg;
        
	// clean whitespace from start and end; also remove any extra comma at end
	long start = 0;
	long end = len - 1;
	while (isspace(str[start]) && start <= end)
		start++;
	while ((isspace(str[end]) || str[end] == ',') && end >= start)
		end--;
		
    ///DEBUGLOG(@"for '%@' (%i), did clean: %i / %i", arg, len, start, end);
		
	if (end < start)
		return @"";
		
	if (pRegMask) {  // separate modifier if requested
		*pRegMask = nil;
	
		int dotPos = start;
		while (str[dotPos] != '.' && dotPos < end)
			dotPos++;
			
		if (dotPos < end) {
			char *regmaskStr = str + dotPos;
            const char chModifChars[] = ".rgbaxyzw";
			
            // only clip the original variable if the modifier was really a channel regmask
            // (it could also be something like .local[0], in which case we don't want to clip)
			if (is_str_from_charset(regmaskStr, chModifChars)) {
	            *pRegMask = [NSString stringWithUTF8String:regmaskStr];
	            end = dotPos - 1;
            }
		}
	}

	str[end + 1] = '\0';
    
    NSString *cleaned = [NSString stringWithUTF8String:(str + start)];

	///DEBUGLOG(@"cleaned arg: '%@' -> '%@'", arg, cleaned);

	return cleaned;
}



#define ADD_TEMP_MOV(constVarStr, constVarWithoutModif) \
					int tempRegIndex = regCount++; \
					NSString *tempMovStr = [NSString stringWithFormat:@"mov r%i, %@\n", tempRegIndex, constVarWithoutModif]; \
					 \
					if (*pPreInstrs == nil) \
						*pPreInstrs = [NSMutableString string]; \
					[(NSMutableString *)*pPreInstrs appendString:tempMovStr]; \
					 \
					[psInst appendFormat:@"r%i", tempRegIndex]; \
					NSRange r; \
					if ((r = [constVarStr rangeOfString:@"."]).location != NSNotFound) \
						[psInst appendString:[constVarStr substringFromIndex:r.location]]; \


#define CHECK_FOR_MULTIPLE_CONSTVAR_AND_STORE(constVarStr, constVarWithoutModif) \
				/*NSLog(@"   const %@: %i, %i", constVarStr, [usedConsts count], [usedConsts containsObject:constVarStr]);*/ \
				if (pPreInstrs == nil || [usedConsts count] == 0 || \
					([usedConsts count] == 1 && [usedConsts containsObject:constVarWithoutModif])) { \
					[psInst appendString:constVarStr]; \
				} else { \
					/* multiple const reads per instruction are not allowed, so we need a temp register */ \
					ADD_TEMP_MOV(constVarStr, constVarWithoutModif); \
				} \
				[usedConsts addObject:constVarWithoutModif]; \
				/*NSLog(@"   ...... %@: %i, %i", constVarStr, [usedConsts count], [usedConsts containsObject:constVarStr]);*/

				
#define	CHECK_FOR_MULTIPLE_SAME_REG_AND_STORE(varStr, varWithoutModif) \
				if (pPreInstrs == nil || ![usedConsts containsObject:varWithoutModif]) { \
					[psInst appendString:varStr]; \
				} else { \
					ADD_TEMP_MOV(varStr, varWithoutModif); \
				} \
				[usedConsts addObject:varWithoutModif];

				
static inline BOOL ARBFPArgumentIsScalarConst(NSString *constArg)
{
	static NSCharacterSet *numCharSet = nil;
    if ( !numCharSet)
        numCharSet = [[NSCharacterSet characterSetWithCharactersInString:@"1234567890.- "] retain];
    
	static NSCharacterSet *inverseNumCharSet = nil;
    if ( !inverseNumCharSet)
        inverseNumCharSet = [[numCharSet invertedSet] retain];

    return ([constArg rangeOfCharacterFromSet:inverseNumCharSet].location == NSNotFound &&
                        [constArg rangeOfCharacterFromSet:numCharSet].location != NSNotFound)
                || ([constArg isEqualToString:@"inf"] || [constArg isEqualToString:@"NaN"] || [constArg isEqualToString:@"nan"] ||
                                                         [constArg isEqualToString:@"NAN"] ||
                    [constArg isEqualToString:@"-inf"])
            ? YES : NO;
}

static inline BOOL ARBFPArgumentIsVectorConst(NSString *constArg, NSRange *pRange)
{
    NSRange range = [constArg rangeOfString:@"{"];
    if (pRange)
        *pRange = range;
    
    return (range.location != NSNotFound) ? YES : NO;
}

static NSString *ConvertARBFPInstructionArguments(NSArray *fpArgComps, PixelShaderParseState *pstate,
												  NSMutableSet *usedConsts,
												  NSString **pPreInstrs)
{
	int compCount = [fpArgComps count];
	NSMutableString *psInst = [NSMutableString string];
	int i;
	int regCount = pstate->registerCount;
	
	if ([pstate->curInst isEqualToString:@"cmp"]) {
		// CMP instruction has different order of operands in ARBfp vs. D3D, so must flip
		fpArgComps = [NSMutableArray arrayWithArray:fpArgComps];
		[(NSMutableArray *)fpArgComps exchangeObjectAtIndex:compCount-1 withObjectAtIndex:compCount-2];
	} 
	
	for (i = 0; i < compCount; i++) {
		NSString *arg = [fpArgComps objectAtIndex:i];
		NSRange range = { 0, 0 };
		BOOL done = NO;
		
		if ([arg length] < 1)
			done = YES;
		
		if (!done && i > 0)
			[psInst appendString:@", "];

		if (!done) {
			// check if this argument is a constant; d3d doesn't support inline constants,
			// so we need to collect them into the constant registers c<0-31>
			
			NSString *constArg = CleanARBFPArgument(arg, NULL);
            
            DEBUGLOG(@"inst %@, arg %i: cleaned '%@' (was '%@')", pstate->curInst, i, constArg, arg);
			
			if (ARBFPArgumentIsScalarConst(constArg)) {
				// this arg is a scalar float constant
				float f = [constArg floatValue];
				int cIndex = -1;

				///NSLog(@" -- got const: %f ('%@', %i)", f, arg, i);
				
				// look for an existing constant with the same value
				int k;
				for (k = 0; k < pstate->floatConstCount; k++) {
					if (f == pstate->floatConstArray[k]) {
						cIndex = k;
						///NSLog(@"found existing const for %f at index %i", f, cIndex);
						break;
					}
				}
				if (cIndex == -1) {
					// didn't find an existing const, so add a new one
					cIndex = pstate->floatConstCount;
					pstate->floatConstArray[cIndex] = f;
					(pstate->floatConstCount)++;
				}
				
				NSString *componentMask;
				switch (cIndex % 4) {
					default: 	componentMask = @"x";  break;
					case 1:  	componentMask = @"y";  break;
					case 2:  	componentMask = @"z";  break;
					case 3:  	componentMask = @"w";  break;
				}
				
				NSString *constVarWithoutModif = [NSString stringWithFormat:@"c%i", (cIndex / 4)];
				NSString *constVarStr = [NSString stringWithFormat:@"c%i.%@", (cIndex / 4), componentMask];

				///NSLog(@" storing const: %@", constVarStr);
													
				
				CHECK_FOR_MULTIPLE_CONSTVAR_AND_STORE(constVarStr, constVarWithoutModif);
				done = YES;
			}
			
			else if (ARBFPArgumentIsVectorConst(constArg, &range)) {
				// this arg is a color vector constant; collect the next three elements
				NSString *arg2 = (i+1 < compCount) ? [fpArgComps objectAtIndex:i+1] : (id)@"0.0";
				NSString *arg3 = (i+2 < compCount) ? [fpArgComps objectAtIndex:i+2] : (id)@"0.0";
				NSString *arg4 = (i+3 < compCount) ? [fpArgComps objectAtIndex:i+3] : (id)@"1.0";
				i += 3;
				
				float f1 = [[constArg substringFromIndex:range.location+1] floatValue];
				float f2 = [arg2 floatValue];
				float f3 = [arg3 floatValue];
				float f4 = [arg4 floatValue];
				int cIndex = -1;
				
				///NSLog(@" -- got const vec: %f %f %f %f; '%@', %i", f1, f2, f3, f4, arg, i);
				
				// look for an existing constant with the same value
				int k;
				for (k = 0; k < pstate->colorConstCount; k++) {
					if (f1 == pstate->colorConstArray[k*4+0] && f2 == pstate->colorConstArray[k*4+1] &&
						f3 == pstate->colorConstArray[k*4+2] && f4 == pstate->colorConstArray[k*4+3]) {
						cIndex = k;
						///NSLog(@"found existing const for {%f, %f, %f, %f} at index %i", f1, f2, f3, f4, cIndex);
						break;
					}
				}
				if (cIndex == -1) {
					// didn't find an existing const, so add a new one
					cIndex = pstate->colorConstCount;
					pstate->colorConstArray[cIndex * 4 + 0] = f1;
					pstate->colorConstArray[cIndex * 4 + 1] = f2;
					pstate->colorConstArray[cIndex * 4 + 2] = f3;
					pstate->colorConstArray[cIndex * 4 + 3] = f4;
					(pstate->colorConstCount)++;
				}
								
				NSString *constVarStr = [NSString stringWithFormat:@"c%i",
													pstate->psConstRegMaxIndex - cIndex];  // put color consts at the end
				
				CHECK_FOR_MULTIPLE_CONSTVAR_AND_STORE(constVarStr, constVarStr);
				done = YES;
			}
		}
			
		if (!done) {
			// this argument isn't a const, so check if it's a temp variable
			NSString *regMask = nil;
			NSString *varName = CleanARBFPArgument(arg, &regMask);
			NSString *psRegName = (NSString *)[pstate->regMap objectForKey:varName];
		
			DEBUGLOG(@"arg %@: is some kind of tempvar: %@ -> %@", arg, varName, psRegName);
			
			if (psRegName) {
				NSMutableString *psVarStr = [NSMutableString stringWithString:psRegName];
				if (regMask)
					[psVarStr appendString:regMask];
				
				// check if register is actually a const
				if ([psRegName characterAtIndex:0] == 'c') {
					CHECK_FOR_MULTIPLE_CONSTVAR_AND_STORE(psVarStr, psRegName);
				}
				
				// in ps_2_0, some instructions have a special rule about src/dest registers not being allowed to be same.
				// LRP: "dest register cannot be the same as 1st or 3rd source register."
				else if ([pstate->curInst isEqualToString:@"lrp"] && (i == 0 || i == 1 || i == compCount-1)) {
					CHECK_FOR_MULTIPLE_SAME_REG_AND_STORE(psVarStr, psRegName);
				}
				// POW: "dest register cannot be the same as 2nd source register."
				else if ([pstate->curInst isEqualToString:@"pow"] && (i == 0 || i == 2 || i == compCount-1)) {
					CHECK_FOR_MULTIPLE_SAME_REG_AND_STORE(psVarStr, psRegName);
				}

				else
					[psInst appendString:psVarStr];	
				
				done = YES;
			}
		}

		if ( !done) {  // unknown argument
			[psInst appendFormat:@"<? %@>", arg];
			NSLog(@"*** unknown argument in ps inststream: '%@'", arg);
		}
	}
	return psInst;
}


static void GetARBFPInstructionComponents(NSString *fpInst, NSString **outOpcode, NSMutableArray **outArgs)
{
	NSCharacterSet *spaceCharSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
	NSRange range;
    //int i;
	
    if ([fpInst length] < 1)
        return;
    
	/*if ([spaceCharSet characterIsMember:[fpInst characterAtIndex:0]]) {
		// delete any starting whitespace from instruction
		range = [fpInst rangeOfCharacterFromSet:[spaceCharSet invertedSet] options:NSLiteralSearch];
		range.length = [fpInst length] - range.location;
		fpInst = [fpInst substringWithRange:range];
	}*/
    fpInst = [fpInst stringByTrimmingCharactersInSet:spaceCharSet];
	
	/*
	NSArray *fpInstComps = [fpInst componentsSeparatedByString:@" "];
	int compCount = [fpInstComps count];
	if (compCount < 1)
		return nil;
	*/	
	NSString *fpOpcode = nil;  //[fpInstComps objectAtIndex:0];
	range = [fpInst rangeOfString:@" "];
	if (range.location != NSNotFound)
		fpOpcode = [fpInst substringToIndex:range.location];
	if (!fpOpcode || [fpOpcode length] < 1)
		return;
			
	if ([fpOpcode isEqualToString:@"END"])
		return;

	// get arguments; they are the comma-separated components after the instruction.
	NSMutableArray *fpInstComps = [NSMutableArray arrayWithArray:[[fpInst substringFromIndex:range.location+1] componentsSeparatedByString:@","]];

	if (outOpcode) *outOpcode = fpOpcode;
	if (outArgs)   *outArgs = fpInstComps;
}

static NSArray *GetARBFPInstructions(NSString *prog)
{
	NSRange range;
	
	// remove arb header
	range = [prog rangeOfString:@"!!ARBfp1.0"];
	if (range.location != NSNotFound) {
        DEBUGLOG(@"removing arb header: range %@", NSStringFromRange(range));
		prog = [prog substringFromIndex:range.location + range.length];
    }

	// remove comments
	NSScanner *scanner = [NSScanner scannerWithString:prog];
    [scanner setCharactersToBeSkipped:nil];
	NSMutableString *cleanedProg = [NSMutableString string];
    
    while ( ![scanner isAtEnd]) {
        NSString *s = nil;
        if ([scanner scanUpToString:@"#" intoString:&s]) {
            [cleanedProg appendString:s];
        }
        
        if ( ![scanner isAtEnd] && [prog characterAtIndex:[scanner scanLocation]] == '#') {
            DEBUGLOG(@"removing comment from program");
            [scanner scanUpToString:@"\n" intoString:&s];
        }
    }
    
	NSArray *fpComps = [cleanedProg componentsSeparatedByString:@";"];
	return fpComps;
}


#pragma mark --- Direct3D pixel shader output ---

static NSString *ConvertARBFPInstructionToD3DPS(NSString *fpInst, PixelShaderParseState *pstate)
{
	static NSDictionary *s_opcodeMapping_arb_to_d3d = nil;
	if ( !s_opcodeMapping_arb_to_d3d) {
		s_opcodeMapping_arb_to_d3d = [[NSDictionary alloc] initWithObjectsAndKeys:  // object is the D3D instr name, key is the ARBfp equivalent
											@"exp", @"ex2",
											@"log", @"lg2",
											@"crs", @"xpd",
											nil];
	}

	static NSCharacterSet *numCharSet = nil;
    if ( !numCharSet) {
        numCharSet = [[NSCharacterSet characterSetWithCharactersInString:@"1234567890.- "] retain];
    }
	NSRange range;
	///int i;

	NSString *fpOpcode = nil;
	NSMutableArray *fpInstComps = nil;
	GetARBFPInstructionComponents(fpInst, &fpOpcode, &fpInstComps);
    
	// because the code in this method was written to assume that the opcode is in the array, just add it at index 0...
	[fpInstComps insertObject:fpOpcode atIndex:0];
	int compCount = [fpInstComps count];

    ///DEBUGLOG(@"ARB inst components: %@", fpInstComps);
    
	
	if ([fpOpcode isEqualToString:@"TEMP"]) {
		// create mappings for declared temps, e.g. "var1" --> "r0"
		int r = pstate->registerCount;
		int i;
		for (i = 1; i < compCount; i++) {
			NSString *arg = [fpInstComps objectAtIndex:i];
			NSString *varName = CleanARBFPArgument(arg, NULL);
			
			NSString *psReg = [NSString stringWithFormat:@"r%i", r++];
			[pstate->regMap setObject:psReg forKey:varName];
			
			DEBUGLOG(@"   -- got PS temp: %@ (arg '%@') -> %@", varName, arg, psReg);
		}
		pstate->registerCount = r;
		return nil;
	}

	if ([fpOpcode isEqualToString:@"PARAM"]) {
		NSString *paramArg = [fpInstComps objectAtIndex:1];
		range = [paramArg rangeOfString:@"="];
		NSString *paramDst = [[paramArg substringToIndex:range.location] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
		NSString *paramSrc = [paramArg substringFromIndex:range.location + 1];
		
		 DEBUGLOG(@"PS param: dst %@ -- src %@", paramDst, paramSrc);
		
        if (pstate->shaderInfo && pstate->shaderInfo->shaderIsConduitFormat && [paramSrc rangeOfString:@"program.local"].location != NSNotFound) {
            // local params are not handled for Conduit format
        } else {
            NSMutableArray *argArr = [NSMutableArray arrayWithObject:paramSrc];
            if ([fpInstComps count] > 2)
                [argArr addObjectsFromArray:[fpInstComps subarrayWithRange:NSMakeRange(2, [fpInstComps count]-2)]];
		
            NSString *psReg = ConvertARBFPInstructionArguments(argArr, pstate, nil, NULL);
            [pstate->regMap setObject:psReg forKey:paramDst];
            
            DEBUGLOG(@"param mapping: %@ -> %@, reg %i", paramSrc, paramDst, psReg);
        }
		return nil;
	}
			
	if ([fpOpcode isEqualToString:@"TEX"]) {
		// convert texture loads
		NSString *dstVarName = CleanARBFPArgument([fpInstComps objectAtIndex:1], NULL);
		
		// find sampler index by looking at the texture argument
		int texIndex = 0;
		NSString *texArg = (NSString *)[fpInstComps objectAtIndex:3];
		range = [texArg rangeOfCharacterFromSet:numCharSet options:NSBackwardsSearch];
		if (range.location != NSNotFound)
			texIndex = [[texArg substringWithRange:range] intValue];
		
		pstate->samplersUsedArray[texIndex] = 1;  // mark that this sampler is in use
		
        NSString *texCoordD3D;
        NSString *texCoordVar = (NSString *)[fpInstComps objectAtIndex:2];
        if ([texCoordVar rangeOfString:@"fragment.texcoord"].location != NSNotFound) {
            // use default texcoords (same for all texcoords because D3D uses normalized)
            texCoordD3D = [NSString stringWithFormat:@"%@0", (pstate->outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v" : @"t"];
        } else {
            // a custom texcoord
            texCoordD3D = [pstate->regMap objectForKey:CleanARBFPArgument(texCoordVar, NULL)];
            DEBUGLOG(@"--- converting TEX: %@ --> %@", texCoordVar, texCoordD3D);
            
            if ([texCoordD3D length] < 1) texCoordD3D = @"v0";  // just in case
        }
        
		return [NSString stringWithFormat:
					@"texld %@, %@, s%i",
					 [pstate->regMap objectForKey:dstVarName], 
                     texCoordD3D,
					 //(pstate->outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v" : @"t",
					 //0, 		// use same texcoords for all texture loads
					 texIndex];
	}
	
	pstate->curInst = [fpOpcode lowercaseString];
    
    DEBUGLOG(@"curinst is: %@", pstate->curInst);
    NSCAssert(pstate->curInst, @"no instruction");

	// some opcodes have different names (e.g. "EX2" in ARBfp -> "exp" in D3D), so do mapping
	if ([s_opcodeMapping_arb_to_d3d objectForKey:pstate->curInst])
		pstate->curInst = [s_opcodeMapping_arb_to_d3d objectForKey:pstate->curInst];
		
	NSMutableString *psInst = [NSMutableString stringWithString:pstate->curInst];
	[psInst appendString:@" "];

	// ps_2_0 only allows reading from one const within one instruction,
	// so we need to keep track of how many consts are read from and move them into free
	// temp registers as necessary
	NSMutableSet *usedConsts = [NSMutableSet setWithCapacity:4];
	NSString *preInstrStr = nil;

	if ([fpInstComps count] > 0) {
		[psInst appendString:ConvertARBFPInstructionArguments(
									[fpInstComps subarrayWithRange:NSMakeRange(1, [fpInstComps count]-1)],
									pstate,
									usedConsts,
									&preInstrStr
								)];
	}
	///else NSLog(@"** fpInstComps count is zero");

	if (preInstrStr)
		[psInst insertString:preInstrStr atIndex:0];
									
	return psInst;
}



static NSString *ConvertARBFragmentProgramIntermediateToD3DPixelShaderAssembly(NSString *prog, LXShaderDialect outputDialect,
                                                                               LXShaderTranslationInfo *shaderSrcInfo)
{
    int i;
	PixelShaderParseState pstate;
	memset(&pstate, 0, sizeof(PixelShaderParseState));
	
	pstate.outputDialect = outputDialect;
    pstate.shaderInfo = shaderSrcInfo;
    
    
    const BOOL isPS3 = (outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? YES : NO;
    int highestFreePSVar = (isPS3) ? 63 : 31;
    
	// create the regmap that maps ARBfp register names into the fixed pixel shader register names
	pstate.regMap = [NSMutableDictionary dictionary];
	[pstate.regMap setObject:@"oC0" forKey:@"oC0"];
	[pstate.regMap setObject:@"oC0" forKey:@"result.color"];

    if (shaderSrcInfo && shaderSrcInfo->shaderIsConduitFormat) {
        const int numProgramLocals = 3 + MAXCOLORPICKERS;       // HARDCODED
        highestFreePSVar -= numProgramLocals;

        // imageinfo in first reg
        [pstate.regMap setObject:[NSString stringWithFormat:@"c%i", highestFreePSVar + 1] forKey:@"imageInfo"];    

        // sliders in two following regs
        [pstate.regMap setObject:[NSString stringWithFormat:@"c%i", highestFreePSVar + 2] forKey:@"slide1"];
        [pstate.regMap setObject:[NSString stringWithFormat:@"c%i", highestFreePSVar + 3] forKey:@"slide2"];

        // reserve const vars for each picker in use
        int pickCount = 0;
        for (i = 1; i < MIN(shaderSrcInfo->arePickersUsedCount, MAXCOLORPICKERS); i++) {
            if (shaderSrcInfo->arePickersUsed[i]) {
                [pstate.regMap setObject:[NSString stringWithFormat:@"c%i", highestFreePSVar + 4 + pickCount] forKey:[NSString stringWithFormat:@"pick%i", pickCount + 1]];
        
                pstate.pickersUsedArray[i] = YES;
                pickCount++;
            }
        }
        shaderSrcInfo->regIndexForFirstProgramLocal = highestFreePSVar + 1;
        shaderSrcInfo->numProgramLocalsInUse = numProgramLocals;
	}
    else {
        const int maxProgramLocals = (isPS3) ? 24 : 14;
        
        highestFreePSVar -= maxProgramLocals;
        
        for (i = 0; i < maxProgramLocals; i++) {
            [pstate.regMap setObject:[NSString stringWithFormat:@"c%i", highestFreePSVar + i + 1]
                              forKey:[NSString stringWithFormat:@"program.local[%i]", i]];
        }
        
        if (shaderSrcInfo) {
            shaderSrcInfo->regIndexForFirstProgramLocal = highestFreePSVar + 1;
            shaderSrcInfo->numProgramLocalsInUse = maxProgramLocals;  // TODO: should check this from actual instructions
        }
        DEBUGLOG(@"shader is plain-jane format; highest free ps var now %i", highestFreePSVar);
    }
    
    pstate.psConstRegMaxIndex = highestFreePSVar;

	
	// normalized texcoords are expected in unit 0, so declare a mapping
	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v0.xy" : @"t0.xy"
				   forKey:@"fragment.texcoord[0]"];

	// TODO: should make the mapping understand channel masks, so these ugly separate x/y mappings would be unnecessary
	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v0.x" : @"t0.x"
				   forKey:@"fragment.texcoord[0].x"];
	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v0.y" : @"t0.y"
				   forKey:@"fragment.texcoord[0].y"];
				   
	// pixel texcoords are expected in unit 7.
	// 2007.12.28 -- mapping changed to texunit 1 for d3d (associated change in ConduitD3DRender.cpp)
	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v1.xy" : @"t1.xy"
				   forKey:@"fragment.texcoord[7]"];

	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v1.x" : @"t1.x"
				   forKey:@"fragment.texcoord[7].x"];
	[pstate.regMap setObject:(outputDialect >= LXShaderDialect_D3DPixelShader_3_0) ? @"v1.y" : @"t1.y"
				   forKey:@"fragment.texcoord[7].y"];

	// break program into instructions, and convert
	NSArray *fpComps = GetARBFPInstructions(prog);
	NSMutableArray *psComps = [NSMutableArray arrayWithCapacity:[fpComps count]];
	
	NSEnumerator *enumerator = [fpComps objectEnumerator];
	NSString *instStr;
	while (instStr = [enumerator nextObject])
	{
        if ([instStr rangeOfString:@"END" options:NSCaseInsensitiveSearch].location != NSNotFound &&
                        [[[instStr uppercaseString] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] hasPrefix:@"END"]) {
            DEBUGLOG(@"-- program end");
            break;
        }
    
		NSString *psStr = nil;
		if ([instStr length] > 2) {
            DEBUGLOG(@" -- converting instStr: %@ ---", instStr);
			psStr = ConvertARBFPInstructionToD3DPS(instStr, &pstate);
		}			
		if (psStr)
			[psComps addObject:psStr];
	}
	
	NSString *psProg = [psComps componentsJoinedByString:@"\n"];
	
	BOOL isPs_3 = (outputDialect >= LXShaderDialect_D3DPixelShader_3_0);
	
	// add constant definitions
	NSMutableString *constDefStr = [NSMutableString string];
	
	for (i = 0; i < pstate.floatConstCount; i += 4) {
		[constDefStr appendFormat:@"def c%i, ", i/4];
		[constDefStr appendFormat:@"%ff, ", pstate.floatConstArray[i]];
		[constDefStr appendFormat:@"%ff, ", (i+1 < pstate.floatConstCount) ? pstate.floatConstArray[i+1] : 0.0f];
		[constDefStr appendFormat:@"%ff, ", (i+2 < pstate.floatConstCount) ? pstate.floatConstArray[i+2] : 0.0f];
		[constDefStr appendFormat:@"%ff\n", (i+3 < pstate.floatConstCount) ? pstate.floatConstArray[i+3] : 0.0f];
	}
	
	for (i = 0; i < pstate.colorConstCount; i++) {
		[constDefStr appendFormat:@"def c%i, ", pstate.psConstRegMaxIndex - i];
		[constDefStr appendFormat:@"%ff, %ff, %ff, %ff\n", pstate.colorConstArray[i*4+0], pstate.colorConstArray[i*4+1], 
														   pstate.colorConstArray[i*4+2], pstate.colorConstArray[i*4+3] ];
	}

	//BOOL gotSamplers = NO;	
	for (i = 0; i < 8; i++) {
		if (pstate.samplersUsedArray[i]) {
			[constDefStr appendFormat:@"dcl_2d s%i\n", i];
			//gotSamplers = YES;
		}
	}
	if (1) { //gotSamplers) {   // always include these declarations for nodes that need normalized coords
		if (isPs_3) {
			[constDefStr appendFormat:@"dcl_texcoord%i v%i.xyzw\n", 0, 0]; // DX ps_3_0 style
			[constDefStr appendFormat:@"dcl_texcoord%i v%i.xyzw\n", 1, 1];
		}
		else {
			[constDefStr appendFormat:@"dcl t%i.xyzw\n", 0];
			[constDefStr appendFormat:@"dcl t%i.xyzw\n", 1];  // texcoord 1 is declared for Gradient, it contains normalized coords
		}
	}
		
	psProg = [NSString stringWithFormat:@"%@\n%@%@\n", (isPs_3) ? @"ps_3_0" : @"ps_2_0", constDefStr, psProg];
	
    if (shaderSrcInfo && shaderSrcInfo->psRegMapPtr) {
        *(shaderSrcInfo->psRegMapPtr) = (LXMapPtr)[pstate.regMap copy];
    }
    
	DEBUGLOG(@"converted ARBfp: %@\n----> dxps: %@\n", prog, psProg);

	return psProg;
}



LXSuccess LXConvertShaderString_OpenGLARBfp_to_D3DPS(const char *str, const char *psFormat,
                                                            char **outStr,
                                                            LXShaderTranslationInfo *shaderInfo)
{
    if ( !str || !outStr) return NO;
    
    NSString *prog = [NSString stringWithUTF8String:str];
    
    LXShaderDialect dialect = LXShaderDialect_D3DPixelShader_2_0;
    if (psFormat) {
        if (0 == strcmp(psFormat, "ps_2_a"))
            dialect = LXShaderDialect_D3DPixelShader_2_a;
        else if (0 == strcmp(psFormat, "ps_3_0"))
            dialect = LXShaderDialect_D3DPixelShader_3_0;
    }
    
    DEBUGLOG(@"---- starting translation: format is %s, program is: %@ -------\n", psFormat, prog);
    
    NSString *outProg = ConvertARBFragmentProgramIntermediateToD3DPixelShaderAssembly(prog, dialect, shaderInfo);
    
    if ([outProg rangeOfString:@"<?"].location != NSNotFound) {
        NSLog(@"*** shader conversion failed: original program is: %@\nresult program is: %@", prog, outProg);
        return NO;
    }
    
    NSLog(@"converted shader program to D3D: %@", outProg);
    
    if ([outProg length] < 7) return NO;
    
    const char *utf8Prog = [outProg UTF8String];
    const size_t len = strlen(utf8Prog);
    
    *outStr = _lx_malloc(len + 1);
    (*outStr)[len] = 0;
    memcpy(*outStr, utf8Prog, len);
    
    return YES;
}



#pragma mark --- OpenGL ES 2 high-level shader output ---

static NSString *translateARBFPVariableToOpenGLES(NSString *str)
{
    NSRange range;
    
    if ([str isEqualToString:@"result.color"]) {
        return @"gl_FragColor";
    }
    if ((range = [str rangeOfString:@"fragment.texcoord[0]"]).location != NSNotFound) {
        NSString *suffix = (range.location+range.length < [str length]) ? [str substringFromIndex:range.location+range.length] : @"";
        if ([suffix length] < 2) {
            // this is a vec2, we must splat it to vec4
            return @"vec4(texcoordPx.s, texcoordPx.t, 0.0, 0.0)";
        } else {
            return [NSString stringWithFormat:@"texcoordPx%@", suffix];
        }
    }
    if ((range = [str rangeOfString:@"fragment.texcoord[7]"]).location != NSNotFound) {
        NSString *suffix = (range.location+range.length < [str length]) ? [str substringFromIndex:range.location+range.length] : @"";
        if ([suffix length] < 2) {
            // this is a vec2, we must splat it to vec4
            return @"vec4(texcoord0.s, texcoord0.t, 0.0, 0.0)";
        } else {
            return [NSString stringWithFormat:@"texcoord0%@", suffix];
        }
    }
    
    return str;
}

static NSArray *translateARBFPArgumentsToOpenGLES(NSArray *args)
{
    if ([args count] < 1) return [NSArray array];
    
    NSMutableArray *arr = [NSMutableArray arrayWithCapacity:4];
    
    LXInteger n = [args count];
    LXInteger i;
    NSRange range;
    
    for (i = 0; i < n; i++) {
        NSString *arg = [args objectAtIndex:i];
        
        if ([arg length] < 1) continue;
        
        arg = CleanARBFPArgument(arg, NULL);
        
        NSString *convArg = nil;
        
        if (ARBFPArgumentIsVectorConst(arg, &range)) {
            // this arg is a color vector constant; collect the next three elements
            arg = [arg substringFromIndex:range.location+1];
            NSString *arg2 = (i+1 < n) ? [args objectAtIndex:i+1] : (id)@"0.0";
            NSString *arg3 = (i+2 < n) ? [args objectAtIndex:i+2] : (id)@"0.0";
            NSString *arg4 = (i+3 < n) ? [args objectAtIndex:i+3] : (id)@"1.0";
            i += 3;
            
            if ((range = [arg4 rangeOfString:@"}"]).location != NSNotFound)
                arg4 = [arg4 substringToIndex:range.location];
            
            arg2 = CleanARBFPArgument(arg2, NULL);
            arg3 = CleanARBFPArgument(arg3, NULL);
            arg4 = CleanARBFPArgument(arg4, NULL);
            
            if ([arg doubleValue] == 0.0)  arg = @"0.0";
            if ([arg2 doubleValue] == 0.0) arg2 = @"0.0";
            if ([arg3 doubleValue] == 0.0) arg3 = @"0.0";
            if ([arg4 doubleValue] == 0.0) arg4 = @"0.0";
            
            convArg = [NSString stringWithFormat:@"vec4(%@, %@, %@, %@)", arg, arg2, arg3, arg4];
        }
        else if (ARBFPArgumentIsScalarConst(arg)) {
            double v = [arg doubleValue];
            if (v == 0.0 || v == -0.0)  arg = @"0.0";
            convArg = arg;
        }
        else {
            convArg = translateARBFPVariableToOpenGLES(arg);
        }
        
        [arr addObject:convArg];
    }
    
    return arr;
}

static void addVec4SplatIfNeeded_unary(NSString *dst, NSString *src1, NSString **prefix, NSString **postfix)
{
    BOOL dstIsVector = ([dst rangeOfString:@"."].location == NSNotFound);
    BOOL src1IsVector = ([src1 rangeOfString:@"."].location == NSNotFound);
    if (dstIsVector && !src1IsVector) {
        *prefix = [NSString stringWithFormat:@"vec4(%@", *prefix];
        *postfix = [NSString stringWithFormat:@"%@)", *postfix];
    }
}

static BOOL appendDstComponentMaskToSrcIfNeeded_unary(NSString *dst, NSString **src)
{
    NSRange range;
    if ((range = [dst rangeOfString:@"."]).location != NSNotFound) {
        NSString *dstMask = [dst substringFromIndex:range.location+1];
        if ([*src rangeOfString:@"."].location == NSNotFound) {
            *src = [NSString stringWithFormat:@"%@.%@", *src, dstMask];
        }
        return YES;
    }
    return NO;
}

static BOOL appendDstComponentMaskToSrcIfNeeded_binary(NSString *dst, NSString **src1, NSString **src2)
{
    NSRange range;
    if ((range = [dst rangeOfString:@"."]).location != NSNotFound) {
        NSString *dstMask = [dst substringFromIndex:range.location+1];
        if ([*src1 rangeOfString:@"."].location == NSNotFound) {
            *src1 = [NSString stringWithFormat:@"%@.%@", *src1, dstMask];
        }
        if ([*src2 rangeOfString:@"."].location == NSNotFound) {
            *src2 = [NSString stringWithFormat:@"%@.%@", *src2, dstMask];
        }
        return YES;
    }
    return NO;
}

static BOOL appendDstComponentMaskToSrcIfNeeded_ternary(NSString *dst, NSString **src1, NSString **src2, NSString **src3)
{
    NSRange range;
    if ((range = [dst rangeOfString:@"."]).location != NSNotFound) {
        NSString *dstMask = [dst substringFromIndex:range.location+1];
        if ([*src1 rangeOfString:@"."].location == NSNotFound) {
            *src1 = [NSString stringWithFormat:@"%@.%@", *src1, dstMask];
        }
        if ([*src2 rangeOfString:@"."].location == NSNotFound) {
            *src2 = [NSString stringWithFormat:@"%@.%@", *src2, dstMask];
        }
        if ([*src3 rangeOfString:@"."].location == NSNotFound) {
            *src3 = [NSString stringWithFormat:@"%@.%@", *src3, dstMask];
        }
        return YES;
    }
    return NO;
}

static NSString *ConvertARBFPInstructionToOpenGLESShader(NSString *fpInst)
{
    NSRange range;
    
	NSString *fpOpcode = nil;
	NSMutableArray *fpInstComps = nil;
	GetARBFPInstructionComponents(fpInst, &fpOpcode, &fpInstComps);
    
    if ([fpOpcode isEqualToString:@"TEMP"]) {
        NSMutableString *s = [NSMutableString string];
        for (NSString *nameArg in fpInstComps) {
            nameArg = CleanARBFPArgument(nameArg, NULL);
            [s appendFormat:@"vec4 %@; ", nameArg];
        }
        return s;
    }
    if ([fpOpcode isEqualToString:@"PARAM"]) {
        NSString *paramArg = [fpInstComps objectAtIndex:0];
		range = [paramArg rangeOfString:@"="];
		NSString *paramDst = [[paramArg substringToIndex:range.location] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
		NSString *paramSrc = [paramArg substringFromIndex:range.location + 1];
		
        DEBUGLOG(@"PS param: dst %@ -- src %@", paramDst, paramSrc);
        
        NSMutableString *s = [NSMutableString string];
        [s appendFormat:@"vec4 %@ = ", paramDst];
        
        if ((range = [paramSrc rangeOfString:@"program.local["]).location != NSNotFound) {
            int varIndex = [[paramSrc substringFromIndex:range.location+range.length] intValue];
            [s appendFormat:@"programLocal%i", varIndex];
        }
        else {
            if (ARBFPArgumentIsVectorConst(paramSrc, &range)) {
                NSString *arg1 = [paramSrc substringFromIndex:range.location+1];
				NSString *arg2 = [fpInstComps objectAtIndex:1];
				NSString *arg3 = [fpInstComps objectAtIndex:2];
				NSString *arg4 = [fpInstComps objectAtIndex:3];
                if ((range = [arg4 rangeOfString:@"}"]).location != NSNotFound)
                    arg4 = [arg4 substringToIndex:range.location];
                arg2 = CleanARBFPArgument(arg2, NULL);
                arg3 = CleanARBFPArgument(arg3, NULL);
                arg4 = CleanARBFPArgument(arg4, NULL);
                
                [s appendFormat:@"vec4(%@, %@, %@, %@)", arg1, arg2, arg3, arg4];
            }
            else {
                [s appendString:paramSrc];
            }
            [s insertString:@"const " atIndex:0];
        }
        [s appendString:@";"];
        return s;
    }
    if ([fpOpcode isEqualToString:@"TEX"]) {
        NSString *dstVarName = CleanARBFPArgument([fpInstComps objectAtIndex:0], NULL);
		
        NSCharacterSet *numCharSet = [NSCharacterSet characterSetWithCharactersInString:@"1234567890.- "];
        
		// find sampler index by looking at the texture argument
		int texIndex = 0;
		NSString *texArg = (NSString *)[fpInstComps objectAtIndex:3];
		range = [texArg rangeOfCharacterFromSet:numCharSet options:NSBackwardsSearch];
		if (range.location != NSNotFound)
			texIndex = [[texArg substringWithRange:range] intValue];
        
        // image1 = texture2d(sampler0, vec2(texcoord0.s, texcoord0.t))
        return [NSString stringWithFormat:@"%@ = texture2D(sampler%i, vec2(texcoord%i.s, texcoord%i.t));",
                dstVarName, texIndex, texIndex, texIndex];
    }
    
    /*
     There are 33 fragment program instructions.  The instructions and
     their respective input and output parameters are summarized in
     Table X.5.
     
     Instruction    Inputs  Output   Description
     -----------    ------  ------   --------------------------------
     ABS            v       v        absolute value
     ADD            v,v     v        add
     CMP            v,v,v   v        compare
     COS            s       ssss     cosine with reduction to [-PI,PI]
     DP3            v,v     ssss     3-component dot product
     DP4            v,v     ssss     4-component dot product
     DPH            v,v     ssss     homogeneous dot product
     DST            v,v     v        distance vector
     EX2            s       ssss     exponential base 2
     FLR            v       v        floor
     FRC            v       v        fraction
     KIL            v       v        kill fragment
     LG2            s       ssss     logarithm base 2
     LIT            v       v        compute light coefficients
     LRP            v,v,v   v        linear interpolation
     MAD            v,v,v   v        multiply and add
     MAX            v,v     v        maximum
     MIN            v,v     v        minimum
     MOV            v       v        move
     MUL            v,v     v        multiply
     POW            s,s     ssss     exponentiate
     RCP            s       ssss     reciprocal
     RSQ            s       ssss     reciprocal square root
     SCS            s       ss--     sine/cosine without reduction
     SGE            v,v     v        set on greater than or equal
     SIN            s       ssss     sine with reduction to [-PI,PI]
     SLT            v,v     v        set on less than
     SUB            v,v     v        subtract
     SWZ            v       v        extended swizzle
     TEX            v,u,t   v        texture sample
     TXB            v,u,t   v        texture sample with bias
     TXP            v,u,t   v        texture sample with projection
     XPD            v,v     v        cross product
     */
    
    NSArray *args = translateARBFPArgumentsToOpenGLES(fpInstComps);
    
    NSString *prefix = @"";
    NSString *postfix = @"";
    if ((range = [fpOpcode rangeOfString:@"_SAT"]).location != NSNotFound) {
        fpOpcode = [fpOpcode substringToIndex:range.location];
        prefix = @"clamp(";
        postfix = @", 0.0, 1.0)";
    }
    
    
    if ([fpOpcode isEqualToString:@"MOV"]) {
        NSString *dst = [args objectAtIndex:0];
        NSString *src1 = [args objectAtIndex:1];
        if ( !appendDstComponentMaskToSrcIfNeeded_unary(dst, &src1)) {
            addVec4SplatIfNeeded_unary(dst, src1, &prefix, &postfix);
        }
        
        return [NSString stringWithFormat:@"%@ = %@%@%@;", dst, prefix, src1, postfix];
    }
    
    // arithmetic operators
    NSDictionary *binaryOpMap = [NSDictionary dictionaryWithObjectsAndKeys:
                                 @"+", @"ADD",
                                 @"-", @"SUB",
                                 @"*", @"MUL",
                                 @"/", @"DIV",
                                 nil];
    if ([[binaryOpMap allKeys] containsObject:fpOpcode]) {
        NSString *op = [binaryOpMap objectForKey:fpOpcode];
        
        NSString *dst = [args objectAtIndex:0];
        NSString *src1 = [args objectAtIndex:1];
        NSString *src2 = [args objectAtIndex:2];
        if ( !appendDstComponentMaskToSrcIfNeeded_binary(dst, &src1, &src2)) {
            addVec4SplatIfNeeded_unary(dst, src1, &prefix, &postfix);
        }
        
        return [NSString stringWithFormat:@"%@ = %@%@ %@ %@%@;", dst, prefix, src1, op, src2, postfix];
    }
    
    if ([fpOpcode isEqualToString:@"MAD"]) {
        NSString *dst = [args objectAtIndex:0];
        NSString *src1 = [args objectAtIndex:1];
        NSString *src2 = [args objectAtIndex:2];
        NSString *src3 = [args objectAtIndex:3];
        if ( !appendDstComponentMaskToSrcIfNeeded_ternary(dst, &src1, &src2, &src3)) {
            addVec4SplatIfNeeded_unary(dst, src1, &prefix, &postfix);
        }
        
        return [NSString stringWithFormat:@"%@ = %@%@ * %@ + %@%@;", dst, prefix, src1, src2, src3, postfix];
    }
    
    if ([fpOpcode isEqualToString:@"RCP"]) {
        return [NSString stringWithFormat:@"%@ = %@1.0 / %@%@;", [args objectAtIndex:0], prefix, [args objectAtIndex:1],  postfix];
    }
    
    if ([fpOpcode isEqualToString:@"LRP"]) {
        NSString *dst = [args objectAtIndex:0];
        NSString *src1 = [args objectAtIndex:3];
        NSString *src2 = [args objectAtIndex:2];
        if ( !appendDstComponentMaskToSrcIfNeeded_unary(dst, &src1)) {
            addVec4SplatIfNeeded_unary(dst, src1, &prefix, &postfix);
        }
        if (ARBFPArgumentIsScalarConst(src1) && !ARBFPArgumentIsScalarConst(src2)) {
            src1 = [NSString stringWithFormat:@"vec4(%@)", src1];
        }
        else if ( !ARBFPArgumentIsScalarConst(src1) && ARBFPArgumentIsScalarConst(src2)) {
            src2 = [NSString stringWithFormat:@"vec4(%@)", src2];
        }
        
        return [NSString stringWithFormat:@"%@ = %@mix(%@, %@, %@)%@;", dst, prefix, src1, src2, [args objectAtIndex:1], postfix];
    }
    
    if ([fpOpcode isEqualToString:@"DP3"]) {
        return [NSString stringWithFormat:@"%@ = %@dot(vec3(%@), vec3(%@))%@;", [args objectAtIndex:0], prefix, [args objectAtIndex:1], [args objectAtIndex:2], postfix];
    }
    
    if ([fpOpcode isEqualToString:@"CMP"]) {
        /*
         result.x = (tmp0.x < 0.0) ? tmp1.x : tmp2.x;
         result.y = (tmp0.y < 0.0) ? tmp1.y : tmp2.y;
         result.z = (tmp0.z < 0.0) ? tmp1.z : tmp2.z;
         result.w = (tmp0.w < 0.0) ? tmp1.w : tmp2.w;
         */
        NSString *dst = [args objectAtIndex:0];
        NSString *cmp = [args objectAtIndex:1];
        NSString *src1 = [args objectAtIndex:2];
        NSString *src2 = [args objectAtIndex:3];
        NSMutableString *s = [NSMutableString string];
        
        if ([dst rangeOfString:@"."].location == NSNotFound) {
            BOOL cmpIsVector = ([cmp rangeOfString:@"."].location == NSNotFound);
            BOOL src1IsVector = ([src1 rangeOfString:@"."].location == NSNotFound);
            BOOL src2IsVector = ([src2 rangeOfString:@"."].location == NSNotFound);
            
            NSString *suffixes[4] = { @".r", @".g", @".b", @".a" };
            LXInteger j;
            for (j = 0; j < 4; j++) {
                NSString *compSuffix = suffixes[j];
                
                [s appendFormat:@"%@%@ = ", dst, compSuffix];
                [s appendFormat:@"(%@%@ < 0.0) ? ", cmp, (cmpIsVector ? compSuffix : @"")];
                [s appendFormat:@"%@%@ : ", src1, (src1IsVector ? compSuffix : @"")];
                [s appendFormat:@"%@%@",    src2, (src2IsVector ? compSuffix : @"")];
                [s appendString:@"; "];
            }
        }
        else {
            [s appendFormat:@"%@ = (%@ < 0.0) ? %@ : %@;", dst, cmp, src1, src2];
        }
        return s;
    }
    
    NSDictionary *unaryFuncMap = [NSDictionary dictionaryWithObjectsAndKeys:
                                  @"abs", @"ABS",
                                  @"cos", @"COS",
                                  @"exp2", @"EX2",
                                  @"floor", @"FLR",
                                  @"fract", @"FRC",
                                  @"log2", @"LG2",
                                  @"inversesqrt", @"RSQ",
                                  @"sin", @"SIN",
                                  nil];
    if ([[unaryFuncMap allKeys] containsObject:fpOpcode]) {
        NSString *func = [unaryFuncMap objectForKey:fpOpcode];
        return [NSString stringWithFormat:@"%@ = %@%@(%@)%@;", [args objectAtIndex:0], prefix, func, [args objectAtIndex:1], postfix];
    }
    
    NSDictionary *binaryFuncMap = [NSDictionary dictionaryWithObjectsAndKeys:
                                   @"dot", @"DP4",
                                   @"distance", @"DST",
                                   @"max", @"MAX",
                                   @"min", @"MIN",
                                   @"pow", @"POW",
                                   @"lessThan", @"SLT",
                                   @"greaterThanEqual", @"SGE",
                                   @"cross", @"XPD",
                                   nil];
    if ([[binaryFuncMap allKeys] containsObject:fpOpcode]) {
        NSString *func = [binaryFuncMap objectForKey:fpOpcode];
        return [NSString stringWithFormat:@"%@ = %@%@(%@, %@)%@;", [args objectAtIndex:0], prefix, func, [args objectAtIndex:1], [args objectAtIndex:2], postfix];
    }

    return nil;
}


LXSuccess LXConvertShaderString_OpenGLARBfp_to_ES2_function_body(const char *str,
                                                                 char **outStr)
{
    if ( !str || !outStr) return NO;
    
    NSString *fp = [NSString stringWithUTF8String:str];
    
    NSArray *fpComps = GetARBFPInstructions(fp);
	NSMutableArray *newComps = [NSMutableArray arrayWithCapacity:[fpComps count]];
	
	NSEnumerator *enumerator = [fpComps objectEnumerator];
	NSString *instStr;
	while (instStr = [enumerator nextObject])
	{
        if ([instStr rangeOfString:@"END" options:NSCaseInsensitiveSearch].location != NSNotFound &&
            [[[instStr uppercaseString] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] hasPrefix:@"END"]) {
            DEBUGLOG(@"-- program end");
            break;
        }
        
		NSString *fsStr = nil;
		if ([instStr length] > 2) {
            instStr = [instStr stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            DEBUGLOG(@" -- converting instStr: %@ ---", instStr);
			fsStr = ConvertARBFPInstructionToOpenGLESShader(instStr);
		}
		if (fsStr) {
            [newComps addObject:fsStr];
        }
	}
    
	NSString *fragShaderProg = [newComps componentsJoinedByString:@"\n"];
    
    const char *utf8Prog = [fragShaderProg UTF8String];
    const size_t len = strlen(utf8Prog);
    
    *outStr = _lx_malloc(len + 1);
    (*outStr)[len] = 0;
    memcpy(*outStr, utf8Prog, len);
    return YES;
}

