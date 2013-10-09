/*
 *  LXColorFunctions.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 31.7.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXColorFunctions.h"


// references an element of 4x4 float array matrix;
// m == matrix array, c == column, r == row
#define MAT(_m,_r,_c) (_m)[(_c)*4 + (_r)]


// basic matrix operations on float array matrices
static LXBool invert_matrix_3d_general( float *mIn, float *mOut );
static void matmul4( float *product, const float *a, const float *b );
static void transform_vector( float vecOut[4], const float v[4], const float *m );
static void transpose_matrix_4x4( float to[16], const float from[16] );


static void print_matrix_4x4(float *m)
{
	int i, j;
	for (i = 0; i < 4; i++) {
		printf("   ");
		for (j = 0; j < 4; j++) {
			printf("%8f ", (double) (MAT(m, i, j)) );
		}
		printf("\n");
	}
}

static void LX4x4MatrixFromGLStyleFloatMatrix(LX4x4Matrix *m, const float *inm)
{
        m->m11 = MAT(inm, 1, 1);
        m->m12 = MAT(inm, 2, 1);
        m->m13 = MAT(inm, 3, 1);
        m->m14 = MAT(inm, 4, 1);
        
        m->m21 = MAT(inm, 1, 2);
        m->m22 = MAT(inm, 2, 2);
        m->m23 = MAT(inm, 3, 2);
        m->m24 = MAT(inm, 4, 2);
        
        m->m31 = MAT(inm, 1, 3);
        m->m32 = MAT(inm, 2, 3);
        m->m33 = MAT(inm, 3, 3);
        m->m34 = MAT(inm, 4, 3);
        
        m->m41 = MAT(inm, 1, 4);
        m->m42 = MAT(inm, 2, 4);
        m->m43 = MAT(inm, 3, 4);
        m->m44 = MAT(inm, 4, 4);
}

static void LXGLStyleFloatMatrixFrom4x4Matrix(float *m, const LX4x4Matrix *inm)
{
        MAT(m, 1, 1) = inm->m11;
        MAT(m, 2, 1) = inm->m12;
        MAT(m, 3, 1) = inm->m13;
        MAT(m, 4, 1) = inm->m14;

        MAT(m, 1, 2) = inm->m21;
        MAT(m, 2, 2) = inm->m22;
        MAT(m, 3, 2) = inm->m23;
        MAT(m, 4, 2) = inm->m24;
        
        MAT(m, 1, 3) = inm->m31;
        MAT(m, 2, 3) = inm->m32;
        MAT(m, 3, 3) = inm->m33;
        MAT(m, 4, 3) = inm->m34;

        MAT(m, 1, 4) = inm->m41;
        MAT(m, 2, 4) = inm->m42;
        MAT(m, 3, 4) = inm->m43;
        MAT(m, 4, 4) = inm->m44;
}


void LXColorConvertChromaticityWhitePoint_xy_to_RGB( float *rgbOut,
											  float xr, float xg, float xb, float xw,
											  float yr, float yg, float yb, float yw )
{
	float mF[16];
	float mF_inv[16];
	float vS[4];
	memset(mF, 0, 16*sizeof(float));
	
	MAT(mF,0,0) = xr / yr;
	MAT(mF,0,1) = xg / yg;
	MAT(mF,0,2) = xb / yb;
	
	MAT(mF,1,0) = MAT(mF,1,1) = MAT(mF,1,2) = 1.0f;
	
	MAT(mF,2,0) = (1.0 - xr - yr) / yr;
	MAT(mF,2,1) = (1.0 - xg - yg) / yg;
	MAT(mF,2,2) = (1.0 - xb - yb) / yb;
	
	invert_matrix_3d_general(mF, mF_inv);
	transpose_matrix_4x4(mF_inv, mF_inv);
	
	vS[0] = xw / yw;
	vS[1] = 1.0;
	vS[2] = (1.0 - xw - yw) / yw;
	vS[3] = 0.0;
	transform_vector(vS, vS, mF_inv);
	///printf(" convert whitepoint: scale %f %f %f \n", vS[0], vS[1], vS[2]);
	
	rgbOut[0] = vS[0];
	rgbOut[1] = vS[1];
	rgbOut[2] = vS[2];
}


void LXColorRGBtoXYZTransformMatrixFromChromaticities(LX4x4Matrix *mOut,
													  float xr, float xg, float xb, //float xw,
													  float yr, float yg, float yb, //float yw )
													  float RY, float GY, float BY )
{
	float mF[16];
	///float mF_inv[16];
	float vS[4];
	memset(mF, 0, 16*sizeof(float));
	
	MAT(mF,0,0) = xr / yr;
	MAT(mF,0,1) = xg / yg;
	MAT(mF,0,2) = xb / yb;
	
	MAT(mF,1,0) = MAT(mF,1,1) = MAT(mF,1,2) = 1.0f;
	
	MAT(mF,2,0) = (1.0 - xr - yr) / yr;
	MAT(mF,2,1) = (1.0 - xg - yg) / yg;
	MAT(mF,2,2) = (1.0 - xb - yb) / yb;

	vS[0] = RY;
	vS[1] = GY;
	vS[2] = BY;
	
	// scale by white point
	MAT(mF,0,0) *= vS[0];
	MAT(mF,1,0) *= vS[0];
	MAT(mF,2,0) *= vS[0];
	
	MAT(mF,0,1) *= vS[1];
	MAT(mF,1,1) *= vS[1];
	MAT(mF,2,1) *= vS[1];
	
	MAT(mF,0,2) *= vS[2];
	MAT(mF,1,2) *= vS[2];
	MAT(mF,2,2) *= vS[2];

	///print_matrix_4x4(mF);
    
    LX4x4MatrixFromGLStyleFloatMatrix(mOut, mF);
}

void LXColorXYZtoRGBTransformMatrixFromChromaticities(LX4x4Matrix *mOut,
													  float xr, float xg, float xb, //float xw,
													  float yr, float yg, float yb, //float yw )
													  float RY, float GY, float BY )
{
    LX4x4Matrix mInv;
    float m1[16];
    float mF[16];
    memset(mF, 0, 16*sizeof(float));
    
	LXColorRGBtoXYZTransformMatrixFromChromaticities(&mInv, xr, xg, xb, yr, yg, yb, RY, GY, BY);

    LXGLStyleFloatMatrixFrom4x4Matrix(m1, &mInv);

	invert_matrix_3d_general(m1, mF);
	
	//print_matrix_4x4(mF);
    
    LX4x4MatrixFromGLStyleFloatMatrix(mOut, mF);
}


void LXColorXYZfromYxy(float *outXYZ, float Y, float x, float y)
{
	outXYZ[0] = (x / y) * Y;
	outXYZ[1] = Y;
	outXYZ[2] = ((1.0 - x - y) / y) * Y;
}


void LXColorGetChromaticAdaptationMatrix(LX4x4Matrix *mOut, LXColorXYZWhitePoint srcWhite, LXColorXYZWhitePoint dstWhite)
{
	// this conversion math is from www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html

/*
	if (srcWhite == kLXWhitePoint_D65 && dstWhite == kLXWhitePoint_D50) {
		float mat_d65_to_d50[16] = { 1.047835,   0.029556,  -0.009238,  0,
									 0.022897,   0.990481,   0.015050,  0,
									 -0.050147, -0.017056,   0.752034,  0,
									 0, 0, 0, 0 };
		//transpose_matrix_4x4(mOut, mat_d65_to_d50);
		memcpy(mOut, mat_d65_to_d50, 16*sizeof(float));
		return;
	}
	else if (srcWhite == kLXWhitePoint_D50 && dstWhite == kLXWhitePoint_D65) {
		printf("sdsadadsa.qwq\n");
		float mat_d50_to_d65[16] = {  0.955556,  -0.028302,   0.012305,  0,
									 -0.023049,   1.009944,  -0.020494,  0,
 									  0.063197,   0.021018,   1.330084,  0,
									 0, 0, 0, 0 };
		//transpose_matrix_4x4(mOut, mat_d50_to_d65);
		memcpy(mOut, mat_d50_to_d65, 16*sizeof(float));
		return;
	}
*/
	// let's do the matrix math for arbitrary chromatic adaptation
	float mat_MA_bradford[16] = {
		0.8951,   -0.7502,    0.0389,  0,
		0.2664,    1.7135,   -0.0685,  0,
	   -0.1614,    0.0367,    1.0296,  0,
		0, 0, 0, 0 };
	float mat_MAinv_bradford[16] = {
		0.986993,  0.432305, -0.008529,  0,
	   -0.147054,  0.518360,  0.040043,  0,
		0.159963,  0.049291,  0.968487,  0,
		0, 0, 0, 0 };
		
	float src_x, src_y, dst_x, dst_y;
	switch (srcWhite) {
		case kLXWhitePoint_D65:
		default:
			src_x = kLXWhitePoint_D65_x;  src_y = kLXWhitePoint_D65_y;  break;
		case kLXWhitePoint_D40:
			src_x = kLXWhitePoint_D40_x;  src_y = kLXWhitePoint_D40_y;  break;
		case kLXWhitePoint_D45:
			src_x = kLXWhitePoint_D45_x;  src_y = kLXWhitePoint_D45_y;  break;
		case kLXWhitePoint_D50:
			src_x = kLXWhitePoint_D50_x;  src_y = kLXWhitePoint_D50_y;  break;
		case kLXWhitePoint_D55:
			src_x = kLXWhitePoint_D55_x;  src_y = kLXWhitePoint_D55_y;  break;
		case kLXWhitePoint_D60:
			src_x = kLXWhitePoint_D60_x;  src_y = kLXWhitePoint_D60_y;  break;
		case kLXWhitePoint_D70:
			src_x = kLXWhitePoint_D70_x;  src_y = kLXWhitePoint_D70_y;  break;
	}

	switch (dstWhite) {
		case kLXWhitePoint_D65:
		default:
			dst_x = kLXWhitePoint_D65_x;  dst_y = kLXWhitePoint_D65_y;  break;
		case kLXWhitePoint_D40:
			dst_x = kLXWhitePoint_D40_x;  dst_y = kLXWhitePoint_D40_y;  break;
		case kLXWhitePoint_D45:
			dst_x = kLXWhitePoint_D45_x;  dst_y = kLXWhitePoint_D45_y;  break;
		case kLXWhitePoint_D50:
			dst_x = kLXWhitePoint_D50_x;  dst_y = kLXWhitePoint_D50_y;  break;
		case kLXWhitePoint_D55:
			dst_x = kLXWhitePoint_D55_x;  dst_y = kLXWhitePoint_D55_y;  break;
		case kLXWhitePoint_D60:
			dst_x = kLXWhitePoint_D60_x;  dst_y = kLXWhitePoint_D60_y;  break;
		case kLXWhitePoint_D70:
			dst_x = kLXWhitePoint_D70_x;  dst_y = kLXWhitePoint_D70_y;  break;
	}
	
	// convert white point xy coordinates to XYZ (Y == 1.0)
	float srcXYZ[4];
	float dstXYZ[4];
	float srcConeVec[4];
	float dstConeVec[4];
	LXColorXYZfromYxy(srcXYZ, 1.0, src_x, src_y);
	LXColorXYZfromYxy(dstXYZ, 1.0, dst_x, dst_y);
	srcXYZ[3] = dstXYZ[3] = 0.0f;
	
	///printf("src white %f %f %f --- ", srcXYZ[0], srcXYZ[1], srcXYZ[2]);
	///printf("dst white %f %f %f\n", dstXYZ[0], dstXYZ[1], dstXYZ[2]);
	
	transpose_matrix_4x4(mat_MA_bradford, mat_MA_bradford);
	transpose_matrix_4x4(mat_MAinv_bradford, mat_MAinv_bradford);
	
	transform_vector(srcConeVec, srcXYZ, mat_MA_bradford);
	transform_vector(dstConeVec, dstXYZ, mat_MA_bradford);
	
	///printf("src cone  %f %f %f --- ", srcConeVec[0], srcConeVec[1], srcConeVec[2]);
	///printf("dst cone  %f %f %f\n", dstConeVec[0], dstConeVec[1], dstConeVec[2]);
	
	float mat_M[16];
	float mat_cone[16];
	memset(mat_cone, 0, 16*sizeof(float));
	mat_cone[0] = dstConeVec[0] / srcConeVec[0];
	mat_cone[5] = dstConeVec[1] / srcConeVec[1];
	mat_cone[10] = dstConeVec[2] / srcConeVec[2];
	
	matmul4(mat_M, mat_cone, mat_MAinv_bradford);
	matmul4(mat_M, mat_MA_bradford, mat_M);
	
	///print_matrix_4x4(mat_M);
    
    LX4x4MatrixFromGLStyleFloatMatrix(mOut, mat_M);
}




