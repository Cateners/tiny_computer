#include <rfb/rfb.h>


int main(int argc,char** argv)
{
  int i;
  uint8_t bytes[256*3];

  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,256,256,8,1,1);
  if(!server)
    return 1;
  server->serverFormat.trueColour=FALSE;
  server->colourMap.count=256;
  server->colourMap.is16=FALSE;
  for(i=0;i<256;i++) {
    bytes[i*3+0]=255-i; /* red */
    bytes[i*3+1]=0; /* green */
    bytes[i*3+2]=i; /* blue */
  }
  bytes[128*3+0]=0xff;
  bytes[128*3+1]=0;
  bytes[128*3+2]=0;
  server->colourMap.data.bytes=bytes;

  server->frameBuffer=(char*)malloc(256*256);
  if(!server->frameBuffer)
      return 1;

  for(i=0;i<256*256;i++)
     server->frameBuffer[i]=(i/256);

  rfbInitServer(server);
  rfbRunEventLoop(server,-1,FALSE);

  return(0);
}
