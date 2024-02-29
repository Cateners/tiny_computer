/*
 * rfbssl_none.c - Secure socket functions (fallback to failing)
 */

/*
 *  Copyright (C) 2011 Johannes Schindelin
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

#include "rfbssl.h"

struct rfbssl_ctx *rfbssl_init_global(char *key, char *cert)
{
    return NULL;
}

int rfbssl_init(rfbClientPtr cl)
{
    return -1;
}

int rfbssl_write(rfbClientPtr cl, const char *buf, int bufsize)
{
    return -1;
}

int rfbssl_peek(rfbClientPtr cl, char *buf, int bufsize)
{
    return -1;
}

int rfbssl_read(rfbClientPtr cl, char *buf, int bufsize)
{
    return -1;
}

int rfbssl_pending(rfbClientPtr cl)
{
    return -1;
}

void rfbssl_destroy(rfbClientPtr cl)
{
}
