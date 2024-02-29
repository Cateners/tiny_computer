/*
 * sockets.c - deal with TCP & UDP sockets.
 *
 * This code should be independent of any changes in the RFB protocol.  It just
 * deals with the X server scheduling stuff, calling rfbNewClientConnection and
 * rfbProcessClientMessage to actually deal with the protocol.  If a socket
 * needs to be closed for any reason then rfbCloseClient should be called. In turn,
 * rfbClientConnectionGone will be called by rfbProcessEvents (non-threaded case)
 * or clientInput (threaded case) in main.c.  To make an active
 * connection out, call rfbConnect - note that this does _not_ call
 * rfbNewClientConnection.
 *
 * This file is divided into two types of function.  Those beginning with
 * "rfb" are specific to sockets using the RFB protocol.  Those without the
 * "rfb" prefix are more general socket routines (which are used by the http
 * code).
 *
 * Thanks to Karl Hakimian for pointing out that some platforms return EAGAIN
 * not EWOULDBLOCK.
 */

/*
 *  Copyright (C) 2011-2012 Christian Beier <dontmind@freeshell.org>
 *  Copyright (C) 2005 Rohit Kumar, Johannes E. Schindelin
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
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

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#ifdef __linux__
/* Setting this on other systems hides definitions such as INADDR_LOOPBACK.
 * The check should be for __GLIBC__ in fact. */
# define _POSIX_SOURCE
#endif
#endif

#include <rfb/rfb.h>

#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef LIBVNCSERVER_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
#include "rfbssl.h"
#endif

#ifdef LIBVNCSERVER_WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval 
{
   long int tv_sec,tv_usec;
}
;
#endif

#ifdef LIBVNCSERVER_HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <errno.h>

#ifdef USE_LIBWRAP
#include <syslog.h>
#include <tcpd.h>
int allow_severity=LOG_INFO;
int deny_severity=LOG_WARNING;
#endif

#include "sockets.h"

int rfbMaxClientWait = 20000;   /* time (ms) after which we decide client has
                                   gone away - needed to stop us hanging */

static rfbBool
rfbNewConnectionFromSock(rfbScreenInfoPtr rfbScreen, rfbSocket sock)
{
    const int one = 1;
#ifdef LIBVNCSERVER_IPv6
    struct sockaddr_storage addr;
#else
    struct sockaddr_in addr;
#endif
    socklen_t addrlen = sizeof(addr);

    getpeername(sock, (struct sockaddr *)&addr, &addrlen);

    if(!rfbSetNonBlocking(sock)) {
      rfbLogPerror("rfbCheckFds: setnonblock");
      rfbCloseSocket(sock);
      return FALSE;
    }

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		   (const char *)&one, sizeof(one)) < 0) {
      rfbLogPerror("rfbCheckFds: setsockopt failed: can't set TCP_NODELAY flag, non TCP socket?");
    }

#ifdef USE_LIBWRAP
    if(!hosts_ctl("vnc",STRING_UNKNOWN,inet_ntoa(addr.sin_addr),
		  STRING_UNKNOWN)) {
      rfbLog("Rejected connection from client %s\n",
	     inet_ntoa(addr.sin_addr));
      rfbCloseSocket(sock);
      return FALSE;
    }
#endif

#ifdef LIBVNCSERVER_IPv6
    char host[1024];
    if(getnameinfo((struct sockaddr*)&addr, addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST) != 0) {
      rfbLogPerror("rfbProcessNewConnection: error in getnameinfo");
    } else {
      rfbLog("Got connection from client %s\n", host);
    }
#else
    rfbLog("Got connection from client %s\n", inet_ntoa(addr.sin_addr));
#endif

    rfbNewClient(rfbScreen,sock);
    return TRUE;
}

/*
 * rfbInitSockets sets up the TCP and UDP sockets to listen for RFB
 * connections.  It does nothing if called again.
 */

