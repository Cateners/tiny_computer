/*
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
 * rre.c - handle RRE encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles an RRE
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleRREBPP CONCAT2E(HandleRRE,BPP)
#define CARDBPP CONCAT3E(uint,BPP,_t)

static rfbBool
HandleRREBPP (rfbClient* client, int rx, int ry, int rw, int rh)
{
  rfbRREHeader hdr;
  int i;
  CARDBPP pix;
  rfbRectangle subrect;

  if (!ReadFromRFBServer(client, (char *)&hdr, sz_rfbRREHeader))
    return FALSE;

  hdr.nSubrects = rfbClientSwap32IfLE(hdr.nSubrects);

  if (!ReadFromRFBServer(client, (char *)&pix, sizeof(pix)))
    return FALSE;

  client->GotFillRect(client, rx, ry, rw, rh, pix);

  for (i = 0; i < hdr.nSubrects; i++) {
    if (!ReadFromRFBServer(client, (char *)&pix, sizeof(pix)))
      return FALSE;

    if (!ReadFromRFBServer(client, (char *)&subrect, sz_rfbRectangle))
      return FALSE;

    subrect.x = rfbClientSwap16IfLE(subrect.x);
    subrect.y = rfbClientSwap16IfLE(subrect.y);
    subrect.w = rfbClientSwap16IfLE(subrect.w);
    subrect.h = rfbClientSwap16IfLE(subrect.h);

    client->GotFillRect(client, rx+subrect.x, ry+subrect.y, subrect.w, subrect.h, pix);
  }

  return TRUE;
}

#undef CARDBPP
