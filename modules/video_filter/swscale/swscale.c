/*
    Copyright (C) 2001-2003 Michael Niedermayer <michaelni@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  supported Input formats: YV12, I420/IYUV, YUY2, UYVY, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8/Y800, YVU9/IF09
  supported output formats: YV12, I420/IYUV, YUY2, UYVY, {BGR,RGB}{1,4,8,15,16,24,32}, Y8/Y800, YVU9/IF09
  {BGR,RGB}{1,4,8,15,16} support dithering
  
  unscaled special converters (YV12=I420=IYUV, Y800=Y8)
  YV12 -> {BGR,RGB}{1,4,8,15,16,24,32}
  x -> x
  YUV9 -> YV12
  YUV9/YV12 -> Y800
  Y800 -> YUV9/YV12
  BGR24 -> BGR32 & RGB24 -> RGB32
  BGR32 -> BGR24 & RGB32 -> RGB24
  BGR15 -> BGR16
*/

/* 
tested special converters (most are tested actually but i didnt write it down ...)
 YV12 -> BGR16
 YV12 -> YV12
 BGR15 -> BGR16
 BGR16 -> BGR16
 YVU9 -> YV12

untested special converters
  YV12/I420 -> BGR15/BGR24/BGR32 (its the yuv2rgb stuff, so it should be ok)
  YV12/I420 -> YV12/I420
  YUY2/BGR15/BGR24/BGR32/RGB24/RGB32 -> same format
  BGR24 -> BGR32 & RGB24 -> RGB32
  BGR32 -> BGR24 & RGB32 -> RGB24
  BGR24 -> YV12
*/

#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "config.h"
#include <assert.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include "swscale.h"
#include "swscale_internal.h"
#include "common.h"
#include "rgb2rgb.h"
#define RUNTIME_CPUDETECT 1

#undef MOVNTQ
#undef PAVGB

//#undef HAVE_MMX2
//#define HAVE_3DNOW
//#undef HAVE_MMX
//#undef ARCH_X86
//#define WORDS_BIGENDIAN
#define DITHER1XBPP

#define FAST_BGR2YV12 // use 7 bit coeffs instead of 15bit

#define RET 0xC3 //near return opcode for X86

#ifdef MP_DEBUG
#define ASSERT(x) assert(x);
#else
#define ASSERT(x) ;
#endif

#ifdef M_PI
#define PI M_PI
#else
#define PI 3.14159265358979323846
#endif

//FIXME replace this with something faster
#define isPlanarYUV(x) ((x)==IMGFMT_YV12 || (x)==IMGFMT_YVU9 \
			|| (x)==IMGFMT_444P || (x)==IMGFMT_422P || (x)==IMGFMT_411P)
#define isYUV(x)       ((x)==IMGFMT_UYVY || (x)==IMGFMT_YUY2 || isPlanarYUV(x))
#define isGray(x)      ((x)==IMGFMT_Y800)
#define isRGB(x)       (((x)&IMGFMT_RGB_MASK)==IMGFMT_RGB)
#define isBGR(x)       (((x)&IMGFMT_BGR_MASK)==IMGFMT_BGR)
#define isSupportedIn(x)  ((x)==IMGFMT_YV12 || (x)==IMGFMT_YUY2 || (x)==IMGFMT_UYVY\
			|| (x)==IMGFMT_BGR32|| (x)==IMGFMT_BGR24|| (x)==IMGFMT_BGR16|| (x)==IMGFMT_BGR15\
			|| (x)==IMGFMT_RGB32|| (x)==IMGFMT_RGB24\
			|| (x)==IMGFMT_Y800 || (x)==IMGFMT_YVU9\
			|| (x)==IMGFMT_444P || (x)==IMGFMT_422P || (x)==IMGFMT_411P)
#define isSupportedOut(x) ((x)==IMGFMT_YV12 || (x)==IMGFMT_YUY2 || (x)==IMGFMT_UYVY\
			|| (x)==IMGFMT_444P || (x)==IMGFMT_422P || (x)==IMGFMT_411P\
			|| isRGB(x) || isBGR(x)\
			|| (x)==IMGFMT_Y800 || (x)==IMGFMT_YVU9)
#define isPacked(x)    ((x)==IMGFMT_YUY2 || (x)==IMGFMT_UYVY ||isRGB(x) || isBGR(x))

#define RGB2YUV_SHIFT 16
#define BY ((int)( 0.098*(1<<RGB2YUV_SHIFT)+0.5))
#define BV ((int)(-0.071*(1<<RGB2YUV_SHIFT)+0.5))
#define BU ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define GY ((int)( 0.504*(1<<RGB2YUV_SHIFT)+0.5))
#define GV ((int)(-0.368*(1<<RGB2YUV_SHIFT)+0.5))
#define GU ((int)(-0.291*(1<<RGB2YUV_SHIFT)+0.5))
#define RY ((int)( 0.257*(1<<RGB2YUV_SHIFT)+0.5))
#define RV ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define RU ((int)(-0.148*(1<<RGB2YUV_SHIFT)+0.5))

extern const int32_t Inverse_Table_6_9[8][4];

/*
NOTES
Special versions: fast Y 1:1 scaling (no interpolation in y direction)

TODO
more intelligent missalignment avoidance for the horizontal scaler
write special vertical cubic upscale version
Optimize C code (yv12 / minmax)
add support for packed pixel yuv input & output
add support for Y8 output
optimize bgr24 & bgr32
add BGR4 output support
write special BGR->BGR scaler
*/

#define ABS(a) ((a) > 0 ? (a) : (-(a)))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

#ifdef ARCH_X86
static uint64_t attribute_used __attribute__((aligned(8))) bF8=       0xF8F8F8F8F8F8F8F8LL;
static uint64_t attribute_used __attribute__((aligned(8))) bFC=       0xFCFCFCFCFCFCFCFCLL;
static uint64_t __attribute__((aligned(8))) w10=       0x0010001000100010LL;
static uint64_t attribute_used __attribute__((aligned(8))) w02=       0x0002000200020002LL;
static uint64_t attribute_used __attribute__((aligned(8))) bm00001111=0x00000000FFFFFFFFLL;
static uint64_t attribute_used __attribute__((aligned(8))) bm00000111=0x0000000000FFFFFFLL;
static uint64_t attribute_used __attribute__((aligned(8))) bm11111000=0xFFFFFFFFFF000000LL;
static uint64_t attribute_used __attribute__((aligned(8))) bm01010101=0x00FF00FF00FF00FFLL;

static volatile uint64_t attribute_used __attribute__((aligned(8))) b5Dither;
static volatile uint64_t attribute_used __attribute__((aligned(8))) g5Dither;
static volatile uint64_t attribute_used __attribute__((aligned(8))) g6Dither;
static volatile uint64_t attribute_used __attribute__((aligned(8))) r5Dither;

static uint64_t __attribute__((aligned(8))) dither4[2]={
	0x0103010301030103LL,
	0x0200020002000200LL,};

static uint64_t __attribute__((aligned(8))) dither8[2]={
	0x0602060206020602LL,
	0x0004000400040004LL,};

static uint64_t __attribute__((aligned(8))) b16Mask=   0x001F001F001F001FLL;
static uint64_t attribute_used __attribute__((aligned(8))) g16Mask=   0x07E007E007E007E0LL;
static uint64_t attribute_used __attribute__((aligned(8))) r16Mask=   0xF800F800F800F800LL;
static uint64_t __attribute__((aligned(8))) b15Mask=   0x001F001F001F001FLL;
static uint64_t attribute_used __attribute__((aligned(8))) g15Mask=   0x03E003E003E003E0LL;
static uint64_t attribute_used __attribute__((aligned(8))) r15Mask=   0x7C007C007C007C00LL;

static uint64_t attribute_used __attribute__((aligned(8))) M24A=   0x00FF0000FF0000FFLL;
static uint64_t attribute_used __attribute__((aligned(8))) M24B=   0xFF0000FF0000FF00LL;
static uint64_t attribute_used __attribute__((aligned(8))) M24C=   0x0000FF0000FF0000LL;

#ifdef FAST_BGR2YV12
static const uint64_t bgr2YCoeff  attribute_used __attribute__((aligned(8))) = 0x000000210041000DULL;
static const uint64_t bgr2UCoeff  attribute_used __attribute__((aligned(8))) = 0x0000FFEEFFDC0038ULL;
static const uint64_t bgr2VCoeff  attribute_used __attribute__((aligned(8))) = 0x00000038FFD2FFF8ULL;
#else
static const uint64_t bgr2YCoeff  attribute_used __attribute__((aligned(8))) = 0x000020E540830C8BULL;
static const uint64_t bgr2UCoeff  attribute_used __attribute__((aligned(8))) = 0x0000ED0FDAC23831ULL;
static const uint64_t bgr2VCoeff  attribute_used __attribute__((aligned(8))) = 0x00003831D0E6F6EAULL;
#endif
static const uint64_t bgr2YOffset attribute_used __attribute__((aligned(8))) = 0x1010101010101010ULL;
static const uint64_t bgr2UVOffset attribute_used __attribute__((aligned(8)))= 0x8080808080808080ULL;
static const uint64_t w1111       attribute_used __attribute__((aligned(8))) = 0x0001000100010001ULL;
#endif

// clipping helper table for C implementations:
static unsigned char clip_table[768];

static SwsVector *sws_getConvVec(SwsVector *a, SwsVector *b);
		  
extern const uint8_t dither_2x2_4[2][8];
extern const uint8_t dither_2x2_8[2][8];
extern const uint8_t dither_8x8_32[8][8];
extern const uint8_t dither_8x8_73[8][8];
extern const uint8_t dither_8x8_220[8][8];

#ifdef ARCH_X86
void in_asm_used_var_warning_killer()
{
 volatile int i= bF8+bFC+w10+
 bm00001111+bm00000111+bm11111000+b16Mask+g16Mask+r16Mask+b15Mask+g15Mask+r15Mask+
 M24A+M24B+M24C+w02 + b5Dither+g5Dither+r5Dither+g6Dither+dither4[0]+dither8[0]+bm01010101;
 if(i) i=0;
}
#endif

static inline void yuv2yuvXinC(int16_t *lumFilter, int16_t **lumSrc, int lumFilterSize,
				    int16_t *chrFilter, int16_t **chrSrc, int chrFilterSize,
				    uint8_t *dest, uint8_t *uDest, uint8_t *vDest, int dstW, int chrDstW)
{
	//FIXME Optimize (just quickly writen not opti..)
	int i;
	for(i=0; i<dstW; i++)
	{
		int val=1<<18;
		int j;
		for(j=0; j<lumFilterSize; j++)
			val += lumSrc[j][i] * lumFilter[j];

		dest[i]= MIN(MAX(val>>19, 0), 255);
	}

	if(uDest != NULL)
		for(i=0; i<chrDstW; i++)
		{
			int u=1<<18;
			int v=1<<18;
			int j;
			for(j=0; j<chrFilterSize; j++)
			{
				u += chrSrc[j][i] * chrFilter[j];
				v += chrSrc[j][i + 2048] * chrFilter[j];
			}

			uDest[i]= MIN(MAX(u>>19, 0), 255);
			vDest[i]= MIN(MAX(v>>19, 0), 255);
		}
}