void
rfbInitSockets(rfbScreenInfoPtr rfbScreen)
{
#ifdef WIN32
    WSADATA unused;
#endif

    in_addr_t iface = rfbScreen->listenInterface;

    if (rfbScreen->socketState == RFB_SOCKET_READY) {
        return;
    }

#ifdef WIN32
    if((errno = WSAStartup(MAKEWORD(2,0), &unused)) != 0) {
	rfbLogPerror("Could not init Windows Sockets\n");
	return;
    }
#endif

    rfbScreen->socketState = RFB_SOCKET_READY;

#ifdef LIBVNCSERVER_WITH_SYSTEMD
    if (sd_listen_fds(0) == 1)
    {
        rfbSocket sock = SD_LISTEN_FDS_START + 0;
        if (sd_is_socket(sock, AF_UNSPEC, 0, 0))
            rfbNewConnectionFromSock(rfbScreen, sock);
        else if (sd_is_socket(sock, AF_UNSPEC, 0, 1))
            rfbProcessNewConnection(rfbScreen);
        return;
    }
    else
        rfbLog("Unable to establish connection with systemd socket\n");
#endif

    if (rfbScreen->inetdSock != RFB_INVALID_SOCKET) {
	const int one = 1;

        if(!rfbSetNonBlocking(rfbScreen->inetdSock))
	    return;

	if (setsockopt(rfbScreen->inetdSock, IPPROTO_TCP, TCP_NODELAY,
		       (const char *)&one, sizeof(one)) < 0) {
	    rfbLogPerror("setsockopt failed: can't set TCP_NODELAY flag, non TCP socket?");
	}

    	FD_ZERO(&(rfbScreen->allFds));
    	FD_SET(rfbScreen->inetdSock, &(rfbScreen->allFds));
    	rfbScreen->maxFd = rfbScreen->inetdSock;
	return;
    }

    FD_ZERO(&(rfbScreen->allFds));

    if(rfbScreen->autoPort && rfbScreen->port>0) {
        int i;
        rfbLog("Autoprobing TCP port \n");
        for (i = 5900; i < 6000; i++) {
            if ((rfbScreen->listenSock = rfbListenOnTCPPort(i, iface)) != RFB_INVALID_SOCKET) {
		rfbScreen->port = i;
		break;
	    }
        }

        if (i >= 6000) {
	    rfbLogPerror("Failure autoprobing");
	    return;
        }

        rfbLog("Autoprobing selected TCP port %d\n", rfbScreen->port);
        FD_SET(rfbScreen->listenSock, &(rfbScreen->allFds));
        rfbScreen->maxFd = rfbScreen->listenSock;
    }

#ifdef LIBVNCSERVER_IPv6
    if(rfbScreen->autoPort && rfbScreen->ipv6port>0) {
        int i;
        rfbLog("Autoprobing TCP6 port \n");
	for (i = 5900; i < 6000; i++) {
            if ((rfbScreen->listen6Sock = rfbListenOnTCP6Port(i, rfbScreen->listen6Interface)) != RFB_INVALID_SOCKET) {
		rfbScreen->ipv6port = i;
		break;
	    }
        }

        if (i >= 6000) {
	    rfbLogPerror("Failure autoprobing");
	    return;
        }

        rfbLog("Autoprobing selected TCP6 port %d\n", rfbScreen->ipv6port);
	FD_SET(rfbScreen->listen6Sock, &(rfbScreen->allFds));
	rfbScreen->maxFd = rfbMax((int)rfbScreen->listen6Sock,rfbScreen->maxFd);
    }
#endif

    if(!rfbScreen->autoPort) {
	    if(rfbScreen->port>0) {

      if ((rfbScreen->listenSock = rfbListenOnTCPPort(rfbScreen->port, iface)) == RFB_INVALID_SOCKET) {
	rfbLogPerror("ListenOnTCPPort");
	return;
      }
      rfbLog("Listening for VNC connections on TCP port %d\n", rfbScreen->port);  
  
      FD_SET(rfbScreen->listenSock, &(rfbScreen->allFds));
      rfbScreen->maxFd = rfbScreen->listenSock;
	    }

#ifdef LIBVNCSERVER_IPv6
	    if (rfbScreen->ipv6port>0) {
      if ((rfbScreen->listen6Sock = rfbListenOnTCP6Port(rfbScreen->ipv6port, rfbScreen->listen6Interface)) == RFB_INVALID_SOCKET) {
	/* ListenOnTCP6Port has its own detailed error printout */
	return;
      }
      rfbLog("Listening for VNC connections on TCP6 port %d\n", rfbScreen->ipv6port);  
	
      FD_SET(rfbScreen->listen6Sock, &(rfbScreen->allFds));
      rfbScreen->maxFd = rfbMax((int)rfbScreen->listen6Sock,rfbScreen->maxFd);
	    }
#endif

    }

    if (rfbScreen->udpPort != 0) {
	rfbLog("rfbInitSockets: listening for input on UDP port %d\n",rfbScreen->udpPort);

	if ((rfbScreen->udpSock = rfbListenOnUDPPort(rfbScreen->udpPort, iface)) == RFB_INVALID_SOCKET) {
	    rfbLogPerror("ListenOnUDPPort");
	    return;
	}
	rfbLog("Listening for VNC connections on TCP port %d\n", rfbScreen->port);  

	FD_SET(rfbScreen->udpSock, &(rfbScreen->allFds));
	rfbScreen->maxFd = rfbMax((int)rfbScreen->udpSock,rfbScreen->maxFd);
    }
}

