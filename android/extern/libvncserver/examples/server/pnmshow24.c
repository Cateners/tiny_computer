/**
   @example pnmshow24.c
   pnmshow24 is like pnmshow, but it uses 3 bytes/pixel internally, which is not
   as efficient as 4 bytes/pixel for translation, because there is no native data
   type of that size, so you have to memcpy pixels and be real cautious with
   endianness. Anyway, it works.
 */

#include <stdio.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>

#ifndef LIBVNCSERVER_ALLOW24BPP
int main() {
	printf("I need the ALLOW24BPP LibVNCServer flag to work\n");
	exit(1);
}
#else

static void HandleKey(rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
  if(down && (key==XK_Escape || key=='q' || key=='Q'))
    rfbCloseClient(cl);
}

int main(int argc,char** argv)
{
  FILE* in=stdin;
  int j,width,height,paddedWidth;
  char buffer[1024];
  rfbScreenInfoPtr rfbScreen;

  if(argc>1) {
    in=fopen(argv[1],"rb");
    if(!in) {
      printf("Couldn't find file %s.\n",argv[1]);
      exit(1);
    }
  }

  fgets(buffer,1024,in);
  if(strncmp(buffer,"P6",2)) {
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
  fgets(buffer,1024,in);

  /* vncviewers have problems with widths which are no multiple of 4. */
  paddedWidth = width;

  /* if your vncviewer doesn't have problems with a width
     which is not a multiple of 4, you can comment this. */
  if(width&3)
    paddedWidth+=4-(width&3);

  /* initialize data for vnc server */
  rfbScreen = rfbGetScreen(&argc,argv,paddedWidth,height,8,3,3);
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
  rfbScreen->frameBuffer = (char*)malloc(paddedWidth*3*height);
  if(!rfbScreen->frameBuffer)
    return 1;
  fread(rfbScreen->frameBuffer,width*3,height,in);
  fclose(in);

  /* pad to paddedWidth */
  if(width != paddedWidth) {
    int padCount = 3*(paddedWidth - width);
    for(j=height-1;j>=0;j--) {
      memmove(rfbScreen->frameBuffer+3*paddedWidth*j,
	      rfbScreen->frameBuffer+3*width*j,
	      3*width);
      memset(rfbScreen->frameBuffer+3*paddedWidth*(j+1)-padCount,
	     0,padCount);
    }
  }

  /* initialize server */
  rfbInitServer(rfbScreen);

  /* run event loop */
  rfbRunEventLoop(rfbScreen,40000,FALSE);

  return(0);
}
#endif