#define YSCALE_YUV_2_PACKEDX_C(type) \
		for(i=0; i<(dstW>>1); i++){\
			int j;\
			int Y1=1<<18;\
			int Y2=1<<18;\
			int U=1<<18;\
			int V=1<<18;\
			type *r, *b, *g;\
			const int i2= 2*i;\
			\
			for(j=0; j<lumFilterSize; j++)\
			{\
				Y1 += lumSrc[j][i2] * lumFilter[j];\
				Y2 += lumSrc[j][i2+1] * lumFilter[j];\
			}\
			for(j=0; j<chrFilterSize; j++)\
			{\
				U += chrSrc[j][i] * chrFilter[j];\
				V += chrSrc[j][i+2048] * chrFilter[j];\
			}\
			Y1>>=19;\
			Y2>>=19;\
			U >>=19;\
			V >>=19;\
			if((Y1|Y2|U|V)&256)\
			{\
				if(Y1>255)   Y1=255;\
				else if(Y1<0)Y1=0;\
				if(Y2>255)   Y2=255;\
				else if(Y2<0)Y2=0;\
				if(U>255)    U=255;\
				else if(U<0) U=0;\
				if(V>255)    V=255;\
				else if(V<0) V=0;\
			}
                        
#define YSCALE_YUV_2_RGBX_C(type) \
			YSCALE_YUV_2_PACKEDX_C(type)\
			r = c->table_rV[V];\
			g = c->table_gU[U] + c->table_gV[V];\
			b = c->table_bU[U];\

#define YSCALE_YUV_2_PACKED2_C \
		for(i=0; i<(dstW>>1); i++){\
			const int i2= 2*i;\
			int Y1= (buf0[i2  ]*yalpha1+buf1[i2  ]*yalpha)>>19;\
			int Y2= (buf0[i2+1]*yalpha1+buf1[i2+1]*yalpha)>>19;\
			int U= (uvbuf0[i     ]*uvalpha1+uvbuf1[i     ]*uvalpha)>>19;\
			int V= (uvbuf0[i+2048]*uvalpha1+uvbuf1[i+2048]*uvalpha)>>19;\

#define YSCALE_YUV_2_RGB2_C(type) \
			YSCALE_YUV_2_PACKED2_C\
			type *r, *b, *g;\
			r = c->table_rV[V];\
			g = c->table_gU[U] + c->table_gV[V];\
			b = c->table_bU[U];\

#define YSCALE_YUV_2_PACKED1_C \
		for(i=0; i<(dstW>>1); i++){\
			const int i2= 2*i;\
			int Y1= buf0[i2  ]>>7;\
			int Y2= buf0[i2+1]>>7;\
			int U= (uvbuf1[i     ])>>7;\
			int V= (uvbuf1[i+2048])>>7;\

#define YSCALE_YUV_2_RGB1_C(type) \
			YSCALE_YUV_2_PACKED1_C\
			type *r, *b, *g;\
			r = c->table_rV[V];\
			g = c->table_gU[U] + c->table_gV[V];\
			b = c->table_bU[U];\

#define YSCALE_YUV_2_PACKED1B_C \
		for(i=0; i<(dstW>>1); i++){\
			const int i2= 2*i;\
			int Y1= buf0[i2  ]>>7;\
			int Y2= buf0[i2+1]>>7;\
			int U= (uvbuf0[i     ] + uvbuf1[i     ])>>8;\
			int V= (uvbuf0[i+2048] + uvbuf1[i+2048])>>8;\

#define YSCALE_YUV_2_RGB1B_C(type) \
			YSCALE_YUV_2_PACKED1B_C\
			type *r, *b, *g;\
			r = c->table_rV[V];\
			g = c->table_gU[U] + c->table_gV[V];\
			b = c->table_bU[U];\

#define YSCALE_YUV_2_ANYRGB_C(func, func2)\
	switch(c->dstFormat)\
	{\
	case IMGFMT_BGR32:\
	case IMGFMT_RGB32:\
		func(uint32_t)\
			((uint32_t*)dest)[i2+0]= r[Y1] + g[Y1] + b[Y1];\
			((uint32_t*)dest)[i2+1]= r[Y2] + g[Y2] + b[Y2];\
		}		\
		break;\
	case IMGFMT_RGB24:\
		func(uint8_t)\
			((uint8_t*)dest)[0]= r[Y1];\
			((uint8_t*)dest)[1]= g[Y1];\
			((uint8_t*)dest)[2]= b[Y1];\
			((uint8_t*)dest)[3]= r[Y2];\
			((uint8_t*)dest)[4]= g[Y2];\
			((uint8_t*)dest)[5]= b[Y2];\
			dest+=6;\
		}\
		break;\
	case IMGFMT_BGR24:\
		func(uint8_t)\
			((uint8_t*)dest)[0]= b[Y1];\
			((uint8_t*)dest)[1]= g[Y1];\
			((uint8_t*)dest)[2]= r[Y1];\
			((uint8_t*)dest)[3]= b[Y2];\
			((uint8_t*)dest)[4]= g[Y2];\
			((uint8_t*)dest)[5]= r[Y2];\
			dest+=6;\
		}\
		break;\
	case IMGFMT_RGB16:\
	case IMGFMT_BGR16:\
		{\
			const int dr1= dither_2x2_8[y&1    ][0];\
			const int dg1= dither_2x2_4[y&1    ][0];\
			const int db1= dither_2x2_8[(y&1)^1][0];\
			const int dr2= dither_2x2_8[y&1    ][1];\
			const int dg2= dither_2x2_4[y&1    ][1];\
			const int db2= dither_2x2_8[(y&1)^1][1];\
			func(uint16_t)\
				((uint16_t*)dest)[i2+0]= r[Y1+dr1] + g[Y1+dg1] + b[Y1+db1];\
				((uint16_t*)dest)[i2+1]= r[Y2+dr2] + g[Y2+dg2] + b[Y2+db2];\
			}\
		}\
		break;\
	case IMGFMT_RGB15:\
	case IMGFMT_BGR15:\
		{\
			const int dr1= dither_2x2_8[y&1    ][0];\
			const int dg1= dither_2x2_8[y&1    ][1];\
			const int db1= dither_2x2_8[(y&1)^1][0];\
			const int dr2= dither_2x2_8[y&1    ][1];\
			const int dg2= dither_2x2_8[y&1    ][0];\
			const int db2= dither_2x2_8[(y&1)^1][1];\
			func(uint16_t)\
				((uint16_t*)dest)[i2+0]= r[Y1+dr1] + g[Y1+dg1] + b[Y1+db1];\
				((uint16_t*)dest)[i2+1]= r[Y2+dr2] + g[Y2+dg2] + b[Y2+db2];\
			}\
		}\
		break;\
	case IMGFMT_RGB8:\
	case IMGFMT_BGR8:\
		{\
			const uint8_t * const d64= dither_8x8_73[y&7];\
			const uint8_t * const d32= dither_8x8_32[y&7];\
			func(uint8_t)\
				((uint8_t*)dest)[i2+0]= r[Y1+d32[(i2+0)&7]] + g[Y1+d32[(i2+0)&7]] + b[Y1+d64[(i2+0)&7]];\
				((uint8_t*)dest)[i2+1]= r[Y2+d32[(i2+1)&7]] + g[Y2+d32[(i2+1)&7]] + b[Y2+d64[(i2+1)&7]];\
			}\
		}\
		break;\
	case IMGFMT_RGB4:\
	case IMGFMT_BGR4:\
		{\
			const uint8_t * const d64= dither_8x8_73 [y&7];\
			const uint8_t * const d128=dither_8x8_220[y&7];\
			func(uint8_t)\
				((uint8_t*)dest)[i]= r[Y1+d128[(i2+0)&7]] + g[Y1+d64[(i2+0)&7]] + b[Y1+d128[(i2+0)&7]]\
				                 + ((r[Y2+d128[(i2+1)&7]] + g[Y2+d64[(i2+1)&7]] + b[Y2+d128[(i2+1)&7]])<<4);\
			}\
		}\
		break;\
	case IMGFMT_RG4B:\
	case IMGFMT_BG4B:\
		{\
			const uint8_t * const d64= dither_8x8_73 [y&7];\
			const uint8_t * const d128=dither_8x8_220[y&7];\
			func(uint8_t)\
				((uint8_t*)dest)[i2+0]= r[Y1+d128[(i2+0)&7]] + g[Y1+d64[(i2+0)&7]] + b[Y1+d128[(i2+0)&7]];\
				((uint8_t*)dest)[i2+1]= r[Y2+d128[(i2+1)&7]] + g[Y2+d64[(i2+1)&7]] + b[Y2+d128[(i2+1)&7]];\
			}\
		}\
		break;\
	case IMGFMT_RGB1:\
	case IMGFMT_BGR1:\
		{\
			const uint8_t * const d128=dither_8x8_220[y&7];\
			uint8_t *g= c->table_gU[128] + c->table_gV[128];\
			for(i=0; i<dstW-7; i+=8){\
				int acc;\
				acc =       g[((buf0[i  ]*yalpha1+buf1[i  ]*yalpha)>>19) + d128[0]];\
				acc+= acc + g[((buf0[i+1]*yalpha1+buf1[i+1]*yalpha)>>19) + d128[1]];\
				acc+= acc + g[((buf0[i+2]*yalpha1+buf1[i+2]*yalpha)>>19) + d128[2]];\
				acc+= acc + g[((buf0[i+3]*yalpha1+buf1[i+3]*yalpha)>>19) + d128[3]];\
				acc+= acc + g[((buf0[i+4]*yalpha1+buf1[i+4]*yalpha)>>19) + d128[4]];\
				acc+= acc + g[((buf0[i+5]*yalpha1+buf1[i+5]*yalpha)>>19) + d128[5]];\
				acc+= acc + g[((buf0[i+6]*yalpha1+buf1[i+6]*yalpha)>>19) + d128[6]];\
				acc+= acc + g[((buf0[i+7]*yalpha1+buf1[i+7]*yalpha)>>19) + d128[7]];\
				((uint8_t*)dest)[0]= acc;\
				dest++;\
			}\
\
/*\
((uint8_t*)dest)-= dstW>>4;\
{\
			int acc=0;\
			int left=0;\
			static int top[1024];\
			static int last_new[1024][1024];\
			static int last_in3[1024][1024];\
			static int drift[1024][1024];\
			int topLeft=0;\
			int shift=0;\
			int count=0;\
			const uint8_t * const d128=dither_8x8_220[y&7];\
			int error_new=0;\
			int error_in3=0;\
			int f=0;\
			\
			for(i=dstW>>1; i<dstW; i++){\
				int in= ((buf0[i  ]*yalpha1+buf1[i  ]*yalpha)>>19);\
				int in2 = (76309 * (in - 16) + 32768) >> 16;\
				int in3 = (in2 < 0) ? 0 : ((in2 > 255) ? 255 : in2);\
				int old= (left*7 + topLeft + top[i]*5 + top[i+1]*3)/20 + in3\
					+ (last_new[y][i] - in3)*f/256;\
				int new= old> 128 ? 255 : 0;\
\
				error_new+= ABS(last_new[y][i] - new);\
				error_in3+= ABS(last_in3[y][i] - in3);\
				f= error_new - error_in3*4;\
				if(f<0) f=0;\
				if(f>256) f=256;\
\
				topLeft= top[i];\
				left= top[i]= old - new;\
				last_new[y][i]= new;\
				last_in3[y][i]= in3;\
\
				acc+= acc + (new&1);\
				if((i&7)==6){\
					((uint8_t*)dest)[0]= acc;\
					((uint8_t*)dest)++;\
				}\
			}\
}\
*/\
		}\
		break;\
	case IMGFMT_YUY2:\
		func2\
			((uint8_t*)dest)[2*i2+0]= Y1;\
			((uint8_t*)dest)[2*i2+1]= U;\
			((uint8_t*)dest)[2*i2+2]= Y2;\
			((uint8_t*)dest)[2*i2+3]= V;\
		}		\
		break;\
	case IMGFMT_UYVY:\
		func2\
			((uint8_t*)dest)[2*i2+0]= U;\
			((uint8_t*)dest)[2*i2+1]= Y1;\
			((uint8_t*)dest)[2*i2+2]= V;\
			((uint8_t*)dest)[2*i2+3]= Y2;\
		}		\
		break;\
	}\