void rfbShutdownSockets(rfbScreenInfoPtr rfbScreen)
{
    if (rfbScreen->socketState!=RFB_SOCKET_READY)
	return;

    rfbScreen->socketState = RFB_SOCKET_SHUTDOWN;

    if(rfbScreen->inetdSock!=RFB_INVALID_SOCKET) {
	FD_CLR(rfbScreen->inetdSock,&rfbScreen->allFds);
	rfbCloseSocket(rfbScreen->inetdSock);
	rfbScreen->inetdSock=RFB_INVALID_SOCKET;
    }

    if(rfbScreen->listenSock!=RFB_INVALID_SOCKET) {
	FD_CLR(rfbScreen->listenSock,&rfbScreen->allFds);
	rfbCloseSocket(rfbScreen->listenSock);
	rfbScreen->listenSock=RFB_INVALID_SOCKET;
    }

    if(rfbScreen->listen6Sock!=RFB_INVALID_SOCKET) {
	FD_CLR(rfbScreen->listen6Sock,&rfbScreen->allFds);
	rfbCloseSocket(rfbScreen->listen6Sock);
	rfbScreen->listen6Sock=RFB_INVALID_SOCKET;
    }

    if(rfbScreen->udpSock!=RFB_INVALID_SOCKET) {
	FD_CLR(rfbScreen->udpSock,&rfbScreen->allFds);
	rfbCloseSocket(rfbScreen->udpSock);
	rfbScreen->udpSock=RFB_INVALID_SOCKET;
    }

#ifdef WIN32
    if(WSACleanup() != 0) {
	errno=WSAGetLastError();
	rfbLogPerror("Could not terminate Windows Sockets\n");
    }
#endif
}

/*
 * rfbCheckFds is called from ProcessInputEvents to check for input on the RFB
 * socket(s).  If there is input to process, the appropriate function in the
 * RFB server code will be called (rfbNewClientConnection,
 * rfbProcessClientMessage, etc).
 */

