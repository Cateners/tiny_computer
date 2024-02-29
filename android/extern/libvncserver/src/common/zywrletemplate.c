
/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE 'ZYWRLE' VNC CODEC SOURCE CODE.         *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A FOLLOWING BSD-STYLE SOURCE LICENSE.                *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE 'ZYWRLE' VNC CODEC SOURCE CODE IS (C) COPYRIGHT 2006         *
 * BY Hitachi Systems & Services, Ltd.                              *
 * (Noriaki Yamazaki, Research & Development Center)               *                                                                 *
 *                                                                  *
 ********************************************************************
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Hitachi Systems & Services, Ltd. nor
the names of its contributors may be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************/

/* Change Log:
     V0.02 : 2008/02/04 : Fix mis encode/decode when width != scanline
	                     (Thanks Johannes Schindelin, author of LibVNC
						  Server/Client)
     V0.01 : 2007/02/06 : Initial release
*/

/* #define ZYWRLE_ENCODE */
/* #define ZYWRLE_DECODE */
#define ZYWRLE_QUANTIZE

/*
[References]
 PLHarr:
   Senecal, J. G., P. Lindstrom, M. A. Duchaineau, and K. I. Joy, "An Improved N-Bit to N-Bit Reversible Haar-Like Transform," Pacific Graphics 2004, October 2004, pp. 371-380.
 EZW:
   Shapiro, JM: Embedded Image Coding Using Zerotrees of Wavelet Coefficients, IEEE Trans. Signal. Process., Vol.41, pp.3445-3462 (1993).
*/


/* Template Macro stuffs. */
#undef ZYWRLE_ANALYZE
#undef ZYWRLE_SYNTHESIZE
#define ZYWRLE_ANALYZE __RFB_CONCAT3E(zywrleAnalyze,BPP,END_FIX)
#define ZYWRLE_SYNTHESIZE __RFB_CONCAT3E(zywrleSynthesize,BPP,END_FIX)

#define ZYWRLE_RGBYUV __RFB_CONCAT3E(zywrleRGBYUV,BPP,END_FIX)
#define ZYWRLE_YUVRGB __RFB_CONCAT3E(zywrleYUVRGB,BPP,END_FIX)
#define ZYWRLE_YMASK __RFB_CONCAT2E(ZYWRLE_YMASK,BPP)
#define ZYWRLE_UVMASK __RFB_CONCAT2E(ZYWRLE_UVMASK,BPP)
#define ZYWRLE_LOAD_PIXEL __RFB_CONCAT2E(ZYWRLE_LOAD_PIXEL,BPP)
#define ZYWRLE_SAVE_PIXEL __RFB_CONCAT2E(ZYWRLE_SAVE_PIXEL,BPP)

/* Packing/Unpacking pixel stuffs.
   Endian conversion stuffs. */
#undef S_0
#undef S_1
#undef L_0
#undef L_1
#undef L_2
#if ZYWRLE_ENDIAN == ENDIAN_BIG
#  define S_0	1
#  define S_1	0
#  define L_0	3
#  define L_1	2
#  define L_2	1
#else
#  define S_0	0
#  define S_1	1
#  define L_0	0
#  define L_1	1
#  define L_2	2
#endif

