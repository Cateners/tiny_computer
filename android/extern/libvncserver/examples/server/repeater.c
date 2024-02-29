/**
    This example shows how to connect to an UltraVNC repeater.

    To get you started, you will need the actual repeater.
    Here's a non-exhaustive link list, some have install instructions,
    some don't:

    * The official UltraVNC repeater for Windows: https://uvnc.com/downloads/repeater/83-repeater-downloads.html
    * The Linux port of the UltraVNC repeater, linked (but not made) by TurboVNC: https://turbovnc.org/Documentation/UltraVNCRepeater
    * An enhanced versions of x11vnc's Perl implementation: https://github.com/tomka/ultravnc-repeater

    After installing and running, you simply connect this server example via
    `./repeater <id> <repeater-host> [<repeater-port>]`, where 'id' is a number.

    For an UltraVNC repeater, the server will then show up in the
    "Waiting servers" list of the repeater's web interface.

    To connect with say the example SDLvncviewer, run
    `./SDLvncviewer -repeaterdest ID:<id> <repeater-host>:[<repeater-port>]`
    where 'id' is the same number you used with the server example above.

    If the ids match, the repeater will then forward packets in between
    the connected server and client. That's it!

 */

#include <rfb/rfb.h>

static void clientGone(rfbClientPtr cl)
{
  rfbShutdownServer(cl->screen, TRUE);
}

int main(int argc,char** argv)
{
  char *repeaterHost;
  int repeaterPort, sock;
  char id[250];
  rfbClientPtr cl;

  int i,j;
  uint16_t* f;

  /* Parse command-line arguments */
  if (argc < 3) {
    fprintf(stderr,
      "Usage: %s <id> <repeater-host> [<repeater-port>]\n", argv[0]);
    exit(1);
  }
  memset(id, 0, sizeof(id));
  if(snprintf(id, sizeof(id), "ID:%s", argv[1]) >= (int)sizeof(id)) {
      /* truncated! */
      fprintf(stderr, "Error, given ID is too long.\n");
      return 1;
  }
  repeaterHost = argv[2];
  repeaterPort = argc < 4 ? 5500 : atoi(argv[3]);

  /* The initialization is identical to simple15.c */
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

  /* Now for the repeater-specific part: */
  server->port = -1; /* do not listen on any port */
  server->ipv6port = -1; /* do not listen on any port */

  /* Make sure to call this _before_ connecting out to the repeater */
  rfbInitServer(server);

  sock = rfbConnectToTcpAddr(repeaterHost, repeaterPort);
  if (sock == RFB_INVALID_SOCKET) {
    perror("connect to repeater");
    return 1;
  }
  if (send(sock, id, sizeof(id),0) != sizeof(id)) {
    perror("writing id");
    return 1;
  }
  cl = rfbNewClient(server, sock);
  if (!cl) {
    perror("new client");
    return 1;
  }
  cl->reverseConnection = 0;
  cl->clientGoneHook = clientGone;

  /* Run the server */
  rfbRunEventLoop(server,-1,FALSE);

  return 0;
}