static inline void yuv2packedXinC(SwsContext *c, int16_t *lumFilter, int16_t **lumSrc, int lumFilterSize,
				    int16_t *chrFilter, int16_t **chrSrc, int chrFilterSize,
				    uint8_t *dest, int dstW, int y)
{
	int i;
	switch(c->dstFormat)
	{
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
		YSCALE_YUV_2_RGBX_C(uint32_t)
			((uint32_t*)dest)[i2+0]= r[Y1] + g[Y1] + b[Y1];
			((uint32_t*)dest)[i2+1]= r[Y2] + g[Y2] + b[Y2];
		}
		break;
	case IMGFMT_RGB24:
		YSCALE_YUV_2_RGBX_C(uint8_t)
			((uint8_t*)dest)[0]= r[Y1];
			((uint8_t*)dest)[1]= g[Y1];
			((uint8_t*)dest)[2]= b[Y1];
			((uint8_t*)dest)[3]= r[Y2];
			((uint8_t*)dest)[4]= g[Y2];
			((uint8_t*)dest)[5]= b[Y2];
			dest+=6;
		}
		break;
	case IMGFMT_BGR24:
		YSCALE_YUV_2_RGBX_C(uint8_t)
			((uint8_t*)dest)[0]= b[Y1];
			((uint8_t*)dest)[1]= g[Y1];
			((uint8_t*)dest)[2]= r[Y1];
			((uint8_t*)dest)[3]= b[Y2];
			((uint8_t*)dest)[4]= g[Y2];
			((uint8_t*)dest)[5]= r[Y2];
			dest+=6;
		}
		break;
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
		{
			const int dr1= dither_2x2_8[y&1    ][0];
			const int dg1= dither_2x2_4[y&1    ][0];
			const int db1= dither_2x2_8[(y&1)^1][0];
			const int dr2= dither_2x2_8[y&1    ][1];
			const int dg2= dither_2x2_4[y&1    ][1];
			const int db2= dither_2x2_8[(y&1)^1][1];
			YSCALE_YUV_2_RGBX_C(uint16_t)
				((uint16_t*)dest)[i2+0]= r[Y1+dr1] + g[Y1+dg1] + b[Y1+db1];
				((uint16_t*)dest)[i2+1]= r[Y2+dr2] + g[Y2+dg2] + b[Y2+db2];
			}
		}
		break;
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
		{
			const int dr1= dither_2x2_8[y&1    ][0];
			const int dg1= dither_2x2_8[y&1    ][1];
			const int db1= dither_2x2_8[(y&1)^1][0];
			const int dr2= dither_2x2_8[y&1    ][1];
			const int dg2= dither_2x2_8[y&1    ][0];
			const int db2= dither_2x2_8[(y&1)^1][1];
			YSCALE_YUV_2_RGBX_C(uint16_t)
				((uint16_t*)dest)[i2+0]= r[Y1+dr1] + g[Y1+dg1] + b[Y1+db1];
				((uint16_t*)dest)[i2+1]= r[Y2+dr2] + g[Y2+dg2] + b[Y2+db2];
			}
		}
		break;
	case IMGFMT_RGB8:
	case IMGFMT_BGR8:
		{
			const uint8_t * const d64= dither_8x8_73[y&7];
			const uint8_t * const d32= dither_8x8_32[y&7];
			YSCALE_YUV_2_RGBX_C(uint8_t)
				((uint8_t*)dest)[i2+0]= r[Y1+d32[(i2+0)&7]] + g[Y1+d32[(i2+0)&7]] + b[Y1+d64[(i2+0)&7]];
				((uint8_t*)dest)[i2+1]= r[Y2+d32[(i2+1)&7]] + g[Y2+d32[(i2+1)&7]] + b[Y2+d64[(i2+1)&7]];
			}
		}
		break;
	case IMGFMT_RGB4:
	case IMGFMT_BGR4:
		{
			const uint8_t * const d64= dither_8x8_73 [y&7];
			const uint8_t * const d128=dither_8x8_220[y&7];
			YSCALE_YUV_2_RGBX_C(uint8_t)
				((uint8_t*)dest)[i]= r[Y1+d128[(i2+0)&7]] + g[Y1+d64[(i2+0)&7]] + b[Y1+d128[(i2+0)&7]]
				                  +((r[Y2+d128[(i2+1)&7]] + g[Y2+d64[(i2+1)&7]] + b[Y2+d128[(i2+1)&7]])<<4);
			}
		}
		break;
	case IMGFMT_RG4B:
	case IMGFMT_BG4B:
		{
			const uint8_t * const d64= dither_8x8_73 [y&7];
			const uint8_t * const d128=dither_8x8_220[y&7];
			YSCALE_YUV_2_RGBX_C(uint8_t)
				((uint8_t*)dest)[i2+0]= r[Y1+d128[(i2+0)&7]] + g[Y1+d64[(i2+0)&7]] + b[Y1+d128[(i2+0)&7]];
				((uint8_t*)dest)[i2+1]= r[Y2+d128[(i2+1)&7]] + g[Y2+d64[(i2+1)&7]] + b[Y2+d128[(i2+1)&7]];
			}
		}
		break;
	case IMGFMT_RGB1:
	case IMGFMT_BGR1:
		{
			const uint8_t * const d128=dither_8x8_220[y&7];
			uint8_t *g= c->table_gU[128] + c->table_gV[128];
			int acc=0;
			for(i=0; i<dstW-1; i+=2){
				int j;
				int Y1=1<<18;
				int Y2=1<<18;

				for(j=0; j<lumFilterSize; j++)
				{
					Y1 += lumSrc[j][i] * lumFilter[j];
					Y2 += lumSrc[j][i+1] * lumFilter[j];
				}
				Y1>>=19;
				Y2>>=19;
				if((Y1|Y2)&256)
				{
					if(Y1>255)   Y1=255;
					else if(Y1<0)Y1=0;
					if(Y2>255)   Y2=255;
					else if(Y2<0)Y2=0;
				}
				acc+= acc + g[Y1+d128[(i+0)&7]];
				acc+= acc + g[Y2+d128[(i+1)&7]];
				if((i&7)==6){
					((uint8_t*)dest)[0]= acc;
					dest++;
				}
			}
		}
		break;
	case IMGFMT_YUY2:
		YSCALE_YUV_2_PACKEDX_C(void)
			((uint8_t*)dest)[2*i2+0]= Y1;
			((uint8_t*)dest)[2*i2+1]= U;
			((uint8_t*)dest)[2*i2+2]= Y2;
			((uint8_t*)dest)[2*i2+3]= V;
		}
                break;
	case IMGFMT_UYVY:
		YSCALE_YUV_2_PACKEDX_C(void)
			((uint8_t*)dest)[2*i2+0]= U;
			((uint8_t*)dest)[2*i2+1]= Y1;
			((uint8_t*)dest)[2*i2+2]= V;
			((uint8_t*)dest)[2*i2+3]= Y2;
		}
                break;
	}
}


//Note: we have C, X86, MMX, MMX2, 3DNOW version therse no 3DNOW+MMX2 one
//Plain C versions
#if !defined (HAVE_MMX) || defined (RUNTIME_CPUDETECT)
#define COMPILE_C
#endif

#ifdef ARCH_POWERPC
#ifdef HAVE_ALTIVEC
#define COMPILE_ALTIVEC
#endif //HAVE_ALTIVEC
#endif //ARCH_POWERPC

#ifdef ARCH_X86

#if (defined (HAVE_MMX) && !defined (HAVE_3DNOW) && !defined (HAVE_MMX2)) || defined (RUNTIME_CPUDETECT)
#define COMPILE_MMX
#endif

#if defined (HAVE_MMX2) || defined (RUNTIME_CPUDETECT)
#define COMPILE_MMX2
#endif

#if (defined (HAVE_3DNOW) && !defined (HAVE_MMX2)) || defined (RUNTIME_CPUDETECT)
#define COMPILE_3DNOW
#endif
#endif //ARCH_X86

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW

#ifdef COMPILE_C
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_ALTIVEC
#define RENAME(a) a ## _C
#include "swscale_template.c"
#endif

#ifdef ARCH_POWERPC
#ifdef COMPILE_ALTIVEC
#undef RENAME
#define HAVE_ALTIVEC
#define RENAME(a) a ## _altivec
#include "swscale_template.c"
#endif
#endif //ARCH_POWERPC

#ifdef ARCH_X86

//X86 versions
/*
#undef RENAME
#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#define ARCH_X86
#define RENAME(a) a ## _X86
#include "swscale_template.c"
*/
//MMX versions
#ifdef COMPILE_MMX
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX
#include "swscale_template.c"
#endif

//MMX2 versions
#ifdef COMPILE_MMX2
#undef RENAME
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX2
#include "swscale_template.c"
#endif

//3DNOW versions
#ifdef COMPILE_3DNOW
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#define HAVE_3DNOW
#define RENAME(a) a ## _3DNow
#include "swscale_template.c"
#endif

#endif //ARCH_X86

// minor note: the HAVE_xyz is messed up after that line so don't use it

static double getSplineCoeff(double a, double b, double c, double d, double dist)
{
//	printf("%f %f %f %f %f\n", a,b,c,d,dist);
	if(dist<=1.0) 	return ((d*dist + c)*dist + b)*dist +a;
	else		return getSplineCoeff(	0.0, 
						 b+ 2.0*c + 3.0*d,
						        c + 3.0*d,
						-b- 3.0*c - 6.0*d,
						dist-1.0);
}