/*   Load/Save pixel stuffs. */
#define ZYWRLE_YMASK15  0xFFFFFFF8
#define ZYWRLE_UVMASK15 0xFFFFFFF8
#define ZYWRLE_LOAD_PIXEL15(pSrc,R,G,B) { \
	R =  (((unsigned char*)pSrc)[S_1]<< 1)& 0xF8;	\
	G = ((((unsigned char*)pSrc)[S_1]<< 6)|(((unsigned char*)pSrc)[S_0]>> 2))& 0xF8;	\
	B =  (((unsigned char*)pSrc)[S_0]<< 3)& 0xF8;	\
}
#define ZYWRLE_SAVE_PIXEL15(pDst,R,G,B) { \
	R &= 0xF8;	\
	G &= 0xF8;	\
	B &= 0xF8;	\
	((unsigned char*)pDst)[S_1] = (unsigned char)( (R>>1)|(G>>6)       );	\
	((unsigned char*)pDst)[S_0] = (unsigned char)(((B>>3)|(G<<2))& 0xFF);	\
}
#define ZYWRLE_YMASK16  0xFFFFFFFC
#define ZYWRLE_UVMASK16 0xFFFFFFF8
#define ZYWRLE_LOAD_PIXEL16(pSrc,R,G,B) { \
	R =   ((unsigned char*)pSrc)[S_1]     & 0xF8;	\
	G = ((((unsigned char*)pSrc)[S_1]<< 5)|(((unsigned char*)pSrc)[S_0]>> 3))& 0xFC;	\
	B =  (((unsigned char*)pSrc)[S_0]<< 3)& 0xF8;	\
}
#define ZYWRLE_SAVE_PIXEL16(pDst,R,G,B) { \
	R &= 0xF8;	\
	G &= 0xFC;	\
	B &= 0xF8;	\
	((unsigned char*)pDst)[S_1] = (unsigned char)(  R    |(G>>5)       );	\
	((unsigned char*)pDst)[S_0] = (unsigned char)(((B>>3)|(G<<3))& 0xFF);	\
}
#define ZYWRLE_YMASK32  0xFFFFFFFF
#define ZYWRLE_UVMASK32 0xFFFFFFFF
#define ZYWRLE_LOAD_PIXEL32(pSrc,R,G,B) { \
	R = ((unsigned char*)pSrc)[L_2];	\
	G = ((unsigned char*)pSrc)[L_1];	\
	B = ((unsigned char*)pSrc)[L_0];	\
}
#define ZYWRLE_SAVE_PIXEL32(pDst,R,G,B) { \
	((unsigned char*)pDst)[L_2] = (unsigned char)R;	\
	((unsigned char*)pDst)[L_1] = (unsigned char)G;	\
	((unsigned char*)pDst)[L_0] = (unsigned char)B;	\
}

#ifndef ZYWRLE_ONCE
#define ZYWRLE_ONCE

#ifdef WIN32
#define InlineX __inline
#else
# ifndef __STRICT_ANSI__
#  define InlineX inline
# else
#  define InlineX
# endif
#endif

#ifdef ZYWRLE_ENCODE
/* Tables for Coefficients filtering. */
#  ifndef ZYWRLE_QUANTIZE
/* Type A:lower bit omitting of EZW style. */
const static unsigned int zywrleParam[3][3]={
	{0x0000F000,0x00000000,0x00000000},
	{0x0000C000,0x00F0F0F0,0x00000000},
	{0x0000C000,0x00C0C0C0,0x00F0F0F0},
/*	{0x0000FF00,0x00000000,0x00000000},
	{0x0000FF00,0x00FFFFFF,0x00000000},
	{0x0000FF00,0x00FFFFFF,0x00FFFFFF}, */
};
#  else
/* Type B:Non liner quantization filter. */
static const signed char zywrleConv[4][256]={
{	/* bi=5, bo=5 r=0.0:PSNR=24.849 */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
},
{	/* bi=5, bo=5 r=2.0:PSNR=74.031 */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 32,
	32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32,
	48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 56, 56, 56, 56, 56,
	56, 56, 56, 56, 64, 64, 64, 64,
	64, 64, 64, 64, 72, 72, 72, 72,
	72, 72, 72, 72, 80, 80, 80, 80,
	80, 80, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 96, 96,
	96, 96, 96, 104, 104, 104, 104, 104,
	104, 104, 104, 104, 104, 112, 112, 112,
	112, 112, 112, 112, 112, 112, 120, 120,
	120, 120, 120, 120, 120, 120, 120, 120,
	0, -120, -120, -120, -120, -120, -120, -120,
	-120, -120, -120, -112, -112, -112, -112, -112,
	-112, -112, -112, -112, -104, -104, -104, -104,
	-104, -104, -104, -104, -104, -104, -96, -96,
	-96, -96, -96, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -80,
	-80, -80, -80, -80, -80, -72, -72, -72,
	-72, -72, -72, -72, -72, -64, -64, -64,
	-64, -64, -64, -64, -64, -56, -56, -56,
	-56, -56, -56, -56, -56, -56, -48, -48,
	-48, -48, -48, -48, -48, -48, -48, -48,
	-48, -32, -32, -32, -32, -32, -32, -32,
	-32, -32, -32, -32, -32, -32, -32, -32,
	-32, -32, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
},
{	/* bi=5, bo=4 r=2.0:PSNR=64.441 */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48,
	64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64,
	80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	104, 104, 104, 104, 104, 104, 104, 104,
	104, 104, 104, 112, 112, 112, 112, 112,
	112, 112, 112, 112, 120, 120, 120, 120,
	120, 120, 120, 120, 120, 120, 120, 120,
	0, -120, -120, -120, -120, -120, -120, -120,
	-120, -120, -120, -120, -120, -112, -112, -112,
	-112, -112, -112, -112, -112, -112, -104, -104,
	-104, -104, -104, -104, -104, -104, -104, -104,
	-104, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -80, -80, -80, -80,
	-80, -80, -80, -80, -80, -80, -80, -80,
	-80, -64, -64, -64, -64, -64, -64, -64,
	-64, -64, -64, -64, -64, -64, -64, -64,
	-64, -48, -48, -48, -48, -48, -48, -48,
	-48, -48, -48, -48, -48, -48, -48, -48,
	-48, -48, -48, -48, -48, -48, -48, -48,
	-48, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
},
{	/* bi=5, bo=2 r=2.0:PSNR=43.175 */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	88, 88, 88, 88, 88, 88, 88, 88,
	0, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, -88, -88, -88, -88, -88, -88, -88,
	-88, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
}
};
const static signed char* zywrleParam[3][3][3]={
	{{zywrleConv[0],zywrleConv[2],zywrleConv[0]},{zywrleConv[0],zywrleConv[0],zywrleConv[0]},{zywrleConv[0],zywrleConv[0],zywrleConv[0]}},
	{{zywrleConv[0],zywrleConv[3],zywrleConv[0]},{zywrleConv[1],zywrleConv[1],zywrleConv[1]},{zywrleConv[0],zywrleConv[0],zywrleConv[0]}},
	{{zywrleConv[0],zywrleConv[3],zywrleConv[0]},{zywrleConv[2],zywrleConv[2],zywrleConv[2]},{zywrleConv[1],zywrleConv[1],zywrleConv[1]}},
};
#  endif
#endif

