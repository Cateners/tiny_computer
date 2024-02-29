#ifndef _RFB_CRYPTO_H
#define _RFB_CRYPTO_H 1

#include <stdint.h>
#include "rfb/rfbconfig.h"

#define SHA1_HASH_SIZE 20
#define MD5_HASH_SIZE 16

/* Generates an MD5 hash of 'in' and writes it to 'out', which must be 16 bytes in size. */
int hash_md5(void *out, const void *in, const size_t in_len);

/* Generates an SHA1 hash of 'in' and writes it to 'out', which must be 20 bytes in size. */
int hash_sha1(void *out, const void *in, const size_t in_len);

/* Fill 'out' with 'len' random bytes. */
void random_bytes(void *out, size_t len);

/*
  Takes the 8-byte key in 'key', reverses the bits in each byte of key as required by the RFB protocol,
  encrypts 'in' with the resulting key using single-key 56-bit DES and writes the result to 'out'.
 */
int encrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len);

/*
  Takes the 8-byte key in 'key', reverses the bits in each byte of key as required by the RFB protocol,
  decrypts 'in' with the resulting key using single-key 56-bit DES and writes the result to 'out'.
 */
int decrypt_rfbdes(void *out, int *out_len, const unsigned char key[8], const void *in, const size_t in_len);

/* Encrypts 'in' with the the 16-byte key in 'key' using AES-128-ECB and writes the result to 'out'. */
int encrypt_aes128ecb(void *out, int *out_len, const unsigned char key[16], const void *in, const size_t in_len);

/*
   Generates a Diffie-Hellman public-private keypair using the generator value 'gen' and prime modulo
   'prime', writing the result to 'pub_out' and 'priv_out', which must be 'keylen' in size.
 */
int dh_generate_keypair(uint8_t *priv_out, uint8_t *pub_out, const uint8_t *gen, const size_t gen_len, const uint8_t *prime, const size_t keylen);

/*
  Computes the shared Diffie-Hellman secret using the private key 'priv', the other side's public
  key 'pub' and the modulo prime 'prime' and writes it to 'shared_out', which must be 'keylen' in size.
 */
int dh_compute_shared_key(uint8_t *shared_out, const uint8_t *priv, const uint8_t *pub, const uint8_t *prime, const size_t keylen);

#endif
