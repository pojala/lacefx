/*
 *  LXFPClosure.c
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 17.2.2007.
 *  Copyright 2007 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXFPClosure.h"
#include "LXBasicTypes.h"
#include "LXStringUtils.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>



typedef enum {
	FPClosureToken_NOP = 0,
	
	FPClosureToken_ABS,
	FPClosureToken_ADD,
	FPClosureToken_CMP,
	FPClosureToken_DP3,
	FPClosureToken_EX2,
	FPClosureToken_FLR,
	FPClosureToken_FRC,
	FPClosureToken_LG2,
	FPClosureToken_LRP,
	
	FPClosureToken_MAD = 10,
	FPClosureToken_MAX,
	FPClosureToken_MIN,
	FPClosureToken_MOV,
	FPClosureToken_MUL,
	FPClosureToken_POW,
	FPClosureToken_RCP,
	FPClosureToken_RSQ,
	FPClosureToken_SIN,
	FPClosureToken_COS,
	FPClosureToken_SUB,
    
    FPClosureTokenOpcodeMask = (1 << 8) - 1
} FPClosureToken;

typedef enum {
    FPTokenModifier_SAT = 1 << 8
} FPClosureTokenModifier;


typedef enum {							// # of anteceding values:
	FPClosureArg_ConstZero = 1,	  		// 0
	FPClosureArg_ConstFloat,  			// 1 (the const as a float)
	FPClosureArg_ConstFloatVec,			// 4 (the vector const)

	FPClosureArg_Register,				// 1 (index of the register)
} FPClosureArg;


typedef union {
	unsigned int arg;  // this contains both an FPClosureArg, as well as a swizzle mask in the high bits (use SWIZZLEMASK/SWIZZLESHR to access it)
	int index;
	float f;
} FPClosureArgValue;




typedef struct _FPClosure {
	char inputsInUse[16];
	unsigned int tokenCount;
	FPClosureToken *tokenList;			// array of token instr strings (e.g. "MUL")
	unsigned int tokenArgCount;		
	FPClosureArgValue *args;			// flat array containing all arguments for all tokens (must iterate through tokenArgBytesList to find a particular argument set)
} FPClosure;


#define PRINT_EXEC 0
#define PRINT_PARSE 0


#ifndef MIN
#define MIN(a, b)  ((a < b) ? a : b)
#endif
#ifndef MAX
#define MAX(a, b)  ((a > b) ? a : b)
#endif


#define MAXINPUTSCALARS 24
#define MAXINPUTVECTORS 16




// I guess I don't trust compiler bitfields, so this is manual shifting for storing the swizzle info in upper bits of the token identifier
#define SWIZZLEMASK 	0xffffff00
#define ARGMASK			0x000000ff
#define SWIZZLESHIFT	8

#define SWZ  3
#define SWZR 9
#define SWZG 6
#define SWZB 3
#define SWZA 0
#define SWZBITS 7  // needs more than two bits, because we want to store RGBA + "off-bit" (i.e. don't modify)
#define SWZ_OFF 7  // any value above 3 indicates "off", but this one is official

#define SWIZZLE_RGBA	((0 << SWZR) + (1 << SWZG) + (2 << SWZB) + (3 << SWZA))
#define SWIZZLE_ABGR    ((3 << SWZR) + (2 << SWZG) + (1 << SWZB) + (0 << SWZA))



#define FIRST_OPENSCALAR_INDEX			50
#define FIRST_OPENVECTOR_INDEX			75


enum {
    kFPClosureError_RegIndexOutOfBounds = 12005,
    kFPClosureError_InvalidDstArgument,
    kFPClosureError_UnknownToken
};


typedef struct {
	int err;
	float regs[FPCLOSURE_NUM_REGS][4];
	int lastDstRegIndex;
} FPClosureExecState;



#define SPLAT_VEC(v, n)  				v[0] = v[1] = v[2] = v[3] = n;

#define SET_VEC(v, n1, n2, n3, n4)		v[0] = n1; v[1] = n2; v[2] = n3; v[3] = n4; 

#define COPY_VEC_FROM_REG(v, rIndex)	{ float *reg = state->regs[rIndex];  (v)[0] = reg[0]; (v)[1] = reg[1]; (v)[2] = reg[2]; (v)[3] = reg[3]; }

#define COPY_VEC(v1, v2)				v1[0] = v2[0];  v1[1] = v2[1];  v1[2] = v2[2];  v1[3] = v2[3]; 


#define ASSERT_REGINDEX(rIndex)  if (rIndex >= FPCLOSURE_NUM_REGS) { \
                                     printf("** assert failure: reg index out of bounds (%i)", rIndex); \
                                     state->err = kFPClosureError_RegIndexOutOfBounds;  return n; }


//#define FPC_ASSERT_TWO_ARGS    if (arN != 2) { printf("** invalid number of args (%) for token %i, expected 2\n", arN, token);  state->err = -112;  return; }
//#define FPC_ASSERT_THREE_ARGS  if (arN != 3) { printf("** invalid number of args (%) for token %i, expected 3\n", arN, token);  state->err = -112;  return; }


static int ConsumeFPTokenArgs(int argCount, FPClosureArgValue *args, float argVecs[][4], int *outRegIndex, int *outRegSwizzle, float *outDstVec,
							   FPClosureExecState *state)
{
	int i;
	int n = 0;  // counts number of args consumed

	for (i = 0; i < argCount; i++) {
		unsigned int swizzle = 	(args[n].arg & SWIZZLEMASK) >> SWIZZLESHIFT;
		FPClosureArg thisArg = 	(FPClosureArg) (args[n].arg & ARGMASK);

#if (PRINT_EXEC)
{
				int rSwiz = (swizzle >> SWZR) & SWZBITS;
				int gSwiz = (swizzle >> SWZG) & SWZBITS;
				int bSwiz = (swizzle >> SWZB) & SWZBITS;
				int aSwiz = (swizzle >> SWZA) & SWZBITS;
		printf(" ARG %i == type %i, swz {%i, %i, %i, %i}, n %i  --- ",  i,  thisArg, rSwiz, gSwiz, bSwiz, aSwiz,  n); fflush(stdout);
}		
#endif		

		n++;
		
		if (i == 0) {
			// this is the dst argument
			if (thisArg != FPClosureArg_Register) {
				printf("** invalid dst argument (%i, swiz %i; argCount %i)\n", thisArg, swizzle, argCount);
				state->err = kFPClosureError_InvalidDstArgument;
				return n;
			}
			
			*outRegIndex = args[n++].index;
			*outRegSwizzle = swizzle;
			
			#if (PRINT_EXEC)
			printf("dst reg index %i --", *outRegIndex); fflush(stdout);
			#endif
			
			ASSERT_REGINDEX(*outRegIndex);
			COPY_VEC_FROM_REG(outDstVec, *outRegIndex);  // must copy, because dst swizzling may mean that previous reg value is not overwritten
		}
		else {
			// this is a source argument
			float *argDst = argVecs[i - 1];
			
			switch (thisArg) {
				
				case FPClosureArg_ConstZero:
					SPLAT_VEC(argDst, 0.0f)
					break;
				
				case FPClosureArg_ConstFloat: {
					float val = args[n++].f;
					SPLAT_VEC(argDst, val);
					break;
				}
					
				case FPClosureArg_ConstFloatVec: {
#if (PRINT_EXEC)
					printf(".. floatvec: { %f, %f, %f, %f }, dst %i... ", args[0].f, args[1].f, args[2].f, args[3].f, i);
#endif
					SET_VEC(argDst, args[n++].f, args[n++].f, args[n++].f, args[n++].f);
					break;
				}
				
				case FPClosureArg_Register: {
					COPY_VEC_FROM_REG(argDst, args[n++].index);
                    
#if (PRINT_EXEC)
                    printf(".. register, index %i: { %f, %f, %f, %f } ... ", args[n-1].index, argDst[0], argDst[1], argDst[2], argDst[3]);
#endif
					break;
				}
					
				default:
					printf("** unknown token in FPClosure stream (%i)\n", thisArg);
					state->err = kFPClosureError_UnknownToken;
					break;
			}
			
			// apply swizzle
			if (thisArg == FPClosureArg_Register) {
				float newVec[4];
				
				int rSwiz = (swizzle >> SWZR) & SWZBITS;
				int gSwiz = (swizzle >> SWZG) & SWZBITS;
				int bSwiz = (swizzle >> SWZB) & SWZBITS;
				int aSwiz = (swizzle >> SWZA) & SWZBITS;
				
				newVec[0] = argDst[rSwiz];
				newVec[1] = argDst[gSwiz];
				newVec[2] = argDst[bSwiz];
				newVec[3] = argDst[aSwiz];
				
				COPY_VEC(argDst, newVec);
			}
		}		
	}
	return n;
}


#define CONSUME_ARGS_AND_COMP_SWIZZLE(argCount) \
			numConsumedArgs = ConsumeFPTokenArgs(argCount, args, argVecs, &dstRegIndex, &dstRegSwizzle, dstVec, state); \
			dstSwiz_r = (dstRegSwizzle >> SWZR) & SWZBITS; \
			dstSwiz_g = (dstRegSwizzle >> SWZG) & SWZBITS; \
			dstSwiz_b = (dstRegSwizzle >> SWZB) & SWZBITS; \
			dstSwiz_a = (dstRegSwizzle >> SWZA) & SWZBITS;
			//printf("swz %i/%i/%i/%i  ", dstSwiz_r, dstSwiz_g, dstSwiz_b, dstSwiz_a);
			
#define STORE_SCALAR(v) \
			dstVec[dstSwiz_r] = v; \
			dstVec[dstSwiz_g] = v; \
			dstVec[dstSwiz_b] = v; \
			dstVec[dstSwiz_a] = v;


#ifndef CLAMP_SAT_F
 #define CLAMP_SAT_F(v)  (v >= 1.0 ? 1.0 : (v < 0.0 ? 0.0 : v))	
#endif

			
						
// returns number of args consumed
static int ExecuteFPClosureToken(FPClosureToken token, FPClosureArgValue *args, FPClosureExecState *state)
{
    // separate the modifier
    int modifier = token & ~FPClosureTokenOpcodeMask;
    token = (FPClosureToken) ((int)token & FPClosureTokenOpcodeMask);

	float argVecs[8][4];
    memset(argVecs, 0, 8*4*sizeof(float));

	int dstRegIndex = -1;
	int dstRegSwizzle = 0;
	int numConsumedArgs = 0;
	
	float dstVec[8] = { 0.0f };  // this vec has the extra elements as a graveyard destination for out-swizzled results
	const float *srcVecA = argVecs[0];
	const float *srcVecB = argVecs[1];
	const float *srcVecC = argVecs[2];
	
	int dstSwiz_r = 0, dstSwiz_g = 0, dstSwiz_b = 0, dstSwiz_a = 0;
	
#if (PRINT_EXEC)	
	printf("exec token: %i (modif %i) ... ", token, modifier); fflush(stdout);
#endif
	
	register float v;

	// do computation
	switch (token)
	{
		case FPClosureToken_NOP:
		default:
			break;
			
		case FPClosureToken_ABS:
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);
			
			dstVec[dstSwiz_r] = FABSF(srcVecA[0]);
			dstVec[dstSwiz_g] = FABSF(srcVecA[1]);
			dstVec[dstSwiz_b] = FABSF(srcVecA[2]);
			dstVec[dstSwiz_a] = FABSF(srcVecA[3]);
			break;
			
		case FPClosureToken_ADD: 
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);

			dstVec[dstSwiz_r] = srcVecA[0] + srcVecB[0];
			dstVec[dstSwiz_g] = srcVecA[1] + srcVecB[1];
			dstVec[dstSwiz_b] = srcVecA[2] + srcVecB[2];
			dstVec[dstSwiz_a] = srcVecA[3] + srcVecB[3];
			break;
			
		case FPClosureToken_CMP: 
			CONSUME_ARGS_AND_COMP_SWIZZLE(4);
			
			// uses ARBfp order for operands -- D3D has B and C inverted
			dstVec[dstSwiz_r] = (srcVecA[0] < 0.0f) ?  srcVecB[0] : srcVecC[0];
			dstVec[dstSwiz_g] = (srcVecA[1] < 0.0f) ?  srcVecB[1] : srcVecC[1];
			dstVec[dstSwiz_b] = (srcVecA[2] < 0.0f) ?  srcVecB[2] : srcVecC[2];
			dstVec[dstSwiz_a] = (srcVecA[3] < 0.0f) ?  srcVecB[3] : srcVecC[3];
			break;
		
		case FPClosureToken_COS: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			v = srcVecA[0];  // takes only scalar input			
			v = cosf(v);
			STORE_SCALAR(v);
			break;
		}
			
		case FPClosureToken_DP3: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);

			v = (srcVecA[0] * srcVecB[0]) + (srcVecA[1] * srcVecB[1]) + (srcVecA[2] * srcVecA[2]);
			STORE_SCALAR(v);
			break;
		}
			
		case FPClosureToken_EX2: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);
			
			v = srcVecA[0];  // takes only scalar input
			v = powf(2.0f, v);
			STORE_SCALAR(v);
			break;
		}
		
		case FPClosureToken_FLR:
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			dstVec[dstSwiz_r] = floorf(srcVecA[0]);
			dstVec[dstSwiz_g] = floorf(srcVecA[1]);
			dstVec[dstSwiz_b] = floorf(srcVecA[2]);
			dstVec[dstSwiz_a] = floorf(srcVecA[3]);
			break;
			
		case FPClosureToken_FRC:
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			dstVec[dstSwiz_r] = srcVecA[0] - floorf(srcVecA[0]);
			dstVec[dstSwiz_g] = srcVecA[1] - floorf(srcVecA[1]);
			dstVec[dstSwiz_b] = srcVecA[2] - floorf(srcVecA[2]);
			dstVec[dstSwiz_a] = srcVecA[3] - floorf(srcVecA[3]);
			break;
			
		case FPClosureToken_LG2: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			v = srcVecA[0];  // takes only scalar input
			v = logf(v);
			v /= logf(2.0f);
			STORE_SCALAR(v);
			break;
		}
				
		case FPClosureToken_LRP:
			CONSUME_ARGS_AND_COMP_SWIZZLE(4);
			
			dstVec[dstSwiz_r] = srcVecA[0] * srcVecB[0]  +  (1.0f - srcVecA[0]) * srcVecC[0];
			dstVec[dstSwiz_g] = srcVecA[1] * srcVecB[1]  +  (1.0f - srcVecA[1]) * srcVecC[1];
			dstVec[dstSwiz_b] = srcVecA[2] * srcVecB[2]  +  (1.0f - srcVecA[2]) * srcVecC[2];
			dstVec[dstSwiz_a] = srcVecA[3] * srcVecB[3]  +  (1.0f - srcVecA[3]) * srcVecC[3];
			break;
			
		case FPClosureToken_MAD:
			CONSUME_ARGS_AND_COMP_SWIZZLE(4);
			
			dstVec[dstSwiz_r] = srcVecA[0] * srcVecB[0] + srcVecC[0];
			dstVec[dstSwiz_g] = srcVecA[1] * srcVecB[1] + srcVecC[1];
			dstVec[dstSwiz_b] = srcVecA[2] * srcVecB[2] + srcVecC[2];
			dstVec[dstSwiz_a] = srcVecA[3] * srcVecB[3] + srcVecC[3];
			break;
			
		case FPClosureToken_MAX:
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);
			
			dstVec[dstSwiz_r] = MAX(srcVecA[0], srcVecB[0]);
			dstVec[dstSwiz_g] = MAX(srcVecA[1], srcVecB[1]);
			dstVec[dstSwiz_b] = MAX(srcVecA[3], srcVecB[2]);
			dstVec[dstSwiz_a] = MAX(srcVecA[4], srcVecB[3]);
			break;
			
		case FPClosureToken_MIN:
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);
			
			dstVec[dstSwiz_r] = MIN(srcVecA[0], srcVecB[0]);
			dstVec[dstSwiz_g] = MIN(srcVecA[1], srcVecB[1]);
			dstVec[dstSwiz_b] = MIN(srcVecA[3], srcVecB[2]);
			dstVec[dstSwiz_a] = MIN(srcVecA[4], srcVecB[3]);
			break;

		case FPClosureToken_MOV:
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);
			
			dstVec[dstSwiz_r] = srcVecA[0];
			dstVec[dstSwiz_g] = srcVecA[1];
			dstVec[dstSwiz_b] = srcVecA[2];
			dstVec[dstSwiz_a] = srcVecA[3];
			break;
			
		case FPClosureToken_MUL: 
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);
			
			dstVec[dstSwiz_r] = srcVecA[0] * srcVecB[0];
			dstVec[dstSwiz_g] = srcVecA[1] * srcVecB[1];
			dstVec[dstSwiz_b] = srcVecA[2] * srcVecB[2];
			dstVec[dstSwiz_a] = srcVecA[3] * srcVecB[3];
			break;
		
		case FPClosureToken_POW: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);

			v = srcVecA[0];  // takes only scalar input
			v = powf(v, srcVecB[0]);
			STORE_SCALAR(v);
			break;
		}
							
		case FPClosureToken_RCP: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			v = srcVecA[0];  // takes only scalar input
			v = (v == 0.0f) ? HUGE_VAL : (1.0f / v);  // div by zero is a CPU exception on x86 -- must check for it!
			STORE_SCALAR(v);
			break;
		}
		
		case FPClosureToken_RSQ: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			v = srcVecA[0];  // takes only scalar input
			v = 1.0f / sqrtf(v);	
			STORE_SCALAR(v);
			break;
		}
		
		case FPClosureToken_SIN: {
			CONSUME_ARGS_AND_COMP_SWIZZLE(2);

			v = srcVecA[0];  // takes only scalar input
			v = sinf(v);
			STORE_SCALAR(v);
			break;
		}
		
		case FPClosureToken_SUB: 
			CONSUME_ARGS_AND_COMP_SWIZZLE(3);

			dstVec[dstSwiz_r] = srcVecA[0] - srcVecB[0];
			dstVec[dstSwiz_g] = srcVecA[1] - srcVecB[1];
			dstVec[dstSwiz_b] = srcVecA[2] - srcVecB[2];
			dstVec[dstSwiz_a] = srcVecA[3] - srcVecB[3];
			break;		
	}

#if (PRINT_EXEC)	
	printf(".. done: %f, %f, %f, %f (nargs %i)\n", dstVec[0], dstVec[1], dstVec[2], dstVec[3], numConsumedArgs); fflush(stdout);
#endif	
	
    if (modifier & FPTokenModifier_SAT) {
        dstVec[0] = CLAMP_SAT_F(dstVec[0]);
        dstVec[1] = CLAMP_SAT_F(dstVec[1]);
        dstVec[2] = CLAMP_SAT_F(dstVec[2]);
        dstVec[3] = CLAMP_SAT_F(dstVec[3]);
    }
    
	// store result
	COPY_VEC( state->regs[dstRegIndex], dstVec );
	
	state->lastDstRegIndex = dstRegIndex;
	
	return numConsumedArgs;
}



///int ExecuteFPClosure(FPClosure *fp, float *outVector, const float *inputScalars, int inputScalarCount, const float *inputVectors, int inputVectorCount)
int LXFPClosureExecute_f(LXFPClosurePtr fp, float * LXRESTRICT outVector,
                                            const float * LXRESTRICT inputScalars, LXInteger inputScalarCount,
                                            const float * LXRESTRICT inputVectors, LXInteger inputVectorCount)
{
	LXInteger i;
	const int c = fp->tokenCount;
	FPClosureArgValue *argsArray = fp->args;
	
#if (PRINT_EXEC)
	printf("starting fpc exec: %i tokens, %i args\n", fp->tokenCount, fp->tokenArgCount);  fflush(stdout);
#endif
	
	FPClosureExecState state;
	memset(&state, 0, sizeof(FPClosureExecState));
	
	// copy input values into state registers
	inputScalarCount = MIN(inputScalarCount, MAXINPUTSCALARS);
	for (i = 0; i < inputScalarCount; i++) {
		float vec[4];
		SPLAT_VEC(vec, inputScalars[i]);
		COPY_VEC(state.regs[FIRST_OPENSCALAR_INDEX + i], vec);
	}
	
	inputVectorCount = MIN(inputVectorCount, MAXINPUTVECTORS);
	for (i = 0; i < inputVectorCount; i++) {
		const float *vec = inputVectors + i*4;
		COPY_VEC(state.regs[FIRST_OPENVECTOR_INDEX + i], vec);
	}
		
	// execute tokens
	for (i = 0; i < c; i++) {
		int numConsumedArgs = ExecuteFPClosureToken(fp->tokenList[i], argsArray, &state);
		argsArray += numConsumedArgs;
	}
	
	// copy result to given output
	COPY_VEC( outVector, state.regs[state.lastDstRegIndex] );
	
	return state.err;
}


LXSuccess LXFPClosureExecuteWithContext(LXFPClosurePtr fp, LXRGBA *outRGBA, LXFPClosureContextPtr ctx, LXError *outError)
{
    LXInteger numScalars = LXFPClosureContextGetScalarCount(ctx);
    LXInteger numVectors = LXFPClosureContextGetVectorCount(ctx);
    
    float outVec[4] = { 0, 0, 0, 0 };
    int result = 0;
    
    if (numScalars == 0 && numVectors == 0) {
        result = LXFPClosureExecute_f(fp, outVec, NULL, 0, NULL, 0);
    }
    else {
        float scalars[1 + numScalars];
        float vectors[1 + numVectors * 4];
        LXInteger i;
        
        for (i = 0; i < numScalars; i++) {
            scalars[i] = LXFPClosureContextGetScalarAtIndex(ctx, i);
        }
        
        for (i = 0; i < numVectors; i++) {
            LXRGBA vec = LXFPClosureContextGetVectorAtIndex(ctx, i);
            
            vectors[i*4 + 0] = vec.r;
            vectors[i*4 + 1] = vec.g;
            vectors[i*4 + 2] = vec.b;
            vectors[i*4 + 3] = vec.a;
        }
        
        result = LXFPClosureExecute_f(fp, outVec, scalars, numScalars, vectors, numVectors);
    }
    
    if (outRGBA) *outRGBA = LXMakeRGBA(outVec[0], outVec[1], outVec[2], outVec[3]);
    
    if (outError) {
        if (result != 0) {
            LXErrorSet(outError, result, "error during execution");
        } else {
            memset(outError, 0, sizeof(LXError));
        }
    }
    
    return (result == 0) ? YES : NO;
}


///void DisposeFPClosure(FPClosure *fp)
void LXFPClosureDestroy(LXFPClosurePtr fp)
{
	if (!fp) return;
	
	if (fp->tokenList) _lx_free(fp->tokenList);
	if (fp->args) _lx_free(fp->args);
	_lx_free(fp);	
}




void TestFPClosure()
{
	FPClosure c;
	memset(&c, 0, sizeof(FPClosure));
	
	///c->inputsInUse[0] = YES;
		
	FPClosureToken tokens[] = { FPClosureToken_ADD, FPClosureToken_RCP };
	
	c.tokenList = tokens;
	c.tokenCount = 2;
	
	
	FPClosureArgValue args[100];
	int n = 0;
	
	// ADD dst
	args[n++].arg = (SWIZZLE_ABGR << SWIZZLESHIFT) | (FPClosureArg_Register);
	args[n++].index = 1;
	
	// ADD src 1
	args[n++].arg = FPClosureArg_ConstFloat;
	args[n++].f = 0.5f;
	
	// ADD src 2
	args[n++].arg = FPClosureArg_ConstFloatVec;
	args[n++].f = 1.0f;
	args[n++].f = 0.9f;	
	args[n++].f = 0.2f;	
	args[n++].f = 0.2f;	
								
	// RCP dst
	args[n++].arg = ( ((0 << SWZR) + (5 << SWZG) + (5 << SWZB) + (5 << SWZA)) << SWIZZLESHIFT) | (FPClosureArg_Register);
	args[n++].index = 1;
	
	// RCP src
	args[n++].arg = FPClosureArg_Register;
	args[n++].index = 1;
	
	
	c.tokenArgCount = 5;
	c.args = args;
	
		
	float result[4] = { -1, -1, -1, -1 };
	
    LXFPClosureExecute_f(&c, result, NULL, 0, NULL, 0);
	
	printf("\n\n---- got result: %f, %f, %f, %f\n", result[0], result[1], result[2], result[3]);
}




int GetFPClosureTokenIDForARBOpcode(char *opcode)
{
	if (opcode == NULL)
		return 0;

    char tempOp[4];
    int modifier = 0;
    int len = strlen(opcode);
    
    if (len > 4) {
        char *modifstr = opcode + len - 4;
        if (0 == strcmp(modifstr, "_SAT")) {
            modifier = FPTokenModifier_SAT;
            
            memcpy(tempOp, opcode, 3);
            tempOp[3] = '\0';
            opcode = tempOp;  // remove the modifier from the opcode
        }
    }
	
	if (0 == strcmp(opcode, "ABS"))
		return FPClosureToken_ABS | modifier;
	if (0 == strcmp(opcode, "ADD"))
		return FPClosureToken_ADD | modifier;
	if (0 == strcmp(opcode, "CMP"))
		return FPClosureToken_CMP | modifier;
	if (0 == strcmp(opcode, "DP3"))
		return FPClosureToken_DP3 | modifier;
	if (0 == strcmp(opcode, "EX2"))
		return FPClosureToken_EX2 | modifier;
	if (0 == strcmp(opcode, "FLR"))
		return FPClosureToken_FLR | modifier;		
	if (0 == strcmp(opcode, "FRC"))
		return FPClosureToken_FRC | modifier;
	if (0 == strcmp(opcode, "LG2"))
		return FPClosureToken_LG2 | modifier;
	if (0 == strcmp(opcode, "LRP"))
		return FPClosureToken_LRP | modifier;
	if (0 == strcmp(opcode, "MAD"))
		return FPClosureToken_MAD | modifier;
	if (0 == strcmp(opcode, "MAX"))
		return FPClosureToken_MAX | modifier;
	if (0 == strcmp(opcode, "MIN"))
		return FPClosureToken_MIN | modifier;
	if (0 == strcmp(opcode, "MOV"))
		return FPClosureToken_MOV | modifier;
	if (0 == strcmp(opcode, "MUL"))
		return FPClosureToken_MUL | modifier;
	if (0 == strcmp(opcode, "POW"))
		return FPClosureToken_POW | modifier;
	if (0 == strcmp(opcode, "RCP"))
		return FPClosureToken_RCP | modifier;
	if (0 == strcmp(opcode, "RSQ"))
		return FPClosureToken_RSQ | modifier;
	if (0 == strcmp(opcode, "SIN"))
		return FPClosureToken_SIN | modifier;
	if (0 == strcmp(opcode, "COS"))
		return FPClosureToken_COS | modifier;
	if (0 == strcmp(opcode, "SUB"))
		return FPClosureToken_SUB | modifier;

	printf("** warning: unknown opcode token in FPClosure program (%s)\n", opcode);
	
	return 	FPClosureToken_NOP;
}


LXINLINE int channelIndexFromChar(char c)
{
	switch (c) {
		case 'R': case 'r': case 'X': case 'x':
			return 0;
		
		case 'G': case 'g': case 'Y': case 'y':
			return 1;
			
		case 'B': case 'b': case 'Z': case 'z':
			return 2;
		
		case 'A': case 'a': case 'W': case 'w':
			return 3;
			
		default:
			return SWZ_OFF;
	}
}


#define SWIZZLE_SCALAR_LOAD(i)   ((i << SWZR) | (i << SWZG) | (i << SWZB) | (i << SWZA))
#define SWIZZLE_SCALAR_STORE(i)  (((0 == i ? 0 : SWZ_OFF) << SWZR) | ((1 == i ? 1 : SWZ_OFF) << SWZG) | ((2 == i ? 2 : SWZ_OFF) << SWZB) | ((3 == i ? 3 : SWZ_OFF) << SWZA))


static unsigned int GetSwizzleFromArgString(char *argStr, int isDst)
{
	char *dotPtr = strchr(argStr, '.');
	if (dotPtr == NULL)
		return SWIZZLE_RGBA;
		
	dotPtr++;  // move past dot

	int chCount = 0;
	while (channelIndexFromChar(dotPtr[chCount]) != SWZ_OFF)
		chCount++;
	
	if (chCount == 0)
		return SWIZZLE_RGBA;
		
	if (chCount == 1) {
		// with a single channel swizzle given, the behaviour depends on whether this is a source or dst swizzle
		int chIndex = channelIndexFromChar(*dotPtr);

			#if (PRINT_PARSE)
				printf("    got %s swizzle, single channel: %i\n", (isDst ? "dst" : "src"), chIndex);
			#endif

		if (isDst)
			return SWIZZLE_SCALAR_STORE(chIndex);
		else
			return SWIZZLE_SCALAR_LOAD(chIndex);		
	}
		
	if ( !isDst || chCount == 4)
		return SWIZZLE_RGBA;
		
	// only dst swizzles support arbitrary masks (e.g. "rgb" or "ga")
	int i;
	unsigned int sws[4] = { SWZ_OFF, SWZ_OFF, SWZ_OFF, SWZ_OFF };  // the default value for a swizzle that doesn't write to the target channel
	
	for (i = 0; i < chCount; i++) {
		int chIndex = channelIndexFromChar(dotPtr[i]);
		if (chIndex >= 0 && chIndex < 4)
			sws[chIndex] = chIndex;
	}
	
    #if (PRINT_PARSE)
	  printf("     got multich swizzle, %i %i %i %i (%s)\n", sws[0], sws[1], sws[2], sws[3], (isDst ? "dst" : "src"));
    #endif
	
	return ((sws[0] << SWZR) | (sws[1] << SWZG) | (sws[2] << SWZB) | (sws[3] << SWZA));
}


int ParseFPClosureArgString(char *argStr, FPClosureArgValue *outArgArr, int outArgArraySize, int isDst)
{
	/*possible arg types are:			// arg count:
	FPClosureArg_ConstZero = 1,	  		// 0
	FPClosureArg_ConstFloat,  			// 1 (the const as a float)
	FPClosureArg_ConstFloatVec,			// 4 (the vector const)
	FPClosureArg_Register,				// 1 (index of the register)
	*/
	
	if (*argStr == '{')  // this is a 4f vector constant
	{
		outArgArr[0].arg = FPClosureArg_ConstFloatVec | (SWIZZLE_RGBA << SWIZZLESHIFT);

		int i;
		for (i = 0; i < 4; i++) {
#if (PRINT_PARSE)
			printf(":: %i : %s\n", i, argStr);
#endif
			while (isspace(*argStr) || *argStr == '{' || *argStr == ',')
				argStr++;

			outArgArr[i+1].f = (*argStr != '\0') ? atof(argStr) : 0.0f;
			
			while (isdigit(*argStr) || *argStr == '-' || *argStr == '.' || *argStr == 'e' || *argStr == 'E'  // legal characters in float constants
								    //|| (*argStr != '\0' && !isspace(*argStr) && *argStr != ','))
								    )
				argStr++;
		}
		
#if (PRINT_PARSE)
			printf(".. parsed constvec arg: { %f, %f, %f, %f}\n", outArgArr[1].f, outArgArr[2].f, outArgArr[3].f, outArgArr[4].f);
#endif		
		return 5;
	}
	
	else if (isalpha(*argStr))  // this is a register/variable reference
	{
		outArgArr[0].arg = FPClosureArg_Register | (GetSwizzleFromArgString(argStr, isDst) << SWIZZLESHIFT);
		
		char varID = *argStr;
		int regIndex = 0;
		
		if (varID == FPCLOSURE_VARID_REGISTER)
			regIndex = atoi(++argStr);
		else if (varID == FPCLOSURE_VARID_OPENSCALAR)
			regIndex = atoi(++argStr) + FIRST_OPENSCALAR_INDEX - 1;  // the var indices in the program string are 1-based
		else if (varID == FPCLOSURE_VARID_OPENVECTOR)
			regIndex = atoi(++argStr) + FIRST_OPENVECTOR_INDEX - 1;
		
		outArgArr[1].index = regIndex;

#if (PRINT_PARSE)
		printf(".. parsed register arg: %c, %i\n", varID, outArgArr[1].index);
#endif
		return 2;
	}
	
	else if (*argStr == '-' || *argStr == '.' || isdigit(*argStr))  // this is a float constant
	{
		outArgArr[0].arg = FPClosureArg_ConstFloat | (0 << SWIZZLESHIFT);  // zero swizzle == '.rrrr'
		outArgArr[1].f = atof(argStr);

#if (PRINT_PARSE)
		printf(".. parsed const arg: %f\n", outArgArr[1].f);
#endif
		return 2;
	}
	
	return 0;
}


