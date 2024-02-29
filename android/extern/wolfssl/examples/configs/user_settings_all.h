/* user_settings_all.h
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


/* should be renamed to user_settings.h for customer use
 * generated from configure options ./configure --enable-all
 *
 * Cleaned up by David Garske
 */

#ifndef WOLFSSL_USER_SETTINGS_H
#define WOLFSSL_USER_SETTINGS_H


#ifdef __cplusplus
extern "C" {
#endif

/* Features */
#define WOLFSSL_PUBLIC_MP /* Make math API's public */
#define KEEP_PEER_CERT /* Retain peer's certificate */
#define KEEP_OUR_CERT /* Keep our certificate */
#define WOLFSSL_ALWAYS_VERIFY_CB /* Always call verify callback (configured via wolfSSL_CTX_set_verify API) */
#define WOLFSSL_VERIFY_CB_ALL_CERTS /* Call verify callback for all intermediate certs */
#define WOLFSSL_ALWAYS_KEEP_SNI
#define WOLFSSL_EXTRA_ALERTS /* Allow sending other TLS alerts */
#define HAVE_EX_DATA /* Enable "extra" EX data API's for user information in CTX/WOLFSSL */
#define HAVE_EXT_CACHE
#define ATOMIC_USER /* Enable Atomic Record Layer callbacks */
#define HAVE_PK_CALLBACKS /* Enable public key callbacks */
#define WOLFSSL_ALT_NAMES /* Allow alternate cert chain validation to any trusted cert (not entire chain presented by peer) */
#define HAVE_NULL_CIPHER /* Enable use of TLS cipher suites without cipher (clear text / no encryption) */
#define WOLFSSL_HAVE_CERT_SERVICE
#define WOLFSSL_JNI
#define WOLFSSL_SEP
#define WOLFCRYPT_HAVE_SRP
#define WOLFSSL_HAVE_WOLFSCEP
#define WOLFSSL_ENCRYPTED_KEYS /* Support for encrypted keys PKCS8 */
#define HAVE_PKCS7
#define WOLFSSL_MULTI_ATTRIB
#define WOLFSSL_DER_LOAD
#define ASN_BER_TO_DER /* BER to DER support */
#define WOLFSSL_SIGNER_DER_CERT
//#define HAVE_THREAD_LS /* DG Commented: Thread local storage - may not be portable */
//#define WOLFSSL_AEAD_ONLY /* automatically set if TLS v1.3 only, but can be enabled for TLS v1.2 manually */

/* TLS Features */
#define WOLFSSL_DTLS
#define WOLFSSL_TLS13
#define WOLFSSL_EITHER_SIDE /* allow generic server/client method for WOLFSSL_CTX new */

/* DG Disabled SSLv3 and TLSv1.0 - should avoid using */
//#define WOLFSSL_ALLOW_SSLV3
//#define WOLFSSL_ALLOW_TLSV10

/* TLS Extensions */
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_ONE_TIME_AUTH
#define HAVE_SNI
#define HAVE_ALPN
#define HAVE_MAX_FRAGMENT
#define HAVE_TRUNCATED_HMAC
#define HAVE_SESSION_TICKET
#define HAVE_EXTENDED_MASTER
#define HAVE_TRUSTED_CA
#define HAVE_ENCRYPT_THEN_MAC

/* TLS Session Cache */
#define SESSION_CERTS
#define PERSIST_SESSION_CACHE
#define PERSIST_CERT_CACHE

/* Key and Certificate Generation */
#define WOLFSSL_KEY_GEN
#define WOLFSSL_CERT_GEN
#define WOLFSSL_CERT_REQ
#define WOLFSSL_CERT_EXT

/* Certificate Revocation */
#define HAVE_OCSP
#define HAVE_CERTIFICATE_STATUS_REQUEST
#define HAVE_CERTIFICATE_STATUS_REQUEST_V2
#define HAVE_CRL
#define HAVE_CRL_IO
#define HAVE_IO_TIMEOUT
//#define HAVE_CRL_MONITOR /* DG Disabled (Monitors CRL files on filesystem) - not portable feature */


/* Fast math key size 4096-bit max */
#define USE_FAST_MATH
#define FP_MAX_BITS 8192
//#define HAVE___UINT128_T 1 /* DG commented: May not be portable */

/* Timing Resistance */
#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING

/* DH Key Sizes */
#define HAVE_FFDHE_2048
#define HAVE_FFDHE_3072

/* ECC Features */
#define HAVE_ECC
#define TFM_ECC256
#define ECC_SHAMIR
#define WOLFSSL_CUSTOM_CURVES /* enable other curves (not just prime) */
#define HAVE_ECC_SECPR2
#define HAVE_ECC_SECPR3
#define HAVE_ECC_BRAINPOOL
#define HAVE_ECC_KOBLITZ
#define HAVE_ECC_CDH /* Co-factor */
#define HAVE_COMP_KEY /* Compressed key support */
#define FP_ECC /* Fixed point caching - speed repeated operations against same key */
#define HAVE_ECC_ENCRYPT

/* RSA */
#define WC_RSA_PSS

/* AES */
#define HAVE_AES_DECRYPT
#define HAVE_AES_ECB
#define WOLFSSL_AES_DIRECT
#define WOLFSSL_AES_COUNTER
#define HAVE_AESGCM
#define HAVE_AESCCM
#define WOLFSSL_AES_OFB
#define WOLFSSL_AES_CFB
#define WOLFSSL_AES_XTS
#define HAVE_AES_KEYWRAP

/* Hashing */
#define WOLFSSL_SHA224
#define WOLFSSL_SHA512
#define WOLFSSL_SHA384
#define WOLFSSL_SHAKE256
#define WOLFSSL_SHA3
#define WOLFSSL_HASH_FLAGS /* enable hash flag API's */

/* Additional Algorithms */
#define HAVE_HASHDRBG
#define HAVE_CURVE25519
#define HAVE_ED25519
#define CURVED25519_SMALL
#define HAVE_CURVE448
#define HAVE_POLY1305
#define HAVE_CHACHA
#define HAVE_HKDF
#define HAVE_X963_KDF
#define WOLFSSL_CMAC
#define WOLFSSL_DES_ECB

/* Non-Standard Algorithms (DG disabled) */
//#define HAVE_CAMELLIA
//#define WOLFSSL_RIPEMD
//#define HAVE_SCRYPT

/* Encoding */
#define WOLFSSL_BASE16
#define WOLFSSL_BASE64_ENCODE

/* Openssl compatibility */
#if 0 /* DG Disabled */
    /* Openssl compatibility API's */
    #define OPENSSL_EXTRA
    #define OPENSSL_ALL
    #define HAVE_OPENSSL_CMD
    #define SSL_TXT_TLSV1_2
    #define SSL_TXT_TLSV1_1
    #define OPENSSL_NO_SSL2
    #define OPENSSL_NO_SSL3
    #define NO_OLD_RNGNAME
    #define NO_OLD_WC_NAMES
    #define NO_OLD_SSL_NAMES
    #define NO_OLD_SHA_NAMES

    /* Openssl compatibility application specific */
    #define WOLFSSL_LIBWEBSOCKETS
    #define WOLFSSL_OPENSSH
    #define WOLFSSL_QT
    #define FORTRESS
    #define HAVE_WEBSERVER
    #define HAVE_LIGHTY
    #define WOLFSSL_NGINX
    #define WOLFSSL_HAPROXY
    #define HAVE_STUNNEL
    #define WOLFSSL_ASIO
    #define ASIO_USE_WOLFSSL
    #define BOOST_ASIO_USE_WOLFSSL
#endif

/* TLS static cipher support - off by default */
#if 0
    #define WOLFSSL_STATIC_RSA
    #define WOLFSSL_STATIC_DH
    #define WOLFSSL_STATIC_PSK
#endif

/* TLS sniffer support - off by default */
#if 0
    #define WOLFSSL_STATIC_EPHEMERAL
    #define WOLFSSL_SNIFFER
#endif


#ifdef __cplusplus
}
#endif


#endif /* WOLFSSL_USER_SETTINGS_H */
