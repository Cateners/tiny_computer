/*
 * Copyright (C)2009-2012 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* TurboJPEG/OSS:  this implements the TurboJPEG API using libjpeg-turbo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef JCS_EXTENSIONS
#define JPEG_INTERNAL_OPTIONS
#endif
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include "./turbojpeg.h"

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))

#define CSTATE_START 100
#define DSTATE_START 200
#define MEMZERO(ptr, size) memset(ptr, 0, size)

#ifndef min
 #define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
 #define max(a,b) ((a)>(b)?(a):(b))
#endif


/* Error handling (based on example in example.c) */

static char errStr[JMSG_LENGTH_MAX]="No error";

struct my_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit(j_common_ptr cinfo)
{
	my_error_ptr myerr=(my_error_ptr)cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

/* Based on output_message() in jerror.c */

static void my_output_message(j_common_ptr cinfo)
{
	(*cinfo->err->format_message)(cinfo, errStr);
}


/* Global structures, macros, etc. */

enum {COMPRESS=1, DECOMPRESS=2};

typedef struct _tjinstance
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct jpeg_destination_mgr jdst;
	struct jpeg_source_mgr jsrc;
	struct my_error_mgr jerr;
	int init;
} tjinstance;

static const int pixelsize[TJ_NUMSAMP]={3, 3, 3, 1, 3};

#define NUMSF 4
static const tjscalingfactor sf[NUMSF]={
	{1, 1},
	{1, 2},
	{1, 4},
	{1, 8}
};

#define _throw(m) {snprintf(errStr, JMSG_LENGTH_MAX, "%s", m);  \
	retval=-1;  goto bailout;}
#define getinstance(handle) tjinstance *this=(tjinstance *)handle;  \
	j_compress_ptr cinfo=NULL;  j_decompress_ptr dinfo=NULL;  \
	(void) cinfo; (void) dinfo; /* silence warnings */ \
	if(!this) {snprintf(errStr, JMSG_LENGTH_MAX, "Invalid handle");  \
		return -1;}  \
	cinfo=&this->cinfo;  dinfo=&this->dinfo;

static int getPixelFormat(int pixelSize, int flags)
{
	if(pixelSize==1) return TJPF_GRAY;
	if(pixelSize==3)
	{
		if(flags&TJ_BGR) return TJPF_BGR;
		else return TJPF_RGB;
	}
	if(pixelSize==4)
	{
		if(flags&TJ_ALPHAFIRST)
		{
			if(flags&TJ_BGR) return TJPF_XBGR;
			else return TJPF_XRGB;
		}
		else
		{
			if(flags&TJ_BGR) return TJPF_BGRX;
			else return TJPF_RGBX;
		}
	}
	return -1;
}

static int setCompDefaults(struct jpeg_compress_struct *cinfo,
	int pixelFormat, int subsamp, int jpegQual)
{
	int retval=0;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			cinfo->in_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			cinfo->in_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			cinfo->in_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
		case TJPF_RGBA:
			cinfo->in_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
		case TJPF_BGRA:
			cinfo->in_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
		case TJPF_ARGB:
			cinfo->in_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
		case TJPF_ABGR:
			cinfo->in_color_space=JCS_EXT_XBGR;  break;
		#else
		case TJPF_RGB:
		case TJPF_BGR:
		case TJPF_RGBX:
		case TJPF_BGRX:
		case TJPF_XRGB:
		case TJPF_XBGR:
		case TJPF_RGBA:
		case TJPF_BGRA:
		case TJPF_ARGB:
		case TJPF_ABGR:
			cinfo->in_color_space=JCS_RGB;  pixelFormat=TJPF_RGB;
			break;
		#endif
	}

	cinfo->input_components=tjPixelSize[pixelFormat];
	jpeg_set_defaults(cinfo);
	if(jpegQual>=0)
	{
		jpeg_set_quality(cinfo, jpegQual, TRUE);
		if(jpegQual>=96) cinfo->dct_method=JDCT_ISLOW;
		else cinfo->dct_method=JDCT_FASTEST;
	}
	if(subsamp==TJSAMP_GRAY)
		jpeg_set_colorspace(cinfo, JCS_GRAYSCALE);
	else
		jpeg_set_colorspace(cinfo, JCS_YCbCr);

	cinfo->comp_info[0].h_samp_factor=tjMCUWidth[subsamp]/8;
	cinfo->comp_info[1].h_samp_factor=1;
	cinfo->comp_info[2].h_samp_factor=1;
	cinfo->comp_info[0].v_samp_factor=tjMCUHeight[subsamp]/8;
	cinfo->comp_info[1].v_samp_factor=1;
	cinfo->comp_info[2].v_samp_factor=1;

	return retval;
}