static inline void initFilter(int16_t **outFilter, int16_t **filterPos, int *outFilterSize, int xInc,
			      int srcW, int dstW, int filterAlign, int one, int flags,
			      SwsVector *srcFilter, SwsVector *dstFilter)
{
	int i;
	int filterSize;
	int filter2Size;
	int minFilterSize;
	double *filter=NULL;
	double *filter2=NULL;
#ifdef ARCH_X86
	if(flags & SWS_CPU_CAPS_MMX)
		asm volatile("emms\n\t"::: "memory"); //FIXME this shouldnt be required but it IS (even for non mmx versions)
#endif

	// Note the +1 is for the MMXscaler which reads over the end
	*filterPos = (int16_t*)memalign(8, (dstW+1)*sizeof(int16_t));

	if(ABS(xInc - 0x10000) <10) // unscaled
	{
		int i;
		filterSize= 1;
		filter= (double*)memalign(8, dstW*sizeof(double)*filterSize);
		for(i=0; i<dstW*filterSize; i++) filter[i]=0;

		for(i=0; i<dstW; i++)
		{
			filter[i*filterSize]=1;
			(*filterPos)[i]=i;
		}

	}
	else if(flags&SWS_POINT) // lame looking point sampling mode
	{
		int i;
		int xDstInSrc;
		filterSize= 1;
		filter= (double*)memalign(8, dstW*sizeof(double)*filterSize);
		
		xDstInSrc= xInc/2 - 0x8000;
		for(i=0; i<dstW; i++)
		{
			int xx= (xDstInSrc - ((filterSize-1)<<15) + (1<<15))>>16;

			(*filterPos)[i]= xx;
			filter[i]= 1.0;
			xDstInSrc+= xInc;
		}
	}
	else if((xInc <= (1<<16) && (flags&SWS_AREA)) || (flags&SWS_FAST_BILINEAR)) // bilinear upscale
	{
		int i;
		int xDstInSrc;
		if     (flags&SWS_BICUBIC) filterSize= 4;
		else if(flags&SWS_X      ) filterSize= 4;
		else			   filterSize= 2; // SWS_BILINEAR / SWS_AREA 
		filter= (double*)memalign(8, dstW*sizeof(double)*filterSize);

		xDstInSrc= xInc/2 - 0x8000;
		for(i=0; i<dstW; i++)
		{
			int xx= (xDstInSrc - ((filterSize-1)<<15) + (1<<15))>>16;
			int j;

			(*filterPos)[i]= xx;
				//Bilinear upscale / linear interpolate / Area averaging
				for(j=0; j<filterSize; j++)
				{
					double d= ABS((xx<<16) - xDstInSrc)/(double)(1<<16);
					double coeff= 1.0 - d;
					if(coeff<0) coeff=0;
					filter[i*filterSize + j]= coeff;
					xx++;
				}
			xDstInSrc+= xInc;
		}
	}
	else
	{
		double xDstInSrc;
		double sizeFactor, filterSizeInSrc;
		const double xInc1= (double)xInc / (double)(1<<16);
		int param= (flags&SWS_PARAM_MASK)>>SWS_PARAM_SHIFT;

		if     (flags&SWS_BICUBIC)	sizeFactor= 4.0;
		else if(flags&SWS_X)		sizeFactor= 8.0;
		else if(flags&SWS_AREA)		sizeFactor= 1.0; //downscale only, for upscale it is bilinear
		else if(flags&SWS_GAUSS)	sizeFactor= 8.0;   // infinite ;)
		else if(flags&SWS_LANCZOS)	sizeFactor= param ? 2.0*param : 6.0;
		else if(flags&SWS_SINC)		sizeFactor= 20.0; // infinite ;)
		else if(flags&SWS_SPLINE)	sizeFactor= 20.0;  // infinite ;)
		else if(flags&SWS_BILINEAR)	sizeFactor= 2.0;
		else {
			sizeFactor= 0.0; //GCC warning killer
			ASSERT(0)
		}
		
		if(xInc1 <= 1.0)	filterSizeInSrc= sizeFactor; // upscale
		else			filterSizeInSrc= sizeFactor*srcW / (double)dstW;

		filterSize= (int)ceil(1 + filterSizeInSrc); // will be reduced later if possible
		if(filterSize > srcW-2) filterSize=srcW-2;

		filter= (double*)memalign(16, dstW*sizeof(double)*filterSize);

		xDstInSrc= xInc1 / 2.0 - 0.5;
		for(i=0; i<dstW; i++)
		{
			int xx= (int)(xDstInSrc - (filterSize-1)*0.5 + 0.5);
			int j;
			(*filterPos)[i]= xx;
			for(j=0; j<filterSize; j++)
			{
				double d= ABS(xx - xDstInSrc)/filterSizeInSrc*sizeFactor;
				double coeff;
				if(flags & SWS_BICUBIC)
				{
					double A= param ? -param*0.01 : -0.60;
					
					// Equation is from VirtualDub
					if(d<1.0)
						coeff = (1.0 - (A+3.0)*d*d + (A+2.0)*d*d*d);
					else if(d<2.0)
						coeff = (-4.0*A + 8.0*A*d - 5.0*A*d*d + A*d*d*d);
					else
						coeff=0.0;
				}
/*				else if(flags & SWS_X)
				{
					double p= param ? param*0.01 : 0.3;
					coeff = d ? sin(d*PI)/(d*PI) : 1.0;
					coeff*= pow(2.0, - p*d*d);
				}*/
				else if(flags & SWS_X)
				{
					double A= param ? param*0.1 : 1.0;
					
					if(d<1.0)
						coeff = cos(d*PI);
					else
						coeff=-1.0;
					if(coeff<0.0) 	coeff= -pow(-coeff, A);
					else		coeff=  pow( coeff, A);
					coeff= coeff*0.5 + 0.5;
				}
				else if(flags & SWS_AREA)
				{
					double srcPixelSize= 1.0/xInc1;
					if(d + srcPixelSize/2 < 0.5) coeff= 1.0;
					else if(d - srcPixelSize/2 < 0.5) coeff= (0.5-d)/srcPixelSize + 0.5;
					else coeff=0.0;
				}
				else if(flags & SWS_GAUSS)
				{
					double p= param ? param*0.1 : 3.0;
					coeff = pow(2.0, - p*d*d);
				}
				else if(flags & SWS_SINC)
				{
					coeff = d ? sin(d*PI)/(d*PI) : 1.0;
				}
				else if(flags & SWS_LANCZOS)
				{
					double p= param ? param : 3.0; 
					coeff = d ? sin(d*PI)*sin(d*PI/p)/(d*d*PI*PI/p) : 1.0;
					if(d>p) coeff=0;
				}
				else if(flags & SWS_BILINEAR)
				{
					coeff= 1.0 - d;
					if(coeff<0) coeff=0;
				}
				else if(flags & SWS_SPLINE)
				{
					double p=-2.196152422706632;
					coeff = getSplineCoeff(1.0, 0.0, p, -p-1.0, d);
				}
				else {
					coeff= 0.0; //GCC warning killer
					ASSERT(0)
				}

				filter[i*filterSize + j]= coeff;
				xx++;
			}
			xDstInSrc+= xInc1;
		}
	}

	/* apply src & dst Filter to filter -> filter2
	   free(filter);
	*/
	ASSERT(filterSize>0)
	filter2Size= filterSize;
	if(srcFilter) filter2Size+= srcFilter->length - 1;
	if(dstFilter) filter2Size+= dstFilter->length - 1;
	ASSERT(filter2Size>0)
	filter2= (double*)memalign(8, filter2Size*dstW*sizeof(double));

	for(i=0; i<dstW; i++)
	{
		int j;
		SwsVector scaleFilter;
		SwsVector *outVec;

		scaleFilter.coeff= filter + i*filterSize;
		scaleFilter.length= filterSize;

		if(srcFilter) outVec= sws_getConvVec(srcFilter, &scaleFilter);
		else	      outVec= &scaleFilter;

		ASSERT(outVec->length == filter2Size)
		//FIXME dstFilter

		for(j=0; j<outVec->length; j++)
		{
			filter2[i*filter2Size + j]= outVec->coeff[j];
		}

		(*filterPos)[i]+= (filterSize-1)/2 - (filter2Size-1)/2;

		if(outVec != &scaleFilter) sws_freeVec(outVec);
	}
	free(filter); filter=NULL;

	/* try to reduce the filter-size (step1 find size and shift left) */
	// Assume its near normalized (*0.5 or *2.0 is ok but * 0.001 is not)
	minFilterSize= 0;
	for(i=dstW-1; i>=0; i--)
	{
		int min= filter2Size;
		int j;
		double cutOff=0.0;

		/* get rid off near zero elements on the left by shifting left */
		for(j=0; j<filter2Size; j++)
		{
			int k;
			cutOff += ABS(filter2[i*filter2Size]);

			if(cutOff > SWS_MAX_REDUCE_CUTOFF) break;

			/* preserve Monotonicity because the core can't handle the filter otherwise */
			if(i<dstW-1 && (*filterPos)[i] >= (*filterPos)[i+1]) break;

			// Move filter coeffs left
			for(k=1; k<filter2Size; k++)
				filter2[i*filter2Size + k - 1]= filter2[i*filter2Size + k];
			filter2[i*filter2Size + k - 1]= 0.0;
			(*filterPos)[i]++;
		}

		cutOff=0.0;
		/* count near zeros on the right */
		for(j=filter2Size-1; j>0; j--)
		{
			cutOff += ABS(filter2[i*filter2Size + j]);

			if(cutOff > SWS_MAX_REDUCE_CUTOFF) break;
			min--;
		}

		if(min>minFilterSize) minFilterSize= min;
	}

        if (flags & SWS_CPU_CAPS_ALTIVEC) {
          // we can handle the special case 4,
          // so we don't want to go to the full 8
          if (minFilterSize < 5)
            filterAlign = 4;

          // we really don't want to waste our time
          // doing useless computation, so fall-back on
          // the scalar C code for very small filter.
          // vectorizing is worth it only if you have
          // decent-sized vector.
          if (minFilterSize < 3)
            filterAlign = 1;
        }

	ASSERT(minFilterSize > 0)
	filterSize= (minFilterSize +(filterAlign-1)) & (~(filterAlign-1));
	ASSERT(filterSize > 0)
	filter= (double*)memalign(8, filterSize*dstW*sizeof(double));
	*outFilterSize= filterSize;

	if(flags&SWS_PRINT_INFO)
		MSG_INFO("SwScaler: reducing / aligning filtersize %d -> %d\n", filter2Size, filterSize);
	/* try to reduce the filter-size (step2 reduce it) */
	for(i=0; i<dstW; i++)
	{
		int j;

		for(j=0; j<filterSize; j++)
		{
			if(j>=filter2Size) filter[i*filterSize + j]= 0.0;
			else		   filter[i*filterSize + j]= filter2[i*filter2Size + j];
		}
	}
	free(filter2); filter2=NULL;
	

	//FIXME try to align filterpos if possible

	//fix borders
	for(i=0; i<dstW; i++)
	{
		int j;
		if((*filterPos)[i] < 0)
		{
			// Move filter coeffs left to compensate for filterPos
			for(j=1; j<filterSize; j++)
			{
				int left= MAX(j + (*filterPos)[i], 0);
				filter[i*filterSize + left] += filter[i*filterSize + j];
				filter[i*filterSize + j]=0;
			}
			(*filterPos)[i]= 0;
		}

		if((*filterPos)[i] + filterSize > srcW)
		{
			int shift= (*filterPos)[i] + filterSize - srcW;
			// Move filter coeffs right to compensate for filterPos
			for(j=filterSize-2; j>=0; j--)
			{
				int right= MIN(j + shift, filterSize-1);
				filter[i*filterSize +right] += filter[i*filterSize +j];
				filter[i*filterSize +j]=0;
			}
			(*filterPos)[i]= srcW - filterSize;
		}
	}

	// Note the +1 is for the MMXscaler which reads over the end
	*outFilter= (int16_t*)memalign(8, *outFilterSize*(dstW+1)*sizeof(int16_t));
	memset(*outFilter, 0, *outFilterSize*(dstW+1)*sizeof(int16_t));

	/* Normalize & Store in outFilter */
	for(i=0; i<dstW; i++)
	{
		int j;
		double error=0;
		double sum=0;
		double scale= one;

		for(j=0; j<filterSize; j++)
		{
			sum+= filter[i*filterSize + j];
		}
		scale/= sum;
		for(j=0; j<*outFilterSize; j++)
		{
			double v= filter[i*filterSize + j]*scale + error;
			int intV= floor(v + 0.5);
			(*outFilter)[i*(*outFilterSize) + j]= intV;
			error = v - intV;
		}
	}
	
	(*filterPos)[dstW]= (*filterPos)[dstW-1]; // the MMX scaler will read over the end
	for(i=0; i<*outFilterSize; i++)
	{
		int j= dstW*(*outFilterSize);
		(*outFilter)[j + i]= (*outFilter)[j + i - (*outFilterSize)];
	}

	free(filter);
}

#ifdef ARCH_X86
static void initMMX2HScaler(int dstW, int xInc, uint8_t *funnyCode, int16_t *filter, int32_t *filterPos, int numSplits)
{
	uint8_t *fragmentA;
	int imm8OfPShufW1A;
	int imm8OfPShufW2A;
	int fragmentLengthA;
	uint8_t *fragmentB;
	int imm8OfPShufW1B;
	int imm8OfPShufW2B;
	int fragmentLengthB;
	int fragmentPos;

	int xpos, i;

	// create an optimized horizontal scaling routine

	//code fragment

	asm volatile(
		"jmp 9f				\n\t"
	// Begin
		"0:				\n\t"
		"movq (%%edx, %%eax), %%mm3	\n\t" 
		"movd (%%ecx, %%esi), %%mm0	\n\t" 
		"movd 1(%%ecx, %%esi), %%mm1	\n\t"
		"punpcklbw %%mm7, %%mm1		\n\t"
		"punpcklbw %%mm7, %%mm0		\n\t"
		"pshufw $0xFF, %%mm1, %%mm1	\n\t"
		"1:				\n\t"
		"pshufw $0xFF, %%mm0, %%mm0	\n\t"
		"2:				\n\t"
		"psubw %%mm1, %%mm0		\n\t"
		"movl 8(%%ebx, %%eax), %%esi	\n\t"
		"pmullw %%mm3, %%mm0		\n\t"
		"psllw $7, %%mm1		\n\t"
		"paddw %%mm1, %%mm0		\n\t"

		"movq %%mm0, (%%edi, %%eax)	\n\t"

		"addl $8, %%eax			\n\t"
	// End
		"9:				\n\t"
//		"int $3\n\t"
		"leal 0b, %0			\n\t"
		"leal 1b, %1			\n\t"
		"leal 2b, %2			\n\t"
		"decl %1			\n\t"
		"decl %2			\n\t"
		"subl %0, %1			\n\t"
		"subl %0, %2			\n\t"
		"leal 9b, %3			\n\t"
		"subl %0, %3			\n\t"


		:"=r" (fragmentA), "=r" (imm8OfPShufW1A), "=r" (imm8OfPShufW2A),
		"=r" (fragmentLengthA)
	);

	asm volatile(
		"jmp 9f				\n\t"
	// Begin
		"0:				\n\t"
		"movq (%%edx, %%eax), %%mm3	\n\t" 
		"movd (%%ecx, %%esi), %%mm0	\n\t" 
		"punpcklbw %%mm7, %%mm0		\n\t"
		"pshufw $0xFF, %%mm0, %%mm1	\n\t"
		"1:				\n\t"
		"pshufw $0xFF, %%mm0, %%mm0	\n\t"
		"2:				\n\t"
		"psubw %%mm1, %%mm0		\n\t"
		"movl 8(%%ebx, %%eax), %%esi	\n\t"
		"pmullw %%mm3, %%mm0		\n\t"
		"psllw $7, %%mm1		\n\t"
		"paddw %%mm1, %%mm0		\n\t"

		"movq %%mm0, (%%edi, %%eax)	\n\t"

		"addl $8, %%eax			\n\t"
	// End
		"9:				\n\t"
//		"int $3\n\t"
		"leal 0b, %0			\n\t"
		"leal 1b, %1			\n\t"
		"leal 2b, %2			\n\t"
		"decl %1			\n\t"
		"decl %2			\n\t"
		"subl %0, %1			\n\t"
		"subl %0, %2			\n\t"
		"leal 9b, %3			\n\t"
		"subl %0, %3			\n\t"


		:"=r" (fragmentB), "=r" (imm8OfPShufW1B), "=r" (imm8OfPShufW2B),
		"=r" (fragmentLengthB)
	);

	xpos= 0; //lumXInc/2 - 0x8000; // difference between pixel centers
	fragmentPos=0;
	
	for(i=0; i<dstW/numSplits; i++)
	{
		int xx=xpos>>16;

		if((i&3) == 0)
		{
			int a=0;
			int b=((xpos+xInc)>>16) - xx;
			int c=((xpos+xInc*2)>>16) - xx;
			int d=((xpos+xInc*3)>>16) - xx;

			filter[i  ] = (( xpos         & 0xFFFF) ^ 0xFFFF)>>9;
			filter[i+1] = (((xpos+xInc  ) & 0xFFFF) ^ 0xFFFF)>>9;
			filter[i+2] = (((xpos+xInc*2) & 0xFFFF) ^ 0xFFFF)>>9;
			filter[i+3] = (((xpos+xInc*3) & 0xFFFF) ^ 0xFFFF)>>9;
			filterPos[i/2]= xx;

			if(d+1<4)
			{
				int maxShift= 3-(d+1);
				int shift=0;

				memcpy(funnyCode + fragmentPos, fragmentB, fragmentLengthB);

				funnyCode[fragmentPos + imm8OfPShufW1B]=
					(a+1) | ((b+1)<<2) | ((c+1)<<4) | ((d+1)<<6);
				funnyCode[fragmentPos + imm8OfPShufW2B]=
					a | (b<<2) | (c<<4) | (d<<6);

				if(i+3>=dstW) shift=maxShift; //avoid overread
				else if((filterPos[i/2]&3) <= maxShift) shift=filterPos[i/2]&3; //Align

				if(shift && i>=shift)
				{
					funnyCode[fragmentPos + imm8OfPShufW1B]+= 0x55*shift;
					funnyCode[fragmentPos + imm8OfPShufW2B]+= 0x55*shift;
					filterPos[i/2]-=shift;
				}

				fragmentPos+= fragmentLengthB;
			}
			else
			{
				int maxShift= 3-d;
				int shift=0;

				memcpy(funnyCode + fragmentPos, fragmentA, fragmentLengthA);

				funnyCode[fragmentPos + imm8OfPShufW1A]=
				funnyCode[fragmentPos + imm8OfPShufW2A]=
					a | (b<<2) | (c<<4) | (d<<6);

				if(i+4>=dstW) shift=maxShift; //avoid overread
				else if((filterPos[i/2]&3) <= maxShift) shift=filterPos[i/2]&3; //partial align

				if(shift && i>=shift)
				{
					funnyCode[fragmentPos + imm8OfPShufW1A]+= 0x55*shift;
					funnyCode[fragmentPos + imm8OfPShufW2A]+= 0x55*shift;
					filterPos[i/2]-=shift;
				}

				fragmentPos+= fragmentLengthA;
			}

			funnyCode[fragmentPos]= RET;
		}
		xpos+=xInc;
	}
	filterPos[i/2]= xpos>>16; // needed to jump to the next part
}
#endif // ARCH_X86