#define SWAP_ROWS(_a, _b) { float *_tmp = _a; (_a) = (_b); (_b) = _tmp; }


static LXBool invert_matrix_3d_general(float *mOrig, float *mOut)
{
   float pos, neg, t;
   float det;
   float mIn[16];
   memcpy(mIn, mOrig, 16*sizeof(float));

    // calculate the determinant of upper left 3x3 submatrix and
    // determine if the matrix is singular
    
   pos = neg = 0.0;
   t =  MAT(mIn,0,0) * MAT(mIn,1,1) * MAT(mIn,2,2);
   if (t >= 0.0) pos += t; else neg += t;

   t =  MAT(mIn,1,0) * MAT(mIn,2,1) * MAT(mIn,0,2);
   if (t >= 0.0) pos += t; else neg += t;

   t =  MAT(mIn,2,0) * MAT(mIn,0,1) * MAT(mIn,1,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,2,0) * MAT(mIn,1,1) * MAT(mIn,0,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,1,0) * MAT(mIn,0,1) * MAT(mIn,2,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,0,0) * MAT(mIn,2,1) * MAT(mIn,1,2);
   if (t >= 0.0) pos += t; else neg += t;

   det = pos + neg;

   if (det*det < 1e-25)
      return NO;

   det = 1.0F / det;
   MAT(mOut,0,0) = (  (MAT(mIn,1,1)*MAT(mIn,2,2) - MAT(mIn,2,1)*MAT(mIn,1,2) )*det);
   MAT(mOut,0,1) = (- (MAT(mIn,0,1)*MAT(mIn,2,2) - MAT(mIn,2,1)*MAT(mIn,0,2) )*det);
   MAT(mOut,0,2) = (  (MAT(mIn,0,1)*MAT(mIn,1,2) - MAT(mIn,1,1)*MAT(mIn,0,2) )*det);
   MAT(mOut,1,0) = (- (MAT(mIn,1,0)*MAT(mIn,2,2) - MAT(mIn,2,0)*MAT(mIn,1,2) )*det);
   MAT(mOut,1,1) = (  (MAT(mIn,0,0)*MAT(mIn,2,2) - MAT(mIn,2,0)*MAT(mIn,0,2) )*det);
   MAT(mOut,1,2) = (- (MAT(mIn,0,0)*MAT(mIn,1,2) - MAT(mIn,1,0)*MAT(mIn,0,2) )*det);
   MAT(mOut,2,0) = (  (MAT(mIn,1,0)*MAT(mIn,2,1) - MAT(mIn,2,0)*MAT(mIn,1,1) )*det);
   MAT(mOut,2,1) = (- (MAT(mIn,0,0)*MAT(mIn,2,1) - MAT(mIn,2,0)*MAT(mIn,0,1) )*det);
   MAT(mOut,2,2) = (  (MAT(mIn,0,0)*MAT(mIn,1,1) - MAT(mIn,1,0)*MAT(mIn,0,1) )*det);

   // translation part
   MAT(mOut,0,3) = - (MAT(mIn,0,3) * MAT(mOut,0,0) +
		     MAT(mIn,1,3) * MAT(mOut,0,1) +
		     MAT(mIn,2,3) * MAT(mOut,0,2) );
   MAT(mOut,1,3) = - (MAT(mIn,0,3) * MAT(mOut,1,0) +
		     MAT(mIn,1,3) * MAT(mOut,1,1) +
		     MAT(mIn,2,3) * MAT(mOut,1,2) );
   MAT(mOut,2,3) = - (MAT(mIn,0,3) * MAT(mOut,2,0) +
		     MAT(mIn,1,3) * MAT(mOut,2,1) +
		     MAT(mIn,2,3) * MAT(mOut,2,2) );
             
   return YES;
}




