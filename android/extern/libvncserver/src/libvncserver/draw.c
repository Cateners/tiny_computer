#include <rfb/rfb.h>

void rfbFillRect(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col)
{
  int rowstride = s->paddedWidthInBytes, bpp = s->bitsPerPixel>>3;
  int i,j;
  char* colour=(char*)&col;

   if(!rfbEndianTest)
    colour += 4-bpp;
  for(j=y1;j<y2;j++)
    for(i=x1;i<x2;i++)
      memcpy(s->frameBuffer+j*rowstride+i*bpp,colour,bpp);
  rfbMarkRectAsModified(s,x1,y1,x2,y2);
}

#define SETPIXEL(x,y) \
  memcpy(s->frameBuffer+(y)*rowstride+(x)*bpp,colour,bpp)

void rfbDrawPixel(rfbScreenInfoPtr s,int x,int y,rfbPixel col)
{
  int rowstride = s->paddedWidthInBytes, bpp = s->bitsPerPixel>>3;
  char* colour=(char*)&col;

  if(!rfbEndianTest)
    colour += 4-bpp;
  SETPIXEL(x,y);
  rfbMarkRectAsModified(s,x,y,x+1,y+1);
}

void rfbDrawLine(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col)
{
  int rowstride = s->paddedWidthInBytes, bpp = s->bitsPerPixel>>3;
  int i;
  char* colour=(char*)&col;

  if(!rfbEndianTest)
    colour += 4-bpp;

#define SWAPPOINTS { i=x1; x1=x2; x2=i; i=y1; y1=y2; y2=i; }
  if(abs(x1-x2)<abs(y1-y2)) {
    if(y1>y2)
      SWAPPOINTS
    for(i=y1;i<=y2;i++)
      SETPIXEL(x1+(i-y1)*(x2-x1)/(y2-y1),i);
    /* TODO: Maybe make this more intelligently? */
    if(x2<x1) { i=x1; x1=x2; x2=i; }
    rfbMarkRectAsModified(s,x1,y1,x2+1,y2+1);
  } else {
    if(x1>x2)
      SWAPPOINTS
    else if(x1==x2) {
      rfbDrawPixel(s,x1,y1,col);
      return;
    }
    for(i=x1;i<=x2;i++)
      SETPIXEL(i,y1+(i-x1)*(y2-y1)/(x2-x1));
    if(y2<y1) { i=y1; y1=y2; y2=i; }
    rfbMarkRectAsModified(s,x1,y1,x2+1,y2+1);
  }
}