static void globalInit(){
    // generating tables:
    int i;
    for(i=0; i<768; i++){
	int c= MIN(MAX(i-256, 0), 255);
	clip_table[i]=c;
    }
}

static SwsFunc getSwsFunc(int flags){
    
#ifdef RUNTIME_CPUDETECT
#ifdef ARCH_X86
	// ordered per speed fasterst first
	if(flags & SWS_CPU_CAPS_MMX2)
		return swScale_MMX2;
	else if(flags & SWS_CPU_CAPS_3DNOW)
		return swScale_3DNow;
	else if(flags & SWS_CPU_CAPS_MMX)
		return swScale_MMX;
	else
		return swScale_C;

#else
#ifdef ARCH_POWERPC
	if(flags & SWS_CPU_CAPS_ALTIVEC)
	  return swScale_altivec;
	else
	  return swScale_C;
#endif
	return swScale_C;
#endif
#else //RUNTIME_CPUDETECT
#ifdef HAVE_MMX2
	return swScale_MMX2;
#elif defined (HAVE_3DNOW)
	return swScale_3DNow;
#elif defined (HAVE_MMX)
	return swScale_MMX;
#elif defined (HAVE_ALTIVEC)
	return swScale_altivec;
#else
	return swScale_C;
#endif
#endif //!RUNTIME_CPUDETECT
}

static int PlanarToNV12Wrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dstParam[], int dstStride[]){
	uint8_t *dst=dstParam[0] + dstStride[0]*srcSliceY;
	/* Copy Y plane */
	if(dstStride[0]==srcStride[0])
		memcpy(dst, src[0], srcSliceH*dstStride[0]);
	else
	{
		int i;
		uint8_t *srcPtr= src[0];
		uint8_t *dstPtr= dst;
		for(i=0; i<srcSliceH; i++)
		{
			memcpy(dstPtr, srcPtr, srcStride[0]);
			srcPtr+= srcStride[0];
			dstPtr+= dstStride[0];
		}
	}
	dst = dstParam[1] + dstStride[1]*srcSliceY;
	interleaveBytes( src[1],src[2],dst,c->srcW,srcSliceH,srcStride[1],srcStride[2],dstStride[0] );

	return srcSliceH;
}

static int PlanarToYuy2Wrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dstParam[], int dstStride[]){
	uint8_t *dst=dstParam[0] + dstStride[0]*srcSliceY;

	yv12toyuy2( src[0],src[1],src[2],dst,c->srcW,srcSliceH,srcStride[0],srcStride[1],dstStride[0] );

	return srcSliceH;
}

static int PlanarToUyvyWrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dstParam[], int dstStride[]){
	uint8_t *dst=dstParam[0] + dstStride[0]*srcSliceY;

	yv12touyvy( src[0],src[1],src[2],dst,c->srcW,srcSliceH,srcStride[0],srcStride[1],dstStride[0] );

	return srcSliceH;
}

/* {RGB,BGR}{15,16,24,32} -> {RGB,BGR}{15,16,24,32} */
static int rgb2rgbWrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
			   int srcSliceH, uint8_t* dst[], int dstStride[]){
	const int srcFormat= c->srcFormat;
	const int dstFormat= c->dstFormat;
	const int srcBpp= ((srcFormat&0xFF) + 7)>>3;
	const int dstBpp= ((dstFormat&0xFF) + 7)>>3;
	const int srcId= (srcFormat&0xFF)>>2; // 1:0, 4:1, 8:2, 15:3, 16:4, 24:6, 32:8 
	const int dstId= (dstFormat&0xFF)>>2;
	void (*conv)(const uint8_t *src, uint8_t *dst, unsigned src_size)=NULL;

	/* BGR -> BGR */
	if(   (isBGR(srcFormat) && isBGR(dstFormat))
	   || (isRGB(srcFormat) && isRGB(dstFormat))){
		switch(srcId | (dstId<<4)){
		case 0x34: conv= rgb16to15; break;
		case 0x36: conv= rgb24to15; break;
		case 0x38: conv= rgb32to15; break;
		case 0x43: conv= rgb15to16; break;
		case 0x46: conv= rgb24to16; break;
		case 0x48: conv= rgb32to16; break;
		case 0x63: conv= rgb15to24; break;
		case 0x64: conv= rgb16to24; break;
		case 0x68: conv= rgb32to24; break;
		case 0x83: conv= rgb15to32; break;
		case 0x84: conv= rgb16to32; break;
		case 0x86: conv= rgb24to32; break;
		default: MSG_ERR("swScaler: internal error %s -> %s converter\n", 
				 vo_format_name(srcFormat), vo_format_name(dstFormat)); break;
		}
	}else if(   (isBGR(srcFormat) && isRGB(dstFormat))
		 || (isRGB(srcFormat) && isBGR(dstFormat))){
		switch(srcId | (dstId<<4)){
		case 0x33: conv= rgb15tobgr15; break;
		case 0x34: conv= rgb16tobgr15; break;
		case 0x36: conv= rgb24tobgr15; break;
		case 0x38: conv= rgb32tobgr15; break;
		case 0x43: conv= rgb15tobgr16; break;
		case 0x44: conv= rgb16tobgr16; break;
		case 0x46: conv= rgb24tobgr16; break;
		case 0x48: conv= rgb32tobgr16; break;
		case 0x63: conv= rgb15tobgr24; break;
		case 0x64: conv= rgb16tobgr24; break;
		case 0x66: conv= rgb24tobgr24; break;
		case 0x68: conv= rgb32tobgr24; break;
		case 0x83: conv= rgb15tobgr32; break;
		case 0x84: conv= rgb16tobgr32; break;
		case 0x86: conv= rgb24tobgr32; break;
		case 0x88: conv= rgb32tobgr32; break;
		default: MSG_ERR("swScaler: internal error %s -> %s converter\n", 
				 vo_format_name(srcFormat), vo_format_name(dstFormat)); break;
		}
	}else{
		MSG_ERR("swScaler: internal error %s -> %s converter\n", 
			 vo_format_name(srcFormat), vo_format_name(dstFormat));
	}

	if(dstStride[0]*srcBpp == srcStride[0]*dstBpp)
		conv(src[0], dst[0] + dstStride[0]*srcSliceY, srcSliceH*srcStride[0]);
	else
	{
		int i;
		uint8_t *srcPtr= src[0];
		uint8_t *dstPtr= dst[0] + dstStride[0]*srcSliceY;

		for(i=0; i<srcSliceH; i++)
		{
			conv(srcPtr, dstPtr, c->srcW*srcBpp);
			srcPtr+= srcStride[0];
			dstPtr+= dstStride[0];
		}
	}     
	return srcSliceH;
}

static int bgr24toyv12Wrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dst[], int dstStride[]){

	rgb24toyv12(
		src[0], 
		dst[0]+ srcSliceY    *dstStride[0], 
		dst[1]+(srcSliceY>>1)*dstStride[1], 
		dst[2]+(srcSliceY>>1)*dstStride[2],
		c->srcW, srcSliceH, 
		dstStride[0], dstStride[1], srcStride[0]);
	return srcSliceH;
}

static int yvu9toyv12Wrapper(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dst[], int dstStride[]){
	int i;

	/* copy Y */
	if(srcStride[0]==dstStride[0]) 
		memcpy(dst[0]+ srcSliceY*dstStride[0], src[0], srcStride[0]*srcSliceH);
	else{
		uint8_t *srcPtr= src[0];
		uint8_t *dstPtr= dst[0] + dstStride[0]*srcSliceY;

		for(i=0; i<srcSliceH; i++)
		{
			memcpy(dstPtr, srcPtr, c->srcW);
			srcPtr+= srcStride[0];
			dstPtr+= dstStride[0];
		}
	}

	if(c->dstFormat==IMGFMT_YV12){
		planar2x(src[1], dst[1], c->chrSrcW, c->chrSrcH, srcStride[1], dstStride[1]);
		planar2x(src[2], dst[2], c->chrSrcW, c->chrSrcH, srcStride[2], dstStride[2]);
	}else{
		planar2x(src[1], dst[2], c->chrSrcW, c->chrSrcH, srcStride[1], dstStride[2]);
		planar2x(src[2], dst[1], c->chrSrcW, c->chrSrcH, srcStride[2], dstStride[1]);
	}
	return srcSliceH;
}

/**
 * bring pointers in YUV order instead of YVU
 */
static inline void sws_orderYUV(int format, uint8_t * sortedP[], int sortedStride[], uint8_t * p[], int stride[]){
	if(format == IMGFMT_YV12 || format == IMGFMT_YVU9
           || format == IMGFMT_444P || format == IMGFMT_422P || format == IMGFMT_411P){
		sortedP[0]= p[0];
		sortedP[1]= p[2];
		sortedP[2]= p[1];
		sortedStride[0]= stride[0];
		sortedStride[1]= stride[2];
		sortedStride[2]= stride[1];
	}
	else if(isPacked(format) || isGray(format) || format == IMGFMT_Y8)
	{
		sortedP[0]= p[0];
		sortedP[1]= 
		sortedP[2]= NULL;
		sortedStride[0]= stride[0];
		sortedStride[1]= 
		sortedStride[2]= 0;
	}
	else if(format == IMGFMT_I420 || format == IMGFMT_IYUV)
	{
		sortedP[0]= p[0];
		sortedP[1]= p[1];
		sortedP[2]= p[2];
		sortedStride[0]= stride[0];
		sortedStride[1]= stride[1];
		sortedStride[2]= stride[2];
	}else{
		MSG_ERR("internal error in orderYUV\n");
	}
}

/* unscaled copy like stuff (assumes nearly identical formats) */
static int simpleCopy(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
             int srcSliceH, uint8_t* dst[], int dstStride[]){

	if(isPacked(c->srcFormat))
	{
		if(dstStride[0]==srcStride[0])
			memcpy(dst[0] + dstStride[0]*srcSliceY, src[0], srcSliceH*dstStride[0]);
		else
		{
			int i;
			uint8_t *srcPtr= src[0];
			uint8_t *dstPtr= dst[0] + dstStride[0]*srcSliceY;
			int length=0;

			/* universal length finder */
			while(length+c->srcW <= ABS(dstStride[0]) 
			   && length+c->srcW <= ABS(srcStride[0])) length+= c->srcW;
			ASSERT(length!=0);

			for(i=0; i<srcSliceH; i++)
			{
				memcpy(dstPtr, srcPtr, length);
				srcPtr+= srcStride[0];
				dstPtr+= dstStride[0];
			}
		}
	}
	else 
	{ /* Planar YUV or gray */
		int plane;
		for(plane=0; plane<3; plane++)
		{
			int length= plane==0 ? c->srcW  : -((-c->srcW  )>>c->chrDstHSubSample);
			int y=      plane==0 ? srcSliceY: -((-srcSliceY)>>c->chrDstVSubSample);
			int height= plane==0 ? srcSliceH: -((-srcSliceH)>>c->chrDstVSubSample);

			if((isGray(c->srcFormat) || isGray(c->dstFormat)) && plane>0)
			{
				if(!isGray(c->dstFormat))
					memset(dst[plane], 128, dstStride[plane]*height);
			}
			else
			{
				if(dstStride[plane]==srcStride[plane])
					memcpy(dst[plane] + dstStride[plane]*y, src[plane], height*dstStride[plane]);
				else
				{
					int i;
					uint8_t *srcPtr= src[plane];
					uint8_t *dstPtr= dst[plane] + dstStride[plane]*y;
					for(i=0; i<height; i++)
					{
						memcpy(dstPtr, srcPtr, length);
						srcPtr+= srcStride[plane];
						dstPtr+= dstStride[plane];
					}
				}
			}
		}
	}
	return srcSliceH;
}