static int setDecompDefaults(struct jpeg_decompress_struct *dinfo,
	int pixelFormat)
{
	int retval=0;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			dinfo->out_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			dinfo->out_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			dinfo->out_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
			dinfo->out_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
			dinfo->out_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
			dinfo->out_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
			dinfo->out_color_space=JCS_EXT_XBGR;  break;
		#if JCS_ALPHA_EXTENSIONS==1
		case TJPF_RGBA:
			dinfo->out_color_space=JCS_EXT_RGBA;  break;
		case TJPF_BGRA:
			dinfo->out_color_space=JCS_EXT_BGRA;  break;
		case TJPF_ARGB:
			dinfo->out_color_space=JCS_EXT_ARGB;  break;
		case TJPF_ABGR:
			dinfo->out_color_space=JCS_EXT_ABGR;  break;
		#endif
		#else
		case TJPF_RGB:
		case TJPF_BGR:
		case TJPF_RGBX:
		case TJPF_BGRX:
		case TJPF_XRGB:
		case TJPF_XBGR:
		case TJPF_RGBA:
		case TJPF_BGRA:
		case TJPF_ARGB:
		case TJPF_ABGR:
			dinfo->out_color_space=JCS_RGB;  break;
		#endif
		default:
			_throw("Unsupported pixel format");
	}

	bailout:
	return retval;
}


static int getSubsamp(j_decompress_ptr dinfo)
{
	int retval=-1, i, k;
	for(i=0; i<NUMSUBOPT; i++)
	{
		if(dinfo->num_components==pixelsize[i])
		{
			if(dinfo->comp_info[0].h_samp_factor==tjMCUWidth[i]/8
				&& dinfo->comp_info[0].v_samp_factor==tjMCUHeight[i]/8)
			{
				int match=0;
				for(k=1; k<dinfo->num_components; k++)
				{
					if(dinfo->comp_info[k].h_samp_factor==1
						&& dinfo->comp_info[k].v_samp_factor==1)
						match++;
				}
				if(match==dinfo->num_components-1)
				{
					retval=i;  break;
				}
			}
		}
	}
	return retval;
}


#ifndef JCS_EXTENSIONS

/* Conversion functions to emulate the colorspace extensions.  This allows the
   TurboJPEG wrapper to be used with libjpeg */

#define TORGB(PS, ROFFSET, GOFFSET, BOFFSET) {  \
	int rowPad=pitch-width*PS;  \
	while(height--)  \
	{  \
		unsigned char *endOfRow=src+width*PS;  \
		while(src<endOfRow)  \
		{  \
			dst[RGB_RED]=src[ROFFSET];  \
			dst[RGB_GREEN]=src[GOFFSET];  \
			dst[RGB_BLUE]=src[BOFFSET];  \
			dst+=RGB_PIXELSIZE;  src+=PS;  \
		}  \
		src+=rowPad;  \
	}  \
}

static unsigned char *toRGB(unsigned char *src, int width, int pitch,
	int height, int pixelFormat, unsigned char *dst)
{
	unsigned char *retval=src;
	switch(pixelFormat)
	{
		case TJPF_RGB:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=3
			retval=dst;  TORGB(3, 0, 1, 2);
			#endif
			break;
		case TJPF_BGR:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=3
			retval=dst;  TORGB(3, 2, 1, 0);
			#endif
			break;
		case TJPF_RGBX:
		case TJPF_RGBA:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 0, 1, 2);
			#endif
			break;
		case TJPF_BGRX:
		case TJPF_BGRA:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 2, 1, 0);
			#endif
			break;
		case TJPF_XRGB:
		case TJPF_ARGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 1, 2, 3);
			#endif
			break;
		case TJPF_XBGR:
		case TJPF_ABGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			retval=dst;  TORGB(4, 3, 2, 1);
			#endif
			break;
	}
	return retval;
}

