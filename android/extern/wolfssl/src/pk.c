/* pk.c
 *
 * Copyright (C) 2006-2022 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#if !defined(WOLFSSL_PK_INCLUDED)
    #ifndef WOLFSSL_IGNORE_FILE_WARN
        #warning pk.c does not need to be compiled separately from ssl.c
    #endif
#else

#ifndef NO_RSA
    #include <wolfssl/wolfcrypt/rsa.h>
#endif

#if defined(OPENSSL_EXTRA) && !defined(NO_BIO) && defined(WOLFSSL_KEY_GEN) && \
    (!defined(HAVE_USER_RSA) || defined(HAVE_ECC) || \
     (!defined(NO_DSA) && !defined(HAVE_SELFTEST)))
/* Forward declaration for wolfSSL_PEM_write_bio_RSA_PUBKEY,
 * wolfSSL_PEM_write_bio_DSA_PUBKEY and wolfSSL_PEM_write_bio_EC_PUBKEY.
 * Implementation in ssl.c.
 */
static int pem_write_bio_pubkey(WOLFSSL_BIO* bio, WOLFSSL_EVP_PKEY* key);
#endif

/*******************************************************************************
 * COMMON FUNCTIONS
 ******************************************************************************/

#if defined(OPENSSL_EXTRA)

#if (!defined(NO_FILESYSTEM) && (defined(OPENSSL_EXTRA) || \
     defined(OPENSSL_ALL))) || (!defined(NO_BIO) && defined(OPENSSL_EXTRA))
/* Convert the PEM encoding in the buffer to DER.
 *
 * @param [in]  pem        Buffer containing PEM encoded data.
 * @param [in]  pemSz      Size of data in buffer in bytes.
 * @param [in]  cb         Password callback when PEM encrypted.
 * @param [in]  pass       NUL terminated string for passphrase when PEM
 *                         encrypted.
 * @param [in]  keyType    Type of key to match against PEM header/footer.
 * @param [out] keyFormat  Format of key.
 * @param [out] der        Buffer holding DER encoding.
 * @return  Negative on failure.
 * @return  Number of bytes consumed on success.
 */
static int pem_mem_to_der(const char* pem, int pemSz, wc_pem_password_cb* cb,
    void* pass, int keyType, int* keyFormat, DerBuffer** der)
{
#ifdef WOLFSSL_SMALL_STACK
    EncryptedInfo* info = NULL;
#else
    EncryptedInfo info[1];
#endif /* WOLFSSL_SMALL_STACK */
    wc_pem_password_cb* localCb = NULL;
    int ret = 0;

    if (cb != NULL) {
        localCb = cb;
    }
    else if (pass != NULL) {
        localCb = wolfSSL_PEM_def_callback;
    }

#ifdef WOLFSSL_SMALL_STACK
    info = (EncryptedInfo*)XMALLOC(sizeof(EncryptedInfo), NULL,
        DYNAMIC_TYPE_ENCRYPTEDINFO);
    if (info == NULL) {
        WOLFSSL_ERROR_MSG("Error getting memory for EncryptedInfo structure");
        ret = MEMORY_E;
    }
#endif /* WOLFSSL_SMALL_STACK */

    if (ret == 0) {
        XMEMSET(info, 0, sizeof(EncryptedInfo));
        info->passwd_cb       = localCb;
        info->passwd_userdata = pass;

        /* Do not strip PKCS8 header */
        ret = PemToDer((const unsigned char *)pem, pemSz, keyType, der, NULL,
            info, keyFormat);
        if (ret < 0) {
            WOLFSSL_ERROR_MSG("Bad PEM To DER");
        }
    }
    if (ret >= 0) {
        ret = (int)info->consumed;
    }

#ifdef WOLFSSL_SMALL_STACK
    XFREE(info, NULL, DYNAMIC_TYPE_ENCRYPTEDINFO);
#endif

    return ret;
}
#endif

#if !defined(NO_RSA) || !defined(WOLFCRYPT_ONLY)
#ifndef NO_BIO
/* Read PEM data from a BIO and decode to DER in a new buffer.
 *
 * @param [in, out] bio        BIO object to read with.
 * @param [in]      cb         Password callback when PEM encrypted.
 * @param [in]      pass       NUL terminated string for passphrase when PEM
 *                             encrypted.
 * @param [in]      keyType    Type of key to match against PEM header/footer.
 * @param [out]     keyFormat  Format of key.
 * @param [out]     der        Buffer holding DER encoding.
 * @return  Negative on failure.
 * @return  Number of bytes consumed on success.
 */
static int pem_read_bio_key(WOLFSSL_BIO* bio, wc_pem_password_cb* cb,
    void* pass, int keyType, int* keyFormat, DerBuffer** der)
{
    int ret;
    char* mem = NULL;
    int memSz;
    int alloced = 0;

    ret = wolfssl_read_bio(bio, &mem, &memSz, &alloced);
    if (ret == 0) {
        ret = pem_mem_to_der(mem, memSz, cb, pass, keyType, keyFormat, der);
        /* Write left over data back to BIO if not a file BIO */
        if ((ret > 0) && ((memSz - ret) > 0) &&
                 (bio->type != WOLFSSL_BIO_FILE)) {
            int res;
            res = wolfSSL_BIO_write(bio, mem + ret, memSz - ret);
            if (res != memSz - ret) {
                WOLFSSL_ERROR_MSG("Unable to write back excess data");
                if (res < 0) {
                    ret = res;
                }
                else {
                    ret = MEMORY_E;
                }
            }
        }
        if (alloced) {
            XFREE(mem, NULL, DYNAMIC_TYPE_OPENSSL);
        }
    }

    return ret;
}
#endif /* !NO_BIO */

#if !defined(NO_FILESYSTEM)
/* Read PEM data from a file and decode to DER in a new buffer.
 *
 * @param [in]  fp         File pointer to read with.
 * @param [in]  cb         Password callback when PEM encrypted.
 * @param [in]  pass       NUL terminated string for passphrase when PEM
 *                         encrypted.
 * @param [in]  keyType    Type of key to match against PEM header/footer.
 * @param [out] keyFormat  Format of key.
 * @param [out] der        Buffer holding DER encoding.
 * @return  Negative on failure.
 * @return  Number of bytes consumed on success.
 */
static int pem_read_file_key(XFILE fp, wc_pem_password_cb* cb, void* pass,
    int keyType, int* keyFormat, DerBuffer** der)
{
    int ret;
    char* mem = NULL;
    int memSz;

    ret = wolfssl_read_file(fp, &mem, &memSz);
    if (ret == 0) {
        ret = pem_mem_to_der(mem, memSz, cb, pass, keyType, keyFormat, der);
        XFREE(mem, NULL, DYNAMIC_TYPE_OPENSSL);
    }

    return ret;
}
#endif /* !NO_FILESYSTEM */
#endif

#if defined(OPENSSL_EXTRA) && ((!defined(NO_RSA) && defined(WOLFSSL_KEY_GEN) \
    && !defined(HAVE_USER_RSA)) || !defined(WOLFCRYPT_ONLY))
/* Convert DER data to PEM in an allocated buffer.
 *
 * @param [in]  der    Buffer containing DER data.
 * @param [in]  derSz  Size of DER data in bytes.
 * @param [in]  type   Type of key being encoded.
 * @param [in]  heap   Heap hint for dynamic memory allocation.
 * @param [out] out    Allocated buffer containing PEM.
 * @param [out] outSz  Size of PEM encoding.
 * @return  WOLFSSL_FAILURE on error.
 * @return  WOLFSSL_SUCCESS on success.
 */
static int der_to_pem_alloc(const unsigned char* der, int derSz, int type,
    void* heap, byte** out, int* outSz)
{
    int ret = WOLFSSL_SUCCESS;
    int pemSz;
    byte* pem = NULL;

    (void)heap;

    pemSz = wc_DerToPem(der, derSz, NULL, 0, type);
    if (pemSz < 0) {
        ret = WOLFSSL_FAILURE;
    }

    if (ret == WOLFSSL_SUCCESS) {
        pem = (byte*)XMALLOC(pemSz, heap, DYNAMIC_TYPE_TMP_BUFFER);
        if (pem == NULL) {
            ret = WOLFSSL_FAILURE;
        }
    }

    if ((ret == WOLFSSL_SUCCESS) && (wc_DerToPem(der, derSz, pem, pemSz,
            type) < 0)) {
        ret = WOLFSSL_FAILURE;
        XFREE(pem, heap, DYNAMIC_TYPE_TMP_BUFFER);
        pem = NULL;
    }

    *out = pem;
    *outSz = pemSz;
    return ret;
}

#ifndef NO_BIO
/* Write the DER data as PEM into BIO.
 *
 * @param [in]      der    Buffer containing DER data.
 * @param [in]      derSz  Size of DER data in bytes.
 * @param [in, out] bio    BIO object to write with.
 * @param [in]      type   Type of key being encoded.
 * @return  WOLFSSL_FAILURE on error.
 * @return  WOLFSSL_SUCCESS on success.
 */
static int der_write_to_bio_as_pem(const unsigned char* der, int derSz,
    WOLFSSL_BIO* bio, int type)
{
    int ret;
    int pemSz;
    byte* pem = NULL;

    ret = der_to_pem_alloc(der, derSz, type, bio->heap, &pem, &pemSz);
    if (ret == WOLFSSL_SUCCESS) {
        int len = wolfSSL_BIO_write(bio, pem, pemSz);
        if (len != pemSz) {
            WOLFSSL_ERROR_MSG("Unable to write full PEM to BIO");
            ret = WOLFSSL_FAILURE;
        }
    }

    XFREE(pem, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}
#endif
#endif

#if (!defined(NO_RSA) && defined(WOLFSSL_KEY_GEN) && \
     !defined(HAVE_USER_RSA)) || (!defined(NO_DH) && defined(WOLFSSL_DH_EXTRA))
#if !defined(NO_FILESYSTEM)
/* Write the DER data as PEM into file pointer.
 *
 * @param [in] der    Buffer containing DER data.
 * @param [in] derSz  Size of DER data in bytes.
 * @param [in] fp     File pointer to write with.
 * @param [in] type   Type of key being encoded.
 * @param [in] heap   Heap hint for dynamic memory allocation.
 * @return  WOLFSSL_FAILURE on error.
 * @return  WOLFSSL_SUCCESS on success.
 */
static int der_write_to_file_as_pem(const unsigned char* der, int derSz,
    XFILE fp, int type, void* heap)
{
    int ret;
    int pemSz;
    byte* pem = NULL;

    ret = der_to_pem_alloc(der, derSz, type, heap, &pem, &pemSz);
    if (ret == WOLFSSL_SUCCESS) {
        int len = (int)XFWRITE(pem, 1, pemSz, fp);
        if (len != pemSz) {
            WOLFSSL_ERROR_MSG("Unable to write full PEM to BIO");
            ret = WOLFSSL_FAILURE;
        }
    }

    XFREE(pem, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}
#endif
#endif

#if !defined(NO_CERTS) && defined(XFPRINTF) && !defined(NO_FILESYSTEM) && \
    !defined(NO_STDIO_FILESYSTEM) && (!defined(NO_RSA) || !defined(NO_DSA) || \
    defined(HAVE_ECC))
/* Print the number bn in hex with name field and indentation indent to file fp.
 *
 * Used by wolfSSL_DSA_print_fp, wolfSSL_RSA_print_fp and
 * wolfSSL_EC_KEY_print_fp to print DSA, RSA and ECC keys and parameters.
 *
 * @param [in] fp      File pointer to write to.
 * @param [in] indent  Number of spaces to prepend to each line.
 * @param [in] field   Name of field.
 * @param [in] bn      Big number to print.
 * @return  1 on success.
 * @return  0 on failure.
 * @return  BAD_FUNC_ARG when fp is invalid, indent is less than 0, or field or
 *          bn or NULL.
 */
static int pk_bn_field_print_fp(XFILE fp, int indent, const char* field,
    const WOLFSSL_BIGNUM* bn)
{
    static const int HEX_INDENT = 4;
    static const int MAX_DIGITS_PER_LINE = 30;

    int ret = 1;
    int i = 0;
    char* buf = NULL;

    /* Internal function - assume parameters are valid. */

    /* Convert BN to hexadecimal character array (allocates buffer). */
    buf = wolfSSL_BN_bn2hex(bn);
    if (buf == NULL) {
        ret = 0;
    }
    if (ret == 1) {
        /* Print leading spaces, name and spaces before data. */
        if (indent > 0) {
            if (XFPRINTF(fp, "%*s", indent, "") < 0)
                ret = 0;
        }
    }
    if (ret == 1) {
        if (XFPRINTF(fp, "%s:\n", field) < 0)
            ret = 0;
    }
    if (ret == 1) {
        if (indent > 0) {
            if (XFPRINTF(fp, "%*s", indent, "") < 0)
                ret = 0;
        }
    }
    if (ret == 1) {
        if (XFPRINTF(fp, "%*s", HEX_INDENT, "") < 0)
            ret = 0;
    }
    if (ret == 1) {
        /* Print first byte - should always exist. */
        if ((buf[i] != '\0') && (buf[i+1] != '\0')) {
            if (XFPRINTF(fp, "%c", buf[i++]) < 0)
                ret = 0;
            else if (XFPRINTF(fp, "%c", buf[i++]) < 0)
                    ret = 0;
        }
    }
    if (ret == 1) {
        /* Print each hexadecimal character with byte separator. */
        while ((buf[i] != '\0') && (buf[i+1] != '\0')) {
            /* Byte separator every two nibbles - one byte. */
            if (XFPRINTF(fp, ":") < 0) {
                ret = 0;
                break;
            }
            /* New line after every 15 bytes - 30 nibbles. */
            if (i % MAX_DIGITS_PER_LINE == 0) {
                if (XFPRINTF(fp, "\n") < 0) {
                    ret = 0;
                    break;
                }
                if (indent > 0) {
                    if (XFPRINTF(fp, "%*s", indent, "") < 0) {
                        ret = 0;
                        break;
                    }
                }
                if (XFPRINTF(fp, "%*s", HEX_INDENT, "") < 0) {
                    ret = 0;
                    break;
                }
            }
            /* Print two nibbles - one byte. */
            if (XFPRINTF(fp, "%c", buf[i++]) < 0) {
                ret = 0;
                break;
            }
            if (XFPRINTF(fp, "%c", buf[i++]) < 0) {
                ret = 0;
                break;
            }
        }
        /* Ensure on new line after data. */
        if (XFPRINTF(fp, "\n") < 0) {
            ret = 0;
        }
    }

    /* Dispose of any allocated character array. */
    XFREE(buf, NULL, DYNAMIC_TYPE_OPENSSL);

    return ret;
}
#endif /* !NO_CERTS && XFPRINTF && !NO_FILESYSTEM && !NO_STDIO_FILESYSTEM &&
        * (!NO_DSA || !NO_RSA || HAVE_ECC) */
#endif /* OPENSSL_EXTRA || WOLFSSL_WPAS_SMALL */

#if defined(OPENSSL_EXTRA)
#if defined(XSNPRINTF) && !defined(NO_BIO) && !defined(NO_RSA) && \
    !defined(HAVE_FAST_RSA)
/* snprintf() must be available */

/* Maximum number of extra indent spaces on each line. */
#define PRINT_NUM_MAX_INDENT        48
/* Maximum size of a line containing a value. */
#define PRINT_NUM_MAX_VALUE_LINE    PRINT_NUM_MAX_INDENT
/* Number of leading spaces on each line. */
#define PRINT_NUM_INDENT_CNT        4
/* Indent spaces for number lines. */
#define PRINT_NUM_INDENT            "    "
/* 4 leading spaces and 15 bytes with colons is a complete line. */
#define PRINT_NUM_MAX_DIGIT_LINE   (PRINT_NUM_INDENT_CNT + 3 * 15)

/* Print indent to BIO.
 *
 * @param [in] bio      BIO object to write to.
 * @param [in] line     Buffer to put characters to before writing to BIO.
 * @param [in] lineLen  Length of buffer.
 * @return  1 on success.
 * @return  0 on failure.
 */
static int wolfssl_print_indent(WOLFSSL_BIO* bio, char* line, int lineLen,
    int indent)
{
    int ret = 1;

    if (indent > 0) {
        /* Print indent spaces. */
        int len_wanted = XSNPRINTF(line, lineLen, "%*s", indent, " ");
        if (len_wanted >= lineLen) {
            WOLFSSL_ERROR_MSG("Buffer overflow formatting indentation");
            ret = 0;
        }
        else {
            /* Write indents string to BIO */
            if (wolfSSL_BIO_write(bio, line, len_wanted) <= 0) {
                ret = 0;
            }
        }
    }

    return ret;
}

/* Print out name, and value in decimal and hex to BIO.
 *
 * @param [in] bio     BIO object to write to.
 * @param [in] value   MP integer to write.
 * @param [in] name    Name of value.
 * @param [in] indent  Number of leading spaces before line.
 * @return  1 on success.
 * @return  0 on failure.
 */
static int wolfssl_print_value(WOLFSSL_BIO* bio, mp_int* value,
    const char* name, int indent)
{
    int ret = 1;
    int len;
    char line[PRINT_NUM_MAX_VALUE_LINE + 1];
    word32 v;

    /* Get the length of hex encoded value. */
    len = mp_unsigned_bin_size(value);
    /* Value must no more than 32-bits - 4 bytes. */
    if ((len < 0) || (len > 4)) {
        WOLFSSL_ERROR_MSG("Error getting exponent size");
        ret = 0;
    }
    if (ret == 1) {
        /* Print any indent spaces. */
        ret = wolfssl_print_indent(bio, line, sizeof(line), indent);
    }
    if (ret == 1) {
        /* Get 32-bits of value. */
        v = (word32)value->dp[0];
        /* Print the line to the string. */
        len = (int)XSNPRINTF(line, sizeof(line), "%s %u (0x%x)\n", name, v,
            v);
        if (len >= (int)sizeof(line)) {
            WOLFSSL_ERROR_MSG("Buffer overflow while formatting value");
            ret = 0;
        } else {
            /* Write string to BIO */
            if (wolfSSL_BIO_write(bio, line, len) <= 0) {
                ret = 0;
            }
        }
    }

    return ret;
}

/* Print out name and multi-precision number to BIO.
 *
 * @param [in] bio     BIO object to write to.
 * @param [in] num     MP integer to write.
 * @param [in] name    Name of value.
 * @param [in] indent  Number of leading spaces before each line.
 * @return  1 on success.
 * @return  0 on failure.
 */
static int wolfssl_print_number(WOLFSSL_BIO* bio, mp_int* num, const char* name,
    int indent)
{
    int ret = 1;
    int rawLen = 0;
    byte* rawKey = NULL;
    char line[PRINT_NUM_MAX_DIGIT_LINE + 1];
    int li = 0; /* Line index. */
    int i;

    /* Allocate a buffer to hold binary encoded data. */
    rawLen = mp_unsigned_bin_size(num);
    if (rawLen == 0) {
        WOLFSSL_ERROR_MSG("Invalid number");
        ret = 0;
    }
    if (ret == 1) {
        rawKey = (byte*)XMALLOC(rawLen, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (rawKey == NULL) {
            WOLFSSL_ERROR_MSG("Memory allocation error");
            ret = 0;
        }
    }
    /* Encode number as big-endian byte array. */
    if ((ret == 1) && (mp_to_unsigned_bin(num, rawKey) < 0)) {
        ret = 0;
    }

    if (ret == 1) {
        /* Print any indent spaces. */
        ret = wolfssl_print_indent(bio, line, sizeof(line), indent);
    }
    if (ret == 1) {
        /* Print header string line to string. */
        li = XSNPRINTF(line, sizeof(line), "%s\n", name);
        if (li >= (int)sizeof(line)) {
            WOLFSSL_ERROR_MSG("Buffer overflow formatting name");
            ret = 0;
        }
        else {
            if (wolfSSL_BIO_write(bio, line, li) <= 0) {
                ret = 0;
            }
        }
    }
    if (ret == 1) {
        /* Print any indent spaces. */
        ret = wolfssl_print_indent(bio, line, sizeof(line), indent);
    }
    if (ret == 1) {
        /* Start first digit line with spaces.
         * Writing out zeros ensures number is a positive value. */
        li = XSNPRINTF(line, sizeof(line), PRINT_NUM_INDENT "%s",
            mp_leading_bit(num) ?  "00:" : "");
        if (li >= (int)sizeof(line)) {
            WOLFSSL_ERROR_MSG("Buffer overflow formatting spaces");
            ret = 0;
        }
    }

    /* Put out each line of numbers. */
    for (i = 0; (ret == 1) && (i < rawLen); i++) {
        /* Encode another byte as 2 hex digits and append colon. */
        int len_wanted = XSNPRINTF(line + li, sizeof(line) - li, "%02x:",
                                   rawKey[i]);
        /* Check if there was room -- if not, print the current line, not
         * including the newest octet.
         */
        if (len_wanted >= (int)sizeof(line) - li) {
            /* bump current octet to the next line. */
            --i;
            /* More bytes coming so add a line break. */
            line[li++] = '\n';
            /* Write out the line. */
            if (wolfSSL_BIO_write(bio, line, li) <= 0) {
                ret = 0;
            }
            if (ret == 1) {
                /* Print any indent spaces. */
                ret = wolfssl_print_indent(bio, line, sizeof(line), indent);
            }
            /* Put the leading spaces on new line. */
            XSTRNCPY(line, PRINT_NUM_INDENT, PRINT_NUM_INDENT_CNT + 1);
            li = PRINT_NUM_INDENT_CNT;
        }
        else {
            li += len_wanted;
        }
    }

    if (ret == 1) {
        /* Put out last line - replace last colon with carriage return. */
        line[li-1] = '\n';
        if (wolfSSL_BIO_write(bio, line, li) <= 0) {
            ret = 0;
        }
    }

    /* Dispose of any allocated data. */
    XFREE(rawKey, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}

#endif /* XSNPRINTF && !NO_BIO && !NO_RSA && !HAVE_FAST_RSA */

#if !defined(NO_RSA) || (!defined(NO_DH) && !defined(NO_CERTS) && \
    defined(HAVE_FIPS) && !FIPS_VERSION_GT(2,0))

/* Uses the DER SEQUENCE to determine size of DER data.
 *
 * Outer SEQUENCE encapsulates all the DER encoding.
 * Add the length of the SEQUENCE data to the length of the SEQUENCE header.
 *
 * @param [in] seq  Buffer holding DER encoded sequence.
 * @param [in] len  Length of data in buffer (may be larger than SEQ).
 * @return  Size of complete DER encoding on success.
 * @return  0 on failure.
 */
static int wolfssl_der_length(const unsigned char* seq, int len)
{
    int ret = 0;
    word32 i = 0;

    /* Check it is a SEQUENCE and get the length of the underlying data.
     * i is updated to be after SEQUENCE header bytes.
     */
    if (GetSequence_ex(seq, &i, &ret, len, 0) >= 0) {
        /* Add SEQUENCE header length to underlying data length. */
        ret += (int)i;
    }

    return ret;
}

#endif /* !NO_RSA */

#endif /* OPENSSL_EXTRA */

#if !defined(NO_RSA) || !defined(NO_DH)
/* Too many defines to check explicitly - prototype it and always include
 * for RSA and DH. */
WC_RNG* wolfssl_make_rng(WC_RNG* rng, int* local);

/* Make a random number generator or get global if possible.
 *
 * Global may not be available and NULL will be returned.
 *
 * @param [in, out] rng    Local random number generator.
 * @param [out]     local  Local random number generator returned.
 * @return  NULL on failure.
 * @return  A random number generator object.
 */
WC_RNG* wolfssl_make_rng(WC_RNG* rng, int* local)
{
    WC_RNG* ret = NULL;

    /* Assume not local until one created. */
    *local = 0;

#ifdef WOLFSSL_SMALL_STACK
    /* Allocate RNG object . */
    rng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
#endif
    /* Check we have a local RNG object and initialize. */
    if ((rng != NULL) && (wc_InitRng(rng) == 0)) {
        ret = rng;
        *local = 1;
    }
    if (ret == NULL) {
    #ifdef HAVE_GLOBAL_RNG
        WOLFSSL_MSG("Bad RNG Init, trying global");
        /* Get the global random number generator instead. */
        ret = wolfssl_get_global_rng();
        if (ret == NULL) {
            /* Create a global random if possible. */
            (void)wolfSSL_RAND_Init();
            ret = wolfssl_get_global_rng();
        }
    #else
        WOLFSSL_ERROR_MSG("Bad RNG Init");
    #endif
    }

    if (ret != rng) {
#ifdef WOLFSSL_SMALL_STACK
        XFREE(rng, NULL, DYNAMIC_TYPE_RNG);
#endif
    }

    return ret;
}
#endif

/*******************************************************************************
 * START OF RSA API
 ******************************************************************************/

#ifndef NO_RSA

/*
 * RSA METHOD
 * Could be used to hold function pointers to implementations of RSA operations.
 */

#if defined(OPENSSL_EXTRA)
/* Return a blank RSA method and set the name and flags.
 *
 * Only one implementation of RSA operations.
 * name is duplicated.
 *
 * @param [in] name   Name to use in method.
 * @param [in] flags  Flags to set into method.
 * @return  Newly allocated RSA method on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA_METHOD *wolfSSL_RSA_meth_new(const char *name, int flags)
{
    WOLFSSL_RSA_METHOD* meth = NULL;
    int name_len = 0;
    int err;

    /* Validate name is not NULL. */
    err = (name == NULL);
    if (!err) {
        /* Allocate an RSA METHOD to return. */
        meth = (WOLFSSL_RSA_METHOD*)XMALLOC(sizeof(WOLFSSL_RSA_METHOD), NULL,
            DYNAMIC_TYPE_OPENSSL);
        err = (meth == NULL);
    }
    if (!err) {
        XMEMSET(meth, 0, sizeof(*meth));
        meth->flags = flags;
        meth->dynamic = 1;

        name_len = (int)XSTRLEN(name);
        meth->name = (char*)XMALLOC(name_len + 1, NULL, DYNAMIC_TYPE_OPENSSL);
        err = (meth->name == NULL);
    }
    if (!err) {
        XMEMCPY(meth->name, name, name_len+1);
    }

    if (err) {
        /* meth->name won't be allocated on error. */
        XFREE(meth, NULL, DYNAMIC_TYPE_OPENSSL);
    }
    return meth;
}

/* Default RSA method is one with wolfSSL name and no flags.
 *
 * @return  Newly allocated wolfSSL RSA method on success.
 * @return  NULL on failure.
 */
const WOLFSSL_RSA_METHOD* wolfSSL_RSA_get_default_method(void)
{
    static const WOLFSSL_RSA_METHOD wolfssl_rsa_meth = {
        0, /* No flags. */
        (char*)"wolfSSL RSA",
        0  /* Static definition. */
    };
    return &wolfssl_rsa_meth;
}

/* Dispose of RSA method and allocated data.
 *
 * @param [in] meth  RSA method to free.
 */
void wolfSSL_RSA_meth_free(WOLFSSL_RSA_METHOD *meth)
{
    /* Free method if available and dynamically allocated. */
    if ((meth != NULL) && meth->dynamic) {
        /* Name was duplicated and must be freed. */
        XFREE(meth->name, NULL, DYNAMIC_TYPE_OPENSSL);
        /* Dispose of RSA method. */
        XFREE(meth, NULL, DYNAMIC_TYPE_OPENSSL);
    }
}

#ifndef NO_WOLFSSL_STUB
/* Stub function for any RSA method setting function.
 *
 * Nothing is stored - not even flags or name.
 *
 * @param [in] meth  RSA method.
 * @param [in] p     A pointer.
 * @return  1 to indicate success.
 */
int wolfSSL_RSA_meth_set(WOLFSSL_RSA_METHOD *meth, void* p)
{
    WOLFSSL_STUB("RSA_METHOD is not implemented.");

    (void)meth;
    (void)p;

    return 1;
}
#endif /* !NO_WOLFSSL_STUB */
#endif /* OPENSSL_EXTRA */

/*
 * RSA constructor/deconstructor APIs
 */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
/* Dispose of RSA key and allocated data.
 *
 * Cannot use rsa after this call.
 *
 * @param [in] rsa  RSA key to free.
 */
void wolfSSL_RSA_free(WOLFSSL_RSA* rsa)
{
    int doFree = 1;

    WOLFSSL_ENTER("wolfSSL_RSA_free");

    /* Validate parameter. */
    if (rsa == NULL) {
        doFree = 0;
    }
    if (doFree) {
        int isZero;
        int err;

        /* Decrement reference count. */
        wolfSSL_RefDec(&rsa->ref, &isZero, &err);
        if (err == 0) {
            /* Continue if reference count is zero. */
            doFree = isZero;
        }
        else {
            /* Didn't reference decrement so can't free. */
            doFree = 0;
        }
    }
    if (doFree) {
        void* heap = rsa->heap;

        /* Dispose of allocated reference counting data. */
        wolfSSL_RefFree(&rsa->ref);

    #ifdef HAVE_EX_DATA_CLEANUP_HOOKS
        wolfSSL_CRYPTO_cleanup_ex_data(&rsa->ex_data);
    #endif

        if (rsa->internal != NULL) {
        #if !defined(HAVE_FIPS) && !defined(HAVE_USER_RSA) && \
            !defined(HAVE_FAST_RSA) && defined(WC_RSA_BLINDING)
            /* Check if RNG is owned before freeing it. */
            if (rsa->ownRng) {
                WC_RNG* rng = ((RsaKey*)(rsa->internal))->rng;
                if ((rng != NULL) && (rng != wolfssl_get_global_rng())) {
                    wc_FreeRng(rng);
                    XFREE(rng, heap, DYNAMIC_TYPE_RNG);
                }
                /* RNG isn't freed by wolfCrypt RSA free. */
            }
        #endif
            /* Dispose of allocated data in wolfCrypt RSA key. */
            wc_FreeRsaKey((RsaKey*)rsa->internal);
            /* Dispose of memory for wolfCrypt RSA key. */
            XFREE(rsa->internal, heap, DYNAMIC_TYPE_RSA);
        }

        /* Dispose of external representation of RSA values. */
        wolfSSL_BN_clear_free(rsa->iqmp);
        wolfSSL_BN_clear_free(rsa->dmq1);
        wolfSSL_BN_clear_free(rsa->dmp1);
        wolfSSL_BN_clear_free(rsa->q);
        wolfSSL_BN_clear_free(rsa->p);
        wolfSSL_BN_clear_free(rsa->d);
        wolfSSL_BN_free(rsa->e);
        wolfSSL_BN_free(rsa->n);

    #if defined(OPENSSL_EXTRA)
        if (rsa->meth) {
            wolfSSL_RSA_meth_free((WOLFSSL_RSA_METHOD*)rsa->meth);
        }
    #endif

        /* Set back to NULLs for safety. */
        ForceZero(rsa, sizeof(*rsa));

        XFREE(rsa, heap, DYNAMIC_TYPE_RSA);
        (void)heap;
    }
}

/* Allocate and initialize a new RSA key.
 *
 * wolfSSL API.
 *
 * @param [in] heap   Heap hint.
 * @param [in] devId  Device identifier value.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_RSA_new_ex(void* heap, int devId)
{
    WOLFSSL_RSA* rsa = NULL;
    RsaKey* key = NULL;
    int err = 0;
    int rsaKeyInited = 0;

    WOLFSSL_ENTER("wolfSSL_RSA_new");

    /* Allocate memory for new wolfCrypt RSA key. */
    key = (RsaKey*)XMALLOC(sizeof(RsaKey), heap, DYNAMIC_TYPE_RSA);
    if (key == NULL) {
        WOLFSSL_ERROR_MSG("wolfSSL_RSA_new malloc RsaKey failure");
        err = 1;
    }
    if (!err) {
        /* Allocate memory for new RSA key. */
        rsa = (WOLFSSL_RSA*)XMALLOC(sizeof(WOLFSSL_RSA), heap,
            DYNAMIC_TYPE_RSA);
        if (rsa == NULL) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_new malloc WOLFSSL_RSA failure");
            err = 1;
        }
    }
    if (!err) {
        /* Clear all fields of RSA key. */
        XMEMSET(rsa, 0, sizeof(WOLFSSL_RSA));
        /* Cache heap to use for all allocations. */
        rsa->heap = heap;
    #ifdef OPENSSL_EXTRA
        /* Always have a method set. */
        rsa->meth = wolfSSL_RSA_get_default_method();
    #endif

        /* Initialize reference counting. */
        wolfSSL_RefInit(&rsa->ref, &err);
    }
    if (!err) {
        /* Initialize wolfCrypt RSA key. */
        if (wc_InitRsaKey_ex(key, heap, devId) != 0) {
            WOLFSSL_ERROR_MSG("InitRsaKey WOLFSSL_RSA failure");
            err = 1;
        }
        else {
            rsaKeyInited = 1;
        }
    }
    #if !defined(HAVE_FIPS) && !defined(HAVE_USER_RSA) && \
        !defined(HAVE_FAST_RSA) && defined(WC_RSA_BLINDING)
    if (!err) {
        WC_RNG* rng;

        /* Create a local RNG. */
        rng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), heap, DYNAMIC_TYPE_RNG);
        if ((rng != NULL) && (wc_InitRng_ex(rng, heap, devId) != 0)) {
            WOLFSSL_MSG("InitRng failure, attempting to use global RNG");
            XFREE(rng, heap, DYNAMIC_TYPE_RNG);
            rng = NULL;
        }

        rsa->ownRng = 1;
        if (rng == NULL) {
            /* Get the wolfSSL global RNG - not thread safe. */
            rng = wolfssl_get_global_rng();
            rsa->ownRng = 0;
        }
        if (rng == NULL) {
            /* Couldn't create global either. */
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_new no WC_RNG for blinding");
            err = 1;
        }
        else {
            /* Set the local or global RNG into the wolfCrypt RSA key. */
            (void)wc_RsaSetRNG(key, rng);
            /* Won't fail as key and rng are not NULL. */
        }
    }
    #endif /* !HAVE_FIPS && !HAVE_USER_RSA && !HAVE_FAST_RSA &&
            * WC_RSA_BLINDING */
    if (!err) {
        /* Set wolfCrypt RSA key into RSA key. */
        rsa->internal = key;
        /* Data from external RSA key has not been set into internal one. */
        rsa->inSet = 0;
    }

    if (err) {
        /* Dispose of any allocated data on error. */
        /* No failure after RNG allocation - no need to free RNG. */
        if (rsaKeyInited) {
            wc_FreeRsaKey(key);
        }
        XFREE(key, heap, DYNAMIC_TYPE_RSA);
        XFREE(rsa, heap, DYNAMIC_TYPE_RSA);
        /* Return NULL. */
        rsa = NULL;
    }
    return rsa;
}

/* Allocate and initialize a new RSA key.
 *
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_RSA_new(void)
{
    /* Call wolfSSL API to do work. */
    return wolfSSL_RSA_new_ex(NULL, INVALID_DEVID);
}

/* Increments ref count of RSA key.
 *
 * @param [in, out] rsa  RSA key.
 * @return  1 on success
 * @return  0 on error
 */
int wolfSSL_RSA_up_ref(WOLFSSL_RSA* rsa)
{
    int err = 0;
    if (rsa != NULL) {
        wolfSSL_RefInc(&rsa->ref, &err);
    }
    return !err;
}

#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA)

/* Allocate a new RSA key and make it a copy.
 *
 * Encodes to and from DER to copy.
 *
 * @param [in] rsa  RSA key to duplicate.
 * @return  RSA key on success.
 * @return  NULL on error.
 */
WOLFSSL_RSA* wolfSSL_RSAPublicKey_dup(WOLFSSL_RSA *rsa)
{
    WOLFSSL_RSA* ret = NULL;
    int derSz = 0;
    byte* derBuf = NULL;
    int err;

    WOLFSSL_ENTER("wolfSSL_RSAPublicKey_dup");

    err = (rsa == NULL);
    if (!err) {
        /* Create a new RSA key to return. */
        ret = wolfSSL_RSA_new();
        if (ret == NULL) {
            WOLFSSL_ERROR_MSG("Error creating a new WOLFSSL_RSA structure");
            err = 1;
        }
    }
    if (!err) {
        /* Encode RSA public key to copy to DER - allocates DER buffer. */
        if ((derSz = wolfSSL_RSA_To_Der(rsa, &derBuf, 1, rsa->heap)) < 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
            err = 1;
        }
    }
    if (!err) {
        /* Decode DER of the RSA public key into new key. */
        if (wolfSSL_RSA_LoadDer_ex(ret, derBuf, derSz,
                WOLFSSL_RSA_LOAD_PUBLIC) != 1) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_LoadDer_ex failed");
            err = 1;
        }
    }

    /* Dispose of any allocated DER buffer. */
    XFREE(derBuf, rsa ? rsa->heap : NULL, DYNAMIC_TYPE_ASN1);
    if (err) {
        /* Disposes of any created RSA key - on error. */
        wolfSSL_RSA_free(ret);
        ret = NULL;
    }
    return ret;
}

/* wolfSSL_RSAPrivateKey_dup not supported */

#endif /* WOLFSSL_KEY_GEN && !HAVE_USER_RSA */

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA)
static int wolfSSL_RSA_To_Der_ex(WOLFSSL_RSA* rsa, byte** outBuf, int publicKey,
    void* heap);
#endif

/*
 * RSA to/from bin APIs
 */

/* Convert RSA public key data to internal.
 *
 * Creates new RSA key from the DER encoded RSA public key.
 *
 * @param [out]     out      Pointer to RSA key to return through. May be NULL.
 * @param [in, out] derBuf   Pointer to start of DER encoded data.
 * @param [in]      derSz    Length of the data in the DER buffer.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA *wolfSSL_d2i_RSAPublicKey(WOLFSSL_RSA **out,
    const unsigned char **derBuf, long derSz)
{
    WOLFSSL_RSA *rsa = NULL;
    int err = 0;

    WOLFSSL_ENTER("wolfSSL_d2i_RSAPublicKey");

    /* Validate parameters. */
    if (derBuf == NULL) {
        WOLFSSL_ERROR_MSG("Bad argument");
        err = 1;
    }
    /* Create a new RSA key to return. */
    if ((!err) && ((rsa = wolfSSL_RSA_new()) == NULL)) {
        WOLFSSL_ERROR_MSG("RSA_new failed");
        err = 1;
    }
    /* Decode RSA key from DER. */
    if ((!err) && (wolfSSL_RSA_LoadDer_ex(rsa, *derBuf, (int)derSz,
            WOLFSSL_RSA_LOAD_PUBLIC) != 1)) {
        WOLFSSL_ERROR_MSG("RSA_LoadDer failed");
        err = 1;
    }
    if ((!err) && (out != NULL)) {
        /* Return through parameter too. */
        *out = rsa;
        /* Move buffer on by the used amount. */
        *derBuf += wolfssl_der_length(*derBuf, (int)derSz);
    }

    if (err) {
        /* Dispose of any created RSA key. */
        wolfSSL_RSA_free(rsa);
        rsa = NULL;
    }
    return rsa;
}

/* Convert RSA private key data to internal.
 *
 * Create a new RSA key from the DER encoded RSA private key.
 *
 * @param [out]     out      Pointer to RSA key to return through. May be NULL.
 * @param [in, out] derBuf   Pointer to start of DER encoded data.
 * @param [in]      derSz    Length of the data in the DER buffer.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA *wolfSSL_d2i_RSAPrivateKey(WOLFSSL_RSA **out,
    const unsigned char **derBuf, long derSz)
{
    WOLFSSL_RSA *rsa = NULL;
    int err = 0;

    WOLFSSL_ENTER("wolfSSL_d2i_RSAPublicKey");

    /* Validate parameters. */
    if (derBuf == NULL) {
        WOLFSSL_ERROR_MSG("Bad argument");
        err = 1;
    }
    /* Create a new RSA key to return. */
    if ((!err) && ((rsa = wolfSSL_RSA_new()) == NULL)) {
        WOLFSSL_ERROR_MSG("RSA_new failed");
        err = 1;
    }
    /* Decode RSA key from DER. */
    if ((!err) && (wolfSSL_RSA_LoadDer_ex(rsa, *derBuf, (int)derSz,
            WOLFSSL_RSA_LOAD_PRIVATE) != 1)) {
        WOLFSSL_ERROR_MSG("RSA_LoadDer failed");
        err = 1;
    }
    if ((!err) && (out != NULL)) {
        /* Return through parameter too. */
        *out = rsa;
        /* Move buffer on by the used amount. */
        *derBuf += wolfssl_der_length(*derBuf, (int)derSz);
    }

    if (err) {
        /* Dispose of any created RSA key. */
        wolfSSL_RSA_free(rsa);
        rsa = NULL;
    }
    return rsa;
}

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA) && \
    !defined(HAVE_FAST_RSA)
/* Converts an internal RSA structure to DER format for the private key.
 *
 * If "pp" is null then buffer size only is returned.
 * If "*pp" is null then a created buffer is set in *pp and the caller is
 *  responsible for free'ing it.
 *
 * @param [in]      rsa  RSA key.
 * @param [in, out] pp   On in, pointer to allocated buffer or NULL.
 *                       May be NULL.
 *                       On out, newly allocated buffer or pointer to byte after
 *                       encoding in passed in buffer.
 *
 * @return  Size of DER encoding on success
 * @return  BAD_FUNC_ARG when rsa is NULL.
 * @return  0 on failure.
 */
int wolfSSL_i2d_RSAPrivateKey(WOLFSSL_RSA *rsa, unsigned char **pp)
{
    int ret;

    WOLFSSL_ENTER("wolfSSL_i2d_RSAPrivateKey");

    /* Validate parameters. */
    if (rsa == NULL) {
        WOLFSSL_ERROR_MSG("Bad Function Arguments");
        ret = BAD_FUNC_ARG;
    }
    /* Encode the RSA key as a DER. Call allocates buffer into pp.
     * No heap hint as this gets returned to the user */
    else if ((ret = wolfSSL_RSA_To_Der_ex(rsa, pp, 0, NULL)) < 0) {
        WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
        ret = 0;
    }

    /* Size of DER encoding. */
    return ret;
}

/* Converts an internal RSA structure to DER format for the public key.
 *
 * If "pp" is null then buffer size only is returned.
 * If "*pp" is null then a created buffer is set in *pp and the caller is
 *  responsible for free'ing it.
 *
 * @param [in]      rsa  RSA key.
 * @param [in, out] pp   On in, pointer to allocated buffer or NULL.
 *                       May be NULL.
 *                       On out, newly allocated buffer or pointer to byte after
 *                       encoding in passed in buffer.
 * @return  Size of DER encoding on success
 * @return  BAD_FUNC_ARG when rsa is NULL.
 * @return  0 on failure.
 */
int wolfSSL_i2d_RSAPublicKey(WOLFSSL_RSA *rsa, unsigned char **pp)
{
    int ret;

    WOLFSSL_ENTER("wolfSSL_i2d_RSAPublicKey");

    /* check for bad functions arguments */
    if (rsa == NULL) {
        WOLFSSL_ERROR_MSG("Bad Function Arguments");
        ret = BAD_FUNC_ARG;
    }
    /* Encode the RSA key as a DER. Call allocates buffer into pp.
     * No heap hint as this gets returned to the user */
    else if ((ret = wolfSSL_RSA_To_Der_ex(rsa, pp, 1, NULL)) < 0) {
        WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
        ret = 0;
    }

    return ret;
}
#endif /* defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA) &&
        * !defined(HAVE_FAST_RSA) */

#endif /* OPENSSL_EXTRA */

/*
 * RSA to/from BIO APIs
 */

/* wolfSSL_d2i_RSAPublicKey_bio not supported */

#if defined(OPENSSL_ALL) || defined(WOLFSSL_ASIO) || defined(WOLFSSL_HAPROXY) \
    || defined(WOLFSSL_NGINX) || defined(WOLFSSL_QT)

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA) && \
    !defined(HAVE_FAST_RSA) && !defined(NO_BIO)

/* Read DER data from a BIO.
 *
 * DER structures start with a constructed sequence. Use this to calculate the
 * total length of the DER data.
 *
 * @param [in]  bio   BIO object to read from.
 * @param [out] out   Buffer holding DER encoding.
 * @return  Number of bytes to DER encoding on success.
 * @return  0 on failure.
 */
static int wolfssl_read_der_bio(WOLFSSL_BIO* bio, unsigned char** out)
{
    int err = 0;
    unsigned char seq[MAX_SEQ_SZ];
    unsigned char* der = NULL;
    int derLen = 0;

    /* Read in a minimal amount to get a SEQUENCE header of any size. */
    if (wolfSSL_BIO_read(bio, seq, sizeof(seq)) != sizeof(seq)) {
        WOLFSSL_ERROR_MSG("wolfSSL_BIO_read() of sequence failure");
        err = 1;
    }
    /* Calculate complete DER encoding length. */
    if ((!err) && ((derLen = wolfssl_der_length(seq, sizeof(seq))) <= 0)) {
        WOLFSSL_ERROR_MSG("DER SEQUENCE decode failed");
        err = 1;
    }
    /* Allocate a buffer to read DER data into. */
    if ((!err) && ((der = (unsigned char*)XMALLOC(derLen, bio->heap,
            DYNAMIC_TYPE_TMP_BUFFER)) == NULL)) {
        WOLFSSL_ERROR_MSG("Malloc failure");
        err = 1;
    }
    if (!err) {
        /* Calculate the unread amount. */
        int len = derLen - sizeof(seq);
        /* Copy the previously read data into the buffer. */
        XMEMCPY(der, seq, sizeof(seq));
        /* Read rest of DER data from BIO. */
        if (wolfSSL_BIO_read(bio, der + sizeof(seq), len) != len) {
            WOLFSSL_ERROR_MSG("wolfSSL_BIO_read() failure");
            err = 1;
        }
    }
    if (!err) {
        /* Return buffer through parameter. */
        *out = der;
    }

    if (err) {
        /* Dispose of any allocated buffer on error. */
        XFREE(der, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
        derLen = 0;
    }
    return derLen;
}

/* Reads the RSA private key data from a BIO to the internal form.
 *
 * Creates new RSA key from the DER encoded RSA private key read from the BIO.
 *
 * @param [in]  bio  BIO object to read from.
 * @param [out] out  Pointer to RSA key to return through. May be NULL.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_d2i_RSAPrivateKey_bio(WOLFSSL_BIO *bio, WOLFSSL_RSA **out)
{
    WOLFSSL_RSA* key = NULL;
    unsigned char* der = NULL;
    int derLen = 0;
    int err;

    WOLFSSL_ENTER("wolfSSL_d2i_RSAPrivateKey_bio()");

    /* Validate parameters. */
    err = (bio == NULL);
    /* Read just DER encoding from BIO - buffer allocated in call. */
    if ((!err) && ((derLen = wolfssl_read_der_bio(bio, &der)) == 0)) {
        err = 1;
    }
    if (!err) {
        /* Keep der for call to deallocate. */
        const unsigned char* cder = der;
        /* Create an RSA key from the data from the BIO. */
        key = wolfSSL_d2i_RSAPrivateKey(NULL, &cder, derLen);
        err = (key == NULL);
    }
    if ((!err) && (out != NULL)) {
        /* Return the created RSA key through the parameter. */
        *out = key;
    }

    if (err) {
        /* Dispose of created key on error. */
        wolfSSL_RSA_free(key);
        key = NULL;
    }
    /* Dispose of allocated data. */
    XFREE(der, bio ? bio->heap : NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return key;
}
#endif /* defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA) &&
        * !defined(HAVE_FAST_RSA) && !NO_BIO */

#endif /* OPENSSL_ALL || WOLFSSL_ASIO || WOLFSSL_HAPROXY || WOLFSSL_QT */

/*
 * RSA DER APIs
 */

#ifdef OPENSSL_EXTRA

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA)
/* Create a DER encoding of key.
 *
 * wolfSSL API.
 *
 * @param [in]  rsa        RSA key.
 * @param [out] outBuf     Allocated buffer containing DER encoding.
 *                         May be NULL.
 * @param [in]  publicKey  Whether to encode as public key.
 * @param [in]  heap       Heap hint.
 * @return  Encoding size on success.
 * @return  Negative on failure.
 */
int wolfSSL_RSA_To_Der(WOLFSSL_RSA* rsa, byte** outBuf, int publicKey,
    void* heap)
{
    byte* p = NULL;
    int ret;

    if (outBuf != NULL) {
        p = *outBuf;
    }
    ret = wolfSSL_RSA_To_Der_ex(rsa, outBuf, publicKey, heap);
    if ((ret > 0) && (p != NULL)) {
        *outBuf = p;
    }
    return ret;
}

/* Create a DER encoding of key.
 *
 * Buffer allocated with heap and DYNAMIC_TYPE_TMP_BUFFER.
 *
 * @param [in]      rsa        RSA key.
 * @param [in, out] outBuf     On in, pointer to allocated buffer or NULL.
 *                             May be NULL.
 *                             On out, newly allocated buffer or pointer to byte
 *                             after encoding in passed in buffer.
 * @param [in]      publicKey  Whether to encode as public key.
 * @return  Encoding size on success.
 * @return  Negative on failure.
 */
static int wolfSSL_RSA_To_Der_ex(WOLFSSL_RSA* rsa, byte** outBuf, int publicKey,
    void* heap)
{
    int ret = 1;
    int derSz = 0;
    byte* derBuf = NULL;

    WOLFSSL_ENTER("wolfSSL_RSA_To_Der");

    /* Unused if memory is disabled. */
    (void)heap;

    /* Validate parameters. */
    if ((rsa == NULL) || ((publicKey != 0) && (publicKey != 1))) {
        WOLFSSL_LEAVE("wolfSSL_RSA_To_Der", BAD_FUNC_ARG);
        ret = BAD_FUNC_ARG;
    }
    /* Push external RSA data into internal RSA key if not set. */
    if ((ret == 1) && (!rsa->inSet)) {
        ret = SetRsaInternal(rsa);
    }
    /* wc_RsaKeyToPublicDer encode regardless of values. */
    if ((ret == 1) && publicKey && (mp_iszero(&((RsaKey*)rsa->internal)->n) ||
            mp_iszero(&((RsaKey*)rsa->internal)->e))) {
        ret = BAD_FUNC_ARG;
    }

    if (ret == 1) {
        if (publicKey) {
            /* Calculate length of DER encoded RSA public key. */
            derSz = wc_RsaPublicKeyDerSize((RsaKey*)rsa->internal, 1);
            if (derSz < 0) {
                WOLFSSL_ERROR_MSG("wc_RsaPublicKeyDerSize failed");
                ret = derSz;
            }
        }
        else {
            /* Calculate length of DER encoded RSA private key. */
            derSz = wc_RsaKeyToDer((RsaKey*)rsa->internal, NULL, 0);
            if (derSz < 0) {
                WOLFSSL_ERROR_MSG("wc_RsaKeyToDer failed");
                ret = derSz;
            }
        }
    }

    if ((ret == 1) && (outBuf != NULL)) {
        derBuf = *outBuf;
        if (derBuf == NULL) {
            /* Allocate buffer to hold DER encoded RSA key. */
            derBuf = (byte*)XMALLOC(derSz, heap, DYNAMIC_TYPE_TMP_BUFFER);
            if (derBuf == NULL) {
                WOLFSSL_ERROR_MSG("Memory allocation failed");
                ret = MEMORY_ERROR;
            }
        }
    }
    if ((ret == 1) && (outBuf != NULL)) {
        if (publicKey) {
            /* RSA public key to DER. */
            derSz = wc_RsaKeyToPublicDer((RsaKey*)rsa->internal, derBuf, derSz);
        }
        else {
            /* RSA private key to DER. */
            derSz = wc_RsaKeyToDer((RsaKey*)rsa->internal, derBuf, derSz);
        }
        if (derSz < 0) {
            WOLFSSL_ERROR_MSG("RSA key encoding failed");
            ret = derSz;
        }
        else if ((*outBuf) != NULL) {
            derBuf = NULL;
            *outBuf += derSz;
        }
        else {
            /* Return allocated buffer. */
            *outBuf = derBuf;
        }
    }
    if (ret == 1) {
        /* Success - return DER encoding size. */
        ret = derSz;
    }

    if ((outBuf != NULL) && (*outBuf != derBuf)) {
        /* Not returning buffer, needs to be disposed of. */
        XFREE(derBuf, heap, DYNAMIC_TYPE_TMP_BUFFER);
    }
    WOLFSSL_LEAVE("wolfSSL_RSA_To_Der", ret);
    return ret;
}
#endif /* WOLFSSL_KEY_GEN && !HAVE_USER_RSA */

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
/* Load the DER encoded private RSA key.
 *
 * wolfSSL API.
 *
 * @param [in] rsa     RSA key.
 * @param [in] derBuf  Buffer holding DER encoding.
 * @param [in] derSz   Length of DER encoding.
 * @return  1 on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_LoadDer(WOLFSSL_RSA* rsa, const unsigned char* derBuf,
    int derSz)
{
    /* Call implementation that handles both private and public keys. */
    return wolfSSL_RSA_LoadDer_ex(rsa, derBuf, derSz, WOLFSSL_RSA_LOAD_PRIVATE);
}

/* Load the DER encoded public or private RSA key.
 *
 * wolfSSL API.
 *
 * @param [in] rsa     RSA key.
 * @param [in] derBuf  Buffer holding DER encoding.
 * @param [in] derSz   Length of DER encoding.
 * @param [in] opt     Indicates public or private key.
 *                     (WOLFSSL_RSA_LOAD_PUBLIC or WOLFSSL_RSA_LOAD_PRIVATE)
 * @return  1 on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_LoadDer_ex(WOLFSSL_RSA* rsa, const unsigned char* derBuf,
    int derSz, int opt)
{
    int ret = 1;
    int res;
    word32 idx = 0;
    word32 algId;

    WOLFSSL_ENTER("wolfSSL_RSA_LoadDer");

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->internal == NULL) || (derBuf == NULL) ||
            (derSz <= 0)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if (ret == 1) {
        rsa->pkcs8HeaderSz = 0;
        /* Check if input buffer has PKCS8 header. In the case that it does not
         * have a PKCS8 header then do not error out. */
        res = ToTraditionalInline_ex((const byte*)derBuf, &idx, (word32)derSz,
            &algId);
        if (res > 0) {
            /* Store size of PKCS#8 header for encoding. */
            WOLFSSL_MSG("Found PKCS8 header");
            rsa->pkcs8HeaderSz = (word16)idx;
        }
        /* When decoding and not PKCS#8, return will be ASN_PARSE_E. */
        else if (res != ASN_PARSE_E) {
            /* Something went wrong while decoding. */
            WOLFSSL_ERROR_MSG("Unexpected error with trying to remove PKCS#8 "
                              "header");
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Decode private or public key data. */
        if (opt == WOLFSSL_RSA_LOAD_PRIVATE) {
            res = wc_RsaPrivateKeyDecode(derBuf, &idx, (RsaKey*)rsa->internal,
                derSz);
        }
        else {
            res = wc_RsaPublicKeyDecode(derBuf, &idx, (RsaKey*)rsa->internal,
                derSz);
        }
        /* Check for error. */
        if (res < 0) {
            if (opt == WOLFSSL_RSA_LOAD_PRIVATE) {
                 WOLFSSL_ERROR_MSG("RsaPrivateKeyDecode failed");
            }
            else {
                 WOLFSSL_ERROR_MSG("RsaPublicKeyDecode failed");
            }
            WOLFSSL_ERROR_VERBOSE(res);
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Set external RSA key data from wolfCrypt key. */
        if (SetRsaExternal(rsa) != 1) {
            ret = -1;
        }
        else {
            rsa->inSet = 1;
        }
    }

    return ret;
}

#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA

#if !defined(NO_BIO) || !defined(NO_FILESYSTEM)
/* Load DER encoded data into WOLFSSL_RSA object.
 *
 * Creates a new WOLFSSL_RSA object if one is not passed in.
 *
 * @param [in, out] rsa   WOLFSSL_RSA object to load into.
 *                        When rsa or *rsa is NULL a new object is created.
 *                        When not NULL and *rsa is NULL then new object
 *                        returned through pointer.
 * @param [in]      in    DER encoded RSA key data.
 * @param [in]      inSz  Size of DER encoded data in bytes.
 * @param [in]      opt   Public or private key encoded in data. Valid values:
 *                        WOLFSSL_RSA_LOAD_PRIVATE, WOLFSSL_RSA_LOAD_PUBLIC.
 * @return  NULL on failure.
 * @return  WOLFSSL_RSA object on success.
 */
static WOLFSSL_RSA* wolfssl_rsa_d2i(WOLFSSL_RSA** rsa, const unsigned char* in,
    long inSz, int opt)
{
    WOLFSSL_RSA* ret = NULL;

    if ((rsa != NULL) && (*rsa != NULL)) {
        ret = *rsa;
    }
    else {
        ret = wolfSSL_RSA_new();
    }
    if ((ret != NULL) && (wolfSSL_RSA_LoadDer_ex(ret, in, (int)inSz, opt)
            != 1)) {
        if ((rsa == NULL) || (ret != *rsa)) {
            wolfSSL_RSA_free(ret);
        }
        ret = NULL;
    }

    if ((rsa != NULL) && (*rsa == NULL)) {
        *rsa = ret;
    }
    return ret;
}
#endif

#endif /* OPENSSL_EXTRA */

/*
 * RSA PEM APIs
 */

#ifdef OPENSSL_EXTRA

#ifndef NO_BIO
#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA)
/* Writes PEM encoding of an RSA public key to a BIO.
 *
 * @param [in] bio  BIO object to write to.
 * @param [in] rsa  RSA key to write.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_bio_RSA_PUBKEY(WOLFSSL_BIO* bio, WOLFSSL_RSA* rsa)
{
    int ret = 1;
    int derSz = 0;
    byte* derBuf = NULL;

    WOLFSSL_ENTER("wolfSSL_PEM_write_bio_RSA_PUBKEY");

    /* Validate parameters. */
    if ((bio == NULL) || (rsa == NULL)) {
        WOLFSSL_ERROR_MSG("Bad Function Arguments");
        ret = 0;
    }

    if (ret == 1) {
        if ((derSz = wolfSSL_RSA_To_Der(rsa, &derBuf, 1, bio->heap)) < 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
            ret = 0;
        }
        if (derBuf == NULL) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed to get buffer");
            ret = 0;
        }
    }
    if ((ret == 1) && (der_write_to_bio_as_pem(derBuf, derSz, bio,
            PUBLICKEY_TYPE) != WOLFSSL_SUCCESS)) {
        ret = 0;
    }

    /* Dispose of DER buffer. */
    XFREE(derBuf, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}

#endif /* WOLFSSL_KEY_GEN && !HAVE_USER_RSA */
#endif /* !NO_BIO */

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA)
#ifndef NO_FILESYSTEM

/* Writes PEM encoding of an RSA public key to a file pointer.
 *
 * @param [in] fp    File pointer to write to.
 * @param [in] rsa   RSA key to write.
 * @param [in] type  PEM type to write out.
 * @return  1 on success.
 * @return  0 on failure.
 */
static int wolfssl_pem_write_rsa_public_key(XFILE fp, WOLFSSL_RSA* rsa,
    int type)
{
    int ret = 1;
    int derSz;
    byte* derBuf = NULL;

    /* Validate parameters. */
    if ((fp == XBADFILE) || (rsa == NULL)) {
        WOLFSSL_ERROR_MSG("Bad Function Arguments");
        ret = 0;
    }

    if (ret == 1) {
        if ((derSz = wolfSSL_RSA_To_Der(rsa, &derBuf, 1, rsa->heap)) < 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
            ret = 0;
        }
        if (derBuf == NULL) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed to get buffer");
            ret = 0;
        }
    }
    if ((ret == 1) && (der_write_to_file_as_pem(derBuf, derSz, fp, type,
            rsa->heap) != WOLFSSL_SUCCESS)) {
        ret = 0;
    }

    /* Dispose of DER buffer. */
    XFREE(derBuf, rsa->heap, DYNAMIC_TYPE_TMP_BUFFER);

    return ret;
}

/* Writes PEM encoding of an RSA public key to a file pointer.
 *
 * Header/footer will contain: PUBLIC KEY
 *
 * @param [in] fp   File pointer to write to.
 * @param [in] rsa  RSA key to write.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_RSA_PUBKEY(XFILE fp, WOLFSSL_RSA* rsa)
{
    return wolfssl_pem_write_rsa_public_key(fp, rsa, PUBLICKEY_TYPE);
}

/* Writes PEM encoding of an RSA public key to a file pointer.
 *
 * Header/footer will contain: RSA PUBLIC KEY
 *
 * @param [in] fp   File pointer to write to.
 * @param [in] rsa  RSA key to write.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_RSAPublicKey(XFILE fp, WOLFSSL_RSA* rsa)
{
    return wolfssl_pem_write_rsa_public_key(fp, rsa, RSA_PUBLICKEY_TYPE);
}
#endif /* !NO_FILESYSTEM */
#endif /* WOLFSSL_KEY_GEN && !HAVE_USER_RSA */

#ifndef NO_BIO
/* Create an RSA public key by reading the PEM encoded data from the BIO.
 *
 * @param [in]  bio   BIO object to read from.
 * @param [out] out   RSA key created.
 * @param [in]  cb    Password callback when PEM encrypted.
 * @param [in]  pass  NUL terminated string for passphrase when PEM encrypted.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA *wolfSSL_PEM_read_bio_RSA_PUBKEY(WOLFSSL_BIO* bio,
    WOLFSSL_RSA** out, wc_pem_password_cb* cb, void *pass)
{
    WOLFSSL_RSA* rsa = NULL;
    DerBuffer*   der = NULL;
    int          keyFormat = 0;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_RSA_PUBKEY");

    if ((bio != NULL) && (pem_read_bio_key(bio, cb, pass, PUBLICKEY_TYPE,
            &keyFormat, &der) >= 0)) {
        rsa = wolfssl_rsa_d2i(out, der->buffer, der->length,
            WOLFSSL_RSA_LOAD_PUBLIC);
        if (rsa == NULL) {
            WOLFSSL_ERROR_MSG("Error loading DER buffer into WOLFSSL_RSA");
        }
    }

    FreeDer(&der);
    if ((out != NULL) && (rsa != NULL)) {
        *out = rsa;
    }
    return rsa;
}
#endif /* !NO_BIO */

#ifndef NO_FILESYSTEM
/* Create an RSA public key by reading the PEM encoded data from the BIO.
 *
 * Header/footer should contain: PUBLIC KEY
 * PEM decoder supports either 'RSA PUBLIC KEY' or 'PUBLIC KEY'.
 *
 * @param [in]  fp    File pointer to read from.
 * @param [out] out   RSA key created.
 * @param [in]  cb    Password callback when PEM encrypted.
 * @param [in]  pass  NUL terminated string for passphrase when PEM encrypted.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA *wolfSSL_PEM_read_RSA_PUBKEY(XFILE fp,
    WOLFSSL_RSA** out, wc_pem_password_cb* cb, void *pass)
{
    WOLFSSL_RSA* rsa = NULL;
    DerBuffer*   der = NULL;
    int          keyFormat = 0;

    WOLFSSL_ENTER("wolfSSL_PEM_read_RSA_PUBKEY");

    if ((fp != XBADFILE) && (pem_read_file_key(fp, cb, pass, PUBLICKEY_TYPE,
            &keyFormat, &der) >= 0)) {
        rsa = wolfssl_rsa_d2i(out, der->buffer, der->length,
            WOLFSSL_RSA_LOAD_PUBLIC);
        if (rsa == NULL) {
            WOLFSSL_ERROR_MSG("Error loading DER buffer into WOLFSSL_RSA");
        }
    }

    FreeDer(&der);
    if ((out != NULL) && (rsa != NULL)) {
        *out = rsa;
    }
    return rsa;
}

/* Create an RSA public key by reading the PEM encoded data from the BIO.
 *
 * Header/footer should contain: RSA PUBLIC KEY
 * PEM decoder supports either 'RSA PUBLIC KEY' or 'PUBLIC KEY'.
 *
 * @param [in]  fp    File pointer to read from.
 * @param [out] rsa   RSA key created.
 * @param [in]  cb    Password callback when PEM encrypted. May be NULL.
 * @param [in]  pass  NUL terminated string for passphrase when PEM encrypted.
 *                    May be NULL.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_PEM_read_RSAPublicKey(XFILE fp, WOLFSSL_RSA** rsa,
    wc_pem_password_cb* cb, void* pass)
{
    return wolfSSL_PEM_read_RSA_PUBKEY(fp, rsa, cb, pass);
}

#endif /* NO_FILESYSTEM */

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_USER_RSA) && \
    (defined(WOLFSSL_PEM_TO_DER) || defined(WOLFSSL_DER_TO_PEM))

/* Writes PEM encoding of an RSA private key to newly allocated buffer.
 *
 * Buffer returned was allocated with: DYNAMIC_TYPE_KEY.
 *
 * @param [in]  rsa       RSA key to write.
 * @param [in]  cipher    Cipher to use when PEM encrypted. May be NULL.
 * @param [in]  passwd    Password string when PEM encrypted. May be NULL.
 * @param [in]  passwdSz  Length of password string when PEM encrypted.
 * @param [out] pem       Allocated buffer with PEM encoding.
 * @param [out] plen      Length of PEM encoding.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_mem_RSAPrivateKey(RSA* rsa, const EVP_CIPHER* cipher,
    unsigned char* passwd, int passwdSz, unsigned char **pem, int *plen)
{
    int ret = 1;
    byte* derBuf = NULL;
    byte* tmp = NULL;
    byte* cipherInfo = NULL;
    int  derSz = 0;
    int  pemSz = 0;
    const int type = PRIVATEKEY_TYPE;

    WOLFSSL_ENTER("wolfSSL_PEM_write_mem_RSAPrivateKey");

    /* Validate parameters. */
    if ((pem == NULL) || (plen == NULL) || (rsa == NULL) ||
            (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    /* Set the RSA key data into the wolfCrypt RSA key if not done so. */
    if ((ret == 1) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = 0;
    }

    /* Encode wolfCrypt RSA key to DER - derBuf allocated in call. */
    if ((ret == 1) && ((derSz = wolfSSL_RSA_To_Der(rsa, &derBuf, 0,
            rsa->heap)) < 0)) {
        WOLFSSL_ERROR_MSG("wolfSSL_RSA_To_Der failed");
        ret = 0;
    }

    /* Encrypt DER buffer if required. */
    if ((ret == 1) && (passwd != NULL) && (passwdSz > 0) && (cipher != NULL)) {
        int blockSz = wolfSSL_EVP_CIPHER_block_size(cipher);
        byte *tmpBuf;

        /* Add space for padding. */
        tmpBuf = (byte*)XREALLOC(derBuf, derSz + blockSz, rsa->heap,
            DYNAMIC_TYPE_TMP_BUFFER);
        if (tmpBuf == NULL) {
            WOLFSSL_ERROR_MSG("Extending DER buffer failed");
            XFREE(derBuf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            ret = 0;
        }
        else {
            derBuf = tmpBuf;

            /* Encrypt DER inline. */
            ret = EncryptDerKey(derBuf, &derSz, cipher, passwd, passwdSz,
                &cipherInfo, derSz + blockSz);
            if (ret != 1) {
                WOLFSSL_ERROR_MSG("EncryptDerKey failed");
            }
        }
    }

    if (ret == 1) {
        /* Calculate PEM encoding size. */
        pemSz = wc_DerToPemEx(derBuf, derSz, NULL, 0, cipherInfo, type);
        if (pemSz <= 0) {
            WOLFSSL_ERROR_MSG("wc_DerToPemEx failed");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Allocate space for PEM encoding plus a NUL terminator. */
        tmp = (byte*)XMALLOC(pemSz + 1, NULL, DYNAMIC_TYPE_KEY);
        if (tmp == NULL) {
            WOLFSSL_ERROR_MSG("malloc failed");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* DER to PEM */
        pemSz = wc_DerToPemEx(derBuf, derSz, tmp, pemSz, cipherInfo, type);
        if (pemSz <= 0) {
            WOLFSSL_ERROR_MSG("wc_DerToPemEx failed");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* NUL terminate string - PEM.  */
        tmp[pemSz] = 0x00;
        /* Return allocated buffer and size. */
        *pem = tmp;
        *plen = pemSz;
        /* Don't free returning buffer. */
        tmp = NULL;
    }

    XFREE(tmp, NULL, DYNAMIC_TYPE_KEY);
    XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);
    XFREE(derBuf, rsa ? rsa->heap : NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}

#ifndef NO_BIO
/* Writes PEM encoding of an RSA private key to a BIO.
 *
 * @param [in] bio     BIO object to write to.
 * @param [in] rsa     RSA key to write.
 * @param [in] cipher  Cipher to use when PEM encrypted.
 * @param [in] passwd  Password string when PEM encrypted.
 * @param [in] len     Length of password string when PEM encrypted.
 * @param [in] cb      Password callback to use when PEM encrypted.
 * @param [in] arg     NUL terminated string for passphrase when PEM encrypted.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_bio_RSAPrivateKey(WOLFSSL_BIO* bio, WOLFSSL_RSA* rsa,
    const WOLFSSL_EVP_CIPHER* cipher, unsigned char* passwd, int len,
    wc_pem_password_cb* cb, void* arg)
{
    int ret = 1;
    byte* pem = NULL;
    int plen;

    (void)cb;
    (void)arg;

    WOLFSSL_ENTER("wolfSSL_PEM_write_bio_RSAPrivateKey");

    /* Validate parameters. */
    if ((bio == NULL) || (rsa == NULL) || (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    if (ret == 1) {
        /* Write PEM to buffer that is allocated in the call. */
        ret = wolfSSL_PEM_write_mem_RSAPrivateKey(rsa, cipher, passwd, len,
            &pem, &plen);
        if (ret != 1) {
            WOLFSSL_ERROR_MSG("wolfSSL_PEM_write_mem_RSAPrivateKey failed");
        }
    }
    /* Write PEM to BIO. */
    if ((ret == 1) && (wolfSSL_BIO_write(bio, pem, plen) <= 0)) {
        WOLFSSL_ERROR_MSG("RSA private key BIO write failed");
        ret = 0;
    }

    /* Dispose of any allocated PEM buffer. */
    XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
    return ret;
}
#endif /* !NO_BIO */

#ifndef NO_FILESYSTEM
/* Writes PEM encoding of an RSA private key to a file pointer.
 *
 * TODO: Support use of the password callback and callback context.
 *
 * @param [in] fp        File pointer to write to.
 * @param [in] rsa       RSA key to write.
 * @param [in] cipher    Cipher to use when PEM encrypted. May be NULL.
 * @param [in] passwd    Password string when PEM encrypted. May be NULL.
 * @param [in] passwdSz  Length of password string when PEM encrypted.
 * @param [in] cb        Password callback to use when PEM encrypted. Unused.
 * @param [in] arg       NUL terminated string for passphrase when PEM
 *                       encrypted. Unused.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_RSAPrivateKey(XFILE fp, WOLFSSL_RSA *rsa,
    const EVP_CIPHER *cipher, unsigned char *passwd, int passwdSz,
    wc_pem_password_cb *cb, void *arg)
{
    int ret = 1;
    byte* pem = NULL;
    int plen;

    (void)cb;
    (void)arg;

    WOLFSSL_ENTER("wolfSSL_PEM_write_RSAPrivateKey");

    /* Validate parameters. */
    if ((fp == XBADFILE) || (rsa == NULL) || (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    if (ret == 1) {
        /* Write PEM to buffer that is allocated in the call. */
        ret = wolfSSL_PEM_write_mem_RSAPrivateKey(rsa, cipher, passwd, passwdSz,
            &pem, &plen);
        if (ret != 1) {
            WOLFSSL_ERROR_MSG("wolfSSL_PEM_write_mem_RSAPrivateKey failed");
        }
    }
    /* Write PEM to file pointer. */
    if ((ret == 1) && ((int)XFWRITE(pem, plen, 1, fp) != 1)) {
        WOLFSSL_ERROR_MSG("RSA private key file write failed");
        ret = 0;
    }

    /* Dispose of any allocated PEM buffer. */
    XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
    return ret;
}
#endif /* NO_FILESYSTEM */
#endif /* WOLFSSL_KEY_GEN && !HAVE_USER_RSA && WOLFSSL_PEM_TO_DER */

#ifndef NO_BIO
/* Create an RSA private key by reading the PEM encoded data from the BIO.
 *
 * @param [in]  bio   BIO object to read from.
 * @param [out] out   RSA key created.
 * @param [in]  cb    Password callback when PEM encrypted.
 * @param [in]  pass  NUL terminated string for passphrase when PEM encrypted.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_PEM_read_bio_RSAPrivateKey(WOLFSSL_BIO* bio,
    WOLFSSL_RSA** out, wc_pem_password_cb* cb, void* pass)
{
    WOLFSSL_RSA* rsa = NULL;
    DerBuffer*   der = NULL;
    int          keyFormat = 0;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_RSAPrivateKey");

    if ((bio != NULL) && (pem_read_bio_key(bio, cb, pass, PRIVATEKEY_TYPE,
            &keyFormat, &der) >= 0)) {
        rsa = wolfssl_rsa_d2i(out, der->buffer, der->length,
            WOLFSSL_RSA_LOAD_PRIVATE);
        if (rsa == NULL) {
            WOLFSSL_ERROR_MSG("Error loading DER buffer into WOLFSSL_RSA");
        }
    }

    FreeDer(&der);
    if ((out != NULL) && (rsa != NULL)) {
        *out = rsa;
    }
    return rsa;
}
#endif /* !NO_BIO */

/* Create an RSA private key by reading the PEM encoded data from the file
 * pointer.
 *
 * @param [in]  fp    File pointer to read from.
 * @param [out] out   RSA key created.
 * @param [in]  cb    Password callback when PEM encrypted.
 * @param [in]  pass  NUL terminated string for passphrase when PEM encrypted.
 * @return  RSA key on success.
 * @return  NULL on failure.
 */
#ifndef NO_FILESYSTEM
WOLFSSL_RSA* wolfSSL_PEM_read_RSAPrivateKey(XFILE fp, WOLFSSL_RSA** out,
    wc_pem_password_cb* cb, void* pass)
{
    WOLFSSL_RSA* rsa = NULL;
    DerBuffer*   der = NULL;
    int          keyFormat = 0;

    WOLFSSL_ENTER("wolfSSL_PEM_read_RSAPrivateKey");

    if ((fp != XBADFILE) && (pem_read_file_key(fp, cb, pass, PRIVATEKEY_TYPE,
            &keyFormat, &der) >= 0)) {
        rsa = wolfssl_rsa_d2i(out, der->buffer, der->length,
            WOLFSSL_RSA_LOAD_PRIVATE);
        if (rsa == NULL) {
            WOLFSSL_ERROR_MSG("Error loading DER buffer into WOLFSSL_RSA");
        }
    }

    FreeDer(&der);
    if ((out != NULL) && (rsa != NULL)) {
        *out = rsa;
    }
    return rsa;
}
#endif /* !NO_FILESYSTEM */

/*
 * RSA print APIs
 */

#if defined(XFPRINTF) && !defined(NO_FILESYSTEM) && \
    !defined(NO_STDIO_FILESYSTEM)
/* Print an RSA key to a file pointer.
 *
 * @param [in] fp      File pointer to write to.
 * @param [in] rsa     RSA key to write.
 * @param [in] indent  Number of spaces to prepend to each line.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_print_fp(XFILE fp, WOLFSSL_RSA* rsa, int indent)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_RSA_print_fp");

    /* Validate parameters. */
    if ((fp == XBADFILE) || (rsa == NULL)) {
        ret = 0;
    }

    /* Set the external data from the wolfCrypt RSA key if not done. */
    if ((ret == 1) && (!rsa->exSet)) {
        ret = SetRsaExternal(rsa);
    }

    /* Get the key size from modulus if available. */
    if ((ret == 1) && (rsa->n != NULL)) {
        int keySize = wolfSSL_BN_num_bits(rsa->n);
        if (keySize == 0) {
            ret = 0;
        }
        else {
            if (XFPRINTF(fp, "%*s", indent, "") < 0)
                ret = 0;
            else if (XFPRINTF(fp, "RSA Private-Key: (%d bit, 2 primes)\n",
                              keySize) < 0)
                ret = 0;
        }
    }
    /* Print out any components available. */
    if ((ret == 1) && (rsa->n != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "modulus", rsa->n);
    }
    if ((ret == 1) && (rsa->d != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "privateExponent", rsa->d);
    }
    if ((ret == 1) && (rsa->p != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "prime1", rsa->p);
    }
    if ((ret == 1) && (rsa->q != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "prime2", rsa->q);
    }
    if ((ret == 1) && (rsa->dmp1 != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "exponent1", rsa->dmp1);
    }
    if ((ret == 1) && (rsa->dmq1 != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "exponent2", rsa->dmq1);
    }
    if ((ret == 1) && (rsa->iqmp != NULL)) {
        ret = pk_bn_field_print_fp(fp, indent, "coefficient", rsa->iqmp);
    }

    WOLFSSL_LEAVE("wolfSSL_RSA_print_fp", ret);

    return ret;
}
#endif /* XFPRINTF && !NO_FILESYSTEM && !NO_STDIO_FILESYSTEM */

#if defined(XSNPRINTF) && !defined(NO_BIO) && !defined(HAVE_FAST_RSA)
/* snprintf() must be available */

/* Maximum size of a header line. */
#define RSA_PRINT_MAX_HEADER_LINE   PRINT_NUM_MAX_INDENT

/* Writes the human readable form of RSA to a BIO.
 *
 * @param [in] bio     BIO object to write to.
 * @param [in] rsa     RSA key to write.
 * @param [in] indent  Number of spaces before each line.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_print(WOLFSSL_BIO* bio, WOLFSSL_RSA* rsa, int indent)
{
    int ret = 1;
    int sz = 0;
    RsaKey* key = NULL;
    char line[RSA_PRINT_MAX_HEADER_LINE];
    int len;
    int i = 0;
    mp_int *num = NULL;
    /* Header strings. */
    const char *name[] = {
        "Modulus:", "Exponent:", "PrivateExponent:", "Prime1:", "Prime2:",
        "Exponent1:", "Exponent2:", "Coefficient:"
    };

    WOLFSSL_ENTER("wolfSSL_RSA_print");

    /* Validate parameters. */
    if ((bio == NULL) || (rsa == NULL) || (indent > PRINT_NUM_MAX_INDENT)) {
        ret = -1;
    }

    if (ret == 1) {
        key = (RsaKey*)rsa->internal;

        /* Get size in bits of key for printing out. */
        sz = wolfSSL_RSA_bits(rsa);
        if (sz <= 0) {
            WOLFSSL_ERROR_MSG("Error getting RSA key size");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Print any indent spaces. */
        ret = wolfssl_print_indent(bio, line, sizeof(line), indent);
    }
    if (ret == 1) {
        /* Print header line. */
        len = XSNPRINTF(line, sizeof(line), "\nRSA %s: (%d bit)\n",
            (!mp_iszero(&key->d)) ? "Private-Key" : "Public-Key", sz);
        if (len >= (int)sizeof(line)) {
            WOLFSSL_ERROR_MSG("Buffer overflow while formatting key preamble");
            ret = 0;
        }
        else {
            if (wolfSSL_BIO_write(bio, line, len) <= 0) {
                ret = 0;
            }
        }
    }

    for (i = 0; (ret == 1) && (i < RSA_INTS); i++) {
        /* Get mp_int for index. */
        switch(i) {
            case 0:
                /* Print out modulus */
                num = &key->n;
                break;
            case 1:
                num = &key->e;
                break;
            case 2:
                num = &key->d;
                break;
            case 3:
                num = &key->p;
                break;
            case 4:
                num = &key->q;
                break;
            case 5:
                num = &key->dP;
                break;
            case 6:
                num = &key->dQ;
                break;
            case 7:
                num = &key->u;
                break;
            default:
                WOLFSSL_ERROR_MSG("Bad index value");
        }

        if (i == 1) {
            /* Print exponent as a 32-bit value. */
            ret = wolfssl_print_value(bio, num, name[i], indent);
        }
        else if (!mp_iszero(num)) {
            /* Print name and MP integer. */
            ret = wolfssl_print_number(bio, num, name[i], indent);
        }
    }

    return ret;
}
#endif /* XSNPRINTF && !NO_BIO && !HAVE_FAST_RSA */

#endif /* OPENSSL_EXTRA */

/*
 * RSA get/set/test APIs
 */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
#if !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA)
/* Set RSA key data (external) from wolfCrypt RSA key (internal).
 *
 * @param [in, out] rsa  RSA key.
 * @return  1 on success.
 * @return  0 on failure.
 */
int SetRsaExternal(WOLFSSL_RSA* rsa)
{
    int ret = 1;

    WOLFSSL_ENTER("SetRsaExternal");

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("rsa key NULL error");
        ret = -1;
    }

    if (ret == 1) {
        RsaKey* key = (RsaKey*)rsa->internal;

        /* Copy modulus. */
        ret = SetIndividualExternal(&rsa->n, &key->n);
        if (ret != 1) {
            WOLFSSL_ERROR_MSG("rsa n error");
        }
        if (ret == 1) {
            /* Copy public exponent. */
            ret = SetIndividualExternal(&rsa->e, &key->e);
            if (ret != 1) {
                WOLFSSL_ERROR_MSG("rsa e error");
            }
        }

        if (key->type == RSA_PRIVATE) {
            if (ret == 1) {
                /* Copy private exponent. */
                ret = SetIndividualExternal(&rsa->d, &key->d);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa d error");
                }
            }
            if (ret == 1) {
                /* Copy first prime. */
                ret = SetIndividualExternal(&rsa->p, &key->p);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa p error");
                }
            }
            if (ret == 1) {
                /* Copy second prime. */
                ret = SetIndividualExternal(&rsa->q, &key->q);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa q error");
                }
            }
        #ifndef RSA_LOW_MEM
            if (ret == 1) {
                /* Copy d mod p-1. */
                ret = SetIndividualExternal(&rsa->dmp1, &key->dP);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa dP error");
                }
            }
            if (ret == 1) {
                /* Copy d mod q-1. */
                ret = SetIndividualExternal(&rsa->dmq1, &key->dQ);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa dq error");
                }
            }
            if (ret == 1) {
                /* Copy 1/q mod p. */
                ret = SetIndividualExternal(&rsa->iqmp, &key->u);
                if (ret != 1) {
                    WOLFSSL_ERROR_MSG("rsa u error");
                }
            }
        #endif /* !RSA_LOW_MEM */
        }
    }
    if (ret == 1) {
        /* External values set. */
        rsa->exSet = 1;
    }
    else {
        /* Return 0 on failure. */
        ret = 0;
    }

    return ret;
}
#endif /* !HAVE_USER_RSA && !HAVE_FAST_RSA */
#endif /* (OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL) */

#ifdef OPENSSL_EXTRA

#if !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA)
/* Set wolfCrypt RSA key data (internal) from RSA key (external).
 *
 * @param [in, out] rsa  RSA key.
 * @return  1 on success.
 * @return  0 on failure.
 */
int SetRsaInternal(WOLFSSL_RSA* rsa)
{
    int ret = 1;

    WOLFSSL_ENTER("SetRsaInternal");

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("rsa key NULL error");
        ret = -1;
    }

    if (ret == 1) {
        RsaKey* key = (RsaKey*)rsa->internal;

        /* Copy down modulus if available. */
        if ((rsa->n != NULL) && (SetIndividualInternal(rsa->n, &key->n) != 1)) {
            WOLFSSL_ERROR_MSG("rsa n key error");
            ret = -1;
        }

        /* Copy down public exponent if available. */
        if ((ret == 1) && (rsa->e != NULL) &&
                (SetIndividualInternal(rsa->e, &key->e) != 1)) {
            WOLFSSL_ERROR_MSG("rsa e key error");
            ret = -1;
        }

        /* Enough numbers for public key */
        key->type = RSA_PUBLIC;

        /* Copy down private exponent if available. */
        if ((ret == 1) && (rsa->d != NULL)) {
            if (SetIndividualInternal(rsa->d, &key->d) != 1) {
                WOLFSSL_ERROR_MSG("rsa d key error");
                ret = -1;
            }
            else {
                /* Enough numbers for private key */
                key->type = RSA_PRIVATE;
           }
        }

        /* Copy down first prime if available. */
        if ((ret == 1) && (rsa->p != NULL) &&
                (SetIndividualInternal(rsa->p, &key->p) != 1)) {
            WOLFSSL_ERROR_MSG("rsa p key error");
            ret = -1;
        }

        /* Copy down second prime if available. */
        if ((ret == 1) && (rsa->q != NULL) &&
                (SetIndividualInternal(rsa->q, &key->q) != 1)) {
            WOLFSSL_ERROR_MSG("rsa q key error");
            ret = -1;
        }

    #ifndef RSA_LOW_MEM
        /* Copy down d mod p-1 if available. */
        if ((ret == 1) && (rsa->dmp1 != NULL) &&
                (SetIndividualInternal(rsa->dmp1, &key->dP) != 1)) {
            WOLFSSL_ERROR_MSG("rsa dP key error");
            ret = -1;
        }

        /* Copy down d mod q-1 if available. */
        if ((ret == 1) && (rsa->dmp1 != NULL) &&
                (SetIndividualInternal(rsa->dmq1, &key->dQ) != 1)) {
            WOLFSSL_ERROR_MSG("rsa dQ key error");
            ret = -1;
        }

        /* Copy down 1/q mod p if available. */
        if ((ret == 1) && (rsa->iqmp != NULL) &&
                (SetIndividualInternal(rsa->iqmp, &key->u) != 1)) {
            WOLFSSL_ERROR_MSG("rsa u key error");
            ret = -1;
        }
    #endif /* !RSA_LOW_MEM */

        if (ret == 1) {
            /* All available numbers have been set down. */
            rsa->inSet = 1;
        }
    }

    return ret;
}

#endif /* HAVE_USER_RSA */

/* Set the RSA method into object.
 *
 * @param [in, out] rsa   RSA key.
 * @param [in]      meth  RSA method.
 * @return  1 always.
 */
int wolfSSL_RSA_set_method(WOLFSSL_RSA *rsa, WOLFSSL_RSA_METHOD *meth)
{
    if (rsa != NULL) {
        /* Store the method into object. */
        rsa->meth = meth;
        /* Copy over flags. */
        rsa->flags = meth->flags;
    }
    /* OpenSSL always assumes it will work. */
    return 1;
}

/* Get the RSA method from the RSA object.
 *
 * @param [in] rsa  RSA key.
 * @return  RSA method on success.
 * @return  NULL when RSA is NULL or no method set.
 */
const WOLFSSL_RSA_METHOD* wolfSSL_RSA_get_method(const WOLFSSL_RSA *rsa)
{
    return (rsa != NULL) ? rsa->meth : NULL;
}

/* Get the size in bytes of the RSA key.
 *
 * Return compliant with OpenSSL
 *
 * @param [in] rsa  RSA key.
 * @return  RSA modulus size in bytes.
 * @return  0 on error.
 */
int wolfSSL_RSA_size(const WOLFSSL_RSA* rsa)
{
    int ret = 0;

    WOLFSSL_ENTER("wolfSSL_RSA_size");

    if (rsa != NULL) {
        /* Make sure we have set the RSA values into wolfCrypt RSA key. */
        if (rsa->inSet || (SetRsaInternal((WOLFSSL_RSA*)rsa) == 1)) {
            /* Get key size in bytes using wolfCrypt RSA key. */
            ret = wc_RsaEncryptSize((RsaKey*)rsa->internal);
        }
    }

    return ret;
}

/* Get the size in bits of the RSA key.
 *
 * Uses external modulus field.
 *
 * @param [in] rsa  RSA key.
 * @return  RSA modulus size in bits.
 * @return  0 on error.
 */
int wolfSSL_RSA_bits(const WOLFSSL_RSA* rsa)
{
    int ret = 0;

    WOLFSSL_ENTER("wolfSSL_RSA_bits");

    if (rsa != NULL) {
        /* Get number of bits in external modulus. */
        ret = wolfSSL_BN_num_bits(rsa->n);
    }

    return ret;
}

#ifndef HAVE_USER_RSA

/* Get the BN objects that are the Chinese-Remainder Theorem (CRT) parameters.
 *
 * Only for those that are not NULL parameters.
 *
 * @param [in]  rsa   RSA key.
 * @param [out] dmp1  BN that is d mod (p - 1). May be NULL.
 * @param [out] dmq1  BN that is d mod (q - 1). May be NULL.
 * @param [out] iqmp  BN that is 1/q mod p. May be NULL.
 */
void wolfSSL_RSA_get0_crt_params(const WOLFSSL_RSA *rsa,
    const WOLFSSL_BIGNUM **dmp1, const WOLFSSL_BIGNUM **dmq1,
    const WOLFSSL_BIGNUM **iqmp)
{
    WOLFSSL_ENTER("wolfSSL_RSA_get0_crt_params");

    /* For any parameters not NULL, return the BN from the key or NULL. */
    if (dmp1 != NULL) {
        *dmp1 = (rsa != NULL) ? rsa->dmp1 : NULL;
    }
    if (dmq1 != NULL) {
        *dmq1 = (rsa != NULL) ? rsa->dmq1 : NULL;
    }
    if (iqmp != NULL) {
        *iqmp = (rsa != NULL) ? rsa->iqmp : NULL;
    }
}

/* Set the BN objects that are the Chinese-Remainder Theorem (CRT) parameters
 * into RSA key.
 *
 * If CRT parameter is NULL then there must be one in the RSA key already.
 *
 * @param [in, out] rsa   RSA key.
 * @param [in]      dmp1  BN that is d mod (p - 1). May be NULL.
 * @param [in]      dmq1  BN that is d mod (q - 1). May be NULL.
 * @param [in]      iqmp  BN that is 1/q mod p. May be NULL.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_set0_crt_params(WOLFSSL_RSA *rsa, WOLFSSL_BIGNUM *dmp1,
                                WOLFSSL_BIGNUM *dmq1, WOLFSSL_BIGNUM *iqmp)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_RSA_set0_crt_params");

    /* If a param is NULL in rsa then it must be non-NULL in the
     * corresponding user input. */
    if ((rsa == NULL) || ((rsa->dmp1 == NULL) && (dmp1 == NULL)) ||
            ((rsa->dmq1 == NULL) && (dmq1 == NULL)) ||
            ((rsa->iqmp == NULL) && (iqmp == NULL))) {
        WOLFSSL_ERROR_MSG("Bad parameters");
        ret = 0;
    }
    if (ret == 1) {
        /* Replace the BNs. */
        if (dmp1 != NULL) {
            wolfSSL_BN_clear_free(rsa->dmp1);
            rsa->dmp1 = dmp1;
        }
        if (dmq1 != NULL) {
            wolfSSL_BN_clear_free(rsa->dmq1);
            rsa->dmq1 = dmq1;
        }
        if (iqmp != NULL) {
            wolfSSL_BN_clear_free(rsa->iqmp);
            rsa->iqmp = iqmp;
        }

        /* Set the values into the wolfCrypt RSA key. */
        if (SetRsaInternal(rsa) != 1) {
            ret = 0;
        }
    }

    return ret;
}

/* Get the BN objects that are the factors of the RSA key (two primes p and q).
 *
 * @param [in]  rsa  RSA key.
 * @param [out] p    BN that is first prime. May be NULL.
 * @param [out] q    BN that is second prime. May be NULL.
 */
void wolfSSL_RSA_get0_factors(const WOLFSSL_RSA *rsa, const WOLFSSL_BIGNUM **p,
                              const WOLFSSL_BIGNUM **q)
{
    WOLFSSL_ENTER("wolfSSL_RSA_get0_factors");

    /* For any primes not NULL, return the BN from the key or NULL. */
    if (p != NULL) {
        *p = (rsa != NULL) ? rsa->p : NULL;
    }
    if (q != NULL) {
        *q = (rsa != NULL) ? rsa->q : NULL;
    }
}

/* Set the BN objects that are the factors of the RSA key (two primes p and q).
 *
 * If factor parameter is NULL then there must be one in the RSA key already.
 *
 * @param [in, out] rsa  RSA key.
 * @param [in]      p    BN that is first prime. May be NULL.
 * @param [in]      q    BN that is second prime. May be NULL.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_set0_factors(WOLFSSL_RSA *rsa, WOLFSSL_BIGNUM *p,
    WOLFSSL_BIGNUM *q)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_RSA_set0_factors");

    /* If a param is null in r then it must be non-null in the
     * corresponding user input. */
    if (rsa == NULL || ((rsa->p == NULL) && (p == NULL)) ||
            ((rsa->q == NULL) && (q == NULL))) {
        WOLFSSL_ERROR_MSG("Bad parameters");
        ret = 0;
    }
    if (ret == 1) {
        /* Replace the BNs. */
        if (p != NULL) {
            wolfSSL_BN_clear_free(rsa->p);
            rsa->p = p;
        }
        if (q != NULL) {
            wolfSSL_BN_clear_free(rsa->q);
            rsa->q = q;
        }

        /* Set the values into the wolfCrypt RSA key. */
        if (SetRsaInternal(rsa) != 1) {
             ret = 0;
        }
    }

    return ret;
}

/* Get the BN objects for the basic key numbers of the RSA key (modulus, public
 * exponent, private exponent).
 *
 * @param [in]  rsa  RSA key.
 * @param [out] n    BN that is the modulus. May be NULL.
 * @param [out] e    BN that is the public exponent. May be NULL.
 * @param [out] d    BN that is the private exponent. May be NULL.
 */
void wolfSSL_RSA_get0_key(const WOLFSSL_RSA *rsa, const WOLFSSL_BIGNUM **n,
    const WOLFSSL_BIGNUM **e, const WOLFSSL_BIGNUM **d)
{
    WOLFSSL_ENTER("wolfSSL_RSA_get0_key");

    /* For any parameters not NULL, return the BN from the key or NULL. */
    if (n != NULL) {
        *n = (rsa != NULL) ? rsa->n : NULL;
    }
    if (e != NULL) {
        *e = (rsa != NULL) ? rsa->e : NULL;
    }
    if (d != NULL) {
        *d = (rsa != NULL) ? rsa->d : NULL;
    }
}

/* Set the BN objects for the basic key numbers into the RSA key (modulus,
 * public exponent, private exponent).
 *
 * If BN parameter is NULL then there must be one in the RSA key already.
 *
 * @param [in,out]  rsa  RSA key.
 * @param [in]      n    BN that is the modulus. May be NULL.
 * @param [in]      e    BN that is the public exponent. May be NULL.
 * @param [in]      d    BN that is the private exponent. May be NULL.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_set0_key(WOLFSSL_RSA *rsa, WOLFSSL_BIGNUM *n, WOLFSSL_BIGNUM *e,
     WOLFSSL_BIGNUM *d)
{
    int ret = 1;

    /* If the fields n and e in r are NULL, the corresponding input
     * parameters MUST be non-NULL for n and e.  d may be
     * left NULL (in case only the public key is used).
     */
    if ((rsa == NULL) || ((rsa->n == NULL) && (n == NULL)) ||
            ((rsa->e == NULL) && (e == NULL))) {
        ret = 0;
    }
    if (ret == 1) {
        /* Replace the BNs. */
        if (n != NULL) {
            wolfSSL_BN_free(rsa->n);
            rsa->n = n;
        }
        if (e != NULL) {
            wolfSSL_BN_free(rsa->e);
            rsa->e = e;
        }
        if (d != NULL) {
            /* Private key is sensitive data. */
            wolfSSL_BN_clear_free(rsa->d);
            rsa->d = d;
        }

        /* Set the values into the wolfCrypt RSA key. */
        if (SetRsaInternal(rsa) != 1) {
            ret = 0;
        }
    }

    return ret;
}

#endif /* !HAVE_USER_RSA */

/* Get the flags of the RSA key.
 *
 * @param [in] rsa  RSA key.
 * @return  Flags set in RSA key on success.
 * @return  0 when RSA key is NULL.
 */
int wolfSSL_RSA_flags(const WOLFSSL_RSA *rsa)
{
    int ret = 0;

    /* Get flags from the RSA key if available. */
    if (rsa != NULL) {
        ret = rsa->flags;
    }

    return ret;
}

/* Set the flags into the RSA key.
 *
 * @param [in, out] rsa    RSA key.
 * @param [in]      flags  Flags to set.
 */
void wolfSSL_RSA_set_flags(WOLFSSL_RSA *rsa, int flags)
{
    /* Add the flags into RSA key if available. */
    if (rsa != NULL) {
        rsa->flags |= flags;
    }
}

/* Clear the flags in the RSA key.
 *
 * @param [in, out] rsa    RSA key.
 * @param [in]      flags  Flags to clear.
 */
void wolfSSL_RSA_clear_flags(WOLFSSL_RSA *rsa, int flags)
{
    /* Clear the flags passed in that are on the RSA key if available. */
    if (rsa != NULL) {
        rsa->flags &= ~flags;
    }
}

/* Test the flags in the RSA key.
 *
 * @param [in] rsa  RSA key.
 * @return  Matching flags of RSA key on success.
 * @return  0 when RSA key is NULL.
 */
int wolfSSL_RSA_test_flags(const WOLFSSL_RSA *rsa, int flags)
{
    /* Return the flags passed in that are set on the RSA key if available. */
    return (rsa != NULL) ?  (rsa->flags & flags) : 0;
}

/* Get the extra data, by index, associated with the RSA key.
 *
 * @param [in] rsa  RSA key.
 * @param [in] idx  Index of extra data.
 * @return  Extra data (anonymous type) on success.
 * @return  NULL on failure.
 */
void* wolfSSL_RSA_get_ex_data(const WOLFSSL_RSA *rsa, int idx)
{
    WOLFSSL_ENTER("wolfSSL_RSA_get_ex_data");

#ifdef HAVE_EX_DATA
    return (rsa == NULL) ? NULL :
        wolfSSL_CRYPTO_get_ex_data(&rsa->ex_data, idx);
#else
    (void)rsa;
    (void)idx;

    return NULL;
#endif
}

/* Set extra data against the RSA key at an index.
 *
 * @param [in, out] rsa   RSA key.
 * @param [in]      idx   Index set set extra data at.
 * @param [in]      data  Extra data of anonymous type.
 * @return 1 on success.
 * @return 0 on failure.
 */
int wolfSSL_RSA_set_ex_data(WOLFSSL_RSA *rsa, int idx, void *data)
{
    WOLFSSL_ENTER("wolfSSL_RSA_set_ex_data");

#ifdef HAVE_EX_DATA
    return (rsa == NULL) ? 0 :
        wolfSSL_CRYPTO_set_ex_data(&rsa->ex_data, idx, data);
#else
    (void)rsa;
    (void)idx;
    (void)data;

    return 0;
#endif
}

#ifdef HAVE_EX_DATA_CLEANUP_HOOKS
/* Set the extra data and cleanup callback against the RSA key at an index.
 *
 * wolfSSL API.
 *
 * @param [in, out] rsa     RSA key.
 * @param [in]      idx     Index set set extra data at.
 * @param [in]      data    Extra data of anonymous type.
 * @param [in]      freeCb  Callback function to free extra data.
 * @return 1 on success.
 * @return 0 on failure.
 */
int wolfSSL_RSA_set_ex_data_with_cleanup(WOLFSSL_RSA *rsa, int idx, void *data,
    wolfSSL_ex_data_cleanup_routine_t freeCb)
{
    WOLFSSL_ENTER("wolfSSL_RSA_set_ex_data_with_cleanup");

    return (rsa == NULL) ? 0 :
        wolfSSL_CRYPTO_set_ex_data_with_cleanup(&rsa->ex_data, idx, data,
            freeCb);
}
#endif /* HAVE_EX_DATA_CLEANUP_HOOKS */

/*
 * RSA check key APIs
 */

#ifdef WOLFSSL_RSA_KEY_CHECK
/* Check that the RSA key is valid using wolfCrypt.
 *
 * @param [in] rsa  RSA key.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_check_key(const WOLFSSL_RSA* rsa)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_RSA_check_key");

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->internal == NULL)) {
        ret = 0;
    }

    /* Constant RSA - assume internal data has been set. */

    /* Check wolfCrypt RSA key. */
    if ((ret == 1) && (wc_CheckRsaKey((RsaKey*)rsa->internal) != 0)) {
        ret = 0;
    }

    WOLFSSL_LEAVE("wolfSSL_RSA_check_key", ret);

    return ret;
}
#endif /* WOLFSSL_RSA_KEY_CHECK */

/*
 * RSA generate APIs
 */

#if !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA)
/* Get a random number generator associated with the RSA key.
 *
 * If not able, then get the global if possible.
 * *tmpRng must not be an initialized RNG.
 * *tmpRng is allocated when WOLFSSL_SMALL_STACK is defined and an RNG isn't
 * associated with the wolfCrypt RSA key.
 *
 * @param [in]  rsa         RSA key.
 * @param [out] tmpRng      Temporary random number generator.
 * @param [out] initTmpRng  Temporary random number generator was initialized.
 *
 * @return  A wolfCrypt RNG to use on success.
 * @return  NULL on error.
 */
WC_RNG* WOLFSSL_RSA_GetRNG(WOLFSSL_RSA* rsa, WC_RNG** tmpRng, int* initTmpRng)
{
    WC_RNG* rng = NULL;
    int err = 0;

    /* Check validity of parameters. */
    if ((rsa == NULL) || (initTmpRng == NULL)) {
        err = 1;
    }
    if (!err) {
        /* Haven't initialized any RNG passed through tmpRng. */
        *initTmpRng = 0;

    #if !defined(HAVE_FIPS) && defined(WC_RSA_BLINDING)
        /* Use wolfCrypt RSA key's RNG if available/set. */
        rng = ((RsaKey*)rsa->internal)->rng;
    #endif
    }
    if ((!err) && (rng == NULL) && (tmpRng != NULL)) {
        /* Make an RNG with tmpRng or get global. */
        rng = wolfssl_make_rng(*tmpRng, initTmpRng);
        if ((rng != NULL) && *initTmpRng) {
            *tmpRng = rng;
        }
    }

    return rng;
}
#endif

/* Use the wolfCrypt RSA APIs to generate a new RSA key.
 *
 * @param [in, out] rsa   RSA key.
 * @param [in]      bits  Number of bits that the modulus must have.
 * @param [in]      e     A BN object holding the public exponent to use.
 * @param [in]      cb    Status callback. Unused.
 * @return 0 on success.
 * @return wolfSSL native error code on error.
 */
static int wolfssl_rsa_generate_key_native(WOLFSSL_RSA* rsa, int bits,
    WOLFSSL_BIGNUM* e, void* cb)
{
#ifdef WOLFSSL_KEY_GEN
    int ret = 0;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  _tmpRng[1];
    WC_RNG* tmpRng = _tmpRng;
#endif
    int initTmpRng = 0;
    WC_RNG* rng = NULL;
#endif

    (void)cb;

    WOLFSSL_ENTER("wolfssl_rsa_generate_key_native");

#ifdef WOLFSSL_KEY_GEN
    /* Get RNG in wolfCrypt RSA key or initialize a new one (or global). */
    rng = WOLFSSL_RSA_GetRNG(rsa, (WC_RNG**)&tmpRng, &initTmpRng);
    if (rng == NULL) {
        /* Something went wrong so return memory error. */
        ret = MEMORY_E;
    }
    if (ret == 0) {
        /* Generate an RSA key. */
        ret = wc_MakeRsaKey((RsaKey*)rsa->internal, bits,
            (long)wolfSSL_BN_get_word(e), rng);
        if (ret != MP_OKAY) {
            WOLFSSL_ERROR_MSG("wc_MakeRsaKey failed");
        }
    }
    if (ret == 0) {
        /* Get the values from wolfCrypt RSA key into external RSA key. */
        ret = SetRsaExternal(rsa);
        if (ret == 1) {
            /* Internal matches external. */
            rsa->inSet = 1;
            /* Return success. */
            ret = 0;
        }
        else {
            /* Something went wrong so return memory error. */
            ret = MEMORY_E;
        }
    }

    /* Finalize RNG if initialized in WOLFSSL_RSA_GetRNG(). */
    if (initTmpRng) {
        wc_FreeRng(tmpRng);
    }
#ifdef WOLFSSL_SMALL_STACK
    /* Dispose of any allocated RNG. */
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    return ret;
#else
    WOLFSSL_ERROR_MSG("No Key Gen built in");

    (void)rsa;
    (void)e;
    (void)bits;

    return NOT_COMPILED_IN;
#endif
}

/* Generate an RSA key that has the specified modulus size and public exponent.
 *
 * Note: Because of wc_MakeRsaKey an RSA key size generated can be rounded
 *       down to nearest multiple of 8. For example generating a key of size
 *       2999 bits will make a key of size 374 bytes instead of 375 bytes.
 *
 * @param [in]      bits  Number of bits that the modulus must have i.e. 2048.
 * @param [in]      e     Public exponent to use i.e. 65537.
 * @param [in]      cb    Status callback. Unused.
 * @param [in]      data  Data to pass to status callback. Unused.
 * @return  A new RSA key on success.
 * @return  NULL on failure.
 */
WOLFSSL_RSA* wolfSSL_RSA_generate_key(int bits, unsigned long e,
    void(*cb)(int, int, void*), void* data)
{
    WOLFSSL_RSA*    rsa = NULL;
    WOLFSSL_BIGNUM* bn  = NULL;
    int             err = 0;

    WOLFSSL_ENTER("wolfSSL_RSA_generate_key");

    (void)cb;
    (void)data;

    /* Validate bits. */
    if (bits < 0) {
        WOLFSSL_ERROR_MSG("Bad argument: bits was less than 0");
        err = 1;
    }
    /* Create a new BN to hold public exponent - for when wolfCrypt supports
     * longer values. */
    if ((!err) && ((bn = wolfSSL_BN_new()) == NULL)) {
        WOLFSSL_ERROR_MSG("Error creating big number");
        err = 1;
    }
    /* Set public exponent. */
    if ((!err) && (wolfSSL_BN_set_word(bn, e) != 1)) {
        WOLFSSL_ERROR_MSG("Error using e value");
        err = 1;
    }

    /* Create an RSA key object to hold generated key. */
    if ((!err) && ((rsa = wolfSSL_RSA_new()) == NULL)) {
        WOLFSSL_ERROR_MSG("memory error");
        err = 1;
    }
    while (!err) {
        int ret;

        /* Use wolfCrypt to generate RSA key. */
        ret = wolfssl_rsa_generate_key_native(rsa, bits, bn, NULL);
    #ifdef HAVE_FIPS
        /* Keep trying if failed to find a prime. */
        if (ret == PRIME_GEN_E) {
            continue;
        }
    #endif
        if (ret != WOLFSSL_ERROR_NONE) {
            /* Unrecoverable error in generation. */
            err = 1;
        }
        /* Done generating - unrecoverable error or success. */
        break;
    }
    if (err) {
        /* Dispose of RSA key object if generation didn't work. */
        wolfSSL_RSA_free(rsa);
        /* Returning NULL on error. */
        rsa = NULL;
    }
    /* Dispose of the temporary BN used for the public exponent. */
    wolfSSL_BN_free(bn);

    return rsa;
}

/* Generate an RSA key that has the specified modulus size and public exponent.
 *
 * Note: Because of wc_MakeRsaKey an RSA key size generated can be rounded
 *       down to nearest multiple of 8. For example generating a key of size
 *       2999 bits will make a key of size 374 bytes instead of 375 bytes.
 *
 * @param [in]      bits  Number of bits that the modulus must have i.e. 2048.
 * @param [in]      e     Public exponent to use, i.e. 65537, as a BN.
 * @param [in]      cb    Status callback. Unused.
 * @return 1 on success.
 * @return 0 on failure.
 */
int wolfSSL_RSA_generate_key_ex(WOLFSSL_RSA* rsa, int bits, WOLFSSL_BIGNUM* e,
    void* cb)
{
    int ret = 1;

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->internal == NULL)) {
        WOLFSSL_ERROR_MSG("bad arguments");
        ret = 0;
    }
    else {
        for (;;) {
            /* Use wolfCrypt to generate RSA key. */
            int gen_ret = wolfssl_rsa_generate_key_native(rsa, bits, e, cb);
        #ifdef HAVE_FIPS
            /* Keep trying again if public key value didn't work. */
            if (gen_ret == PRIME_GEN_E) {
                continue;
            }
        #endif
            if (gen_ret != WOLFSSL_ERROR_NONE) {
                /* Unrecoverable error in generation. */
                ret = 0;
            }
            /* Done generating - unrecoverable error or success. */
            break;
        }
    }

    return ret;
}

#endif /* OPENSSL_EXTRA */

/*
 * RSA padding APIs
 */

#if defined(WC_RSA_PSS) && (defined(OPENSSL_ALL) || defined(WOLFSSL_ASIO) || \
        defined(WOLFSSL_HAPROXY) || defined(WOLFSSL_NGINX))
#if !defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0)
/* Add PKCS#1 PSS padding to hash.
 *
 *
 *                                +-----------+
 *                                |     M     |
 *                                +-----------+
 *                                      |
 *                                      V
 *                                    Hash
 *                                      |
 *                                      V
 *                        +--------+----------+----------+
 *                   M' = |Padding1|  mHash   |   salt   |
 *                        +--------+----------+----------+
 *                                       |
 *             +--------+----------+     V
 *       DB =  |Padding2|maskedseed|   Hash
 *             +--------+----------+     |
 *                       |               |
 *                       V               |    +--+
 *                      xor <--- MGF <---|    |bc|
 *                       |               |    +--+
 *                       |               |      |
 *                       V               V      V
 *             +-------------------+----------+--+
 *       EM =  |    maskedDB       |maskedseed|bc|
 *             +-------------------+----------+--+
 * Diagram taken from https://tools.ietf.org/html/rfc3447#section-9.1
 *
 * @param [in]  rsa      RSA key.
 * @param [out] em       Encoded message.
 * @param [in[  mHash    Message hash.
 * @param [in]  hashAlg  Hash algorithm.
 * @param [in]  saltLen  Length of salt to generate.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_padding_add_PKCS1_PSS(WOLFSSL_RSA *rsa, unsigned char *em,
    const unsigned char *mHash, const WOLFSSL_EVP_MD *hashAlg, int saltLen)
{
    int ret = 1;
    enum wc_HashType hashType;
    int hashLen;
    int emLen;
    int mgf;
    int initTmpRng = 0;
    WC_RNG *rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  _tmpRng[1];
    WC_RNG* tmpRng = _tmpRng;
#endif

    WOLFSSL_ENTER("wolfSSL_RSA_padding_add_PKCS1_PSS");

    /* Validate parameters. */
    if ((rsa == NULL) || (em == NULL) || (mHash == NULL) || (hashAlg == NULL)) {
        ret = 0;
    }

    if (ret == 1) {
        /* Get/create an RNG. */
        rng = WOLFSSL_RSA_GetRNG(rsa, (WC_RNG**)&tmpRng, &initTmpRng);
        if (rng == NULL) {
            WOLFSSL_ERROR_MSG("WOLFSSL_RSA_GetRNG error");
            ret = 0;
        }
    }

    /* TODO: use wolfCrypt RSA key to get emLen and bits? */
    /* Set the external data from the wolfCrypt RSA key if not done. */
    if ((ret == 1) && (!rsa->exSet)) {
        ret = SetRsaExternal(rsa);
    }

    if (ret == 1) {
        /* Get the wolfCrypt hash algorithm type. */
        hashType = EvpMd2MacType(hashAlg);
        if (hashType > WC_HASH_TYPE_MAX) {
            WOLFSSL_ERROR_MSG("EvpMd2MacType error");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Get the wolfCrypt MGF algorithm from hash algorithm. */
        mgf = wc_hash2mgf(hashType);
        if (mgf == WC_MGF1NONE) {
            WOLFSSL_ERROR_MSG("wc_hash2mgf error");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Get the length of the hash output. */
        hashLen = wolfSSL_EVP_MD_size(hashAlg);
        if (hashLen < 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_EVP_MD_size error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Get length of RSA key - encrypted message length. */
        emLen = wolfSSL_RSA_size(rsa);
        if (ret <= 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_size error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Calculate the salt length to use for special cases. */
        /* TODO: use special case wolfCrypt values? */
        switch (saltLen) {
        /* Negative saltLen values are treated differently. */
        case RSA_PSS_SALTLEN_DIGEST:
            saltLen = hashLen;
            break;
        case RSA_PSS_SALTLEN_MAX_SIGN:
        case RSA_PSS_SALTLEN_MAX:
        #ifdef WOLFSSL_PSS_LONG_SALT
            saltLen = emLen - hashLen - 2;
        #else
            saltLen = hashLen;
        #endif
            break;
        default:
            if (saltLen < 0) {
                /* No other negative values implemented. */
                WOLFSSL_ERROR_MSG("invalid saltLen");
                ret = 0;
            }
        }
    }

    if (ret == 1) {
        /* Generate RSA PKCS#1 PSS padding for hash using wolfCrypt. */
        if (wc_RsaPad_ex(mHash, hashLen, em, emLen, RSA_BLOCK_TYPE_1, rng,
                WC_RSA_PSS_PAD, hashType, mgf, NULL, 0, saltLen,
                wolfSSL_BN_num_bits(rsa->n), NULL) != MP_OKAY) {
            WOLFSSL_ERROR_MSG("wc_RsaPad_ex error");
            ret = 0;
        }
    }

    /* Finalize RNG if initialized in WOLFSSL_RSA_GetRNG(). */
    if (initTmpRng) {
        wc_FreeRng(tmpRng);
    }
#ifdef WOLFSSL_SMALL_STACK
    /* Dispose of any allocated RNG. */
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    return ret;
}

/* Checks that the hash is valid for the RSA PKCS#1 PSS encoded message.
 *
 * Refer to wolfSSL_RSA_padding_add_PKCS1_PSS for a diagram.
 *
 * @param [in]  rsa      RSA key.
 * @param [in[  mHash    Message hash.
 * @param [in]  hashAlg  Hash algorithm.
 * @param [in]  em       Encoded message.
 * @param [in]  saltLen  Length of salt to generate.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_verify_PKCS1_PSS(WOLFSSL_RSA *rsa, const unsigned char *mHash,
                                 const WOLFSSL_EVP_MD *hashAlg,
                                 const unsigned char *em, int saltLen)
{
    int ret = 1;
    int hashLen;
    int mgf;
    int emLen;
    int mPrimeLen;
    enum wc_HashType hashType;
    byte *mPrime = NULL;
    byte *buf = NULL;

    WOLFSSL_ENTER("wolfSSL_RSA_verify_PKCS1_PSS");

    /* Validate parameters. */
    if ((rsa == NULL) || (mHash == NULL) || (hashAlg == NULL) || (em == NULL)) {
        ret = 0;
    }

    /* TODO: use wolfCrypt RSA key to get emLen and bits? */
    /* Set the external data from the wolfCrypt RSA key if not done. */
    if ((ret == 1) && (!rsa->exSet)) {
        ret = SetRsaExternal(rsa);
    }

    if (ret == 1) {
        /* Get hash length for hash algorithm. */
        hashLen = wolfSSL_EVP_MD_size(hashAlg);
        if (hashLen < 0) {
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Get length of RSA key - encrypted message length. */
        emLen = wolfSSL_RSA_size(rsa);
        if (emLen <= 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_size error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Calculate the salt length to use for special cases. */
        /* TODO: use special case wolfCrypt values. */
        switch (saltLen) {
        /* Negative saltLen values are treated differently */
        case RSA_PSS_SALTLEN_DIGEST:
            saltLen = hashLen;
            break;
        case RSA_PSS_SALTLEN_MAX_SIGN:
        case RSA_PSS_SALTLEN_MAX:
        #ifdef WOLFSSL_PSS_LONG_SALT
            saltLen = emLen - hashLen - 2;
        #else
            saltLen = hashLen;
        #endif
            break;
        default:
            if (saltLen < 0) {
                /* No other negative values implemented. */
                WOLFSSL_ERROR_MSG("invalid saltLen");
                ret = 0;
            }
        }
    }

    if (ret == 1) {
        /* Get the wolfCrypt hash algorithm type. */
        hashType = EvpMd2MacType(hashAlg);
        if (hashType > WC_HASH_TYPE_MAX) {
            WOLFSSL_ERROR_MSG("EvpMd2MacType error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Get the wolfCrypt MGF algorithm from hash algorithm. */
        if ((mgf = wc_hash2mgf(hashType)) == WC_MGF1NONE) {
            WOLFSSL_ERROR_MSG("wc_hash2mgf error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Allocate buffer to unpad inline with. */
        buf = (byte*)XMALLOC(emLen, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (buf == NULL) {
            WOLFSSL_ERROR_MSG("malloc error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Copy encrypted message to temp for inline unpadding. */
        XMEMCPY(buf, em, emLen);

        /* Remove and verify the PSS padding. */
        mPrimeLen = wc_RsaUnPad_ex(buf, emLen, &mPrime, RSA_BLOCK_TYPE_1,
            WC_RSA_PSS_PAD, hashType, mgf, NULL, 0, saltLen,
            wolfSSL_BN_num_bits(rsa->n), NULL);
        if (mPrimeLen < 0) {
            WOLFSSL_ERROR_MSG("wc_RsaPad_ex error");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Verify the hash is correct. */
        if (wc_RsaPSS_CheckPadding_ex(mHash, hashLen, mPrime, mPrimeLen,
                hashType, saltLen, wolfSSL_BN_num_bits(rsa->n)) != MP_OKAY) {
            WOLFSSL_ERROR_MSG("wc_RsaPSS_CheckPadding_ex error");
            ret = 0;
        }
    }

    /* Dispose of any allocated buffer. */
    XFREE(buf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}
#endif /* !HAVE_FIPS || FIPS_VERSION_GT(2,0) */
#endif /* WC_RSA_PSS && (OPENSSL_ALL || WOLFSSL_ASIO || WOLFSSL_HAPROXY ||
        *                WOLFSSL_NGINX) */

/*
 * RSA sign/verify APIs
 */

#ifndef WOLFSSL_PSS_SALT_LEN_DISCOVER
    #define DEF_PSS_SALT_LEN    RSA_PSS_SALT_LEN_DEFAULT
#else
    #define DEF_PSS_SALT_LEN    RSA_PSS_SALT_LEN_DISCOVER
#endif

#if defined(OPENSSL_EXTRA)

#if !defined(HAVE_USER_RSA)

/* Encode the message hash.
 *
 * Used by signing and verification.
 *
 * @param [in]  hashAlg   Hash algorithm OID.
 * @param [in]  hash      Hash of message to encode for signing.
 * @param [in]  hLen      Length of hash of message.
 * @param [out] enc       Encoded message hash.
 * @param [out] encLen    Length of encoded message hash.
 * @param [in]  padding   Which padding scheme is being used.
 * @return  1 on success.
 * @return  0 on failure.
 */
static int wolfssl_rsa_sig_encode(int hashAlg, const unsigned char* hash,
    unsigned int hLen, unsigned char* enc, unsigned int* encLen, int padding)
{
    int ret = 1;
    int hType = WC_HASH_TYPE_NONE;

    /* Validate parameters. */
    if ((hash == NULL) || (enc == NULL) || (encLen == NULL)) {
        ret = 0;
    }

    if ((ret == 1) && (hashAlg != NID_undef) &&
            (padding == RSA_PKCS1_PADDING)) {
        /* Convert hash algorithm to hash type for PKCS#1.5 padding. */
        hType = nid2oid(hashAlg, oidHashType);
        if (hType == -1) {
            ret = 0;
        }
    }
    if ((ret == 1) && (padding == RSA_PKCS1_PADDING)) {
        /* PKCS#1.5 encoding. */
        word32 encSz = wc_EncodeSignature(enc, hash, hLen, hType);
        if (encSz == 0) {
            WOLFSSL_ERROR_MSG("Bad Encode Signature");
            ret = 0;
        }
        else  {
            *encLen = (unsigned int)encSz;
        }
    }
    /* Other padding schemes require the hash as is. */
    if ((ret == 1) && (padding != RSA_PKCS1_PADDING)) {
        XMEMCPY(enc, hash, hLen);
        *encLen = hLen;
    }

    return ret;
}

/* Sign the message hash using hash algorithm and RSA key.
 *
 * @param [in]  hashAlg   Hash algorithm OID.
 * @param [in]  hash      Hash of message to encode for signing.
 * @param [in]  hLen      Length of hash of message.
 * @param [out] enc       Encoded message hash.
 * @param [out] encLen    Length of encoded message hash.
 * @param [in]  rsa       RSA key.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_sign(int hashAlg, const unsigned char* hash, unsigned int hLen,
    unsigned char* sigRet, unsigned int* sigLen, WOLFSSL_RSA* rsa)
{
    if (sigLen != NULL) {
        /* No size checking in this API */
        *sigLen = RSA_MAX_SIZE / CHAR_BIT;
    }
    /* flag is 1: output complete signature. */
    return wolfSSL_RSA_sign_generic_padding(hashAlg, hash, hLen, sigRet,
        sigLen, rsa, 1, RSA_PKCS1_PADDING);
}

/* Sign the message hash using hash algorithm and RSA key.
 * wolfSSL API.
 *
 * @param [in]  hashAlg   Hash algorithm NID.
 * @param [in]  hash      Hash of message to encode for signing.
 * @param [in]  hLen      Length of hash of message.
 * @param [out] enc       Encoded message hash.
 * @param [out] encLen    Length of encoded message hash.
 * @param [in]  rsa       RSA key.
 * @param [in]  flag      When 1: Output encrypted signature.
 *                        When 0: Output encoded hash.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_sign_ex(int hashAlg, const unsigned char* hash,
    unsigned int hLen, unsigned char* sigRet, unsigned int* sigLen,
    WOLFSSL_RSA* rsa, int flag)
{
    int ret = 0;

    if ((flag == 0) || (flag == 1)) {
        if (sigLen != NULL) {
            /* No size checking in this API */
            *sigLen = RSA_MAX_SIZE / CHAR_BIT;
        }
        ret = wolfSSL_RSA_sign_generic_padding(hashAlg, hash, hLen, sigRet,
            sigLen, rsa, flag, RSA_PKCS1_PADDING);
    }

    return ret;
}

/**
 * Sign a message hash with the chosen message digest, padding, and RSA key.
 *
 * wolfSSL API.
 *
 * @param [in]      hashAlg  Hash NID
 * @param [in]      hash     Message hash to sign.
 * @param [in]      mLen     Length of message hash to sign.
 * @param [out]     sigRet   Output buffer.
 * @param [in, out] sigLen   On Input: length of sigRet buffer.
 *                           On Output: length of data written to sigRet.
 * @param [in]      rsa      RSA key used to sign the input.
 * @param [in]      flag     1: Output the signature.
 *                           0: Output the value that the unpadded signature
 *                              should be compared to.
 * @param [in]      padding  Padding to use. Only RSA_PKCS1_PSS_PADDING and
 *                           RSA_PKCS1_PADDING are currently supported for
 *                           signing.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_sign_generic_padding(int hashAlg, const unsigned char* hash,
    unsigned int hLen, unsigned char* sigRet, unsigned int* sigLen,
    WOLFSSL_RSA* rsa, int flag, int padding)
{
    int     ret        = 1;
    word32  outLen     = 0;
    int     signSz     = 0;
    WC_RNG* rng        = NULL;
    int     initTmpRng = 0;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng     = NULL;
    byte*   encodedSig = NULL;
#else
    WC_RNG  _tmpRng[1];
    WC_RNG* tmpRng = _tmpRng;
    byte    encodedSig[MAX_ENCODED_SIG_SZ];
#endif
    unsigned int encSz = 0;


    WOLFSSL_ENTER("wolfSSL_RSA_sign_generic_padding");

    if (flag == 0) {
        /* Only encode message. */
        return wolfssl_rsa_sig_encode(hashAlg, hash, hLen, sigRet, sigLen,
            padding);
    }

    /* Validate parameters. */
    if ((hash == NULL) || (sigRet == NULL) || sigLen == NULL || rsa == NULL) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    /* Set wolfCrypt RSA key data from external if not already done. */
    if ((ret == 1) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = 0;
    }

    if (ret == 1) {
        /* Get the maximum signature length. */
        outLen = (word32)wolfSSL_BN_num_bytes(rsa->n);
        /* Check not an error return. */
        if (outLen == 0) {
            WOLFSSL_ERROR_MSG("Bad RSA size");
            ret = 0;
        }
        /* Check signature buffer is big enough. */
        else if (outLen > *sigLen) {
            WOLFSSL_ERROR_MSG("Output buffer too small");
            ret = 0;
        }
    }

#ifdef WOLFSSL_SMALL_STACK
    if (ret == 1) {
        /* Allocate encoded signature buffer if doing PKCS#1 padding. */
        encodedSig = (byte*)XMALLOC(MAX_ENCODED_SIG_SZ, NULL,
            DYNAMIC_TYPE_SIGNATURE);
        if (encodedSig == NULL) {
            ret = 0;
        }
    }
#endif

    if (ret == 1) {
        /* Get/create an RNG. */
        rng = WOLFSSL_RSA_GetRNG(rsa, (WC_RNG**)&tmpRng, &initTmpRng);
        if (rng == NULL) {
            WOLFSSL_ERROR_MSG("WOLFSSL_RSA_GetRNG error");
            ret = 0;
        }
    }

    /* Either encodes with PKCS#1.5 or copies hash into encodedSig. */
    if ((ret == 1) && (wolfssl_rsa_sig_encode(hashAlg, hash, hLen, encodedSig,
            &encSz, padding) == 0)) {
        WOLFSSL_ERROR_MSG("Bad Encode Signature");
        ret = 0;
    }

    if (ret == 1) {
        switch (padding) {
    #if defined(WC_RSA_NO_PADDING) || defined(WC_RSA_DIRECT)
        case RSA_NO_PADDING:
            if ((signSz = wc_RsaDirect(encodedSig, encSz, sigRet, &outLen,
                (RsaKey*)rsa->internal, RSA_PRIVATE_ENCRYPT, rng)) <= 0) {
                WOLFSSL_ERROR_MSG("Bad Rsa Sign no pad");
                ret = 0;
            }
            break;
    #endif
    #if defined(WC_RSA_PSS) && !defined(HAVE_SELFTEST) && \
        (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5,1))
        case RSA_PKCS1_PSS_PADDING:
        {
            enum wc_HashType hType =
                wc_OidGetHash(nid2oid(hashAlg, oidHashType));
        #ifndef WOLFSSL_PSS_SALT_LEN_DISCOVER
            WOLFSSL_MSG("Using RSA-PSS with hash length salt. "
                        "OpenSSL uses max length by default.");
        #endif
            /* Create RSA PSS signature. */
            if ((signSz = wc_RsaPSS_Sign_ex(encodedSig, encSz, sigRet, outLen,
                    hType, wc_hash2mgf(hType), DEF_PSS_SALT_LEN,
                    (RsaKey*)rsa->internal, rng)) <= 0) {
                WOLFSSL_ERROR_MSG("Bad Rsa Sign");
                ret = 0;
            }
            break;
        }
    #endif
    #ifndef WC_NO_RSA_OAEP
        case RSA_PKCS1_OAEP_PADDING:
            /* Not a signature padding scheme. */
            WOLFSSL_ERROR_MSG("RSA_PKCS1_OAEP_PADDING not supported for "
                              "signing");
            ret = 0;
            break;
    #endif
        case RSA_PKCS1_PADDING:
        {
            /* Sign (private encrypt) PKCS#1 encoded signature. */
            if ((signSz = wc_RsaSSL_Sign(encodedSig, encSz, sigRet, outLen,
                    (RsaKey*)rsa->internal, rng)) <= 0) {
                WOLFSSL_ERROR_MSG("Bad Rsa Sign");
                ret = 0;
            }
            break;
        }
        default:
            WOLFSSL_ERROR_MSG("Unsupported padding");
            ret = 0;
            break;
        }
    }

    if (ret == 1) {
        /* Return the size of signature generated. */
        *sigLen = (unsigned int)signSz;
    }

    /* Finalize RNG if initialized in WOLFSSL_RSA_GetRNG(). */
    if (initTmpRng) {
        wc_FreeRng(tmpRng);
    }
#ifdef WOLFSSL_SMALL_STACK
    /* Dispose of any allocated RNG and encoded signature. */
    XFREE(tmpRng,     NULL, DYNAMIC_TYPE_RNG);
    XFREE(encodedSig, NULL, DYNAMIC_TYPE_SIGNATURE);
#endif

    WOLFSSL_LEAVE("wolfSSL_RSA_sign_generic_padding", ret);
    return ret;
}

/**
 * Verify a message hash with the chosen message digest, padding, and RSA key.
 *
 * @param [in]  hashAlg  Hash NID
 * @param [in]  hash     Message hash.
 * @param [in]  mLen     Length of message hash.
 * @param [in]  sigRet   Signature data.
 * @param [in]  sigLen   Length of signature data.
 * @param [in]  rsa      RSA key used to sign the input
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_verify(int hashAlg, const unsigned char* hash,
    unsigned int hLen, const unsigned char* sig, unsigned int sigLen,
    WOLFSSL_RSA* rsa)
{
    return wolfSSL_RSA_verify_ex(hashAlg, hash, hLen, sig, sigLen, rsa,
        RSA_PKCS1_PADDING);
}

/**
 * Verify a message hash with the chosen message digest, padding, and RSA key.
 *
 * wolfSSL API.
 *
 * @param [in]  hashAlg  Hash NID
 * @param [in]  hash     Message hash.
 * @param [in]  mLen     Length of message hash.
 * @param [in]  sigRet   Signature data.
 * @param [in]  sigLen   Length of signature data.
 * @param [in]  rsa      RSA key used to sign the input
 * @param [in]  padding  Padding to use. Only RSA_PKCS1_PSS_PADDING and
 *                       RSA_PKCS1_PADDING are currently supported for
 *                       signing.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_RSA_verify_ex(int hashAlg, const unsigned char* hash,
    unsigned int hLen, const unsigned char* sig, unsigned int sigLen,
    WOLFSSL_RSA* rsa, int padding)
{
    int              ret    = 1;
#ifdef WOLFSSL_SMALL_STACK
    unsigned char*   encodedSig = NULL;
#else
    unsigned char    encodedSig[MAX_ENCODED_SIG_SZ];
#endif
    unsigned char*   sigDec = NULL;
    unsigned int     len    = MAX_ENCODED_SIG_SZ;
    int              verLen = 0;
#if (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5, 1)) && !defined(HAVE_SELFTEST)
    enum wc_HashType hType = WC_HASH_TYPE_NONE;
#endif

    WOLFSSL_ENTER("wolfSSL_RSA_verify");

    /* Validate parameters. */
    if ((hash == NULL) || (sig == NULL) || (rsa == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    if (ret == 1) {
        /* Allocate memory for decrypted signature. */
        sigDec = (unsigned char *)XMALLOC(sigLen, NULL,
            DYNAMIC_TYPE_TMP_BUFFER);
        if (sigDec == NULL) {
            WOLFSSL_ERROR_MSG("Memory allocation failure");
            ret = 0;
        }
    }
#ifdef WOLFSSL_SMALL_STACK
    if ((ret == 1) && (padding != RSA_PKCS1_PSS_PADDING)) {
        /* Allocate memory for encoded signature. */
        encodedSig = (unsigned char *)XMALLOC(len, NULL,
            DYNAMIC_TYPE_TMP_BUFFER);
        if (encodedSig == NULL) {
            WOLFSSL_ERROR_MSG("Memory allocation failure");
            ret = 0;
        }
    }
#endif
    if ((ret == 1) && (padding != RSA_PKCS1_PSS_PADDING)) {
        /* Make encoded signature to compare with decrypted signature. */
        if (wolfssl_rsa_sig_encode(hashAlg, hash, hLen, encodedSig, &len,
                padding) <= 0) {
            WOLFSSL_ERROR_MSG("Message Digest Error");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Decrypt signature */
    #if (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5, 1)) && \
        !defined(HAVE_SELFTEST)
        hType = wc_OidGetHash(nid2oid(hashAlg, oidHashType));
        if ((verLen = wc_RsaSSL_Verify_ex2(sig, sigLen, (unsigned char *)sigDec,
                sigLen, (RsaKey*)rsa->internal, padding, hType)) <= 0) {
            WOLFSSL_ERROR_MSG("RSA Decrypt error");
            ret = 0;
        }
    #else
        verLen = wc_RsaSSL_Verify(sig, sigLen, (unsigned char *)sigDec, sigLen,
            (RsaKey*)rsa->internal);
        if (verLen < 0) {
            ret = 0;
        }
    #endif
    }
    if (ret == 1) {
    #if defined(WC_RSA_PSS) && !defined(HAVE_SELFTEST) && \
        (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5, 1))
        if (padding == RSA_PKCS1_PSS_PADDING) {
            /* Check PSS padding is valid. */
            if (wc_RsaPSS_CheckPadding_ex(hash, hLen, sigDec, verLen,
                    hType, DEF_PSS_SALT_LEN,
                    mp_count_bits(&((RsaKey*)rsa->internal)->n)) != 0) {
                WOLFSSL_ERROR_MSG("wc_RsaPSS_CheckPadding_ex error");
                ret = 0;
            }
        }
        else
    #endif /* WC_RSA_PSS && !HAVE_SELFTEST && (!HAVE_FIPS ||
            * FIPS_VERSION >= 5.1) */
        /* Compare decrypted signature to encoded signature. */
        if ((int)len != verLen || XMEMCMP(encodedSig, sigDec, verLen) != 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_RSA_verify_ex failed");
            ret = 0;
        }
    }

    /* Dispose of any allocated data. */
#ifdef WOLFSSL_SMALL_STACK
    XFREE(encodedSig, NULL, DYNAMIC_TYPE_TMP_BUFFER);
#endif
    XFREE(sigDec, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    return ret;
}

/*
 * RSA public/private encrypt/decrypt APIs
 */

#if !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA)

/* Encrypt with the RSA public key.
 *
 * Return compliant with OpenSSL.
 *
 * @param [in]  len      Length of data to encrypt.
 * @param [in]  from     Data to encrypt.
 * @param [out] to       Encrypted data.
 * @param [in]  rsa      RSA key.
 * @param [in]  padding  Type of padding to place around plaintext.
 * @return  Size of encrypted data on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_public_encrypt(int len, const unsigned char* from,
    unsigned char* to, WOLFSSL_RSA* rsa, int padding)
{
    int ret = 0;
    int initTmpRng = 0;
    WC_RNG *rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  _tmpRng[1];
    WC_RNG* tmpRng = _tmpRng;
#endif
#if !defined(HAVE_FIPS)
    int  mgf = WC_MGF1NONE;
    enum wc_HashType hash = WC_HASH_TYPE_NONE;
    int pad_type = WC_RSA_NO_PAD;
#endif
    int outLen = 0;

    WOLFSSL_ENTER("RSA_public_encrypt");

    /* Validate parameters. */
    if ((len < 0) || (rsa == NULL) || (rsa->internal == NULL) ||
            (from == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if (ret == 0) {
    #if !defined(HAVE_FIPS)
        /* Convert to wolfCrypt padding, hash and MGF. */
        switch (padding) {
        case RSA_PKCS1_PADDING:
            pad_type = WC_RSA_PKCSV15_PAD;
            break;
        case RSA_PKCS1_OAEP_PADDING:
            pad_type = WC_RSA_OAEP_PAD;
            hash = WC_HASH_TYPE_SHA;
            mgf = WC_MGF1SHA1;
            break;
        case RSA_NO_PADDING:
            pad_type = WC_RSA_NO_PAD;
            break;
        default:
            WOLFSSL_ERROR_MSG("RSA_public_encrypt doesn't support padding "
                              "scheme");
            ret = -1;
        }
    #else
        /* Check for supported padding schemes in FIPS. */
        /* TODO: Do we support more schemes in later versions of FIPS? */
        if (padding != RSA_PKCS1_PADDING) {
            WOLFSSL_ERROR_MSG("RSA_public_encrypt pad type not supported in "
                              "FIPS");
            ret = -1;
        }
    #endif
    }

    /* Set wolfCrypt RSA key data from external if not already done. */
    if ((ret == 0) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = -1;
    }

    if (ret == 0) {
        /* Calculate maximum length of encrypted data. */
        outLen = wolfSSL_RSA_size(rsa);
        if (outLen == 0) {
            WOLFSSL_ERROR_MSG("Bad RSA size");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Get an RNG. */
        rng = WOLFSSL_RSA_GetRNG(rsa, (WC_RNG**)&tmpRng, &initTmpRng);
        if (rng == NULL) {
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Use wolfCrypt to public-encrypt with RSA key. */
    #if !defined(HAVE_FIPS)
        ret = wc_RsaPublicEncrypt_ex(from, len, to, outLen,
            (RsaKey*)rsa->internal, rng, pad_type, hash, mgf, NULL, 0);
    #else
        ret = wc_RsaPublicEncrypt(from, len, to, outLen, (RsaKey*)rsa->internal,
            rng);
    #endif
    }

    /* Finalize RNG if initialized in WOLFSSL_RSA_GetRNG(). */
    if (initTmpRng) {
        wc_FreeRng(tmpRng);
    }
#ifdef WOLFSSL_SMALL_STACK
    /* Dispose of any allocated RNG. */
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    /* wolfCrypt error means return -1. */
    if (ret <= 0) {
        ret = -1;
    }
    WOLFSSL_LEAVE("RSA_public_encrypt", ret);
    return ret;
}

/* Decrypt with the RSA public key.
 *
 * Return compliant with OpenSSL.
 *
 * @param [in]  len      Length of encrypted data.
 * @param [in]  from     Encrypted data.
 * @param [out] to       Decrypted data.
 * @param [in]  rsa      RSA key.
 * @param [in]  padding  Type of padding to around plaintext to remove.
 * @return  Size of decrypted data on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_private_decrypt(int len, const unsigned char* from,
    unsigned char* to, WOLFSSL_RSA* rsa, int padding)
{
    int ret = 0;
#if !defined(HAVE_FIPS)
    int mgf = WC_MGF1NONE;
    enum wc_HashType hash = WC_HASH_TYPE_NONE;
    int pad_type = WC_RSA_NO_PAD;
#endif
    int outLen = 0;

    WOLFSSL_ENTER("RSA_private_decrypt");

    /* Validate parameters. */
    if ((len < 0) || (rsa == NULL) || (rsa->internal == NULL) ||
            (from == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if (ret == 0) {
    #if !defined(HAVE_FIPS)
        switch (padding) {
        case RSA_PKCS1_PADDING:
            pad_type = WC_RSA_PKCSV15_PAD;
            break;
        case RSA_PKCS1_OAEP_PADDING:
            pad_type = WC_RSA_OAEP_PAD;
            hash = WC_HASH_TYPE_SHA;
            mgf = WC_MGF1SHA1;
            break;
        case RSA_NO_PADDING:
            pad_type = WC_RSA_NO_PAD;
            break;
        default:
            WOLFSSL_ERROR_MSG("RSA_private_decrypt unsupported padding");
            ret = -1;
        }
    #else
        /* Check for supported padding schemes in FIPS. */
        /* TODO: Do we support more schemes in later versions of FIPS? */
        if (padding != RSA_PKCS1_PADDING) {
            WOLFSSL_ERROR_MSG("RSA_public_encrypt pad type not supported in "
                              "FIPS");
            ret = -1;
        }
    #endif
    }

    /* Set wolfCrypt RSA key data from external if not already done. */
    if ((ret == 0) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = -1;
    }

    if (ret == 0) {
        /* Calculate maximum length of decrypted data. */
        outLen = wolfSSL_RSA_size(rsa);
        if (outLen == 0) {
            WOLFSSL_ERROR_MSG("Bad RSA size");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Use wolfCrypt to private-decrypt with RSA key.
         * Size of 'to' buffer must be size of RSA key */
    #if !defined(HAVE_FIPS)
        ret = wc_RsaPrivateDecrypt_ex(from, len, to, outLen,
            (RsaKey*)rsa->internal, pad_type, hash, mgf, NULL, 0);
    #else
        ret = wc_RsaPrivateDecrypt(from, len, to, outLen,
            (RsaKey*)rsa->internal);
    #endif
    }

    /* wolfCrypt error means return -1. */
    if (ret <= 0) {
        ret = -1;
    }
    WOLFSSL_LEAVE("RSA_private_decrypt", ret);
    return ret;
}

/* Decrypt with the RSA public key.
 *
 * @param [in]  len      Length of encrypted data.
 * @param [in]  from     Encrypted data.
 * @param [out] to       Decrypted data.
 * @param [in]  rsa      RSA key.
 * @param [in]  padding  Type of padding to around plaintext to remove.
 * @return  Size of decrypted data on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_public_decrypt(int len, const unsigned char* from,
    unsigned char* to, WOLFSSL_RSA* rsa, int padding)
{
    int ret = 0;
#if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
    int pad_type = WC_RSA_NO_PAD;
#endif
    int outLen = 0;

    WOLFSSL_ENTER("RSA_public_decrypt");

    /* Validate parameters. */
    if ((len < 0) || (rsa == NULL) || (rsa->internal == NULL) ||
            (from == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if (ret == 0) {
    #if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
        switch (padding) {
        case RSA_PKCS1_PADDING:
            pad_type = WC_RSA_PKCSV15_PAD;
            break;
        case RSA_NO_PADDING:
            pad_type = WC_RSA_NO_PAD;
            break;
        /* TODO: RSA_X931_PADDING not supported */
        default:
            WOLFSSL_ERROR_MSG("RSA_public_decrypt unsupported padding");
            ret = -1;
        }
    #else
        if (padding != RSA_PKCS1_PADDING) {
            WOLFSSL_ERROR_MSG("RSA_public_decrypt pad type not supported in "
                              "FIPS");
            ret = -1;
        }
    #endif
    }

    /* Set wolfCrypt RSA key data from external if not already done. */
    if ((ret == 0) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = -1;
    }

    if (ret == 0) {
        /* Calculate maximum length of encrypted data. */
        outLen = wolfSSL_RSA_size(rsa);
        if (outLen == 0) {
            WOLFSSL_ERROR_MSG("Bad RSA size");
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Use wolfCrypt to public-decrypt with RSA key. */
    #if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
        /* Size of 'to' buffer must be size of RSA key. */
        ret = wc_RsaSSL_Verify_ex(from, len, to, outLen,
            (RsaKey*)rsa->internal, pad_type);
    #else
        /* For FIPS v1/v2 only PKCSV15 padding is supported */
        ret = wc_RsaSSL_Verify(from, len, to, outLen, (RsaKey*)rsa->internal);
    #endif
    }

    /* wolfCrypt error means return -1. */
    if (ret <= 0) {
        ret = -1;
    }
    WOLFSSL_LEAVE("RSA_public_decrypt", ret);
    return ret;
}

/* Encrypt with the RSA private key.
 *
 * Calls wc_RsaSSL_Sign.
 *
 * @param [in]  len      Length of data to encrypt.
 * @param [in]  from     Data to encrypt.
 * @param [out] to       Encrypted data.
 * @param [in]  rsa      RSA key.
 * @param [in]  padding  Type of padding to place around plaintext.
 * @return  Size of encrypted data on success.
 * @return  -1 on failure.
 */
int wolfSSL_RSA_private_encrypt(int len, const unsigned char* from,
    unsigned char* to, WOLFSSL_RSA* rsa, int padding)
{
    int ret = 0;
    int initTmpRng = 0;
    WC_RNG *rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  _tmpRng[1];
    WC_RNG* tmpRng = _tmpRng;
#endif

    WOLFSSL_ENTER("wolfSSL_RSA_private_encrypt");

    /* Validate parameters. */
    if ((len < 0) || (rsa == NULL) || (rsa->internal == NULL) ||
            (from == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if (ret == 0) {
        switch (padding) {
        case RSA_PKCS1_PADDING:
    #ifdef WC_RSA_NO_PADDING
        case RSA_NO_PADDING:
    #endif
            break;
        /* TODO: RSA_X931_PADDING not supported */
        default:
            WOLFSSL_ERROR_MSG("RSA_private_encrypt unsupported padding");
            ret = -1;
        }
    }

    /* Set wolfCrypt RSA key data from external if not already done. */
    if ((ret == 0) && (!rsa->inSet) && (SetRsaInternal(rsa) != 1)) {
        ret = -1;
    }

    if (ret == 0) {
        /* Get an RNG. */
        rng = WOLFSSL_RSA_GetRNG(rsa, (WC_RNG**)&tmpRng, &initTmpRng);
        if (rng == NULL) {
            ret = -1;
        }
    }

    if (ret == 0) {
        /* Use wolfCrypt to private-encrypt with RSA key.
         * Size of output buffer must be size of RSA key. */
        if (padding == RSA_PKCS1_PADDING) {
            ret = wc_RsaSSL_Sign(from, (word32)len, to, wolfSSL_RSA_size(rsa),
                    (RsaKey*)rsa->internal, rng);
        }
    #ifdef WC_RSA_NO_PADDING
        else if (padding == RSA_NO_PADDING) {
            word32 outLen = wolfSSL_RSA_size(rsa);
            ret = wc_RsaFunction(from, (word32)len, to, &outLen,
                    RSA_PRIVATE_ENCRYPT, (RsaKey*)rsa->internal, rng);
            if (ret == 0)
                ret = (int)outLen;
        }
    #endif
    }

    /* Finalize RNG if initialized in WOLFSSL_RSA_GetRNG(). */
    if (initTmpRng) {
        wc_FreeRng(tmpRng);
    }
#ifdef WOLFSSL_SMALL_STACK
    /* Dispose of any allocated RNG. */
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    /* wolfCrypt error means return -1. */
    if (ret <= 0) {
        ret = -1;
    }
    WOLFSSL_LEAVE("wolfSSL_RSA_private_encrypt", ret);
    return ret;
}
#endif /* !HAVE_USER_RSA && !HAVE_FAST_RSA */

/*
 * RSA misc operation APIs
 */

/* Calculate d mod p-1 and q-1 into BNs.
 *
 * wolfSSL API.
 *
 * @param [in, out] rsa  RSA key.
 * @return 1 on success.
 * @return -1 on failure.
 */
int wolfSSL_RSA_GenAdd(WOLFSSL_RSA* rsa)
{
    int     ret = 1;
    int     err;
    mp_int* t = NULL;
#ifdef WOLFSSL_SMALL_STACK
    mp_int  *tmp = (mp_int *)XMALLOC(sizeof(*tmp), rsa->heap,
                                     DYNAMIC_TYPE_TMP_BUFFER);
    if (tmp == NULL) {
        WOLFSSL_ERROR_MSG("Memory allocation failure");
        return -1;
    }
#else
    mp_int  tmp[1];
#endif

    WOLFSSL_ENTER("wolfSSL_RsaGenAdd");

    /* Validate parameters. */
    if ((rsa == NULL) || (rsa->p == NULL) || (rsa->q == NULL) ||
            (rsa->d == NULL) || (rsa->dmp1 == NULL) || (rsa->dmq1 == NULL)) {
        WOLFSSL_ERROR_MSG("rsa no init error");
        ret = -1;
    }

    if (ret == 1) {
        /* Initialize temp MP integer. */
        if (mp_init(tmp) != MP_OKAY) {
            WOLFSSL_ERROR_MSG("mp_init error");
            ret = -1;
        }
    }

    if (ret == 1) {
        t = tmp;

        /* Sub 1 from p into temp. */
        err = mp_sub_d((mp_int*)rsa->p->internal, 1, tmp);
        if (err != MP_OKAY) {
            WOLFSSL_ERROR_MSG("mp_sub_d error");
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Calculate d mod (p - 1) into dmp1 MP integer of BN. */
        err = mp_mod((mp_int*)rsa->d->internal, tmp,
            (mp_int*)rsa->dmp1->internal);
        if (err != MP_OKAY) {
            WOLFSSL_ERROR_MSG("mp_mod error");
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Sub 1 from q into temp. */
        err = mp_sub_d((mp_int*)rsa->q->internal, 1, tmp);
        if (err != MP_OKAY) {
            WOLFSSL_ERROR_MSG("mp_sub_d error");
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Calculate d mod (q - 1) into dmq1 MP integer of BN. */
        err = mp_mod((mp_int*)rsa->d->internal, tmp,
            (mp_int*)rsa->dmq1->internal);
        if (err != MP_OKAY) {
            WOLFSSL_ERROR_MSG("mp_mod error");
            ret = -1;
        }
    }

    mp_clear(t);

#ifdef WOLFSSL_SMALL_STACK
    XFREE(tmp, rsa->heap, DYNAMIC_TYPE_TMP_BUFFER);
#endif

    return ret;
}

#endif /* !HAVE_USER_RSA */

#ifndef NO_WOLFSSL_STUB
/* Enable blinding for RSA key operations.
 *
 * Blinding is a compile time option in wolfCrypt.
 *
 * @param [in] rsa    RSA key. Unused.
 * @param [in] bnCtx  BN context to use for blinding. Unused.
 * @return 1 always.
 */
int wolfSSL_RSA_blinding_on(WOLFSSL_RSA* rsa, WOLFSSL_BN_CTX* bnCtx)
{
    WOLFSSL_STUB("RSA_blinding_on");
    WOLFSSL_ENTER("wolfSSL_RSA_blinding_on");

    (void)rsa;
    (void)bnCtx;

    return 1;  /* on by default */
}
#endif

#endif /* OPENSSL_EXTRA */

#endif /* !NO_RSA */

/*******************************************************************************
 * END OF RSA API
 ******************************************************************************/


/*******************************************************************************
 * START OF DSA API
 ******************************************************************************/

#ifndef NO_DSA

#if defined(OPENSSL_EXTRA) && defined(XFPRINTF) && !defined(NO_FILESYSTEM) && \
    !defined(NO_STDIO_FILESYSTEM)
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_DSA_print_fp(XFILE fp, WOLFSSL_DSA* dsa, int indent)
{
    int ret = 1;
    int pBits;

    WOLFSSL_ENTER("wolfSSL_DSA_print_fp");

    if (fp == XBADFILE || dsa == NULL) {
        ret = 0;
    }

    if (ret == 1 && dsa->p != NULL) {
        pBits = wolfSSL_BN_num_bits(dsa->p);
        if (pBits == 0) {
            ret = 0;
        }
        else {
            if (XFPRINTF(fp, "%*s", indent, "") < 0)
                ret = 0;
            else if (XFPRINTF(fp, "Private-Key: (%d bit)\n", pBits) < 0)
                ret = 0;
        }
    }
    if (ret == 1 && dsa->priv_key != NULL) {
        ret = pk_bn_field_print_fp(fp, indent, "priv", dsa->priv_key);
    }
    if (ret == 1 && dsa->pub_key != NULL) {
        ret = pk_bn_field_print_fp(fp, indent, "pub", dsa->pub_key);
    }
    if (ret == 1 && dsa->p != NULL) {
        ret = pk_bn_field_print_fp(fp, indent, "P", dsa->p);
    }
    if (ret == 1 && dsa->q != NULL) {
        ret = pk_bn_field_print_fp(fp, indent, "Q", dsa->q);
    }
    if (ret == 1 && dsa->g != NULL) {
        ret = pk_bn_field_print_fp(fp, indent, "G", dsa->g);
    }

    WOLFSSL_LEAVE("wolfSSL_DSA_print_fp", ret);

    return ret;
}
#endif /* OPENSSL_EXTRA && XSNPRINTF && !NO_FILESYSTEM && NO_STDIO_FILESYSTEM */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
static void InitwolfSSL_DSA(WOLFSSL_DSA* dsa)
{
    if (dsa) {
        dsa->p        = NULL;
        dsa->q        = NULL;
        dsa->g        = NULL;
        dsa->pub_key  = NULL;
        dsa->priv_key = NULL;
        dsa->internal = NULL;
        dsa->inSet    = 0;
        dsa->exSet    = 0;
    }
}


WOLFSSL_DSA* wolfSSL_DSA_new(void)
{
    WOLFSSL_DSA* external;
    DsaKey*     key;

    WOLFSSL_MSG("wolfSSL_DSA_new");

    key = (DsaKey*) XMALLOC(sizeof(DsaKey), NULL, DYNAMIC_TYPE_DSA);
    if (key == NULL) {
        WOLFSSL_MSG("wolfSSL_DSA_new malloc DsaKey failure");
        return NULL;
    }

    external = (WOLFSSL_DSA*) XMALLOC(sizeof(WOLFSSL_DSA), NULL,
                                    DYNAMIC_TYPE_DSA);
    if (external == NULL) {
        WOLFSSL_MSG("wolfSSL_DSA_new malloc WOLFSSL_DSA failure");
        XFREE(key, NULL, DYNAMIC_TYPE_DSA);
        return NULL;
    }

    InitwolfSSL_DSA(external);
    if (wc_InitDsaKey(key) != 0) {
        WOLFSSL_MSG("wolfSSL_DSA_new InitDsaKey failure");
        XFREE(key, NULL, DYNAMIC_TYPE_DSA);
        wolfSSL_DSA_free(external);
        return NULL;
    }
    external->internal = key;

    return external;
}


void wolfSSL_DSA_free(WOLFSSL_DSA* dsa)
{
    WOLFSSL_MSG("wolfSSL_DSA_free");

    if (dsa) {
        if (dsa->internal) {
            FreeDsaKey((DsaKey*)dsa->internal);
            XFREE(dsa->internal, NULL, DYNAMIC_TYPE_DSA);
            dsa->internal = NULL;
        }
        wolfSSL_BN_free(dsa->priv_key);
        wolfSSL_BN_free(dsa->pub_key);
        wolfSSL_BN_free(dsa->g);
        wolfSSL_BN_free(dsa->q);
        wolfSSL_BN_free(dsa->p);
        InitwolfSSL_DSA(dsa);  /* set back to NULLs for safety */

        XFREE(dsa, NULL, DYNAMIC_TYPE_DSA);

        /* dsa = NULL, don't try to access or double free it */
    }
}

/* wolfSSL -> OpenSSL */
int SetDsaExternal(WOLFSSL_DSA* dsa)
{
    DsaKey* key;
    WOLFSSL_MSG("Entering SetDsaExternal");

    if (dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("dsa key NULL error");
        return -1;
    }

    key = (DsaKey*)dsa->internal;

    if (SetIndividualExternal(&dsa->p, &key->p) != 1) {
        WOLFSSL_MSG("dsa p key error");
        return -1;
    }

    if (SetIndividualExternal(&dsa->q, &key->q) != 1) {
        WOLFSSL_MSG("dsa q key error");
        return -1;
    }

    if (SetIndividualExternal(&dsa->g, &key->g) != 1) {
        WOLFSSL_MSG("dsa g key error");
        return -1;
    }

    if (SetIndividualExternal(&dsa->pub_key, &key->y) != 1) {
        WOLFSSL_MSG("dsa y key error");
        return -1;
    }

    if (SetIndividualExternal(&dsa->priv_key, &key->x) != 1) {
        WOLFSSL_MSG("dsa x key error");
        return -1;
    }

    dsa->exSet = 1;

    return 1;
}
#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA
/* Openssl -> WolfSSL */
int SetDsaInternal(WOLFSSL_DSA* dsa)
{
    DsaKey* key;
    WOLFSSL_MSG("Entering SetDsaInternal");

    if (dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("dsa key NULL error");
        return -1;
    }

    key = (DsaKey*)dsa->internal;

    if (dsa->p != NULL &&
        SetIndividualInternal(dsa->p, &key->p) != 1) {
        WOLFSSL_MSG("rsa p key error");
        return -1;
    }

    if (dsa->q != NULL &&
        SetIndividualInternal(dsa->q, &key->q) != 1) {
        WOLFSSL_MSG("rsa q key error");
        return -1;
    }

    if (dsa->g != NULL &&
        SetIndividualInternal(dsa->g, &key->g) != 1) {
        WOLFSSL_MSG("rsa g key error");
        return -1;
    }

    if (dsa->pub_key != NULL) {
        if (SetIndividualInternal(dsa->pub_key, &key->y) != 1) {
            WOLFSSL_MSG("rsa pub_key error");
            return -1;
        }

        /* public key */
        key->type = DSA_PUBLIC;
    }

    if (dsa->priv_key != NULL) {
        if (SetIndividualInternal(dsa->priv_key, &key->x) != 1) {
            WOLFSSL_MSG("rsa priv_key error");
            return -1;
        }

        /* private key */
        key->type = DSA_PRIVATE;
    }

    dsa->inSet = 1;

    return 1;
}

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_DSA_generate_key(WOLFSSL_DSA* dsa)
{
    int ret = 0;

    WOLFSSL_ENTER("wolfSSL_DSA_generate_key");

    if (dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("Bad arguments");
        return 0;
    }

    if (dsa->inSet == 0) {
        WOLFSSL_MSG("No DSA internal set, do it");

        if (SetDsaInternal(dsa) != 1) {
            WOLFSSL_MSG("SetDsaInternal failed");
            return ret;
        }
    }

#ifdef WOLFSSL_KEY_GEN
    {
        int initTmpRng = 0;
        WC_RNG *rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
        WC_RNG *tmpRng;
#else
        WC_RNG tmpRng[1];
#endif

#ifdef WOLFSSL_SMALL_STACK
        tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
        if (tmpRng == NULL)
            return -1;
#endif
        if (wc_InitRng(tmpRng) == 0) {
            rng = tmpRng;
            initTmpRng = 1;
        }
        else {
            WOLFSSL_MSG("Bad RNG Init, trying global");
            rng = wolfssl_get_global_rng();
        }

        if (rng) {
            /* These were allocated above by SetDsaInternal(). They should
             * be cleared before wc_MakeDsaKey() which reinitializes
             * x and y. */
            mp_clear(&((DsaKey*)dsa->internal)->x);
            mp_clear(&((DsaKey*)dsa->internal)->y);

            if (wc_MakeDsaKey(rng, (DsaKey*)dsa->internal) != MP_OKAY)
                WOLFSSL_MSG("wc_MakeDsaKey failed");
            else if (SetDsaExternal(dsa) != 1)
                WOLFSSL_MSG("SetDsaExternal failed");
            else
                ret = 1;
        }

        if (initTmpRng)
            wc_FreeRng(tmpRng);

#ifdef WOLFSSL_SMALL_STACK
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif
    }
#else /* WOLFSSL_KEY_GEN */
    WOLFSSL_MSG("No Key Gen built in");
#endif
    return ret;
}


/* Returns a pointer to a new WOLFSSL_DSA structure on success and NULL on fail
 */
WOLFSSL_DSA* wolfSSL_DSA_generate_parameters(int bits, unsigned char* seed,
        int seedLen, int* counterRet, unsigned long* hRet,
        WOLFSSL_BN_CB cb, void* CBArg)
{
    WOLFSSL_DSA* dsa;

    WOLFSSL_ENTER("wolfSSL_DSA_generate_parameters()");

    (void)cb;
    (void)CBArg;
    dsa = wolfSSL_DSA_new();
    if (dsa == NULL) {
        return NULL;
    }

    if (wolfSSL_DSA_generate_parameters_ex(dsa, bits, seed, seedLen,
                                  counterRet, hRet, NULL) != 1) {
        wolfSSL_DSA_free(dsa);
        return NULL;
    }

    return dsa;
}


/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_DSA_generate_parameters_ex(WOLFSSL_DSA* dsa, int bits,
                                       unsigned char* seed, int seedLen,
                                       int* counterRet,
                                       unsigned long* hRet, void* cb)
{
    int ret = 0;

    (void)bits;
    (void)seed;
    (void)seedLen;
    (void)counterRet;
    (void)hRet;
    (void)cb;

    WOLFSSL_ENTER("wolfSSL_DSA_generate_parameters_ex");

    if (dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("Bad arguments");
        return 0;
    }

#ifdef WOLFSSL_KEY_GEN
    {
        int initTmpRng = 0;
        WC_RNG *rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
        WC_RNG *tmpRng;
#else
        WC_RNG tmpRng[1];
#endif

#ifdef WOLFSSL_SMALL_STACK
        tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
        if (tmpRng == NULL)
            return -1;
#endif
        if (wc_InitRng(tmpRng) == 0) {
            rng = tmpRng;
            initTmpRng = 1;
        }
        else {
            WOLFSSL_MSG("Bad RNG Init, trying global");
            rng = wolfssl_get_global_rng();
        }

        if (rng) {
            if (wc_MakeDsaParameters(rng, bits,
                                     (DsaKey*)dsa->internal) != MP_OKAY)
                WOLFSSL_MSG("wc_MakeDsaParameters failed");
            else if (SetDsaExternal(dsa) != 1)
                WOLFSSL_MSG("SetDsaExternal failed");
            else
                ret = 1;
        }

        if (initTmpRng)
            wc_FreeRng(tmpRng);

#ifdef WOLFSSL_SMALL_STACK
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif
    }
#else /* WOLFSSL_KEY_GEN */
    WOLFSSL_MSG("No Key Gen built in");
#endif

    return ret;
}

void wolfSSL_DSA_get0_pqg(const WOLFSSL_DSA *d, const WOLFSSL_BIGNUM **p,
        const WOLFSSL_BIGNUM **q, const WOLFSSL_BIGNUM **g)
{
    WOLFSSL_ENTER("wolfSSL_DSA_get0_pqg");
    if (d != NULL) {
        if (p != NULL)
            *p = d->p;
        if (q != NULL)
            *q = d->q;
        if (g != NULL)
            *g = d->g;
    }
}

int wolfSSL_DSA_set0_pqg(WOLFSSL_DSA *d, WOLFSSL_BIGNUM *p,
        WOLFSSL_BIGNUM *q, WOLFSSL_BIGNUM *g)
{
    WOLFSSL_ENTER("wolfSSL_DSA_set0_pqg");
    if (d == NULL || p == NULL || q == NULL || g == NULL) {
        WOLFSSL_MSG("Bad parameter");
        return 0;
    }
    wolfSSL_BN_free(d->p);
    wolfSSL_BN_free(d->q);
    wolfSSL_BN_free(d->g);
    d->p = p;
    d->q = q;
    d->g = g;
    return 1;
}

void wolfSSL_DSA_get0_key(const WOLFSSL_DSA *d,
        const WOLFSSL_BIGNUM **pub_key, const WOLFSSL_BIGNUM **priv_key)
{
    WOLFSSL_ENTER("wolfSSL_DSA_get0_key");
    if (d != NULL) {
        if (pub_key != NULL)
            *pub_key = d->pub_key;
        if (priv_key != NULL)
            *priv_key = d->priv_key;
    }
}

int wolfSSL_DSA_set0_key(WOLFSSL_DSA *d, WOLFSSL_BIGNUM *pub_key,
        WOLFSSL_BIGNUM *priv_key)
{
    WOLFSSL_ENTER("wolfSSL_DSA_set0_key");

    /* The private key may be NULL */
    if (pub_key == NULL) {
        WOLFSSL_MSG("Bad parameter");
        return 0;
    }

    wolfSSL_BN_free(d->pub_key);
    wolfSSL_BN_free(d->priv_key);
    d->pub_key = pub_key;
    d->priv_key = priv_key;

    return 1;
}

WOLFSSL_DSA_SIG* wolfSSL_DSA_SIG_new(void)
{
    WOLFSSL_DSA_SIG* sig;
    WOLFSSL_ENTER("wolfSSL_DSA_SIG_new");
    sig = (WOLFSSL_DSA_SIG*)XMALLOC(sizeof(WOLFSSL_DSA_SIG), NULL,
        DYNAMIC_TYPE_OPENSSL);
    if (sig)
        XMEMSET(sig, 0, sizeof(WOLFSSL_DSA_SIG));
    return sig;
}

void wolfSSL_DSA_SIG_free(WOLFSSL_DSA_SIG *sig)
{
    WOLFSSL_ENTER("wolfSSL_DSA_SIG_free");
    if (sig) {
        if (sig->r) {
            wolfSSL_BN_free(sig->r);
        }
        if (sig->s) {
            wolfSSL_BN_free(sig->s);
        }
        XFREE(sig, NULL, DYNAMIC_TYPE_OPENSSL);
    }
}

void wolfSSL_DSA_SIG_get0(const WOLFSSL_DSA_SIG *sig,
        const WOLFSSL_BIGNUM **r, const WOLFSSL_BIGNUM **s)
{
    WOLFSSL_ENTER("wolfSSL_DSA_SIG_get0");
    if (sig != NULL) {
        *r = sig->r;
        *s = sig->s;
    }
}

int wolfSSL_DSA_SIG_set0(WOLFSSL_DSA_SIG *sig, WOLFSSL_BIGNUM *r,
        WOLFSSL_BIGNUM *s)
{
    WOLFSSL_ENTER("wolfSSL_DSA_SIG_set0");
    if (r == NULL || s == NULL) {
        WOLFSSL_MSG("Bad parameter");
        return 0;
    }

    wolfSSL_BN_clear_free(sig->r);
    wolfSSL_BN_clear_free(sig->s);
    sig->r = r;
    sig->s = s;

    return 1;
}

#ifndef HAVE_SELFTEST
/**
 *
 * @param sig The input signature to encode
 * @param out The output buffer. If *out is NULL then a new buffer is
 *            allocated. Otherwise the output is written to the buffer.
 * @return length on success and -1 on error
 */
int wolfSSL_i2d_DSA_SIG(const WOLFSSL_DSA_SIG *sig, byte **out)
{
    /* Space for sequence + two asn ints */
    byte buf[MAX_SEQ_SZ + 2*(ASN_TAG_SZ + MAX_LENGTH_SZ + DSA_MAX_HALF_SIZE)];
    word32 bufLen = sizeof(buf);

    WOLFSSL_ENTER("wolfSSL_i2d_DSA_SIG");

    if (sig == NULL || sig->r == NULL || sig->s == NULL ||
            out == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }

    if (StoreECC_DSA_Sig(buf, &bufLen,
            (mp_int*)sig->r->internal, (mp_int*)sig->s->internal) != 0) {
        WOLFSSL_MSG("StoreECC_DSA_Sig error");
        return -1;
    }

    if (*out == NULL) {
        byte* tmp = (byte*)XMALLOC(bufLen, NULL, DYNAMIC_TYPE_ASN1);
        if (tmp == NULL) {
            WOLFSSL_MSG("malloc error");
            return -1;
        }
        *out = tmp;
    }

   XMEMCPY(*out, buf, bufLen);

    return (int)bufLen;
}

/**
 * Same as wolfSSL_DSA_SIG_new but also initializes the internal bignums as well.
 * @return New WOLFSSL_DSA_SIG with r and s created as well
 */
static WOLFSSL_DSA_SIG* wolfSSL_DSA_SIG_new_bn(void)
{
    WOLFSSL_DSA_SIG* ret;

    if ((ret = wolfSSL_DSA_SIG_new()) == NULL) {
        WOLFSSL_MSG("wolfSSL_DSA_SIG_new error");
        return NULL;
    }

    if ((ret->r = wolfSSL_BN_new()) == NULL) {
        WOLFSSL_MSG("wolfSSL_BN_new error");
        wolfSSL_DSA_SIG_free(ret);
        return NULL;
    }

    if ((ret->s = wolfSSL_BN_new()) == NULL) {
        WOLFSSL_MSG("wolfSSL_BN_new error");
        wolfSSL_DSA_SIG_free(ret);
        return NULL;
    }

    return ret;
}

/**
 * This parses a DER encoded ASN.1 structure. The ASN.1 encoding is:
 * ASN1_SEQUENCE
 *   ASN1_INTEGER (DSA r)
 *   ASN1_INTEGER (DSA s)
 * Alternatively, if the input is DSA_160_SIG_SIZE or DSA_256_SIG_SIZE in
 * length then this API interprets this as two unsigned binary numbers.
 * @param sig    If non-null then free'd first and then newly created
 *               WOLFSSL_DSA_SIG is assigned
 * @param pp     Input buffer that is moved forward on success
 * @param length Length of input buffer
 * @return Newly created WOLFSSL_DSA_SIG on success or NULL on failure
 */
WOLFSSL_DSA_SIG* wolfSSL_d2i_DSA_SIG(WOLFSSL_DSA_SIG **sig,
        const unsigned char **pp, long length)
{
    WOLFSSL_DSA_SIG* ret;
    mp_int* r;
    mp_int* s;

    WOLFSSL_ENTER("wolfSSL_d2i_DSA_SIG");

    if (pp == NULL || *pp == NULL || length < 0) {
        WOLFSSL_MSG("Bad function arguments");
        return NULL;
    }

    if ((ret = wolfSSL_DSA_SIG_new_bn()) == NULL) {
        WOLFSSL_MSG("wolfSSL_DSA_SIG_new_bn error");
        return NULL;
    }

    r = (mp_int*)ret->r->internal;
    s = (mp_int*)ret->s->internal;

    if (DecodeECC_DSA_Sig(*pp, (word32)length, r, s) != 0) {
        if (length == DSA_160_SIG_SIZE || length == DSA_256_SIG_SIZE) {
            /* Two raw numbers of length/2 size each */
            if (mp_read_unsigned_bin(r, *pp, (int)length/2) != 0) {
                WOLFSSL_MSG("r mp_read_unsigned_bin error");
                wolfSSL_DSA_SIG_free(ret);
                return NULL;
            }

            if (mp_read_unsigned_bin(s, *pp + (length/2), (int)length/2) != 0) {
                WOLFSSL_MSG("s mp_read_unsigned_bin error");
                wolfSSL_DSA_SIG_free(ret);
                return NULL;
            }

            *pp += length;
        }
        else {
            WOLFSSL_MSG("DecodeECC_DSA_Sig error");
            wolfSSL_DSA_SIG_free(ret);
            return NULL;
        }
    }
    else {
        /* DecodeECC_DSA_Sig success move pointer forward */
#ifndef NO_STRICT_ECDSA_LEN
        *pp += length;
#else
        {
            /* We need to figure out how much to move by ourselves */
            word32 idx = 0;
            int len = 0;
            if (GetSequence(*pp, &idx, &len, (word32)length) < 0) {
                WOLFSSL_MSG("GetSequence error");
                wolfSSL_DSA_SIG_free(ret);
                return NULL;
            }
            *pp += len;
        }
#endif
    }

    if (sig != NULL) {
        if (*sig != NULL)
            wolfSSL_DSA_SIG_free(*sig);
        *sig = ret;
    }

    return ret;
}
#endif /* HAVE_SELFTEST */

/* return 1 on success, < 0 otherwise */
int wolfSSL_DSA_do_sign(const unsigned char* d, unsigned char* sigRet,
                       WOLFSSL_DSA* dsa)
{
    int     ret = -1;
    int     initTmpRng = 0;
    WC_RNG* rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif

    WOLFSSL_ENTER("wolfSSL_DSA_do_sign");

    if (d == NULL || sigRet == NULL || dsa == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return ret;
    }

    if (dsa->inSet == 0) {
        WOLFSSL_MSG("No DSA internal set, do it");
        if (SetDsaInternal(dsa) != 1) {
            WOLFSSL_MSG("SetDsaInternal failed");
            return ret;
        }
    }

#ifdef WOLFSSL_SMALL_STACK
    tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
    if (tmpRng == NULL)
        return -1;
#endif

    if (wc_InitRng(tmpRng) == 0) {
        rng = tmpRng;
        initTmpRng = 1;
    }
    else {
        WOLFSSL_MSG("Bad RNG Init, trying global");
        rng = wolfssl_get_global_rng();
    }

    if (rng) {
        if (wc_DsaSign(d, sigRet, (DsaKey*)dsa->internal, rng) < 0)
            WOLFSSL_MSG("DsaSign failed");
        else
            ret = 1;
    }

    if (initTmpRng)
        wc_FreeRng(tmpRng);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    return ret;
}

#ifndef HAVE_SELFTEST
WOLFSSL_DSA_SIG* wolfSSL_DSA_do_sign_ex(const unsigned char* digest,
                                        int inLen, WOLFSSL_DSA* dsa)
{
    byte sigBin[DSA_MAX_SIG_SIZE];
    const byte *tmp = sigBin;
    int sigLen;

    WOLFSSL_ENTER("wolfSSL_DSA_do_sign_ex");

    if (!digest || !dsa || inLen != WC_SHA_DIGEST_SIZE) {
        WOLFSSL_MSG("Bad function arguments");
        return NULL;
    }

    if (wolfSSL_DSA_do_sign(digest, sigBin, dsa) != 1) {
        WOLFSSL_MSG("wolfSSL_DSA_do_sign error");
        return NULL;
    }

    if (dsa->internal == NULL) {
        WOLFSSL_MSG("dsa->internal is null");
        return NULL;
    }

    sigLen = mp_unsigned_bin_size(&((DsaKey*)dsa->internal)->q);
    if (sigLen <= 0) {
        WOLFSSL_MSG("mp_unsigned_bin_size error");
        return NULL;
    }

    /* 2 * sigLen for the two points r and s */
    return wolfSSL_d2i_DSA_SIG(NULL, &tmp, 2 * sigLen);
}
#endif /* !HAVE_SELFTEST */

int wolfSSL_DSA_do_verify(const unsigned char* d, unsigned char* sig,
                        WOLFSSL_DSA* dsa, int *dsacheck)
{
    int    ret = -1;

    WOLFSSL_ENTER("wolfSSL_DSA_do_verify");

    if (d == NULL || sig == NULL || dsa == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }
    if (dsa->inSet == 0)
    {
        WOLFSSL_MSG("No DSA internal set, do it");

        if (SetDsaInternal(dsa) != 1) {
            WOLFSSL_MSG("SetDsaInternal failed");
            return -1;
        }
    }

    ret = DsaVerify(d, sig, (DsaKey*)dsa->internal, dsacheck);
    if (ret != 0 || *dsacheck != 1) {
        WOLFSSL_MSG("DsaVerify failed");
        return ret;
    }

    return 1;
}


int wolfSSL_DSA_bits(const WOLFSSL_DSA *d)
{
    if (!d)
        return 0;
    if (!d->exSet && SetDsaExternal((WOLFSSL_DSA*)d) != 1)
        return 0;
    return wolfSSL_BN_num_bits(d->p);
}

#ifndef HAVE_SELFTEST
int wolfSSL_DSA_do_verify_ex(const unsigned char* digest, int digest_len,
                             WOLFSSL_DSA_SIG* sig, WOLFSSL_DSA* dsa)
{
    int dsacheck, sz;
    byte sigBin[DSA_MAX_SIG_SIZE];
    byte* sigBinPtr = sigBin;
    DsaKey* key;
    int qSz;

    WOLFSSL_ENTER("wolfSSL_DSA_do_verify_ex");

    if (!digest || !sig || !dsa || digest_len != WC_SHA_DIGEST_SIZE) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    if (!sig->r || !sig->s) {
        WOLFSSL_MSG("No signature found in DSA_SIG");
        return 0;
    }

    if (dsa->inSet == 0) {
        WOLFSSL_MSG("No DSA internal set, do it");
        if (SetDsaInternal(dsa) != 1) {
            WOLFSSL_MSG("SetDsaInternal failed");
            return 0;
        }
    }

    key = (DsaKey*)dsa->internal;

    if (key == NULL) {
        WOLFSSL_MSG("dsa->internal is null");
        return 0;
    }

    qSz = mp_unsigned_bin_size(&key->q);
    if (qSz < 0 || qSz > DSA_MAX_HALF_SIZE) {
        WOLFSSL_MSG("mp_unsigned_bin_size error");
        return 0;
    }

    /* read r */
    /* front pad with zeros */
    if ((sz = wolfSSL_BN_num_bytes(sig->r)) < 0 || sz > DSA_MAX_HALF_SIZE)
        return 0;
    while (sz++ < qSz)
        *sigBinPtr++ = 0;
    if (wolfSSL_BN_bn2bin(sig->r, sigBinPtr) == -1)
        return 0;

    /* Move to s */
    sigBinPtr = sigBin + qSz;

    /* read s */
    /* front pad with zeros */
    if ((sz = wolfSSL_BN_num_bytes(sig->s)) < 0 || sz > DSA_MAX_HALF_SIZE)
        return 0;
    while (sz++ < qSz)
        *sigBinPtr++ = 0;
    if (wolfSSL_BN_bn2bin(sig->s, sigBinPtr) == -1)
        return 0;

    if ((wolfSSL_DSA_do_verify(digest, sigBin, dsa, &dsacheck)
                                         != 1) || dsacheck != 1) {
        return 0;
    }

    return 1;
}
#endif /* !HAVE_SELFTEST */

WOLFSSL_API int wolfSSL_i2d_DSAparams(const WOLFSSL_DSA* dsa,
    unsigned char** out)
{
    int ret = 0;
    word32 derLen = 0;
    int preAllocated = 1;
    DsaKey* key = NULL;

    WOLFSSL_ENTER("wolfSSL_i2d_DSAparams");

    if (dsa == NULL || dsa->internal == NULL || out == NULL) {
        ret = BAD_FUNC_ARG;
    }

    if (ret == 0) {
        key = (DsaKey*)dsa->internal;
        ret = wc_DsaKeyToParamsDer_ex(key, NULL, &derLen);
        if (ret == LENGTH_ONLY_E) {
            ret = 0;
        }
    }
    if (ret == 0 && *out == NULL) {
        /* If we're allocating out for the caller, we don't increment out just
           past the end of the DER buffer. If out is already allocated, we do.
           (OpenSSL convention) */
        preAllocated = 0;
        *out = (unsigned char*)XMALLOC(derLen, key->heap, DYNAMIC_TYPE_OPENSSL);
        if (*out == NULL) {
            ret = MEMORY_E;
        }
    }
    if (ret == 0) {
        ret = wc_DsaKeyToParamsDer_ex(key, *out, &derLen);
    }
    if (ret >= 0 && preAllocated == 1) {
        *out += derLen;
    }

    if (ret < 0 && preAllocated == 0) {
        XFREE(*out, key ? key->heap : NULL, DYNAMIC_TYPE_OPENSSL);
    }

    WOLFSSL_LEAVE("wolfSSL_i2d_DSAparams", ret);

    return ret;
}

WOLFSSL_DSA* wolfSSL_d2i_DSAparams(WOLFSSL_DSA** dsa, const unsigned char** der,
    long derLen)
{
    WOLFSSL_DSA* ret = NULL;
    int err = 0;
    word32 idx = 0;
    int asnLen;
    DsaKey* internalKey = NULL;

    WOLFSSL_ENTER("wolfSSL_d2i_DSAparams");

    if (der == NULL || *der == NULL || derLen <= 0) {
        err = 1;
    }
    if (err == 0) {
        ret = wolfSSL_DSA_new();
        err = ret == NULL;
    }
    if (err == 0) {
        err = GetSequence(*der, &idx, &asnLen, (word32)derLen) <= 0;
    }
    if (err == 0) {
        internalKey = (DsaKey*)ret->internal;
        err = GetInt(&internalKey->p, *der, &idx, (word32)derLen) != 0;
    }
    if (err == 0) {
        err = GetInt(&internalKey->q, *der, &idx, (word32)derLen) != 0;
    }
    if (err == 0) {
        err = GetInt(&internalKey->g, *der, &idx, (word32)derLen) != 0;
    }
    if (err == 0) {
        err = SetIndividualExternal(&ret->p, &internalKey->p)
                != 1;
    }
    if (err == 0) {
        err = SetIndividualExternal(&ret->q, &internalKey->q)
                != 1;
    }
    if (err == 0) {
        err = SetIndividualExternal(&ret->g, &internalKey->g)
                != 1;
    }
    if (err == 0 && dsa != NULL) {
        *dsa = ret;
    }

    if (err != 0 && ret != NULL) {
        wolfSSL_DSA_free(ret);
        ret = NULL;
    }

    return ret;
}

#if defined(WOLFSSL_KEY_GEN)
#ifndef NO_BIO

/* Takes a DSA Privatekey and writes it out to a WOLFSSL_BIO
 * Returns 1 or 0
 */
int wolfSSL_PEM_write_bio_DSAPrivateKey(WOLFSSL_BIO* bio, WOLFSSL_DSA* dsa,
                                       const EVP_CIPHER* cipher,
                                       unsigned char* passwd, int len,
                                       wc_pem_password_cb* cb, void* arg)
{
    int ret = 0, der_max_len = 0, derSz = 0;
    byte *derBuf;
    WOLFSSL_EVP_PKEY* pkey;

    WOLFSSL_ENTER("wolfSSL_PEM_write_bio_DSAPrivateKey");

    if (bio == NULL || dsa == NULL) {
        WOLFSSL_MSG("Bad Function Arguments");
        return 0;
    }

    pkey = wolfSSL_EVP_PKEY_new_ex(bio->heap);
    if (pkey == NULL) {
        WOLFSSL_MSG("wolfSSL_EVP_PKEY_new_ex failed");
        return 0;
    }

    pkey->type   = EVP_PKEY_DSA;
    pkey->dsa    = dsa;
    pkey->ownDsa = 0;

    /* 4 > size of pub, priv, p, q, g + ASN.1 additional information */
    der_max_len = MAX_DSA_PRIVKEY_SZ;

    derBuf = (byte*)XMALLOC(der_max_len, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    if (derBuf == NULL) {
        WOLFSSL_MSG("Malloc failed");
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    /* convert key to der format */
    derSz = wc_DsaKeyToDer((DsaKey*)dsa->internal, derBuf, der_max_len);
    if (derSz < 0) {
        WOLFSSL_MSG("wc_DsaKeyToDer failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    pkey->pkey.ptr = (char*)XMALLOC(derSz, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    if (pkey->pkey.ptr == NULL) {
        WOLFSSL_MSG("key malloc failed");
        XFREE(derBuf, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    /* add der info to the evp key */
    pkey->pkey_sz = derSz;
    XMEMCPY(pkey->pkey.ptr, derBuf, derSz);
    XFREE(derBuf, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);

    ret = wolfSSL_PEM_write_bio_PrivateKey(bio, pkey, cipher, passwd, len,
                                        cb, arg);
    wolfSSL_EVP_PKEY_free(pkey);

    return ret;
}

#ifndef HAVE_SELFTEST
/* Takes a DSA public key and writes it out to a WOLFSSL_BIO
 * Returns 1 or 0
 */
int wolfSSL_PEM_write_bio_DSA_PUBKEY(WOLFSSL_BIO* bio, WOLFSSL_DSA* dsa)
{
    int ret = 0;
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_ENTER("wolfSSL_PEM_write_bio_DSA_PUBKEY");

    if (bio == NULL || dsa == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    pkey = wolfSSL_EVP_PKEY_new_ex(bio->heap);
    if (pkey == NULL) {
        WOLFSSL_MSG("wolfSSL_EVP_PKEY_new_ex failed");
        return 0;
    }

    pkey->type   = EVP_PKEY_DSA;
    pkey->dsa    = dsa;
    pkey->ownDsa = 0;

    ret = pem_write_bio_pubkey(bio, pkey);
    wolfSSL_EVP_PKEY_free(pkey);
    return ret;
}
#endif /* HAVE_SELFTEST */
#endif /* !NO_BIO */

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_mem_DSAPrivateKey(WOLFSSL_DSA* dsa,
                                        const EVP_CIPHER* cipher,
                                        unsigned char* passwd, int passwdSz,
                                        unsigned char **pem, int *plen)
{
#if defined(WOLFSSL_PEM_TO_DER) || defined(WOLFSSL_DER_TO_PEM)
    byte *derBuf, *tmp, *cipherInfo = NULL;
    int  der_max_len = 0, derSz = 0;
    const int type = DSA_PRIVATEKEY_TYPE;
    const char* header = NULL;
    const char* footer = NULL;

    WOLFSSL_MSG("wolfSSL_PEM_write_mem_DSAPrivateKey");

    if (pem == NULL || plen == NULL || dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    if (wc_PemGetHeaderFooter(type, &header, &footer) != 0)
        return 0;

    if (dsa->inSet == 0) {
        WOLFSSL_MSG("No DSA internal set, do it");

        if (SetDsaInternal(dsa) != 1) {
            WOLFSSL_MSG("SetDsaInternal failed");
            return 0;
        }
    }

    der_max_len = MAX_DSA_PRIVKEY_SZ;

    derBuf = (byte*)XMALLOC(der_max_len, NULL, DYNAMIC_TYPE_DER);
    if (derBuf == NULL) {
        WOLFSSL_MSG("malloc failed");
        return 0;
    }

    /* Key to DER */
    derSz = wc_DsaKeyToDer((DsaKey*)dsa->internal, derBuf, der_max_len);
    if (derSz < 0) {
        WOLFSSL_MSG("wc_DsaKeyToDer failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        return 0;
    }

    /* encrypt DER buffer if required */
    if (passwd != NULL && passwdSz > 0 && cipher != NULL) {
        int ret;

        ret = EncryptDerKey(derBuf, &derSz, cipher,
                            passwd, passwdSz, &cipherInfo, der_max_len);
        if (ret != 1) {
            WOLFSSL_MSG("EncryptDerKey failed");
            XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
            return ret;
        }
        /* tmp buffer with a max size */
        *plen = (derSz * 2) + (int)XSTRLEN(header) + 1 +
            (int)XSTRLEN(footer) + 1 + HEADER_ENCRYPTED_KEY_SIZE;
    }
    else { /* tmp buffer with a max size */
        *plen = (derSz * 2) + (int)XSTRLEN(header) + 1 +
            (int)XSTRLEN(footer) + 1;
    }

    tmp = (byte*)XMALLOC(*plen, NULL, DYNAMIC_TYPE_PEM);
    if (tmp == NULL) {
        WOLFSSL_MSG("malloc failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        if (cipherInfo != NULL)
            XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);
        return 0;
    }

    /* DER to PEM */
    *plen = wc_DerToPemEx(derBuf, derSz, tmp, *plen, cipherInfo, type);
    if (*plen <= 0) {
        WOLFSSL_MSG("wc_DerToPemEx failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        if (cipherInfo != NULL)
            XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);
        return 0;
    }
    XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
    if (cipherInfo != NULL)
        XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);

    *pem = (byte*)XMALLOC((*plen)+1, NULL, DYNAMIC_TYPE_KEY);
    if (*pem == NULL) {
        WOLFSSL_MSG("malloc failed");
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        return 0;
    }
    XMEMSET(*pem, 0, (*plen)+1);

    if (XMEMCPY(*pem, tmp, *plen) == NULL) {
        WOLFSSL_MSG("XMEMCPY failed");
        XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        return 0;
    }
    XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);

    return 1;
#else
    (void)dsa;
    (void)cipher;
    (void)passwd;
    (void)passwdSz;
    (void)pem;
    (void)plen;
    return 0;
#endif /* WOLFSSL_PEM_TO_DER || WOLFSSL_DER_TO_PEM */
}

#ifndef NO_FILESYSTEM
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_DSAPrivateKey(XFILE fp, WOLFSSL_DSA *dsa,
                                    const EVP_CIPHER *enc,
                                    unsigned char *kstr, int klen,
                                    wc_pem_password_cb *cb, void *u)
{
    byte *pem;
    int  plen, ret;

    (void)cb;
    (void)u;

    WOLFSSL_MSG("wolfSSL_PEM_write_DSAPrivateKey");

    if (fp == XBADFILE || dsa == NULL || dsa->internal == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    ret = wolfSSL_PEM_write_mem_DSAPrivateKey(dsa, enc, kstr, klen, &pem,
        &plen);
    if (ret != 1) {
        WOLFSSL_MSG("wolfSSL_PEM_write_mem_DSAPrivateKey failed");
        return 0;
    }

    ret = (int)XFWRITE(pem, plen, 1, fp);
    if (ret != 1) {
        WOLFSSL_MSG("DSA private key file write failed");
        return 0;
    }

    XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
    return 1;
}

#endif /* NO_FILESYSTEM */
#endif /* defined(WOLFSSL_KEY_GEN) */

#ifndef NO_FILESYSTEM
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
#ifndef NO_WOLFSSL_STUB
int wolfSSL_PEM_write_DSA_PUBKEY(XFILE fp, WOLFSSL_DSA *x)
{
    (void)fp;
    (void)x;
    WOLFSSL_STUB("PEM_write_DSA_PUBKEY");
    WOLFSSL_MSG("wolfSSL_PEM_write_DSA_PUBKEY not implemented");

    return 0;
}
#endif
#endif /* NO_FILESYSTEM */

#ifndef NO_BIO

#if (defined(OPENSSL_EXTRA) || defined(OPENSSL_ALL)) && (!defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && defined(WOLFSSL_KEY_GEN))
/* Uses the same format of input as wolfSSL_PEM_read_bio_PrivateKey but expects
 * the results to be an DSA key.
 *
 * bio  structure to read DSA private key from
 * dsa  if not null is then set to the result
 * cb   password callback for reading PEM
 * pass password string
 *
 * returns a pointer to a new WOLFSSL_DSA structure on success and NULL on fail
 */
WOLFSSL_DSA* wolfSSL_PEM_read_bio_DSAPrivateKey(WOLFSSL_BIO* bio,
                                                WOLFSSL_DSA** dsa,
                                                wc_pem_password_cb* cb,
                                                void* pass)
{
    WOLFSSL_EVP_PKEY* pkey = NULL;
    WOLFSSL_DSA* local;
    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_DSAPrivateKey");


    pkey = wolfSSL_PEM_read_bio_PrivateKey(bio, NULL, cb, pass);
    if (pkey == NULL) {
        WOLFSSL_MSG("Error in PEM_read_bio_PrivateKey");
         return NULL;
     }
     /* Since the WOLFSSL_DSA structure is being taken from WOLFSSL_EVP_PKEY the
     * flag indicating that the WOLFSSL_DSA structure is owned should be FALSE
     * to avoid having it free'd */
    pkey->ownDsa = 0;
    local = pkey->dsa;
    if (dsa != NULL) {
        *dsa = local;
    }
     wolfSSL_EVP_PKEY_free(pkey);
    return local;
}

/* Reads an DSA public key from a WOLFSSL_BIO into a WOLFSSL_DSA.
 * Returns 1 or 0
 */
WOLFSSL_DSA *wolfSSL_PEM_read_bio_DSA_PUBKEY(WOLFSSL_BIO* bio,WOLFSSL_DSA** dsa,
                                             wc_pem_password_cb* cb, void* pass)
{
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_DSA* local;
    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_DSA_PUBKEY");

    pkey = wolfSSL_PEM_read_bio_PUBKEY(bio, NULL, cb, pass);
    if (pkey == NULL) {
        WOLFSSL_MSG("wolfSSL_PEM_read_bio_PUBKEY failed");
        return NULL;
    }

    /* Since the WOLFSSL_DSA structure is being taken from WOLFSSL_EVP_PKEY the
     * flag indicating that the WOLFSSL_DSA structure is owned should be FALSE
     * to avoid having it free'd */
    pkey->ownDsa = 0;
    local = pkey->dsa;
    if (dsa != NULL) {
        *dsa = local;
    }

    wolfSSL_EVP_PKEY_free(pkey);
    return local;
}
#endif /* (OPENSSL_EXTRA || OPENSSL_ALL) && (!NO_CERTS &&
          !NO_FILESYSTEM && WOLFSSL_KEY_GEN) */

#endif /* NO_BIO */

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
/* return 1 if success, -1 if error */
int wolfSSL_DSA_LoadDer(WOLFSSL_DSA* dsa, const unsigned char* derBuf, int derSz)
{
    word32 idx = 0;
    int    ret;

    WOLFSSL_ENTER("wolfSSL_DSA_LoadDer");

    if (dsa == NULL || dsa->internal == NULL || derBuf == NULL || derSz <= 0) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }

    ret = DsaPrivateKeyDecode(derBuf, &idx, (DsaKey*)dsa->internal, derSz);
    if (ret < 0) {
        WOLFSSL_MSG("DsaPrivateKeyDecode failed");
        return -1;
    }

    if (SetDsaExternal(dsa) != 1) {
        WOLFSSL_MSG("SetDsaExternal failed");
        return -1;
    }

    dsa->inSet = 1;

    return 1;
}

/* Loads DSA key from DER buffer. opt = DSA_LOAD_PRIVATE or DSA_LOAD_PUBLIC.
    returns 1 on success, or 0 on failure.  */
int wolfSSL_DSA_LoadDer_ex(WOLFSSL_DSA* dsa, const unsigned char* derBuf,
                                                            int derSz, int opt)
{
    word32 idx = 0;
    int    ret;

    WOLFSSL_ENTER("wolfSSL_DSA_LoadDer");

    if (dsa == NULL || dsa->internal == NULL || derBuf == NULL || derSz <= 0) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }

    if (opt == WOLFSSL_DSA_LOAD_PRIVATE) {
        ret = DsaPrivateKeyDecode(derBuf, &idx, (DsaKey*)dsa->internal, derSz);
    }
    else {
        ret = DsaPublicKeyDecode(derBuf, &idx, (DsaKey*)dsa->internal, derSz);
    }

    if (ret < 0 && opt == WOLFSSL_DSA_LOAD_PRIVATE) {
        WOLFSSL_ERROR_VERBOSE(ret);
        WOLFSSL_MSG("DsaPrivateKeyDecode failed");
        return -1;
    }
    else if (ret < 0 && opt == WOLFSSL_DSA_LOAD_PUBLIC) {
        WOLFSSL_ERROR_VERBOSE(ret);
        WOLFSSL_MSG("DsaPublicKeyDecode failed");
        return -1;
    }

    if (SetDsaExternal(dsa) != 1) {
        WOLFSSL_MSG("SetDsaExternal failed");
        return -1;
    }

    dsa->inSet = 1;

    return 1;
}
#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA
#ifndef NO_BIO
WOLFSSL_DSA *wolfSSL_PEM_read_bio_DSAparams(WOLFSSL_BIO *bp, WOLFSSL_DSA **x,
        wc_pem_password_cb *cb, void *u)
{
    WOLFSSL_DSA* dsa;
    DsaKey* key;
    int    length;
    unsigned char*  buf;
    word32 bufSz;
    int ret;
    word32 idx = 0;
    DerBuffer* pDer;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_DSAparams");

    ret = wolfSSL_BIO_get_mem_data(bp, &buf);
    if (ret <= 0) {
        WOLFSSL_LEAVE("wolfSSL_PEM_read_bio_DSAparams", ret);
        return NULL;
    }

    bufSz = (word32)ret;

    if (cb != NULL || u != NULL) {
        /*
         * cb is for a call back when encountering encrypted PEM files
         * if cb == NULL and u != NULL then u = null terminated password string
         */
        WOLFSSL_MSG("Not yet supporting call back or password for encrypted PEM");
    }

    if (PemToDer(buf, (long)bufSz, DSA_PARAM_TYPE, &pDer, NULL, NULL,
                    NULL) < 0 ) {
        WOLFSSL_MSG("Issue converting from PEM to DER");
        return NULL;
    }

    if (GetSequence(pDer->buffer, &idx, &length, pDer->length) < 0) {
        WOLFSSL_LEAVE("wolfSSL_PEM_read_bio_DSAparams", ret);
        FreeDer(&pDer);
        return NULL;
    }

    dsa = wolfSSL_DSA_new();
    if (dsa == NULL) {
        FreeDer(&pDer);
        WOLFSSL_MSG("Error creating DSA struct");
        return NULL;
    }

    key = (DsaKey*)dsa->internal;
    if (key == NULL) {
        FreeDer(&pDer);
        wolfSSL_DSA_free(dsa);
        WOLFSSL_MSG("Error finding DSA key struct");
        return NULL;
    }

    if (GetInt(&key->p,  pDer->buffer, &idx, pDer->length) < 0 ||
        GetInt(&key->q,  pDer->buffer, &idx, pDer->length) < 0 ||
        GetInt(&key->g,  pDer->buffer, &idx, pDer->length) < 0 ) {
        WOLFSSL_MSG("dsa key error");
        FreeDer(&pDer);
        wolfSSL_DSA_free(dsa);
        return NULL;
    }

    if (SetIndividualExternal(&dsa->p, &key->p) != 1) {
        WOLFSSL_MSG("dsa p key error");
        FreeDer(&pDer);
        wolfSSL_DSA_free(dsa);
        return NULL;
    }

    if (SetIndividualExternal(&dsa->q, &key->q) != 1) {
        WOLFSSL_MSG("dsa q key error");
        FreeDer(&pDer);
        wolfSSL_DSA_free(dsa);
        return NULL;
    }

    if (SetIndividualExternal(&dsa->g, &key->g) != 1) {
        WOLFSSL_MSG("dsa g key error");
        FreeDer(&pDer);
        wolfSSL_DSA_free(dsa);
        return NULL;
    }

    if (x != NULL) {
        *x = dsa;
    }

    FreeDer(&pDer);
    return dsa;
}
#endif /* !NO_BIO */

#if !defined(NO_DH)
WOLFSSL_DH *wolfSSL_DSA_dup_DH(const WOLFSSL_DSA *dsa)
{
    WOLFSSL_DH* dh;
    DhKey*      key;

    WOLFSSL_ENTER("wolfSSL_DSA_dup_DH");

    if (dsa == NULL) {
        return NULL;
    }

    dh = wolfSSL_DH_new();
    if (dh == NULL) {
        return NULL;
    }
    key = (DhKey*)dh->internal;

    if (dsa->p != NULL &&
        SetIndividualInternal(((WOLFSSL_DSA*)dsa)->p, &key->p)
                                                           != 1) {
        WOLFSSL_MSG("rsa p key error");
        wolfSSL_DH_free(dh);
        return NULL;
    }
    if (dsa->g != NULL &&
        SetIndividualInternal(((WOLFSSL_DSA*)dsa)->g, &key->g)
                                                           != 1) {
        WOLFSSL_MSG("rsa g key error");
        wolfSSL_DH_free(dh);
        return NULL;
    }

    if (SetIndividualExternal(&dh->p, &key->p) != 1) {
        WOLFSSL_MSG("dsa p key error");
        wolfSSL_DH_free(dh);
        return NULL;
    }
    if (SetIndividualExternal(&dh->g, &key->g) != 1) {
        WOLFSSL_MSG("dsa g key error");
        wolfSSL_DH_free(dh);
        return NULL;
    }

    return dh;
}
#endif /* !NO_DH */

#endif /* OPENSSL_EXTRA */

#endif /* !NO_DSA */

/*******************************************************************************
 * END OF DSA API
 ******************************************************************************/


/*******************************************************************************
 * START OF DH API
 ******************************************************************************/

#ifndef NO_DH

#ifdef OPENSSL_EXTRA

/*
 * DH constructor/deconstructor APIs
 */

/* Allocate and initialize a new DH key.
 *
 * @return  DH key on success.
 * @return  NULL on failure.
 */
WOLFSSL_DH* wolfSSL_DH_new(void)
{
    int err = 0;
    WOLFSSL_DH* dh = NULL;
    DhKey* key = NULL;

    WOLFSSL_ENTER("wolfSSL_DH_new");

    /* Allocate OpenSSL DH key. */
    dh = (WOLFSSL_DH*)XMALLOC(sizeof(WOLFSSL_DH), NULL, DYNAMIC_TYPE_DH);
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("wolfSSL_DH_new malloc WOLFSSL_DH failure");
        err = 1;
    }

    if (!err) {
        /* Clear key data. */
        XMEMSET(dh, 0, sizeof(WOLFSSL_DH));
        /* Initialize reference counting. */
        wolfSSL_RefInit(&dh->ref, &err);
    }
    if (!err) {
        /* Allocate wolfSSL DH key. */
        key = (DhKey*)XMALLOC(sizeof(DhKey), NULL, DYNAMIC_TYPE_DH);
        if (key == NULL) {
            WOLFSSL_ERROR_MSG("wolfSSL_DH_new malloc DhKey failure");
            err = 1;
        }
    }
    if (!err) {
        /* Set and initialize wolfSSL DH key. */
        dh->internal = key;
        if (wc_InitDhKey(key) != 0) {
            WOLFSSL_ERROR_MSG("wolfSSL_DH_new InitDhKey failure");
            err = 1;
        }
    }

    if (err && (dh != NULL)) {
        /* Dispose of the allocated memory. */
        XFREE(key, NULL, DYNAMIC_TYPE_DH);
        wolfSSL_RefFree(&dh->ref);
        XFREE(dh, NULL, DYNAMIC_TYPE_DH);
        dh = NULL;
    }
    return dh;
}

#if defined(HAVE_PUBLIC_FFDHE) || (defined(HAVE_FIPS) && FIPS_VERSION_EQ(2,0))
/* Set the DH parameters based on the NID.
 *
 * @param [in, out] dh   DH key to set.
 * @param [in]      nid  Numeric ID of predefined DH parameters.
 * @return  0 on success.
 * @return  1 on failure.
 */
static int wolfssl_dh_set_nid(WOLFSSL_DH* dh, int nid)
{
    int err = 0;
    const DhParams* params = NULL;

    /* HAVE_PUBLIC_FFDHE not required to expose wc_Dh_ffdhe* functions in
     * FIPS v2 module */
    switch (nid) {
#ifdef HAVE_FFDHE_2048
    case NID_ffdhe2048:
        params = wc_Dh_ffdhe2048_Get();
        break;
#endif /* HAVE_FFDHE_2048 */
#ifdef HAVE_FFDHE_3072
    case NID_ffdhe3072:
        params = wc_Dh_ffdhe3072_Get();
        break;
#endif /* HAVE_FFDHE_3072 */
#ifdef HAVE_FFDHE_4096
    case NID_ffdhe4096:
        params = wc_Dh_ffdhe4096_Get();
        break;
#endif /* HAVE_FFDHE_4096 */
    default:
        break;
    }
    if (params == NULL) {
        WOLFSSL_ERROR_MSG("Unable to find DH params for nid.");
        err = 1;
    }

    if (!err) {
        /* Set prime from data retrieved. */
        dh->p = wolfSSL_BN_bin2bn(params->p, params->p_len, NULL);
        if (dh->p == NULL) {
            WOLFSSL_ERROR_MSG("Error converting p hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
    if (!err) {
        /* Set generator from data retrieved. */
        dh->g = wolfSSL_BN_bin2bn(params->g, params->g_len, NULL);
        if (dh->g == NULL) {
            WOLFSSL_ERROR_MSG("Error converting g hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
#ifdef HAVE_FFDHE_Q
    if (!err) {
        /* Set order from data retrieved. */
        dh->q = wolfSSL_BN_bin2bn(params->q, params->q_len, NULL);
        if (dh->q == NULL) {
            WOLFSSL_ERROR_MSG("Error converting q hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
#endif

    /* Synchronize the external into internal DH key's parameters. */
    if ((!err) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Failed to set internal DH params.");
        err = 1;
    }
    if (!err) {
        /* External DH key parameters were set. */
        dh->exSet = 1;
    }

    if (err == 1) {
        /* Dispose of any external parameters. */
    #ifdef HAVE_FFDHE_Q
        wolfSSL_BN_free(dh->q);
        dh->q = NULL;
    #endif
        wolfSSL_BN_free(dh->p);
        dh->p = NULL;
        wolfSSL_BN_free(dh->g);
        dh->g = NULL;
    }

    return err;
}
#elif !defined(HAVE_PUBLIC_FFDHE) && (!defined(HAVE_FIPS) || \
      FIPS_VERSION_GT(2,0))
/* Set the DH parameters based on the NID.
 *
 * FIPS v2 and lower doesn't support wc_DhSetNamedKey.
 *
 * @param [in, out] dh   DH key to set.
 * @param [in]      nid  Numeric ID of predefined DH parameters.
 * @return  0 on success.
 * @return  1 on failure.
 */
static int wolfssl_dh_set_nid(WOLFSSL_DH* dh, int nid)
{
    int err = 0;
    int name = 0;
#ifdef HAVE_FFDHE_Q
    int elements = ELEMENT_P | ELEMENT_G | ELEMENT_Q;
#else
    int elements = ELEMENT_P | ELEMENT_G;
#endif /* HAVE_FFDHE_Q */

    switch (nid) {
#ifdef HAVE_FFDHE_2048
    case NID_ffdhe2048:
        name = WC_FFDHE_2048;
        break;
#endif /* HAVE_FFDHE_2048 */
#ifdef HAVE_FFDHE_3072
    case NID_ffdhe3072:
        name = WC_FFDHE_3072;
        break;
#endif /* HAVE_FFDHE_3072 */
#ifdef HAVE_FFDHE_4096
    case NID_ffdhe4096:
        name = WC_FFDHE_4096;
        break;
#endif /* HAVE_FFDHE_4096 */
    default:
        err = 1;
        WOLFSSL_ERROR_MSG("Unable to find DH params for nid.");
        break;
    }
    /* Set the internal DH key's parameters based on name. */
    if ((!err) && (wc_DhSetNamedKey((DhKey*)dh->internal, name) != 0)) {
        WOLFSSL_ERROR_MSG("wc_DhSetNamedKey failed.");
        err = 1;
    }
    /* Synchronize the internal into external DH key's parameters. */
    if (!err && (SetDhExternal_ex(dh, elements) != 1)) {
        WOLFSSL_ERROR_MSG("Failed to set external DH params.");
        err = 1;
    }

    return err;
}
#else
/* Set the DH parameters based on the NID.
 *
 * Pre-defined DH parameters not available.
 *
 * @param [in, out] dh   DH key to set.
 * @param [in]      nid  Numeric ID of predefined DH parameters.
 * @return  1 for failure.
 */
static int wolfssl_dh_set_nid(WOLFSSL_DH* dh, int nid)
{
    return 1;
}
#endif

/* Allocate and initialize a new DH key with the parameters based on the NID.
 *
 * @param [in] nid  Numeric ID of DH parameters.
 *
 * @return  DH key on success.
 * @return  NULL on failure.
 */
WOLFSSL_DH* wolfSSL_DH_new_by_nid(int nid)
{
    WOLFSSL_DH* dh = NULL;
    int err = 0;

    WOLFSSL_ENTER("wolfSSL_DH_new_by_nid");

    /* Allocate a new DH key. */
    dh = wolfSSL_DH_new();
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Failed to create WOLFSSL_DH.");
        err = 1;
    }
    if (!err) {
        /* Set the parameters based on NID. */
        err = wolfssl_dh_set_nid(dh, nid);
    }

    if (err && (dh != NULL)) {
        /* Dispose of the key on failure to set. */
        wolfSSL_DH_free(dh);
        dh = NULL;
    }

    WOLFSSL_LEAVE("wolfSSL_DH_new_by_nid", err);

    return dh;
}

/* Dispose of DH key and allocated data.
 *
 * Cannot use dh after this call.
 *
 * @param [in] dh  DH key to free.
 */
void wolfSSL_DH_free(WOLFSSL_DH* dh)
{
    int doFree = 0;

    WOLFSSL_ENTER("wolfSSL_DH_free");

    if (dh != NULL) {
        int err;

        /* Only free if all references to it are done */
        wolfSSL_RefDec(&dh->ref, &doFree, &err);
        /* Ignore errors - doFree will be 0 on error. */
        (void)err;
    }
    if (doFree) {
        /* Dispose of allocated reference counting data. */
        wolfSSL_RefFree(&dh->ref);

        /* Dispose of wolfSSL DH key. */
        if (dh->internal) {
            wc_FreeDhKey((DhKey*)dh->internal);
            XFREE(dh->internal, NULL, DYNAMIC_TYPE_DH);
            dh->internal = NULL;
        }

        /* Dispose of any allocated BNs. */
        wolfSSL_BN_free(dh->priv_key);
        wolfSSL_BN_free(dh->pub_key);
        wolfSSL_BN_free(dh->g);
        wolfSSL_BN_free(dh->p);
        wolfSSL_BN_free(dh->q);
        /* Set back to NULLs for safety. */
        XMEMSET(dh, 0, sizeof(WOLFSSL_DH));

        XFREE(dh, NULL, DYNAMIC_TYPE_DH);
    }
}

/* Increments ref count of DH key.
 *
 * @param [in, out] dh  DH key.
 * @return  1 on success
 * @return  0 on error
 */
int wolfSSL_DH_up_ref(WOLFSSL_DH* dh)
{
    int err = 1;

    WOLFSSL_ENTER("wolfSSL_DH_up_ref");

    if (dh != NULL) {
        wolfSSL_RefInc(&dh->ref, &err);
    }

    return !err;
}

#if defined(WOLFSSL_QT) || defined(OPENSSL_ALL) || defined(WOLFSSL_OPENSSH) || \
    defined(OPENSSL_EXTRA)

#ifdef WOLFSSL_DH_EXTRA
/* Duplicate the DH key.
 *
 * Internal DH key in 'dh' is updated if necessary.
 *
 * @param [in, out] dh  DH key to duplicate.
 * @return  NULL on failure.
 * @return  DH key on success.
 */
WOLFSSL_DH* wolfSSL_DH_dup(WOLFSSL_DH* dh)
{
    WOLFSSL_DH* ret = NULL;
    int err = 0;

    WOLFSSL_ENTER("wolfSSL_DH_dup");

    /* Validate parameters. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad parameter");
        err = 1;
    }

    /* Ensure internal DH key is set. */
    if ((!err) && (dh->inSet == 0) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Bad DH set internal");
        err = 1;
    }

    /* Create a new DH key object. */
    if ((!err) && (!(ret = wolfSSL_DH_new()))) {
        WOLFSSL_ERROR_MSG("wolfSSL_DH_new error");
        err = 1;
    }
    /* Copy internal DH key from original to new. */
    if ((!err) && (wc_DhKeyCopy((DhKey*)dh->internal, (DhKey*)ret->internal) !=
            MP_OKAY)) {
        WOLFSSL_ERROR_MSG("wc_DhKeyCopy error");
        err = 1;
    }
    if (!err) {
        ret->inSet = 1;

         /* Synchronize the internal into external DH key's parameters. */
        if (SetDhExternal(ret) != 1) {
            WOLFSSL_ERROR_MSG("SetDhExternal error");
            err = 1;
        }
    }

    /* Dispose of any allocated DH key on error. */
    if (err && (ret != NULL)) {
        wolfSSL_DH_free(ret);
        ret = NULL;
    }
    return ret;
}
#endif /* WOLFSSL_DH_EXTRA */

#endif

/* Allocate and initialize a new DH key with 2048-bit parameters.
 *
 * See RFC 5114 section 2.3, "2048-bit MODP Group with 256-bit Prime Order
 * Subgroup."
 *
 * @return  NULL on failure.
 * @return  DH Key on success.
 */
WOLFSSL_DH* wolfSSL_DH_get_2048_256(void)
{
    WOLFSSL_DH* dh;
    int err = 0;
    static const byte pHex[] = {
        0x87, 0xA8, 0xE6, 0x1D, 0xB4, 0xB6, 0x66, 0x3C, 0xFF, 0xBB, 0xD1, 0x9C,
        0x65, 0x19, 0x59, 0x99, 0x8C, 0xEE, 0xF6, 0x08, 0x66, 0x0D, 0xD0, 0xF2,
        0x5D, 0x2C, 0xEE, 0xD4, 0x43, 0x5E, 0x3B, 0x00, 0xE0, 0x0D, 0xF8, 0xF1,
        0xD6, 0x19, 0x57, 0xD4, 0xFA, 0xF7, 0xDF, 0x45, 0x61, 0xB2, 0xAA, 0x30,
        0x16, 0xC3, 0xD9, 0x11, 0x34, 0x09, 0x6F, 0xAA, 0x3B, 0xF4, 0x29, 0x6D,
        0x83, 0x0E, 0x9A, 0x7C, 0x20, 0x9E, 0x0C, 0x64, 0x97, 0x51, 0x7A, 0xBD,
        0x5A, 0x8A, 0x9D, 0x30, 0x6B, 0xCF, 0x67, 0xED, 0x91, 0xF9, 0xE6, 0x72,
        0x5B, 0x47, 0x58, 0xC0, 0x22, 0xE0, 0xB1, 0xEF, 0x42, 0x75, 0xBF, 0x7B,
        0x6C, 0x5B, 0xFC, 0x11, 0xD4, 0x5F, 0x90, 0x88, 0xB9, 0x41, 0xF5, 0x4E,
        0xB1, 0xE5, 0x9B, 0xB8, 0xBC, 0x39, 0xA0, 0xBF, 0x12, 0x30, 0x7F, 0x5C,
        0x4F, 0xDB, 0x70, 0xC5, 0x81, 0xB2, 0x3F, 0x76, 0xB6, 0x3A, 0xCA, 0xE1,
        0xCA, 0xA6, 0xB7, 0x90, 0x2D, 0x52, 0x52, 0x67, 0x35, 0x48, 0x8A, 0x0E,
        0xF1, 0x3C, 0x6D, 0x9A, 0x51, 0xBF, 0xA4, 0xAB, 0x3A, 0xD8, 0x34, 0x77,
        0x96, 0x52, 0x4D, 0x8E, 0xF6, 0xA1, 0x67, 0xB5, 0xA4, 0x18, 0x25, 0xD9,
        0x67, 0xE1, 0x44, 0xE5, 0x14, 0x05, 0x64, 0x25, 0x1C, 0xCA, 0xCB, 0x83,
        0xE6, 0xB4, 0x86, 0xF6, 0xB3, 0xCA, 0x3F, 0x79, 0x71, 0x50, 0x60, 0x26,
        0xC0, 0xB8, 0x57, 0xF6, 0x89, 0x96, 0x28, 0x56, 0xDE, 0xD4, 0x01, 0x0A,
        0xBD, 0x0B, 0xE6, 0x21, 0xC3, 0xA3, 0x96, 0x0A, 0x54, 0xE7, 0x10, 0xC3,
        0x75, 0xF2, 0x63, 0x75, 0xD7, 0x01, 0x41, 0x03, 0xA4, 0xB5, 0x43, 0x30,
        0xC1, 0x98, 0xAF, 0x12, 0x61, 0x16, 0xD2, 0x27, 0x6E, 0x11, 0x71, 0x5F,
        0x69, 0x38, 0x77, 0xFA, 0xD7, 0xEF, 0x09, 0xCA, 0xDB, 0x09, 0x4A, 0xE9,
        0x1E, 0x1A, 0x15, 0x97
    };
    static const byte gHex[] = {
        0x3F, 0xB3, 0x2C, 0x9B, 0x73, 0x13, 0x4D, 0x0B, 0x2E, 0x77, 0x50, 0x66,
        0x60, 0xED, 0xBD, 0x48, 0x4C, 0xA7, 0xB1, 0x8F, 0x21, 0xEF, 0x20, 0x54,
        0x07, 0xF4, 0x79, 0x3A, 0x1A, 0x0B, 0xA1, 0x25, 0x10, 0xDB, 0xC1, 0x50,
        0x77, 0xBE, 0x46, 0x3F, 0xFF, 0x4F, 0xED, 0x4A, 0xAC, 0x0B, 0xB5, 0x55,
        0xBE, 0x3A, 0x6C, 0x1B, 0x0C, 0x6B, 0x47, 0xB1, 0xBC, 0x37, 0x73, 0xBF,
        0x7E, 0x8C, 0x6F, 0x62, 0x90, 0x12, 0x28, 0xF8, 0xC2, 0x8C, 0xBB, 0x18,
        0xA5, 0x5A, 0xE3, 0x13, 0x41, 0x00, 0x0A, 0x65, 0x01, 0x96, 0xF9, 0x31,
        0xC7, 0x7A, 0x57, 0xF2, 0xDD, 0xF4, 0x63, 0xE5, 0xE9, 0xEC, 0x14, 0x4B,
        0x77, 0x7D, 0xE6, 0x2A, 0xAA, 0xB8, 0xA8, 0x62, 0x8A, 0xC3, 0x76, 0xD2,
        0x82, 0xD6, 0xED, 0x38, 0x64, 0xE6, 0x79, 0x82, 0x42, 0x8E, 0xBC, 0x83,
        0x1D, 0x14, 0x34, 0x8F, 0x6F, 0x2F, 0x91, 0x93, 0xB5, 0x04, 0x5A, 0xF2,
        0x76, 0x71, 0x64, 0xE1, 0xDF, 0xC9, 0x67, 0xC1, 0xFB, 0x3F, 0x2E, 0x55,
        0xA4, 0xBD, 0x1B, 0xFF, 0xE8, 0x3B, 0x9C, 0x80, 0xD0, 0x52, 0xB9, 0x85,
        0xD1, 0x82, 0xEA, 0x0A, 0xDB, 0x2A, 0x3B, 0x73, 0x13, 0xD3, 0xFE, 0x14,
        0xC8, 0x48, 0x4B, 0x1E, 0x05, 0x25, 0x88, 0xB9, 0xB7, 0xD2, 0xBB, 0xD2,
        0xDF, 0x01, 0x61, 0x99, 0xEC, 0xD0, 0x6E, 0x15, 0x57, 0xCD, 0x09, 0x15,
        0xB3, 0x35, 0x3B, 0xBB, 0x64, 0xE0, 0xEC, 0x37, 0x7F, 0xD0, 0x28, 0x37,
        0x0D, 0xF9, 0x2B, 0x52, 0xC7, 0x89, 0x14, 0x28, 0xCD, 0xC6, 0x7E, 0xB6,
        0x18, 0x4B, 0x52, 0x3D, 0x1D, 0xB2, 0x46, 0xC3, 0x2F, 0x63, 0x07, 0x84,
        0x90, 0xF0, 0x0E, 0xF8, 0xD6, 0x47, 0xD1, 0x48, 0xD4, 0x79, 0x54, 0x51,
        0x5E, 0x23, 0x27, 0xCF, 0xEF, 0x98, 0xC5, 0x82, 0x66, 0x4B, 0x4C, 0x0F,
        0x6C, 0xC4, 0x16, 0x59
    };
    static const byte qHex[] = {
        0x8C, 0xF8, 0x36, 0x42, 0xA7, 0x09, 0xA0, 0x97, 0xB4, 0x47, 0x99, 0x76,
        0x40, 0x12, 0x9D, 0xA2, 0x99, 0xB1, 0xA4, 0x7D, 0x1E, 0xB3, 0x75, 0x0B,
        0xA3, 0x08, 0xB0, 0xFE, 0x64, 0xF5, 0xFB, 0xD3
    };

    /* Create a new DH key to return. */
    dh = wolfSSL_DH_new();
    if (dh == NULL) {
        err = 1;
    }
    if (!err) {
        /* Set prime. */
        dh->p = wolfSSL_BN_bin2bn(pHex, (int)sizeof(pHex), NULL);
        if (dh->p == NULL) {
            WOLFSSL_ERROR_MSG("Error converting p hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
    if (!err) {
        /* Set generator. */
        dh->g = wolfSSL_BN_bin2bn(gHex, (int)sizeof(gHex), NULL);
        if (dh->g == NULL) {
            WOLFSSL_ERROR_MSG("Error converting g hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
    if (!err) {
        /* Set order. */
        dh->q = wolfSSL_BN_bin2bn(qHex, (int)sizeof(qHex), NULL);
        if (dh->q == NULL) {
            WOLFSSL_ERROR_MSG("Error converting q hex to WOLFSSL_BIGNUM.");
            err = 1;
        }
    }
    /* Set values into wolfSSL DH key. */
    if ((!err) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Error setting DH parameters.");
        err = 1;
    }
    if (!err) {
        /* External DH key parameters were set. */
        dh->exSet = 1;
    }

    /* Dispose of any allocated DH key on error. */
    if (err && (dh != NULL)) {
        wolfSSL_DH_free(dh);
        dh = NULL;
    }

    return dh;
}

/* TODO: consider changing strings to byte arrays. */

/* Returns a big number with the 768-bit prime from RFC 2409.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 768-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_768_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 768
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A63A3620FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_768_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 768 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 1024-bit prime from RFC 2409.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 1024-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_1024_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 1024
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE65381FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_1024_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 1024 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 1536-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 1536-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_1536_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 1536
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA237327FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_1536_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 1536 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 2048-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 2048-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_2048_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 2048
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28F"
        "B5C55DF06F4C52C9DE2BCBF695581718"
        "3995497CEA956AE515D2261898FA0510"
        "15728E5A8AACAA68FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_2048_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 2048 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 3072-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 3072-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_3072_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 3072
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28F"
        "B5C55DF06F4C52C9DE2BCBF695581718"
        "3995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33"
        "A85521ABDF1CBA64ECFB850458DBEF0A"
        "8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619D"
        "CEE3D2261AD2EE6BF12FFA06D98A0864"
        "D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E2"
        "08E24FA074E5AB3143DB5BFCE0FD108E"
        "4B82D120A93AD2CAFFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_3072_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 3072 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 4096-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 4096-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_4096_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 4096
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28F"
        "B5C55DF06F4C52C9DE2BCBF695581718"
        "3995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33"
        "A85521ABDF1CBA64ECFB850458DBEF0A"
        "8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619D"
        "CEE3D2261AD2EE6BF12FFA06D98A0864"
        "D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E2"
        "08E24FA074E5AB3143DB5BFCE0FD108E"
        "4B82D120A92108011A723C12A787E6D7"
        "88719A10BDBA5B2699C327186AF4E23C"
        "1A946834B6150BDA2583E9CA2AD44CE8"
        "DBBBC2DB04DE8EF92E8EFC141FBECAA6"
        "287C59474E6BC05D99B2964FA090C3A2"
        "233BA186515BE7ED1F612970CEE2D7AF"
        "B81BDD762170481CD0069127D5B05AA9"
        "93B4EA988D8FDDC186FFB7DC90A6C08F"
        "4DF435C934063199FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_4096_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 4096 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/* Returns a big number with the 6144-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 6144-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_6144_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 6144
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28F"
        "B5C55DF06F4C52C9DE2BCBF695581718"
        "3995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33"
        "A85521ABDF1CBA64ECFB850458DBEF0A"
        "8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619D"
        "CEE3D2261AD2EE6BF12FFA06D98A0864"
        "D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E2"
        "08E24FA074E5AB3143DB5BFCE0FD108E"
        "4B82D120A92108011A723C12A787E6D7"
        "88719A10BDBA5B2699C327186AF4E23C"
        "1A946834B6150BDA2583E9CA2AD44CE8"
        "DBBBC2DB04DE8EF92E8EFC141FBECAA6"
        "287C59474E6BC05D99B2964FA090C3A2"
        "233BA186515BE7ED1F612970CEE2D7AF"
        "B81BDD762170481CD0069127D5B05AA9"
        "93B4EA988D8FDDC186FFB7DC90A6C08F"
        "4DF435C93402849236C3FAB4D27C7026"
        "C1D4DCB2602646DEC9751E763DBA37BD"
        "F8FF9406AD9E530EE5DB382F413001AE"
        "B06A53ED9027D831179727B0865A8918"
        "DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
        "DB7F1447E6CC254B332051512BD7AF42"
        "6FB8F401378CD2BF5983CA01C64B92EC"
        "F032EA15D1721D03F482D7CE6E74FEF6"
        "D55E702F46980C82B5A84031900B1C9E"
        "59E7C97FBEC7E8F323A97A7E36CC88BE"
        "0F1D45B7FF585AC54BD407B22B4154AA"
        "CC8F6D7EBF48E1D814CC5ED20F8037E0"
        "A79715EEF29BE32806A1D58BB7C5DA76"
        "F550AA3D8A1FBFF0EB19CCB1A313D55C"
        "DA56C9EC2EF29632387FE8D76E3C0468"
        "043E8F663F4860EE12BF2D5B0B7474D6"
        "E694F91E6DCC4024FFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_6144_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 6144 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}


/* Returns a big number with the 8192-bit prime from RFC 3526.
 *
 * @param [in, out] bn  If not NULL then this BN is set and returned.
 *                      If NULL then a new BN is created, set and returned.
 *
 * @return  NULL on failure.
 * @return  WOLFSSL_BIGNUM with value set to 8192-bit prime on success.
 */
WOLFSSL_BIGNUM* wolfSSL_DH_8192_prime(WOLFSSL_BIGNUM* bn)
{
#if WOLFSSL_MAX_BN_BITS >= 8192
    static const char prm[] = {
        "FFFFFFFFFFFFFFFFC90FDAA22168C234"
        "C4C6628B80DC1CD129024E088A67CC74"
        "020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F1437"
        "4FE1356D6D51C245E485B576625E7EC6"
        "F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE6"
        "49286651ECE45B3DC2007CB8A163BF05"
        "98DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB"
        "9ED529077096966D670C354E4ABC9804"
        "F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28F"
        "B5C55DF06F4C52C9DE2BCBF695581718"
        "3995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33"
        "A85521ABDF1CBA64ECFB850458DBEF0A"
        "8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619D"
        "CEE3D2261AD2EE6BF12FFA06D98A0864"
        "D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E2"
        "08E24FA074E5AB3143DB5BFCE0FD108E"
        "4B82D120A92108011A723C12A787E6D7"
        "88719A10BDBA5B2699C327186AF4E23C"
        "1A946834B6150BDA2583E9CA2AD44CE8"
        "DBBBC2DB04DE8EF92E8EFC141FBECAA6"
        "287C59474E6BC05D99B2964FA090C3A2"
        "233BA186515BE7ED1F612970CEE2D7AF"
        "B81BDD762170481CD0069127D5B05AA9"
        "93B4EA988D8FDDC186FFB7DC90A6C08F"
        "4DF435C93402849236C3FAB4D27C7026"
        "C1D4DCB2602646DEC9751E763DBA37BD"
        "F8FF9406AD9E530EE5DB382F413001AE"
        "B06A53ED9027D831179727B0865A8918"
        "DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
        "DB7F1447E6CC254B332051512BD7AF42"
        "6FB8F401378CD2BF5983CA01C64B92EC"
        "F032EA15D1721D03F482D7CE6E74FEF6"
        "D55E702F46980C82B5A84031900B1C9E"
        "59E7C97FBEC7E8F323A97A7E36CC88BE"
        "0F1D45B7FF585AC54BD407B22B4154AA"
        "CC8F6D7EBF48E1D814CC5ED20F8037E0"
        "A79715EEF29BE32806A1D58BB7C5DA76"
        "F550AA3D8A1FBFF0EB19CCB1A313D55C"
        "DA56C9EC2EF29632387FE8D76E3C0468"
        "043E8F663F4860EE12BF2D5B0B7474D6"
        "E694F91E6DBE115974A3926F12FEE5E4"
        "38777CB6A932DF8CD8BEC4D073B931BA"
        "3BC832B68D9DD300741FA7BF8AFC47ED"
        "2576F6936BA424663AAB639C5AE4F568"
        "3423B4742BF1C978238F16CBE39D652D"
        "E3FDB8BEFC848AD922222E04A4037C07"
        "13EB57A81A23F0C73473FC646CEA306B"
        "4BCBC8862F8385DDFA9D4B7FA2C087E8"
        "79683303ED5BDD3A062B3CF5B3A278A6"
        "6D2A13F83F44F82DDF310EE074AB6A36"
        "4597E899A0255DC164F31CC50846851D"
        "F9AB48195DED7EA1B1D510BD7EE74D73"
        "FAF36BC31ECFA268359046F4EB879F92"
        "4009438B481C6CD7889A002ED5EE382B"
        "C9190DA6FC026E479558E4475677E9AA"
        "9E3050E2765694DFC81F56E880B96E71"
        "60C980DD98EDD3DFFFFFFFFFFFFFFFFF"
    };

    WOLFSSL_ENTER("wolfSSL_DH_8192_prime");

    /* Set prime into BN. Creates a new BN when bn is NULL. */
    if (wolfSSL_BN_hex2bn(&bn, prm) != 1) {
        WOLFSSL_ERROR_MSG("Error converting DH 8192 prime to big number");
        bn = NULL;
    }

    return bn;
#else
    (void)bn;
    return NULL;
#endif
}

/*
 * DH to/from bin APIs
 */

#ifndef NO_CERTS

/* Load the DER encoded DH parameters/key into DH key.
 *
 * @param [in, out] dh      DH key to load parameters into.
 * @param [in]      der     Buffer holding DER encoded parameters data.
 * @param [in, out] idx     On in, index at which DH key DER data starts.
 *                          On out, index after DH key DER data.
 * @param [in]      derSz   Size of DER buffer in bytes.
 *
 * @return  0 on success.
 * @return  1 when decoding DER or setting the external key fails.
 */
static int wolfssl_dh_load_key(WOLFSSL_DH* dh, const unsigned char* der,
    word32* idx, word32 derSz)
{
    int err = 0;

#if !defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0)
    int ret;

    /* Decode DH parameters/key from DER. */
    ret = wc_DhKeyDecode(der, idx, (DhKey*)dh->internal, derSz);
    if (ret != 0) {
        WOLFSSL_ERROR_MSG("DhKeyDecode() failed");
        err = 1;
    }
    if (!err) {
        /* wolfSSL DH key set. */
        dh->inSet = 1;

        /* Set the external DH key based on wolfSSL DH key. */
        if (SetDhExternal(dh) != 1) {
            WOLFSSL_ERROR_MSG("SetDhExternal failed");
            err = 1;
        }
    }
#else
    byte* p;
    byte* g;
    word32 pSz = MAX_DH_SIZE;
    word32 gSz = MAX_DH_SIZE;

    /* Only DH parameters supported. */
    /* Load external and set internal. */
    p = (byte*)XMALLOC(pSz, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
    g = (byte*)XMALLOC(gSz, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
    if ((p == NULL) || (g == NULL)) {
        err = 1;
    }
    /* Extract the p and g as data from the DER encoded DH parameters. */
    if ((!err) && (wc_DhParamsLoad(der + *idx, derSz - *idx, p, &pSz, g,
            &gSz) < 0)) {
        err = 1;
    }
    if (!err) {
        /* Put p and g in as big numbers - free existing BNs. */
        if (dh->p != NULL) {
            wolfSSL_BN_free(dh->p);
            dh->p = NULL;
        }
        if (dh->g != NULL) {
            wolfSSL_BN_free(dh->g);
            dh->g = NULL;
        }
        dh->p = wolfSSL_BN_bin2bn(p, (int)pSz, NULL);
        dh->g = wolfSSL_BN_bin2bn(g, (int)gSz, NULL);
        if (dh->p == NULL || dh->g == NULL) {
            err = 1;
        }
        else {
            /* External DH key parameters were set. */
            dh->exSet = 1;
        }
    }

    /* Set internal as the outside has been updated. */
    if ((!err) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Unable to set internal DH structure");
        err = 1;
    }

    if (!err) {
        *idx += wolfssl_der_length(der + *idx, derSz - *idx);
    }

    XFREE(p, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
    XFREE(g, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
#endif

    return err;
}

#ifdef OPENSSL_ALL

#if !defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0)
/* Convert DER encoded DH parameters to a WOLFSSL_DH structure.
 *
 * @param [out]     dh      DH key to put parameters into. May be NULL.
 * @param [in, out] pp      Pointer to DER encoded DH parameters.
 *                          Value updated to end of data when dh is not NULL.
 * @param [in]      length  Length of data available in bytes.
 *
 * @return  DH key on success.
 * @return  NULL on failure.
 */
WOLFSSL_DH *wolfSSL_d2i_DHparams(WOLFSSL_DH** dh, const unsigned char** pp,
    long length)
{
    WOLFSSL_DH *newDh = NULL;
    word32 idx = 0;
    int err = 0;

    WOLFSSL_ENTER("wolfSSL_d2i_DHparams");

    /* Validate parameters. */
    if ((pp == NULL) || (length <= 0)) {
        WOLFSSL_ERROR_MSG("bad argument");
        err = 1;
    }

    /* Create new DH key to return. */
    if ((!err) && ((newDh = wolfSSL_DH_new()) == NULL)) {
        WOLFSSL_ERROR_MSG("wolfSSL_DH_new() failed");
        err = 1;
    }
    if ((!err) && (wolfssl_dh_load_key(newDh, *pp, &idx,
            (word32)length) != 0)) {
        WOLFSSL_ERROR_MSG("Loading DH parameters failed");
        err = 1;
    }

    if ((!err) && (dh != NULL)) {
        /* Return through parameter too. */
        *dh = newDh;
        /* Move buffer on by the used amount. */
        *pp += idx;
    }

    if (err && (newDh != NULL)) {
        /* Dispose of any created DH key. */
        wolfSSL_DH_free(newDh);
        newDh = NULL;
    }
    return newDh;
}
#endif /* !HAVE_FIPS || FIPS_VERSION_GT(2,0) */

/* Calculate the number of bytes require to represent a length value in ASN.
 *
 * @param [in] l  Length value to use.
 * @return  Number of bytes required to represent length value.
 */
#define ASN_LEN_SIZE(l)             \
    (((l) < 128) ? 1 : (((l) < 256) ? 2 : 3))

/* Converts internal WOLFSSL_DH structure to DER encoded DH parameters.
 *
 * @params [in]      dh   DH key with parameters to encode.
 * @params [in, out] out  Pointer to buffer to encode into.
 *                        When NULL or pointer to NULL, only length returned.
 * @return  0 on error.
 * @return  Size of DER encoding in bytes on success.
 */
int wolfSSL_i2d_DHparams(const WOLFSSL_DH *dh, unsigned char **out)
{
#if (!defined(HAVE_FIPS) || FIPS_VERSION_GT(5,0)) && defined(WOLFSSL_DH_EXTRA)
    /* Set length to an arbitrarily large value for wc_DhParamsToDer(). */
    word32 len = (word32)-1;
    int err = 0;

    /* Validate parameters. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad parameters");
        err = 1;
    }

    /* Push external DH data into internal DH key if not set. */
    if ((!err) && (!dh->inSet) && (SetDhInternal((WOLFSSL_DH*)dh) != 1)) {
        WOLFSSL_ERROR_MSG("Bad DH set internal");
        err = 1;
    }
    if (!err) {
        int ret;
        unsigned char* der = NULL;

        /* Use *out when available otherwise NULL. */
        if (out != NULL) {
            der = *out;
        }
        /* Get length and/or encode. */
        ret = wc_DhParamsToDer((DhKey*)dh->internal, der, &len);
        /* Length of encoded data is returned on success. */
        if (ret > 0) {
            *out += len;
        }
        /* An error occurred unless only length returned. */
        else if (ret != LENGTH_ONLY_E) {
            err = 1;
        }
    }

    /* Set return to 0 on error. */
    if (err) {
        len = 0;
    }
    return (int)len;
#else
    word32 len;
    int ret = 0;
    int pSz;
    int gSz;

    WOLFSSL_ENTER("wolfSSL_i2d_DHparams");

    /* Validate parameters. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad parameters");
        len = 0;
    }
    else {
        /* SEQ <len>
         *   INT <len> [0x00] <prime>
         *   INT <len> [0x00] <generator>
         * Integers have 0x00 prepended if the top bit of positive number is
         * set.
         */
        /* Get total length of prime including any prepended zeros. */
        pSz = mp_unsigned_bin_size((mp_int*)dh->p->internal) +
              mp_leading_bit((mp_int*)dh->p->internal);
        /* Get total length of generator including any prepended zeros. */
        gSz = mp_unsigned_bin_size((mp_int*)dh->g->internal) +
              mp_leading_bit((mp_int*)dh->g->internal);
        /* Calculate length of data in sequence. */
        len = 1 + ASN_LEN_SIZE(pSz) + pSz +
              1 + ASN_LEN_SIZE(gSz) + gSz;
        /* Add in the length of the SEQUENCE. */
        len += 1 + ASN_LEN_SIZE(len);

        if ((out != NULL) && (*out != NULL)) {
            /* Encode parameters. */
            ret = StoreDHparams(*out, &len, (mp_int*)dh->p->internal,
                (mp_int*)dh->g->internal);
            if (ret != MP_OKAY) {
                WOLFSSL_ERROR_MSG("StoreDHparams error");
                len = 0;
            }
            else {
                /* Move pointer on if encoded. */
                *out += len;
            }
        }
    }

    return (int)len;
#endif
}

#endif /* OPENSSL_ALL */

#endif /* !NO_CERTS */

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) ||  \
 ((!defined(NO_BIO) || !defined(NO_FILESYSTEM)) && \
  defined(HAVE_LIGHTY) || defined(HAVE_STUNNEL) || \
  defined(WOLFSSL_MYSQL_COMPATIBLE))

/* Load the DER encoded DH parameters into DH key.
 *
 * @param [in, out] dh      DH key to load parameters into.
 * @param [in]      derBuf  Buffer holding DER encoded parameters data.
 * @param [in]      derSz   Size of DER data in buffer in bytes.
 *
 * @return  1 on success.
 * @return  -1 when DH or derBuf is NULL,
 *                  internal DH key in DH is NULL,
 *                  derSz is 0 or less,
 *                  error decoding DER data or
 *                  setting external parameter values fails.
 */
int wolfSSL_DH_LoadDer(WOLFSSL_DH* dh, const unsigned char* derBuf, int derSz)
{
    int    ret = 1;
    word32 idx = 0;

    /* Validate parameters. */
    if ((dh == NULL) || (dh->internal == NULL) || (derBuf == NULL) ||
            (derSz <= 0)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }

    if ((ret == 1) && (wolfssl_dh_load_key(dh, derBuf, &idx,
            (word32)derSz) != 0)) {
        WOLFSSL_ERROR_MSG("DH key decode failed");
        ret = -1;
    }

    return ret;
}

#endif

/*
 * DH PEM APIs
 */

#if defined(HAVE_LIGHTY) || defined(HAVE_STUNNEL) \
    || defined(WOLFSSL_MYSQL_COMPATIBLE) || defined(OPENSSL_EXTRA)

#if !defined(NO_BIO) || !defined(NO_FILESYSTEM)
/* Create a DH key by reading the PEM encoded data from the BIO.
 *
 * @param [in]      bio         BIO object to read from.
 * @param [in, out] dh          DH key to use. May be NULL.
 * @param [in]      pem         PEM data to decode.
 * @param [in]      pemSz       Size of PEM data in bytes.
 * @param [in]      memAlloced  Indicates that pem was allocated and is to be
 *                              freed after use.
 * @return  DH key on success.
 * @return  NULL on failure.
 */
static WOLFSSL_DH *wolfssl_dhparams_read_pem(WOLFSSL_DH **dh,
    unsigned char* pem, int pemSz, int memAlloced)
{
    WOLFSSL_DH* localDh = NULL;
    DerBuffer *der = NULL;
    int err = 0;

    /* Convert PEM to DER assuming DH Parameter format. */
    if ((!err) && (PemToDer(pem, pemSz, DH_PARAM_TYPE, &der, NULL, NULL,
            NULL) < 0)) {
        /* Convert PEM to DER assuming X9.42 DH Parameter format. */
        if (PemToDer(pem, pemSz, X942_PARAM_TYPE, &der, NULL, NULL, NULL)
                != 0) {
            err = 1;
        }
    }
    if (memAlloced) {
        /* PEM data no longer needed.  */
        XFREE(pem, NULL, DYNAMIC_TYPE_PEM);
    }

    if (!err) {
        /* Use the DH key passed in or allocate a new one. */
        if (dh != NULL) {
            localDh = *dh;
        }
        if (localDh == NULL) {
            localDh = wolfSSL_DH_new();
            if (localDh == NULL) {
                err = 1;
            }
        }
    }
    /* Load the DER encoded DH parameters from buffer into a DH key. */
    if ((!err) && (wolfSSL_DH_LoadDer(localDh, der->buffer, der->length)
            != 1)) {
        /* Free an allocated DH key. */
        if ((dh == NULL) || (localDh != *dh)) {
            wolfSSL_DH_free(localDh);
        }
        localDh = NULL;
        err = 1;
    }
    /* Return the DH key on success. */
    if ((!err) && (dh != NULL)) {
        *dh = localDh;
    }

    /* Dispose of DER data. */
    if (der != NULL) {
        FreeDer(&der);
    }
    return localDh;
}
#endif /* !NO_BIO || !NO_FILESYSTEM */

#ifndef NO_BIO
/* Create a DH key by reading the PEM encoded data from the BIO.
 *
 * DH parameters are public data and are not expected to be encrypted.
 *
 * @param [in]      bio   BIO object to read from.
 * @param [in, out] dh    DH key to   When pointer to
 *                        NULL, a new DH key is created.
 * @param [in]      cb    Password callback when PEM encrypted. Not used.
 * @param [in]      pass  NUL terminated string for passphrase when PEM
 *                        encrypted. Not used.
 * @return  DH key on success.
 * @return  NULL on failure.
 */
WOLFSSL_DH *wolfSSL_PEM_read_bio_DHparams(WOLFSSL_BIO *bio, WOLFSSL_DH **dh,
    wc_pem_password_cb *cb, void *pass)
{
    WOLFSSL_DH* localDh = NULL;
    int err = 0;
    unsigned char* mem = NULL;
    int size = 0;
    int memAlloced = 0;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_DHparams");

    (void)cb;
    (void)pass;

    /* Validate parameters. */
    if (bio == NULL) {
        WOLFSSL_ERROR_MSG("Bad Function Argument bio is NULL");
        err = 1;
    }

    /* Get buffer of data from BIO or read data from the BIO into a new buffer.
     */
    if ((!err) && (wolfssl_read_bio(bio, (char**)&mem, &size, &memAlloced)
            != 0)) {
        err = 1;
    }
    if (!err) {
        /* Create a DH key from the PEM - try two different headers. */
        localDh = wolfssl_dhparams_read_pem(dh, mem, size, memAlloced);
    }

    return localDh;
}

#endif /* !NO_BIO */

#ifndef NO_FILESYSTEM
/* Read DH parameters from a file pointer into DH key.
 *
 * DH parameters are public data and are not expected to be encrypted.
 *
 * @param [in]      fp    File pointer to read DH parameter file from.
 * @param [in, out] dh    DH key with parameters if not NULL. When pointer to
 *                        NULL, a new DH key is created.
 * @param [in]      cb    Password callback when PEM encrypted. Not used.
 * @param [in]      pass  NUL terminated string for passphrase when PEM
 *                        encrypted. Not used.
 *
 * @return  NULL on failure.
 * @return  DH key with parameters set on success.
 */
WOLFSSL_DH* wolfSSL_PEM_read_DHparams(XFILE fp, WOLFSSL_DH** dh,
    wc_pem_password_cb* cb, void* pass)
{
    WOLFSSL_DH* localDh = NULL;
    int err = 0;
    unsigned char* mem = NULL;
    int size = 0;

    (void)cb;
    (void)pass;

    /* Read data from file pointer. */
    if (wolfssl_read_file(fp, (char**)&mem, &size) != 0) {
        err = 1;
    }
    if (!err) {
        localDh = wolfssl_dhparams_read_pem(dh, mem, size, 1);
    }

    return localDh;
}
#endif /* !NO_FILESYSTEM */

#if defined(WOLFSSL_DH_EXTRA) && !defined(NO_FILESYSTEM)
/* Encoded parameter data in DH key as DER.
 *
 * @param [in, out] dh    DH key object to encode.
 * @param [out]     out   Buffer containing DER encoding.
 * @param [in]      heap  Heap hint.
 * @return  <0 on error.
 * @return  Length of DER encoded DH parameters in bytes.
 */
static int wolfssl_dhparams_to_der(WOLFSSL_DH* dh, unsigned char** out,
    void* heap)
{
    int ret = -1;
    int err = 0;
    byte* der = NULL;
    word32 derSz;
    DhKey* key;

    /* Set internal parameters based on external parameters. */
    if ((dh->inSet == 0) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Unable to set internal DH structure");
        err = 1;
    }
    if (!err) {
        /* Use wolfSSL API to get length of DER encode DH parameters. */
        key = (DhKey*)dh->internal;
        ret = wc_DhParamsToDer(key, NULL, &derSz);
        if (ret != LENGTH_ONLY_E) {
            WOLFSSL_ERROR_MSG("Failed to get size of DH params");
            err = 1;
        }
    }

    if (!err) {
        /* Allocate memory for DER encoding. */
        der = (byte*)XMALLOC(derSz, heap, DYNAMIC_TYPE_TMP_BUFFER);
        if (der == NULL) {
            WOLFSSL_LEAVE("wolfssl_dhparams_to_der", MEMORY_E);
            err = 1;
        }
    }
    if (!err) {
        /* Encode DH parameters into DER buffer. */
        ret = wc_DhParamsToDer(key, der, &derSz);
        if (ret < 0) {
            WOLFSSL_ERROR_MSG("Failed to export DH params");
            err = 1;
        }
    }

    if (!err) {
        *out = der;
        der = NULL;
    }
    if (der != NULL) {
        XFREE(der, heap, DYNAMIC_TYPE_TMP_BUFFER);
    }

    return ret;
}

/* Writes the DH parameters in PEM format from "dh" out to the file pointer
 * passed in.
 *
 * @param [in]  fp  File pointer to write to.
 * @param [in]  dh  DH key to write.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_PEM_write_DHparams(XFILE fp, WOLFSSL_DH* dh)
{
    int ret = 1;
    int derSz;
    byte* derBuf = NULL;
    void* heap = NULL;

    WOLFSSL_ENTER("wolfSSL_PEM_write_DHparams");

    /* Validate parameters. */
    if ((fp == XBADFILE) || (dh == NULL)) {
        WOLFSSL_ERROR_MSG("Bad Function Arguments");
        ret = 0;
    }

    if (ret == 1) {
        DhKey* key = (DhKey*)dh->internal;
        if (key)
            heap = key->heap;
        if ((derSz = wolfssl_dhparams_to_der(dh, &derBuf, heap)) < 0) {
            WOLFSSL_ERROR_MSG("DER encoding failed");
            ret = 0;
        }
        if (derBuf == NULL) {
            WOLFSSL_ERROR_MSG("DER encoding failed to get buffer");
            ret = 0;
        }
    }
    if ((ret == 1) && (der_write_to_file_as_pem(derBuf, derSz, fp,
            DH_PARAM_TYPE, NULL) != WOLFSSL_SUCCESS)) {
        ret = 0;
    }

    /* Dispose of DER buffer. */
    XFREE(derBuf, heap, DYNAMIC_TYPE_TMP_BUFFER);

    WOLFSSL_LEAVE("wolfSSL_PEM_write_DHparams", ret);

    return ret;
}
#endif /* WOLFSSL_DH_EXTRA && !NO_FILESYSTEM */

#endif /* HAVE_LIGHTY || HAVE_STUNNEL || WOLFSSL_MYSQL_COMPATIBLE ||
        * OPENSSL_EXTRA */

/*
 * DH get/set APIs
 */

#ifdef OPENSSL_EXTRA

#if defined(WOLFSSL_QT) || defined(OPENSSL_ALL) \
    || defined(WOLFSSL_OPENSSH) || defined(OPENSSL_EXTRA)

/* Set the members of DhKey into WOLFSSL_DH
 * Specify elements to set via the 2nd parameter
 *
 * @param [in, out] dh   DH key to synchronize.
 * @param [in]      elm  Elements to synchronize.
 * @return  1 on success.
 * @return  -1 on failure.
 */
int SetDhExternal_ex(WOLFSSL_DH *dh, int elm)
{
    int ret = 1;
    DhKey *key = NULL;

    WOLFSSL_ENTER("SetDhExternal_ex");

    /* Validate parameters. */
    if ((dh == NULL) || (dh->internal == NULL)) {
        WOLFSSL_ERROR_MSG("dh key NULL error");
        ret = -1;
    }

    if (ret == 1) {
        /* Get the wolfSSL DH key. */
        key = (DhKey*)dh->internal;
    }

    if ((ret == 1) && (elm & ELEMENT_P)) {
        /* Set the prime. */
        if (SetIndividualExternal(&dh->p, &key->p) != 1) {
            WOLFSSL_ERROR_MSG("dh param p error");
            ret = -1;
        }
    }
    if ((ret == 1) && (elm & ELEMENT_G)) {
        /* Set the generator. */
        if (SetIndividualExternal(&dh->g, &key->g) != 1) {
            WOLFSSL_ERROR_MSG("dh param g error");
            ret = -1;
        }
    }
    if ((ret == 1) && (elm & ELEMENT_Q)) {
        /* Set the order. */
        if (SetIndividualExternal(&dh->q, &key->q) != 1) {
            WOLFSSL_ERROR_MSG("dh param q error");
            ret = -1;
        }
    }
#ifdef WOLFSSL_DH_EXTRA
    if ((ret == 1) && (elm & ELEMENT_PRV)) {
        /* Set the private key. */
        if (SetIndividualExternal(&dh->priv_key, &key->priv) != 1) {
            WOLFSSL_ERROR_MSG("No DH Private Key");
            ret = -1;
        }
    }
    if ((ret == 1) && (elm & ELEMENT_PUB)) {
        /* Set the public key. */
        if (SetIndividualExternal(&dh->pub_key, &key->pub) != 1) {
            WOLFSSL_ERROR_MSG("No DH Public Key");
            ret = -1;
        }
    }
#endif /* WOLFSSL_DH_EXTRA */

    if (ret == 1) {
        /* On success record that the external values have been set. */
        dh->exSet = 1;
    }

    return ret;
}
/* Set the members of DhKey into WOLFSSL_DH
 * DhKey was populated from wc_DhKeyDecode
 * p, g, pub_key and priv_key are set.
 *
 * @param [in, out] dh   DH key to synchronize.
 * @return  1 on success.
 * @return  -1 on failure.
 */
int SetDhExternal(WOLFSSL_DH *dh)
{
    /* Assuming Q not required when using this API. */
    int elements = ELEMENT_P | ELEMENT_G | ELEMENT_PUB | ELEMENT_PRV;
    WOLFSSL_ENTER("SetDhExternal");
    return SetDhExternal_ex(dh, elements);
}
#endif /* WOLFSSL_QT || OPENSSL_ALL || WOLFSSL_OPENSSH || OPENSSL_EXTRA */

/* Set the internal/wolfSSL DH key with data from the external parts.
 *
 * @param [in, out] dh   DH key to synchronize.
 * @return  1 on success.
 * @return  -1 on failure.
 */
int SetDhInternal(WOLFSSL_DH* dh)
{
    int ret = 1;
    DhKey *key = NULL;

    WOLFSSL_ENTER("SetDhInternal");

    /* Validate parameters. */
    if ((dh == NULL) || (dh->p == NULL) || (dh->g == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }
    if (ret == 1) {
        /* Get the wolfSSL DH key. */
        key = (DhKey*)dh->internal;

        /* Clear out key and initialize. */
        wc_FreeDhKey(key);
        if (wc_InitDhKey(key) != 0) {
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Transfer prime. */
        if (SetIndividualInternal(dh->p, &key->p) != 1) {
            ret = -1;
        }
    }
    if (ret == 1) {
        /* Transfer generator. */
        if (SetIndividualInternal(dh->g, &key->g) != 1) {
            ret = -1;
        }
    }
#ifdef HAVE_FFDHE_Q
    /* Transfer order if available. */
    if ((ret == 1) && (dh->q != NULL)) {
        if (SetIndividualInternal(dh->q, &key->q) != 1) {
            ret = -1;
        }
    }
#endif
#ifdef WOLFSSL_DH_EXTRA
    /* Transfer private key if available. */
    if ((ret == 1) && (dh->priv_key != NULL) &&
            (!wolfSSL_BN_is_zero(dh->priv_key))) {
        if (SetIndividualInternal(dh->priv_key, &key->priv) != 1) {
            ret = -1;
        }
    }
    /* Transfer public key if available. */
    if ((ret == 1) && (dh->pub_key != NULL) &&
            (!wolfSSL_BN_is_zero(dh->pub_key))) {
        if (SetIndividualInternal(dh->pub_key, &key->pub) != 1) {
            ret = -1;
        }
    }
#endif /* WOLFSSL_DH_EXTRA */

    if (ret == 1) {
        /* On success record that the internal values have been set. */
        dh->inSet = 1;
    }

    return ret;
}

/* Get the size, in bytes, of the DH key.
 *
 * Return code compliant with OpenSSL.
 *
 * @param [in] dh  DH key.
 * @return  -1 on error.
 * @return  Size of DH key in bytes on success.
 */
int wolfSSL_DH_size(WOLFSSL_DH* dh)
{
    int ret = -1;

    WOLFSSL_ENTER("wolfSSL_DH_size");

    /* Validate paramater. */
    if (dh != NULL) {
        /* Size of key is size of prime in bytes. */
        ret = wolfSSL_BN_num_bytes(dh->p);
    }

    return ret;
}

/**
 * Return parameters p, q and/or g of the DH key.
 *
 * @param [in]  dh  DH key to retrieve parameters from.
 * @param [out] p   Pointer to return prime in. May be NULL.
 * @param [out] q   Pointer to return order in. May be NULL.
 * @param [out] g   Pointer to return generator in. May be NULL.
 */
void wolfSSL_DH_get0_pqg(const WOLFSSL_DH *dh, const WOLFSSL_BIGNUM **p,
    const WOLFSSL_BIGNUM **q, const WOLFSSL_BIGNUM **g)
{
    WOLFSSL_ENTER("wolfSSL_DH_get0_pqg");

    if (dh != NULL) {
        /* Return prime if required. */
        if (p != NULL) {
            *p = dh->p;
        }
        /* Return order if required. */
        if (q != NULL) {
            *q = dh->q;
        }
        /* Return generator if required. */
        if (g != NULL) {
            *g = dh->g;
        }
    }
}

#if !defined(HAVE_FIPS) || (defined(HAVE_FIPS) && !defined(WOLFSSL_DH_EXTRA)) \
 || (defined(HAVE_FIPS_VERSION) && FIPS_VERSION_GT(2,0))
#if defined(OPENSSL_ALL) || \
    defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 0x10100000L
/* Sets the parameters p, g and optionally q into the DH key.
 *
 * Ownership of p, q and g get taken over by "dh" on success and should be
 * free'd with a call to wolfSSL_DH_free -- not individually.
 *
 * @param [in, out] dh   DH key to set.
 * @parma [in]      p    Prime value to set. May be NULL when value already
 *                       present.
 * @parma [in]      q    Order value to set. May be NULL.
 * @parma [in]      g    Generator value to set. May be NULL when value already
 *                       present.
 * @return  1 on success.
 * @return  0 on failure.
 */
int wolfSSL_DH_set0_pqg(WOLFSSL_DH *dh, WOLFSSL_BIGNUM *p,
    WOLFSSL_BIGNUM *q, WOLFSSL_BIGNUM *g)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_DH_set0_pqg");

    /* Validate parameters - q is optional. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }
    /* p can be NULL if we already have one set. */
    if ((ret == 1) && (p == NULL) && (dh->p == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }
    /* g can be NULL if we already have one set. */
    if ((ret == 1) && (g == NULL) && (dh->g == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    if (ret == 1) {
        /* Invalidate internal key. */
        dh->inSet = 0;

        /* Free external representation of parameters and set with those passed
         * in. */
        if (p != NULL) {
            wolfSSL_BN_free(dh->p);
            dh->p = p;
        }
        if (q != NULL) {
            wolfSSL_BN_free(dh->q);
            dh->q = q;
        }
        if (g != NULL) {
            wolfSSL_BN_free(dh->g);
            dh->g = g;
        }
        /* External DH key parameters were set. */
        dh->exSet = 1;

        /* Set internal/wolfSSL DH key as well. */
        if (SetDhInternal(dh) != 1) {
            WOLFSSL_ERROR_MSG("Unable to set internal DH key");
            /* Don't keep parameters on failure. */
            dh->p = NULL;
            dh->q = NULL;
            dh->g = NULL;
            /* Internal and external DH key not set. */
            dh->inSet = 0;
            dh->exSet = 0;
            ret = 0;
        }
    }

    return ret;
}

/* Set the length of the DH private key in bits.
 *
 * Length field is checked at generation.
 *
 * @param [in, out] dh   DH key to set.
 * @param [in]      len  Length of DH private key in bytes.
 * @return  0 on failure.
 * @return  1 on success.
 */
int wolfSSL_DH_set_length(WOLFSSL_DH *dh, long len)
{
    int ret = 1;

    WOLFSSL_ENTER("wolfSSL_DH_set_length");

    /* Validate parameter. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }
    else {
        /* Store length. */
        dh->length = (int)len;
    }

    return ret;
}
#endif /* OPENSSL_ALL || (v1.1.0 or later) */
#endif

/* Get the public and private keys requested.
 *
 * @param [in]  dh         DH key to get keys from.
 * @param [out] pub_key    Pointer to return public key in. May be NULL.
 * @param [out] priv_key   Pointer to return private key in. May be NULL.
 */
void wolfSSL_DH_get0_key(const WOLFSSL_DH *dh, const WOLFSSL_BIGNUM **pub_key,
    const WOLFSSL_BIGNUM **priv_key)
{
    WOLFSSL_ENTER("wolfSSL_DH_get0_key");

    /* Get only when valid DH passed in. */
    if (dh != NULL) {
        /* Return public key if required and available. */
        if ((pub_key != NULL) && (dh->pub_key != NULL)) {
            *pub_key = dh->pub_key;
        }
        /* Return private key if required and available. */
        if ((priv_key != NULL) && (dh->priv_key != NULL)) {
            *priv_key = dh->priv_key;
        }
    }
}

/* Set the public and/or private key.
 *
 * @param [in, out] dh        DH key to have keys set into.
 * @param [in]      pub_key   Public key to set. May be NULL.
 * @param [in]      priv_key  Private key to set. May be NULL.
 * @return  0 on failure.
 * @return  1 on success.
 */
int wolfSSL_DH_set0_key(WOLFSSL_DH *dh, WOLFSSL_BIGNUM *pub_key,
    WOLFSSL_BIGNUM *priv_key)
{
    int ret = 1;
#ifdef WOLFSSL_DH_EXTRA
    DhKey *key = NULL;
#endif

    WOLFSSL_ENTER("wolfSSL_DH_set0_key");

    /* Validate parameters. */
    if (dh == NULL) {
        ret = 0;
    }
#ifdef WOLFSSL_DH_EXTRA
    else {
        key = (DhKey*)dh->internal;
    }
#endif

    /* Replace public key when one passed in. */
    if ((ret == 1) && (pub_key != NULL)) {
        wolfSSL_BN_free(dh->pub_key);
        dh->pub_key = pub_key;
    #ifdef WOLFSSL_DH_EXTRA
        if (SetIndividualInternal(dh->pub_key, &key->pub) != 1) {
            ret = 0;
        }
    #endif
    }

    /* Replace private key when one passed in. */
    if ((ret == 1) && (priv_key != NULL)) {
        wolfSSL_BN_clear_free(dh->priv_key);
        dh->priv_key = priv_key;
    #ifdef WOLFSSL_DH_EXTRA
        if (SetIndividualInternal(dh->priv_key, &key->priv) != 1) {
            ret = 0;
        }
    #endif
    }

    return ret;
}

#endif /* OPENSSL_EXTRA */

/*
 * DH check APIs
 */

#ifdef OPENSSL_EXTRA

#ifndef NO_CERTS

#ifdef OPENSSL_ALL
/* Check whether BN number is a prime.
 *
 * @param [in]  n        Number to check.
 * @param [out] isPrime  MP_YES when prime and MP_NO when not.
 * @return  1 on success.
 * @return  0 on error.
 */
static int wolfssl_dh_check_prime(WOLFSSL_BIGNUM* n, int* isPrime)
{
    int ret = 1;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif
    WC_RNG* rng;
    int localRng;

    /* Make an RNG with tmpRng or get global. */
    rng = wolfssl_make_rng(tmpRng, &localRng);
    if (rng == NULL) {
        ret = 0;
    }
    if (ret == 1) {
        mp_int* prime = (mp_int*)n->internal;

        if (mp_prime_is_prime_ex(prime, 8, isPrime, rng) != 0) {
            ret = 0;
        }
        /* Free local random number generator if created. */
        if (localRng) {
            wc_FreeRng(rng);
        #ifdef WOLFSSL_SMALL_STACK
            XFREE(rng, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        #endif
        }
    }

    return ret;
}

/* Checks the Diffie-Hellman parameters.
 *
 * Checks that the generator and prime are available.
 * Checks that the prime is prime.
 * OpenSSL expects codes to be non-NULL.
 *
 * @param [in]  dh     DH key to check.
 * @param [out] codes  Codes of checks that failed.
 * @return  1 on success.
 * @return  0 when DH is NULL, there were errors or failed to create a random
 *          number generator.
 */
int wolfSSL_DH_check(const WOLFSSL_DH *dh, int *codes)
{
    int ret = 1;
    int errors = 0;

    WOLFSSL_ENTER("wolfSSL_DH_check");

    /* Validate parameters. */
    if (dh == NULL) {
        ret = 0;
    }

    /* Check generator available. */
    if ((ret == 1) && ((dh->g == NULL) || (dh->g->internal == NULL))) {
        errors |= DH_NOT_SUITABLE_GENERATOR;
    }

    if (ret == 1) {
        /* Check prime available. */
        if ((dh->p == NULL) || (dh->p->internal == NULL)) {
            errors |= DH_CHECK_P_NOT_PRIME;
        }
        else {
            /* Test if dh->p is prime. */
            int isPrime = MP_NO;
            ret = wolfssl_dh_check_prime(dh->p, &isPrime);
            /* Set error code if parameter p is not prime. */
            if ((ret == 1) && (isPrime != MP_YES)) {
                errors |= DH_CHECK_P_NOT_PRIME;
            }
        }
    }

    /* Return errors when user wants exact issues. */
    if (codes != NULL) {
        *codes = errors;
    }
    else if (errors) {
        ret = 0;
    }

    return ret;
}

#endif /* OPENSSL_ALL */

#endif /* !NO_CERTS */

#endif /* OPENSSL_EXTRA */

/*
 * DH generate APIs
 */

#if defined(OPENSSL_ALL) || (defined(OPENSSL_EXTRA) && \
    (defined(HAVE_STUNNEL) || defined(WOLFSSL_NGINX) || \
    defined(HAVE_LIGHTY) || defined(WOLFSSL_HAPROXY) || \
    defined(WOLFSSL_OPENSSH) || defined(HAVE_SBLIM_SFCB)))

#if defined(WOLFSSL_KEY_GEN) && !defined(HAVE_SELFTEST)
/* Generate DH parameters.
 *
 * @param [in] prime_len  Length of prime in bits.
 * @param [in] generator  Gnerator value to use.
 * @param [in] callback   Called with progress information. Unused.
 * @param [in] cb_arg     User callback argument. Unused.
 * @return  NULL on failure.
 * @return  DH key on success.
 */
WOLFSSL_DH *wolfSSL_DH_generate_parameters(int prime_len, int generator,
                           void (*callback) (int, int, void *), void *cb_arg)
{
    WOLFSSL_DH* dh = NULL;

    WOLFSSL_ENTER("wolfSSL_DH_generate_parameters");
    /* Not supported by wolfSSl APIs. */
    (void)callback;
    (void)cb_arg;

    /* Create an empty DH key. */
    if ((dh = wolfSSL_DH_new()) == NULL) {
        WOLFSSL_ERROR_MSG("wolfSSL_DH_new error");
    }
    /* Generate parameters into DH key. */
    else if (wolfSSL_DH_generate_parameters_ex(dh, prime_len, generator, NULL)
            != 1) {
        WOLFSSL_ERROR_MSG("wolfSSL_DH_generate_parameters_ex error");
        wolfSSL_DH_free(dh);
        dh = NULL;
    }

    return dh;
}

/* Generate DH parameters.
 *
 * @param [in] dh         DH key to generate parameters into.
 * @param [in] prime_len  Length of prime in bits.
 * @param [in] generator  Gnerator value to use.
 * @param [in] callback   Called with progress information. Unused.
 * @param [in] cb_arg     User callback argument. Unused.
 * @return  0 on failure.
 * @return  1 on success.
 */
int wolfSSL_DH_generate_parameters_ex(WOLFSSL_DH* dh, int prime_len,
    int generator, void (*callback) (int, int, void *))
{
    int ret = 1;
    DhKey* key;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif
    WC_RNG* rng = NULL;
    int localRng = 0;

    WOLFSSL_ENTER("wolfSSL_DH_generate_parameters_ex");
    /* Not supported by wolfSSL APIs. */
    (void)callback;
    (void)generator;

    /* Validate parameters. */
    if (dh == NULL) {
        WOLFSSL_ERROR_MSG("Bad parameter");
        ret = 0;
    }

    if (ret == 1) {
        /* Make an RNG with tmpRng or get global. */
        rng = wolfssl_make_rng(tmpRng, &localRng);
        if (rng == NULL) {
            WOLFSSL_ERROR_MSG("No RNG to use");
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Get internal/wolfSSL DH key. */
        key = (DhKey*)dh->internal;

        /* Clear out data from internal DH key. */
        wc_FreeDhKey(key);
        /* Re-initialize internal DH key. */
        if (wc_InitDhKey(key) != 0) {
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Generate parameters into internal DH key. */
        if (wc_DhGenerateParams(rng, prime_len, key) != 0) {
            WOLFSSL_ERROR_MSG("wc_DhGenerateParams error");
            ret = 0;
        }
    }

    /* Free local random number generator if created. */
    if (localRng) {
        wc_FreeRng(rng);
    #ifdef WOLFSSL_SMALL_STACK
        XFREE(rng, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    #endif
    }

    if (ret == 1) {
        /* Internal parameters set by generation. */
        dh->inSet = 1;

        WOLFSSL_MSG("wolfSSL does not support using a custom generator.");

        /* Synchronize the external to the internal parameters. */
        if (SetDhExternal(dh) != 1) {
            WOLFSSL_ERROR_MSG("SetDhExternal error");
            ret = 0;
        }
    }

    return ret;
}
#endif /* WOLFSSL_KEY_GEN && !HAVE_SELFTEST */

#endif /* OPENSSL_ALL || (OPENSSL_EXTRA && (HAVE_STUNNEL || WOLFSSL_NGINX ||
        * HAVE_LIGHTY || WOLFSSL_HAPROXY || WOLFSSL_OPENSSH ||
        * HAVE_SBLIM_SFCB)) */

#ifdef OPENSSL_EXTRA

#if !defined(HAVE_FIPS) || (defined(HAVE_FIPS) && !defined(WOLFSSL_DH_EXTRA)) \
 || (defined(HAVE_FIPS_VERSION) && FIPS_VERSION_GT(2,0))
/* Generate a public/private key pair base on parameters.
 *
 * @param [in, out] dh  DH key to generate keys into.
 * @return  1 on success.
 * @return  0 on error.
 */
int wolfSSL_DH_generate_key(WOLFSSL_DH* dh)
{
    int     ret    = 1;
    word32  pubSz  = 0;
    word32  privSz = 0;
    int     localRng = 0;
    WC_RNG* rng    = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif
    unsigned char* pub    = NULL;
    unsigned char* priv   = NULL;

    WOLFSSL_ENTER("wolfSSL_DH_generate_key");

    /* Validate parameters. */
    if ((dh == NULL) || (dh->p == NULL) || (dh->g == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = 0;
    }

    /* Synchronize the external and internal parameters. */
    if ((ret == 1) && (dh->inSet == 0) && (SetDhInternal(dh) != 1)) {
        WOLFSSL_ERROR_MSG("Bad DH set internal");
        ret = 0;
    }

    if (ret == 1) {
        /* Make a new RNG or use global. */
        rng = wolfssl_make_rng(tmpRng, &localRng);
        /* Check we have a random number generator. */
        if (rng == NULL) {
            ret = 0;
        }
    }

    if (ret == 1) {
        /* Get the size of the prime in bytes. */
        pubSz = wolfSSL_BN_num_bytes(dh->p);
        if (pubSz == 0) {
            WOLFSSL_ERROR_MSG("Prime parameter invalid");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Private key size can be as much as the size of the prime. */
        if (dh->length) {
            privSz = dh->length / 8; /* to bytes */
        }
        else {
            privSz = pubSz;
        }
        /* Allocate public and private key arrays. */
        pub = (unsigned char*)XMALLOC(pubSz, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
        priv = (unsigned char*)XMALLOC(privSz, NULL, DYNAMIC_TYPE_PRIVATE_KEY);
        if (pub == NULL || priv == NULL) {
            WOLFSSL_ERROR_MSG("Unable to malloc memory");
            ret = 0;
        }
    }
    if (ret == 1) {
        /* Dispose of old public and private keys. */
        wolfSSL_BN_free(dh->pub_key);
        wolfSSL_BN_free(dh->priv_key);

        /* Allocate new public and private keys. */
        dh->pub_key = wolfSSL_BN_new();
        dh->priv_key = wolfSSL_BN_new();
        if (dh->pub_key == NULL) {
            WOLFSSL_ERROR_MSG("Bad DH new pub");
            ret = 0;
        }
        if (dh->priv_key == NULL) {
            WOLFSSL_ERROR_MSG("Bad DH new priv");
            ret = 0;
        }
    }

    PRIVATE_KEY_UNLOCK();
    /* Generate public and private keys into arrays. */
    if ((ret == 1) && (wc_DhGenerateKeyPair((DhKey*)dh->internal, rng, priv,
            &privSz, pub, &pubSz) < 0)) {
        WOLFSSL_ERROR_MSG("Bad wc_DhGenerateKeyPair");
        ret = 0;
    }
    /* Set public key from array. */
    if ((ret == 1) && (wolfSSL_BN_bin2bn(pub, pubSz, dh->pub_key) == NULL)) {
        WOLFSSL_ERROR_MSG("Bad DH bn2bin error pub");
        ret = 0;
    }
    /* Set private key from array. */
    if ((ret == 1) && (wolfSSL_BN_bin2bn(priv, privSz, dh->priv_key) == NULL)) {
        WOLFSSL_ERROR_MSG("Bad DH bn2bin error priv");
        ret = 0;
    }
    PRIVATE_KEY_LOCK();

    if (localRng) {
        /* Free an initialized local random number generator. */
        wc_FreeRng(rng);
    #ifdef WOLFSSL_SMALL_STACK
        XFREE(rng, NULL, DYNAMIC_TYPE_RNG);
    #endif
    }
    /* Dispose of allocated data. */
    XFREE(pub,  NULL, DYNAMIC_TYPE_PUBLIC_KEY);
    XFREE(priv, NULL, DYNAMIC_TYPE_PRIVATE_KEY);

    return ret;
}


/* Compute the shared key from the private key and peer's public key.
 *
 * Return code compliant with OpenSSL.
 * OpenSSL returns 0 when number of bits in p are smaller than minimum
 * supported.
 *
 * @param [out] key       Buffer to place shared key.
 * @param [in]  otherPub  Peer's public key.
 * @param [in]  dh        DH key containing private key.
 * @return  -1 on error.
 * @return  Size of shared secret in bytes on success.
 */
int wolfSSL_DH_compute_key(unsigned char* key, const WOLFSSL_BIGNUM* otherPub,
    WOLFSSL_DH* dh)
{
    int            ret    = 0;
    word32         keySz  = 0;
    int            pubSz  = MAX_DHKEY_SZ;
    int            privSz = MAX_DHKEY_SZ;
    int            sz;
#ifdef WOLFSSL_SMALL_STACK
    unsigned char* pub    = NULL;
    unsigned char* priv   = NULL;
#else
    unsigned char  pub [MAX_DHKEY_SZ];
    unsigned char  priv[MAX_DHKEY_SZ];
#endif

    WOLFSSL_ENTER("wolfSSL_DH_compute_key");

    /* Validate parameters. */
    if ((dh == NULL) || (dh->priv_key == NULL) || (otherPub == NULL)) {
        WOLFSSL_ERROR_MSG("Bad function arguments");
        ret = -1;
    }
    /* Get the maximum size of computed DH key. */
    if ((ret == 0) && ((keySz = (word32)DH_size(dh)) == 0)) {
        WOLFSSL_ERROR_MSG("Bad DH_size");
        ret = -1;
    }
    if (ret == 0) {
        /* Validate the size of the private key. */
        sz = wolfSSL_BN_num_bytes(dh->priv_key);
        if (sz > (int)privSz) {
            WOLFSSL_ERROR_MSG("Bad priv internal size");
            ret = -1;
        }
    }
    if (ret == 0) {
    #ifdef WOLFSSL_SMALL_STACK
        /* Keep real private key size to minimize amount allocated. */
        privSz = sz;
    #endif

        /* Validate the size of the public key. */
        sz = wolfSSL_BN_num_bytes(otherPub);
        if (sz > (int)pubSz) {
            WOLFSSL_ERROR_MSG("Bad otherPub size");
            ret = -1;
        }
    }

    if (ret == 0) {
    #ifdef WOLFSSL_SMALL_STACK
        /* Allocate memory for the public key array. */
        pub = (unsigned char*)XMALLOC(sz, NULL, DYNAMIC_TYPE_PUBLIC_KEY);
        if (pub == NULL)
            ret = -1;
    }
    if (ret == 0) {
        /* Allocate memory for the private key array. */
        priv = (unsigned char*)XMALLOC(privSz, NULL, DYNAMIC_TYPE_PRIVATE_KEY);
        if (priv == NULL) {
            ret = -1;
        }
    }
    if (ret == 0) {
    #endif
        /* Get the private key into the array. */
        privSz = wolfSSL_BN_bn2bin(dh->priv_key, priv);
        if (privSz <= 0) {
            ret = -1;
        }
    }
    if (ret == 0) {
        /* Get the public key into the array. */
        pubSz  = wolfSSL_BN_bn2bin(otherPub, pub);
        if (privSz <= 0) {
            ret = -1;
        }
    }
    /* Synchronize the external into the internal parameters. */
    if ((ret == 0) && ((dh->inSet == 0) && (SetDhInternal(dh) != 1))) {
        WOLFSSL_ERROR_MSG("Bad DH set internal");
        ret = -1;
    }

    PRIVATE_KEY_UNLOCK();
    /* Calculate shared secret from private and public keys. */
    if ((ret == 0) && (wc_DhAgree((DhKey*)dh->internal, key, &keySz, priv,
            privSz, pub, pubSz) < 0)) {
        WOLFSSL_ERROR_MSG("wc_DhAgree failed");
        ret = -1;
    }
    if (ret == 0) {
        /* Return actual length. */
        ret = (int)keySz;
    }
    PRIVATE_KEY_LOCK();

#ifdef WOLFSSL_SMALL_STACK
    if (priv != NULL)
#endif
    {
        /* Zeroize sensitive data. */
        ForceZero(priv, privSz);
    }
#ifdef WOLFSSL_SMALL_STACK
    XFREE(pub,  NULL, DYNAMIC_TYPE_PUBLIC_KEY);
    XFREE(priv, NULL, DYNAMIC_TYPE_PRIVATE_KEY);
#endif

    WOLFSSL_LEAVE("wolfSSL_DH_compute_key", ret);

    return ret;
}
#endif /* !HAVE_FIPS || (HAVE_FIPS && !WOLFSSL_DH_EXTRA) ||
        * HAVE_FIPS_VERSION > 2 */

#endif /* OPENSSL_EXTRA */

#endif /* NO_DH */

/*******************************************************************************
 * END OF DH API
 ******************************************************************************/


/*******************************************************************************
 * START OF EC API
 ******************************************************************************/

#ifdef HAVE_ECC

#if defined(OPENSSL_EXTRA)

#ifndef NO_CERTS

#if defined(XFPRINTF) && !defined(NO_FILESYSTEM) && \
    !defined(NO_STDIO_FILESYSTEM)
int wolfSSL_EC_KEY_print_fp(XFILE fp, WOLFSSL_EC_KEY* key, int indent)
{
    int ret = 1;
    int bits = 0;
    int priv = 0;
    int nid = 0;
    const char* curve;
    const char* nistName;
    WOLFSSL_BIGNUM* pubBn = NULL;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_print_fp");

    if (fp == XBADFILE || key == NULL || key->group == NULL || indent < 0) {
        ret = 0;
    }

    if (ret == 1) {
        bits = wolfSSL_EC_GROUP_order_bits(key->group);
        if (bits <= 0) {
            WOLFSSL_MSG("Failed to get group order bits.");
            ret = 0;
        }
    }
    if (ret == 1) {
        if (XFPRINTF(fp, "%*s", indent, "") < 0)
            ret = 0;
    }
    if (ret == 1) {
        if (key->priv_key != NULL && !wolfSSL_BN_is_zero(key->priv_key)) {
            if (XFPRINTF(fp, "Private-Key: (%d bit)\n", bits) < 0)
                ret = 0;
            priv = 1;
        }
        else {
            if (XFPRINTF(fp, "Public-Key: (%d bit)\n", bits) < 0)
                ret = 0;
        }

        if (priv) {
            ret = pk_bn_field_print_fp(fp, indent, "priv", key->priv_key);
        }
    }
    if (ret == 1 && key->pub_key != NULL && key->pub_key->exSet) {
        pubBn = wolfSSL_EC_POINT_point2bn(key->group, key->pub_key,
                                          POINT_CONVERSION_UNCOMPRESSED, NULL,
                                          NULL);
        if (pubBn == NULL) {
            WOLFSSL_MSG("wolfSSL_EC_POINT_point2bn failed.");
            ret = 0;
        }
        else {
            ret = pk_bn_field_print_fp(fp, indent, "pub", pubBn);
        }
    }
    if (ret == 1) {
        nid = wolfSSL_EC_GROUP_get_curve_name(key->group);
        if (nid > 0) {
            curve = wolfSSL_OBJ_nid2ln(nid);
            if (curve != NULL) {
                if (XFPRINTF(fp, "%*s", indent, "") < 0)
                    ret = 0;
                else if (XFPRINTF(fp, "ASN1 OID: %s\n", curve) < 0)
                    ret = 0;
            }
            nistName = wolfSSL_EC_curve_nid2nist(nid);
            if (nistName != NULL) {
                if (XFPRINTF(fp, "%*s", indent, "") < 0)
                    ret = 0;
                else if (XFPRINTF(fp, "NIST CURVE: %s\n", nistName) < 0)
                    ret = 0;
            }
        }
    }

    if (pubBn != NULL) {
        wolfSSL_BN_free(pubBn);
    }

    WOLFSSL_LEAVE("wolfSSL_EC_KEY_print_fp", ret);

    return ret;
}
#endif /* XFPRINTF && !NO_FILESYSTEM && !NO_STDIO_FILESYSTEM */

#if defined(OPENSSL_ALL)

/* Copies ecc_key into new WOLFSSL_EC_KEY object
 *
 * src  : EC_KEY to duplicate. If EC_KEY is not null, create new EC_KEY and copy
 * internal ecc_key from src to dup.
 *
 * Returns pointer to duplicate EC_KEY.
 */
WOLFSSL_EC_KEY *wolfSSL_EC_KEY_dup(const WOLFSSL_EC_KEY *src)
{
    WOLFSSL_EC_KEY *newKey;
    ecc_key *key, *srcKey;
    int ret;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_dup");

    if (src == NULL || src->internal == NULL || src->group == NULL || \
       src->pub_key == NULL || src->priv_key == NULL) {

        WOLFSSL_MSG("src NULL error");
        return NULL;
    }

    newKey = wolfSSL_EC_KEY_new();
    if (newKey == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new error");
        return NULL;
    }

    key = (ecc_key*)newKey->internal;
    if (key == NULL) {
        WOLFSSL_MSG("ecc_key NULL error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }
    srcKey = (ecc_key*)src->internal;

    /* ecc_key */
    /* copy pubkey */
    ret = wc_ecc_copy_point(&srcKey->pubkey, &key->pubkey);
    if (ret != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_copy_point error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    /* copy private key k */
    ret = mp_copy(&srcKey->k, &key->k);
    if (ret != MP_OKAY) {
        WOLFSSL_MSG("mp_copy error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    /* copy domain parameters */
    if (srcKey->dp) {
        ret = wc_ecc_set_curve(key, 0, srcKey->dp->id);
        if (ret != 0) {
            WOLFSSL_MSG("wc_ecc_set_curve error");
            return NULL;
        }
    }

    key->type  = srcKey->type;
    key->idx   = srcKey->idx;
    key->state = srcKey->state;
    key->flags = srcKey->flags;

    /* Copy group */
    if (newKey->group == NULL) {
        WOLFSSL_MSG("EC_GROUP_new_by_curve_name error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    newKey->group->curve_idx = src->group->curve_idx;
    newKey->group->curve_nid = src->group->curve_nid;
    newKey->group->curve_oid = src->group->curve_oid;

    /* Copy public key */
    if (src->pub_key->internal == NULL || newKey->pub_key->internal == NULL) {
        WOLFSSL_MSG("NULL pub_key error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    /* Copy public key internal */
    ret = wc_ecc_copy_point((ecc_point*)src->pub_key->internal,
                            (ecc_point*)newKey->pub_key->internal);
    if (ret != MP_OKAY) {
        WOLFSSL_MSG("ecc_copy_point error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    /* Copy X, Y, Z */
    newKey->pub_key->X = wolfSSL_BN_dup(src->pub_key->X);
    if (!newKey->pub_key->X && src->pub_key->X) {
        WOLFSSL_MSG("Error copying EC_POINT");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }
    newKey->pub_key->Y = wolfSSL_BN_dup(src->pub_key->Y);
    if (!newKey->pub_key->Y && src->pub_key->Y) {
        WOLFSSL_MSG("Error copying EC_POINT");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }
    newKey->pub_key->Z = wolfSSL_BN_dup(src->pub_key->Z);
    if (!newKey->pub_key->Z && src->pub_key->Z) {
        WOLFSSL_MSG("Error copying EC_POINT");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    newKey->pub_key->inSet = src->pub_key->inSet;
    newKey->pub_key->exSet = src->pub_key->exSet;
    newKey->pkcs8HeaderSz = src->pkcs8HeaderSz;

    /* Copy private key */
    if (src->priv_key->internal == NULL || newKey->priv_key->internal == NULL) {
        WOLFSSL_MSG("NULL priv_key error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    /* Free priv_key before call to newKey function */
    wolfSSL_BN_free(newKey->priv_key);
    newKey->priv_key = wolfSSL_BN_dup(src->priv_key);
    if (newKey->priv_key == NULL) {
        WOLFSSL_MSG("BN_newKey error");
        wolfSSL_EC_KEY_free(newKey);
        return NULL;
    }

    return newKey;
}

#endif /* OPENSSL_ALL */

#endif /* !NO_CERTS */

#ifdef ALT_ECC_SIZE
static int SetIndividualInternalEcc(WOLFSSL_BIGNUM* bn, mp_int* mpi)
{
    WOLFSSL_MSG("Entering SetIndividualInternal");

    if (bn == NULL || bn->internal == NULL) {
        WOLFSSL_MSG("bn NULL error");
        return -1;
    }

    if (mpi == NULL) {
        WOLFSSL_MSG("mpi NULL error");
        return -1;
    }

    if (mp_copy((mp_int*)bn->internal, mpi) != MP_OKAY) {
        WOLFSSL_MSG("mp_copy error");
        return -1;
    }

    return 1;
}
#endif /* ALT_ECC_SIZE */

/* EC_POINT Openssl -> WolfSSL */
static int SetECPointInternal(WOLFSSL_EC_POINT *p)
{
    ecc_point* point;
    WOLFSSL_ENTER("SetECPointInternal");

    if (p == NULL || p->internal == NULL) {
        WOLFSSL_MSG("ECPoint NULL error");
        return -1;
    }

    point = (ecc_point*)p->internal;

#ifndef ALT_ECC_SIZE
    if (p->X != NULL && SetIndividualInternal(p->X, point->x)
                                                           != 1) {
        WOLFSSL_MSG("ecc point X error");
        return -1;
    }

    if (p->Y != NULL && SetIndividualInternal(p->Y, point->y)
                                                           != 1) {
        WOLFSSL_MSG("ecc point Y error");
        return -1;
    }

    if (p->Z != NULL && SetIndividualInternal(p->Z, point->z)
                                                           != 1) {
        WOLFSSL_MSG("ecc point Z error");
        return -1;
    }
#else
    if (p->X != NULL && SetIndividualInternalEcc(p->X, point->x)
                                                           != 1) {
        WOLFSSL_MSG("ecc point X error");
        return -1;
    }

    if (p->Y != NULL && SetIndividualInternalEcc(p->Y, point->y)
                                                           != 1) {
        WOLFSSL_MSG("ecc point Y error");
        return -1;
    }

    if (p->Z != NULL && SetIndividualInternalEcc(p->Z, point->z)
                                                           != 1) {
        WOLFSSL_MSG("ecc point Z error");
        return -1;
    }
#endif

    p->inSet = 1;

    return 1;
}

/* EC_POINT WolfSSL -> OpenSSL */
static int SetECPointExternal(WOLFSSL_EC_POINT *p)
{
    ecc_point* point;

    WOLFSSL_ENTER("SetECPointExternal");

    if (p == NULL || p->internal == NULL) {
        WOLFSSL_MSG("ECPoint NULL error");
        return -1;
    }

    point = (ecc_point*)p->internal;

    if (SetIndividualExternal(&p->X, point->x) != 1) {
        WOLFSSL_MSG("ecc point X error");
        return -1;
    }

    if (SetIndividualExternal(&p->Y, point->y) != 1) {
        WOLFSSL_MSG("ecc point Y error");
        return -1;
    }

    if (SetIndividualExternal(&p->Z, point->z) != 1) {
        WOLFSSL_MSG("ecc point Z error");
        return -1;
    }

    p->exSet = 1;

    return 1;
}


/* EC_KEY wolfSSL -> OpenSSL */
int SetECKeyExternal(WOLFSSL_EC_KEY* eckey)
{
    ecc_key* key;

    WOLFSSL_ENTER("SetECKeyExternal");

    if (eckey == NULL || eckey->internal == NULL) {
        WOLFSSL_MSG("ec key NULL error");
        return -1;
    }

    key = (ecc_key*)eckey->internal;

    /* set group (OID, nid and idx) */
    eckey->group->curve_oid = ecc_sets[key->idx].oidSum;
    eckey->group->curve_nid = EccEnumToNID(ecc_sets[key->idx].id);
    eckey->group->curve_idx = key->idx;

    if (eckey->pub_key->internal != NULL) {
        /* set the internal public key */
        if (wc_ecc_copy_point(&key->pubkey,
                             (ecc_point*)eckey->pub_key->internal) != MP_OKAY) {
            WOLFSSL_MSG("SetECKeyExternal ecc_copy_point failed");
            return -1;
        }

        /* set the external pubkey (point) */
        if (SetECPointExternal(eckey->pub_key) != 1) {
            WOLFSSL_MSG("SetECKeyExternal SetECPointExternal failed");
            return -1;
        }
    }

    /* set the external privkey */
    if (key->type == ECC_PRIVATEKEY) {
        if (SetIndividualExternal(&eckey->priv_key, &key->k) != 1) {
            WOLFSSL_MSG("ec priv key error");
            return -1;
        }
    }

    eckey->exSet = 1;

    return 1;
}

/* EC_KEY Openssl -> WolfSSL */
int SetECKeyInternal(WOLFSSL_EC_KEY* eckey)
{
    ecc_key* key;

    WOLFSSL_ENTER("SetECKeyInternal");

    if (eckey == NULL || eckey->internal == NULL || eckey->group == NULL) {
        WOLFSSL_MSG("ec key NULL error");
        return -1;
    }

    key = (ecc_key*)eckey->internal;

    /* validate group */
    if ((eckey->group->curve_idx < 0) ||
        (wc_ecc_is_valid_idx(eckey->group->curve_idx) == 0)) {
        WOLFSSL_MSG("invalid curve idx");
        return -1;
    }

    /* set group (idx of curve and corresponding domain parameters) */
    key->idx = eckey->group->curve_idx;
    key->dp = &ecc_sets[key->idx];

    /* set pubkey (point) */
    if (eckey->pub_key != NULL) {
        if (SetECPointInternal(eckey->pub_key) != 1) {
            WOLFSSL_MSG("ec key pub error");
            return -1;
        }

        /* copy over the public point to key */
        if (wc_ecc_copy_point((ecc_point*)eckey->pub_key->internal,
                                                     &key->pubkey) != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_copy_point error");
            return -1;
        }

        /* public key */
        key->type = ECC_PUBLICKEY;
    }

    /* set privkey */
    if (eckey->priv_key != NULL) {
        if (SetIndividualInternal(eckey->priv_key, &key->k)
                                                           != 1) {
            WOLFSSL_MSG("ec key priv error");
            return -1;
        }

        /* private key */
        if (!mp_iszero(&key->k))
            key->type = ECC_PRIVATEKEY;
    }

    eckey->inSet = 1;

    return 1;
}

WOLFSSL_EC_POINT *wolfSSL_EC_KEY_get0_public_key(const WOLFSSL_EC_KEY *key)
{
    WOLFSSL_ENTER("wolfSSL_EC_KEY_get0_public_key");

    if (key == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_get0_public_key Bad arguments");
        return NULL;
    }

    return key->pub_key;
}

const WOLFSSL_EC_GROUP *wolfSSL_EC_KEY_get0_group(const WOLFSSL_EC_KEY *key)
{
    WOLFSSL_ENTER("wolfSSL_EC_KEY_get0_group");

    if (key == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_get0_group Bad arguments");
        return NULL;
    }

    return key->group;
}


/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_EC_KEY_set_private_key(WOLFSSL_EC_KEY *key,
                                   const WOLFSSL_BIGNUM *priv_key)
{
    WOLFSSL_ENTER("wolfSSL_EC_KEY_set_private_key");

    if (key == NULL || priv_key == NULL) {
        WOLFSSL_MSG("Bad arguments");
        return 0;
    }

    /* free key if previously set */
    if (key->priv_key != NULL)
        wolfSSL_BN_free(key->priv_key);

    key->priv_key = wolfSSL_BN_dup(priv_key);
    if (key->priv_key == NULL) {
        WOLFSSL_MSG("key ecc priv key NULL");
        return 0;
    }

    if (SetECKeyInternal(key) != 1) {
        WOLFSSL_MSG("SetECKeyInternal failed");
        wolfSSL_BN_free(key->priv_key);
        return 0;
    }

    return 1;
}


WOLFSSL_BIGNUM *wolfSSL_EC_KEY_get0_private_key(const WOLFSSL_EC_KEY *key)
{
    WOLFSSL_ENTER("wolfSSL_EC_KEY_get0_private_key");

    if (key == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_get0_private_key Bad arguments");
        return NULL;
    }

    if (wolfSSL_BN_is_zero(key->priv_key)) {
        /* return NULL if not set */
        return NULL;
    }

    return key->priv_key;
}

WOLFSSL_EC_KEY *wolfSSL_EC_KEY_new_by_curve_name(int nid)
{
    WOLFSSL_EC_KEY *key;
    int x;
    int eccEnum = NIDToEccEnum(nid);

    WOLFSSL_ENTER("wolfSSL_EC_KEY_new_by_curve_name");

    key = wolfSSL_EC_KEY_new();
    if (key == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new failure");
        return NULL;
    }

    /* set the nid of the curve */
    key->group->curve_nid = nid;

    if (eccEnum != -1) {
        /* search and set the corresponding internal curve idx */
        for (x = 0; ecc_sets[x].size != 0; x++) {
            if (ecc_sets[x].id == eccEnum) {
                key->group->curve_idx = x;
                key->group->curve_oid = ecc_sets[x].oidSum;
                break;
            }
        }

        /* if not found, we don't support this curve. */
        if (ecc_sets[x].size == 0) {
            wolfSSL_EC_KEY_free(key);
            key = NULL;
        }
    }

    return key;
}

const char* wolfSSL_EC_curve_nid2nist(int nid)
{
    const WOLF_EC_NIST_NAME* nist_name;
    for (nist_name = kNistCurves; nist_name->name != NULL; nist_name++) {
        if (nist_name->nid == nid) {
            return nist_name->name;
        }
    }
    return NULL;
}

/**
 * return nist curve id
 * @param name nist curve name
 * @return nist curve id when found, 0 when not found
 */
int wolfSSL_EC_curve_nist2nid(const char* name)
{
    const WOLF_EC_NIST_NAME* nist_name;
    for (nist_name = kNistCurves; nist_name->name != NULL; nist_name++) {
        if (XSTRCMP(nist_name->name, name) == 0) {
            return nist_name->nid;
        }
    }
    return 0;
}

static void InitwolfSSL_ECKey(WOLFSSL_EC_KEY* key)
{
    if (key) {
        key->group    = NULL;
        key->pub_key  = NULL;
        key->priv_key = NULL;
        key->internal = NULL;
        key->inSet    = 0;
        key->exSet    = 0;
        key->form     = POINT_CONVERSION_UNCOMPRESSED;
    }
}

WOLFSSL_EC_KEY *wolfSSL_EC_KEY_new_ex(void* heap, int devId)
{
    WOLFSSL_EC_KEY *external;
    WOLFSSL_ENTER("wolfSSL_EC_KEY_new");

    external = (WOLFSSL_EC_KEY*)XMALLOC(sizeof(WOLFSSL_EC_KEY), heap,
                                        DYNAMIC_TYPE_ECC);
    if (external == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new malloc WOLFSSL_EC_KEY failure");
        return NULL;
    }
    XMEMSET(external, 0, sizeof(WOLFSSL_EC_KEY));
    external->heap = heap;

    InitwolfSSL_ECKey(external);

    external->refCount = 1;
#ifndef SINGLE_THREADED
    if (wc_InitMutex(&external->refMutex) != 0) {
        WOLFSSL_MSG("wc_InitMutex WOLFSSL_EC_KEY failure");
        XFREE(external, heap, DYNAMIC_TYPE_ECC);
        return NULL;
    }
#endif

    external->internal = (ecc_key*)XMALLOC(sizeof(ecc_key), heap,
                                           DYNAMIC_TYPE_ECC);
    if (external->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new malloc ecc key failure");
        goto error;
    }
    XMEMSET(external->internal, 0, sizeof(ecc_key));

    if (wc_ecc_init_ex((ecc_key*)external->internal, heap, devId) != 0) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new init ecc key failure");
        goto error;
    }

    /* Group unknown at creation */
    external->group = wolfSSL_EC_GROUP_new_by_curve_name(NID_undef);
    if (external->group == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new malloc WOLFSSL_EC_GROUP failure");
        goto error;
    }

    /* public key */
    external->pub_key = wolfSSL_EC_POINT_new(external->group);
    if (external->pub_key == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_new failure");
        goto error;
    }

    /* private key */
    external->priv_key = wolfSSL_BN_new();
    if (external->priv_key == NULL) {
        WOLFSSL_MSG("wolfSSL_BN_new failure");
        goto error;
    }

    return external;
error:
    wolfSSL_EC_KEY_free(external);
    return NULL;
}

WOLFSSL_EC_KEY *wolfSSL_EC_KEY_new(void)
{
    return wolfSSL_EC_KEY_new_ex(NULL, INVALID_DEVID);
}

void wolfSSL_EC_KEY_free(WOLFSSL_EC_KEY *key)
{
    int doFree = 0;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_free");

    if (key != NULL) {
        void* heap = key->heap;

    #ifndef SINGLE_THREADED
        if (wc_LockMutex(&key->refMutex) != 0) {
            WOLFSSL_MSG("Could not lock EC_KEY mutex");
            return;
        }
    #endif

        /* only free if all references to it are done */
        key->refCount--;
        if (key->refCount == 0) {
            doFree = 1;
        }
    #ifndef SINGLE_THREADED
        wc_UnLockMutex(&key->refMutex);
    #endif

        if (doFree == 0) {
            return;
        }

    #ifndef SINGLE_THREADED
        wc_FreeMutex(&key->refMutex);
    #endif

        if (key->internal != NULL) {
            wc_ecc_free((ecc_key*)key->internal);
            XFREE(key->internal, heap, DYNAMIC_TYPE_ECC);
        }
        wolfSSL_BN_free(key->priv_key);
        wolfSSL_EC_POINT_free(key->pub_key);
        wolfSSL_EC_GROUP_free(key->group);
        InitwolfSSL_ECKey(key); /* set back to NULLs for safety */

        XFREE(key, heap, DYNAMIC_TYPE_ECC);
        (void)heap;
        /* key = NULL, don't try to access or double free it */
    }
}

/* Increments ref count of WOLFSSL_EC_KEY.
 * Return 1 on success, 0 on error */
int wolfSSL_EC_KEY_up_ref(WOLFSSL_EC_KEY* key)
{
    if (key) {
    #ifndef SINGLE_THREADED
        if (wc_LockMutex(&key->refMutex) != 0) {
            WOLFSSL_MSG("Failed to lock EC_KEY mutex");
        }
    #endif
        key->refCount++;
    #ifndef SINGLE_THREADED
        wc_UnLockMutex(&key->refMutex);
    #endif
        return 1;
    }

    return 0;
}

/* set the group in WOLFSSL_EC_KEY and return 1 on success */
int wolfSSL_EC_KEY_set_group(WOLFSSL_EC_KEY *key, WOLFSSL_EC_GROUP *group)
{
    if (key == NULL || group == NULL)
        return 0;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_set_group");

    if (key->group != NULL) {
        /* free the current group */
        wolfSSL_EC_GROUP_free(key->group);
    }

    key->group = wolfSSL_EC_GROUP_dup(group);
    if (key->group == NULL) {
        return 0;
    }

    return 1;
}


int wolfSSL_EC_KEY_generate_key(WOLFSSL_EC_KEY *key)
{
    int     initTmpRng = 0;
    int     eccEnum;
    WC_RNG* rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif
    int ret;
    ecc_key* ecKey;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_generate_key");

    if (key == NULL || key->internal == NULL ||
        key->group == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_generate_key Bad arguments");
        return 0;
    }
    if (key->group->curve_idx < 0) {
        /* generate key using the default curve */
        /* group should be set, but to retain compat use index 0 */
        key->group->curve_idx = ECC_CURVE_DEF;
    }

#ifdef WOLFSSL_SMALL_STACK
    tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
    if (tmpRng == NULL)
        return 0;
#endif

    if (wc_InitRng(tmpRng) == 0) {
        rng = tmpRng;
        initTmpRng = 1;
    }
    else {
        WOLFSSL_MSG("Bad RNG Init, trying global");
        rng = wolfssl_get_global_rng();
    }

    if (rng == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_generate_key failed to set RNG");
#ifdef WOLFSSL_SMALL_STACK
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif
        return 0;
    }

    /* NIDToEccEnum returns -1 for invalid NID so if key->group->curve_nid
     * is 0 then pass ECC_CURVE_DEF as arg */
    ecKey = (ecc_key*)key->internal;
    eccEnum = key->group->curve_nid ?
            NIDToEccEnum(key->group->curve_nid) : ECC_CURVE_DEF;
    ret = wc_ecc_make_key_ex(rng, 0, ecKey, eccEnum);
#if defined(WOLFSSL_ASYNC_CRYPT)
    ret = wc_AsyncWait(ret, &ecKey->asyncDev, WC_ASYNC_FLAG_NONE);
#endif

    if (ret != 0) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_generate_key wc_ecc_make_key failed");
#ifdef WOLFSSL_SMALL_STACK
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif
        return 0;
    }

    if (initTmpRng)
        wc_FreeRng(tmpRng);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    if (SetECKeyExternal(key) != 1) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_generate_key SetECKeyExternal failed");
        return 0;
    }

    return 1;
}

#ifndef NO_WOLFSSL_STUB
void wolfSSL_EC_KEY_set_asn1_flag(WOLFSSL_EC_KEY *key, int asn1_flag)
{
    (void)key;
    (void)asn1_flag;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_set_asn1_flag");
    WOLFSSL_STUB("EC_KEY_set_asn1_flag");
}
#endif

static int setupPoint(const WOLFSSL_EC_POINT *p) {
    if (!p) {
        return 0;
    }
    if (p->inSet == 0) {
        WOLFSSL_MSG("No ECPoint internal set, do it");

        if (SetECPointInternal((WOLFSSL_EC_POINT *)p) != 1) {
            WOLFSSL_MSG("SetECPointInternal SetECPointInternal failed");
            return 0;
        }
    }
    return 1;
}

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_EC_KEY_set_public_key(WOLFSSL_EC_KEY *key,
                                  const WOLFSSL_EC_POINT *pub)
{
    ecc_point *pub_p, *key_p;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_set_public_key");

    if (key == NULL || key->internal == NULL ||
        pub == NULL || pub->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_set_public_key Bad arguments");
        return 0;
    }

    if (key->inSet == 0) {
        if (SetECKeyInternal(key) != 1) {
            WOLFSSL_MSG("SetECKeyInternal failed");
            return 0;
        }
    }

    if (setupPoint(pub) != 1) {
        return 0;
    }

    pub_p = (ecc_point*)pub->internal;
    key_p = (ecc_point*)key->pub_key->internal;

    /* create new point if required */
    if (key_p == NULL)
        key_p = wc_ecc_new_point();

    if (key_p == NULL) {
        WOLFSSL_MSG("key ecc point NULL");
        return 0;
    }

    if (wc_ecc_copy_point(pub_p, key_p) != MP_OKAY) {
        WOLFSSL_MSG("ecc_copy_point failure");
        return 0;
    }

    if (SetECPointExternal(key->pub_key) != 1) {
        WOLFSSL_MSG("SetECKeyInternal failed");
        return 0;
    }

    if (SetECKeyInternal(key) != 1) {
        WOLFSSL_MSG("SetECKeyInternal failed");
        return 0;
    }

    wolfSSL_EC_POINT_dump("pub", pub);
    wolfSSL_EC_POINT_dump("key->pub_key", key->pub_key);

    return 1;
}

int wolfSSL_EC_KEY_check_key(const WOLFSSL_EC_KEY *key)
{
    WOLFSSL_ENTER("wolfSSL_EC_KEY_check_key");

    if (key == NULL || key->internal == NULL) {
        WOLFSSL_MSG("Bad parameter");
        return 0;
    }

    if (key->inSet == 0) {
        if (SetECKeyInternal((WOLFSSL_EC_KEY*)key) != 1) {
            WOLFSSL_MSG("SetECKeyInternal failed");
            return 0;
        }
    }

    return wc_ecc_check_key((ecc_key*)key->internal) == 0 ?
            1 : 0;
}
/* End EC_KEY */

/* Calculate and return maximum size of the ECDSA signature for the curve */
int wolfSSL_ECDSA_size(const WOLFSSL_EC_KEY *key)
{
    const EC_GROUP *group;
    int bits, bytes;
    word32 headerSz = SIG_HEADER_SZ; /* 2*ASN_TAG + 2*LEN(ENUM) */

    if (key == NULL) {
        return 0;
    }

    if ((group = wolfSSL_EC_KEY_get0_group(key)) == NULL) {
        return 0;
    }
    if ((bits = wolfSSL_EC_GROUP_order_bits(group)) == 0) {
        /* group is not set */
        return 0;
    }

    bytes = (bits + 7) / 8;  /* bytes needed to hold bits */
    return headerSz +
            ECC_MAX_PAD_SZ + /* possible leading zeroes in r and s */
            bytes + bytes;   /* r and s */
}

int wolfSSL_ECDSA_sign(int type,
    const unsigned char *digest, int digestSz,
    unsigned char *sig, unsigned int *sigSz, WOLFSSL_EC_KEY *key)
{
    int ret = 1;
    WC_RNG* rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
#else
    WC_RNG  tmpRng[1];
#endif
    int initTmpRng = 0;

    WOLFSSL_ENTER("wolfSSL_ECDSA_sign");

    if (!key) {
        return 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
    if (tmpRng == NULL)
        return 0;
#endif

    if (wc_InitRng(tmpRng) == 0) {
        rng = tmpRng;
        initTmpRng = 1;
    }
    else {
        WOLFSSL_MSG("Bad RNG Init, trying global");
        rng = wolfssl_get_global_rng();
    }
    if (rng) {
        if (wc_ecc_sign_hash(digest, digestSz, sig, sigSz, rng,
                (ecc_key*)key->internal) != 0) {
            ret = 0;
        }
        if (initTmpRng) {
            wc_FreeRng(tmpRng);
        }
    } else {
        ret = 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    if (tmpRng)
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    (void)type;
    return ret;
}

int wolfSSL_ECDSA_verify(int type,
    const unsigned char *digest, int digestSz,
    const unsigned char *sig, int sigSz, WOLFSSL_EC_KEY *key)
{
    int ret = 1;
    int verify = 0;

    WOLFSSL_ENTER("wolfSSL_ECDSA_verify");

    if (key == NULL) {
        return 0;
    }

    if (wc_ecc_verify_hash(sig, sigSz, digest, digestSz,
            &verify, (ecc_key*)key->internal) != 0) {
        ret = 0;
    }
    if (ret == 1 && verify != 1) {
        WOLFSSL_MSG("wolfSSL_ECDSA_verify failed");
        ret = 0;
    }

    (void)type;
    return ret;
}

#ifndef HAVE_SELFTEST
/* ECC point compression types were not included in selftest ecc.h */

char* wolfSSL_EC_POINT_point2hex(const WOLFSSL_EC_GROUP* group,
                                 const WOLFSSL_EC_POINT* point, int form,
                                 WOLFSSL_BN_CTX* ctx)
{
    static const char* hexDigit = "0123456789ABCDEF";
    char* hex = NULL;
    int id;
    int i, sz, len;

    (void)ctx;

    if (group == NULL || point == NULL)
        return NULL;

    id = wc_ecc_get_curve_id(group->curve_idx);

    if ((sz = wc_ecc_get_curve_size_from_id(id)) < 0)
        return NULL;

    len = sz + 1;
    if (form == POINT_CONVERSION_UNCOMPRESSED)
        len += sz;
    hex = (char*)XMALLOC(2 * len + 1, NULL, DYNAMIC_TYPE_ECC);
    if (hex == NULL)
        return NULL;
    XMEMSET(hex, 0, 2 * len + 1);

    /* Put in x-ordinate after format byte. */
    i = sz - mp_unsigned_bin_size((mp_int*)point->X->internal) + 1;
    if (mp_to_unsigned_bin((mp_int*)point->X->internal, (byte*)(hex + i)) < 0) {
        XFREE(hex,  NULL, DYNAMIC_TYPE_ECC);
        return NULL;
    }

    if (form == POINT_CONVERSION_COMPRESSED) {
        hex[0] = mp_isodd((mp_int*)point->Y->internal) ? ECC_POINT_COMP_ODD :
                                                         ECC_POINT_COMP_EVEN;
    }
    else {
        hex[0] = ECC_POINT_UNCOMP;
        /* Put in y-ordinate after x-ordinate */
        i = 1 + 2 * sz - mp_unsigned_bin_size((mp_int*)point->Y->internal);
        if (mp_to_unsigned_bin((mp_int*)point->Y->internal,
                                                        (byte*)(hex + i)) < 0) {
            XFREE(hex,  NULL, DYNAMIC_TYPE_ECC);
            return NULL;
        }
    }

    for (i = len-1; i >= 0; i--) {
        byte b = hex[i];
        hex[i * 2 + 1] = hexDigit[b  & 0xf];
        hex[i * 2    ] = hexDigit[b >>   4];
    }

    return hex;
}

#endif /* HAVE_SELFTEST */

void wolfSSL_EC_POINT_dump(const char *msg, const WOLFSSL_EC_POINT *p)
{
#if defined(DEBUG_WOLFSSL)
    char *num;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_dump");

    if (!WOLFSSL_IS_DEBUG_ON() || wolfSSL_GetLoggingCb()) {
        return;
    }

    if (p == NULL) {
        printf("%s = NULL", msg);
        return;
    }

    printf("%s:\n\tinSet=%d, exSet=%d\n", msg, p->inSet, p->exSet);
    num = wolfSSL_BN_bn2hex(p->X);
    printf("\tX = %s\n", num);
    XFREE(num, NULL, DYNAMIC_TYPE_OPENSSL);
    num = wolfSSL_BN_bn2hex(p->Y);
    printf("\tY = %s\n", num);
    XFREE(num, NULL, DYNAMIC_TYPE_OPENSSL);
    num = wolfSSL_BN_bn2hex(p->Z);
    printf("\tZ = %s\n", num);
    XFREE(num, NULL, DYNAMIC_TYPE_OPENSSL);
#else
    (void)msg;
    (void)p;
#endif
}

/* Start EC_GROUP */

/* return code compliant with OpenSSL :
 *   0 if equal, 1 if not and -1 in case of error
 */
int wolfSSL_EC_GROUP_cmp(const WOLFSSL_EC_GROUP *a, const WOLFSSL_EC_GROUP *b,
                         WOLFSSL_BN_CTX *ctx)
{
    (void)ctx;

    WOLFSSL_ENTER("wolfSSL_EC_GROUP_cmp");

    if (a == NULL || b == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_cmp Bad arguments");
        return -1;
    }

    /* ok */
    if ((a->curve_idx == b->curve_idx) && (a->curve_nid == b->curve_nid))
        return 0;

    /* ko */
    return 1;
}

WOLFSSL_EC_GROUP *wolfSSL_EC_GROUP_dup(const WOLFSSL_EC_GROUP *src)
{
    if (!src)
        return NULL;
    return wolfSSL_EC_GROUP_new_by_curve_name(src->curve_nid);
}

#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
const WOLFSSL_EC_METHOD* wolfSSL_EC_GROUP_method_of(
                                                const WOLFSSL_EC_GROUP *group)
{
    return group;
}

int wolfSSL_EC_METHOD_get_field_type(const WOLFSSL_EC_METHOD *meth)
{
    if (meth) {
        return NID_X9_62_prime_field;
    }
    return 0;
}

void wolfSSL_EC_GROUP_free(WOLFSSL_EC_GROUP *group)
{
    WOLFSSL_ENTER("wolfSSL_EC_GROUP_free");

    XFREE(group, NULL, DYNAMIC_TYPE_ECC);
    /* group = NULL, don't try to access or double free it */
}
#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA
#ifndef NO_WOLFSSL_STUB
void wolfSSL_EC_GROUP_set_asn1_flag(WOLFSSL_EC_GROUP *group, int flag)
{
    (void)group;
    (void)flag;

    WOLFSSL_ENTER("wolfSSL_EC_GROUP_set_asn1_flag");
    WOLFSSL_STUB("EC_GROUP_set_asn1_flag");
}
#endif

/* return code compliant with OpenSSL :
 *   the curve nid if success, 0 if error
 */
int wolfSSL_EC_GROUP_get_curve_name(const WOLFSSL_EC_GROUP *group)
{
    int nid;
    WOLFSSL_ENTER("wolfSSL_EC_GROUP_get_curve_name");

    if (group == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_get_curve_name Bad arguments");
        return 0;
    }

    /* If curve_nid is ECC Enum type, return corresponding OpenSSL nid */
    if ((nid = EccEnumToNID(group->curve_nid)) != -1)
        return nid;

    return group->curve_nid;
}

/* return code compliant with OpenSSL :
 *   the degree of the curve if success, 0 if error
 */
int wolfSSL_EC_GROUP_get_degree(const WOLFSSL_EC_GROUP *group)
{
    int nid;
    int tmp;

    WOLFSSL_ENTER("wolfSSL_EC_GROUP_get_degree");

    if (group == NULL || group->curve_idx < 0) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_get_degree Bad arguments");
        return 0;
    }

    /* If curve_nid passed in is an ecc_curve_id enum, convert it to the
        corresponding OpenSSL NID */
    tmp = EccEnumToNID(group->curve_nid);
    if (tmp != -1) {
        nid = tmp;
    }
    else {
        nid = group->curve_nid;
    }

    switch(nid) {
        case NID_secp112r1:
        case NID_secp112r2:
            return 112;
        case NID_secp128r1:
        case NID_secp128r2:
            return 128;
        case NID_secp160k1:
        case NID_secp160r1:
        case NID_secp160r2:
        case NID_brainpoolP160r1:
            return 160;
        case NID_secp192k1:
        case NID_brainpoolP192r1:
        case NID_X9_62_prime192v1:
            return 192;
        case NID_secp224k1:
        case NID_secp224r1:
        case NID_brainpoolP224r1:
            return 224;
        case NID_secp256k1:
        case NID_brainpoolP256r1:
        case NID_X9_62_prime256v1:
            return 256;
        case NID_brainpoolP320r1:
            return 320;
        case NID_secp384r1:
        case NID_brainpoolP384r1:
            return 384;
        case NID_secp521r1:
            return 521;
        case NID_brainpoolP512r1:
            return 512;
        default:
            return 0;
    }
}
#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(WOLFSSL_WPAS_SMALL)
WOLFSSL_EC_GROUP *wolfSSL_EC_GROUP_new_by_curve_name(int nid)
{
    WOLFSSL_EC_GROUP *g;
    int x, eccEnum;

    WOLFSSL_ENTER("wolfSSL_EC_GROUP_new_by_curve_name");

    /* curve group */
    g = (WOLFSSL_EC_GROUP*)XMALLOC(sizeof(WOLFSSL_EC_GROUP), NULL,
                                    DYNAMIC_TYPE_ECC);
    if (g == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_new_by_curve_name malloc failure");
        return NULL;
    }
    XMEMSET(g, 0, sizeof(WOLFSSL_EC_GROUP));

    /* set the nid of the curve */
    g->curve_nid = nid;
    g->curve_idx = -1;

    /* If NID passed in is OpenSSL type, convert it to ecc_curve_id enum */
    eccEnum = NIDToEccEnum(nid);
    if (eccEnum != -1) {
        /* search and set the corresponding internal curve idx */
        for (x = 0; ecc_sets[x].size != 0; x++) {
            if (ecc_sets[x].id == eccEnum) {
                g->curve_idx = x;
                g->curve_oid = ecc_sets[x].oidSum;
                break;
            }
        }
    }

    return g;
}

/* Converts OpenSSL NID value of ECC curves to the associated enum values in
   ecc_curve_id, used by ecc_sets[].*/
int NIDToEccEnum(int n)
{
    WOLFSSL_ENTER("NIDToEccEnum()");

    switch(n) {
        case NID_X9_62_prime192v1:
            return ECC_SECP192R1;
        case NID_X9_62_prime192v2:
            return ECC_PRIME192V2;
        case NID_X9_62_prime192v3:
            return ECC_PRIME192V3;
        case NID_X9_62_prime239v1:
            return ECC_PRIME239V1;
        case NID_X9_62_prime239v2:
            return ECC_PRIME239V2;
        case NID_X9_62_prime239v3:
            return ECC_PRIME239V3;
        case NID_X9_62_prime256v1:
            return ECC_SECP256R1;
        case NID_secp112r1:
            return ECC_SECP112R1;
        case NID_secp112r2:
            return ECC_SECP112R2;
        case NID_secp128r1:
            return ECC_SECP128R1;
        case NID_secp128r2:
            return ECC_SECP128R2;
        case NID_secp160r1:
            return ECC_SECP160R1;
        case NID_secp160r2:
            return ECC_SECP160R2;
        case NID_secp224r1:
            return ECC_SECP224R1;
        case NID_secp384r1:
            return ECC_SECP384R1;
        case NID_secp521r1:
            return ECC_SECP521R1;
        case NID_secp160k1:
            return ECC_SECP160K1;
        case NID_secp192k1:
            return ECC_SECP192K1;
        case NID_secp224k1:
            return ECC_SECP224K1;
        case NID_secp256k1:
            return ECC_SECP256K1;
        case NID_brainpoolP160r1:
            return ECC_BRAINPOOLP160R1;
        case NID_brainpoolP192r1:
            return ECC_BRAINPOOLP192R1;
        case NID_brainpoolP224r1:
            return ECC_BRAINPOOLP224R1;
        case NID_brainpoolP256r1:
            return ECC_BRAINPOOLP256R1;
        case NID_brainpoolP320r1:
            return ECC_BRAINPOOLP320R1;
        case NID_brainpoolP384r1:
            return ECC_BRAINPOOLP384R1;
        case NID_brainpoolP512r1:
            return ECC_BRAINPOOLP512R1;
        default:
            WOLFSSL_MSG("NID not found");
            return -1;
    }
}

int wolfSSL_EC_GROUP_order_bits(const WOLFSSL_EC_GROUP *group)
{
    int ret = 0;
#ifdef WOLFSSL_SMALL_STACK
    mp_int *order = (mp_int *)XMALLOC(sizeof(*order), NULL,
                                      DYNAMIC_TYPE_TMP_BUFFER);
    if (order == NULL)
        return 0;
#else
    mp_int order[1];
#endif

    if (group == NULL || group->curve_idx < 0) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_order_bits NULL error");
        ret = -1;
    }

    if (ret == 0)
        ret = mp_init(order);

    if (ret == 0) {
        ret = mp_read_radix(order, ecc_sets[group->curve_idx].order,
            MP_RADIX_HEX);
        if (ret == 0)
            ret = mp_count_bits(order);
        mp_clear(order);
    }

#ifdef WOLFSSL_SMALL_STACK
    XFREE(order, NULL, DYNAMIC_TYPE_TMP_BUFFER);
#endif

    if (ret == -1)
        ret = 0;

    return ret;
}
#endif /* OPENSSL_EXTRA || WOLFSSL_WPAS_SMALL */

#if defined(OPENSSL_EXTRA)
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_EC_GROUP_get_order(const WOLFSSL_EC_GROUP *group,
                               WOLFSSL_BIGNUM *order, WOLFSSL_BN_CTX *ctx)
{
    (void)ctx;

    if (group == NULL || order == NULL || order->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_get_order NULL error");
        return 0;
    }

    if (mp_init((mp_int*)order->internal) != MP_OKAY) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_get_order mp_init failure");
        return 0;
    }

    if (mp_read_radix((mp_int*)order->internal,
                  ecc_sets[group->curve_idx].order, MP_RADIX_HEX) != MP_OKAY) {
        WOLFSSL_MSG("wolfSSL_EC_GROUP_get_order mp_read order failure");
        mp_clear((mp_int*)order->internal);
        return 0;
    }

    return 1;
}

/* End EC_GROUP */

/* Start EC_POINT */

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_ECPoint_i2d(const WOLFSSL_EC_GROUP *group,
                        const WOLFSSL_EC_POINT *p,
                        unsigned char *out, unsigned int *len)
{
    int err;

    WOLFSSL_ENTER("wolfSSL_ECPoint_i2d");

    if (group == NULL || p == NULL || len == NULL) {
        WOLFSSL_MSG("wolfSSL_ECPoint_i2d NULL error");
        return 0;
    }

    if (setupPoint(p) != 1) {
        return 0;
    }

    if (out != NULL) {
        wolfSSL_EC_POINT_dump("i2d p", p);
    }

    err = wc_ecc_export_point_der(group->curve_idx, (ecc_point*)p->internal,
                                  out, len);
    if (err != MP_OKAY && !(out == NULL && err == LENGTH_ONLY_E)) {
        WOLFSSL_MSG("wolfSSL_ECPoint_i2d wc_ecc_export_point_der failed");
        return 0;
    }

    return 1;
}

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_ECPoint_d2i(unsigned char *in, unsigned int len,
                        const WOLFSSL_EC_GROUP *group, WOLFSSL_EC_POINT *p)
{
    WOLFSSL_ENTER("wolfSSL_ECPoint_d2i");

    if (group == NULL || p == NULL || p->internal == NULL || in == NULL) {
        WOLFSSL_MSG("wolfSSL_ECPoint_d2i NULL error");
        return 0;
    }

#if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
    if (wc_ecc_import_point_der_ex(in, len, group->curve_idx,
                                   (ecc_point*)p->internal, 0) != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_import_point_der_ex failed");
       return 0;
    }
#else
    /* ECC_POINT_UNCOMP is not defined CAVP self test so use magic number */
    if (in[0] == 0x04) {
        if (wc_ecc_import_point_der(in, len, group->curve_idx,
                                    (ecc_point*)p->internal) != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_import_point_der failed");
            return 0;
        }
    }
    else {
        WOLFSSL_MSG("Only uncompressed points supported with HAVE_SELFTEST");
        return 0;
    }
#endif

    /* Set new external point */
    if (SetECPointExternal(p) != 1) {
        WOLFSSL_MSG("SetECPointExternal failed");
        return 0;
    }

    wolfSSL_EC_POINT_dump("d2i p", p);

    return 1;
}

size_t wolfSSL_EC_POINT_point2oct(const WOLFSSL_EC_GROUP *group,
                                  const WOLFSSL_EC_POINT *p,
                                  char form,
                                  byte *buf, size_t len, WOLFSSL_BN_CTX *ctx)
{
    word32 min_len = (word32)len;
#if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
    int compressed = form == POINT_CONVERSION_COMPRESSED ? 1 : 0;
#endif /* !HAVE_SELFTEST */

    WOLFSSL_ENTER("EC_POINT_point2oct");

    if (!group || !p) {
        return 0;
    }

    if (setupPoint(p) != 1) {
        return 0;
    }

    if (wolfSSL_EC_POINT_is_at_infinity(group, p)) {
        /* encodes to a single 0 octet */
        if (buf != NULL) {
            if (len < 1) {
                ECerr(EC_F_EC_GFP_SIMPLE_POINT2OCT, EC_R_BUFFER_TOO_SMALL);
                return 0;
            }
            buf[0] = 0;
        }
        return 1;
    }

    if (form != POINT_CONVERSION_UNCOMPRESSED
#ifndef HAVE_SELFTEST
            && form != POINT_CONVERSION_COMPRESSED
#endif /* !HAVE_SELFTEST */
            ) {
        WOLFSSL_MSG("Unsupported curve form");
        return 0;
    }

#if !defined(HAVE_SELFTEST) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
    if (wc_ecc_export_point_der_ex(group->curve_idx, (ecc_point*)p->internal,
               buf, &min_len, compressed) != (buf ? MP_OKAY : LENGTH_ONLY_E)) {
        return 0;
    }
#else
    if (wc_ecc_export_point_der(group->curve_idx, (ecc_point*)p->internal,
                                buf, &min_len) != (buf ? MP_OKAY : LENGTH_ONLY_E)) {
        return 0;
    }
#endif /* !HAVE_SELFTEST */

    (void)ctx;

    return (size_t)min_len;
}


int wolfSSL_EC_POINT_oct2point(const WOLFSSL_EC_GROUP *group,
                               WOLFSSL_EC_POINT *p, const unsigned char *buf,
                               size_t len, WOLFSSL_BN_CTX *ctx)
{
    WOLFSSL_ENTER("wolfSSL_EC_POINT_oct2point");

    if (!group || !p) {
        return 0;
    }

    (void)ctx;

    return wolfSSL_ECPoint_d2i((unsigned char*)buf, (unsigned int)len, group, p);
}


WOLFSSL_EC_KEY *wolfSSL_o2i_ECPublicKey(WOLFSSL_EC_KEY **a, const unsigned char **in,
                                        long len)
{
    WOLFSSL_EC_KEY* ret;

    WOLFSSL_ENTER("wolfSSL_o2i_ECPublicKey");

    if (!a || !*a || !(*a)->group || !in || !*in || len <= 0) {
        WOLFSSL_MSG("wolfSSL_o2i_ECPublicKey Bad arguments");
        return NULL;
    }

    ret = *a;

    if (wolfSSL_EC_POINT_oct2point(ret->group, ret->pub_key, *in, len, NULL)
            != 1) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_oct2point error");
        return NULL;
    }

    *in += len;
    return ret;
}

int wolfSSL_i2o_ECPublicKey(const WOLFSSL_EC_KEY *in, unsigned char **out)
{
    size_t len;
    unsigned char *tmp = NULL;
    char form;
    WOLFSSL_ENTER("wolfSSL_i2o_ECPublicKey");

    if (!in) {
        WOLFSSL_MSG("wolfSSL_i2o_ECPublicKey Bad arguments");
        return 0;
    }

    if (!in->exSet) {
        if (SetECKeyExternal((WOLFSSL_EC_KEY*)in) != 1) {
            WOLFSSL_MSG("SetECKeyExternal failure");
            return 0;
        }
    }

#ifdef HAVE_COMP_KEY
    /* Default to compressed form if not set */
    form = in->form == POINT_CONVERSION_UNCOMPRESSED ?
            POINT_CONVERSION_UNCOMPRESSED:
            POINT_CONVERSION_COMPRESSED;
#else
    form = POINT_CONVERSION_UNCOMPRESSED;
#endif

    len = wolfSSL_EC_POINT_point2oct(in->group, in->pub_key, form,
                                     NULL, 0, NULL);

    if (len != 0 && out) {
        if (!*out) {
            if (!(tmp = (unsigned char*)XMALLOC(len, NULL,
                                                DYNAMIC_TYPE_OPENSSL))) {
                WOLFSSL_MSG("malloc failed");
                return 0;
            }
            *out = tmp;
        }

        if (wolfSSL_EC_POINT_point2oct(in->group, in->pub_key, form, *out,
                                       len, NULL) == 0) {
            if (tmp) {
                XFREE(tmp, NULL, DYNAMIC_TYPE_OPENSSL);
                *out = NULL;
            }
            return 0;
        }

        if (!tmp) {
            /* Move buffer forward if it was not alloced in this function */
            *out += len;
        }
    }

    return (int)len;
}

#ifdef HAVE_ECC_KEY_IMPORT
WOLFSSL_EC_KEY *wolfSSL_d2i_ECPrivateKey(WOLFSSL_EC_KEY **key, const unsigned char **in,
                                         long len)
{
    word32 idx = 0;
    WOLFSSL_EC_KEY *eckey = NULL;
    WOLFSSL_ENTER("wolfSSL_d2i_ECPrivateKey");

    if (!in || !*in || len <= 0) {
        WOLFSSL_MSG("wolfSSL_d2i_ECPrivateKey Bad arguments");
        return NULL;
    }

    if (!(eckey = wolfSSL_EC_KEY_new())) {
        WOLFSSL_MSG("wolfSSL_EC_KEY_new error");
        return NULL;
    }

    if (wc_EccPrivateKeyDecode(*in, &idx, (ecc_key*)eckey->internal,
            (word32)len) != 0) {
        WOLFSSL_MSG("wc_EccPrivateKeyDecode error");
        goto error;
    }

    eckey->inSet = 1;

    if (SetECKeyExternal(eckey) != 1) {
        WOLFSSL_MSG("SetECKeyExternal error");
        goto error;
    }

    if (key) {
        *key = eckey;
    }

    return eckey;

error:
    wolfSSL_EC_KEY_free(eckey);
    return NULL;
}
#endif /* HAVE_ECC_KEY_IMPORT */

int wolfSSL_i2d_ECPrivateKey(const WOLFSSL_EC_KEY *in, unsigned char **out)
{
    word32 len;
    byte* buf = NULL;
    WOLFSSL_ENTER("wolfSSL_i2d_ECPrivateKey");

    if (!in) {
        WOLFSSL_MSG("wolfSSL_i2d_ECPrivateKey Bad arguments");
        return 0;
    }

    if (!in->inSet && SetECKeyInternal(
            (WOLFSSL_EC_KEY*)in) != 1) {
        WOLFSSL_MSG("SetECKeyInternal error");
        return 0;
    }

    if ((len = wc_EccKeyDerSize((ecc_key*)in->internal, 0)) <= 0) {
        WOLFSSL_MSG("wc_EccKeyDerSize error");
        return 0;
    }

    if (out) {
        if (!(buf = (byte*)XMALLOC(len, NULL, DYNAMIC_TYPE_TMP_BUFFER))) {
            WOLFSSL_MSG("tmp buffer malloc error");
            return 0;
        }

        if (wc_EccPrivateKeyToDer((ecc_key*)in->internal, buf, len) < 0) {
            WOLFSSL_MSG("wc_EccPrivateKeyToDer error");
            XFREE(buf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            return 0;
        }

        if (*out) {
            XMEMCPY(*out, buf, len);
            XFREE(buf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        }
        else {
            *out = buf;
        }
    }

    return (int)len;
}

void wolfSSL_EC_KEY_set_conv_form(WOLFSSL_EC_KEY *eckey, char form)
{
    if (eckey && (form == POINT_CONVERSION_UNCOMPRESSED
#ifdef HAVE_COMP_KEY
                  || form == POINT_CONVERSION_COMPRESSED
#endif
                  )) {
        eckey->form = form;
    } else {
        WOLFSSL_MSG("Incorrect form or HAVE_COMP_KEY not compiled in");
    }
}

point_conversion_form_t wolfSSL_EC_KEY_get_conv_form(const WOLFSSL_EC_KEY* key)
{
    if (key != NULL) {
        return key->form;
    }

    return -1;
}

/* wolfSSL_EC_POINT_point2bn should return "in" if not null */
WOLFSSL_BIGNUM *wolfSSL_EC_POINT_point2bn(const WOLFSSL_EC_GROUP *group,
    const WOLFSSL_EC_POINT *p, char form, WOLFSSL_BIGNUM *in,
    WOLFSSL_BN_CTX *ctx)
{
    size_t len;
    byte *buf;
    WOLFSSL_BIGNUM *ret = NULL;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_oct2point");

    if (!group || !p) {
        return NULL;
    }

    if ((len = wolfSSL_EC_POINT_point2oct(group, p, form,
                                          NULL, 0, ctx)) == 0) {
        return NULL;
    }

    if (!(buf = (byte*)XMALLOC(len, NULL, DYNAMIC_TYPE_TMP_BUFFER))) {
        WOLFSSL_MSG("malloc failed");
        return NULL;
    }

    if (wolfSSL_EC_POINT_point2oct(group, p, form,
                                   buf, len, ctx) == len) {
        ret = wolfSSL_BN_bin2bn(buf, (int)len, in);
    }

    XFREE(buf, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    return ret;
}

#if defined(USE_ECC_B_PARAM) && (!defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0))
int wolfSSL_EC_POINT_is_on_curve(const WOLFSSL_EC_GROUP *group,
                                 const WOLFSSL_EC_POINT *point,
                                 WOLFSSL_BN_CTX *ctx)
{
    (void)ctx;
    WOLFSSL_ENTER("wolfSSL_EC_POINT_is_on_curve");

    if (!group || !point) {
        WOLFSSL_MSG("Invalid arguments");
        return 0;
    }

    if (!point->inSet && SetECPointInternal((WOLFSSL_EC_POINT*)point)) {
        WOLFSSL_MSG("SetECPointInternal error");
        return 0;
    }

    return wc_ecc_point_is_on_curve((ecc_point*)point->internal,
        group->curve_idx)
            == MP_OKAY ? 1 : 0;
}
#endif /* USE_ECC_B_PARAM && !(FIPS_VERSION <= 2) */

WOLFSSL_EC_POINT *wolfSSL_EC_POINT_new(const WOLFSSL_EC_GROUP *group)
{
    WOLFSSL_EC_POINT *p;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_new");

    if (group == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_new NULL error");
        return NULL;
    }

    p = (WOLFSSL_EC_POINT *)XMALLOC(sizeof(WOLFSSL_EC_POINT), NULL,
                                    DYNAMIC_TYPE_ECC);
    if (p == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_new malloc ecc point failure");
        return NULL;
    }
    XMEMSET(p, 0, sizeof(WOLFSSL_EC_POINT));

    p->internal = wc_ecc_new_point();
    if (p->internal == NULL) {
        WOLFSSL_MSG("ecc_new_point failure");
        XFREE(p, NULL, DYNAMIC_TYPE_ECC);
        return NULL;
    }

    return p;
}

#if !defined(WOLFSSL_SP_MATH) && !defined(WOLF_CRYPTO_CB_ONLY_ECC)
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_EC_POINT_get_affine_coordinates_GFp(const WOLFSSL_EC_GROUP *group,
                                                const WOLFSSL_EC_POINT *point,
                                                WOLFSSL_BIGNUM *x,
                                                WOLFSSL_BIGNUM *y,
                                                WOLFSSL_BN_CTX *ctx)
{
    mp_digit mp;
#ifdef WOLFSSL_SMALL_STACK
    mp_int* modulus = NULL;
#else
    mp_int modulus[1];
#endif
    (void)ctx;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_get_affine_coordinates_GFp");

    if (group == NULL || point == NULL || point->internal == NULL ||
        x == NULL || y == NULL ||
        wolfSSL_EC_POINT_is_at_infinity(group, point)) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_get_affine_coordinates_GFp NULL error");
        return 0;
    }

    if (setupPoint(point) != 1) {
        return 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    modulus = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (modulus == NULL) {
        return 0;
    }
#endif

    if (!wolfSSL_BN_is_one(point->Z)) {
        if (mp_init(modulus) != MP_OKAY) {
            WOLFSSL_MSG("mp_init failed");
       #ifdef WOLFSSL_SMALL_STACK
            XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
        #endif
            return 0;
        }
        /* Map the Jacobian point back to affine space */
        if (mp_read_radix(modulus, ecc_sets[group->curve_idx].prime,
                MP_RADIX_HEX) != MP_OKAY) {
            WOLFSSL_MSG("mp_read_radix failed");
            mp_clear(modulus);
        #ifdef WOLFSSL_SMALL_STACK
            XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
        #endif
            return 0;
        }
        if (mp_montgomery_setup(modulus, &mp) != MP_OKAY) {
            WOLFSSL_MSG("mp_montgomery_setup failed");
            mp_clear(modulus);
        #ifdef WOLFSSL_SMALL_STACK
            XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
        #endif
            return 0;
        }
        if (ecc_map((ecc_point*)point->internal, modulus, mp) != MP_OKAY) {
            WOLFSSL_MSG("ecc_map failed");
            mp_clear(modulus);
        #ifdef WOLFSSL_SMALL_STACK
            XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
        #endif
            return 0;
        }
        if (SetECPointExternal((WOLFSSL_EC_POINT *)point) != 1) {
            WOLFSSL_MSG("SetECPointExternal failed");
            mp_clear(modulus);
        #ifdef WOLFSSL_SMALL_STACK
            XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
        #endif
            return 0;
        }

        mp_clear(modulus);
    }

    BN_copy(x, point->X);
    BN_copy(y, point->Y);

#ifdef WOLFSSL_SMALL_STACK
    XFREE(modulus, NULL, DYNAMIC_TYPE_BIGINT);
#endif

    return 1;
}
#endif

int wolfSSL_EC_POINT_set_affine_coordinates_GFp(const WOLFSSL_EC_GROUP *group,
                                                WOLFSSL_EC_POINT *point,
                                                const WOLFSSL_BIGNUM *x,
                                                const WOLFSSL_BIGNUM *y,
                                                WOLFSSL_BN_CTX *ctx)
{
    (void)ctx;
    WOLFSSL_ENTER("wolfSSL_EC_POINT_set_affine_coordinates_GFp");

    if (group == NULL || point == NULL || point->internal == NULL ||
        x == NULL || y == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_set_affine_coordinates_GFp NULL error");
        return 0;
    }

    if (!point->X) {
        point->X = wolfSSL_BN_new();
    }
    if (!point->Y) {
        point->Y = wolfSSL_BN_new();
    }
    if (!point->Z) {
        point->Z = wolfSSL_BN_new();
    }
    if (!point->X || !point->Y || !point->Z) {
        WOLFSSL_MSG("wolfSSL_BN_new failed");
        return 0;
    }

    BN_copy(point->X, x);
    BN_copy(point->Y, y);
    BN_copy(point->Z, wolfSSL_BN_value_one());

    if (SetECPointInternal((WOLFSSL_EC_POINT *)point) != 1) {
        WOLFSSL_MSG("SetECPointInternal failed");
        return 0;
    }

    return 1;
}

#if !defined(WOLFSSL_ATECC508A) && !defined(WOLFSSL_ATECC608A) && \
    !defined(HAVE_SELFTEST) && !defined(WOLFSSL_SP_MATH) && \
    !defined(WOLF_CRYPTO_CB_ONLY_ECC)
int wolfSSL_EC_POINT_add(const WOLFSSL_EC_GROUP *group, WOLFSSL_EC_POINT *r,
                         const WOLFSSL_EC_POINT *p1,
                         const WOLFSSL_EC_POINT *p2, WOLFSSL_BN_CTX *ctx)
{
#ifdef WOLFSSL_SMALL_STACK
    mp_int* a = NULL;
    mp_int* prime = NULL;
    mp_int* mu = NULL;
#else
    mp_int a[1];
    mp_int prime[1];
    mp_int mu[1];
#endif
    mp_digit mp = 0;
    ecc_point* montP1 = NULL;
    ecc_point* montP2 = NULL;
    ecc_point* eccP1;
    ecc_point* eccP2;
    int ret = 0;

    (void)ctx;

    if (!group || !r || !p1 || !p2) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_add error");
        return 0;
    }

    if (setupPoint(r) != 1 ||
        setupPoint(p1) != 1 ||
        setupPoint(p2) != 1) {
        WOLFSSL_MSG("setupPoint error");
        return 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    a = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (a == NULL) {
        WOLFSSL_MSG("Failed to allocate memory for mp_int a");
        return 0;
    }
    prime = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (prime == NULL) {
        WOLFSSL_MSG("Failed to allocate memory for mp_int prime");
        XFREE(a, NULL, DYNAMIC_TYPE_BIGINT);
        return 0;
    }
    mu = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (mu == NULL) {
        WOLFSSL_MSG("Failed to allocate memory for mp_int mu");
        XFREE(a, NULL, DYNAMIC_TYPE_BIGINT);
        XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
        return 0;
    }
    XMEMSET(a, 0, sizeof(mp_int));
    XMEMSET(prime, 0, sizeof(mp_int));
    XMEMSET(mu, 0, sizeof(mp_int));
#endif

    /* read the curve prime and a */
    if (mp_init_multi(prime, a, mu, NULL, NULL, NULL) != MP_OKAY) {
        WOLFSSL_MSG("mp_init_multi error");
        goto cleanup;
    }

    if (mp_read_radix(a, ecc_sets[group->curve_idx].Af, MP_RADIX_HEX)
            != MP_OKAY) {
        WOLFSSL_MSG("mp_read_radix a error");
        goto cleanup;
    }

    if (mp_read_radix(prime, ecc_sets[group->curve_idx].prime, MP_RADIX_HEX)
            != MP_OKAY) {
        WOLFSSL_MSG("mp_read_radix prime error");
        goto cleanup;
    }

    if (mp_montgomery_setup(prime, &mp) != MP_OKAY) {
        WOLFSSL_MSG("mp_montgomery_setup nqm error");
        goto cleanup;
    }

    eccP1 = (ecc_point*)p1->internal;
    eccP2 = (ecc_point*)p2->internal;

    if (!(montP1 = wc_ecc_new_point_h(NULL)) ||
            !(montP2 = wc_ecc_new_point_h(NULL))) {
        WOLFSSL_MSG("wc_ecc_new_point_h nqm error");
        goto cleanup;
    }

    if ((mp_montgomery_calc_normalization(mu, prime)) != MP_OKAY) {
        WOLFSSL_MSG("mp_montgomery_calc_normalization error");
        goto cleanup;
    }

    /* Convert to Montgomery form */
    if (mp_cmp_d(mu, 1) == MP_EQ) {
        if (wc_ecc_copy_point(eccP1, montP1) != MP_OKAY ||
                wc_ecc_copy_point(eccP2, montP2) != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_copy_point error");
            goto cleanup;
        }
    } else {
        if (mp_mulmod(eccP1->x, mu, prime, montP1->x) != MP_OKAY ||
                mp_mulmod(eccP1->y, mu, prime, montP1->y) != MP_OKAY ||
                mp_mulmod(eccP1->z, mu, prime, montP1->z) != MP_OKAY) {
            WOLFSSL_MSG("mp_mulmod error");
            goto cleanup;
        }
        if (mp_mulmod(eccP2->x, mu, prime, montP2->x) != MP_OKAY ||
                mp_mulmod(eccP2->y, mu, prime, montP2->y) != MP_OKAY ||
                mp_mulmod(eccP2->z, mu, prime, montP2->z) != MP_OKAY) {
            WOLFSSL_MSG("mp_mulmod error");
            goto cleanup;
        }
    }

    if (ecc_projective_add_point(montP1, montP2, (ecc_point*)r->internal,
            a, prime, mp) != MP_OKAY) {
        WOLFSSL_MSG("ecc_projective_add_point error");
        goto cleanup;
    }

    if (ecc_map((ecc_point*)r->internal, prime, mp) != MP_OKAY) {
        WOLFSSL_MSG("ecc_map error");
        goto cleanup;
    }

    ret = 1;
cleanup:
    mp_clear(a);
    mp_clear(prime);
    mp_clear(mu);
    wc_ecc_del_point_h(montP1, NULL);
    wc_ecc_del_point_h(montP2, NULL);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(a, NULL, DYNAMIC_TYPE_BIGINT);
    XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
    XFREE(mu, NULL, DYNAMIC_TYPE_BIGINT);
#endif
    return ret;
}

/* Calculate the value: generator * n + q * m
 * return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_EC_POINT_mul(const WOLFSSL_EC_GROUP *group, WOLFSSL_EC_POINT *r,
                         const WOLFSSL_BIGNUM *n, const WOLFSSL_EC_POINT *q,
                         const WOLFSSL_BIGNUM *m, WOLFSSL_BN_CTX *ctx)
{
#ifdef WOLFSSL_SMALL_STACK
    mp_int* a = NULL;
    mp_int* prime = NULL;
#else
    mp_int a[1], prime[1];
#endif
    int ret = 0;
    ecc_point* result = NULL;
    ecc_point* tmp = NULL;

    (void)ctx;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_mul");

    if (!group || !r) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_mul NULL error");
        return 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    a = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (a == NULL)  {
        return 0;
    }
    prime = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (prime == NULL)  {
        XFREE(a, NULL, DYNAMIC_TYPE_BIGINT);
        return 0;
    }
#endif

    if (!(result = wc_ecc_new_point())) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_new error");
        return 0;
    }

    /* read the curve prime and a */
    if (mp_init_multi(prime, a, NULL, NULL, NULL, NULL) != MP_OKAY) {
        WOLFSSL_MSG("mp_init_multi error");
        goto cleanup;
    }

    if (q && setupPoint(q) != 1) {
        WOLFSSL_MSG("setupPoint error");
        goto cleanup;
    }

    if (mp_read_radix(prime, ecc_sets[group->curve_idx].prime, MP_RADIX_HEX)
            != MP_OKAY) {
        WOLFSSL_MSG("mp_read_radix prime error");
        goto cleanup;
    }

    if (mp_read_radix(a, ecc_sets[group->curve_idx].Af, MP_RADIX_HEX)
            != MP_OKAY) {
        WOLFSSL_MSG("mp_read_radix a error");
        goto cleanup;
    }

    if (n) {
        /* load generator */
    #if !defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0)
        if (wc_ecc_get_generator(result, group->curve_idx)
                != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_get_generator error");
            goto cleanup;
        }
    #else
        /* wc_ecc_get_generator is not defined in the FIPS v2 module. */
        if (mp_read_radix(result->x, ecc_sets[group->curve_idx].Gx,
                MP_RADIX_HEX) != MP_OKAY) {
            WOLFSSL_MSG("mp_read_radix Gx error");
            goto cleanup;
        }
        if (mp_read_radix(result->y, ecc_sets[group->curve_idx].Gy,
                MP_RADIX_HEX) != MP_OKAY) {
            WOLFSSL_MSG("mp_read_radix Gy error");
            goto cleanup;
        }
        if (mp_set(result->z, 1) != MP_OKAY) {
            WOLFSSL_MSG("mp_set Gz error");
            goto cleanup;
        }
    #endif /* NOPT_FIPS_VERSION == 2 */
    }

    if (n && q && m) {
        /* r = generator * n + q * m */
#ifdef ECC_SHAMIR
        if (ecc_mul2add(result, (mp_int*)n->internal,
                        (ecc_point*)q->internal, (mp_int*)m->internal,
                        result, a, prime, NULL)
                != MP_OKAY) {
            WOLFSSL_MSG("ecc_mul2add error");
            goto cleanup;
        }
#else
        mp_digit mp = 0;
        if (mp_montgomery_setup(prime, &mp) != MP_OKAY) {
            WOLFSSL_MSG("mp_montgomery_setup nqm error");
            goto cleanup;
        }
        if (!(tmp = wc_ecc_new_point())) {
            WOLFSSL_MSG("wolfSSL_EC_POINT_new nqm error");
            goto cleanup;
        }
        /* r = generator * n */
        if (wc_ecc_mulmod((mp_int*)n->internal, result, result, a, prime, 0)
                != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_mulmod nqm error");
            goto cleanup;
        }
        /* tmp = q * m */
        if (wc_ecc_mulmod((mp_int*)m->internal, (ecc_point*)q->internal,
                tmp, a, prime, 0) != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_mulmod nqm error");
            goto cleanup;
        }
        /* result = result + tmp */
        if (ecc_projective_add_point(tmp, result, result, a, prime, mp)
                != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_mulmod nqm error");
            goto cleanup;
        }
        if (ecc_map(result, prime, mp) != MP_OKAY) {
            WOLFSSL_MSG("ecc_map nqm error");
            goto cleanup;
        }
#endif
    }
    else if (n) {
        /* r = generator * n */
        if (wc_ecc_mulmod((mp_int*)n->internal, result, result, a, prime, 1)
                != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_mulmod gn error");
            goto cleanup;
        }
    }
    else if (q && m) {
        /* r = q * m */
        if (wc_ecc_mulmod((mp_int*)m->internal, (ecc_point*)q->internal,
                           result, a, prime, 1) != MP_OKAY) {
            WOLFSSL_MSG("wc_ecc_mulmod qm error");
            goto cleanup;
        }
    }

    /* copy to destination */
    if (wc_ecc_copy_point(result, (ecc_point*)r->internal)) {
        WOLFSSL_MSG("wc_ecc_copy_point error");
        goto cleanup;
    }
    r->inSet = 1;
    if (SetECPointExternal(r) != 1) {
        WOLFSSL_MSG("SetECPointExternal error");
        goto cleanup;
    }

    ret = 1;
cleanup:
    mp_clear(a);
    mp_clear(prime);
    wc_ecc_del_point(result);
    wc_ecc_del_point(tmp);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(a, NULL, DYNAMIC_TYPE_BIGINT);
    XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
#endif
    return ret;
}
#endif /* !WOLFSSL_ATECC508A && !WOLFSSL_ATECC608A && !HAVE_SELFTEST &&
        * !WOLFSSL_SP_MATH */

/* (x, y) -> (x, -y) */
int wolfSSL_EC_POINT_invert(const WOLFSSL_EC_GROUP *group, WOLFSSL_EC_POINT *a,
                            WOLFSSL_BN_CTX *ctx)
{
    ecc_point* p;
#ifdef WOLFSSL_SMALL_STACK
    mp_int* prime = NULL;
#else
    mp_int prime[1];
#endif

    (void)ctx;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_invert");

    if (!group || !a || !a->internal || setupPoint(a) != 1) {
        return 0;
    }

    p = (ecc_point*)a->internal;

#ifdef WOLFSSL_SMALL_STACK
    prime = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (prime == NULL) {
        return 0;
    }
#endif

    /* read the curve prime and a */
    if (mp_init_multi(prime, NULL, NULL, NULL, NULL, NULL) != MP_OKAY) {
        WOLFSSL_MSG("mp_init_multi error");
    #ifdef WOLFSSL_SMALL_STACK
        XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
    #endif
        return 0;
    }

    if (mp_sub(prime, p->y, p->y) != MP_OKAY) {
        WOLFSSL_MSG("mp_sub error");
    #ifdef WOLFSSL_SMALL_STACK
        XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
    #endif
        return 0;
    }

    if (SetECPointExternal(a) != 1) {
        WOLFSSL_MSG("SetECPointExternal error");
    #ifdef WOLFSSL_SMALL_STACK
        XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
    #endif
        return 0;
    }

#ifdef WOLFSSL_SMALL_STACK
    XFREE(prime, NULL, DYNAMIC_TYPE_BIGINT);
#endif

    return 1;
}

void wolfSSL_EC_POINT_clear_free(WOLFSSL_EC_POINT *p)
{
    WOLFSSL_ENTER("wolfSSL_EC_POINT_clear_free");

    wolfSSL_EC_POINT_free(p);
}

/* return code compliant with OpenSSL :
 *   0 if equal, 1 if not and -1 in case of error
 */
int wolfSSL_EC_POINT_cmp(const WOLFSSL_EC_GROUP *group,
                         const WOLFSSL_EC_POINT *a, const WOLFSSL_EC_POINT *b,
                         WOLFSSL_BN_CTX *ctx)
{
    int ret;

    (void)ctx;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_cmp");

    if (group == NULL || a == NULL || a->internal == NULL || b == NULL ||
        b->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_cmp Bad arguments");
        return -1;
    }

    ret = wc_ecc_cmp_point((ecc_point*)a->internal, (ecc_point*)b->internal);
    if (ret == MP_EQ)
        return 0;
    else if (ret == MP_LT || ret == MP_GT)
        return 1;

    return -1;
}

int wolfSSL_EC_POINT_copy(WOLFSSL_EC_POINT *dest, const WOLFSSL_EC_POINT *src)
{
    WOLFSSL_ENTER("wolfSSL_EC_POINT_copy");

    if (!dest || !src) {
        return 0;
    }

    if (setupPoint(src) != 1) {
        return 0;
    }

    if (wc_ecc_copy_point((ecc_point*) dest->internal,
                          (ecc_point*) src->internal) != MP_OKAY) {
        return 0;
    }

    dest->inSet = 1;

    if (SetECPointExternal(dest) != 1) {
        return 0;
    }

    return 1;
}
#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)
void wolfSSL_EC_POINT_free(WOLFSSL_EC_POINT *p)
{
    WOLFSSL_ENTER("wolfSSL_EC_POINT_free");

    if (p != NULL) {
        if (p->internal != NULL) {
            wc_ecc_del_point((ecc_point*)p->internal);
            p->internal = NULL;
        }

        wolfSSL_BN_free(p->X);
        wolfSSL_BN_free(p->Y);
        wolfSSL_BN_free(p->Z);
        p->X = NULL;
        p->Y = NULL;
        p->Z = NULL;
        p->inSet = p->exSet = 0;

        XFREE(p, NULL, DYNAMIC_TYPE_ECC);
        /* p = NULL, don't try to access or double free it */
    }
}
#endif /* OPENSSL_EXTRA || OPENSSL_EXTRA_X509_SMALL */

#ifdef OPENSSL_EXTRA
/* return code compliant with OpenSSL :
 *   1 if point at infinity, 0 else
 */
int wolfSSL_EC_POINT_is_at_infinity(const WOLFSSL_EC_GROUP *group,
                                    const WOLFSSL_EC_POINT *point)
{
    int ret;

    WOLFSSL_ENTER("wolfSSL_EC_POINT_is_at_infinity");

    if (group == NULL || point == NULL || point->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_EC_POINT_is_at_infinity NULL error");
        return 0;
    }

    if (setupPoint(point) != 1) {
        return 0;
    }
    #ifndef WOLF_CRYPTO_CB_ONLY_ECC
    ret = wc_ecc_point_is_at_infinity((ecc_point*)point->internal);
    if (ret < 0) {
        WOLFSSL_MSG("ecc_point_is_at_infinity failure");
        return 0;
    }
    #else
        WOLFSSL_MSG("ecc_point_is_at_infinitiy compiled out");
        return 0;
    #endif
    return ret;
}

/* End EC_POINT */

#if !defined(HAVE_FIPS) || FIPS_VERSION_GT(2,0)
size_t wolfSSL_EC_get_builtin_curves(WOLFSSL_EC_BUILTIN_CURVE *r, size_t nitems)
{
    size_t i, min_nitems;
#ifdef HAVE_SELFTEST
    size_t ecc_sets_count;
    for (i = 0; ecc_sets[i].size != 0 && ecc_sets[i].name != NULL; i++);
    ecc_sets_count = i;
#endif

    if (r == NULL || nitems == 0)
        return ecc_sets_count;

    min_nitems = nitems < ecc_sets_count ? nitems : ecc_sets_count;

    for (i = 0; i < min_nitems; i++) {
        r[i].nid = EccEnumToNID(ecc_sets[i].id);
        r[i].comment = wolfSSL_OBJ_nid2sn(r[i].nid);
    }

    return min_nitems;
}
#endif /* !HAVE_FIPS || FIPS_VERSION_GT(2,0) */

/* Start ECDSA_SIG */
void wolfSSL_ECDSA_SIG_free(WOLFSSL_ECDSA_SIG *sig)
{
    WOLFSSL_ENTER("wolfSSL_ECDSA_SIG_free");

    if (sig) {
        wolfSSL_BN_free(sig->r);
        wolfSSL_BN_free(sig->s);

        XFREE(sig, NULL, DYNAMIC_TYPE_ECC);
    }
}

WOLFSSL_ECDSA_SIG *wolfSSL_ECDSA_SIG_new(void)
{
    WOLFSSL_ECDSA_SIG *sig;

    WOLFSSL_ENTER("wolfSSL_ECDSA_SIG_new");

    sig = (WOLFSSL_ECDSA_SIG*) XMALLOC(sizeof(WOLFSSL_ECDSA_SIG), NULL,
                                       DYNAMIC_TYPE_ECC);
    if (sig == NULL) {
        WOLFSSL_MSG("wolfSSL_ECDSA_SIG_new malloc ECDSA signature failure");
        return NULL;
    }

    sig->s = NULL;
    sig->r = wolfSSL_BN_new();
    if (sig->r == NULL) {
        WOLFSSL_MSG("wolfSSL_ECDSA_SIG_new malloc ECDSA r failure");
        wolfSSL_ECDSA_SIG_free(sig);
        return NULL;
    }

    sig->s = wolfSSL_BN_new();
    if (sig->s == NULL) {
        WOLFSSL_MSG("wolfSSL_ECDSA_SIG_new malloc ECDSA s failure");
        wolfSSL_ECDSA_SIG_free(sig);
        return NULL;
    }

    return sig;
}

void wolfSSL_ECDSA_SIG_get0(const WOLFSSL_ECDSA_SIG* sig,
    const WOLFSSL_BIGNUM** r, const WOLFSSL_BIGNUM** s)
{
    if (sig == NULL) {
        return;
    }

    if (r != NULL) {
        *r = sig->r;
    }
    if (s != NULL) {
        *s = sig->s;
    }
}

int wolfSSL_ECDSA_SIG_set0(WOLFSSL_ECDSA_SIG* sig, WOLFSSL_BIGNUM* r,
    WOLFSSL_BIGNUM* s)
{
    if (sig == NULL || r == NULL || s == NULL) {
        return 0;
    }

    wolfSSL_BN_free(sig->r);
    wolfSSL_BN_free(sig->s);

    sig->r = r;
    sig->s = s;

    return 1;
}

/* return signature structure on success, NULL otherwise */
WOLFSSL_ECDSA_SIG *wolfSSL_ECDSA_do_sign(const unsigned char *d, int dlen,
                                         WOLFSSL_EC_KEY *key)
{
    WOLFSSL_ECDSA_SIG *sig = NULL;
    int     initTmpRng = 0;
    WC_RNG* rng = NULL;
#ifdef WOLFSSL_SMALL_STACK
    WC_RNG* tmpRng = NULL;
    byte*   out = NULL;
    mp_int* sig_r = NULL;
    mp_int* sig_s = NULL;
#else
    WC_RNG  tmpRng[1];
    byte    out[ECC_BUFSIZE];
    mp_int sig_r[1], sig_s[1];
#endif
    word32 outlen = ECC_BUFSIZE;

    WOLFSSL_ENTER("wolfSSL_ECDSA_do_sign");

    if (d == NULL || key == NULL || key->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_ECDSA_do_sign Bad arguments");
        return NULL;
    }

    /* set internal key if not done */
    if (key->inSet == 0)
    {
        WOLFSSL_MSG("wolfSSL_ECDSA_do_sign No EC key internal set, do it");

        if (SetECKeyInternal(key) != 1) {
            WOLFSSL_MSG("wolfSSL_ECDSA_do_sign SetECKeyInternal failed");
            return NULL;
        }
    }

#ifdef WOLFSSL_SMALL_STACK
    tmpRng = (WC_RNG*)XMALLOC(sizeof(WC_RNG), NULL, DYNAMIC_TYPE_RNG);
    if (tmpRng == NULL)
        return NULL;
    out = (byte*)XMALLOC(outlen, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (out == NULL) {
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
        return NULL;
    }
    sig_r = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (sig_r == NULL) {
        XFREE(out, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
        return NULL;
    }
    sig_s = (mp_int*)XMALLOC(sizeof(mp_int), NULL, DYNAMIC_TYPE_BIGINT);
    if (sig_s == NULL) {
        XFREE(sig_r, NULL, DYNAMIC_TYPE_BIGINT);
        XFREE(out, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
        return NULL;
    }
#endif

    if (wc_InitRng(tmpRng) == 0) {
        rng = tmpRng;
        initTmpRng = 1;
    }
    else {
        WOLFSSL_MSG("wolfSSL_ECDSA_do_sign Bad RNG Init, trying global");
        rng = wolfssl_get_global_rng();
    }

    if (rng) {
        /* use wc_ecc_sign_hash because it supports crypto callbacks */
        if (wc_ecc_sign_hash(d, dlen, out, &outlen, rng,
                                                (ecc_key*)key->internal) == 0) {
            if (mp_init_multi(sig_r, sig_s, NULL, NULL, NULL, NULL) == MP_OKAY) {
               /* put signature blob in ECDSA structure */
                if (DecodeECC_DSA_Sig(out, outlen, sig_r, sig_s) == 0) {
                    sig = wolfSSL_ECDSA_SIG_new();
                    if (sig == NULL) {
                        WOLFSSL_MSG("wolfSSL_ECDSA_SIG_new failed");
                    }
                    else if (SetIndividualExternal(&sig->r, sig_r)
                            != 1) {
                        WOLFSSL_MSG("ecdsa r key error");
                        wolfSSL_ECDSA_SIG_free(sig);
                        sig = NULL;
                    }
                    else if (SetIndividualExternal(&sig->s, sig_s)
                            != 1) {
                        WOLFSSL_MSG("ecdsa s key error");
                        wolfSSL_ECDSA_SIG_free(sig);
                        sig = NULL;
                    }
                }
                mp_free(sig_r);
                mp_free(sig_s);
            }
        }
        else {
            WOLFSSL_MSG("wc_ecc_sign_hash failed");
        }
    }

    if (initTmpRng)
        wc_FreeRng(tmpRng);
#ifdef WOLFSSL_SMALL_STACK
    XFREE(sig_s, NULL, DYNAMIC_TYPE_BIGINT);
    XFREE(sig_r, NULL, DYNAMIC_TYPE_BIGINT);
    XFREE(out, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(tmpRng, NULL, DYNAMIC_TYPE_RNG);
#endif

    return sig;
}

/* return code compliant with OpenSSL :
 *   1 for a valid signature, 0 for an invalid signature and -1 on error
 */
int wolfSSL_ECDSA_do_verify(const unsigned char *d, int dlen,
                            const WOLFSSL_ECDSA_SIG *sig, WOLFSSL_EC_KEY *key)
{
    int check_sign = 0;
#ifdef WOLF_CRYPTO_CB_ONLY_ECC
    byte signature[ECC_MAX_SIG_SIZE];
    word32 signaturelen = (word32)sizeof(signature);
    char* r;
    char* s;
    int ret = 0;
#endif

    WOLFSSL_ENTER("wolfSSL_ECDSA_do_verify");

    if (d == NULL || sig == NULL || key == NULL || key->internal == NULL) {
        WOLFSSL_MSG("wolfSSL_ECDSA_do_verify Bad arguments");
        return -1;
    }

    /* set internal key if not done */
    if (key->inSet == 0)
    {
        WOLFSSL_MSG("No EC key internal set, do it");

        if (SetECKeyInternal(key) != 1) {
            WOLFSSL_MSG("SetECKeyInternal failed");
            return -1;
        }
    }

#ifndef WOLF_CRYPTO_CB_ONLY_ECC
    if (wc_ecc_verify_hash_ex((mp_int*)sig->r->internal,
                              (mp_int*)sig->s->internal, d, dlen, &check_sign,
                              (ecc_key *)key->internal) != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_verify_hash failed");
        return -1;
    }
    else if (check_sign == 0) {
        WOLFSSL_MSG("wc_ecc_verify_hash incorrect signature detected");
        return 0;
    }
#else
    /* convert big number to hex */
    r = wolfSSL_BN_bn2hex(sig->r);
    s = wolfSSL_BN_bn2hex(sig->s);
    /* get DER-encoded ECDSA signature */
    ret = wc_ecc_rs_to_sig((const char*)r, (const char*)s,
                                        signature, &signaturelen);
    /* free r and s */
    if (r)
        XFREE(r, NULL, DYNAMIC_TYPE_OPENSSL);
    if (s)
        XFREE(s, NULL, DYNAMIC_TYPE_OPENSSL);

    if (ret != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_verify_hash failed");
        return -1;
    }
    /* verify hash. expects to call wc_CryptoCb_EccVerify internally */
    ret = wc_ecc_verify_hash(signature, signaturelen, d, dlen, &check_sign,
                        (ecc_key*)key->internal);

    if (ret != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_verify_hash failed");
        return -1;
    }
    else if (check_sign == 0) {
        WOLFSSL_MSG("wc_ecc_verify_hash incorrect signature detected");
        return 0;
    }
#endif /* WOLF_CRYPTO_CB_ONLY_ECC */

    return 1;
}

WOLFSSL_ECDSA_SIG *wolfSSL_d2i_ECDSA_SIG(WOLFSSL_ECDSA_SIG **sig,
                                         const unsigned char **pp, long len)
{
    WOLFSSL_ECDSA_SIG *s = NULL;

    if (pp == NULL)
        return NULL;
    if (sig != NULL)
        s = *sig;
    if (s == NULL) {
        s = wolfSSL_ECDSA_SIG_new();
        if (s == NULL)
            return NULL;
    }

    /* DecodeECC_DSA_Sig calls mp_init, so free these */
    mp_free((mp_int*)s->r->internal);
    mp_free((mp_int*)s->s->internal);

    if (DecodeECC_DSA_Sig(*pp, (word32)len, (mp_int*)s->r->internal,
                                          (mp_int*)s->s->internal) != MP_OKAY) {
        if (sig == NULL || *sig == NULL)
            wolfSSL_ECDSA_SIG_free(s);
        return NULL;
    }

    *pp += len;
    if (sig != NULL)
        *sig = s;
    return s;
}

int wolfSSL_i2d_ECDSA_SIG(const WOLFSSL_ECDSA_SIG *sig, unsigned char **pp)
{
    word32 len;

    if (sig == NULL)
        return 0;

    /* ASN.1: SEQ + INT + INT
     *   ASN.1 Integer must be a positive value - prepend zero if number has
     *   top bit set.
     */
    len = 2 + mp_leading_bit((mp_int*)sig->r->internal) +
              mp_unsigned_bin_size((mp_int*)sig->r->internal) +
          2 + mp_leading_bit((mp_int*)sig->s->internal) +
              mp_unsigned_bin_size((mp_int*)sig->s->internal);
    /* Two bytes required for length if ASN.1 SEQ data greater than 127 bytes
     * and less than 256 bytes.
     */
    len = 1 + ((len > 127) ? 2 : 1) + len;
    if (pp != NULL && *pp != NULL) {
        if (StoreECC_DSA_Sig(*pp, &len, (mp_int*)sig->r->internal,
                                        (mp_int*)sig->s->internal) != MP_OKAY) {
            len = 0;
        }
        else
            *pp += len;
    }

    return (int)len;
}
/* End ECDSA_SIG */

#ifndef WOLF_CRYPTO_CB_ONLY_ECC
/* Start ECDH */
/* return code compliant with OpenSSL :
 *   length of computed key if success, -1 if error
 */
int wolfSSL_ECDH_compute_key(void *out, size_t outlen,
                             const WOLFSSL_EC_POINT *pub_key,
                             WOLFSSL_EC_KEY *ecdh,
                             void *(*KDF) (const void *in, size_t inlen,
                                           void *out, size_t *outlen))
{
    word32 len;
    ecc_key* key;
    int ret;
#if defined(ECC_TIMING_RESISTANT) && !defined(HAVE_SELFTEST) && \
    (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5,0))
    int setGlobalRNG = 0;
#endif
    (void)KDF;

    WOLFSSL_ENTER("wolfSSL_ECDH_compute_key");

    if (out == NULL || pub_key == NULL || pub_key->internal == NULL ||
        ecdh == NULL || ecdh->internal == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }

    /* set internal key if not done */
    if (ecdh->inSet == 0)
    {
        WOLFSSL_MSG("No EC key internal set, do it");

        if (SetECKeyInternal(ecdh) != 1) {
            WOLFSSL_MSG("SetECKeyInternal failed");
            return -1;
        }
    }

    len = (word32)outlen;
    key = (ecc_key*)ecdh->internal;

#if defined(ECC_TIMING_RESISTANT) && !defined(HAVE_SELFTEST) && \
    (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5,0))
    if (key->rng == NULL) {
        if ((key->rng = wolfssl_get_global_rng()) == NULL) {
            if (wolfSSL_RAND_Init() != 1) {
                WOLFSSL_MSG("No RNG to use");
                return -1;
            }
            key->rng = wolfssl_get_global_rng();
        }
        setGlobalRNG = 1;
    }
#endif
    PRIVATE_KEY_UNLOCK();
    ret = wc_ecc_shared_secret_ssh(key, (ecc_point*)pub_key->internal,
            (byte *)out, &len);
    PRIVATE_KEY_LOCK();
#if defined(ECC_TIMING_RESISTANT) && !defined(HAVE_SELFTEST) && \
    (!defined(HAVE_FIPS) || FIPS_VERSION_GE(5,0))
    if (setGlobalRNG)
        key->rng = NULL;
#endif
    if (ret != MP_OKAY) {
        WOLFSSL_MSG("wc_ecc_shared_secret failed");
        return -1;
    }

    return len;
}
#endif /* WOLF_CRYPTO_CB_ONLY_ECC */
/* End ECDH */
#if !defined(NO_FILESYSTEM)

#ifndef NO_BIO

#ifdef WOLFSSL_KEY_GEN
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_EC_PUBKEY(XFILE fp, WOLFSSL_EC_KEY* key)
{
    int ret = 1;
    WOLFSSL_BIO* bio = NULL;

    WOLFSSL_ENTER("wolfSSL_PEM_write_EC_PUBKEY");

    if (fp == XBADFILE || key == NULL) {
        WOLFSSL_MSG("Bad argument.");
        ret = 0;
    }

    if (ret == 1) {
        bio = wolfSSL_BIO_new_fp(fp, BIO_NOCLOSE);
        if (bio == NULL) {
            WOLFSSL_MSG("wolfSSL_BIO_new failed.");
            ret = 0;
        }
    }
    if (ret == 1 && wolfSSL_PEM_write_bio_EC_PUBKEY(bio, key)
        != 1) {
        WOLFSSL_MSG("wolfSSL_PEM_write_bio_EC_PUBKEY failed.");
        ret = 0;
    }

    if (bio != NULL) {
        wolfSSL_BIO_free(bio);
    }

    WOLFSSL_LEAVE("wolfSSL_PEM_write_EC_PUBKEY", ret);

    return ret;
}
#endif

/* Uses the same format of input as wolfSSL_PEM_read_bio_PrivateKey but expects
 * the results to be an EC key.
 *
 * bio  structure to read EC private key from
 * ec   if not null is then set to the result
 * cb   password callback for reading PEM
 * pass password string
 *
 * returns a pointer to a new WOLFSSL_EC_KEY struct on success and NULL on fail
 */

WOLFSSL_EC_KEY* wolfSSL_PEM_read_bio_EC_PUBKEY(WOLFSSL_BIO* bio,
                                               WOLFSSL_EC_KEY** ec,
                                               wc_pem_password_cb* cb,
                                               void *pass)
{
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_EC_KEY* local;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_EC_PUBKEY");

    pkey = wolfSSL_PEM_read_bio_PUBKEY(bio, NULL, cb, pass);
    if (pkey == NULL) {
        return NULL;
    }

    /* Since the WOLFSSL_EC_KEY structure is being taken from WOLFSSL_EVP_PKEY the
     * flag indicating that the WOLFSSL_EC_KEY structure is owned should be FALSE
     * flag indicating that the WOLFSSL_EC_KEY structure is owned should be FALSE
     * to avoid having it free'd */
    pkey->ownEcc = 0;
    local = pkey->ecc;
    if (ec != NULL) {
        *ec = local;
    }

    wolfSSL_EVP_PKEY_free(pkey);
    return local;
}

/* Reads a private EC key from a WOLFSSL_BIO into a WOLFSSL_EC_KEY.
 * Returns 1 or 0
 */
WOLFSSL_EC_KEY* wolfSSL_PEM_read_bio_ECPrivateKey(WOLFSSL_BIO* bio,
                                                  WOLFSSL_EC_KEY** ec,
                                                  wc_pem_password_cb* cb,
                                                  void *pass)
{
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_EC_KEY* local;

    WOLFSSL_ENTER("wolfSSL_PEM_read_bio_ECPrivateKey");

    pkey = wolfSSL_PEM_read_bio_PrivateKey(bio, NULL, cb, pass);
    if (pkey == NULL) {
        return NULL;
    }

    /* Since the WOLFSSL_EC_KEY structure is being taken from WOLFSSL_EVP_PKEY the
     * flag indicating that the WOLFSSL_EC_KEY structure is owned should be FALSE
     * to avoid having it free'd */
    pkey->ownEcc = 0;
    local = pkey->ecc;
    if (ec != NULL) {
        *ec = local;
    }

    wolfSSL_EVP_PKEY_free(pkey);
    return local;
}
#endif /* !NO_BIO */
#endif /* NO_FILESYSTEM */

#if defined(WOLFSSL_KEY_GEN)
#ifndef NO_BIO
/* Takes a public WOLFSSL_EC_KEY and writes it out to WOLFSSL_BIO
 * Returns 1 or 0
 */
int wolfSSL_PEM_write_bio_EC_PUBKEY(WOLFSSL_BIO* bio, WOLFSSL_EC_KEY* ec)
{
    int ret = 0;
    WOLFSSL_EVP_PKEY* pkey;

    WOLFSSL_ENTER("wolfSSL_PEM_write_bio_EC_PUBKEY");

    if (bio == NULL || ec == NULL) {
        WOLFSSL_MSG("Bad Function Arguments");
        return 0;
    }

    /* Initialize pkey structure */
    pkey = wolfSSL_EVP_PKEY_new_ex(bio->heap);
    if (pkey == NULL) {
        WOLFSSL_MSG("wolfSSL_EVP_PKEY_new_ex failed");
        return 0;
    }

    /* Set pkey info */
    pkey->ecc    = ec;
    pkey->ownEcc = 0; /* pkey does not own ECC */
    pkey->type = EVP_PKEY_EC;

    if ((ret = pem_write_bio_pubkey(bio, pkey)) != 1) {
        WOLFSSL_MSG("wolfSSL_PEM_write_bio_PUBKEY failed");
    }
    wolfSSL_EVP_PKEY_free(pkey);

    return ret;
}

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_bio_ECPrivateKey(WOLFSSL_BIO* bio, WOLFSSL_EC_KEY* ec,
                                       const EVP_CIPHER* cipher,
                                       unsigned char* passwd, int len,
                                       wc_pem_password_cb* cb, void* arg)
{
    int ret = 0, der_max_len = 0, derSz = 0;
    byte *derBuf;
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_ENTER("WOLFSSL_PEM_write_bio_ECPrivateKey");

    if (bio == NULL || ec == NULL) {
        WOLFSSL_MSG("Bad Function Arguments");
        return 0;
    }

    /* Initialize pkey structure */
    pkey = wolfSSL_EVP_PKEY_new_ex(bio->heap);
    if (pkey == NULL) {
        WOLFSSL_MSG("wolfSSL_EVP_PKEY_new_ex failed");
        return 0;
    }

    /* Set pkey info */
    pkey->ecc    = ec;
    pkey->ownEcc = 0; /* pkey does not own ECC */
    pkey->type = EVP_PKEY_EC;

    /* 4 > size of pub, priv + ASN.1 additional informations
     */
    der_max_len = 4 * wc_ecc_size((ecc_key*)ec->internal) + AES_BLOCK_SIZE;

    derBuf = (byte*)XMALLOC(der_max_len, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    if (derBuf == NULL) {
        WOLFSSL_MSG("Malloc failed");
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    /* convert key to der format */
    derSz = wc_EccKeyToDer((ecc_key*)ec->internal, derBuf, der_max_len);
    if (derSz < 0) {
        WOLFSSL_MSG("wc_EccKeyToDer failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    pkey->pkey.ptr = (char*)XMALLOC(derSz, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    if (pkey->pkey.ptr == NULL) {
        WOLFSSL_MSG("key malloc failed");
        XFREE(derBuf, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
        wolfSSL_EVP_PKEY_free(pkey);
        return 0;
    }

    /* add der info to the evp key */
    pkey->pkey_sz = derSz;
    XMEMCPY(pkey->pkey.ptr, derBuf, derSz);
    XFREE(derBuf, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);

    ret = wolfSSL_PEM_write_bio_PrivateKey(bio, pkey, cipher, passwd, len,
                                        cb, arg);
    wolfSSL_EVP_PKEY_free(pkey);

    return ret;
}

#endif /* !NO_BIO */

/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_mem_ECPrivateKey(WOLFSSL_EC_KEY* ecc,
                                       const EVP_CIPHER* cipher,
                                       unsigned char* passwd, int passwdSz,
                                       unsigned char **pem, int *plen)
{
#if defined(WOLFSSL_PEM_TO_DER) || defined(WOLFSSL_DER_TO_PEM)
    byte *derBuf, *tmp, *cipherInfo = NULL;
    int  der_max_len = 0, derSz = 0;
    const int type = ECC_PRIVATEKEY_TYPE;
    const char* header = NULL;
    const char* footer = NULL;

    WOLFSSL_MSG("wolfSSL_PEM_write_mem_ECPrivateKey");
    if (pem == NULL || plen == NULL || ecc == NULL || ecc->internal == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    if (wc_PemGetHeaderFooter(type, &header, &footer) != 0)
        return 0;

    if (ecc->inSet == 0) {
        WOLFSSL_MSG("No ECC internal set, do it");

        if (SetECKeyInternal(ecc) != 1) {
            WOLFSSL_MSG("SetECKeyInternal failed");
            return 0;
        }
    }

    /* 4 > size of pub, priv + ASN.1 additional information */
    der_max_len = 4 * wc_ecc_size((ecc_key*)ecc->internal) + AES_BLOCK_SIZE;

    derBuf = (byte*)XMALLOC(der_max_len, NULL, DYNAMIC_TYPE_DER);
    if (derBuf == NULL) {
        WOLFSSL_MSG("malloc failed");
        return 0;
    }

    /* Key to DER */
    derSz = wc_EccKeyToDer((ecc_key*)ecc->internal, derBuf, der_max_len);
    if (derSz < 0) {
        WOLFSSL_MSG("wc_EccKeyToDer failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        return 0;
    }

    /* encrypt DER buffer if required */
    if (passwd != NULL && passwdSz > 0 && cipher != NULL) {
        int ret;

        ret = EncryptDerKey(derBuf, &derSz, cipher,
                            passwd, passwdSz, &cipherInfo, der_max_len);
        if (ret != 1) {
            WOLFSSL_MSG("EncryptDerKey failed");
            XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
            return ret;
        }

        /* tmp buffer with a max size */
        *plen = (derSz * 2) + (int)XSTRLEN(header) + 1 +
            (int)XSTRLEN(footer) + 1 + HEADER_ENCRYPTED_KEY_SIZE;
    }
    else { /* tmp buffer with a max size */
        *plen = (derSz * 2) + (int)XSTRLEN(header) + 1 +
            (int)XSTRLEN(footer) + 1;
    }

    tmp = (byte*)XMALLOC(*plen, NULL, DYNAMIC_TYPE_PEM);
    if (tmp == NULL) {
        WOLFSSL_MSG("malloc failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        if (cipherInfo != NULL)
            XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);
        return 0;
    }

    /* DER to PEM */
    *plen = wc_DerToPemEx(derBuf, derSz, tmp, *plen, cipherInfo, type);
    if (*plen <= 0) {
        WOLFSSL_MSG("wc_DerToPemEx failed");
        XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        if (cipherInfo != NULL)
            XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);
        return 0;
    }
    XFREE(derBuf, NULL, DYNAMIC_TYPE_DER);
    if (cipherInfo != NULL)
        XFREE(cipherInfo, NULL, DYNAMIC_TYPE_STRING);

    *pem = (byte*)XMALLOC((*plen)+1, NULL, DYNAMIC_TYPE_KEY);
    if (*pem == NULL) {
        WOLFSSL_MSG("malloc failed");
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        return 0;
    }
    XMEMSET(*pem, 0, (*plen)+1);

    if (XMEMCPY(*pem, tmp, *plen) == NULL) {
        WOLFSSL_MSG("XMEMCPY failed");
        XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
        XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);
        return 0;
    }
    XFREE(tmp, NULL, DYNAMIC_TYPE_PEM);

    return 1;
#else
    (void)ecc;
    (void)cipher;
    (void)passwd;
    (void)passwdSz;
    (void)pem;
    (void)plen;
    return 0;
#endif /* WOLFSSL_PEM_TO_DER || WOLFSSL_DER_TO_PEM */
}

#ifndef NO_FILESYSTEM
/* return code compliant with OpenSSL :
 *   1 if success, 0 if error
 */
int wolfSSL_PEM_write_ECPrivateKey(XFILE fp, WOLFSSL_EC_KEY *ecc,
                                   const EVP_CIPHER *enc,
                                   unsigned char *kstr, int klen,
                                   wc_pem_password_cb *cb, void *u)
{
    byte *pem;
    int  plen, ret;

    (void)cb;
    (void)u;

    WOLFSSL_MSG("wolfSSL_PEM_write_ECPrivateKey");

    if (fp == XBADFILE || ecc == NULL || ecc->internal == NULL) {
        WOLFSSL_MSG("Bad function arguments");
        return 0;
    }

    ret = wolfSSL_PEM_write_mem_ECPrivateKey(ecc, enc, kstr, klen, &pem, &plen);
    if (ret != 1) {
        WOLFSSL_MSG("wolfSSL_PEM_write_mem_ECPrivateKey failed");
        return 0;
    }

    ret = (int)XFWRITE(pem, plen, 1, fp);
    if (ret != 1) {
        WOLFSSL_MSG("ECC private key file write failed");
        return 0;
    }

    XFREE(pem, NULL, DYNAMIC_TYPE_KEY);
    return 1;
}

#endif /* NO_FILESYSTEM */
#endif /* defined(WOLFSSL_KEY_GEN) */

#ifndef NO_BIO

/* returns a new WOLFSSL_EC_GROUP structure on success and NULL on fail */
WOLFSSL_EC_GROUP* wolfSSL_PEM_read_bio_ECPKParameters(WOLFSSL_BIO* bio,
        WOLFSSL_EC_GROUP** group, wc_pem_password_cb* cb, void* pass)
{
    WOLFSSL_EVP_PKEY* pkey;
    WOLFSSL_EC_GROUP* ret = NULL;

    /* check on if bio is null is done in wolfSSL_PEM_read_bio_PrivateKey */
    pkey = wolfSSL_PEM_read_bio_PrivateKey(bio, NULL, cb, pass);
    if (pkey != NULL) {
        if (pkey->type != EVP_PKEY_EC) {
            WOLFSSL_MSG("Unexpected key type");
        }
        else {
            ret = (WOLFSSL_EC_GROUP*)wolfSSL_EC_KEY_get0_group(pkey->ecc);

            /* set ecc group to null so it is not free'd when pkey is free'd */
            pkey->ecc->group = NULL;
        }
    }

    (void)group;
    wolfSSL_EVP_PKEY_free(pkey);
    return ret;
}

#endif /* !NO_BIO */

/* return 1 if success, -1 if error */
int wolfSSL_EC_KEY_LoadDer(WOLFSSL_EC_KEY* key, const unsigned char* derBuf,
                           int derSz)
{
    return wolfSSL_EC_KEY_LoadDer_ex(key, derBuf, derSz,
                                     WOLFSSL_EC_KEY_LOAD_PRIVATE);
}

int wolfSSL_EC_KEY_LoadDer_ex(WOLFSSL_EC_KEY* key, const unsigned char* derBuf,
                              int derSz, int opt)
{
    int ret;
    word32 idx = 0;
    word32 algId;

    WOLFSSL_ENTER("wolfSSL_EC_KEY_LoadDer");

    if (key == NULL || key->internal == NULL || derBuf == NULL || derSz <= 0) {
        WOLFSSL_MSG("Bad function arguments");
        return -1;
    }

    key->pkcs8HeaderSz = 0;

    /* Check if input buffer has PKCS8 header. In the case that it does not
     * have a PKCS8 header then do not error out. */
    if ((ret = ToTraditionalInline_ex((const byte*)derBuf, &idx, (word32)derSz,
                                                                 &algId)) > 0) {
        WOLFSSL_MSG("Found PKCS8 header");
        key->pkcs8HeaderSz = (word16)idx;
    }
    else {
        if (ret != ASN_PARSE_E) {
            WOLFSSL_MSG("Unexpected error with trying to remove PKCS8 header");
            return -1;
        }
    }

    if (opt == WOLFSSL_EC_KEY_LOAD_PRIVATE) {
        ret = wc_EccPrivateKeyDecode(derBuf, &idx, (ecc_key*)key->internal,
                                     derSz);
    }
    else {
        ret = wc_EccPublicKeyDecode(derBuf, &idx, (ecc_key*)key->internal,
                                    derSz);
    }
    if (ret < 0) {
        if (opt == WOLFSSL_EC_KEY_LOAD_PRIVATE) {
            WOLFSSL_MSG("wc_EccPrivateKeyDecode failed");
        }
        else {
            WOLFSSL_MSG("wc_EccPublicKeyDecode failed");
        }
        return -1;
    }

    if (SetECKeyExternal(key) != 1) {
        WOLFSSL_MSG("SetECKeyExternal failed");
        return -1;
    }

    key->inSet = 1;

    return 1;
}

#endif /* OPENSSL_EXTRA || WOLFSSL_WPAS_SMALL*/

#endif /* HAVE_ECC */

/*******************************************************************************
 * END OF EC API
 ******************************************************************************/

#endif /* !WOLFSSL_PK_INCLUDED */

