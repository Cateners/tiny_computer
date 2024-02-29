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
 * hextile.c - handle hextile encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a hextile
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleHextileBPP CONCAT2E(HandleHextile,BPP)
#define CARDBPP CONCAT3E(uint,BPP,_t)

static rfbBool
HandleHextileBPP (rfbClient* client, int rx, int ry, int rw, int rh)
{
  CARDBPP bg = 0, fg;
  int i;
  uint8_t *ptr;
  int x, y, w, h;
  int sx, sy, sw, sh;
  uint8_t subencoding;
  uint8_t nSubrects;

  for (y = ry; y < ry+rh; y += 16) {
    for (x = rx; x < rx+rw; x += 16) {
      w = h = 16;
      if (rx+rw - x < 16)
	w = rx+rw - x;
      if (ry+rh - y < 16)
	h = ry+rh - y;

      if (!ReadFromRFBServer(client, (char *)&subencoding, 1))
	return FALSE;

      if (subencoding & rfbHextileRaw) {
	if (!ReadFromRFBServer(client, client->buffer, w * h * (BPP / 8)))
	  return FALSE;

	client->GotBitmap(client, (uint8_t *)client->buffer, x, y, w, h);

	continue;
      }

      if (subencoding & rfbHextileBackgroundSpecified)
	if (!ReadFromRFBServer(client, (char *)&bg, sizeof(bg)))
	  return FALSE;

      client->GotFillRect(client, x, y, w, h, bg);

      if (subencoding & rfbHextileForegroundSpecified)
	if (!ReadFromRFBServer(client, (char *)&fg, sizeof(fg)))
	  return FALSE;

      if (!(subencoding & rfbHextileAnySubrects)) {
	continue;
      }

      if (!ReadFromRFBServer(client, (char *)&nSubrects, 1))
	return FALSE;

      ptr = (uint8_t*)client->buffer;

      if (subencoding & rfbHextileSubrectsColoured) {
	if (!ReadFromRFBServer(client, client->buffer, nSubrects * (2 + (BPP / 8))))
	  return FALSE;

	for (i = 0; i < nSubrects; i++) {
#if BPP==8
	  GET_PIXEL8(fg, ptr);
#elif BPP==16
	  GET_PIXEL16(fg, ptr);
#elif BPP==32
	  GET_PIXEL32(fg, ptr);
#else
#error "Invalid BPP"
#endif
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;

	  client->GotFillRect(client, x+sx, y+sy, sw, sh, fg);
	}

      } else {
	if (!ReadFromRFBServer(client, client->buffer, nSubrects * 2))
	  return FALSE;

	for (i = 0; i < nSubrects; i++) {
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;

	  client->GotFillRect(client, x+sx, y+sy, sw, sh, fg);
	}
      }
    }
  }

  return TRUE;
}

#undef CARDBPP