static InlineX void Harr(signed char* pX0, signed char* pX1)
{
	/* Piecewise-Linear Harr(PLHarr) */
	int X0 = (int)*pX0, X1 = (int)*pX1;
	int orgX0 = X0, orgX1 = X1;
	if ((X0 ^ X1) & 0x80) {
		/* differ sign */
		X1 += X0;
		if (((X1^orgX1)&0x80)==0) {
			/* |X1| > |X0| */
			X0 -= X1;	/* H = -B */
		}
	} else {
		/* same sign */
		X0 -= X1;
		if (((X0 ^ orgX0) & 0x80) == 0) {
			/* |X0| > |X1| */
			X1 += X0;	/* L = A */
		}
	}
	*pX0 = (signed char)X1;
	*pX1 = (signed char)X0;
}
/*
 1D-Wavelet transform.

 In coefficients array, the famous 'pyramid' decomposition is well used.

 1D Model:
   |L0L0L0L0|L0L0L0L0|H0H0H0H0|H0H0H0H0| : level 0
   |L1L1L1L1|H1H1H1H1|H0H0H0H0|H0H0H0H0| : level 1

 But this method needs line buffer because H/L is different position from X0/X1.
 So, I used 'interleave' decomposition instead of it.

 1D Model:
   |L0H0L0H0|L0H0L0H0|L0H0L0H0|L0H0L0H0| : level 0
   |L1H0H1H0|L1H0H1H0|L1H0H1H0|L1H0H1H0| : level 1

 In this method, H/L and X0/X1 is always same position.
 This lead us to more speed and less memory.
 Of cause, the result of both method is quite same
 because its only difference is that coefficient position.
*/
static InlineX void WaveletLevel(int* data, int size, int l, int SkipPixel)
{
	int s, ofs;
	signed char* pX0;
	signed char* end;

	pX0 = (signed char*)data;
	s = (8<<l)*SkipPixel;
	end = pX0+(size>>(l+1))*s;
	s -= 2;
	ofs = (4<<l)*SkipPixel;
	while (pX0 < end) {
		Harr(pX0, pX0+ofs);
		pX0++;
		Harr(pX0, pX0+ofs);
		pX0++;
		Harr(pX0, pX0+ofs);
		pX0 += s;
	}
}
#define InvWaveletLevel(d,s,l,pix) WaveletLevel(d,s,l,pix)

