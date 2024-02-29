/*
 * Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifndef __ZRLE_OUT_STREAM_H__
#define __ZRLE_OUT_STREAM_H__

#include <zlib.h>
#include "zrletypes.h"
#include "rfb/rfb.h"

typedef struct {
  zrle_U8 *start;
  zrle_U8 *ptr;
  zrle_U8 *end;
} zrleBuffer;

typedef struct {
  zrleBuffer in;
  zrleBuffer out;

  z_stream   zs;
} zrleOutStream;

#define ZRLE_BUFFER_LENGTH(b) ((b)->ptr - (b)->start)

zrleOutStream *zrleOutStreamNew           (void);
void           zrleOutStreamFree          (zrleOutStream *os);
rfbBool        zrleOutStreamFlush         (zrleOutStream *os);
void           zrleOutStreamWriteBytes    (zrleOutStream *os,
					   const zrle_U8 *data,
					   int            length);
void           zrleOutStreamWriteU8       (zrleOutStream *os,
					   zrle_U8        u);
void           zrleOutStreamWriteOpaque8  (zrleOutStream *os,
					   zrle_U8        u);
void           zrleOutStreamWriteOpaque16 (zrleOutStream *os,
					   zrle_U16       u);
void           zrleOutStreamWriteOpaque32 (zrleOutStream *os,
					   zrle_U32       u);
void           zrleOutStreamWriteOpaque24A(zrleOutStream *os,
					   zrle_U32       u);
void           zrleOutStreamWriteOpaque24B(zrleOutStream *os,
					   zrle_U32       u);

#endif /* __ZRLE_OUT_STREAM_H__ */