#define FROMRGB(PS, ROFFSET, GOFFSET, BOFFSET, SETALPHA) {  \
	int rowPad=pitch-width*PS;  \
	while(height--)  \
	{  \
		unsigned char *endOfRow=dst+width*PS;  \
		while(dst<endOfRow)  \
		{  \
			dst[ROFFSET]=src[RGB_RED];  \
			dst[GOFFSET]=src[RGB_GREEN];  \
			dst[BOFFSET]=src[RGB_BLUE];  \
			SETALPHA  \
			dst+=PS;  src+=RGB_PIXELSIZE;  \
		}  \
		dst+=rowPad;  \
	}  \
}

static void fromRGB(unsigned char *src, unsigned char *dst, int width,
	int pitch, int height, int pixelFormat)
{
	switch(pixelFormat)
	{
		case TJPF_RGB:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=3
			FROMRGB(3, 0, 1, 2,);
			#endif
			break;
		case TJPF_BGR:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=3
			FROMRGB(3, 2, 1, 0,);
			#endif
			break;
		case TJPF_RGBX:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			FROMRGB(4, 0, 1, 2,);
			#endif
			break;
		case TJPF_RGBA:
			#if RGB_RED!=0 || RGB_GREEN!=1 || RGB_BLUE!=2 || RGB_PIXELSIZE!=4
			FROMRGB(4, 0, 1, 2, dst[3]=0xFF;);
			#endif
			break;
		case TJPF_BGRX:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			FROMRGB(4, 2, 1, 0,);
			#endif
			break;
		case TJPF_BGRA:
			#if RGB_RED!=2 || RGB_GREEN!=1 || RGB_BLUE!=0 || RGB_PIXELSIZE!=4
			FROMRGB(4, 2, 1, 0, dst[3]=0xFF;);  return;
			#endif
			break;
		case TJPF_XRGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			FROMRGB(4, 1, 2, 3,);  return;
			#endif
			break;
		case TJPF_ARGB:
			#if RGB_RED!=1 || RGB_GREEN!=2 || RGB_BLUE!=3 || RGB_PIXELSIZE!=4
			FROMRGB(4, 1, 2, 3, dst[0]=0xFF;);  return;
			#endif
			break;
		case TJPF_XBGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			FROMRGB(4, 3, 2, 1,);  return;
			#endif
			break;
		case TJPF_ABGR:
			#if RGB_RED!=3 || RGB_GREEN!=2 || RGB_BLUE!=1 || RGB_PIXELSIZE!=4
			FROMRGB(4, 3, 2, 1, dst[0]=0xFF;);  return;
			#endif
			break;
	}
}

#endif


/* General API functions */

DLLEXPORT char* DLLCALL tjGetErrorStr(void)
{
	return errStr;
}


DLLEXPORT int DLLCALL tjDestroy(tjhandle handle)
{
	getinstance(handle);
	if(setjmp(this->jerr.setjmp_buffer)) return -1;
	if(this->init&COMPRESS) jpeg_destroy_compress(cinfo);
	if(this->init&DECOMPRESS) jpeg_destroy_decompress(dinfo);
	free(this);
	return 0;
}


/* Compressor  */

static boolean empty_output_buffer(j_compress_ptr cinfo)
{
	ERREXIT(cinfo, JERR_BUFFER_SIZE);
	return TRUE;
}

static void dst_noop(j_compress_ptr cinfo)
{
}

static tjhandle _tjInitCompress(tjinstance *this)
{
	/* This is also straight out of example.c */
	this->cinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);
		return NULL;
	}

	jpeg_create_compress(&this->cinfo);
	this->cinfo.dest=&this->jdst;
	this->jdst.init_destination=dst_noop;
	this->jdst.empty_output_buffer=empty_output_buffer;
	this->jdst.term_destination=dst_noop;

	this->init|=COMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitCompress(void)
{
	tjinstance *this=NULL;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitCompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitCompress(this);
}


DLLEXPORT unsigned long DLLCALL tjBufSize(int width, int height,
	int jpegSubsamp)
{
	unsigned long retval=0;  int mcuw, mcuh, chromasf;
	if(width<1 || height<1 || jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT)
		_throw("tjBufSize(): Invalid argument");

	/*
	 * This allows for rare corner cases in which a JPEG image can actually be
	 * larger than the uncompressed input (we wouldn't mention it if it hadn't
	 * happened before.)
	 */
	mcuw=tjMCUWidth[jpegSubsamp];
	mcuh=tjMCUHeight[jpegSubsamp];
	chromasf=jpegSubsamp==TJSAMP_GRAY? 0: 4*64/(mcuw*mcuh);
	retval=PAD(width, mcuw) * PAD(height, mcuh) * (2 + chromasf) + 2048;

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height)
{
	unsigned long retval=0;
	if(width<1 || height<1)
		_throw("TJBUFSIZE(): Invalid argument");

	/*
	 * This allows for rare corner cases in which a JPEG image can actually be
	 * larger than the uncompressed input (we wouldn't mention it if it hadn't
	 * happened before.)
	 */
	retval=PAD(width, 16) * PAD(height, 16) * 6 + 2048;

	bailout:
	return retval;
}