#ifdef ZYWRLE_ENCODE
#  ifndef ZYWRLE_QUANTIZE
/* Type A:lower bit omitting of EZW style. */
static InlineX void FilterWaveletSquare(int* pBuf, int width, int height, int level, int l)
{
	int r, s;
	int x, y;
	int* pH;
	const unsigned int* pM;

	pM = &(zywrleParam[level-1][l]);
	s = 2<<l;
	for (r = 1; r < 4; r++) {
		pH   = pBuf;
		if (r & 0x01)
			pH +=  s>>1;
		if (r & 0x02)
			pH += (s>>1)*width;
		for (y = 0; y < height / s; y++) {
			for (x = 0; x < width / s; x++) {
				/*
				 these are same following code.
				     pH[x] = pH[x] / (~pM[x]+1) * (~pM[x]+1);
				     ( round pH[x] with pM[x] bit )
				 '&' operator isn't 'round' but is 'floor'.
				 So, we must offset when pH[x] is negative.
				*/
				if (((signed char*)pH)[0] & 0x80)
					((signed char*)pH)[0] += ~((signed char*)pM)[0];
				if (((signed char*)pH)[1] & 0x80)
					((signed char*)pH)[1] += ~((signed char*)pM)[1];
				if (((signed char*)pH)[2] & 0x80)
					((signed char*)pH)[2] += ~((signed char*)pM)[2];
				*pH &= *pM;
				pH += s;
			}
			pH += (s-1)*width;
		}
	}
}
#  else
/*
 Type B:Non liner quantization filter.

 Coefficients have Gaussian curve and smaller value which is
 large part of coefficients isn't more important than larger value.
 So, I use filter of Non liner quantize/dequantize table.
 In general, Non liner quantize formula is explained as following.

    y=f(x)   = sign(x)*round( ((abs(x)/(2^7))^ r   )* 2^(bo-1) )*2^(8-bo)
    x=f-1(y) = sign(y)*round( ((abs(y)/(2^7))^(1/r))* 2^(bi-1) )*2^(8-bi)
 ( r:power coefficient  bi:effective MSB in input  bo:effective MSB in output )

   r < 1.0 : Smaller value is more important than larger value.
   r > 1.0 : Larger value is more important than smaller value.
   r = 1.0 : Liner quantization which is same with EZW style.

 r = 0.75 is famous non liner quantization used in MP3 audio codec.
 In contrast to audio data, larger value is important in wavelet coefficients.
 So, I select r = 2.0 table( quantize is x^2, dequantize sqrt(x) ).

 As compared with EZW style liner quantization, this filter tended to be
 more sharp edge and be more compression rate but be more blocking noise and be less quality.
 Especially, the surface of graphic objects has distinguishable noise in middle quality mode.

 We need only quantized-dequantized(filtered) value rather than quantized value itself
 because all values are packed or palette-lized in later ZRLE section.
 This lead us not to need to modify client decoder when we change
 the filtering procedure in future.
 Client only decodes coefficients given by encoder.
*/
static InlineX void FilterWaveletSquare(int* pBuf, int width, int height, int level, int l)
{
	int r, s;
	int x, y;
	int* pH;
	const signed char** pM;

	pM = zywrleParam[level-1][l];
	s = 2<<l;
	for (r = 1; r < 4; r++) {
		pH   = pBuf;
		if (r & 0x01)
			pH +=  s>>1;
		if (r & 0x02)
			pH += (s>>1)*width;
		for (y = 0; y < height / s; y++) {
			for (x = 0; x < width / s; x++) {
				((signed char*)pH)[0] = pM[0][((unsigned char*)pH)[0]];
				((signed char*)pH)[1] = pM[1][((unsigned char*)pH)[1]];
				((signed char*)pH)[2] = pM[2][((unsigned char*)pH)[2]];
				pH += s;
			}
			pH += (s-1)*width;
		}
	}
}
#  endif