int
rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec)
{
    int nfds;
    fd_set fds;
    struct timeval tv;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char buf[6];
    rfbClientIteratorPtr i;
    rfbClientPtr cl;
    int result = 0;

    if (!rfbScreen->inetdInitDone && rfbScreen->inetdSock != RFB_INVALID_SOCKET) {
	rfbNewClientConnection(rfbScreen,rfbScreen->inetdSock); 
	rfbScreen->inetdInitDone = TRUE;
    }

    do {
	memcpy((char *)&fds, (char *)&(rfbScreen->allFds), sizeof(fd_set));
	tv.tv_sec = 0;
	tv.tv_usec = usec;
	nfds = select(rfbScreen->maxFd + 1, &fds, NULL, NULL /* &fds */, &tv);
	if (nfds == 0) {
	    /* timed out, check for async events */
            i = rfbGetClientIterator(rfbScreen);
            while((cl = rfbClientIteratorNext(i))) {
                if (cl->onHold)
                    continue;
                if (FD_ISSET(cl->sock, &(rfbScreen->allFds)))
                    rfbSendFileTransferChunk(cl);
            }
            rfbReleaseClientIterator(i);
	    return result;
	}

	if (nfds < 0) {
#ifdef WIN32
	    errno = WSAGetLastError();
#endif
	    if (errno != EINTR)
		rfbLogPerror("rfbCheckFds: select");
	    return -1;
	}

	result += nfds;

	if (rfbScreen->listenSock != RFB_INVALID_SOCKET && FD_ISSET(rfbScreen->listenSock, &fds)) {

	    if (!rfbProcessNewConnection(rfbScreen))
                return -1;

	    FD_CLR(rfbScreen->listenSock, &fds);
	    if (--nfds == 0)
		return result;
	}

	if (rfbScreen->listen6Sock != RFB_INVALID_SOCKET && FD_ISSET(rfbScreen->listen6Sock, &fds)) {

	    if (!rfbProcessNewConnection(rfbScreen))
                return -1;

	    FD_CLR(rfbScreen->listen6Sock, &fds);
	    if (--nfds == 0)
		return result;
	}

	if ((rfbScreen->udpSock != RFB_INVALID_SOCKET) && FD_ISSET(rfbScreen->udpSock, &fds)) {
	    if(!rfbScreen->udpClient)
		rfbNewUDPClient(rfbScreen);
	    if (recvfrom(rfbScreen->udpSock, buf, 1, MSG_PEEK,
			(struct sockaddr *)&addr, &addrlen) < 0) {
		rfbLogPerror("rfbCheckFds: UDP: recvfrom");
		rfbDisconnectUDPSock(rfbScreen);
		rfbScreen->udpSockConnected = FALSE;
	    } else {
		if (!rfbScreen->udpSockConnected ||
			(memcmp(&addr, &rfbScreen->udpRemoteAddr, addrlen) != 0))
		{
		    /* new remote end */
		    rfbLog("rfbCheckFds: UDP: got connection\n");

		    memcpy(&rfbScreen->udpRemoteAddr, &addr, addrlen);
		    rfbScreen->udpSockConnected = TRUE;

		    if (connect(rfbScreen->udpSock,
				(struct sockaddr *)&addr, addrlen) < 0) {
			rfbLogPerror("rfbCheckFds: UDP: connect");
			rfbDisconnectUDPSock(rfbScreen);
			return -1;
		    }

		    rfbNewUDPConnection(rfbScreen,rfbScreen->udpSock);
		}

		rfbProcessUDPInput(rfbScreen);
	    }

	    FD_CLR(rfbScreen->udpSock, &fds);
	    if (--nfds == 0)
		return result;
	}

	i = rfbGetClientIterator(rfbScreen);
	while((cl = rfbClientIteratorNext(i))) {

	    if (cl->onHold)
		continue;

            if (FD_ISSET(cl->sock, &(rfbScreen->allFds)))
            {
                if (FD_ISSET(cl->sock, &fds))
                {
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
                    do {
                        rfbProcessClientMessage(cl);
                    } while (cl->sock != RFB_INVALID_SOCKET && webSocketsHasDataInBuffer(cl));
#else
                    rfbProcessClientMessage(cl);
#endif
                }
                else
                    rfbSendFileTransferChunk(cl);
            }
	}
	rfbReleaseClientIterator(i);
    } while(rfbScreen->handleEventsEagerly);
    return result;
}

rfbBool
rfbProcessNewConnection(rfbScreenInfoPtr rfbScreen)
{
    rfbSocket sock = RFB_INVALID_SOCKET;
    fd_set listen_fds; 
    rfbSocket chosen_listen_sock = RFB_INVALID_SOCKET;
#if defined LIBVNCSERVER_HAVE_SYS_RESOURCE_H && defined LIBVNCSERVER_HAVE_FCNTL_H
    struct rlimit rlim;
    size_t maxfds, curfds, i;
#endif
    /* Do another select() call to find out which listen socket
       has an incoming connection pending. We know that at least 
       one of them has, so this should not block for too long! */
    FD_ZERO(&listen_fds);  
    if(rfbScreen->listenSock != RFB_INVALID_SOCKET)
      FD_SET(rfbScreen->listenSock, &listen_fds);
    if(rfbScreen->listen6Sock != RFB_INVALID_SOCKET)
      FD_SET(rfbScreen->listen6Sock, &listen_fds);
    if (select(rfbScreen->maxFd+1, &listen_fds, NULL, NULL, NULL) == -1) {
      rfbLogPerror("rfbProcessNewConnection: error in select");
      return FALSE;
    }
    if (rfbScreen->listenSock != RFB_INVALID_SOCKET && FD_ISSET(rfbScreen->listenSock, &listen_fds))
      chosen_listen_sock = rfbScreen->listenSock;
    if (rfbScreen->listen6Sock != RFB_INVALID_SOCKET && FD_ISSET(rfbScreen->listen6Sock, &listen_fds))
      chosen_listen_sock = rfbScreen->listen6Sock;


    /*
      Avoid accept() giving EMFILE, i.e. running out of file descriptors, a situation that's hard to recover from.
      https://stackoverflow.com/questions/47179793/how-to-gracefully-handle-accept-giving-emfile-and-close-the-connection
      describes the problem nicely.
      Our approach is to deny new clients when we have reached a certain fraction of the per-process limit of file descriptors.
      TODO: add Windows support.
     */
#if defined LIBVNCSERVER_HAVE_SYS_RESOURCE_H && defined LIBVNCSERVER_HAVE_FCNTL_H
    if(getrlimit(RLIMIT_NOFILE, &rlim) < 0)
	maxfds = 100;  /* use a sane default if getting the limit fails */
    else
	maxfds = rlim.rlim_cur;

    /* get the number of currently open fds as per https://stackoverflow.com/a/7976880/361413 */
    curfds = 0;
    for(i = 0; i < maxfds; ++i)
	if(fcntl(i, F_GETFD) != -1)
	    ++curfds;

    if(curfds > maxfds * rfbScreen->fdQuota) {
	rfbErr("rfbProcessNewconnection: open fd count of %lu exceeds quota %.1f of limit %lu, denying connection\n", curfds, rfbScreen->fdQuota, maxfds);
	sock = accept(chosen_listen_sock, NULL, NULL);
	rfbCloseSocket(sock);
	return FALSE;
    }
#endif

    if ((sock = accept(chosen_listen_sock, NULL, NULL)) == RFB_INVALID_SOCKET) {
      rfbLogPerror("rfbProcessNewconnection: accept");
      return FALSE;
    }

    return rfbNewConnectionFromSock(rfbScreen, sock);
}