static int remove_dup_fourcc(int fourcc)
{
	switch(fourcc)
	{
	    case IMGFMT_I420:
	    case IMGFMT_IYUV: return IMGFMT_YV12;
	    case IMGFMT_Y8  : return IMGFMT_Y800;
	    case IMGFMT_IF09: return IMGFMT_YVU9;
	    default: return fourcc;
	}
}

static void getSubSampleFactors(int *h, int *v, int format){
	switch(format){
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
		*h=1;
		*v=0;
		break;
	case IMGFMT_YV12:
	case IMGFMT_Y800: //FIXME remove after different subsamplings are fully implemented
		*h=1;
		*v=1;
		break;
	case IMGFMT_YVU9:
		*h=2;
		*v=2;
		break;
	case IMGFMT_444P:
		*h=0;
		*v=0;
		break;
	case IMGFMT_422P:
		*h=1;
		*v=0;
		break;
	case IMGFMT_411P:
		*h=2;
		*v=0;
		break;
	default:
		*h=0;
		*v=0;
		break;
	}
}

static uint16_t roundToInt16(int64_t f){
	int r= (f + (1<<15))>>16;
	     if(r<-0x7FFF) return 0x8000;
	else if(r> 0x7FFF) return 0x7FFF;
	else               return r;
}

/**
 * @param inv_table the yuv2rgb coeffs, normally Inverse_Table_6_9[x]
 * @param fullRange if 1 then the luma range is 0..255 if 0 its 16..235
 * @return -1 if not supported
 */
int sws_setColorspaceDetails(SwsContext *c, const int inv_table[4], int srcRange, const int table[4], int dstRange, int brightness, int contrast, int saturation){
	int64_t crv =  inv_table[0];
	int64_t cbu =  inv_table[1];
	int64_t cgu = -inv_table[2];
	int64_t cgv = -inv_table[3];
	int64_t cy  = 1<<16;
	int64_t oy  = 0;

	if(isYUV(c->dstFormat) || isGray(c->dstFormat)) return -1;
	memcpy(c->srcColorspaceTable, inv_table, sizeof(int)*4);
	memcpy(c->dstColorspaceTable,     table, sizeof(int)*4);

	c->brightness= brightness;
	c->contrast  = contrast;
	c->saturation= saturation;
	c->srcRange  = srcRange;
	c->dstRange  = dstRange;

	c->uOffset=   0x0400040004000400LL;
	c->vOffset=   0x0400040004000400LL;

	if(!srcRange){
		cy= (cy*255) / 219;
		oy= 16<<16;
	}

	cy = (cy *contrast             )>>16;
	crv= (crv*contrast * saturation)>>32;
	cbu= (cbu*contrast * saturation)>>32;
	cgu= (cgu*contrast * saturation)>>32;
	cgv= (cgv*contrast * saturation)>>32;

	oy -= 256*brightness;

	c->yCoeff=    roundToInt16(cy *8192) * 0x0001000100010001ULL;
	c->vrCoeff=   roundToInt16(crv*8192) * 0x0001000100010001ULL;
	c->ubCoeff=   roundToInt16(cbu*8192) * 0x0001000100010001ULL;
	c->vgCoeff=   roundToInt16(cgv*8192) * 0x0001000100010001ULL;
	c->ugCoeff=   roundToInt16(cgu*8192) * 0x0001000100010001ULL;
	c->yOffset=   roundToInt16(oy *   8) * 0x0001000100010001ULL;

	yuv2rgb_c_init_tables(c, inv_table, srcRange, brightness, contrast, saturation);
	//FIXME factorize

#ifdef HAVE_ALTIVEC
	yuv2rgb_altivec_init_tables (c, inv_table);
#endif	
	return 0;
}

/**
 * @return -1 if not supported
 */
int sws_getColorspaceDetails(SwsContext *c, int **inv_table, int *srcRange, int **table, int *dstRange, int *brightness, int *contrast, int *saturation){
	if(isYUV(c->dstFormat) || isGray(c->dstFormat)) return -1;

	*inv_table = c->srcColorspaceTable;
	*table     = c->dstColorspaceTable;
	*srcRange  = c->srcRange;
	*dstRange  = c->dstRange;
	*brightness= c->brightness;
	*contrast  = c->contrast;
	*saturation= c->saturation;
	
	return 0;	
}

