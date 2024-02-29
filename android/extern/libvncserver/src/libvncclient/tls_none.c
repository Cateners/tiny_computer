/*
 *  Copyright (C) 2012 Christian Beier.
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

#include <rfb/rfbclient.h>
#include <errno.h>
#include "tls.h"

rfbBool HandleAnonTLSAuth(rfbClient* client) 
{
  rfbClientLog("TLS is not supported.\n");
  return FALSE;
}


rfbBool HandleVeNCryptAuth(rfbClient* client)
{
  rfbClientLog("TLS is not supported.\n");
  return FALSE;
}


int ReadFromTLS(rfbClient* client, char *out, unsigned int n)
{
  rfbClientLog("TLS is not supported.\n");
  errno = EINTR;
  return -1;
}


int WriteToTLS(rfbClient* client, const char *buf, unsigned int n)
{
  rfbClientLog("TLS is not supported.\n");
  errno = EINTR;
  return -1;
}


void FreeTLS(rfbClient* client)
{

}

#ifdef LIBVNCSERVER_HAVE_SASL
int
GetTLSCipherBits(rfbClient* client)
{
  rfbClientLog("TLS is not supported.\n");
  return 0;
}
#endif /* LIBVNCSERVER_HAVE_SASL */