#define MAXLINELEN 256
#define MAXARGLEN  64


char *FindARBfpArgument(char *linePtr, int *outArgLen)
{	
	if (linePtr == NULL || *linePtr == '\0')
		return NULL;

	// skip whitespace and commas
	while (isspace(*linePtr) || *linePtr == ',') {
		linePtr++;
		if (*linePtr == '\0')
			return NULL;
	}
	int argLen = 0;
	
	if (*linePtr == '{') {
		char *bracketPtr = linePtr+1;
		// this is a bracketed vector constant, so find closing bracket	
		while (*bracketPtr != '}' && *bracketPtr != '\0')
			bracketPtr++;
		
		if (*bracketPtr == '}')  // include closing bracket in argument
			bracketPtr++;
			
		argLen = bracketPtr - linePtr;
	}
	else {
		// find argument end by looking for whitespace or comma
		char *endPtr = linePtr+1;
		while (*endPtr != '\0' && !isspace(*endPtr) && *endPtr != ',')
			endPtr++;
		
		argLen = endPtr - linePtr;
	}
	
	argLen = MIN(argLen, MAXARGLEN - 1);
	*outArgLen = argLen;
	
	return (argLen > 0) ? linePtr : NULL;
}



LXFPClosurePtr LXFPClosureCreateWithString(const char *fpStr, size_t fpStrLen)
{
	const char *lcqHeaderStr = "!!LCQfp1.0";
    const char *arbHeaderStr = "!!ARBfp1.0";
	//const char *arbEndStr = "END";
	
	if (fpStrLen < 13) {
		printf("** can't parse FPClosure, too short input\n");
		return NULL;
	}
	
	// skip past !!ARBfp1.0 header
	if (0 != strncmp(fpStr, arbHeaderStr, 10) &&
        0 != strncmp(fpStr, lcqHeaderStr, 10) ) {
		printf("** can't parse FPClosure, missing or invalid header: %s\n", fpStr);
		return NULL;
	}
	
	char *prog = (char *)fpStr + strlen(arbHeaderStr);
	
	// find end position
	char *fpEnd = strrchr(prog, 'E');
	int progLen = fpEnd - prog;			// check for fpEnd == NULL is below, so this is ok
	
	if (fpEnd == NULL || fpEnd < prog || progLen < 1) {
		printf("** can't parse FPClosure, END missing: %s\n", fpStr);
		return NULL;
	}
	
	unsigned int tokenCount = 0;
	FPClosureToken *tokenList = NULL;
	
	unsigned int tokenArgCount = 0;	
	FPClosureArgValue *tokenArgList = NULL;
	
	
	// read tokens separated by colon
	char lineStr[MAXLINELEN];
	char args[8][MAXARGLEN];
	char *lineEndPtr;
	
	while (prog < fpEnd && (lineEndPtr = strchr(prog, ';')))
	{
		int lineLen = MIN(lineEndPtr - prog, MAXLINELEN - 1);
		
		//printf("linelen %i (progLen %i)\n", lineLen, fpEnd - prog);
		
		if (lineLen > 1) {
			memcpy(lineStr, prog, lineLen);
			lineStr[lineLen] = '\0';
			
			//printf(" got line:  %s\n", lineStr); fflush(stdout);
			
			// clear arguments array
			int i;
			for (i = 0; i < 8; i++)
				memset(args[i], 0, MAXARGLEN);

			// read arguments within line
			char *argPtr = lineStr;
			///char *argEndPtr;
			i = 0;
			while (argPtr && argPtr < (lineStr + lineLen) && i < 8)
			{				
				int argLen = 0;
				argPtr = FindARBfpArgument(argPtr, &argLen);
				
				if (argPtr) {
					memcpy(args[i], argPtr, argLen);
					args[i][argLen] = '\0';
					
					//printf("   got arg: %s (%i, len %i)\n", args[i], i, argLen);
					
					argPtr += argLen;
					i++;
				}
			}
			
			/*printf("  .. args: ");
			for (i = 0; i < 8; i++) {
				if (strlen(args[i]) > 0)
					printf("%s, ", args[i]);
				else
					printf("(null arg), ");
			}
			printf("\n");
			*/
			
			for (i = 0; i < 8; i++) {
				if (args[i][0] == '\0')
					break;
				
				if (i == 0) { // this is the opcode
					tokenList = (FPClosureToken *) realloc(tokenList, ++tokenCount * sizeof(FPClosureToken));
					
					tokenList[tokenCount-1] = (FPClosureToken) GetFPClosureTokenIDForARBOpcode(args[i]);
				}
				else {  // this is an argument, so determine its size
					FPClosureArgValue argScratch[8];
					memset(argScratch, 0, 8*sizeof(FPClosureArgValue));
					
					int isDst = (i == 1);
					int argSize = ParseFPClosureArgString(args[i], argScratch, 8, isDst);
				
					if (argSize > 0) {
						tokenArgList = (FPClosureArgValue *) realloc(tokenArgList, (tokenArgCount + argSize) * sizeof(FPClosureArgValue));
						memcpy(tokenArgList + tokenArgCount, argScratch, argSize * sizeof(FPClosureArgValue));
						
						tokenArgCount += argSize;
					}
				}
				
			}
		}
		
		prog += (lineEndPtr - prog) + 1;  // move past line terminator
	}
	
#if (PRINT_PARSE)
	printf("finished parsing, got %i tokens and %i args\n\n", tokenCount, tokenArgCount);
#endif
	
	FPClosure *cls = (FPClosure *) _lx_calloc(1, sizeof(FPClosure));
	cls->tokenCount = tokenCount;
	cls->tokenList = tokenList;
	cls->tokenArgCount = tokenArgCount;
	cls->args = tokenArgList;
	
	return cls;
}


