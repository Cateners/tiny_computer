/**
* @example pnmshow.c
*/
#include <stdio.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>

#ifndef HAVE_HANDLEKEY
static void HandleKey(rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
  if(down && (key==XK_Escape || key=='q' || key=='Q'))
    rfbCloseClient(cl);
}
#endif

int main(int argc,char** argv)
{
  FILE* in=stdin;
  int i,j,k,l,width,height,paddedWidth;
  char buffer[1024];
  rfbScreenInfoPtr rfbScreen;
  enum { BW, GRAY, TRUECOLOUR } picType=TRUECOLOUR;
  int bytesPerPixel,bitsPerPixelInFile;

  if(argc>1) {
    in=fopen(argv[1],"rb");
    if(!in) {
      printf("Couldn't find file %s.\n",argv[1]);
      exit(1);
    }
  }

  fgets(buffer,1024,in);
  if(!strncmp(buffer,"P6",2)) {
	  picType=TRUECOLOUR;
	  bytesPerPixel=4; bitsPerPixelInFile=3*8;
  } else if(!strncmp(buffer,"P5",2)) {
	  picType=GRAY;
	  bytesPerPixel=1; bitsPerPixelInFile=1*8;
  } else if(!strncmp(buffer,"P4",2)) {
	  picType=BW;
	  bytesPerPixel=1; bitsPerPixelInFile=1;
  } else {
    printf("Not a ppm.\n");
    exit(2);
  }

  /* skip comments */
  do {
    fgets(buffer,1024,in);
  } while(buffer[0]=='#');

  /* get width & height */
  sscanf(buffer,"%d %d",&width,&height);
  rfbLog("Got width %d and height %d.\n",width,height);
  if(picType!=BW)
	fgets(buffer,1024,in);
  else
	  width=1+((width-1)|7);

  /* vncviewers have problems with widths which are no multiple of 4. */
  paddedWidth = width;
  if(width&3)
    paddedWidth+=4-(width&3);

  /* initialize data for vnc server */
  rfbScreen = rfbGetScreen(&argc,argv,paddedWidth,height,8,(bitsPerPixelInFile+7)/8,bytesPerPixel);
  if(!rfbScreen)
    return 1;
  if(argc>1)
    rfbScreen->desktopName = argv[1];
  else
    rfbScreen->desktopName = "Picture";
  rfbScreen->alwaysShared = TRUE;
  rfbScreen->kbdAddEvent = HandleKey;

  /* enable http */
  rfbScreen->httpDir = "../webclients";

  /* allocate picture and read it */
  rfbScreen->frameBuffer = (char*)malloc(paddedWidth*bytesPerPixel*height);
  if(!rfbScreen->frameBuffer)
      exit(1);
  fread(rfbScreen->frameBuffer,width*bitsPerPixelInFile/8,height,in);
  fclose(in);

  if(picType!=TRUECOLOUR) {
	  rfbScreen->serverFormat.trueColour=FALSE;
	  rfbScreen->colourMap.count=256;
	  rfbScreen->colourMap.is16=FALSE;
	  rfbScreen->colourMap.data.bytes=malloc(256*3);
	  if(!rfbScreen->colourMap.data.bytes)
		  exit(1);
	  for(i=0;i<256;i++)
		  memset(rfbScreen->colourMap.data.bytes+3*i,i,3);
  }

  switch(picType) {
	case TRUECOLOUR:
		  /* correct the format to 4 bytes instead of 3 (and pad to paddedWidth) */
		  for(j=height-1;j>=0;j--) {
		    for(i=width-1;i>=0;i--)
		      for(k=2;k>=0;k--)
			rfbScreen->frameBuffer[(j*paddedWidth+i)*4+k]=
			  rfbScreen->frameBuffer[(j*width+i)*3+k];
		    for(i=width*4;i<paddedWidth*4;i++)
		      rfbScreen->frameBuffer[j*paddedWidth*4+i]=0;
		  }
		  break;
	case GRAY:
		  break;
	case BW:
		  /* correct the format from 1 bit to 8 bits */
		  for(j=height-1;j>=0;j--)
			  for(i=width-1;i>=0;i-=8) {
				  l=(unsigned char)rfbScreen->frameBuffer[(j*width+i)/8];
				  for(k=7;k>=0;k--)
					  rfbScreen->frameBuffer[j*paddedWidth+i+7-k]=(l&(1<<k))?0:255;
			  }
		  break;
  }

  /* initialize server */
  rfbInitServer(rfbScreen);

  /* run event loop */
  rfbRunEventLoop(rfbScreen,40000,FALSE);

  return(0);
}
