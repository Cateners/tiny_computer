#ifndef TLS_H
#define TLS_H

/*
 *  Copyright (C) 2009 Vic Lee.
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

/* Handle Anonymous TLS Authentication (18) with the server.
 * After authentication, client->tlsSession will be set.
 */
rfbBool HandleAnonTLSAuth(rfbClient* client);

/* Handle VeNCrypt Authentication (19) with the server.
 * The callback function GetX509Credential will be called.
 * After authentication, client->tlsSession will be set.
 */
rfbBool HandleVeNCryptAuth(rfbClient* client);

/* Read desired bytes from TLS session.
 * It's a wrapper function over gnutls_record_recv() and return values
 * are same as read(), that is, >0 for actual bytes read, 0 for EOF,
 * or EAGAIN, EINTR.
 * This should be a non-blocking call. Blocking is handled in sockets.c.
 */
int ReadFromTLS(rfbClient* client, char *out, unsigned int n);

/* Write desired bytes to TLS session.
 * It's a wrapper function over gnutls_record_send() and it will be
 * blocking call, until all bytes are written or error returned.
 */
int WriteToTLS(rfbClient* client, const char *buf, unsigned int n);

/* Free TLS resources */
void FreeTLS(rfbClient* client);

#ifdef LIBVNCSERVER_HAVE_SASL
/* Get the number of bits in the current cipher */
int GetTLSCipherBits(rfbClient* client);
#endif /* LIBVNCSERVER_HAVE_SASL */

#endif /* TLS_H */
