/*
 *  Copyright (C) 2017 Wiki Wang <wikiwang@live.com>.  All Rights Reserved.
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
 * trle.c - handle trle encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a trle
 * encoded rectangle with BPP bits per pixel.
 */

#ifndef REALBPP
#define REALBPP BPP
#endif

#if !defined(UNCOMP) || UNCOMP == 0
#define HandleTRLE CONCAT2E(HandleTRLE, REALBPP)
#elif UNCOMP > 0
#define HandleTRLE CONCAT3E(HandleTRLE, REALBPP, Down)
#else
#define HandleTRLE CONCAT3E(HandleTRLE, REALBPP, Up)
#endif
#define CARDBPP CONCAT3E(uint, BPP, _t)
#define CARDREALBPP CONCAT3E(uint, REALBPP, _t)

#if REALBPP != BPP && defined(UNCOMP) && UNCOMP != 0
#if UNCOMP > 0
#define UncompressCPixel(pointer) ((*(CARDBPP *)pointer) >> UNCOMP)
#else
#define UncompressCPixel(pointer) ((*(CARDBPP *)pointer) << (-(UNCOMP)))
#endif
#else
#define UncompressCPixel(pointer) (*(CARDBPP *)pointer)
#endif

static rfbBool HandleTRLE(rfbClient *client, int rx, int ry, int rw, int rh) {
  int x, y, w, h;
  uint8_t type, last_type = 0;
  int min_buffer_size = 16 * 16 * (REALBPP / 8) * 2;
  uint8_t *buffer;
  CARDBPP palette[128];
  int bpp = 0, mask = 0, divider = 0;
  CARDBPP color = 0;

  /* First make sure we have a large enough raw buffer to hold the
   * decompressed data.  In practice, with a fixed REALBPP, fixed frame
   * buffer size and the first update containing the entire frame
   * buffer, this buffer allocation should only happen once, on the
   * first update.
   */
  if (client->raw_buffer_size < min_buffer_size) {

    if (client->raw_buffer != NULL) {

      free(client->raw_buffer);
    }

    client->raw_buffer_size = min_buffer_size;
    client->raw_buffer = (char *)malloc(client->raw_buffer_size);
  }

  rfbClientLog("Update %d %d %d %d\n", rx, ry, rw, rh);

  for (y = ry; y < ry + rh; y += 16) {
    for (x = rx; x < rx + rw; x += 16) {
      w = h = 16;
      if (rx + rw - x < 16)
        w = rx + rw - x;
      if (ry + rh - y < 16)
        h = ry + rh - y;

      if (!ReadFromRFBServer(client, (char *)(&type), 1))
        return FALSE;

      buffer = (uint8_t*)(client->raw_buffer);

      switch (type) {
      case 0: {
        if (!ReadFromRFBServer(client, (char *)buffer, w * h * REALBPP / 8))
          return FALSE;
#if REALBPP != BPP
        int i, j;

        for (j = y * client->width; j < (y + h) * client->width;
             j += client->width)
          for (i = x; i < x + w; i++, buffer += REALBPP / 8)
            ((CARDBPP *)client->frameBuffer)[j + i] = UncompressCPixel(buffer);
#else
        client->GotBitmap(client, buffer, x, y, w, h);
#endif
        type = last_type;
        break;
      }
      case 1: {
        if (!ReadFromRFBServer(client, (char *)buffer, REALBPP / 8))
          return FALSE;

        color = UncompressCPixel(buffer);

        client->GotFillRect(client, x, y, w, h, color);

        last_type = type;
        break;
      }
      case_127:
      case 127:
        switch (last_type) {
        case 0:
          return FALSE;
        case 1:
          client->GotFillRect(client, x, y, w, h, color);
          type = last_type;
          break;
        case 128:
          return FALSE;
        default:
          if (last_type >= 130) {
            last_type = last_type & 0x7f;

            bpp = (last_type > 4 ? (last_type > 16 ? 8 : 4)
                                 : (last_type > 2 ? 2 : 1)),
            mask = (1 << bpp) - 1, divider = (8 / bpp);
          }
          if (last_type <= 16) {
            int i, j, shift;

            if (!ReadFromRFBServer(client, (char*)buffer,
                                   (w + divider - 1) / divider * h))
              return FALSE;

            /* read palettized pixels */
            for (j = y * client->width; j < (y + h) * client->width;
                 j += client->width) {
              for (i = x, shift = 8 - bpp; i < x + w; i++) {
                ((CARDBPP *)client->frameBuffer)[j + i] =
                    palette[((*buffer) >> shift) & mask];
                shift -= bpp;
                if (shift < 0) {
                  shift = 8 - bpp;
                  buffer++;
                }
              }
              if (shift < 8 - bpp)
                buffer++;

              type = last_type;
            }
          } else
            return FALSE;
        }
        break;
      case 128: {
        int i = 0, j = 0;
        while (j < h) {
	  int color, length, buffer_pos = 0;
          /* read color */
          if (!ReadFromRFBServer(client, (char*)buffer, REALBPP / 8 + 1))
            return FALSE;
          color = UncompressCPixel(buffer);
          buffer += REALBPP / 8;
	  buffer_pos += REALBPP / 8;
          /* read run length */
          length = 1;
          while (*buffer == 0xff && buffer_pos < client->raw_buffer_size-1) {
            if (!ReadFromRFBServer(client, (char*)buffer + 1, 1))
              return FALSE;
            length += *buffer;
            buffer++;
	    buffer_pos++;
          }
          length += *buffer;
          buffer++;
          while (j < h && length > 0) {
            ((CARDBPP *)client->frameBuffer)[(y + j) * client->width + x + i] =
                color;
            length--;
            i++;
            if (i >= w) {
              i = 0;
              j++;
            }
          }
          if (length > 0)
            rfbClientLog("Warning: possible TRLE corruption\n");
        }

        type = last_type;

        break;
      }
      case_129:
      case 129: {
        int i, j;
        /* read palettized pixels */
        i = j = 0;
        while (j < h) {
	  int color, length, buffer_pos = 0;
          /* read color */
          if (!ReadFromRFBServer(client, (char *)buffer, 1))
            return FALSE;
          color = palette[(*buffer) & 0x7f];
          length = 1;
          if (*buffer & 0x80) {
            if (!ReadFromRFBServer(client, (char *)buffer + 1, 1))
              return FALSE;
            buffer++;
	    buffer_pos++;
            /* read run length */
            while (*buffer == 0xff && buffer_pos < client->raw_buffer_size-1) {
              if (!ReadFromRFBServer(client, (char *)buffer + 1, 1))
                return FALSE;
              length += *buffer;
              buffer++;
	      buffer_pos++;
            }
            length += *buffer;
          }
          buffer++;
          while (j < h && length > 0) {
            ((CARDBPP *)client->frameBuffer)[(y + j) * client->width + x + i] =
                color;
            length--;
            i++;
            if (i >= w) {
              i = 0;
              j++;
            }
          }
          if (length > 0)
            rfbClientLog("Warning: possible TRLE corruption\n");
        }

        if (type == 129) {
          type = last_type;
        }

        break;
      }
      default:
        if (type <= 16) {
          int i;

          bpp = (type > 4 ? 4 : (type > 2 ? 2 : 1)),
          mask = (1 << bpp) - 1, divider = (8 / bpp);

          if (!ReadFromRFBServer(client, (char *)buffer, type * REALBPP / 8))
            return FALSE;

          /* read palette */
          for (i = 0; i < type; i++, buffer += REALBPP / 8)
            palette[i] = UncompressCPixel(buffer);

          last_type = type;
          goto case_127;
        } else if (type >= 130) {
          int i;

          if (!ReadFromRFBServer(client, (char *)buffer, (type - 128) * REALBPP / 8))
            return FALSE;

          /* read palette */
          for (i = 0; i < type - 128; i++, buffer += REALBPP / 8)
            palette[i] = UncompressCPixel(buffer);

          last_type = type;
          goto case_129;
        } else
          return FALSE;
      }
      last_type = type;
    }
  }

  return TRUE;
}

#undef CARDBPP
#undef CARDREALBPP
#undef HandleTRLE
#undef UncompressCPixel
#undef REALBPP
#undef UNCOMP
