/*
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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

#ifdef LIBVNCSERVER_HAVE_LIBZ

/*
 * zlib.c - handle zlib encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles an zlib
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleZlibBPP CONCAT2E(HandleZlib,BPP)
#define CARDBPP CONCAT3E(uint,BPP,_t)

static rfbBool
HandleZlibBPP (rfbClient* client, int rx, int ry, int rw, int rh)
{
  rfbZlibHeader hdr;
  int remaining;
  int inflateResult;
  int toRead;

  /* First make sure we have a large enough raw buffer to hold the
   * decompressed data.  In practice, with a fixed BPP, fixed frame
   * buffer size and the first update containing the entire frame
   * buffer, this buffer allocation should only happen once, on the
   * first update.
   */
  if ( client->raw_buffer_size < (( rw * rh ) * ( BPP / 8 ))) {

    if ( client->raw_buffer != NULL ) {

      free( client->raw_buffer );

    }

    client->raw_buffer_size = (( rw * rh ) * ( BPP / 8 ));
    client->raw_buffer = (char*) malloc( client->raw_buffer_size );

  }

  if (!ReadFromRFBServer(client, (char *)&hdr, sz_rfbZlibHeader))
    return FALSE;

  remaining = rfbClientSwap32IfLE(hdr.nBytes);

  /* Need to initialize the decompressor state. */
  client->decompStream.next_in   = ( Bytef * )client->buffer;
  client->decompStream.avail_in  = 0;
  client->decompStream.next_out  = ( Bytef * )client->raw_buffer;
  client->decompStream.avail_out = client->raw_buffer_size;
  client->decompStream.data_type = Z_BINARY;

  /* Initialize the decompression stream structures on the first invocation. */
  if ( client->decompStreamInited == FALSE ) {

    inflateResult = inflateInit( &client->decompStream );

    if ( inflateResult != Z_OK ) {
      rfbClientLog(
              "inflateInit returned error: %d, msg: %s\n",
              inflateResult,
              client->decompStream.msg);
      return FALSE;
    }

    client->decompStreamInited = TRUE;

  }

  inflateResult = Z_OK;

  /* Process buffer full of data until no more to process, or
   * some type of inflater error, or Z_STREAM_END.
   */
  while (( remaining > 0 ) &&
         ( inflateResult == Z_OK )) {
  
    if ( remaining > RFB_BUFFER_SIZE ) {
      toRead = RFB_BUFFER_SIZE;
    }
    else {
      toRead = remaining;
    }

    /* Fill the buffer, obtaining data from the server. */
    if (!ReadFromRFBServer(client, client->buffer,toRead))
      return FALSE;

    client->decompStream.next_in  = ( Bytef * )client->buffer;
    client->decompStream.avail_in = toRead;

    /* Need to uncompress buffer full. */
    inflateResult = inflate( &client->decompStream, Z_SYNC_FLUSH );

    /* We never supply a dictionary for compression. */
    if ( inflateResult == Z_NEED_DICT ) {
      rfbClientLog("zlib inflate needs a dictionary!\n");
      return FALSE;
    }
    if ( inflateResult < 0 ) {
      rfbClientLog(
              "zlib inflate returned error: %d, msg: %s\n",
              inflateResult,
              client->decompStream.msg);
      return FALSE;
    }

    /* Result buffer allocated to be at least large enough.  We should
     * never run out of space!
     */
    if (( client->decompStream.avail_in > 0 ) &&
        ( client->decompStream.avail_out <= 0 )) {
      rfbClientLog("zlib inflate ran out of space!\n");
      return FALSE;
    }

    remaining -= toRead;

  } /* while ( remaining > 0 ) */

  if ( inflateResult == Z_OK ) {

    /* Put the uncompressed contents of the update on the screen. */
    client->GotBitmap(client, (uint8_t *)client->raw_buffer, rx, ry, rw, rh);
  }
  else {

    rfbClientLog(
            "zlib inflate returned error: %d, msg: %s\n",
            inflateResult,
            client->decompStream.msg);
    return FALSE;

  }

  return TRUE;
}

#undef CARDBPP

#endif
