/*
 * crypto_included.c - Crypto wrapper (included version)
 */

/*
 *  Copyright (C) 2011 Gernot Tenchio
 *  Copyright (C) 2019 Christian Beier
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

#include <string.h>
#include "sha.h"
#include "d3des.h"
#include "crypto.h"


int hash_md5(void *out, const void *in, const size_t in_len)
{
    return 0;
}

int hash_sha1(void *out, const void *in, const size_t in_len)
{
    SHA1Context sha1;
    if(SHA1Reset(&sha1) != shaSuccess)
	return 0;
    if(SHA1Input(&sha1, in, in_len) != shaSuccess)
	return 0;
    if(SHA1Result(&sha1, out) != shaSuccess)
	return 0;

    return 1;
}

void random_bytes(void *out, size_t len)
{

}

int encrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len)
{
    int eightbyteblocks = in_len/8;
    int i;
    rfbDesKey((unsigned char*)key, EN0);
    for(i = 0; i < eightbyteblocks; ++i)
	rfbDes((unsigned char*)in + i*8, (unsigned char*)out + i*8);

    *out_len = in_len;

    return 1;
}

int decrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len)
{
    int eightbyteblocks = in_len/8;
    int i;
    rfbDesKey((unsigned char*)key, DE1);
    for(i = 0; i < eightbyteblocks; ++i)
	rfbDes((unsigned char*)in + i*8, (unsigned char*)out + i*8);

    *out_len = in_len;

    return 1;
}

int encrypt_aes128ecb(void *out, int *out_len, const unsigned char key[16], const void *in, const size_t in_len)
{
    return 0;
}

int dh_generate_keypair(uint8_t *priv_out, uint8_t *pub_out, const uint8_t *gen, const size_t gen_len, const uint8_t *prime, const size_t keylen)
{
    return 0;
}

int dh_compute_shared_key(uint8_t *shared_out, const uint8_t *priv, const uint8_t *pub, const uint8_t *prime, const size_t keylen)
{
    return 0;
}