void
rfbDisconnectUDPSock(rfbScreenInfoPtr rfbScreen)
{
  rfbScreen->udpSockConnected = FALSE;
}



void
rfbCloseClient(rfbClientPtr cl)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    cl->sock = RFB_INVALID_SOCKET;
#endif
    rfbExtensionData* extension;

    for(extension=cl->extensions; extension; extension=extension->next)
	if(extension->extension->close)
	    extension->extension->close(cl, extension->data);

    LOCK(cl->updateMutex);
#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    if (cl->sock != RFB_INVALID_SOCKET)
#endif
      {
	/* Remove client sock from allFds and adapt maxFd */
	FD_CLR(cl->sock,&(cl->screen->allFds));
	if(cl->sock==cl->screen->maxFd)
	  while(cl->screen->maxFd>0
		&& !FD_ISSET(cl->screen->maxFd,&(cl->screen->allFds)))
	    cl->screen->maxFd--;
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
	/* Has to happen before socket close as the SSL implementation might send a goodbye */
	if (cl->sslctx)
	    rfbssl_destroy(cl);
	free(cl->wspath);
#endif
      }
    TSIGNAL(cl->updateCond);
    UNLOCK(cl->updateMutex);

    /*
       Client socket closing, either via the client-to-server thread or directly.
    */
#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    if(cl->screen->backgroundLoop) {
	/* Indicate to client-to-server thread that it should not go on */
	cl->state = RFB_SHUTDOWN;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
	/*
	  Notify the thread. This simply writes a NULL byte to the notify pipe in order to get past the select()
	  in clientInput(), the loop in there will then break because the client state has been set to
	  RFB_SHUTDOWN. Client socket closing will be done by the thread.
	*/
	write(cl->pipe_notify_client_thread[1], "\x00", 1);
	/*
	  No joining of threads here, this is fire and forget.
	*/
#endif
    } else
#endif
	/* Either no threading support or threading support with screen->backgroundloop == false */
	{
	    /* Close client sock */
	    rfbCloseSocket(cl->sock);
	    cl->sock = RFB_INVALID_SOCKET;
	}
}


/*
 * rfbConnect is called to make a connection out to a given TCP address.
 */

rfbSocket
rfbConnect(rfbScreenInfoPtr rfbScreen,
           char *host,
           int port)
{
    rfbSocket sock;
    int one = 1;

    rfbLog("Making connection to client on host %s port %d\n",
	   host,port);

    if ((sock = rfbConnectToTcpAddr(host, port)) == RFB_INVALID_SOCKET) {
	rfbLogPerror("connection failed");
	return RFB_INVALID_SOCKET;
    }

    if(!rfbSetNonBlocking(sock)) {
        rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }

    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		   (const char *)&one, sizeof(one)) < 0) {
	rfbLogPerror("setsockopt failed: can't set TCP_NODELAY flag, non TCP socket?");
    }

    /* AddEnabledDevice(sock); */
    FD_SET(sock, &rfbScreen->allFds);
    rfbScreen->maxFd = rfbMax(sock,rfbScreen->maxFd);

    return sock;
}

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
size_t fuzz_offset;
size_t fuzz_size;
const uint8_t *fuzz_data;
#endif

/*
 * ReadExact reads an exact number of bytes from a client.  Returns 1 if
 * those bytes have been read, 0 if the other end has closed, or -1 if an error
 * occurred (errno is set to ETIMEDOUT if it timed out).
 */