DLLEXPORT int DLLCALL tjCompress2(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char **jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	#endif

	getinstance(handle)
	if((this->init&COMPRESS)==0)
		_throw("tjCompress2(): Instance has not been initialized for compression");

	if(srcBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF || jpegBuf==NULL || jpegSize==NULL
		|| jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT || jpegQual<0 || jpegQual>100)
		_throw("tjCompress2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY)
	{
		rgbBuf=(unsigned char *)malloc(width*height*RGB_PIXELSIZE);
		if(!rgbBuf) _throw("tjCompress2(): Memory allocation failure");
		srcBuf=toRGB(srcBuf, width, pitch, height, pixelFormat, rgbBuf);
		pitch=width*RGB_PIXELSIZE;
	}
	#endif

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setCompDefaults(cinfo, pixelFormat, jpegSubsamp, jpegQual)==-1)
		return -1;

	this->jdst.next_output_byte=*jpegBuf;
	this->jdst.free_in_buffer=tjBufSize(width, height, jpegSubsamp);

	jpeg_start_compress(cinfo, TRUE);
	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
		_throw("tjCompress2(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&srcBuf[(height-i-1)*pitch];
		else row_pointer[i]=&srcBuf[i*pitch];
	}
	while(cinfo->next_scanline<cinfo->image_height)
	{
		jpeg_write_scanlines(cinfo, &row_pointer[cinfo->next_scanline],
			cinfo->image_height-cinfo->next_scanline);
	}
	jpeg_finish_compress(cinfo);
 	*jpegSize=tjBufSize(width, height, jpegSubsamp)
		-(unsigned long)(this->jdst.free_in_buffer);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	return retval;
}

DLLEXPORT int DLLCALL tjCompress(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelSize, unsigned char *jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int retval=0;  unsigned long size = 0;
	retval=tjCompress2(handle, srcBuf, width, pitch, height,
		getPixelFormat(pixelSize, flags), &jpegBuf, &size, jpegSubsamp, jpegQual,
		flags);
	*jpegSize=size;
	return retval;
}


/* Decompressor */

static boolean fill_input_buffer(j_decompress_ptr dinfo)
{
	ERREXIT(dinfo, JERR_BUFFER_SIZE);
	return TRUE;
}

static void skip_input_data(j_decompress_ptr dinfo, long num_bytes)
{
	dinfo->src->next_input_byte += (size_t) num_bytes;
	dinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void src_noop(j_decompress_ptr dinfo)
{
}

static tjhandle _tjInitDecompress(tjinstance *this)
{
	/* This is also straight out of example.c */
	this->dinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);
		return NULL;
	}

	jpeg_create_decompress(&this->dinfo);
	this->dinfo.src=&this->jsrc;
	this->jsrc.init_source=src_noop;
	this->jsrc.fill_input_buffer=fill_input_buffer;
	this->jsrc.skip_input_data=skip_input_data;
	this->jsrc.resync_to_restart=jpeg_resync_to_restart;
	this->jsrc.term_source=src_noop;

	this->init|=DECOMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitDecompress(void)
{
	tjinstance *this;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitDecompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitDecompress(this);
}


DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height,
	int *jpegSubsamp)
{
	int retval=0;

	getinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompressHeader2(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || width==NULL || height==NULL
		|| jpegSubsamp==NULL)
		_throw("tjDecompressHeader2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		return -1;
	}

	this->jsrc.bytes_in_buffer=jpegSize;
	this->jsrc.next_input_byte=jpegBuf;
	jpeg_read_header(dinfo, TRUE);

	*width=dinfo->image_width;
	*height=dinfo->image_height;
	*jpegSubsamp=getSubsamp(dinfo);

	jpeg_abort_decompress(dinfo);

	if(*jpegSubsamp<0)
		_throw("tjDecompressHeader2(): Could not determine subsampling type for JPEG image");
	if(*width<1 || *height<1)
		_throw("tjDecompressHeader2(): Invalid data returned in header");

	bailout:
	return retval;
}

DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height)
{
	int jpegSubsamp;
	return tjDecompressHeader2(handle, jpegBuf, jpegSize, width, height,
		&jpegSubsamp);
}


DLLEXPORT tjscalingfactor* DLLCALL tjGetScalingFactors(int *numscalingfactors)
{
	if(numscalingfactors==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjGetScalingFactors(): Invalid argument");
		return NULL;
	}

	*numscalingfactors=NUMSF;
	return (tjscalingfactor *)sf;
}


DLLEXPORT int DLLCALL tjDecompress2(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelFormat, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	int jpegwidth, jpegheight, scaledw, scaledh;
	#ifndef JCS_EXTENSIONS
	unsigned char *rgbBuf=NULL;
	unsigned char *_dstBuf=NULL;  int _pitch=0;
	#endif

	getinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompress2(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || dstBuf==NULL || width<0 || pitch<0
		|| height<0 || pixelFormat<0 || pixelFormat>=TJ_NUMPF)
		_throw("tjDecompress2(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	this->jsrc.bytes_in_buffer=jpegSize;
	this->jsrc.next_input_byte=jpegBuf;
	jpeg_read_header(dinfo, TRUE);
	if(setDecompDefaults(dinfo, pixelFormat)==-1)
	{
		retval=-1;  goto bailout;
	}

	if(flags&TJFLAG_FASTUPSAMPLE) dinfo->do_fancy_upsampling=FALSE;

	jpegwidth=dinfo->image_width;  jpegheight=dinfo->image_height;
	if(width==0) width=jpegwidth;
	if(height==0) height=jpegheight;
	for(i=0; i<NUMSF; i++)
	{
		scaledw=TJSCALED(jpegwidth, sf[i]);
		scaledh=TJSCALED(jpegheight, sf[i]);
		if(scaledw<=width && scaledh<=height)
				break;
	}
	if(scaledw>width || scaledh>height)
		_throw("tjDecompress2(): Could not scale down to desired image dimensions");
	#ifndef JCS_EXTENSIONS
	width=scaledw;  height=scaledh;
	#endif
	dinfo->scale_num=sf[i].num;
	dinfo->scale_denom=sf[i].denom;

	jpeg_start_decompress(dinfo);
	if(pitch==0) pitch=dinfo->output_width*tjPixelSize[pixelFormat];

	#ifndef JCS_EXTENSIONS
	if(pixelFormat!=TJPF_GRAY &&
		(RGB_RED!=tjRedOffset[pixelFormat] ||
			RGB_GREEN!=tjGreenOffset[pixelFormat] ||
			RGB_BLUE!=tjBlueOffset[pixelFormat] ||
			RGB_PIXELSIZE!=tjPixelSize[pixelFormat]))
	{
		rgbBuf=(unsigned char *)malloc(width*height*3);
		if(!rgbBuf) _throw("tjDecompress2(): Memory allocation failure");
		_pitch=pitch;  pitch=width*3;
		_dstBuf=dstBuf;  dstBuf=rgbBuf;
	}
	#endif

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)
		*dinfo->output_height))==NULL)
		_throw("tjDecompress2(): Memory allocation failure");
	for(i=0; i<(int)dinfo->output_height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP)
			row_pointer[i]=&dstBuf[(dinfo->output_height-i-1)*pitch];
		else row_pointer[i]=&dstBuf[i*pitch];
	}
	while(dinfo->output_scanline<dinfo->output_height)
	{
		jpeg_read_scanlines(dinfo, &row_pointer[dinfo->output_scanline],
			dinfo->output_height-dinfo->output_scanline);
	}
	jpeg_finish_decompress(dinfo);

	#ifndef JCS_EXTENSIONS
	fromRGB(rgbBuf, _dstBuf, width, _pitch, height, pixelFormat);
	#endif

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	#ifndef JCS_EXTENSIONS
	if(rgbBuf) free(rgbBuf);
	#endif
	if(row_pointer) free(row_pointer);
	return retval;
}

DLLEXPORT int DLLCALL tjDecompress(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelSize, int flags)
{
	return tjDecompress2(handle, jpegBuf, jpegSize, dstBuf, width, pitch,
		height, getPixelFormat(pixelSize, flags), flags);
}
