/* This example shows how to use 15-bit (which is handled as 16-bit
   internally). */

#include <rfb/rfb.h>

int main(int argc,char** argv)
{                                       
  int i,j;
  uint16_t* f;
                         
  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,5,3,2);
  if(!server)
    return 1;
  server->frameBuffer=(char*)malloc(400*300*2);
  if(!server->frameBuffer)
    return 1;
  f=(uint16_t*)server->frameBuffer;
  for(j=0;j<300;j++)
    for(i=0;i<400;i++)
      f[j*400+i]=/* red */ ((j*32/300) << 10) |
		 /* green */ (((j+400-i)*32/700) << 5) |
		 /* blue */ (i*32/400);

  rfbInitServer(server);           
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