SwsContext *sws_getContext(int srcW, int srcH, int origSrcFormat, int dstW, int dstH, int origDstFormat, int flags,
                         SwsFilter *srcFilter, SwsFilter *dstFilter){

	SwsContext *c;
	int i;
	int usesVFilter, usesHFilter;
	int unscaled, needsDither;
	int srcFormat, dstFormat;
	SwsFilter dummyFilter= {NULL, NULL, NULL, NULL};
#ifdef ARCH_X86
	if(flags & SWS_CPU_CAPS_MMX)
		asm volatile("emms\n\t"::: "memory");
#endif

#ifndef RUNTIME_CPUDETECT //ensure that the flags match the compiled variant if cpudetect is off
	flags &= ~(SWS_CPU_CAPS_MMX|SWS_CPU_CAPS_MMX2|SWS_CPU_CAPS_3DNOW|SWS_CPU_CAPS_ALTIVEC);
#ifdef HAVE_MMX2
	flags |= SWS_CPU_CAPS_MMX|SWS_CPU_CAPS_MMX2;
#elif defined (HAVE_3DNOW)
	flags |= SWS_CPU_CAPS_MMX|SWS_CPU_CAPS_3DNOW;
#elif defined (HAVE_MMX)
	flags |= SWS_CPU_CAPS_MMX;
#elif defined (HAVE_ALTIVEC)
	flags |= SWS_CPU_CAPS_ALTIVEC;
#endif
#endif
	if(clip_table[512] != 255) globalInit();
	if(rgb15to16 == NULL) sws_rgb2rgb_init(flags);

	/* avoid duplicate Formats, so we don't need to check to much */
	srcFormat = remove_dup_fourcc(origSrcFormat);
	dstFormat = remove_dup_fourcc(origDstFormat);

	unscaled = (srcW == dstW && srcH == dstH);
	needsDither= (isBGR(dstFormat) || isRGB(dstFormat)) 
		     && (dstFormat&0xFF)<24
		     && ((dstFormat&0xFF)<(srcFormat&0xFF) || (!(isRGB(srcFormat) || isBGR(srcFormat))));

	if(!isSupportedIn(srcFormat)) 
	{
		MSG_ERR("swScaler: %s is not supported as input format\n", vo_format_name(srcFormat));
		return NULL;
	}
	if(!isSupportedOut(dstFormat))
	{
		MSG_ERR("swScaler: %s is not supported as output format\n", vo_format_name(dstFormat));
		return NULL;
	}

	/* sanity check */
	if(srcW<4 || srcH<1 || dstW<8 || dstH<1) //FIXME check if these are enough and try to lowwer them after fixing the relevant parts of the code
	{
		 MSG_ERR("swScaler: %dx%d -> %dx%d is invalid scaling dimension\n", 
			srcW, srcH, dstW, dstH);
		return NULL;
	}

	if(!dstFilter) dstFilter= &dummyFilter;
	if(!srcFilter) srcFilter= &dummyFilter;

	c= memalign(64, sizeof(SwsContext));
	memset(c, 0, sizeof(SwsContext));

	c->srcW= srcW;
	c->srcH= srcH;
	c->dstW= dstW;
	c->dstH= dstH;
	c->lumXInc= ((srcW<<16) + (dstW>>1))/dstW;
	c->lumYInc= ((srcH<<16) + (dstH>>1))/dstH;
	c->flags= flags;
	c->dstFormat= dstFormat;
	c->srcFormat= srcFormat;
	c->origDstFormat= origDstFormat;
	c->origSrcFormat= origSrcFormat;
        c->vRounder= 4* 0x0001000100010001ULL;

	usesHFilter= usesVFilter= 0;
	if(dstFilter->lumV!=NULL && dstFilter->lumV->length>1) usesVFilter=1;
	if(dstFilter->lumH!=NULL && dstFilter->lumH->length>1) usesHFilter=1;
	if(dstFilter->chrV!=NULL && dstFilter->chrV->length>1) usesVFilter=1;
	if(dstFilter->chrH!=NULL && dstFilter->chrH->length>1) usesHFilter=1;
	if(srcFilter->lumV!=NULL && srcFilter->lumV->length>1) usesVFilter=1;
	if(srcFilter->lumH!=NULL && srcFilter->lumH->length>1) usesHFilter=1;
	if(srcFilter->chrV!=NULL && srcFilter->chrV->length>1) usesVFilter=1;
	if(srcFilter->chrH!=NULL && srcFilter->chrH->length>1) usesHFilter=1;

	getSubSampleFactors(&c->chrSrcHSubSample, &c->chrSrcVSubSample, srcFormat);
	getSubSampleFactors(&c->chrDstHSubSample, &c->chrDstVSubSample, dstFormat);

	// reuse chroma for 2 pixles rgb/bgr unless user wants full chroma interpolation
	if((isBGR(dstFormat) || isRGB(dstFormat)) && !(flags&SWS_FULL_CHR_H_INT)) c->chrDstHSubSample=1;

	// drop some chroma lines if the user wants it
	c->vChrDrop= (flags&SWS_SRC_V_CHR_DROP_MASK)>>SWS_SRC_V_CHR_DROP_SHIFT;
	c->chrSrcVSubSample+= c->vChrDrop;

	// drop every 2. pixel for chroma calculation unless user wants full chroma
	if((isBGR(srcFormat) || isRGB(srcFormat)) && !(flags&SWS_FULL_CHR_H_INP)) 
		c->chrSrcHSubSample=1;

	c->chrIntHSubSample= c->chrDstHSubSample;
	c->chrIntVSubSample= c->chrSrcVSubSample;

	// note the -((-x)>>y) is so that we allways round toward +inf
	c->chrSrcW= -((-srcW) >> c->chrSrcHSubSample);
	c->chrSrcH= -((-srcH) >> c->chrSrcVSubSample);
	c->chrDstW= -((-dstW) >> c->chrDstHSubSample);
	c->chrDstH= -((-dstH) >> c->chrDstVSubSample);

	sws_setColorspaceDetails(c, Inverse_Table_6_9[SWS_CS_DEFAULT], 0, Inverse_Table_6_9[SWS_CS_DEFAULT] /* FIXME*/, 0, 0, 1<<16, 1<<16); 

	/* unscaled special Cases */
	if(unscaled && !usesHFilter && !usesVFilter)
	{
		/* yv12_to_nv12 */
		if(srcFormat == IMGFMT_YV12 && dstFormat == IMGFMT_NV12)
		{
			c->swScale= PlanarToNV12Wrapper;
		}
		/* yuv2bgr */
		if((srcFormat==IMGFMT_YV12 || srcFormat==IMGFMT_422P) && (isBGR(dstFormat) || isRGB(dstFormat)))
		{
			c->swScale= yuv2rgb_get_func_ptr(c);
		}
		
		if( srcFormat==IMGFMT_YVU9 && dstFormat==IMGFMT_YV12 )
		{
			c->swScale= yvu9toyv12Wrapper;
		}

		/* bgr24toYV12 */
		if(srcFormat==IMGFMT_BGR24 && dstFormat==IMGFMT_YV12)
			c->swScale= bgr24toyv12Wrapper;
		
		/* rgb/bgr -> rgb/bgr (no dither needed forms) */
		if(   (isBGR(srcFormat) || isRGB(srcFormat))
		   && (isBGR(dstFormat) || isRGB(dstFormat)) 
		   && !needsDither)
			c->swScale= rgb2rgbWrapper;

		/* LQ converters if -sws 0 or -sws 4*/
		if(c->flags&(SWS_FAST_BILINEAR|SWS_POINT)){
			/* rgb/bgr -> rgb/bgr (dither needed forms) */
			if(  (isBGR(srcFormat) || isRGB(srcFormat))
			  && (isBGR(dstFormat) || isRGB(dstFormat)) 
			  && needsDither)
				c->swScale= rgb2rgbWrapper;

			/* yv12_to_yuy2 */
			if(srcFormat == IMGFMT_YV12 && 
			    (dstFormat == IMGFMT_YUY2 || dstFormat == IMGFMT_UYVY))
			{
				if (dstFormat == IMGFMT_YUY2)
				    c->swScale= PlanarToYuy2Wrapper;
				else
				    c->swScale= PlanarToUyvyWrapper;
			}
		}

#ifdef HAVE_ALTIVEC
		if ((c->flags & SWS_CPU_CAPS_ALTIVEC) &&
		    ((srcFormat == IMGFMT_YV12 && 
		      (dstFormat == IMGFMT_YUY2 || dstFormat == IMGFMT_UYVY)))) {
		  // unscaled YV12 -> packed YUV, we want speed
		  if (dstFormat == IMGFMT_YUY2)
		    c->swScale= yv12toyuy2_unscaled_altivec;
		  else
		    c->swScale= yv12touyvy_unscaled_altivec;
		}
#endif

		/* simple copy */
		if(   srcFormat == dstFormat
		   || (isPlanarYUV(srcFormat) && isGray(dstFormat))
		   || (isPlanarYUV(dstFormat) && isGray(srcFormat))
		  )
		{
			c->swScale= simpleCopy;
		}

		if(c->swScale){
			if(flags&SWS_PRINT_INFO)
				MSG_INFO("SwScaler: using unscaled %s -> %s special converter\n", 
					vo_format_name(srcFormat), vo_format_name(dstFormat));
			return c;
		}
	}

	if(flags & SWS_CPU_CAPS_MMX2)
	{
		c->canMMX2BeUsed= (dstW >=srcW && (dstW&31)==0 && (srcW&15)==0) ? 1 : 0;
		if(!c->canMMX2BeUsed && dstW >=srcW && (srcW&15)==0 && (flags&SWS_FAST_BILINEAR))
		{
			if(flags&SWS_PRINT_INFO)
				MSG_INFO("SwScaler: output Width is not a multiple of 32 -> no MMX2 scaler\n");
		}
		if(usesHFilter) c->canMMX2BeUsed=0;
	}
	else
		c->canMMX2BeUsed=0;

	c->chrXInc= ((c->chrSrcW<<16) + (c->chrDstW>>1))/c->chrDstW;
	c->chrYInc= ((c->chrSrcH<<16) + (c->chrDstH>>1))/c->chrDstH;

	// match pixel 0 of the src to pixel 0 of dst and match pixel n-2 of src to pixel n-2 of dst
	// but only for the FAST_BILINEAR mode otherwise do correct scaling
	// n-2 is the last chrominance sample available
	// this is not perfect, but noone shuld notice the difference, the more correct variant
	// would be like the vertical one, but that would require some special code for the
	// first and last pixel
	if(flags&SWS_FAST_BILINEAR)
	{
		if(c->canMMX2BeUsed)
		{
			c->lumXInc+= 20;
			c->chrXInc+= 20;
		}
		//we don't use the x86asm scaler if mmx is available
		else if(flags & SWS_CPU_CAPS_MMX)
		{
			c->lumXInc = ((srcW-2)<<16)/(dstW-2) - 20;
			c->chrXInc = ((c->chrSrcW-2)<<16)/(c->chrDstW-2) - 20;
		}
	}

	/* precalculate horizontal scaler filter coefficients */
	{
		const int filterAlign=
		  (flags & SWS_CPU_CAPS_MMX) ? 4 :
		  (flags & SWS_CPU_CAPS_ALTIVEC) ? 8 :
		  1;

		initFilter(&c->hLumFilter, &c->hLumFilterPos, &c->hLumFilterSize, c->lumXInc,
				 srcW      ,       dstW, filterAlign, 1<<14,
				 (flags&SWS_BICUBLIN) ? (flags|SWS_BICUBIC)  : flags,
				 srcFilter->lumH, dstFilter->lumH);
		initFilter(&c->hChrFilter, &c->hChrFilterPos, &c->hChrFilterSize, c->chrXInc,
				 c->chrSrcW, c->chrDstW, filterAlign, 1<<14,
				 (flags&SWS_BICUBLIN) ? (flags|SWS_BILINEAR) : flags,
				 srcFilter->chrH, dstFilter->chrH);

#ifdef ARCH_X86
// can't downscale !!!
		if(c->canMMX2BeUsed && (flags & SWS_FAST_BILINEAR))
		{
			c->lumMmx2Filter   = (int16_t*)memalign(8, (dstW        /8+8)*sizeof(int16_t));
			c->chrMmx2Filter   = (int16_t*)memalign(8, (c->chrDstW  /4+8)*sizeof(int16_t));
			c->lumMmx2FilterPos= (int32_t*)memalign(8, (dstW      /2/8+8)*sizeof(int32_t));
			c->chrMmx2FilterPos= (int32_t*)memalign(8, (c->chrDstW/2/4+8)*sizeof(int32_t));

			initMMX2HScaler(      dstW, c->lumXInc, c->funnyYCode , c->lumMmx2Filter, c->lumMmx2FilterPos, 8);
			initMMX2HScaler(c->chrDstW, c->chrXInc, c->funnyUVCode, c->chrMmx2Filter, c->chrMmx2FilterPos, 4);
		}
#endif
	} // Init Horizontal stuff



	/* precalculate vertical scaler filter coefficients */
	{
		const int filterAlign=
		  (flags & SWS_CPU_CAPS_ALTIVEC) ? 8 :
		  1;

		initFilter(&c->vLumFilter, &c->vLumFilterPos, &c->vLumFilterSize, c->lumYInc,
				srcH      ,        dstH, filterAlign, (1<<12)-4,
				(flags&SWS_BICUBLIN) ? (flags|SWS_BICUBIC)  : flags,
				srcFilter->lumV, dstFilter->lumV);
		initFilter(&c->vChrFilter, &c->vChrFilterPos, &c->vChrFilterSize, c->chrYInc,
				c->chrSrcH, c->chrDstH, filterAlign, (1<<12)-4,
				(flags&SWS_BICUBLIN) ? (flags|SWS_BILINEAR) : flags,
				srcFilter->chrV, dstFilter->chrV);
	}

	// Calculate Buffer Sizes so that they won't run out while handling these damn slices
	c->vLumBufSize= c->vLumFilterSize;
	c->vChrBufSize= c->vChrFilterSize;
	for(i=0; i<dstH; i++)
	{
		int chrI= i*c->chrDstH / dstH;
		int nextSlice= MAX(c->vLumFilterPos[i   ] + c->vLumFilterSize - 1,
				 ((c->vChrFilterPos[chrI] + c->vChrFilterSize - 1)<<c->chrSrcVSubSample));

		nextSlice>>= c->chrSrcVSubSample;
		nextSlice<<= c->chrSrcVSubSample;
		if(c->vLumFilterPos[i   ] + c->vLumBufSize < nextSlice)
			c->vLumBufSize= nextSlice - c->vLumFilterPos[i   ];
		if(c->vChrFilterPos[chrI] + c->vChrBufSize < (nextSlice>>c->chrSrcVSubSample))
			c->vChrBufSize= (nextSlice>>c->chrSrcVSubSample) - c->vChrFilterPos[chrI];
	}

	// allocate pixbufs (we use dynamic allocation because otherwise we would need to
	c->lumPixBuf= (int16_t**)memalign(4, c->vLumBufSize*2*sizeof(int16_t*));
	c->chrPixBuf= (int16_t**)memalign(4, c->vChrBufSize*2*sizeof(int16_t*));
	//Note we need at least one pixel more at the end because of the mmx code (just in case someone wanna replace the 4000/8000)
	for(i=0; i<c->vLumBufSize; i++)
		c->lumPixBuf[i]= c->lumPixBuf[i+c->vLumBufSize]= (uint16_t*)memalign(8, 4000);
	for(i=0; i<c->vChrBufSize; i++)
		c->chrPixBuf[i]= c->chrPixBuf[i+c->vChrBufSize]= (uint16_t*)memalign(8, 8000);

	//try to avoid drawing green stuff between the right end and the stride end
	for(i=0; i<c->vLumBufSize; i++) memset(c->lumPixBuf[i], 0, 4000);
	for(i=0; i<c->vChrBufSize; i++) memset(c->chrPixBuf[i], 64, 8000);

	ASSERT(c->chrDstH <= dstH)

	if(flags&SWS_PRINT_INFO)
	{
#ifdef DITHER1XBPP
		char *dither= " dithered";
#else
		char *dither= "";
#endif
		if(flags&SWS_FAST_BILINEAR)
			MSG_INFO("\nSwScaler: FAST_BILINEAR scaler, ");
		else if(flags&SWS_BILINEAR)
			MSG_INFO("\nSwScaler: BILINEAR scaler, ");
		else if(flags&SWS_BICUBIC)
			MSG_INFO("\nSwScaler: BICUBIC scaler, ");
		else if(flags&SWS_X)
			MSG_INFO("\nSwScaler: Experimental scaler, ");
		else if(flags&SWS_POINT)
			MSG_INFO("\nSwScaler: Nearest Neighbor / POINT scaler, ");
		else if(flags&SWS_AREA)
			MSG_INFO("\nSwScaler: Area Averageing scaler, ");
		else if(flags&SWS_BICUBLIN)
			MSG_INFO("\nSwScaler: luma BICUBIC / chroma BILINEAR scaler, ");
		else if(flags&SWS_GAUSS)
			MSG_INFO("\nSwScaler: Gaussian scaler, ");
		else if(flags&SWS_SINC)
			MSG_INFO("\nSwScaler: Sinc scaler, ");
		else if(flags&SWS_LANCZOS)
			MSG_INFO("\nSwScaler: Lanczos scaler, ");
		else if(flags&SWS_SPLINE)
			MSG_INFO("\nSwScaler: Bicubic spline scaler, ");
		else
			MSG_INFO("\nSwScaler: ehh flags invalid?! ");

		if(dstFormat==IMGFMT_BGR15 || dstFormat==IMGFMT_BGR16)
			MSG_INFO("from %s to%s %s ", 
				vo_format_name(srcFormat), dither, vo_format_name(dstFormat));
		else
			MSG_INFO("from %s to %s ", 
				vo_format_name(srcFormat), vo_format_name(dstFormat));

		if(flags & SWS_CPU_CAPS_MMX2)
			MSG_INFO("using MMX2\n");
		else if(flags & SWS_CPU_CAPS_3DNOW)
			MSG_INFO("using 3DNOW\n");
		else if(flags & SWS_CPU_CAPS_MMX)
			MSG_INFO("using MMX\n");
		else if(flags & SWS_CPU_CAPS_ALTIVEC)
			MSG_INFO("using AltiVec\n");
		else 
			MSG_INFO("using C\n");
	}

	if(flags & SWS_PRINT_INFO)
	{
		if(flags & SWS_CPU_CAPS_MMX)
		{
			if(c->canMMX2BeUsed && (flags&SWS_FAST_BILINEAR))
				MSG_V("SwScaler: using FAST_BILINEAR MMX2 scaler for horizontal scaling\n");
			else
			{
				if(c->hLumFilterSize==4)
					MSG_V("SwScaler: using 4-tap MMX scaler for horizontal luminance scaling\n");
				else if(c->hLumFilterSize==8)
					MSG_V("SwScaler: using 8-tap MMX scaler for horizontal luminance scaling\n");
				else
					MSG_V("SwScaler: using n-tap MMX scaler for horizontal luminance scaling\n");

				if(c->hChrFilterSize==4)
					MSG_V("SwScaler: using 4-tap MMX scaler for horizontal chrominance scaling\n");
				else if(c->hChrFilterSize==8)
					MSG_V("SwScaler: using 8-tap MMX scaler for horizontal chrominance scaling\n");
				else
					MSG_V("SwScaler: using n-tap MMX scaler for horizontal chrominance scaling\n");
			}
		}
		else
		{
#ifdef ARCH_X86
			MSG_V("SwScaler: using X86-Asm scaler for horizontal scaling\n");
#else
			if(flags & SWS_FAST_BILINEAR)
				MSG_V("SwScaler: using FAST_BILINEAR C scaler for horizontal scaling\n");
			else
				MSG_V("SwScaler: using C scaler for horizontal scaling\n");
#endif
		}
		if(isPlanarYUV(dstFormat))
		{
			if(c->vLumFilterSize==1)
				MSG_V("SwScaler: using 1-tap %s \"scaler\" for vertical scaling (YV12 like)\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
			else
				MSG_V("SwScaler: using n-tap %s scaler for vertical scaling (YV12 like)\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
		}
		else
		{
			if(c->vLumFilterSize==1 && c->vChrFilterSize==2)
				MSG_V("SwScaler: using 1-tap %s \"scaler\" for vertical luminance scaling (BGR)\n"
				       "SwScaler:       2-tap scaler for vertical chrominance scaling (BGR)\n",(flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
			else if(c->vLumFilterSize==2 && c->vChrFilterSize==2)
				MSG_V("SwScaler: using 2-tap linear %s scaler for vertical scaling (BGR)\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
			else
				MSG_V("SwScaler: using n-tap %s scaler for vertical scaling (BGR)\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
		}

		if(dstFormat==IMGFMT_BGR24)
			MSG_V("SwScaler: using %s YV12->BGR24 Converter\n",
				(flags & SWS_CPU_CAPS_MMX2) ? "MMX2" : ((flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C"));
		else if(dstFormat==IMGFMT_BGR32)
			MSG_V("SwScaler: using %s YV12->BGR32 Converter\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
		else if(dstFormat==IMGFMT_BGR16)
			MSG_V("SwScaler: using %s YV12->BGR16 Converter\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");
		else if(dstFormat==IMGFMT_BGR15)
			MSG_V("SwScaler: using %s YV12->BGR15 Converter\n", (flags & SWS_CPU_CAPS_MMX) ? "MMX" : "C");

		MSG_V("SwScaler: %dx%d -> %dx%d\n", srcW, srcH, dstW, dstH);
	}
	if(flags & SWS_PRINT_INFO)
	{
		MSG_DBG2("SwScaler:Lum srcW=%d srcH=%d dstW=%d dstH=%d xInc=%d yInc=%d\n",
			c->srcW, c->srcH, c->dstW, c->dstH, c->lumXInc, c->lumYInc);
		MSG_DBG2("SwScaler:Chr srcW=%d srcH=%d dstW=%d dstH=%d xInc=%d yInc=%d\n",
			c->chrSrcW, c->chrSrcH, c->chrDstW, c->chrDstH, c->chrXInc, c->chrYInc);
	}

	c->swScale= getSwsFunc(flags);
	return c;
}

/**
 * swscale warper, so we don't need to export the SwsContext.
 * assumes planar YUV to be in YUV order instead of YVU
 */
int sws_scale_ordered(SwsContext *c, uint8_t* src[], int srcStride[], int srcSliceY,
                           int srcSliceH, uint8_t* dst[], int dstStride[]){
	//copy strides, so they can safely be modified
	int srcStride2[3]= {srcStride[0], srcStride[1], srcStride[2]};
	int dstStride2[3]= {dstStride[0], dstStride[1], dstStride[2]};
	return c->swScale(c, src, srcStride2, srcSliceY, srcSliceH, dst, dstStride2);
}

/**
 * swscale warper, so we don't need to export the SwsContext
 */
int sws_scale(SwsContext *c, uint8_t* srcParam[], int srcStrideParam[], int srcSliceY,
                           int srcSliceH, uint8_t* dstParam[], int dstStrideParam[]){
	int srcStride[3];
	int dstStride[3];
	uint8_t *src[3];
	uint8_t *dst[3];
	sws_orderYUV(c->origSrcFormat, src, srcStride, srcParam, srcStrideParam);
	sws_orderYUV(c->origDstFormat, dst, dstStride, dstParam, dstStrideParam);
//printf("sws: slice %d %d\n", srcSliceY, srcSliceH);

	return c->swScale(c, src, srcStride, srcSliceY, srcSliceH, dst, dstStride);
}

SwsFilter *sws_getDefaultFilter(float lumaGBlur, float chromaGBlur, 
				float lumaSharpen, float chromaSharpen,
				float chromaHShift, float chromaVShift,
				int verbose)
{
	SwsFilter *filter= malloc(sizeof(SwsFilter));

	if(lumaGBlur!=0.0){
		filter->lumH= sws_getGaussianVec(lumaGBlur, 3.0);
		filter->lumV= sws_getGaussianVec(lumaGBlur, 3.0);
	}else{
		filter->lumH= sws_getIdentityVec();
		filter->lumV= sws_getIdentityVec();
	}

	if(chromaGBlur!=0.0){
		filter->chrH= sws_getGaussianVec(chromaGBlur, 3.0);
		filter->chrV= sws_getGaussianVec(chromaGBlur, 3.0);
	}else{
		filter->chrH= sws_getIdentityVec();
		filter->chrV= sws_getIdentityVec();
	}

	if(chromaSharpen!=0.0){
		SwsVector *g= sws_getConstVec(-1.0, 3);
		SwsVector *id= sws_getConstVec(10.0/chromaSharpen, 1);
		g->coeff[1]=2.0;
		sws_addVec(id, g);
		sws_convVec(filter->chrH, id);
		sws_convVec(filter->chrV, id);
		sws_freeVec(g);
		sws_freeVec(id);
	}

	if(lumaSharpen!=0.0){
		SwsVector *g= sws_getConstVec(-1.0, 3);
		SwsVector *id= sws_getConstVec(10.0/lumaSharpen, 1);
		g->coeff[1]=2.0;
		sws_addVec(id, g);
		sws_convVec(filter->lumH, id);
		sws_convVec(filter->lumV, id);
		sws_freeVec(g);
		sws_freeVec(id);
	}

	if(chromaHShift != 0.0)
		sws_shiftVec(filter->chrH, (int)(chromaHShift+0.5));

	if(chromaVShift != 0.0)
		sws_shiftVec(filter->chrV, (int)(chromaVShift+0.5));

	sws_normalizeVec(filter->chrH, 1.0);
	sws_normalizeVec(filter->chrV, 1.0);
	sws_normalizeVec(filter->lumH, 1.0);
	sws_normalizeVec(filter->lumV, 1.0);

	if(verbose) sws_printVec(filter->chrH);
	if(verbose) sws_printVec(filter->lumH);

        return filter;
}

/**
 * returns a normalized gaussian curve used to filter stuff
 * quality=3 is high quality, lowwer is lowwer quality
 */
SwsVector *sws_getGaussianVec(double variance, double quality){
	const int length= (int)(variance*quality + 0.5) | 1;
	int i;
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	double middle= (length-1)*0.5;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++)
	{
		double dist= i-middle;
		coeff[i]= exp( -dist*dist/(2*variance*variance) ) / sqrt(2*variance*PI);
	}

	sws_normalizeVec(vec, 1.0);

	return vec;
}

SwsVector *sws_getConstVec(double c, int length){
	int i;
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++)
		coeff[i]= c;

	return vec;
}


SwsVector *sws_getIdentityVec(void){
	double *coeff= memalign(sizeof(double), sizeof(double));
	SwsVector *vec= malloc(sizeof(SwsVector));
	coeff[0]= 1.0;

	vec->coeff= coeff;
	vec->length= 1;

	return vec;
}

void sws_normalizeVec(SwsVector *a, double height){
	int i;
	double sum=0;
	double inv;

	for(i=0; i<a->length; i++)
		sum+= a->coeff[i];

	inv= height/sum;

	for(i=0; i<a->length; i++)
		a->coeff[i]*= inv;
}

void sws_scaleVec(SwsVector *a, double scalar){
	int i;

	for(i=0; i<a->length; i++)
		a->coeff[i]*= scalar;
}

static SwsVector *sws_getConvVec(SwsVector *a, SwsVector *b){
	int length= a->length + b->length - 1;
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	int i, j;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++) coeff[i]= 0.0;

	for(i=0; i<a->length; i++)
	{
		for(j=0; j<b->length; j++)
		{
			coeff[i+j]+= a->coeff[i]*b->coeff[j];
		}
	}

	return vec;
}

static SwsVector *sws_sumVec(SwsVector *a, SwsVector *b){
	int length= MAX(a->length, b->length);
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	int i;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++) coeff[i]= 0.0;

	for(i=0; i<a->length; i++) coeff[i + (length-1)/2 - (a->length-1)/2]+= a->coeff[i];
	for(i=0; i<b->length; i++) coeff[i + (length-1)/2 - (b->length-1)/2]+= b->coeff[i];

	return vec;
}

static SwsVector *sws_diffVec(SwsVector *a, SwsVector *b){
	int length= MAX(a->length, b->length);
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	int i;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++) coeff[i]= 0.0;

	for(i=0; i<a->length; i++) coeff[i + (length-1)/2 - (a->length-1)/2]+= a->coeff[i];
	for(i=0; i<b->length; i++) coeff[i + (length-1)/2 - (b->length-1)/2]-= b->coeff[i];

	return vec;
}

/* shift left / or right if "shift" is negative */
static SwsVector *sws_getShiftedVec(SwsVector *a, int shift){
	int length= a->length + ABS(shift)*2;
	double *coeff= memalign(sizeof(double), length*sizeof(double));
	int i;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= length;

	for(i=0; i<length; i++) coeff[i]= 0.0;

	for(i=0; i<a->length; i++)
	{
		coeff[i + (length-1)/2 - (a->length-1)/2 - shift]= a->coeff[i];
	}

	return vec;
}

void sws_shiftVec(SwsVector *a, int shift){
	SwsVector *shifted= sws_getShiftedVec(a, shift);
	free(a->coeff);
	a->coeff= shifted->coeff;
	a->length= shifted->length;
	free(shifted);
}

void sws_addVec(SwsVector *a, SwsVector *b){
	SwsVector *sum= sws_sumVec(a, b);
	free(a->coeff);
	a->coeff= sum->coeff;
	a->length= sum->length;
	free(sum);
}

void sws_subVec(SwsVector *a, SwsVector *b){
	SwsVector *diff= sws_diffVec(a, b);
	free(a->coeff);
	a->coeff= diff->coeff;
	a->length= diff->length;
	free(diff);
}

void sws_convVec(SwsVector *a, SwsVector *b){
	SwsVector *conv= sws_getConvVec(a, b);
	free(a->coeff);  
	a->coeff= conv->coeff;
	a->length= conv->length;
	free(conv);
}

SwsVector *sws_cloneVec(SwsVector *a){
	double *coeff= memalign(sizeof(double), a->length*sizeof(double));
	int i;
	SwsVector *vec= malloc(sizeof(SwsVector));

	vec->coeff= coeff;
	vec->length= a->length;

	for(i=0; i<a->length; i++) coeff[i]= a->coeff[i];

	return vec;
}

void sws_printVec(SwsVector *a){
	int i;
	double max=0;
	double min=0;
	double range;

	for(i=0; i<a->length; i++)
		if(a->coeff[i]>max) max= a->coeff[i];

	for(i=0; i<a->length; i++)
		if(a->coeff[i]<min) min= a->coeff[i];

	range= max - min;

	for(i=0; i<a->length; i++)
	{
		int x= (int)((a->coeff[i]-min)*60.0/range +0.5);
		MSG_DBG2("%1.3f ", a->coeff[i]);
		for(;x>0; x--) MSG_DBG2(" ");
		MSG_DBG2("|\n");
	}
}

void sws_freeVec(SwsVector *a){
	if(!a) return;
	if(a->coeff) free(a->coeff);
	a->coeff=NULL;
	a->length=0;
	free(a);
}

void sws_freeFilter(SwsFilter *filter){
	if(!filter) return;

	if(filter->lumH) sws_freeVec(filter->lumH);
	if(filter->lumV) sws_freeVec(filter->lumV);
	if(filter->chrH) sws_freeVec(filter->chrH);
	if(filter->chrV) sws_freeVec(filter->chrV);
	free(filter);
}


void sws_freeContext(SwsContext *c){
	int i;
	if(!c) return;

	if(c->lumPixBuf)
	{
		for(i=0; i<c->vLumBufSize; i++)
		{
			if(c->lumPixBuf[i]) free(c->lumPixBuf[i]);
			c->lumPixBuf[i]=NULL;
		}
		free(c->lumPixBuf);
		c->lumPixBuf=NULL;
	}

	if(c->chrPixBuf)
	{
		for(i=0; i<c->vChrBufSize; i++)
		{
			if(c->chrPixBuf[i]) free(c->chrPixBuf[i]);
			c->chrPixBuf[i]=NULL;
		}
		free(c->chrPixBuf);
		c->chrPixBuf=NULL;
	}

	if(c->vLumFilter) free(c->vLumFilter);
	c->vLumFilter = NULL;
	if(c->vChrFilter) free(c->vChrFilter);
	c->vChrFilter = NULL;
	if(c->hLumFilter) free(c->hLumFilter);
	c->hLumFilter = NULL;
	if(c->hChrFilter) free(c->hChrFilter);
	c->hChrFilter = NULL;

	if(c->vLumFilterPos) free(c->vLumFilterPos);
	c->vLumFilterPos = NULL;
	if(c->vChrFilterPos) free(c->vChrFilterPos);
	c->vChrFilterPos = NULL;
	if(c->hLumFilterPos) free(c->hLumFilterPos);
	c->hLumFilterPos = NULL;
	if(c->hChrFilterPos) free(c->hChrFilterPos);
	c->hChrFilterPos = NULL;

	if(c->lumMmx2Filter) free(c->lumMmx2Filter);
	c->lumMmx2Filter=NULL;
	if(c->chrMmx2Filter) free(c->chrMmx2Filter);
	c->chrMmx2Filter=NULL;
	if(c->lumMmx2FilterPos) free(c->lumMmx2FilterPos);
	c->lumMmx2FilterPos=NULL;
	if(c->chrMmx2FilterPos) free(c->chrMmx2FilterPos);
	c->chrMmx2FilterPos=NULL;
	if(c->yuvTable) free(c->yuvTable);
	c->yuvTable=NULL;

	free(c);
}

