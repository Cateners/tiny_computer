/*
 *  Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
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
 * cursor.c - code to support cursor shape updates (XCursor and
 * RichCursor preudo-encodings).
 */

#include <rfb/rfbclient.h>


#define OPER_SAVE     0
#define OPER_RESTORE  1

#define MAX_CURSOR_SIZE 1024

#define RGB24_TO_PIXEL(bpp,r,g,b)                                       \
   ((((uint##bpp##_t)(r) & 0xFF) * client->format.redMax + 127) / 255             \
    << client->format.redShift |                                              \
    (((uint##bpp##_t)(g) & 0xFF) * client->format.greenMax + 127) / 255           \
    << client->format.greenShift |                                            \
    (((uint##bpp##_t)(b) & 0xFF) * client->format.blueMax + 127) / 255            \
    << client->format.blueShift)


rfbBool HandleCursorShape(rfbClient* client,int xhot, int yhot, int width, int height, uint32_t enc)
{
  int bytesPerPixel;
  size_t bytesPerRow, bytesMaskData;
  rfbXCursorColors rgb;
  uint32_t colors[2];
  char *buf;
  uint8_t *ptr;
  int x, y, b;

  bytesPerPixel = client->format.bitsPerPixel / 8;
  bytesPerRow = (width + 7) / 8;
  bytesMaskData = bytesPerRow * height;

  if (width * height == 0)
    return TRUE;

  if (width >= MAX_CURSOR_SIZE || height >= MAX_CURSOR_SIZE)
    return FALSE;

  /* Allocate memory for pixel data and temporary mask data. */
  if(client->rcSource)
    free(client->rcSource);

  client->rcSource = malloc((size_t)width * height * bytesPerPixel);
  if (client->rcSource == NULL)
    return FALSE;

  buf = malloc(bytesMaskData);
  if (buf == NULL) {
    free(client->rcSource);
    client->rcSource = NULL;
    return FALSE;
  }

  /* Read and decode cursor pixel data, depending on the encoding type. */

  if (enc == rfbEncodingXCursor) {
    /* Read and convert background and foreground colors. */
    if (!ReadFromRFBServer(client, (char *)&rgb, sz_rfbXCursorColors)) {
      free(client->rcSource);
      client->rcSource = NULL;
      free(buf);
      return FALSE;
    }
    colors[0] = RGB24_TO_PIXEL(32, rgb.backRed, rgb.backGreen, rgb.backBlue);
    colors[1] = RGB24_TO_PIXEL(32, rgb.foreRed, rgb.foreGreen, rgb.foreBlue);

    /* Read 1bpp pixel data into a temporary buffer. */
    if (!ReadFromRFBServer(client, buf, bytesMaskData)) {
      free(client->rcSource);
      client->rcSource = NULL;
      free(buf);
      return FALSE;
    }

    /* Convert 1bpp data to byte-wide color indices. */
    ptr = client->rcSource;
    for (y = 0; y < height; y++) {
      for (x = 0; x < width / 8; x++) {
	for (b = 7; b >= 0; b--) {
	  *ptr = buf[y * bytesPerRow + x] >> b & 1;
	  ptr += bytesPerPixel;
	}
      }
      for (b = 7; b > 7 - width % 8; b--) {
	*ptr = buf[y * bytesPerRow + x] >> b & 1;
	ptr += bytesPerPixel;
      }
    }

    /* Convert indices into the actual pixel values. */
    switch (bytesPerPixel) {
    case 1:
      for (x = 0; x < width * height; x++)
	client->rcSource[x] = (uint8_t)colors[client->rcSource[x]];
      break;
    case 2:
      for (x = 0; x < width * height; x++)
	((uint16_t *)client->rcSource)[x] = (uint16_t)colors[client->rcSource[x * 2]];
      break;
    case 4:
      for (x = 0; x < width * height; x++)
	((uint32_t *)client->rcSource)[x] = colors[client->rcSource[x * 4]];
      break;
    }

  } else {			/* enc == rfbEncodingRichCursor */

    if (!ReadFromRFBServer(client, (char *)client->rcSource, width * height * bytesPerPixel)) {
      free(client->rcSource);
      client->rcSource = NULL;
      free(buf);
      return FALSE;
    }

  }

  /* Read and decode mask data. */

  if (!ReadFromRFBServer(client, buf, bytesMaskData)) {
    free(client->rcSource);
    client->rcSource = NULL;
    free(buf);
    return FALSE;
  }

  if(client->rcMask)
    free(client->rcMask);

  client->rcMask = malloc((size_t)width * height);
  if (client->rcMask == NULL) {
    free(client->rcSource);
    client->rcSource = NULL;
    free(buf);
    return FALSE;
  }

  ptr = client->rcMask;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width / 8; x++) {
      for (b = 7; b >= 0; b--) {
	*ptr++ = buf[y * bytesPerRow + x] >> b & 1;
      }
    }
    for (b = 7; b > 7 - width % 8; b--) {
      *ptr++ = buf[y * bytesPerRow + x] >> b & 1;
    }
  }

  if (client->GotCursorShape != NULL) {
     client->GotCursorShape(client, xhot, yhot, width, height, bytesPerPixel);
  }

  free(buf);

  return TRUE;
}