static InlineX void Wavelet(int* pBuf, int width, int height, int level)
{
	int l, s;
	int* pTop;
	int* pEnd;

	for (l = 0; l < level; l++) {
		pTop = pBuf;
		pEnd = pBuf+height*width;
		s = width<<l;
		while (pTop < pEnd) {
			WaveletLevel(pTop, width, l, 1);
			pTop += s;
		}
		pTop = pBuf;
		pEnd = pBuf+width;
		s = 1<<l;
		while (pTop < pEnd) {
			WaveletLevel(pTop, height,l, width);
			pTop += s;
		}
		FilterWaveletSquare(pBuf, width, height, level, l);
	}
}
#endif
#ifdef ZYWRLE_DECODE
static InlineX void InvWavelet(int* pBuf, int width, int height, int level)
{
	int l, s;
	int* pTop;
	int* pEnd;

	for (l = level - 1; l >= 0; l--) {
		pTop = pBuf;
		pEnd = pBuf+width;
		s = 1<<l;
		while (pTop < pEnd) {
			InvWaveletLevel(pTop, height,l, width);
			pTop += s;
		}
		pTop = pBuf;
		pEnd = pBuf+height*width;
		s = width<<l;
		while (pTop < pEnd) {
			InvWaveletLevel(pTop, width, l, 1);
			pTop += s;
		}
	}
}
#endif

/* Load/Save coefficients stuffs.
 Coefficients manages as 24 bits little-endian pixel. */
#define ZYWRLE_LOAD_COEFF(pSrc,R,G,B) { \
	R = ((signed char*)pSrc)[2];	\
	G = ((signed char*)pSrc)[1];	\
	B = ((signed char*)pSrc)[0];	\
}
#define ZYWRLE_SAVE_COEFF(pDst,R,G,B) { \
	((signed char*)pDst)[2] = (signed char)R;	\
	((signed char*)pDst)[1] = (signed char)G;	\
	((signed char*)pDst)[0] = (signed char)B;	\
}

/*
 RGB <=> YUV conversion stuffs.
 YUV coversion is explained as following formula in strict meaning:
   Y =  0.299R + 0.587G + 0.114B (   0<=Y<=255)
   U = -0.169R - 0.331G + 0.500B (-128<=U<=127)
   V =  0.500R - 0.419G - 0.081B (-128<=V<=127)

 I use simple conversion RCT(reversible color transform) which is described
 in JPEG-2000 specification.
   Y = (R + 2G + B)/4 (   0<=Y<=255)
   U = B-G (-256<=U<=255)
   V = R-G (-256<=V<=255)
*/
#define ROUND(x) (((x)<0)?0:(((x)>255)?255:(x)))
	/* RCT is N-bit RGB to N-bit Y and N+1-bit UV.
	 For make Same N-bit, UV is lossy.
	 More exact PLHarr, we reduce to odd range(-127<=x<=127). */
#define ZYWRLE_RGBYUV1(R,G,B,Y,U,V,ymask,uvmask) { \
	Y = (R+(G<<1)+B)>>2;	\
	U =  B-G;	\
	V =  R-G;	\
	Y -= 128;	\
	U >>= 1;	\
	V >>= 1;	\
	Y &= ymask;	\
	U &= uvmask;	\
	V &= uvmask;	\
	if (Y == -128)	\
		Y += (0xFFFFFFFF-ymask+1);	\
	if (U == -128)	\
		U += (0xFFFFFFFF-uvmask+1);	\
	if (V == -128)	\
		V += (0xFFFFFFFF-uvmask+1);	\
}
#define ZYWRLE_YUVRGB1(R,G,B,Y,U,V) { \
	Y += 128;	\
	U <<= 1;	\
	V <<= 1;	\
	G = Y-((U+V)>>2);	\
	B = U+G;	\
	R = V+G;	\
	G = ROUND(G);	\
	B = ROUND(B);	\
	R = ROUND(R);	\
}