int
rfbReadExactTimeout(rfbClientPtr cl, char* buf, int len, int timeout)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (fuzz_offset + len <= fuzz_size) {
        memcpy(buf, fuzz_data + fuzz_offset, len);
        fuzz_offset += len;
        return 1;
    }
    return 0;
#endif
    rfbSocket sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;

    while (len > 0) {
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
        if (cl->wsctx) {
            n = webSocketsDecode(cl, buf, len);
        } else if (cl->sslctx) {
	    n = rfbssl_read(cl, buf, len);
	} else {
            n = read(sock, buf, len);
        }
#else
        n = read(sock, buf, len);
#endif

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            return 0;

        } else {
#ifdef WIN32
	    errno = WSAGetLastError();
#endif
	    if (errno == EINTR)
		continue;

#ifdef LIBVNCSERVER_ENOENT_WORKAROUND
	    if (errno != ENOENT)
#endif
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return n;
            }

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
	    if (cl->sslctx) {
		if (rfbssl_pending(cl))
		    continue;
	    }
#endif
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            n = select(sock+1, &fds, NULL, &fds, &tv);
            if (n < 0) {
                rfbLogPerror("ReadExact: select");
                return n;
            }
            if (n == 0) {
                rfbErr("ReadExact: select timeout\n");
                errno = ETIMEDOUT;
                return -1;
            }
        }
    }
#undef DEBUG_READ_EXACT
#ifdef DEBUG_READ_EXACT
    rfbLog("ReadExact %d bytes\n",len);
    for(n=0;n<len;n++)
	    fprintf(stderr,"%02x ",(unsigned char)buf[n]);
    fprintf(stderr,"\n");
#endif

    return 1;
}

int rfbReadExact(rfbClientPtr cl,char* buf,int len)
{
  /* favor the per-screen value if set */
  if(cl->screen && cl->screen->maxClientWait)
    return(rfbReadExactTimeout(cl,buf,len,cl->screen->maxClientWait));
  else
    return(rfbReadExactTimeout(cl,buf,len,rfbMaxClientWait));
}

/*
 * PeekExact peeks at an exact number of bytes from a client.  Returns 1 if
 * those bytes have been read, 0 if the other end has closed, or -1 if an
 * error occurred (errno is set to ETIMEDOUT if it timed out).
 */

int
rfbPeekExactTimeout(rfbClientPtr cl, char* buf, int len, int timeout)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (fuzz_offset + len <= fuzz_size) {
        memcpy(buf, fuzz_data + fuzz_offset, len);
        fuzz_offset += len;
        return 1;
    }
    return 0;
#endif
    rfbSocket sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;

    while (len > 0) {
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
	if (cl->sslctx)
	    n = rfbssl_peek(cl, buf, len);
	else
#endif
	    n = recv(sock, buf, len, MSG_PEEK);

        if (n == len) {

            break;

        } else if (n == 0) {

            return 0;

        } else {
#ifdef WIN32
	    errno = WSAGetLastError();
#endif
	    if (errno == EINTR)
		continue;

#ifdef LIBVNCSERVER_ENOENT_WORKAROUND
	    if (errno != ENOENT)
#endif
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return n;
            }

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
	    if (cl->sslctx) {
		if (rfbssl_pending(cl))
		    continue;
	    }
#endif
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            n = select(sock+1, &fds, NULL, &fds, &tv);
            if (n < 0) {
                rfbLogPerror("PeekExact: select");
                return n;
            }
            if (n == 0) {
                errno = ETIMEDOUT;
                return -1;
            }
        }
    }
#undef DEBUG_READ_EXACT
#ifdef DEBUG_READ_EXACT
    rfbLog("PeekExact %d bytes\n",len);
    for(n=0;n<len;n++)
	    fprintf(stderr,"%02x ",(unsigned char)buf[n]);
    fprintf(stderr,"\n");
#endif

    return 1;
}

/*
 * WriteExact writes an exact number of bytes to a client.  Returns 1 if
 * those bytes have been written, or -1 if an error occurred (errno is set to
 * ETIMEDOUT if it timed out).
 */

int
rfbWriteExact(rfbClientPtr cl,
              const char *buf,
              int len)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return 1;
#endif
    rfbSocket sock = cl->sock;
    int n;
    fd_set fds;
    struct timeval tv;
    int totalTimeWaited = 0;
    const int timeout = (cl->screen && cl->screen->maxClientWait) ? cl->screen->maxClientWait : rfbMaxClientWait;

