/*
 * crypto_gnutls.c - Crypto wrapper (libgcrypt version)
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
#include <gcrypt.h>
#include "crypto.h"

static int mpiToBytes(const gcry_mpi_t value, uint8_t *result, size_t size)
{
    gcry_error_t error;
    size_t len;
    int i;

    error = gcry_mpi_print(GCRYMPI_FMT_USG, result, size, &len, value);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	return 0;
    for (i=size-1;i>(int)size-1-(int)len;--i)
	result[i] = result[i-size+len];
    for (;i>=0;--i)
	result[i] = 0;
    return 1;
}

static unsigned char reverseByte(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

int hash_md5(void *out, const void *in, const size_t in_len)
{
    int result = 0;
    gcry_error_t error;
    gcry_md_hd_t md5 = NULL;
    void *digest;

    error = gcry_md_open(&md5, GCRY_MD_MD5, 0);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    gcry_md_write(md5, in, in_len);

    if(!(digest = gcry_md_read(md5, GCRY_MD_MD5)))
       goto out;

    memcpy(out, digest, gcry_md_get_algo_dlen(GCRY_MD_MD5));

    result = 1;

 out:
    gcry_md_close(md5);
    return result;
}

int hash_sha1(void *out, const void *in, const size_t in_len)
{
    int result = 0;
    gcry_error_t error;
    gcry_md_hd_t sha1 = NULL;
    void *digest;

    error = gcry_md_open(&sha1, GCRY_MD_SHA1, 0);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    gcry_md_write(sha1, in, in_len);

    if(!(digest = gcry_md_read(sha1, GCRY_MD_SHA1)))
       goto out;

    memcpy(out, digest, gcry_md_get_algo_dlen(GCRY_MD_SHA1));

    result = 1;

 out:
    gcry_md_close(sha1);
    return result;
}

void random_bytes(void *out, size_t len)
{
    gcry_randomize(out, len, GCRY_STRONG_RANDOM);
}

int encrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len)
{
    int result = 0;
    gcry_error_t error;
    gcry_cipher_hd_t des = NULL;
    unsigned char mungedkey[8];
    int i;

    for (i = 0; i < 8; i++)
      mungedkey[i] = reverseByte(key[i]);

    error = gcry_cipher_open(&des, GCRY_CIPHER_DES, GCRY_CIPHER_MODE_ECB, 0);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_setkey(des, mungedkey, 8);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_encrypt(des, out, in_len, in, in_len);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    *out_len = in_len;

    result = 1;

 out:
    gcry_cipher_close(des);
    return result;
}

int decrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len)
{
    int result = 0;
    gcry_error_t error;
    gcry_cipher_hd_t des = NULL;
    unsigned char mungedkey[8];
    int i;

    for (i = 0; i < 8; i++)
      mungedkey[i] = reverseByte(key[i]);

    error = gcry_cipher_open(&des, GCRY_CIPHER_DES, GCRY_CIPHER_MODE_ECB, 0);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_setkey(des, mungedkey, 8);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_decrypt(des, out, in_len, in, in_len);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    *out_len = in_len;

    result = 1;

 out:
    gcry_cipher_close(des);
    return result;
}


int encrypt_aes128ecb(void *out, int *out_len, const unsigned char key[16], const void *in, const size_t in_len)
{
    int result = 0;
    gcry_error_t error;
    gcry_cipher_hd_t aes = NULL;

    error = gcry_cipher_open(&aes, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, 0);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_setkey(aes, key, 16);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    error = gcry_cipher_encrypt(aes, out, in_len, in, in_len);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;
    *out_len = in_len;

    result = 1;

 out:
    gcry_cipher_close(aes);
    return result;
}

int dh_generate_keypair(uint8_t *priv_out, uint8_t *pub_out, const uint8_t *gen, const size_t gen_len, const uint8_t *prime, const size_t keylen)
{
    int result = 0;
    gcry_error_t error;
    gcry_mpi_t genmpi = NULL, modmpi = NULL, privmpi = NULL, pubmpi = NULL;

    error = gcry_mpi_scan(&genmpi, GCRYMPI_FMT_USG, gen, gen_len, NULL);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;
    error = gcry_mpi_scan(&modmpi, GCRYMPI_FMT_USG, prime, keylen, NULL);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    privmpi = gcry_mpi_new(keylen);
    if (!privmpi)
	goto out;
    gcry_mpi_randomize(privmpi, (keylen/8)*8, GCRY_STRONG_RANDOM);

    pubmpi = gcry_mpi_new(keylen);
    if (!pubmpi)
	goto out;

    gcry_mpi_powm(pubmpi, genmpi, privmpi, modmpi);

    if (!mpiToBytes(pubmpi, pub_out, keylen))
	goto out;
    if (!mpiToBytes(privmpi, priv_out, keylen))
	goto out;

    result = 1;

 out:
    gcry_mpi_release(genmpi);
    gcry_mpi_release(modmpi);
    gcry_mpi_release(privmpi);
    gcry_mpi_release(pubmpi);
    return result;
}

int dh_compute_shared_key(uint8_t *shared_out, const uint8_t *priv, const uint8_t *pub, const uint8_t *prime, const size_t keylen)
{
    int result = 1;
    gcry_error_t error;
    gcry_mpi_t keympi = NULL, modmpi = NULL, privmpi = NULL, pubmpi = NULL;

    error = gcry_mpi_scan(&privmpi, GCRYMPI_FMT_USG, priv, keylen, NULL);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;
    error = gcry_mpi_scan(&pubmpi, GCRYMPI_FMT_USG, pub, keylen, NULL);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;
    error = gcry_mpi_scan(&modmpi, GCRYMPI_FMT_USG, prime, keylen, NULL);
    if (gcry_err_code(error) != GPG_ERR_NO_ERROR)
	goto out;

    keympi = gcry_mpi_new(keylen);
    if (!keympi)
	goto out;

    gcry_mpi_powm(keympi, pubmpi, privmpi, modmpi);

    if (!mpiToBytes(keympi, shared_out, keylen))
	goto out;

    result = 1;

 out:
    gcry_mpi_release(keympi);
    gcry_mpi_release(modmpi);
    gcry_mpi_release(privmpi);
    gcry_mpi_release(pubmpi);

    return result;
}