/*
 coefficient packing/unpacking stuffs.
 Wavelet transform makes 4 sub coefficient image from 1 original image.

 model with pyramid decomposition:
   +------+------+
   |      |      |
   |  L   |  Hx  |
   |      |      |
   +------+------+
   |      |      |
   |  H   |  Hxy |
   |      |      |
   +------+------+

 So, we must transfer each sub images individually in strict meaning.
 But at least ZRLE meaning, following one decompositon image is same as
 avobe individual sub image. I use this format.
 (Strictly saying, transfer order is reverse(Hxy->Hy->Hx->L)
  for simplified procedure for any wavelet level.)

   +------+------+
   |      L      |
   +------+------+
   |      Hx     |
   +------+------+
   |      Hy     |
   +------+------+
   |      Hxy    |
   +------+------+
*/
#define INC_PTR(data) \
	data++;	\
	if( data-pData >= (w+uw) ){	\
		data += scanline-(w+uw);	\
		pData = data;	\
	}

#define ZYWRLE_TRANSFER_COEFF(pBuf,data,r,w,h,scanline,level,TRANS)	\
	pH = pBuf;	\
	s = 2<<level;	\
	if (r & 0x01)	\
		pH +=  s>>1;	\
	if (r & 0x02)	\
		pH += (s>>1)*w;	\
	pEnd = pH+h*w;	\
	while (pH < pEnd) {	\
		pLine = pH+w;	\
		while (pH < pLine) {	\
			TRANS	\
			INC_PTR(data)	\
			pH += s;	\
		}	\
		pH += (s-1)*w;	\
	}

#define ZYWRLE_PACK_COEFF(pBuf,data,r,width,height,scanline,level)	\
	ZYWRLE_TRANSFER_COEFF(pBuf,data,r,width,height,scanline,level,ZYWRLE_LOAD_COEFF(pH,R,G,B);ZYWRLE_SAVE_PIXEL(data,R,G,B);)

#define ZYWRLE_UNPACK_COEFF(pBuf,data,r,width,height,scanline,level)	\
	ZYWRLE_TRANSFER_COEFF(pBuf,data,r,width,height,scanline,level,ZYWRLE_LOAD_PIXEL(data,R,G,B);ZYWRLE_SAVE_COEFF(pH,R,G,B);)

#define ZYWRLE_SAVE_UNALIGN(data,TRANS)	\
	pTop = pBuf+w*h;	\
	pEnd = pBuf + (w+uw)*(h+uh);	\
	while (pTop < pEnd) {	\
		TRANS	\
		INC_PTR(data)	\
		pTop++;	\
	}

#define ZYWRLE_LOAD_UNALIGN(data,TRANS)	\
	pTop = pBuf+w*h;	\
	if (uw) {	\
		pData=         data + w;	\
		pEnd = (int*)(pData+ h*scanline);	\
		while (pData < (PIXEL_T*)pEnd) {	\
			pLine = (int*)(pData + uw);	\
			while (pData < (PIXEL_T*)pLine) {	\
				TRANS	\
				pData++;	\
				pTop++;	\
			}	\
			pData += scanline-uw;	\
		}	\
	}	\
	if (uh) {	\
		pData=         data +  h*scanline;	\
		pEnd = (int*)(pData+ uh*scanline);	\
		while (pData < (PIXEL_T*)pEnd) {	\
			pLine = (int*)(pData + w);	\
			while (pData < (PIXEL_T*)pLine) {	\
				TRANS	\
				pData++;	\
				pTop++;	\
			}	\
			pData += scanline-w;	\
		}	\
	}	\
	if (uw && uh) {	\
		pData=         data + w+ h*scanline;	\
		pEnd = (int*)(pData+   uh*scanline);	\
		while (pData < (PIXEL_T*)pEnd) {	\
			pLine = (int*)(pData + uw);	\
			while (pData < (PIXEL_T*)pLine) {	\
				TRANS	\
				pData++;	\
				pTop++;	\
			}	\
			pData += scanline-uw;	\
		}	\
	}

static InlineX void zywrleCalcSize(int* pW, int* pH, int level)
{
	*pW &= ~((1<<level)-1);
	*pH &= ~((1<<level)-1);
}

#endif /* ZYWRLE_ONCE */

