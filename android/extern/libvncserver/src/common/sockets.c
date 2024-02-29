/*
 * common/sockets.c - Common internal socket functions used by both
 * libvncclient and libvncserver.
 */

/*
 *  Copyright (C) 2022 Christian Beier
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "sockets.h"


rfbBool sock_set_nonblocking(rfbSocket sock, rfbBool non_blocking, void (*log)(const char *format, ...))
{
#ifdef WIN32
  unsigned long block = non_blocking ? 0 : 1;
  if(ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR) {
    errno=WSAGetLastError();
#else
  int flags = fcntl(sock, F_GETFL);
  int new_flags;
  if(non_blocking)
      new_flags = flags | O_NONBLOCK;
  else
      new_flags = flags & ~O_NONBLOCK;
  if(flags < 0 || fcntl(sock, F_SETFL, new_flags) < 0) {
#endif
    log("Setting socket to %sblocking mode failed: %s\n", non_blocking ? "non-" : "", strerror(errno));
    return FALSE;
  }
  return TRUE;
}


rfbBool sock_wait_for_connected(int socket, unsigned int timeout_seconds)
{
  fd_set writefds;
  fd_set exceptfds;
  struct timeval timeout;

  timeout.tv_sec=timeout_seconds;
  timeout.tv_usec=0;

  FD_ZERO(&writefds);
  FD_SET(socket, &writefds);
  FD_ZERO(&exceptfds);
  FD_SET(socket, &exceptfds);
  if (select(socket+1, NULL, &writefds, &exceptfds, &timeout)==1) {
#ifdef WIN32
    if (FD_ISSET(socket, &exceptfds))
      return FALSE;
#else
    int so_error;
    socklen_t len = sizeof so_error;
    getsockopt(socket, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error!=0)
      return FALSE;
#endif
    return TRUE;
  }

  return FALSE;
}