#undef DEBUG_WRITE_EXACT
#ifdef DEBUG_WRITE_EXACT
    rfbLog("WriteExact %d bytes\n",len);
    for(n=0;n<len;n++)
	    fprintf(stderr,"%02x ",(unsigned char)buf[n]);
    fprintf(stderr,"\n");
#endif

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
    if (cl->wsctx) {
        char *tmp = NULL;

        while (len > UPDATE_BUF_SIZE) {
            /* webSocketsEncode() can only handle data lengths up to UPDATE_BUF_SIZE
               so split large writes into multiple smaller writes/frames */

            if (rfbWriteExact(cl, buf, UPDATE_BUF_SIZE) == -1) {
                return -1;
            }

            buf += UPDATE_BUF_SIZE;
            len -= UPDATE_BUF_SIZE;
        }

        if ((len = webSocketsEncode(cl, buf, len, &tmp)) < 0) {
            rfbErr("WriteExact: WebSockets encode error\n");
            return -1;
        }
        buf = tmp;
    }
#endif

    LOCK(cl->outputMutex);
    while (len > 0) {
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
        if (cl->sslctx)
	    n = rfbssl_write(cl, buf, len);
	else
#endif
	    n = write(sock, buf, len);

        if (n > 0) {

            buf += n;
            len -= n;

        } else if (n == 0) {

            rfbErr("WriteExact: write returned 0?\n");
            UNLOCK(cl->outputMutex);
            return 0;

        } else {
#ifdef WIN32
			errno = WSAGetLastError();
#endif
	    if (errno == EINTR)
		continue;

            if (errno != EWOULDBLOCK && errno != EAGAIN) {
	        UNLOCK(cl->outputMutex);
                return n;
            }

            /* Retry every 5 seconds until we exceed timeout.  We
               need to do this because select doesn't necessarily return
               immediately when the other end has gone away */

            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            n = select(sock+1, NULL, &fds, NULL /* &fds */, &tv);
	    if (n < 0) {
#ifdef WIN32
                errno=WSAGetLastError();
#endif
       	        if(errno==EINTR)
		    continue;
                rfbLogPerror("WriteExact: select");
                UNLOCK(cl->outputMutex);
                return n;
            }
            if (n == 0) {
                totalTimeWaited += 5000;
                if (totalTimeWaited >= timeout) {
                    errno = ETIMEDOUT;
                    UNLOCK(cl->outputMutex);
                    return -1;
                }
            } else {
                totalTimeWaited = 0;
            }
        }
    }
    UNLOCK(cl->outputMutex);
    return 1;
}

/* currently private, called by rfbProcessArguments() */
int
rfbStringToAddr(char *str, in_addr_t *addr)  {
    if (str == NULL || *str == '\0' || strcmp(str, "any") == 0) {
        *addr = htonl(INADDR_ANY);
    } else if (strcmp(str, "localhost") == 0) {
        *addr = htonl(INADDR_LOOPBACK);
    } else {
        struct hostent *hp;
        if ((*addr = inet_addr(str)) == htonl(INADDR_NONE)) {
            if (!(hp = gethostbyname(str))) {
                return 0;
            }
            *addr = *(unsigned long *)hp->h_addr;
        }
    }
    return 1;
}

rfbSocket
rfbListenOnTCPPort(int port,
                   in_addr_t iface)
{
    struct sockaddr_in addr;
    rfbSocket sock;
    int one = 1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = iface;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == RFB_INVALID_SOCKET) {
	return RFB_INVALID_SOCKET;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		   (const char *)&one, sizeof(one)) < 0) {
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }
    if (listen(sock, 32) < 0) {
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }

    return sock;
}


rfbSocket
rfbListenOnTCP6Port(int port,
                    const char* iface)
{
#ifndef LIBVNCSERVER_IPv6
    rfbLogPerror("This LibVNCServer does not have IPv6 support");
    return RFB_INVALID_SOCKET;
#else
    rfbSocket sock;
    int one = 1;
    int rv;
    struct addrinfo hints, *servinfo, *p;
    char port_str[8];

    snprintf(port_str, 8, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* fill in wildcard address if iface == NULL */

    if ((rv = getaddrinfo(iface, port_str, &hints, &servinfo)) != 0) {
        rfbErr("rfbListenOnTCP6Port error in getaddrinfo: %s\n", gai_strerror(rv));
        return RFB_INVALID_SOCKET;
    }
    
    /* loop through all the results and bind to the first we can */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;
        }

#ifdef IPV6_V6ONLY
	/* we have separate IPv4 and IPv6 sockets since some OS's do not support dual binding */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&one, sizeof(one)) < 0) {
	  rfbLogPerror("rfbListenOnTCP6Port error in setsockopt IPV6_V6ONLY");
	  rfbCloseSocket(sock);
	  freeaddrinfo(servinfo);
	  return RFB_INVALID_SOCKET;
	}
