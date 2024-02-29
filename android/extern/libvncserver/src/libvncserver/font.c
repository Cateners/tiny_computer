#include <rfb/rfb.h>

int rfbDrawChar(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
		 int x,int y,unsigned char c,rfbPixel col)
{
  int i,j,width,height;
  unsigned char* data=font->data+font->metaData[c*5];
  unsigned char d=*data;
  int rowstride=rfbScreen->paddedWidthInBytes;
  int bpp=rfbScreen->serverFormat.bitsPerPixel/8;
  char *colour=(char*)&col;

  if(!rfbEndianTest)
    colour += 4-bpp;

  width=font->metaData[c*5+1];
  height=font->metaData[c*5+2];
  x+=font->metaData[c*5+3];
  y+=-font->metaData[c*5+4]-height+1;

  for(j=0;j<height;j++) {
    for(i=0;i<width;i++) {
      if((i&7)==0) {
	d=*data;
	data++;
      }
      if(d&0x80 && y+j >= 0 && y+j < rfbScreen->height &&
          x+i >= 0 && x+i < rfbScreen->width)
	memcpy(rfbScreen->frameBuffer+(y+j)*rowstride+(x+i)*bpp,colour,bpp);
      d<<=1;
    }
    /* if((i&7)!=0) data++; */
  }
  return(width);
}

void rfbDrawString(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
		   int x,int y,const char* string,rfbPixel colour)
{
  while(*string) {
    x+=rfbDrawChar(rfbScreen,font,x,y,*string,colour);
    string++;
  }
}

/* TODO: these two functions need to be more efficient */
/* if col==bcol, assume transparent background */
int rfbDrawCharWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
			int x,int y,unsigned char c,
			int x1,int y1,int x2,int y2,
			rfbPixel col,rfbPixel bcol)
{
  int i,j,width,height;
  unsigned char* data=font->data+font->metaData[c*5];
  unsigned char d;
  int rowstride=rfbScreen->paddedWidthInBytes;
  int bpp=rfbScreen->serverFormat.bitsPerPixel/8,extra_bytes=0;
  char* colour=(char*)&col;
  char* bcolour=(char*)&bcol;

  if(!rfbEndianTest) {
    colour+=4-bpp;
    bcolour+=4-bpp;
  }

  width=font->metaData[c*5+1];
  height=font->metaData[c*5+2];
  x+=font->metaData[c*5+3];
  y+=-font->metaData[c*5+4]-height+1;

  /* after clipping, x2 will be count of bytes between rows,
   * x1 start of i, y1 start of j, width and height will be adjusted. */
  if(y1>y) { y1-=y; data+=(width+7)/8; height-=y1; y+=y1; } else y1=0;
  if(x1>x) { x1-=x; data+=x1; width-=x1; x+=x1; extra_bytes+=x1/8; } else x1=0;
  if(y2<y+height) height-=y+height-y2;
  if(x2<x+width) { extra_bytes+=(x1+width)/8-(x+width-x2+7)/8; width-=x+width-x2; }

  d=*data;
  for(j=y1;j<height;j++) {
    if((x1&7)!=0)
      d=data[-1]; /* TODO: check if in this case extra_bytes is correct! */
    for(i=x1;i<width;i++) {
      if((i&7)==0) {
	d=*data;
	data++;
      }
      /* if(x+i>=x1 && x+i<x2 && y+j>=y1 && y+j<y2) */ {
	 if(d&0x80) {
	   memcpy(rfbScreen->frameBuffer+(y+j)*rowstride+(x+i)*bpp,
		  colour,bpp);
	 } else if(bcol!=col) {
	   memcpy(rfbScreen->frameBuffer+(y+j)*rowstride+(x+i)*bpp,
		  bcolour,bpp);
	 }
      }
      d<<=1;
    }
    /* if((i&7)==0) data++; */
    data += extra_bytes;
  }
  return(width);
}

void rfbDrawStringWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
			   int x,int y,const char* string,
			   int x1,int y1,int x2,int y2,
			   rfbPixel colour,rfbPixel backColour)
{
  while(*string) {
    x+=rfbDrawCharWithClip(rfbScreen,font,x,y,*string,x1,y1,x2,y2,
			   colour,backColour);
    string++;
  }
}

int rfbWidthOfString(rfbFontDataPtr font,const char* string)
{
  int i=0;
  while(*string) {
    i+=font->metaData[*string*5+1];
    string++;
  }
  return(i);
}

int rfbWidthOfChar(rfbFontDataPtr font,unsigned char c)
{
  return(font->metaData[c*5+1]+font->metaData[c*5+3]);
}

void rfbFontBBox(rfbFontDataPtr font,unsigned char c,int* x1,int* y1,int* x2,int* y2)
{
  *x1+=font->metaData[c*5+3];
  *y1+=-font->metaData[c*5+4]-font->metaData[c*5+2]+1;
  *x2=*x1+font->metaData[c*5+1]+1;
  *y2=*y1+font->metaData[c*5+2]+1;
}

#ifndef INT_MAX
#define INT_MAX 0x7fffffff
#endif

void rfbWholeFontBBox(rfbFontDataPtr font,
		      int *x1, int *y1, int *x2, int *y2)
{
   int i;
   int* m=font->metaData;
   
   (*x1)=(*y1)=INT_MAX; (*x2)=(*y2)=1-(INT_MAX);
   for(i=0;i<256;i++) {
      if(m[i*5+1]-m[i*5+3]>(*x2))
	(*x2)=m[i*5+1]-m[i*5+3];
      if(-m[i*5+2]+m[i*5+4]<(*y1))
	(*y1)=-m[i*5+2]+m[i*5+4];
      if(m[i*5+3]<(*x1))
	(*x1)=m[i*5+3];
      if(-m[i*5+4]>(*y2))
	(*y2)=-m[i*5+4];
   }
   (*x2)++;
   (*y2)++;
}

rfbFontDataPtr rfbLoadConsoleFont(char *filename)
{
  FILE *f=fopen(filename,"rb");
  rfbFontDataPtr p;
  int i;

  if(!f) return NULL;

  p=(rfbFontDataPtr)malloc(sizeof(rfbFontData));
  if(!p) {
      fclose(f);
      return NULL;
  }

  p->data=(unsigned char*)malloc(4096);
  p->metaData=(int*)malloc(256*5*sizeof(int));
  if(!p->data || !p->metaData || 1!=fread(p->data,4096,1,f)) {
    free(p->data);
    free(p->metaData);
    free(p);
    fclose(f);
    return NULL;
  }
  fclose(f);
  for(i=0;i<256;i++) {
    p->metaData[i*5+0]=i*16; /* offset */
    p->metaData[i*5+1]=8; /* width */
    p->metaData[i*5+2]=16; /* height */
    p->metaData[i*5+3]=0; /* xhot */
    p->metaData[i*5+4]=0; /* yhot */
  }
  return(p);
}

void rfbFreeFont(rfbFontDataPtr f)
{
  free(f->data);
  free(f->metaData);
  free(f);
}