#ifndef CPIXEL
#ifdef ZYWRLE_ENCODE
static InlineX void ZYWRLE_RGBYUV(int* pBuf, PIXEL_T* data, int width, int height, int scanline)
{
	int R, G, B;
	int Y, U, V;
	int* pLine;
	int* pEnd;
	pEnd = pBuf+height*width;
	while (pBuf < pEnd) {
		pLine = pBuf+width;
		while (pBuf < pLine) {
			ZYWRLE_LOAD_PIXEL(data,R,G,B);
			ZYWRLE_RGBYUV1(R,G,B,Y,U,V,ZYWRLE_YMASK,ZYWRLE_UVMASK);
			ZYWRLE_SAVE_COEFF(pBuf,V,Y,U);
			pBuf++;
			data++;
		}
		data += scanline-width;
	}
}
#endif
#ifdef ZYWRLE_DECODE
static InlineX void ZYWRLE_YUVRGB(int* pBuf, PIXEL_T* data, int width, int height, int scanline) {
	int R, G, B;
	int Y, U, V;
	int* pLine;
	int* pEnd;
	pEnd = pBuf+height*width;
	while (pBuf < pEnd) {
		pLine = pBuf+width;
		while (pBuf < pLine) {
			ZYWRLE_LOAD_COEFF(pBuf,V,Y,U);
			ZYWRLE_YUVRGB1(R,G,B,Y,U,V);
			ZYWRLE_SAVE_PIXEL(data,R,G,B);
			pBuf++;
			data++;
		}
		data += scanline-width;
	}
}
#endif

#ifdef ZYWRLE_ENCODE
PIXEL_T* ZYWRLE_ANALYZE(PIXEL_T* dst, PIXEL_T* src, int w, int h, int scanline, int level, int* pBuf) {
	int l;
	int uw = w;
	int uh = h;
	int* pTop;
	int* pEnd;
	int* pLine;
	PIXEL_T* pData;
	int R, G, B;
	int s;
	int* pH;

	zywrleCalcSize(&w, &h, level);
	if (w == 0 || h == 0)
		return NULL;
	uw -= w;
	uh -= h;

	pData = dst;
	ZYWRLE_LOAD_UNALIGN(src,*(PIXEL_T*)pTop=*pData;)
	ZYWRLE_RGBYUV(pBuf, src, w, h, scanline);
	Wavelet(pBuf, w, h, level);
	for (l = 0; l < level; l++) {
		ZYWRLE_PACK_COEFF(pBuf, dst, 3, w, h, scanline, l);
		ZYWRLE_PACK_COEFF(pBuf, dst, 2, w, h, scanline, l);
		ZYWRLE_PACK_COEFF(pBuf, dst, 1, w, h, scanline, l);
		if (l == level - 1) {
			ZYWRLE_PACK_COEFF(pBuf, dst, 0, w, h, scanline, l);
		}
	}
	ZYWRLE_SAVE_UNALIGN(dst,*dst=*(PIXEL_T*)pTop;)
	return dst;
}
#endif
#ifdef ZYWRLE_DECODE
PIXEL_T* ZYWRLE_SYNTHESIZE(PIXEL_T* dst, PIXEL_T* src, int w, int h, int scanline, int level, int* pBuf)
{
	int l;
	int uw = w;
	int uh = h;
	int* pTop;
	int* pEnd;
	int* pLine;
	PIXEL_T* pData;
	int R, G, B;
	int s;
	int* pH;

	zywrleCalcSize(&w, &h, level);
	if (w == 0 || h == 0)
		return NULL;
	uw -= w;
	uh -= h;

	pData = src;
	for (l = 0; l < level; l++) {
		ZYWRLE_UNPACK_COEFF(pBuf, src, 3, w, h, scanline, l);
		ZYWRLE_UNPACK_COEFF(pBuf, src, 2, w, h, scanline, l);
		ZYWRLE_UNPACK_COEFF(pBuf, src, 1, w, h, scanline, l);
		if (l == level - 1) {
			ZYWRLE_UNPACK_COEFF(pBuf, src, 0, w, h, scanline, l);
		}
	}
	ZYWRLE_SAVE_UNALIGN(src,*(PIXEL_T*)pTop=*src;)
	InvWavelet(pBuf, w, h, level);
	ZYWRLE_YUVRGB(pBuf, dst, w, h, scanline);
	ZYWRLE_LOAD_UNALIGN(dst,*pData=*(PIXEL_T*)pTop;)
	return src;
}
#endif
#endif  /* CPIXEL */

#undef ZYWRLE_RGBYUV
#undef ZYWRLE_YUVRGB
#undef ZYWRLE_LOAD_PIXEL
#undef ZYWRLE_SAVE_PIXEL
