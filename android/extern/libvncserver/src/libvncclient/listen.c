/*
 *  Copyright (C) 2011-2012 Christian Beier <dontmind@freeshell.org>
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * listen.c - listen for incoming connections
 */

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#endif
#if LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else // #ifdef WIN32
#include <sys/wait.h>
#include <sys/utsname.h>
#endif
#if LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <rfb/rfbclient.h>

/*
 * listenForIncomingConnections() - listen for incoming connections from
 * servers, and fork a new process to deal with each connection.
 */

void
listenForIncomingConnections(rfbClient* client)
{
#ifdef WIN32
  /* FIXME */
  rfbClientErr("listenForIncomingConnections on MinGW32 NOT IMPLEMENTED\n");
  return;
#else
  int listenSocket = RFB_INVALID_SOCKET, listen6Socket = RFB_INVALID_SOCKET;
  fd_set fds;

  client->listenSpecified = TRUE;

  listenSocket = ListenAtTcpPortAndAddress(client->listenPort, client->listenAddress);

  if (listenSocket == RFB_INVALID_SOCKET)
    return;

  rfbClientLog("%s -listen: Listening on port %d\n",
	  client->programName,client->listenPort);
  rfbClientLog("%s -listen: Command line errors are not reported until "
	  "a connection comes in.\n", client->programName);

#ifdef LIBVNCSERVER_IPv6 /* only try that if we're IPv6-capable, otherwise we may try to bind to the same port which would make all that listening fail */ 
  /* only do IPv6 listen of listen6Port is set */
  if (client->listen6Port != RFB_INVALID_SOCKET)
    {
      listen6Socket = ListenAtTcpPortAndAddress(client->listen6Port, client->listen6Address);

      if (listen6Socket == RFB_INVALID_SOCKET)
	return;

      rfbClientLog("%s -listen: Listening on IPV6 port %d\n",
		   client->programName,client->listenPort);
      rfbClientLog("%s -listen: Command line errors are not reported until "
		   "a connection comes in.\n", client->programName);
    }
#endif

  while (TRUE) {
    int r;
    /* reap any zombies */
    int status, pid;
    while ((pid= wait4(-1, &status, WNOHANG, (struct rusage *)0))>0);

    /* TODO: callback for discard any events (like X11 events) */

    FD_ZERO(&fds); 

    if(listenSocket != RFB_INVALID_SOCKET)
      FD_SET(listenSocket, &fds);  
    if(listen6Socket != RFB_INVALID_SOCKET)
      FD_SET(listen6Socket, &fds);

    r = select(rfbMax(listenSocket, listen6Socket)+1, &fds, NULL, NULL, NULL);

    if (r > 0) {
      if (FD_ISSET(listenSocket, &fds))
	client->sock = AcceptTcpConnection(client->listenSock); 
      else if (FD_ISSET(listen6Socket, &fds))
	client->sock = AcceptTcpConnection(client->listen6Sock);

      if (client->sock == RFB_INVALID_SOCKET)
	return;
      if (!SetNonBlocking(client->sock))
	return;

      /* Now fork off a new process to deal with it... */

      switch (fork()) {

      case -1: 
	rfbClientErr("fork\n"); 
	return;

      case 0:
	/* child - return to caller */
	rfbCloseSocket(listenSocket);
	rfbCloseSocket(listen6Socket);
	return;

      default:
	/* parent - go round and listen again */
	rfbCloseSocket(client->sock);
	break;
      }
    }
  }
#endif
}



/*
 * listenForIncomingConnectionsNoFork() - listen for incoming connections
 * from servers, but DON'T fork, instead just wait timeout microseconds.
 * If timeout is negative, block indefinitely.
 * Returns 1 on success (there was an incoming connection on the listen socket
 * and we accepted it successfully), -1 on error, 0 on timeout.
 */

int
listenForIncomingConnectionsNoFork(rfbClient* client, int timeout)
{
  fd_set fds;
  struct timeval to;
  int r;

  to.tv_sec= timeout / 1000000;
  to.tv_usec= timeout % 1000000;

  client->listenSpecified = TRUE;

  if (client->listenSock == RFB_INVALID_SOCKET)
    {
      client->listenSock = ListenAtTcpPortAndAddress(client->listenPort, client->listenAddress);

      if (client->listenSock == RFB_INVALID_SOCKET)
	return -1;

      rfbClientLog("%s -listennofork: Listening on port %d\n",
		   client->programName,client->listenPort);
      rfbClientLog("%s -listennofork: Command line errors are not reported until "
		   "a connection comes in.\n", client->programName);
    }

#ifdef LIBVNCSERVER_IPv6 /* only try that if we're IPv6-capable, otherwise we may try to bind to the same port which would make all that listening fail */ 
  /* only do IPv6 listen of listen6Port is set */
  if (client->listen6Port != RFB_INVALID_SOCKET && client->listen6Sock == RFB_INVALID_SOCKET)
    {
      client->listen6Sock = ListenAtTcpPortAndAddress(client->listen6Port, client->listen6Address);

      if (client->listen6Sock == RFB_INVALID_SOCKET)
	return -1;

      rfbClientLog("%s -listennofork: Listening on IPV6 port %d\n",
		   client->programName,client->listenPort);
      rfbClientLog("%s -listennofork: Command line errors are not reported until "
		   "a connection comes in.\n", client->programName);
    }
#endif

  FD_ZERO(&fds);

  if(client->listenSock != RFB_INVALID_SOCKET)
    FD_SET(client->listenSock, &fds);
  if(client->listen6Sock != RFB_INVALID_SOCKET)
    FD_SET(client->listen6Sock, &fds);

  if (timeout < 0)
    r = select(rfbMax(client->listenSock, client->listen6Sock) +1, &fds, NULL, NULL, NULL);
  else
    r = select(rfbMax(client->listenSock, client->listen6Sock) +1, &fds, NULL, NULL, &to);

  if (r > 0)
    {
      if (FD_ISSET(client->listenSock, &fds))
	client->sock = AcceptTcpConnection(client->listenSock); 
      else if (FD_ISSET(client->listen6Sock, &fds))
	client->sock = AcceptTcpConnection(client->listen6Sock);

      if (client->sock == RFB_INVALID_SOCKET)
	return -1;
      if (!SetNonBlocking(client->sock))
	return -1;

      if(client->listenSock != RFB_INVALID_SOCKET) {
	rfbCloseSocket(client->listenSock);
	client->listenSock = RFB_INVALID_SOCKET;
      }
      if(client->listen6Sock != RFB_INVALID_SOCKET) {
	rfbCloseSocket(client->listen6Sock);
	client->listen6Sock = RFB_INVALID_SOCKET;
      }
      return r;
    }

  /* r is now either 0 (timeout) or -1 (error) */
  return r;
}


