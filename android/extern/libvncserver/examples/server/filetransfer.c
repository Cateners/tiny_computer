/**
 * @example filetransfer.c
 * Demonstrates a server capable of TightVNC-1.3.x file transfer.
 * NB That TightVNC-2.x uses a different, incompatible file transfer protocol.
 */

#include <rfb/rfb.h>

int main(int argc,char** argv)
{                                                                
  rfbRegisterTightVNCFileTransferExtension();
  rfbScreenInfoPtr server=rfbGetScreen(&argc,argv,400,300,8,3,4);
  if(!server)
    return 1;
  server->frameBuffer=(char*)malloc(400*300*4);
  rfbInitServer(server);           
  rfbRunEventLoop(server,-1,FALSE);
  return(0);
}