#define A(row,col)  a[(col<<2) + row]
#define B(row,col)  b[(col<<2) + row]
#define P(row,col)  product[(col<<2) + row]

static void matmul4(float *product, const float *a, const float *b)
{
   float prod[16];
   float *orig = NULL;
   
   // check if multiplication is in-place
   if (product == a || product == b) {
      orig = product;
	  product = prod;
   }
   
   int i;
   for (i = 0; i < 4; i++) {
      const float ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
      P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
      P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
      P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
      P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
   }
   
   if (orig) {
	  memcpy(orig, prod, sizeof(float)*16);
   }
}


static void transpose_matrix_4x4(float to[16], const float orig[16])
{
   // defend against in-place operation
   float from[16];
   memcpy(from, orig, 16*sizeof(float));
    
   to[0] = from[0];
   to[1] = from[4];
   to[2] = from[8];
   to[3] = from[12];
   to[4] = from[1];
   to[5] = from[5];
   to[6] = from[9];
   to[7] = from[13];
   to[8] = from[2];
   to[9] = from[6];
   to[10] = from[10];
   to[11] = from[14];
   to[12] = from[3];
   to[13] = from[7];
   to[14] = from[11];
   to[15] = from[15];
}


static void transform_vector(float u[4], const float v[4], const float *m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
   
   u[0] = v0 * MAT(m,0,0) + v1 * MAT(m,1,0) + v2 * MAT(m,2,0) + v3 * MAT(m,3,0);
   
   ///printf("   ... %f (%f) %f (%f) %f (%f) .. \n", MAT(m,0,2), v0 * MAT(m,0,2), MAT(m,1,2), v1 * MAT(m,1,2), MAT(m,2,2), v2 * MAT(m,2,2));
   
   u[1] = v0 * MAT(m,0,1) + v1 * MAT(m,1,1) + v2 * MAT(m,2,1) + v3 * MAT(m,3,1);
   u[2] = v0 * MAT(m,0,2) + v1 * MAT(m,1,2) + v2 * MAT(m,2,2) + v3 * MAT(m,3,2);
   u[3] = v0 * MAT(m,0,3) + v1 * MAT(m,1,3) + v2 * MAT(m,2,3) + v3 * MAT(m,3,3);
}