#endif

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one)) < 0) {
	  rfbLogPerror("rfbListenOnTCP6Port: error in setsockopt SO_REUSEADDR");
	  rfbCloseSocket(sock);
	  freeaddrinfo(servinfo);
	  return RFB_INVALID_SOCKET;
	}

	if (bind(sock, p->ai_addr, p->ai_addrlen) < 0) {
	  rfbCloseSocket(sock);
	  continue;
	}

        break;
    }

    if (p == NULL)  {
        rfbLogPerror("rfbListenOnTCP6Port: error in bind IPv6 socket");
        freeaddrinfo(servinfo);
        return RFB_INVALID_SOCKET;
    }

    /* all done with this structure now */
    freeaddrinfo(servinfo);

    if (listen(sock, 32) < 0) {
        rfbLogPerror("rfbListenOnTCP6Port: error in listen on IPv6 socket");
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }

    return sock;
#endif
}


rfbSocket
rfbConnectToTcpAddr(char *host,
                    int port)
{
    rfbSocket sock;
#ifdef LIBVNCSERVER_IPv6
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char port_str[8];

    snprintf(port_str, 8, "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port_str, &hints, &servinfo)) != 0) {
        rfbErr("rfbConnectToTcpAddr: error in getaddrinfo: %s\n", gai_strerror(rv));
        return RFB_INVALID_SOCKET;
    }

    /* loop through all the results and connect to the first we can */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == RFB_INVALID_SOCKET)
            continue;

	if (sock_set_nonblocking(sock, TRUE, rfbErr)) {
	    if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
		break;
	    } else {
#ifdef WIN32
		errno=WSAGetLastError();
#endif
		if ((errno == EWOULDBLOCK || errno == EINPROGRESS) && sock_wait_for_connected(sock, rfbMaxClientWait/1000))
		    break;
		rfbCloseSocket(sock);
	    }
	} else {
	    rfbCloseSocket(sock);
	}
    }

    /* all failed */
    if (p == NULL) {
        rfbLogPerror("rfbConnectToTcoAddr: failed to connect\n");
        sock = RFB_INVALID_SOCKET; /* set return value */
    } else {
	/* one succeeded, re-set to blocking */
	if (!sock_set_nonblocking(sock, FALSE, rfbErr))
	    rfbCloseSocket(sock);
    }

    /* all done with this structure now */
    freeaddrinfo(servinfo);
#else
    struct hostent *hp;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if ((addr.sin_addr.s_addr = inet_addr(host)) == htonl(INADDR_NONE))
    {
	if (!(hp = gethostbyname(host))) {
	    errno = EINVAL;
	    return RFB_INVALID_SOCKET;
	}
	addr.sin_addr.s_addr = *(unsigned long *)hp->h_addr;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == RFB_INVALID_SOCKET) {
	return RFB_INVALID_SOCKET;
    }

    if (!sock_set_nonblocking(sock, TRUE, rfbErr)) {
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef WIN32
	errno=WSAGetLastError();
#endif
	if (!((errno == EWOULDBLOCK || errno == EINPROGRESS) && sock_wait_for_connected(sock, rfbMaxClientWait/1000))) {
	    rfbErr("rfbConnectToTcpAddr: connect\n");
	    rfbCloseSocket(sock);
	    return RFB_INVALID_SOCKET;
	}
    }

    if (!sock_set_nonblocking(sock, FALSE, rfbErr)) {
	rfbCloseSocket(sock);
	return RFB_INVALID_SOCKET;
    }

#endif
    return sock;
}

rfbSocket
rfbListenOnUDPPort(int port,
                   in_addr_t iface)
{
    struct sockaddr_in addr;
    rfbSocket sock;
    int one = 1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = iface;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == RFB_INVALID_SOCKET) {
	return RFB_INVALID_SOCKET;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		   (const char *)&one, sizeof(one)) < 0) {
	return RFB_INVALID_SOCKET;
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	return RFB_INVALID_SOCKET;
    }

    return sock;
}

/*
 * rfbSetNonBlocking sets a socket into non-blocking mode.
 */
rfbBool
rfbSetNonBlocking(rfbSocket sock)
{
    return sock_set_nonblocking(sock, TRUE, rfbLog);
}
