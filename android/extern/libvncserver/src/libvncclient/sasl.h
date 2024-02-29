#ifndef RFBSASL_H
#define RFBSASL_H

/*
 *  Copyright (C) 2017 S. Waterman.  All Rights Reserved.
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

#ifdef LIBVNCSERVER_HAVE_SASL

#include <rfb/rfbclient.h>

/*
 *  Perform the SASL authentication process
 */
rfbBool HandleSASLAuth(rfbClient *client);

/* 
 * Read from SASL when the SASL SSF is in use.
 */
int ReadFromSASL(rfbClient* client, char *out, unsigned int n);

#endif  /* LIBVNCSERVER_HAVE_SASL */

#endif /* RFBSASL_H */
