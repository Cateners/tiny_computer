/* benchmark.c
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


/* wolfCrypt benchmark */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#ifndef WOLFSSL_USER_SETTINGS
    #include <wolfssl/options.h>
#endif
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/version.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/ecc.h>

#ifdef HAVE_PTHREAD
    #include <pthread.h>
#endif
#if defined(HAVE_PTHREAD) ||                                          \
    (!defined(NO_CRYPT_BENCHMARK) && !defined(NO_STDIO_FILESYSTEM) && \
     !defined(NO_ERROR_STRINGS) && !defined(NO_MAIN_DRIVER) &&        \
     !defined(BENCH_EMBEDDED))
    #include <errno.h>
    #ifndef WOLFSSL_ZEPHYR
        #include <unistd.h>
    #endif
#endif

#ifdef NO_STDIO_FILESYSTEM
#define fflush(...) do {} while (0)
#endif

/* Macro to disable benchmark */
#ifndef NO_CRYPT_BENCHMARK

#include <wolfssl/wolfcrypt/mem_track.h>

/* only for stack size check */
#if defined(WOLFSSL_ASYNC_CRYPT)
    #ifndef WC_NO_ASYNC_THREADING
        #define WC_ENABLE_BENCH_THREADING
    #endif
#endif

#ifdef USE_FLAT_BENCHMARK_H
    #include "benchmark.h"
#else
    #include "wolfcrypt/benchmark/benchmark.h"
#endif

#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
static const char info_prefix[] = "###, ";
static const char err_prefix[] = "!!!, ";
#else
static const char info_prefix[] = "";
static const char err_prefix[] = "";
#endif


/* printf mappings */
#ifdef FREESCALE_MQX
    #include <mqx.h>
    /* see wc_port.h for fio.h and nio.h includes */
#elif defined(FREESCALE_KSDK_1_3)
    #include "fsl_debug_console.h"
    #include "fsl_os_abstraction.h"

    #undef printf
    #define printf PRINTF
#elif defined(WOLFSSL_DEOS)
    #include <deos.h>
    #include <printx.h>
    #undef printf
    #define printf printx
#elif defined(MICRIUM)
    #if (OS_VERSION < 50000)
        #include <bsp_ser.h>
        void BSP_Ser_Printf (CPU_CHAR* format, ...);
        #undef printf
        #define printf BSP_Ser_Printf
    #endif
#elif defined(WOLFSSL_ZEPHYR)
    #include <stdio.h>
    #define BENCH_EMBEDDED
    #define printf printfk
    static int printfk(const char *fmt, ...)
    {
        int ret;
        char line[150];
        va_list ap;

        va_start(ap, fmt);

        ret = vsnprintf(line, sizeof(line), fmt, ap);
        line[sizeof(line)-1] = '\0';
        printk("%s", line);

        va_end(ap);

        return ret;
    }

#elif defined(WOLFSSL_TELIT_M2MB)
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include "m2m_log.h" /* for M2M_LOG_INFO - not standard API */
    /* remap printf */
    #undef printf
    #define printf M2M_LOG_INFO
    /* OS requires occasional sleep() */
    #ifndef TEST_SLEEP_MS
        #define TEST_SLEEP_MS 50
    #endif
    #define TEST_SLEEP() m2mb_os_taskSleep(M2MB_OS_MS2TICKS(TEST_SLEEP_MS))
    /* don't use file system for these tests, since ./certs dir isn't loaded */
    #undef  NO_FILESYSTEM
    #define NO_FILESYSTEM

/* ANDROID_V454 (for android studio) displays information in a textview
 * and redirects printf to the textview output instead of using
 * __android_log_print() */
#elif defined(ANDROID) && !defined(ANDROID_V454)
    #ifdef XMALLOC_USER
        #include <stdlib.h>  /* we're using malloc / free direct here */
    #endif
    #ifndef STRING_USER
        #include <stdio.h>
    #endif
    #include <android/log.h>

    #define printf(...)       \
              __android_log_print(ANDROID_LOG_DEBUG, "[WOLFCRYPT]", __VA_ARGS__)
    #define fprintf(fp, ...)  \
              __android_log_print(ANDROID_LOG_DEBUG, "[WOLFCRYPT]", __VA_ARGS__)

#else
    #if defined(XMALLOC_USER) || defined(FREESCALE_MQX)
        /* MQX classic needs for EXIT_FAILURE */
        #include <stdlib.h>  /* we're using malloc / free direct here */
    #endif

    #ifndef STRING_USER
        #include <string.h>
        #include <stdio.h>
    #endif

    /* enable way for customer to override test/bench printf */
    #ifdef XPRINTF
        #undef  printf
        #define printf XPRINTF
    #elif defined(NETOS)
        #undef printf
        #define printf dc_log_printf
    #endif
#endif

#include <wolfssl/wolfcrypt/memory.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/des3.h>
#include <wolfssl/wolfcrypt/arc4.h>
#include <wolfssl/wolfcrypt/chacha.h>
#include <wolfssl/wolfcrypt/chacha20_poly1305.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/poly1305.h>
#include <wolfssl/wolfcrypt/camellia.h>
#include <wolfssl/wolfcrypt/md5.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfcrypt/sha3.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/ripemd.h>
#include <wolfssl/wolfcrypt/cmac.h>
#ifndef NO_HMAC
    #include <wolfssl/wolfcrypt/hmac.h>
#endif
#ifdef WOLFSSL_SIPHASH
    #include <wolfssl/wolfcrypt/siphash.h>
#endif
#ifndef NO_PWDBASED
    #include <wolfssl/wolfcrypt/pwdbased.h>
#endif
#ifdef HAVE_ECC
    #include <wolfssl/wolfcrypt/ecc.h>
#endif
#ifdef HAVE_CURVE25519
    #include <wolfssl/wolfcrypt/curve25519.h>
#endif
#ifdef HAVE_ED25519
    #include <wolfssl/wolfcrypt/ed25519.h>
#endif
#ifdef HAVE_CURVE448
    #include <wolfssl/wolfcrypt/curve448.h>
#endif
#ifdef HAVE_ED448
    #include <wolfssl/wolfcrypt/ed448.h>
#endif
#ifdef WOLFSSL_HAVE_KYBER
    #include <wolfssl/wolfcrypt/kyber.h>
#ifdef WOLFSSL_WC_KYBER
    #include <wolfssl/wolfcrypt/wc_kyber.h>
#endif
#endif
#ifdef WOLFCRYPT_HAVE_ECCSI
    #include <wolfssl/wolfcrypt/eccsi.h>
#endif
#ifdef WOLFCRYPT_HAVE_SAKKE
    #include <wolfssl/wolfcrypt/sakke.h>
#endif
#ifdef HAVE_LIBOQS
    #include <oqs/kem.h>
    #include <oqs/sig.h>
    #include <wolfssl/wolfcrypt/ext_kyber.h>
#endif

#if defined(HAVE_PQC)
    #if defined(HAVE_FALCON)
        #include <wolfssl/wolfcrypt/falcon.h>
    #endif
    #if defined(HAVE_DILITHIUM)
        #include <wolfssl/wolfcrypt/dilithium.h>
    #endif
    #if defined(HAVE_SPHINCS)
        #include <wolfssl/wolfcrypt/sphincs.h>
    #endif
#endif

#ifdef HAVE_PQM4
    #include <api_kyber.h>
    #define PQM4_PUBLIC_KEY_LENGTH    CRYPTO_PUBLICKEYBYTES
    #define PQM4_PRIVATE_KEY_LENGTH   CRYPTO_SECRETKEYBYTES
    #define PQM4_SHARED_SECRET_LENGTH CRYPTO_BYTES
    #define PQM4_CIPHERTEXT_LENGTH    CRYPTO_CIPHERTEXTBYTES
    typedef char OQS_KEM;
    #include <wolfssl/wolfcrypt/ext_kyber.h>
#endif

#include <wolfssl/wolfcrypt/dh.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/types.h>

#ifdef WOLF_CRYPTO_CB
    #include <wolfssl/wolfcrypt/cryptocb.h>
    #ifdef HAVE_INTEL_QA_SYNC
        #include <wolfssl/wolfcrypt/port/intel/quickassist_sync.h>
    #endif
    #ifdef HAVE_CAVIUM_OCTEON_SYNC
        #include <wolfssl/wolfcrypt/port/cavium/cavium_octeon_sync.h>
    #endif
    #ifdef HAVE_RENESAS_SYNC
        #include <wolfssl/wolfcrypt/port/renesas/renesas_sync.h>
    #endif
#endif

#ifdef WOLFSSL_ASYNC_CRYPT
    #include <wolfssl/wolfcrypt/async.h>
#endif

#ifdef HAVE_FIPS
    #include <wolfssl/wolfcrypt/fips_test.h>

    static void myFipsCb(int ok, int err, const char* hash)
    {
        printf("%sin my Fips callback, ok = %d, err = %d\n",
               ok ? info_prefix : err_prefix, ok, err);
        printf("%smessage = %s\n", ok ? info_prefix : err_prefix,
               wc_GetErrorString(err));
        printf("%shash = %s\n", ok ? info_prefix : err_prefix, hash);

        if (err == IN_CORE_FIPS_E) {
            printf("%sIn core integrity hash check failure, copy above hash\n",
                   err_prefix);
            printf("%sinto verifyCore[] in fips_test.c and rebuild\n",
                   err_prefix);
        }
    }
#endif

#ifdef WOLFSSL_STATIC_MEMORY
    static WOLFSSL_HEAP_HINT* HEAP_HINT;
#else
    #define HEAP_HINT NULL
#endif /* WOLFSSL_STATIC_MEMORY */

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#undef LIBCALL_CHECK_RET
#if defined(NO_STDIO_FILESYSTEM) || defined(NO_ERROR_STRINGS) || \
    defined(NO_MAIN_DRIVER) || defined(BENCH_EMBEDDED)
#define LIBCALL_CHECK_RET(...) __VA_ARGS__
#else
#define LIBCALL_CHECK_RET(...) do {                           \
        int _libcall_ret = (__VA_ARGS__);                     \
        if (_libcall_ret < 0) {                               \
            printf("%s%s L%d error %d for \"%s\"\n",          \
                    err_prefix, __FILE__, __LINE__,           \
                    errno, #__VA_ARGS__);                     \
            fflush(stdout);                                   \
            _exit(1);                                         \
        }                                                     \
    } while(0)
#endif

#undef PTHREAD_CHECK_RET
#define PTHREAD_CHECK_RET(...) do {                                  \
        int _pthread_ret = (__VA_ARGS__);                            \
        if (_pthread_ret != 0) {                                     \
            errno = _pthread_ret;                                    \
            printf("%s%s L%d error %d for \"%s\"\n",                 \
                   err_prefix, __FILE__, __LINE__,                   \
                   _pthread_ret, #__VA_ARGS__);                      \
            fflush(stdout);                                          \
            _exit(1);                                                \
        }                                                            \
    } while(0)

/* optional macro to add sleep between tests */
#ifndef TEST_SLEEP
    /* stub the sleep macro */
    #define TEST_SLEEP()
#endif

#define TEST_STRING    "Everyone gets Friday off."
#define TEST_STRING_SZ 25


/* Bit values for each algorithm that is able to be benchmarked.
 * Common grouping of algorithms also.
 * Each algorithm has a unique value for its type e.g. cipher.
 */
/* Cipher algorithms. */
#define BENCH_AES_CBC            0x00000001
#define BENCH_AES_GCM            0x00000002
#define BENCH_AES_ECB            0x00000004
#define BENCH_AES_XTS            0x00000008
#define BENCH_AES_CTR            0x00000010
#define BENCH_AES_CCM            0x00000020
#define BENCH_CAMELLIA           0x00000100
#define BENCH_ARC4               0x00000200
#define BENCH_CHACHA20           0x00001000
#define BENCH_CHACHA20_POLY1305  0x00002000
#define BENCH_DES                0x00004000
#define BENCH_AES_CFB            0x00010000
#define BENCH_AES_OFB            0x00020000
#define BENCH_AES_SIV            0x00040000
/* Digest algorithms. */
#define BENCH_MD5                0x00000001
#define BENCH_POLY1305           0x00000002
#define BENCH_SHA                0x00000004
#define BENCH_SHA224             0x00000010
#define BENCH_SHA256             0x00000020
#define BENCH_SHA384             0x00000040
#define BENCH_SHA512             0x00000080
#define BENCH_SHA2               (BENCH_SHA224 | BENCH_SHA256 | \
                                  BENCH_SHA384 | BENCH_SHA512)
#define BENCH_SHA3_224           0x00000100
#define BENCH_SHA3_256           0x00000200
#define BENCH_SHA3_384           0x00000400
#define BENCH_SHA3_512           0x00000800
#define BENCH_SHA3               (BENCH_SHA3_224 | BENCH_SHA3_256 | \
                                  BENCH_SHA3_384 | BENCH_SHA3_512)
#define BENCH_SHAKE128           0x00001000
#define BENCH_SHAKE256           0x00002000
#define BENCH_SHAKE              (BENCH_SHAKE128 | BENCH_SHAKE256)
#define BENCH_RIPEMD             0x00004000
#define BENCH_BLAKE2B            0x00008000
#define BENCH_BLAKE2S            0x00010000

/* MAC algorithms. */
#define BENCH_CMAC               0x00000001
#define BENCH_HMAC_MD5           0x00000002
#define BENCH_HMAC_SHA           0x00000004
#define BENCH_HMAC_SHA224        0x00000010
#define BENCH_HMAC_SHA256        0x00000020
#define BENCH_HMAC_SHA384        0x00000040
#define BENCH_HMAC_SHA512        0x00000080
#define BENCH_HMAC               (BENCH_HMAC_MD5    | BENCH_HMAC_SHA    | \
                                  BENCH_HMAC_SHA224 | BENCH_HMAC_SHA256 | \
                                  BENCH_HMAC_SHA384 | BENCH_HMAC_SHA512)
#define BENCH_PBKDF2             0x00000100
#define BENCH_SIPHASH            0x00000200

/* Asymmetric algorithms. */
#define BENCH_RSA_KEYGEN         0x00000001
#define BENCH_RSA                0x00000002
#define BENCH_RSA_SZ             0x00000004
#define BENCH_DH                 0x00000010
#define BENCH_KYBER              0x00000020
#define BENCH_ECC_MAKEKEY        0x00001000
#define BENCH_ECC                0x00002000
#define BENCH_ECC_ENCRYPT        0x00004000
#define BENCH_ECC_ALL            0x00008000
#define BENCH_CURVE25519_KEYGEN  0x00010000
#define BENCH_CURVE25519_KA      0x00020000
#define BENCH_ED25519_KEYGEN     0x00040000
#define BENCH_ED25519_SIGN       0x00080000
#define BENCH_CURVE448_KEYGEN    0x00100000
#define BENCH_CURVE448_KA        0x00200000
#define BENCH_ED448_KEYGEN       0x00400000
#define BENCH_ED448_SIGN         0x00800000
#define BENCH_ECC_P256           0x01000000
#define BENCH_ECC_P384           0x02000000
#define BENCH_ECC_P521           0x04000000
#define BENCH_ECCSI_KEYGEN       0x00000020
#define BENCH_ECCSI_PAIRGEN      0x00000040
#define BENCH_ECCSI_VALIDATE     0x00000080
#define BENCH_ECCSI              0x00000400
#define BENCH_SAKKE_KEYGEN       0x10000000
#define BENCH_SAKKE_RSKGEN       0x20000000
#define BENCH_SAKKE_VALIDATE     0x40000000
#define BENCH_SAKKE              0x80000000

/* Post-Quantum Asymmetric algorithms. */
#define BENCH_FALCON_LEVEL1_SIGN        0x00000001
#define BENCH_FALCON_LEVEL5_SIGN        0x00000002
#define BENCH_KYBER_LEVEL1_KEYGEN       0x00000004
#define BENCH_KYBER_LEVEL1_ENCAP        0x00000008
#define BENCH_KYBER_LEVEL3_KEYGEN       0x00000010
#define BENCH_KYBER_LEVEL3_ENCAP        0x00000020
#define BENCH_KYBER_LEVEL5_KEYGEN       0x00000040
#define BENCH_KYBER_LEVEL5_ENCAP        0x00000080
#define BENCH_DILITHIUM_LEVEL2_SIGN     0x04000000
#define BENCH_DILITHIUM_LEVEL3_SIGN     0x08000000
#define BENCH_DILITHIUM_LEVEL5_SIGN     0x10000000
#define BENCH_DILITHIUM_AES_LEVEL2_SIGN 0x20000000
#define BENCH_DILITHIUM_AES_LEVEL3_SIGN 0x40000000
#define BENCH_DILITHIUM_AES_LEVEL5_SIGN 0x80000000

/* Post-Quantum Asymmetric algorithms. (Part 2) */
#define BENCH_SPHINCS_FAST_LEVEL1_SIGN  0x00000001
#define BENCH_SPHINCS_FAST_LEVEL3_SIGN  0x00000002
#define BENCH_SPHINCS_FAST_LEVEL5_SIGN  0x00000004
#define BENCH_SPHINCS_SMALL_LEVEL1_SIGN 0x00000008
#define BENCH_SPHINCS_SMALL_LEVEL3_SIGN 0x00000010
#define BENCH_SPHINCS_SMALL_LEVEL5_SIGN 0x00000020

/* Other */
#define BENCH_RNG                0x00000001
#define BENCH_SCRYPT             0x00000002

#if defined(HAVE_AESGCM) || defined(HAVE_AESCCM)
/* Define AES_AUTH_ADD_SZ already here, since it's used in the
 * static declaration of `bench_Usage_msg1`. */
#if !defined(AES_AUTH_ADD_SZ) && \
        defined(STM32_CRYPTO) && !defined(STM32_AESGCM_PARTIAL) || \
        defined(WOLFSSL_XILINX_CRYPT_VERSAL)
    /* For STM32 use multiple of 4 to leverage crypto hardware
     * Xilinx Versal requires to use multiples of 16 bytes */
    #define AES_AUTH_ADD_SZ 16
#endif
#ifndef AES_AUTH_ADD_SZ
    #define AES_AUTH_ADD_SZ 13
#endif
#endif

/* Benchmark all compiled in algorithms.
 * When 1, ignore other benchmark algorithm values.
 *      0, only benchmark algorithm values set.
 */
static int bench_all = 1;
/* Cipher algorithms to benchmark. */
static int bench_cipher_algs = 0;
/* Digest algorithms to benchmark. */
static int bench_digest_algs = 0;
/* MAC algorithms to benchmark. */
static int bench_mac_algs = 0;
/* Asymmetric algorithms to benchmark. */
static int bench_asym_algs = 0;
/* Post-Quantum Asymmetric algorithms to benchmark. */
static int bench_pq_asym_algs = 0;
/* Post-Quantum Asymmetric algorithms to benchmark. (Part 2)*/
static int bench_pq_asym_algs2 = 0;
/* Other cryptographic algorithms to benchmark. */
static int bench_other_algs = 0;

#if !defined(WOLFSSL_BENCHMARK_ALL) && !defined(NO_MAIN_DRIVER)

/* The mapping of command line option to bit values. */
typedef struct bench_alg {
    /* Command line option string. */
    const char* str;
    /* Bit values to set. */
    word32 val;
} bench_alg;

#ifndef MAIN_NO_ARGS
/* All recognized cipher algorithm choosing command line options. */
static const bench_alg bench_cipher_opt[] = {
    { "-cipher",             0xffffffff              },
#ifdef HAVE_AES_CBC
    { "-aes-cbc",            BENCH_AES_CBC           },
#endif
#ifdef HAVE_AESGCM
    { "-aes-gcm",            BENCH_AES_GCM           },
#endif
#ifdef WOLFSSL_AES_DIRECT
    { "-aes-ecb",            BENCH_AES_ECB           },
#endif
#ifdef WOLFSSL_AES_XTS
    { "-aes-xts",            BENCH_AES_XTS           },
#endif
#ifdef WOLFSSL_AES_CFB
    { "-aes-cfb",            BENCH_AES_CFB           },
#endif
#ifdef WOLFSSL_AES_OFB
    { "-aes-ofb",            BENCH_AES_OFB           },
#endif
#ifdef WOLFSSL_AES_COUNTER
    { "-aes-ctr",            BENCH_AES_CTR           },
#endif
#ifdef HAVE_AESCCM
    { "-aes-ccm",            BENCH_AES_CCM           },
#endif
#ifdef WOLFSSL_AES_SIV
    { "-aes-siv",            BENCH_AES_SIV           },
#endif
#ifdef HAVE_CAMELLIA
    { "-camellia",           BENCH_CAMELLIA          },
#endif
#ifndef NO_RC4
    { "-arc4",               BENCH_ARC4              },
#endif
#ifdef HAVE_CHACHA
    { "-chacha20",           BENCH_CHACHA20          },
#endif
#if defined(HAVE_CHACHA) && defined(HAVE_POLY1305)
    { "-chacha20-poly1305",  BENCH_CHACHA20_POLY1305 },
#endif
#ifndef NO_DES3
    { "-des",                BENCH_DES               },
#endif
    { NULL, 0 }
};

/* All recognized digest algorithm choosing command line options. */
static const bench_alg bench_digest_opt[] = {
    { "-digest",             0xffffffff              },
#ifndef NO_MD5
    { "-md5",                BENCH_MD5               },
#endif
#ifdef HAVE_POLY1305
    { "-poly1305",           BENCH_POLY1305          },
#endif
#ifndef NO_SHA
    { "-sha",                BENCH_SHA               },
#endif
#if defined(WOLFSSL_SHA224) || !defined(NO_SHA256) || defined(WOLFSSL_SHA384) \
                                                   || defined(WOLFSSL_SHA512)
    { "-sha2",               BENCH_SHA2              },
#endif
#ifdef WOLFSSL_SHA224
    { "-sha224",             BENCH_SHA224            },
#endif
#ifndef NO_SHA256
    { "-sha256",             BENCH_SHA256            },
#endif
#ifdef WOLFSSL_SHA384
    { "-sha384",             BENCH_SHA384            },
#endif
#ifdef WOLFSSL_SHA512
    { "-sha512",             BENCH_SHA512            },
#endif
#ifdef WOLFSSL_SHA3
    { "-sha3",               BENCH_SHA3              },
    #ifndef WOLFSSL_NOSHA3_224
    { "-sha3-224",           BENCH_SHA3_224          },
    #endif
    #ifndef WOLFSSL_NOSHA3_256
    { "-sha3-256",           BENCH_SHA3_256          },
    #endif
    #ifndef WOLFSSL_NOSHA3_384
    { "-sha3-384",           BENCH_SHA3_384          },
    #endif
    #ifndef WOLFSSL_NOSHA3_512
    { "-sha3-512",           BENCH_SHA3_512          },
    #endif
    #if defined(WOLFSSL_SHAKE128) || defined(WOLFSSL_SHAKE256)
    { "-shake",              BENCH_SHAKE             },
    #endif
    #ifdef WOLFSSL_SHAKE128
    { "-shake128",           BENCH_SHAKE128          },
    #endif
    #ifdef WOLFSSL_SHAKE256
    { "-shake256",           BENCH_SHAKE256          },
    #endif
#endif
#ifdef WOLFSSL_RIPEMD
    { "-ripemd",             BENCH_RIPEMD            },
#endif
#ifdef HAVE_BLAKE2
    { "-blake2b",            BENCH_BLAKE2B           },
#endif
#ifdef HAVE_BLAKE2S
    { "-blake2s",            BENCH_BLAKE2S           },
#endif
    { NULL, 0 }
};

/* All recognized MAC algorithm choosing command line options. */
static const bench_alg bench_mac_opt[] = {
    { "-mac",                0xffffffff              },
#ifdef WOLFSSL_CMAC
    { "-cmac",               BENCH_CMAC              },
#endif
#ifndef NO_HMAC
    { "-hmac",               BENCH_HMAC              },
    #ifndef NO_MD5
    { "-hmac-md5",           BENCH_HMAC_MD5          },
    #endif
    #ifndef NO_SHA
    { "-hmac-sha",           BENCH_HMAC_SHA          },
    #endif
    #ifdef WOLFSSL_SHA224
    { "-hmac-sha224",        BENCH_HMAC_SHA224       },
    #endif
    #ifndef NO_SHA256
    { "-hmac-sha256",        BENCH_HMAC_SHA256       },
    #endif
    #ifdef WOLFSSL_SHA384
    { "-hmac-sha384",        BENCH_HMAC_SHA384       },
    #endif
    #ifdef WOLFSSL_SHA512
    { "-hmac-sha512",        BENCH_HMAC_SHA512       },
    #endif
    #ifndef NO_PWDBASED
    { "-pbkdf2",             BENCH_PBKDF2            },
    #endif
    #ifdef WOLFSSL_SIPHASH
    { "-siphash",            BENCH_SIPHASH           },
    #endif
#endif
    { NULL, 0 }
};

/* All recognized asymmetric algorithm choosing command line options. */
static const bench_alg bench_asym_opt[] = {
    { "-asym",               0xffffffff              },
#ifndef NO_RSA
    #ifdef WOLFSSL_KEY_GEN
    { "-rsa-kg",             BENCH_RSA_KEYGEN        },
    #endif
    { "-rsa",                BENCH_RSA               },
    { "-rsa-sz",             BENCH_RSA_SZ            },
#endif
#ifndef NO_DH
    { "-dh",                 BENCH_DH                },
#endif
#ifdef WOLFSSL_HAVE_KYBER
    { "-kyber",              BENCH_KYBER             },
#endif
#ifdef HAVE_ECC
    { "-ecc-kg",             BENCH_ECC_MAKEKEY       },
    { "-ecc",                BENCH_ECC               },
    #ifdef HAVE_ECC_ENCRYPT
    { "-ecc-enc",            BENCH_ECC_ENCRYPT       },
    #endif
    { "-ecc-all",            BENCH_ECC_ALL           },
#endif
#ifdef HAVE_CURVE25519
    { "-curve25519-kg",      BENCH_CURVE25519_KEYGEN },
    #ifdef HAVE_CURVE25519_SHARED_SECRET
    { "-x25519",             BENCH_CURVE25519_KA     },
    #endif
#endif
#ifdef HAVE_ED25519
    { "-ed25519-kg",         BENCH_ED25519_KEYGEN    },
    { "-ed25519",            BENCH_ED25519_SIGN      },
#endif
#ifdef HAVE_CURVE448
    { "-curve448-kg",        BENCH_CURVE448_KEYGEN   },
    #ifdef HAVE_CURVE448_SHARED_SECRET
    { "-x448",               BENCH_CURVE448_KA       },
    #endif
#endif
#ifdef HAVE_ED448
    { "-ed448-kg",           BENCH_ED448_KEYGEN      },
    { "-ed448",              BENCH_ED448_SIGN        },
#endif
#ifdef WOLFCRYPT_HAVE_ECCSI
    { "-eccsi-kg",           BENCH_ECCSI_KEYGEN      },
    { "-eccsi-pair",         BENCH_ECCSI_PAIRGEN     },
    { "-eccsi-val",          BENCH_ECCSI_VALIDATE    },
    { "-eccsi",              BENCH_ECCSI             },
#endif
#ifdef WOLFCRYPT_HAVE_SAKKE
    { "-sakke-kg",           BENCH_SAKKE_KEYGEN      },
    { "-sakke-rsk",          BENCH_SAKKE_RSKGEN      },
    { "-sakke-val",          BENCH_SAKKE_VALIDATE    },
    { "-sakke",              BENCH_SAKKE             },
#endif
    { NULL, 0 }
};

/* All recognized other cryptographic algorithm choosing command line options.
 */
static const bench_alg bench_other_opt[] = {
    { "-other",              0xffffffff              },
#ifndef WC_NO_RNG
    { "-rng",                BENCH_RNG               },
#endif
#ifdef HAVE_SCRYPT
    { "-scrypt",             BENCH_SCRYPT            },
#endif
    { NULL, 0}
};
#endif /* MAIN_NO_ARGS */

#endif /* !WOLFSSL_BENCHMARK_ALL && !NO_MAIN_DRIVER */

#if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
/* The post-quantum-specific mapping of command line option to bit values and
 * OQS name. */
typedef struct bench_pq_alg {
    /* Command line option string. */
    const char* str;
    /* Bit values to set. */
    word32 val;
    const char* pqc_name;
} bench_pq_alg;

/* All recognized post-quantum asymmetric algorithm choosing command line
 * options. */
static const bench_pq_alg bench_pq_asym_opt[] = {
    { "-pq",                 0xffffffff, NULL},
#ifdef HAVE_PQM4
    { "-kyber_level1-kg", BENCH_KYBER_LEVEL1_KEYGEN, NULL },
    { "-kyber_level1-ed", BENCH_KYBER_LEVEL1_ENCAP, NULL },
#endif
#ifdef HAVE_LIBOQS
    { "-falcon_level1", BENCH_FALCON_LEVEL1_SIGN,
      OQS_SIG_alg_falcon_512 },
    { "-falcon_level5", BENCH_FALCON_LEVEL5_SIGN,
      OQS_SIG_alg_falcon_1024 },
    { "-dilithium_level2", BENCH_DILITHIUM_LEVEL2_SIGN,
      OQS_SIG_alg_dilithium_2 },
    { "-dilithium_level3", BENCH_DILITHIUM_LEVEL3_SIGN,
      OQS_SIG_alg_dilithium_3 },
    { "-dilithium_level5", BENCH_DILITHIUM_LEVEL5_SIGN,
      OQS_SIG_alg_dilithium_5 },
    { "-dilithium_aes_level2", BENCH_DILITHIUM_AES_LEVEL2_SIGN,
      OQS_SIG_alg_dilithium_2_aes },
    { "-dilithium_aes_level3", BENCH_DILITHIUM_AES_LEVEL3_SIGN,
      OQS_SIG_alg_dilithium_3_aes },
    { "-dilithium_aes_level5", BENCH_DILITHIUM_AES_LEVEL5_SIGN,
      OQS_SIG_alg_dilithium_5_aes },
    { "-kyber_level1-kg",    BENCH_KYBER_LEVEL1_KEYGEN,
      OQS_KEM_alg_kyber_512 },
    { "-kyber_level1-ed",       BENCH_KYBER_LEVEL1_ENCAP,
      OQS_KEM_alg_kyber_512 },
    { "-kyber_level3-kg",    BENCH_KYBER_LEVEL3_KEYGEN,
      OQS_KEM_alg_kyber_768 },
    { "-kyber_level3-ed",       BENCH_KYBER_LEVEL3_ENCAP,
      OQS_KEM_alg_kyber_768 },
    { "-kyber_level5-kg",    BENCH_KYBER_LEVEL5_KEYGEN,
      OQS_KEM_alg_kyber_1024 },
    { "-kyber_level5-ed",       BENCH_KYBER_LEVEL5_ENCAP,
      OQS_KEM_alg_kyber_1024 },
#endif /* HAVE_LIBOQS */
    { NULL, 0, NULL }
};

#ifdef HAVE_LIBOQS
/* All recognized post-quantum asymmetric algorithm choosing command line
 * options. (Part 2) */
static const bench_pq_alg bench_pq_asym_opt2[] = {
    { "-pq",                 0xffffffff, NULL},
    { "-sphincs_fast_level1", BENCH_SPHINCS_FAST_LEVEL1_SIGN,
      OQS_SIG_alg_sphincs_shake256_128f_simple },
    { "-sphincs_fast_level3", BENCH_SPHINCS_FAST_LEVEL3_SIGN,
      OQS_SIG_alg_sphincs_shake256_192f_simple },
    { "-sphincs_fast_level5", BENCH_SPHINCS_FAST_LEVEL5_SIGN,
      OQS_SIG_alg_sphincs_shake256_256f_simple },
    { "-sphincs_small_level1", BENCH_SPHINCS_SMALL_LEVEL1_SIGN,
      OQS_SIG_alg_sphincs_shake256_128s_simple },
    { "-sphincs_small_level3", BENCH_SPHINCS_SMALL_LEVEL3_SIGN,
      OQS_SIG_alg_sphincs_shake256_192s_simple },
    { "-sphincs_small_level5", BENCH_SPHINCS_SMALL_LEVEL5_SIGN,
      OQS_SIG_alg_sphincs_shake256_256s_simple },
    { NULL, 0, NULL }
};
#endif /* HAVE_LIBOQS */
#endif /* HAVE_PQC */

#ifdef HAVE_WNR
    const char* wnrConfigFile = "wnr-example.conf";
#endif

#if defined(WOLFSSL_MDK_ARM)
    extern XFILE wolfSSL_fopen(const char *fname, const char *mode);
    #define fopen wolfSSL_fopen
#endif

static int lng_index = 0;

#ifndef NO_MAIN_DRIVER
#ifndef MAIN_NO_ARGS
static const char* bench_Usage_msg1[][21] = {
    /* 0 English  */
    {   "-? <num>    Help, print this usage\n            0: English, 1: Japanese\n",
        "-csv        Print terminal output in csv format\n",
        "-base10     Display bytes as power of 10 (eg 1 kB = 1000 Bytes)\n",
        "-no_aad     No additional authentication data passed.\n",
        "-aad_size <num>   With <num> bytes of AAD.\n",
       ("-all_aad    With AAD length of 0, "
                     WC_STRINGIFY(AES_AUTH_ADD_SZ)
                     " and\n"
        "            (if set via -aad_size) <aad_size> bytes.\n"
       ),
        "-dgst_full  Full digest operation performed.\n",
        "-rsa_sign   Measure RSA sign/verify instead of encrypt/decrypt.\n",
        "<keySz> -rsa-sz\n            Measure RSA <key size> performance.\n",
        "-ffhdhe2048 Measure DH using FFDHE 2048-bit parameters.\n",
        "-ffhdhe3072 Measure DH using FFDHE 3072-bit parameters.\n",
        "-p256       Measure ECC using P-256 curve.\n",
        "-p384       Measure ECC using P-384 curve.\n",
        "-p521       Measure ECC using P-521 curve.\n",
        "-ecc-all    Bench all enabled ECC curves.\n",
        "-<alg>      Algorithm to benchmark. Available algorithms include:\n",
        "-lng <num>  Display benchmark result by specified language.\n            0: English, 1: Japanese\n",
        "<num>       Size of block in bytes\n",
       ("-blocks <num>  Number of blocks. Can be used together with the 'Size of block'\n"
        "            option, but must be used after that one.\n"
       ),
        "-threads <num> Number of threads to run\n",
        "-print      Show benchmark stats summary\n"
    },
#ifndef NO_MULTIBYTE_PRINT
    /* 1 Japanese */
    {   "-? <num>    ヘルプ, 使い方を表示します。\n            0: 英語、 1: 日本語\n",
        "-csv        csv 形式で端末に出力します。\n",
        "-base10     バイトを10のべき乗で表示します。(例 1 kB = 1000 Bytes)\n",
        "-no_aad     追加の認証データを使用しません.\n",
        "-aad_size <num>  TBD.\n",
        "-all_aad    TBD.\n",
        "-dgst_full  フルの digest 暗号操作を実施します。\n",
        "-rsa_sign   暗号/復号化の代わりに RSA の署名/検証を測定します。\n",
        "<keySz> -rsa-sz\n            RSA <key size> の性能を測定します。\n",
        "-ffhdhe2048 Measure DH using FFDHE 2048-bit parameters.\n",
        "-ffhdhe3072 Measure DH using FFDHE 3072-bit parameters.\n",
        "-p256       Measure ECC using P-256 curve.\n",
        "-p384       Measure ECC using P-384 curve.\n",
        "-p521       Measure ECC using P-521 curve.\n",
        "-ecc-all    Bench all enabled ECC curves.\n",
        "-<alg>      アルゴリズムのベンチマークを実施します。\n            利用可能なアルゴリズムは下記を含みます:\n",
        "-lng <num>  指定された言語でベンチマーク結果を表示します。\n            0: 英語、 1: 日本語\n",
        "<num>       ブロックサイズをバイト単位で指定します。\n",
        "-blocks <num>  TBD.\n",
        "-threads <num> 実行するスレッド数\n",
        "-print      ベンチマーク統計の要約を表示する\n"
    },
#endif
};
#endif /* MAIN_NO_ARGS */
#endif

static const char* bench_result_words1[][4] = {
    { "took", "seconds" , "Cycles per byte", NULL },           /* 0 English  */
#ifndef NO_MULTIBYTE_PRINT
    { "を"   , "秒で処理", "1バイトあたりのサイクル数", NULL },     /* 1 Japanese */
#endif
};

#if !defined(NO_RSA) || \
    defined(HAVE_ECC) || !defined(NO_DH) || defined(HAVE_ECC_ENCRYPT) || \
    defined(HAVE_CURVE25519) || defined(HAVE_CURVE25519_SHARED_SECRET)  || \
    defined(HAVE_ED25519) || defined(HAVE_CURVE448) || \
    defined(HAVE_CURVE448_SHARED_SECRET) || defined(HAVE_ED448) || \
    defined(WOLFSSL_HAVE_KYBER)

static const char* bench_desc_words[][15] = {
    /* 0           1          2         3        4        5         6            7            8          9        10        11       12          13       14 */
    {"public", "private", "key gen", "agree" , "sign", "verify", "encrypt", "decrypt", "rsk gen", "encap", "derive", "valid", "pair gen", "decap", NULL}, /* 0 English */
#ifndef NO_MULTIBYTE_PRINT
    {"公開鍵", "秘密鍵" ,"鍵生成" , "鍵共有" , "署名", "検証"  , "暗号化"    , "復号化"    , "rsk gen", "encap", "derive", "valid", "pair gen", "decap", NULL}, /* 1 Japanese */
#endif
};

#endif

#if defined(__GNUC__) && defined(__x86_64__) && !defined(NO_ASM) && !defined(WOLFSSL_SGX)
    #define HAVE_GET_CYCLES
    static WC_INLINE word64 get_intel_cycles(void);
    static THREAD_LS_T word64 total_cycles;
    #define INIT_CYCLE_COUNTER
    #define BEGIN_INTEL_CYCLES total_cycles = get_intel_cycles();
    #define END_INTEL_CYCLES   total_cycles = get_intel_cycles() - total_cycles;
    /* s == size in bytes that 1 count represents, normally BENCH_SIZE */
    #define SHOW_INTEL_CYCLES(b, n, s) \
        (void)XSNPRINTF((b) + XSTRLEN(b), (n) - XSTRLEN(b), " %s = %6.2f\n", \
            bench_result_words1[lng_index][2], \
            count == 0 ? 0 : (float)total_cycles / ((word64)count*(s)))
    #define SHOW_INTEL_CYCLES_CSV(b, n, s) \
        (void)XSNPRINTF((b) + XSTRLEN(b), (n) - XSTRLEN(b), "%.6f,\n", \
            count == 0 ? 0 : (float)total_cycles / ((word64)count*(s)))
#elif defined(LINUX_CYCLE_COUNT)
    #include <linux/perf_event.h>
    #include <sys/syscall.h>
    #include <unistd.h>

    static THREAD_LS_T word64 begin_cycles;
    static THREAD_LS_T word64 total_cycles;
    static THREAD_LS_T int cycles = -1;
    static THREAD_LS_T struct perf_event_attr atr;

    #define INIT_CYCLE_COUNTER do { \
        atr.type   = PERF_TYPE_HARDWARE; \
        atr.config = PERF_COUNT_HW_CPU_CYCLES; \
        cycles = (int)syscall(__NR_perf_event_open, &atr, 0, -1, -1, 0); \
    } while (0);

    #define BEGIN_INTEL_CYCLES read(cycles, &begin_cycles, sizeof(begin_cycles));
    #define END_INTEL_CYCLES   do { \
        read(cycles, &total_cycles, sizeof(total_cycles)); \
        total_cycles = total_cycles - begin_cycles; \
    } while (0);

    /* s == size in bytes that 1 count represents, normally BENCH_SIZE */
    #define SHOW_INTEL_CYCLES(b, n, s) \
        (void)XSNPRINTF(b + XSTRLEN(b), n - XSTRLEN(b), " %s = %6.2f\n", \
        bench_result_words1[lng_index][2], \
            (float)total_cycles / (count*s))
    #define SHOW_INTEL_CYCLES_CSV(b, n, s) \
        (void)XSNPRINTF(b + XSTRLEN(b), n - XSTRLEN(b), "%.6f,\n", \
            (float)total_cycles / (count*s))

#elif defined(SYNERGY_CYCLE_COUNT)
    #include "hal_data.h"
    static THREAD_LS_T word64 begin_cycles;
    static THREAD_LS_T word64 total_cycles;

    #define INIT_CYCLE_COUNTER
    #define BEGIN_INTEL_CYCLES begin_cycles = DWT->CYCCNT = 0;
    #define END_INTEL_CYCLES   total_cycles =  DWT->CYCCNT - begin_cycles;

    /* s == size in bytes that 1 count represents, normally BENCH_SIZE */
    #define SHOW_INTEL_CYCLES(b, n, s) \
        (void)XSNPRINTF(b + XSTRLEN(b), n - XSTRLEN(b), " %s = %6.2f\n", \
        bench_result_words1[lng_index][2], \
            (float)total_cycles / (count*s))
    #define SHOW_INTEL_CYCLES_CSV(b, n, s) \
        (void)XSNPRINTF(b + XSTRLEN(b), n - XSTRLEN(b), "%.6f,\n", \
            (float)total_cycles / (count*s))

#else
    #define INIT_CYCLE_COUNTER
    #define BEGIN_INTEL_CYCLES
    #define END_INTEL_CYCLES
    #define SHOW_INTEL_CYCLES(b, n, s)     b[XSTRLEN(b)] = '\n'
    #define SHOW_INTEL_CYCLES_CSV(b, n, s)     b[XSTRLEN(b)] = '\n'
#endif

/* determine benchmark buffer to use (if NO_FILESYSTEM) */
#if !defined(USE_CERT_BUFFERS_1024) && !defined(USE_CERT_BUFFERS_2048) && \
    !defined(USE_CERT_BUFFERS_3072) && !defined(USE_CERT_BUFFERS_4096)
    #define USE_CERT_BUFFERS_2048 /* default to 2048 */
#endif

#if defined(USE_CERT_BUFFERS_1024) || defined(USE_CERT_BUFFERS_2048) || \
    defined(USE_CERT_BUFFERS_3072) || defined(USE_CERT_BUFFERS_4096) || \
    !defined(NO_DH)
    /* include test cert and key buffers for use with NO_FILESYSTEM */
    #include <wolfssl/certs_test.h>
#endif

#if defined(HAVE_BLAKE2) || defined(HAVE_BLAKE2S)
    #include <wolfssl/wolfcrypt/blake2.h>
#endif

#ifdef _MSC_VER
    /* 4996 warning to use MS extensions e.g., strcpy_s instead of strncpy */
    #pragma warning(disable: 4996)
#endif

#ifdef WOLFSSL_CURRTIME_REMAP
    #define current_time WOLFSSL_CURRTIME_REMAP
#else
    double current_time(int reset);
#endif

#ifdef LINUX_RUSAGE_UTIME
    static void check_for_excessive_stime(const char *desc,
                                          const char *desc_extra);
#endif

#if defined(DEBUG_WOLFSSL) && !defined(HAVE_VALGRIND) && \
        !defined(HAVE_STACK_SIZE)
#ifdef __cplusplus
    extern "C" {
#endif
    WOLFSSL_API int wolfSSL_Debugging_ON(void);
    WOLFSSL_API void wolfSSL_Debugging_OFF(void);
#ifdef __cplusplus
    }  /* extern "C" */
#endif
#endif

#if !defined(WC_NO_RNG) && \
        ((!defined(NO_RSA) && !defined(WOLFSSL_RSA_VERIFY_ONLY)) \
        || !defined(NO_DH) || defined(WOLFSSL_KEY_GEN) || defined(HAVE_ECC) \
        || defined(HAVE_CURVE25519) || defined(HAVE_ED25519) \
        || defined(HAVE_CURVE448) || defined(HAVE_ED448) \
        || defined(WOLFSSL_HAVE_KYBER))
    #define HAVE_LOCAL_RNG
    static THREAD_LS_T WC_RNG gRng;
    #define GLOBAL_RNG &gRng
#else
    #define GLOBAL_RNG NULL
#endif

#if defined(HAVE_ED25519) || defined(HAVE_CURVE25519) || \
    defined(HAVE_CURVE448) || defined(HAVE_ED448) || \
    defined(HAVE_ECC) || !defined(NO_DH) || \
    !defined(NO_RSA) || defined(HAVE_SCRYPT) || \
    defined(WOLFSSL_HAVE_KYBER)
    #define BENCH_ASYM
#endif

#if defined(BENCH_ASYM)
#if defined(HAVE_ECC) || !defined(NO_RSA) || !defined(NO_DH) || \
    defined(HAVE_CURVE25519) || defined(HAVE_ED25519) || \
    defined(HAVE_CURVE448) || defined(HAVE_ED448) || \
    defined(WOLFSSL_HAVE_KYBER)
static const char* bench_result_words2[][5] = {
    { "ops took", "sec"     , "avg" , "ops/sec", NULL },            /* 0 English  */
#ifndef NO_MULTIBYTE_PRINT
    { "回処理を", "秒で実施", "平均", "処理/秒", NULL },            /* 1 Japanese */
#endif
};
#endif
#endif

#ifdef WOLFSSL_CAAM
    #include <wolfssl/wolfcrypt/port/caam/wolfcaam.h>
    #ifdef WOLFSSL_SECO_CAAM
        #define SECO_MAX_UPDATES 10000
        #define SECO_BENCHMARK_NONCE 0x7777
        #define SECO_KEY_STORE_ID 1
    #endif

    static THREAD_LS_T int devId = WOLFSSL_CAAM_DEVID;
#else
    static THREAD_LS_T int devId = INVALID_DEVID;
#endif

/* Asynchronous helper macros */
#ifdef WC_ENABLE_BENCH_THREADING
    typedef struct ThreadData {
        pthread_t thread_id;
    } ThreadData;
    static ThreadData* g_threadData;
    static volatile int g_threadCount;
#endif

#if defined(WOLFSSL_ASYNC_CRYPT) || defined(WOLFSSL_CAAM)
    #ifndef NO_HW_BENCH
        #define BENCH_DEVID
    #endif
    #define BENCH_DEVID_GET_NAME(useDeviceID) (useDeviceID) ? "HW" : "SW"
#else
    #define BENCH_DEVID_GET_NAME(useDeviceID) ""
#endif

#ifdef WOLFSSL_ASYNC_CRYPT
    static WOLF_EVENT_QUEUE eventQueue;

    #define BENCH_ASYNC_GET_DEV(obj)      (&(obj)->asyncDev)
    #define BENCH_MAX_PENDING             (WOLF_ASYNC_MAX_PENDING)


    static int bench_async_check(int* ret, WC_ASYNC_DEV* asyncDev,
        int callAgain, int* times, int limit, int* pending)
    {
        int allowNext = 0;

        /* this state can be set from a different thread */
        WOLF_EVENT_STATE state = asyncDev->event.state;

        /* if algo doesn't require calling again then use this flow */
        if (state == WOLF_EVENT_STATE_DONE) {
            if (callAgain) {
                /* needs called again, so allow it and handle completion in bench_async_handle */
                allowNext = 1;
            }
            else {
                *ret = asyncDev->event.ret;
                asyncDev->event.state = WOLF_EVENT_STATE_READY;
                (*times)++;
                if (*pending > 0) /* to support case where async blocks */
                    (*pending)--;

                if ((*times + *pending) < limit)
                    allowNext = 1;
            }
        }

        /* if slot is available and we haven't reached limit, start another */
        else if (state == WOLF_EVENT_STATE_READY && (*times + *pending) < limit) {
            allowNext = 1;
        }

        return allowNext;
    }

    static int bench_async_handle(int* ret, WC_ASYNC_DEV* asyncDev,
        int callAgain, int* times, int* pending)
    {
        WOLF_EVENT_STATE state = asyncDev->event.state;

        if (*ret == WC_PENDING_E) {
            if (state == WOLF_EVENT_STATE_DONE) {
                *ret = asyncDev->event.ret;
                asyncDev->event.state = WOLF_EVENT_STATE_READY;
                (*times)++;
                (*pending)--;
            }
            else {
                (*pending)++;
                *ret = wc_AsyncHandle(asyncDev, &eventQueue,
                    callAgain ? WC_ASYNC_FLAG_CALL_AGAIN : WC_ASYNC_FLAG_NONE);
            }
        }
        else if (*ret >= 0) {
            *ret = asyncDev->event.ret;
            asyncDev->event.state = WOLF_EVENT_STATE_READY;
            (*times)++;
            if (*pending > 0)  /* to support case where async blocks */
                (*pending)--;
        }

        return (*ret >= 0) ? 1 : 0;
    }

    static WC_INLINE int bench_async_poll(int* pending)
    {
        int ret, asyncDone = 0;

        ret = wolfAsync_EventQueuePoll(&eventQueue, NULL, NULL, 0,
                                       WOLF_POLL_FLAG_CHECK_HW, &asyncDone);
        if (ret != 0) {
            printf("%sAsync poll failed %d\n", err_prefix, ret);
            return ret;
        }

        if (asyncDone == 0) {
        #ifndef WC_NO_ASYNC_THREADING
            /* give time to other threads */
            wc_AsyncThreadYield();
        #endif
        }

        (void)pending;

        return asyncDone;
    }

#else
    #define BENCH_MAX_PENDING             (1)
    #define BENCH_ASYNC_GET_DEV(obj)      NULL

    static WC_INLINE int bench_async_check(int* ret, void* asyncDev,
        int callAgain, int* times, int limit, int* pending)
    {
        (void)ret;
        (void)asyncDev;
        (void)callAgain;
        (void)times;
        (void)limit;
        (void)pending;

        return 1;
    }

    static WC_INLINE int bench_async_handle(int* ret, void* asyncDev,
        int callAgain, int* times, int* pending)
    {
        (void)asyncDev;
        (void)callAgain;
        (void)pending;

        if (*ret >= 0) {
            /* operation completed */
            (*times)++;
            return 1;
        }
        return 0;
    }
    #define bench_async_poll(p)
#endif /* WOLFSSL_ASYNC_CRYPT */



/* maximum runtime for each benchmark */
#ifndef BENCH_MIN_RUNTIME_SEC
    #define BENCH_MIN_RUNTIME_SEC   1.0f
#endif

#if defined(HAVE_AESGCM) || defined(HAVE_AESCCM)
    #define AES_AUTH_TAG_SZ 16
    #define BENCH_CIPHER_ADD AES_AUTH_TAG_SZ
    static word32 aesAuthAddSz = AES_AUTH_ADD_SZ;
    #if !defined(AES_AAD_OPTIONS_DEFAULT)
        #if !defined(NO_MAIN_DRIVER)
            #define AES_AAD_OPTIONS_DEFAULT 0x1U
        #else
            #define AES_AAD_OPTIONS_DEFAULT 0x3U
        #endif
    #endif
    #define AES_AAD_STRING(s)           (aesAuthAddSz == 0 ? (s "-no_AAD") : (aesAuthAddSz == AES_AUTH_ADD_SZ ? (s) : (s "-custom")))
    enum en_aad_options {
        AAD_SIZE_DEFAULT = 0x1U,
        AAD_SIZE_ZERO = 0x2U,
        AAD_SIZE_CUSTOM = 0x4U,
    };
    static word32 aes_aad_options = AES_AAD_OPTIONS_DEFAULT;
    static word32 aes_aad_size = 0;
    static void bench_aes_aad_options_wrap(void (*fn)(int), int i)
    {
        word32 aesAuthAddSz_orig = aesAuthAddSz;
        word32 options = aes_aad_options;
        while(options) {
            if (options & AAD_SIZE_DEFAULT) {
                aesAuthAddSz = AES_AUTH_ADD_SZ;
                options &= ~AAD_SIZE_DEFAULT;
            } else if (options & AAD_SIZE_ZERO) {
                aesAuthAddSz = 0;
                options &= ~AAD_SIZE_ZERO;
            } else if (options & AAD_SIZE_CUSTOM) {
                aesAuthAddSz = aes_aad_size;
                options &= ~AAD_SIZE_CUSTOM;
            }
            fn(i);
            aesAuthAddSz = aesAuthAddSz_orig;
        }
    }
#endif
#ifndef BENCH_CIPHER_ADD
    #define BENCH_CIPHER_ADD 0
#endif


/* use kB instead of mB for embedded benchmarking */
#ifdef BENCH_EMBEDDED
    enum BenchmarkBounds {
        scryptCnt  = 1,
        ntimes     = 2,
        genTimes   = BENCH_MAX_PENDING,
        agreeTimes = 2
    };
    /* how many kB to test (en/de)cryption */
    #define NUM_BLOCKS 25
    #define BENCH_SIZE (1024uL)
#else
    enum BenchmarkBounds {
        scryptCnt  = 10,
        ntimes     = 100,
        genTimes   = BENCH_MAX_PENDING, /* must be at least BENCH_MAX_PENDING */
        agreeTimes = 100
    };
    /* how many megs to test (en/de)cryption */
    #define NUM_BLOCKS 5
    #define BENCH_SIZE (1024*1024uL)
#endif
static int    numBlocks  = NUM_BLOCKS;
static word32 bench_size = BENCH_SIZE;
static int base2 = 1;
static int digest_stream = 1;
#ifndef NO_RSA
/* Don't measure RSA sign/verify by default */
static int rsa_sign_verify = 0;
#endif
#ifndef NO_DH
/* Use the FFDHE parameters */
static int use_ffdhe = 0;
#endif

/* Don't print out in CSV format by default */
static int csv_format = 0;

#ifdef WOLFSSL_XILINX_CRYPT_VERSAL
/* Versal PLM maybe prints an error message to the same console.
 * In order to not mix those outputs up, sleep a little while
 * before erroring out.
 */
#define SLEEP_ON_ERROR(ret) do{ if (ret != 0) { sleep(1); } }while(0)
#else
#define SLEEP_ON_ERROR(ret) do{ /* noop */ }while(0)
#endif

/* globals for cipher tests */
static THREAD_LS_T byte* bench_plain = NULL;
static THREAD_LS_T byte* bench_cipher = NULL;

static const XGEN_ALIGN byte bench_key_buf[] =
{
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
    0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67,
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
    0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
};

static const XGEN_ALIGN byte bench_iv_buf[] =
{
    0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81
};
static THREAD_LS_T byte* bench_key = NULL;
static THREAD_LS_T byte* bench_iv = NULL;

#ifdef WOLFSSL_STATIC_MEMORY
    #ifdef WOLFSSL_STATIC_MEMORY_TEST_SZ
        static byte gBenchMemory[WOLFSSL_STATIC_MEMORY_TEST_SZ];
    #elif defined(BENCH_EMBEDDED)
        static byte gBenchMemory[50000];
    #else
        static byte gBenchMemory[400000];
    #endif
#endif


/* This code handles cases with systems where static (non cost) ram variables
    aren't properly initialized with data */
static void benchmark_static_init(int force)
{
    static int gBenchStaticInit = 0;
    if (gBenchStaticInit == 0 || force) {
        gBenchStaticInit = 1;

        /* Init static variables */
        numBlocks  = NUM_BLOCKS;
        bench_size = BENCH_SIZE;
    #if defined(HAVE_AESGCM) || defined(HAVE_AESCCM)
        aesAuthAddSz    = AES_AUTH_ADD_SZ;
        aes_aad_options = AES_AAD_OPTIONS_DEFAULT;
        aes_aad_size    = 0;
    #endif
        base2 = 1;
        digest_stream = 1;

        bench_all = 1;
        bench_cipher_algs = 0;
        bench_digest_algs = 0;
        bench_mac_algs = 0;
        bench_asym_algs = 0;
        bench_pq_asym_algs = 0;
        bench_other_algs = 0;
        csv_format = 0;
    }
}



/******************************************************************************/
/* Begin Stats Functions */
/******************************************************************************/
typedef enum bench_stat_type {
    BENCH_STAT_ASYM,
    BENCH_STAT_SYM,
    BENCH_STAT_IGNORE,
} bench_stat_type_t;

#ifdef WC_ENABLE_BENCH_THREADING
    static int gPrintStats = 0;

    static pthread_mutex_t bench_lock = PTHREAD_MUTEX_INITIALIZER;

    #ifndef BENCH_MAX_NAME_SZ
    #define BENCH_MAX_NAME_SZ 24
    #endif
    typedef struct bench_stats {
        struct bench_stats* next;
        struct bench_stats* prev;
        char algo[BENCH_MAX_NAME_SZ+1]; /* may not be static, so make copy */
        const char* desc;
        double perfsec;
        int strength;
        int useDeviceID;
        int finishCount;
        bench_stat_type_t type;
        int lastRet;
        const char* perftype;
    } bench_stats_t;
    static bench_stats_t* bench_stats_head;
    static bench_stats_t* bench_stats_tail;

    static bench_stats_t* bench_stats_add(bench_stat_type_t type,
        const char* algo, int strength, const char* desc, int useDeviceID,
        double perfsec, const char* perftype, int ret)
    {
        bench_stats_t* bstat = NULL;

        /* protect bench_stats_head and bench_stats_tail access */
        PTHREAD_CHECK_RET(pthread_mutex_lock(&bench_lock));

        if (algo != NULL) {
            /* locate existing in list */
            for (bstat = bench_stats_head; bstat != NULL; bstat = bstat->next) {
                /* match based on algo, strength and desc */
                if (XSTRNCMP(bstat->algo, algo, BENCH_MAX_NAME_SZ) == 0 &&
                    bstat->strength == strength &&
                    bstat->desc == desc &&
                    bstat->useDeviceID == useDeviceID) {
                    break;
                }
            }
        }

        if (bstat == NULL) {
            /* allocate new and put on list */
            bstat = (bench_stats_t*)XMALLOC(sizeof(bench_stats_t), NULL,
                DYNAMIC_TYPE_INFO);
            if (bstat) {
                XMEMSET(bstat, 0, sizeof(bench_stats_t));

                /* add to list */
                bstat->next = NULL;
                if (bench_stats_tail == NULL)  {
                    bench_stats_head = bstat;
                }
                else {
                    bench_stats_tail->next = bstat;
                    bstat->prev = bench_stats_tail;
                }
                bench_stats_tail = bstat; /* add to the end either way */
            }
        }
        if (bstat) {
            bstat->type = type;
            if (algo != NULL)
                XSTRNCPY(bstat->algo, algo, BENCH_MAX_NAME_SZ);
            bstat->strength = strength;
            bstat->desc = desc;
            bstat->useDeviceID = useDeviceID;
            bstat->perfsec += perfsec;
            bstat->finishCount++;
            bstat->perftype = perftype;
            if (bstat->lastRet > ret)
                bstat->lastRet = ret; /* track last error */
        }
        PTHREAD_CHECK_RET(pthread_mutex_unlock(&bench_lock));

        return bstat;
    }

    void bench_stats_print(void)
    {
        bench_stats_t* bstat;

        /* protect bench_stats_head and bench_stats_tail access */
        PTHREAD_CHECK_RET(pthread_mutex_lock(&bench_lock));

        for (bstat = bench_stats_head; bstat != NULL; ) {
            if (bstat->type == BENCH_STAT_SYM) {
                printf("%-16s%s %8.3f %s/s\n", bstat->desc,
                    BENCH_DEVID_GET_NAME(bstat->useDeviceID), bstat->perfsec,
                    base2 ? "MB" : "mB");
            }
            else {
                printf("%-5s %4d %-9s %s %.3f ops/sec\n",
                    bstat->algo, bstat->strength, bstat->desc,
                    BENCH_DEVID_GET_NAME(bstat->useDeviceID), bstat->perfsec);
            }

            bstat = bstat->next;
        }

        PTHREAD_CHECK_RET(pthread_mutex_unlock(&bench_lock));
    }

#else /* !WC_ENABLE_BENCH_THREADING */

    typedef struct bench_stats {
        const char* algo;
        const char* desc;
        double perfsec;
        const char* perftype;
        int strength;
        bench_stat_type_t type;
        int ret;
    } bench_stats_t;
    /* 16 threads and 8 different operations. */
    #define MAX_BENCH_STATS (16 * 8)
    static bench_stats_t gStats[MAX_BENCH_STATS];
    static int gStatsCount;

    static bench_stats_t* bench_stats_add(bench_stat_type_t type,
            const char* algo, int strength, const char* desc, int useDeviceID,
            double perfsec, const char* perftype, int ret)
    {
        bench_stats_t* bstat = NULL;
        if (gStatsCount >= MAX_BENCH_STATS)
            return bstat;

        bstat = &gStats[gStatsCount++];
        bstat->algo = algo;
        bstat->desc = desc;
        bstat->perfsec = perfsec;
        bstat->perftype = perftype;
        bstat->strength = strength;
        bstat->type = type;
        bstat->ret = ret;

        (void)useDeviceID;

        return bstat;
    }

    void bench_stats_print(void)
    {
        int i;
        bench_stats_t* bstat;

        for (i=0; i<gStatsCount; i++) {
            bstat = &gStats[i];
            if (bstat->type == BENCH_STAT_SYM) {
                printf("%-16s %8.3f %s/s\n", bstat->desc, bstat->perfsec,
                    base2 ? "MB" : "mB");
            }
            else if (bstat->type == BENCH_STAT_ASYM) {
                printf("%-5s %4d %-9s %.3f ops/sec\n",
                    bstat->algo, bstat->strength, bstat->desc, bstat->perfsec);
            }
        }
    }
#endif /* WC_ENABLE_BENCH_THREADING */

static WC_INLINE void bench_stats_init(void)
{
#if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_NO_ASYNC_THREADING)
    bench_stats_head = NULL;
    bench_stats_tail = NULL;
#endif
    INIT_CYCLE_COUNTER
}

static WC_INLINE void bench_stats_start(int* count, double* start)
{
    *count = 0;
    *start = current_time(1);
    BEGIN_INTEL_CYCLES
}

static WC_INLINE int bench_stats_check(double start)
{
    return ((current_time(0) - start) < BENCH_MIN_RUNTIME_SEC);
}


/* countSz is number of bytes that 1 count represents. Normally bench_size,
 * except for AES direct that operates on AES_BLOCK_SIZE blocks */
static void bench_stats_sym_finish(const char* desc, int useDeviceID, int count,
                                   int countSz, double start, int ret)
{
    double total, persec = 0, blocks = (double)count;
    const char* blockType;
    char msg[128] = {0};
    const char** word = bench_result_words1[lng_index];
    static int sym_header_printed = 0;

    END_INTEL_CYCLES
    total = current_time(0) - start;

#ifdef LINUX_RUSAGE_UTIME
    check_for_excessive_stime(desc, "");
#endif

    /* calculate actual bytes */
    blocks *= countSz;
    if (csv_format == 1) {
        /* only print out header once */
        if (sym_header_printed == 0) {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
            printf("%s", "\"sym\",Algorithm,HW/SW,bytes_total,seconds_total,"
                   "MB/s,cycles_total,Cycles per byte,\n");
#else
            printf("\n\nSymmetric Ciphers:\n\n");
            printf("Algorithm,MB/s,Cycles per byte,\n");
#endif
            sym_header_printed = 1;
        }
    }

    if (base2) {
        /* determine if we should show as KiB or MiB */
        if (blocks > (1024UL * 1024UL)) {
            blocks /= (1024UL * 1024UL);
            blockType = "MiB";
        }
        else if (blocks > 1024) {
            blocks /= 1024;
            blockType = "KiB";
        }
        else {
            blockType = "bytes";
        }
    }
    else {
        /* determine if we should show as KB or MB */
        if (blocks > (1000UL * 1000UL)) {
            blocks /= (1000UL * 1000UL);
            blockType = "MB";
        }
        else if (blocks > 1000) {
            blocks /= 1000; /* make KB */
            blockType = "KB";
        }
        else {
            blockType = "bytes";
        }
    }

    /* calculate blocks per second */
    if (total > 0) {
        persec = (1 / total) * blocks;
    }

    SLEEP_ON_ERROR(ret);
    /* format and print to terminal */
    if (csv_format == 1) {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        word64 bytes_processed;
        if (blockType[0] == 'K')
            bytes_processed = (word64)(blocks * (base2 ? 1024. : 1000.));
        else if (blockType[0] == 'M')
            bytes_processed =
                (word64)(blocks * (base2 ? (1024. * 1024.) : (1000. * 1000.)));
        else
            bytes_processed = (word64)blocks;
#endif
        if (blockType[0] == 'K')
            persec /= base2 ? 1024. : 1000.;
        else if (blockType[0] == 'b')
            persec /= base2 ? (1024. * 1024.) : (1000. * 1000.);
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        /* note this codepath brings in all the fields from the non-CSV case. */
        (void)XSNPRINTF(msg, sizeof(msg), "sym,%s,%s,%lu,%f,%f,%lu,", desc,
                        BENCH_DEVID_GET_NAME(useDeviceID),
                        bytes_processed, total, persec, total_cycles);
#else
        (void)XSNPRINTF(msg, sizeof(msg), "%s,%f,", desc, persec);
#endif
        SHOW_INTEL_CYCLES_CSV(msg, sizeof(msg), countSz);
    } else {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        (void)XSNPRINTF(msg, sizeof(msg),
                 "%-24s%s %5.0f %s %s %5.3f %s, %8.3f %s/s"
                 ", %lu cycles,",
                 desc, BENCH_DEVID_GET_NAME(useDeviceID), blocks, blockType,
                 word[0], total, word[1], persec, blockType, total_cycles);
#else
        (void)XSNPRINTF(msg, sizeof(msg),
                 "%-24s%s %5.0f %s %s %5.3f %s, %8.3f %s/s",
                 desc, BENCH_DEVID_GET_NAME(useDeviceID), blocks, blockType,
                 word[0], total, word[1], persec, blockType);
#endif
        SHOW_INTEL_CYCLES(msg, sizeof(msg), countSz);
    }
    printf("%s", msg);

    /* show errors */
    if (ret < 0) {
        printf("%sBenchmark %s failed: %d\n", err_prefix, desc, ret);
    }

#ifndef WOLFSSL_SGX
    fflush(stdout);
#endif

    /* Add to thread stats */
    bench_stats_add(BENCH_STAT_SYM, desc, 0, desc, useDeviceID, persec,
        blockType, ret);

    (void)useDeviceID;
    (void)ret;

    TEST_SLEEP();
}

#ifdef BENCH_ASYM
#if defined(HAVE_ECC) || !defined(NO_RSA) || !defined(NO_DH) || \
    defined(HAVE_CURVE25519) || defined(HAVE_ED25519) || \
    defined(HAVE_CURVE448) || defined(HAVE_ED448) || \
    defined(WOLFSSL_HAVE_KYBER)
static void bench_stats_asym_finish_ex(const char* algo, int strength,
    const char* desc, const char* desc_extra, int useDeviceID, int count,
    double start, int ret)
{
    double total, each = 0, opsSec, milliEach;
    const char **word = bench_result_words2[lng_index];
    const char* kOpsSec = "Ops/Sec";
    char msg[256] = {0};
    static int asym_header_printed = 0;

    total = current_time(0) - start;

#ifdef LINUX_RUSAGE_UTIME
    check_for_excessive_stime(desc, desc_extra);
#endif

#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
    END_INTEL_CYCLES
#endif

    if (count > 0)
        each  = total / count; /* per second  */
    opsSec = count / total;    /* ops second */
    milliEach = each * 1000;   /* milliseconds */

    SLEEP_ON_ERROR(ret);
    /* format and print to terminal */
    if (csv_format == 1) {
        /* only print out header once */
        if (asym_header_printed == 0) {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
            printf("%s", "\"asym\",Algorithm,key size,operation,avg ms,ops/sec,"
                   "ops,secs,cycles,cycles/op\n");
#else
            printf("\n%sAsymmetric Ciphers:\n\n", info_prefix);
            printf("%sAlgorithm,key size,operation,avg ms,ops/sec,\n",
                   info_prefix);
#endif
            asym_header_printed = 1;
        }
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        (void)XSNPRINTF(msg, sizeof(msg),
                        "asym,%s,%d,%s%s,%.3f,%.3f,%d,%f,%lu,%.6f\n",
                        algo, strength, desc, desc_extra, milliEach, opsSec,
                        count, total, total_cycles,
                        (double)total_cycles / (double)count);
#else
        (void)XSNPRINTF(msg, sizeof(msg), "%s,%d,%s%s,%.3f,%.3f,\n", algo,
                        strength, desc, desc_extra, milliEach, opsSec);
#endif
    } else {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        (void)XSNPRINTF(msg, sizeof(msg),
                        "%-6s %5d %8s%-2s %s %6d %s %5.3f %s, %s %5.3f ms,"
                        " %.3f %s, %lu cycles\n", algo, strength, desc,
                        desc_extra, BENCH_DEVID_GET_NAME(useDeviceID),
                        count, word[0], total, word[1], word[2], milliEach,
                        opsSec, word[3], total_cycles);
#else
        (void)XSNPRINTF(msg, sizeof(msg),
                        "%-6s %5d %8s%-2s %s %6d %s %5.3f %s, %s %5.3f ms,"
                        " %.3f %s\n", algo, strength, desc, desc_extra,
                        BENCH_DEVID_GET_NAME(useDeviceID), count, word[0],
                        total, word[1], word[2], milliEach, opsSec, word[3]);
#endif
    }
    printf("%s", msg);

    /* show errors */
    if (ret < 0) {
        printf("%sBenchmark %s %s %d failed: %d\n",
               err_prefix, algo, desc, strength, ret);
    }

#ifndef WOLFSSL_SGX
    fflush(stdout);
#endif

    /* Add to thread stats */
    bench_stats_add(BENCH_STAT_ASYM, algo, strength, desc, useDeviceID, opsSec,
                    kOpsSec, ret);

    (void)useDeviceID;
    (void)ret;

    TEST_SLEEP();
}

static void bench_stats_asym_finish(const char* algo, int strength,
    const char* desc, int useDeviceID, int count, double start, int ret)
{
    bench_stats_asym_finish_ex(algo, strength, desc, "", useDeviceID, count,
                               start, ret);
}
#endif

#if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
static void bench_stats_pq_asym_finish(const char* algo, int useDeviceID, int count,
                                       double start, int ret)
{
    double total, each = 0, opsSec, milliEach;
    const char **word = bench_result_words2[lng_index];
    const char* kOpsSec = "Ops/Sec";
    char msg[128] = {0};
    static int pqasym_header_printed = 0;

    total = current_time(0) - start;

#ifdef LINUX_RUSAGE_UTIME
    check_for_excessive_stime(algo, "");
#endif

#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
    END_INTEL_CYCLES
#endif

    if (count > 0)
        each  = total / count; /* per second  */
    opsSec = count / total;    /* ops second */
    milliEach = each * 1000;   /* milliseconds */

    /* format and print to terminal */
    if (csv_format == 1) {
        /* only print out header once */
        if (pqasym_header_printed == 0) {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
            printf("%s", "\"pq_asym\",Algorithm,avg ms,ops/sec,ops,secs,cycles,"
                   "cycles/op\n");
#else
            printf("\nPost Quantum Asymmetric Ciphers:\n\n");
            printf("Algorithm,avg ms,ops/sec,\n");
#endif
            pqasym_header_printed = 1;
        }
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
        (void)XSNPRINTF(msg, sizeof(msg),
                        "pq_asym,%s,%.3f,%.3f,%d,%f,%lu,%.6f,\n",
                        algo, milliEach, opsSec, count, total, total_cycles,
                        (double)total_cycles / (double)count);
#else
        (void)XSNPRINTF(msg, sizeof(msg), "%s %.3f,%.3f,\n", algo, milliEach,
                        opsSec);
#endif
    } else {
#ifdef GENERATE_MACHINE_PARSEABLE_REPORT
         (void)XSNPRINTF(msg, sizeof(msg),
                         "%-18s %s %6d %s %5.3f %s, %s %5.3f ms,"
                         " %.3f %s, %lu cycles\n",
                         algo, BENCH_DEVID_GET_NAME(useDeviceID),
                         count, word[0], total, word[1], word[2], milliEach,
                         opsSec, word[3], total_cycles);
#else
         (void)XSNPRINTF(msg, sizeof(msg), "%-18s %s %6d %s %5.3f %s, %s %5.3f ms,"
         " %.3f %s\n", algo, BENCH_DEVID_GET_NAME(useDeviceID),
         count, word[0], total, word[1], word[2], milliEach, opsSec, word[3]);
#endif
    }
    printf("%s", msg);

    /* show errors */
    if (ret < 0) {
        printf("%sBenchmark %s failed: %d\n", err_prefix, algo, ret);
    }

    /* Add to thread stats */
    bench_stats_add(BENCH_STAT_ASYM, algo, 0, "", useDeviceID, opsSec, kOpsSec, ret);

    (void)useDeviceID;
    (void)ret;

    TEST_SLEEP();
}
#endif
#endif /* BENCH_ASYM */

static WC_INLINE void bench_stats_free(void)
{
#if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_NO_ASYNC_THREADING)
    bench_stats_t* bstat;
    for (bstat = bench_stats_head; bstat != NULL; ) {
        bench_stats_t* next = bstat->next;
        XFREE(bstat, NULL, DYNAMIC_TYPE_INFO);
        bstat = next;
    }
    bench_stats_head = NULL;
    bench_stats_tail = NULL;
#endif
}
/******************************************************************************/
/* End Stats Functions */
/******************************************************************************/


static void* benchmarks_do(void* args)
{
    int bench_buf_size;

#ifdef WOLFSSL_ASYNC_CRYPT
#ifndef WC_NO_ASYNC_THREADING
    ThreadData* threadData = (ThreadData*)args;

    if (wolfAsync_DevOpenThread(&devId, &threadData->thread_id) < 0)
#else
    if (wolfAsync_DevOpen(&devId) < 0)
#endif
    {
        printf("%sAsync device open failed\n%sRunning without async\n",
               err_prefix, err_prefix);
    }
#endif /* WOLFSSL_ASYNC_CRYPT */

    (void)args;

#ifdef WOLFSSL_ASYNC_CRYPT
    if (wolfEventQueue_Init(&eventQueue) != 0) {
        printf("%sAsync event queue init failure!\n", err_prefix);
    }
#endif

#ifdef WOLF_CRYPTO_CB
#ifdef HAVE_INTEL_QA_SYNC
    devId = wc_CryptoCb_InitIntelQa();
    if (devId == INVALID_DEVID) {
        printf("%sCouldn't init the Intel QA\n", err_prefix);
    }
#endif
#ifdef HAVE_CAVIUM_OCTEON_SYNC
    devId = wc_CryptoCb_InitOcteon();
    if (devId == INVALID_DEVID) {
        printf("%sCouldn't get the Octeon device ID\n", err_prefix);
    }
#endif
#ifdef HAVE_RENESAS_SYNC
    devId = wc_CryptoCb_CryptInitRenesasCmn(NULL, &guser_PKCbInfo);
    if (devId == INVALID_DEVID) {
        printf("%sCouldn't get the Renesas device ID\n", err_prefix);
    }
#endif
#endif

#if defined(HAVE_LOCAL_RNG)
    {
        int rngRet;

#ifndef HAVE_FIPS
        rngRet = wc_InitRng_ex(&gRng, HEAP_HINT, devId);
#else
        rngRet = wc_InitRng(&gRng);
#endif
        if (rngRet < 0) {
            printf("%sInitRNG failed\n", err_prefix);
            return NULL;
        }
    }
#endif

    /* setup bench plain, cipher, key and iv globals */
    /* make sure bench buffer is multiple of 16 (AES block size) */
    bench_buf_size = (int)bench_size + BENCH_CIPHER_ADD;
    if (bench_buf_size % 16)
        bench_buf_size += 16 - (bench_buf_size % 16);

#ifdef WOLFSSL_AFALG_XILINX_AES
    bench_plain = (byte*)aligned_alloc(64, (size_t)bench_buf_size + 16);
    bench_cipher = (byte*)aligned_alloc(64, (size_t)bench_buf_size + 16);
#else
    bench_plain = (byte*)XMALLOC((size_t)bench_buf_size + 16, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
    bench_cipher = (byte*)XMALLOC((size_t)bench_buf_size + 16, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
#endif
    if (bench_plain == NULL || bench_cipher == NULL) {
        XFREE(bench_plain, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
        XFREE(bench_cipher, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
        bench_plain = bench_cipher = NULL;

        printf("%sBenchmark block buffer alloc failed!\n", err_prefix);
        goto exit;
    }
    XMEMSET(bench_plain, 0, (size_t)bench_buf_size);
    XMEMSET(bench_cipher, 0, (size_t)bench_buf_size);

#if defined(WOLFSSL_ASYNC_CRYPT) || defined(HAVE_INTEL_QA_SYNC)
    bench_key = (byte*)XMALLOC(sizeof(bench_key_buf), HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
    bench_iv = (byte*)XMALLOC(sizeof(bench_iv_buf), HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
    if (bench_key == NULL || bench_iv == NULL) {
        XFREE(bench_key, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
        XFREE(bench_iv, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
        bench_key = bench_iv = NULL;

        printf("%sBenchmark cipher buffer alloc failed!\n", err_prefix);
        goto exit;
    }
    XMEMCPY(bench_key, bench_key_buf, sizeof(bench_key_buf));
    XMEMCPY(bench_iv, bench_iv_buf, sizeof(bench_iv_buf));
#else
    bench_key = (byte*)bench_key_buf;
    bench_iv = (byte*)bench_iv_buf;
#endif

#ifndef WC_NO_RNG
    if (bench_all || (bench_other_algs & BENCH_RNG))
        bench_rng();
#endif /* WC_NO_RNG */
#ifndef NO_AES
#ifdef HAVE_AES_CBC
    if (bench_all || (bench_cipher_algs & BENCH_AES_CBC)) {
    #ifndef NO_SW_BENCH
        bench_aescbc(0);
    #endif
    #if defined(BENCH_DEVID) || defined(HAVE_RENESAS_SYNC)
        bench_aescbc(1);
    #endif
    }
#endif
#ifdef HAVE_AESGCM
    if (bench_all || (bench_cipher_algs & BENCH_AES_GCM)) {
    #ifndef NO_SW_BENCH
        bench_aes_aad_options_wrap(bench_aesgcm, 0);
    #endif
    #if ((defined(WOLFSSL_ASYNC_CRYPT) && defined(WC_ASYNC_ENABLE_3DES)) || \
         defined(HAVE_INTEL_QA_SYNC) || defined(HAVE_CAVIUM_OCTEON_SYNC) || \
         defined(HAVE_RENESAS_SYNC)  || defined(WOLFSSL_CAAM)) && \
        !defined(NO_HW_BENCH)
        bench_aes_aad_options_wrap(bench_aesgcm, 1);
    #endif

        bench_gmac();
    }
#endif
#ifdef HAVE_AES_ECB
    if (bench_all || (bench_cipher_algs & BENCH_AES_ECB)) {
    #ifndef NO_SW_BENCH
        bench_aesecb(0);
    #endif
    #ifdef BENCH_DEVID
        bench_aesecb(1);
    #endif
    }
#endif
#ifdef WOLFSSL_AES_XTS
    if (bench_all || (bench_cipher_algs & BENCH_AES_XTS))
        bench_aesxts();
#endif
#ifdef WOLFSSL_AES_CFB
    if (bench_all || (bench_cipher_algs & BENCH_AES_CFB))
        bench_aescfb();
#endif
#ifdef WOLFSSL_AES_OFB
    if (bench_all || (bench_cipher_algs & BENCH_AES_OFB))
        bench_aesofb();
#endif
#ifdef WOLFSSL_AES_COUNTER
    if (bench_all || (bench_cipher_algs & BENCH_AES_CTR))
        bench_aesctr();
#endif
#ifdef HAVE_AESCCM
    if (bench_all || (bench_cipher_algs & BENCH_AES_CCM)) {
        bench_aes_aad_options_wrap(bench_aesccm, 0);
    #ifdef BENCH_DEVID
        bench_aes_aad_options_wrap(bench_aesccm, 1);
    #endif
    }
#endif
#ifdef WOLFSSL_AES_SIV
    if (bench_all || (bench_cipher_algs & BENCH_AES_SIV))
        bench_aessiv();
#endif
#endif /* !NO_AES */

#ifdef HAVE_CAMELLIA
    if (bench_all || (bench_cipher_algs & BENCH_CAMELLIA))
        bench_camellia();
#endif
#ifndef NO_RC4
    if (bench_all || (bench_cipher_algs & BENCH_ARC4)) {
    #ifndef NO_SW_BENCH
        bench_arc4(0);
    #endif
    #ifdef BENCH_DEVID
        bench_arc4(1);
    #endif
    }
#endif
#ifdef HAVE_CHACHA
    if (bench_all || (bench_cipher_algs & BENCH_CHACHA20))
        bench_chacha();
#endif
#if defined(HAVE_CHACHA) && defined(HAVE_POLY1305)
    if (bench_all || (bench_cipher_algs & BENCH_CHACHA20_POLY1305))
        bench_chacha20_poly1305_aead();
#endif
#ifndef NO_DES3
    if (bench_all || (bench_cipher_algs & BENCH_DES)) {
    #ifndef NO_SW_BENCH
        bench_des(0);
    #endif
    #ifdef BENCH_DEVID
        bench_des(1);
    #endif
    }
#endif
#ifndef NO_MD5
    if (bench_all || (bench_digest_algs & BENCH_MD5)) {
    #ifndef NO_SW_BENCH
        bench_md5(0);
    #endif
    #ifdef BENCH_DEVID
        bench_md5(1);
    #endif
    }
#endif
#ifdef HAVE_POLY1305
    if (bench_all || (bench_digest_algs & BENCH_POLY1305))
        bench_poly1305();
#endif
#ifndef NO_SHA
    if (bench_all || (bench_digest_algs & BENCH_SHA)) {
    #ifndef NO_SW_BENCH
        bench_sha(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha(1);
    #endif
    }
#endif
#ifdef WOLFSSL_SHA224
    if (bench_all || (bench_digest_algs & BENCH_SHA224)) {
    #ifndef NO_SW_BENCH
        bench_sha224(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha224(1);
    #endif
    }
#endif
#ifndef NO_SHA256
    if (bench_all || (bench_digest_algs & BENCH_SHA256)) {
    #ifndef NO_SW_BENCH
        bench_sha256(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha256(1);
    #endif
    }
#endif
#ifdef WOLFSSL_SHA384
    if (bench_all || (bench_digest_algs & BENCH_SHA384)) {
    #ifndef NO_SW_BENCH
        bench_sha384(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha384(1);
    #endif
    }
#endif
#ifdef WOLFSSL_SHA512
    if (bench_all || (bench_digest_algs & BENCH_SHA512)) {
    #ifndef NO_SW_BENCH
        bench_sha512(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha512(1);
    #endif
    }
#endif
#ifdef WOLFSSL_SHA3
    #ifndef WOLFSSL_NOSHA3_224
    if (bench_all || (bench_digest_algs & BENCH_SHA3_224)) {
    #ifndef NO_SW_BENCH
        bench_sha3_224(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha3_224(1);
    #endif
    }
    #endif /* WOLFSSL_NOSHA3_224 */
    #ifndef WOLFSSL_NOSHA3_256
    if (bench_all || (bench_digest_algs & BENCH_SHA3_256)) {
    #ifndef NO_SW_BENCH
        bench_sha3_256(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha3_256(1);
    #endif
    }
    #endif /* WOLFSSL_NOSHA3_256 */
    #ifndef WOLFSSL_NOSHA3_384
    if (bench_all || (bench_digest_algs & BENCH_SHA3_384)) {
    #ifndef NO_SW_BENCH
        bench_sha3_384(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha3_384(1);
    #endif
    }
    #endif /* WOLFSSL_NOSHA3_384 */
    #ifndef WOLFSSL_NOSHA3_512
    if (bench_all || (bench_digest_algs & BENCH_SHA3_512)) {
    #ifndef NO_SW_BENCH
        bench_sha3_512(0);
    #endif
    #ifdef BENCH_DEVID
        bench_sha3_512(1);
    #endif
    }
    #endif /* WOLFSSL_NOSHA3_512 */
    #ifdef WOLFSSL_SHAKE128
    if (bench_all || (bench_digest_algs & BENCH_SHAKE128)) {
    #ifndef NO_SW_BENCH
        bench_shake128(0);
    #endif
    #ifdef BENCH_DEVID
        bench_shake128(1);
    #endif
    }
    #endif /* WOLFSSL_SHAKE128 */
    #ifdef WOLFSSL_SHAKE256
    if (bench_all || (bench_digest_algs & BENCH_SHAKE256)) {
    #ifndef NO_SW_BENCH
        bench_shake256(0);
    #endif
    #ifdef BENCH_DEVID
        bench_shake256(1);
    #endif
    }
    #endif /* WOLFSSL_SHAKE256 */
#endif
#ifdef WOLFSSL_RIPEMD
    if (bench_all || (bench_digest_algs & BENCH_RIPEMD))
        bench_ripemd();
#endif
#ifdef HAVE_BLAKE2
    if (bench_all || (bench_digest_algs & BENCH_BLAKE2B))
        bench_blake2b();
#endif
#ifdef HAVE_BLAKE2S
    if (bench_all || (bench_digest_algs & BENCH_BLAKE2S))
        bench_blake2s();
#endif
#ifdef WOLFSSL_CMAC
    if (bench_all || (bench_mac_algs & BENCH_CMAC)) {
        bench_cmac(0);
    #ifdef BENCH_DEVID
        bench_cmac(1);
    #endif
    }
#endif

#ifndef NO_HMAC
    #ifndef NO_MD5
        if (bench_all || (bench_mac_algs & BENCH_HMAC_MD5)) {
        #ifndef NO_SW_BENCH
            bench_hmac_md5(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_md5(1);
        #endif
        }
    #endif
    #ifndef NO_SHA
        if (bench_all || (bench_mac_algs & BENCH_HMAC_SHA)) {
        #ifndef NO_SW_BENCH
            bench_hmac_sha(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_sha(1);
        #endif
        }
    #endif
    #ifdef WOLFSSL_SHA224
        if (bench_all || (bench_mac_algs & BENCH_HMAC_SHA224)) {
        #ifndef NO_SW_BENCH
            bench_hmac_sha224(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_sha224(1);
        #endif
        }
    #endif
    #ifndef NO_SHA256
        if (bench_all || (bench_mac_algs & BENCH_HMAC_SHA256)) {
        #ifndef NO_SW_BENCH
            bench_hmac_sha256(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_sha256(1);
        #endif
        }
    #endif
    #ifdef WOLFSSL_SHA384
        if (bench_all || (bench_mac_algs & BENCH_HMAC_SHA384)) {
        #ifndef NO_SW_BENCH
            bench_hmac_sha384(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_sha384(1);
        #endif
        }
    #endif
    #ifdef WOLFSSL_SHA512
        if (bench_all || (bench_mac_algs & BENCH_HMAC_SHA512)) {
        #ifndef NO_SW_BENCH
            bench_hmac_sha512(0);
        #endif
        #ifdef BENCH_DEVID
            bench_hmac_sha512(1);
        #endif
        }
    #endif
    #ifndef NO_PWDBASED
        if (bench_all || (bench_mac_algs & BENCH_PBKDF2)) {
            bench_pbkdf2();
        }
    #endif
    #ifdef WOLFSSL_SIPHASH
        if (bench_all || (bench_mac_algs & BENCH_SIPHASH)) {
            bench_siphash();
        }
    #endif
#endif /* NO_HMAC */

#ifdef HAVE_SCRYPT
    if (bench_all || (bench_other_algs & BENCH_SCRYPT))
        bench_scrypt();
#endif

#ifndef NO_RSA
    #ifdef WOLFSSL_KEY_GEN
        if (bench_all || (bench_asym_algs & BENCH_RSA_KEYGEN)) {
        #ifndef NO_SW_BENCH
            if (((word32)bench_asym_algs == 0xFFFFFFFFU) ||
                        (bench_asym_algs & BENCH_RSA_SZ) == 0) {
                bench_rsaKeyGen(0);
            }
            else {
                bench_rsaKeyGen_size(0, bench_size);
            }
        #endif
        #ifdef BENCH_DEVID
            if (bench_asym_algs & BENCH_RSA_SZ) {
                bench_rsaKeyGen_size(1, bench_size);
            }
            else {
                bench_rsaKeyGen(1);
            }
        #endif
        }
    #endif
    if (bench_all || (bench_asym_algs & BENCH_RSA)) {
    #ifndef NO_SW_BENCH
        bench_rsa(0);
    #endif
    #ifdef BENCH_DEVID
        bench_rsa(1);
    #endif
    }

    #ifdef WOLFSSL_KEY_GEN
    if (bench_asym_algs & BENCH_RSA_SZ) {
    #ifndef NO_SW_BENCH
        bench_rsa_key(0, bench_size);
    #endif
    #ifdef BENCH_DEVID
        bench_rsa_key(1, bench_size);
    #endif
    }
    #endif
#endif

#ifndef NO_DH
    if (bench_all || (bench_asym_algs & BENCH_DH)) {
    #ifndef NO_SW_BENCH
        bench_dh(0);
    #endif
    #ifdef BENCH_DEVID
        bench_dh(1);
    #endif
    }
#endif

#ifdef WOLFSSL_HAVE_KYBER
    if (bench_all || (bench_asym_algs & BENCH_KYBER)) {
    #ifdef WOLFSSL_KYBER512
        bench_kyber(KYBER512);
    #endif
    #ifdef WOLFSSL_KYBER768
        bench_kyber(KYBER768);
    #endif
    #ifdef WOLFSSL_KYBER1024
        bench_kyber(KYBER1024);
    #endif
    }
#endif

#ifdef HAVE_ECC
    if (bench_all || (bench_asym_algs & BENCH_ECC_MAKEKEY) ||
            (bench_asym_algs & BENCH_ECC) ||
            (bench_asym_algs & BENCH_ECC_ALL) ||
            (bench_asym_algs & BENCH_ECC_ENCRYPT)) {

        if (bench_asym_algs & BENCH_ECC_ALL) {
            #if defined(HAVE_FIPS) || defined(HAVE_SELFTEST)
            printf("%snot supported in FIPS mode (no ending enum value)\n",
                   err_prefix);
            #else
            int curveId = (int)ECC_SECP192R1;

            /* set make key and encrypt */
            bench_asym_algs |= BENCH_ECC_MAKEKEY | BENCH_ECC |
                               BENCH_ECC_ENCRYPT;
            if (csv_format != 1) {
                printf("\n%sECC Benchmarks:\n", info_prefix);
            }

            do {
            #ifdef WOLFCRYPT_HAVE_SAKKE
                /* SAKKE is not useable with ECDH/ECDSA. Run separate test. */
                if (curveId == ECC_SAKKE_1) {
                    curveId++;
                    continue;
                }
            #endif

                if (wc_ecc_get_curve_size_from_id(curveId) !=
                        ECC_BAD_ARG_E) {
                    bench_ecc_curve(curveId);
                    if (csv_format != 1) {
                        printf("\n");
                    }
                }
                curveId++;
            } while (curveId != (int)ECC_CURVE_MAX);
            #endif
        }
        else if (bench_asym_algs & BENCH_ECC_P256) {
            bench_ecc_curve((int)ECC_SECP256R1);
        }
        else if (bench_asym_algs & BENCH_ECC_P384) {
            bench_ecc_curve((int)ECC_SECP384R1);
        }
        else if (bench_asym_algs & BENCH_ECC_P521) {
            bench_ecc_curve((int)ECC_SECP521R1);
        }
        else {
            #ifndef NO_ECC256
            bench_ecc_curve((int)ECC_SECP256R1);
            #elif defined(HAVE_ECC384)
            bench_ecc_curve((int)ECC_SECP384R1);
            #elif defined(HAVE_ECC521)
            bench_ecc_curve((int)ECC_SECP521R1);
            #endif
            #ifdef HAVE_ECC_BRAINPOOL
            bench_ecc_curve((int)ECC_BRAINPOOLP256R1);
            #endif
        }
    }
#endif

#ifdef HAVE_CURVE25519
    if (bench_all || (bench_asym_algs & BENCH_CURVE25519_KEYGEN)) {
        bench_curve25519KeyGen(0);
    #ifdef BENCH_DEVID
        bench_curve25519KeyGen(1);
    #endif
    }

    #ifdef HAVE_CURVE25519_SHARED_SECRET
    if (bench_all || (bench_asym_algs & BENCH_CURVE25519_KA)) {
        bench_curve25519KeyAgree(0);
    #ifdef BENCH_DEVID
        bench_curve25519KeyAgree(1);
    #endif
    }
    #endif
#endif

#ifdef HAVE_ED25519
    if (bench_all || (bench_asym_algs & BENCH_ED25519_KEYGEN))
        bench_ed25519KeyGen();
    if (bench_all || (bench_asym_algs & BENCH_ED25519_SIGN))
        bench_ed25519KeySign();
#endif

#ifdef HAVE_CURVE448
    if (bench_all || (bench_asym_algs & BENCH_CURVE448_KEYGEN))
        bench_curve448KeyGen();
    #ifdef HAVE_CURVE448_SHARED_SECRET
    if (bench_all || (bench_asym_algs & BENCH_CURVE448_KA))
        bench_curve448KeyAgree();
    #endif
#endif

#ifdef HAVE_ED448
    if (bench_all || (bench_asym_algs & BENCH_ED448_KEYGEN))
        bench_ed448KeyGen();
    if (bench_all || (bench_asym_algs & BENCH_ED448_SIGN))
        bench_ed448KeySign();
#endif

#ifdef WOLFCRYPT_HAVE_ECCSI
    #ifdef WOLFCRYPT_ECCSI_KMS
        if (bench_all || (bench_asym_algs & BENCH_ECCSI_KEYGEN)) {
            bench_eccsiKeyGen();
        }
        if (bench_all || (bench_asym_algs & BENCH_ECCSI_PAIRGEN)) {
            bench_eccsiPairGen();
        }
    #endif
    #ifdef WOLFCRYPT_ECCSI_CLIENT
        if (bench_all || (bench_asym_algs & BENCH_ECCSI_VALIDATE)) {
            bench_eccsiValidate();
        }
        if (bench_all || (bench_asym_algs & BENCH_ECCSI)) {
            bench_eccsi();
        }
    #endif
#endif

#ifdef WOLFCRYPT_HAVE_SAKKE
    #ifdef WOLFCRYPT_SAKKE_KMS
        if (bench_all || (bench_asym_algs & BENCH_SAKKE_KEYGEN)) {
            bench_sakkeKeyGen();
        }
        if (bench_all || (bench_asym_algs & BENCH_SAKKE_RSKGEN)) {
            bench_sakkeRskGen();
        }
    #endif
    #ifdef WOLFCRYPT_SAKKE_CLIENT
        if (bench_all || (bench_asym_algs & BENCH_SAKKE_VALIDATE)) {
            bench_sakkeValidate();
        }
        if (bench_all || (bench_asym_algs & BENCH_SAKKE)) {
            bench_sakke();
        }
    #endif
#endif

#if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL1_KEYGEN))
        bench_pqcKemKeygen(BENCH_KYBER_LEVEL1_KEYGEN);
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL1_ENCAP))
        bench_pqcKemEncapDecap(BENCH_KYBER_LEVEL1_ENCAP);
#endif
#if defined(HAVE_LIBOQS)
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL3_KEYGEN))
        bench_pqcKemKeygen(BENCH_KYBER_LEVEL3_KEYGEN);
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL3_ENCAP))
        bench_pqcKemEncapDecap(BENCH_KYBER_LEVEL3_ENCAP);
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL5_KEYGEN))
        bench_pqcKemKeygen(BENCH_KYBER_LEVEL5_KEYGEN);
    if (bench_all || (bench_pq_asym_algs & BENCH_KYBER_LEVEL5_ENCAP))
        bench_pqcKemEncapDecap(BENCH_KYBER_LEVEL5_ENCAP);
#ifdef HAVE_FALCON
    if (bench_all || (bench_pq_asym_algs & BENCH_FALCON_LEVEL1_SIGN))
        bench_falconKeySign(1);
    if (bench_all || (bench_pq_asym_algs & BENCH_FALCON_LEVEL5_SIGN))
        bench_falconKeySign(5);
#endif
#ifdef HAVE_DILITHIUM
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_LEVEL2_SIGN))
        bench_dilithiumKeySign(2, SHAKE_VARIANT);
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_LEVEL3_SIGN))
        bench_dilithiumKeySign(3, SHAKE_VARIANT);
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_LEVEL5_SIGN))
        bench_dilithiumKeySign(5, SHAKE_VARIANT);
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_AES_LEVEL2_SIGN))
        bench_dilithiumKeySign(2, AES_VARIANT);
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_AES_LEVEL3_SIGN))
        bench_dilithiumKeySign(3, AES_VARIANT);
    if (bench_all || (bench_pq_asym_algs & BENCH_DILITHIUM_AES_LEVEL5_SIGN))
        bench_dilithiumKeySign(5, AES_VARIANT);
#endif

#ifdef HAVE_SPHINCS
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_FAST_LEVEL1_SIGN))
        bench_sphincsKeySign(1, FAST_VARIANT);
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_FAST_LEVEL3_SIGN))
        bench_sphincsKeySign(3, FAST_VARIANT);
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_FAST_LEVEL5_SIGN))
        bench_sphincsKeySign(5, FAST_VARIANT);
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_SMALL_LEVEL1_SIGN))
        bench_sphincsKeySign(1, SMALL_VARIANT);
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_SMALL_LEVEL3_SIGN))
        bench_sphincsKeySign(3, SMALL_VARIANT);
    if (bench_all || (bench_pq_asym_algs2 & BENCH_SPHINCS_SMALL_LEVEL5_SIGN))
        bench_sphincsKeySign(5, SMALL_VARIANT);
#endif
#endif /* HAVE_LIBOQS */

exit:
    /* free benchmark buffers */
    XFREE(bench_plain, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
    XFREE(bench_cipher, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
#ifdef WOLFSSL_ASYNC_CRYPT
    XFREE(bench_key, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
    XFREE(bench_iv, HEAP_HINT, DYNAMIC_TYPE_WOLF_BIGINT);
#endif

#if defined(HAVE_LOCAL_RNG)
    wc_FreeRng(&gRng);
#endif

/* cleanup the thread if fixed point cache is enabled and have thread local */
#if defined(HAVE_THREAD_LS) && defined(HAVE_ECC) && defined(FP_ECC)
    wc_ecc_fp_free();
#endif

    (void)bench_cipher_algs;
    (void)bench_digest_algs;
    (void)bench_mac_algs;
    (void)bench_asym_algs;
    (void)bench_other_algs;
    (void)bench_pq_asym_algs;
    (void)bench_pq_asym_algs2;

    return NULL;
}

int benchmark_init(void)
{
    int ret = 0;

    benchmark_static_init(0);

#ifdef WOLFSSL_STATIC_MEMORY
    ret = wc_LoadStaticMemory(&HEAP_HINT, gBenchMemory, sizeof(gBenchMemory),
                                                            WOLFMEM_GENERAL, 1);
    if (ret != 0) {
        printf("%sunable to load static memory %d\n", err_prefix, ret);
    }
#endif /* WOLFSSL_STATIC_MEMORY */

    if ((ret = wolfCrypt_Init()) != 0) {
        printf("%swolfCrypt_Init failed %d\n", err_prefix, ret);
        return EXIT_FAILURE;
    }
#ifdef WOLFSSL_SECO_CAAM
    if (wc_SECO_OpenHSM(SECO_KEY_STORE_ID,
            SECO_BENCHMARK_NONCE, SECO_MAX_UPDATES, CAAM_KEYSTORE_CREATE)
            != 0) {
        printf("%sunable to open HSM\n", err_prefix);
        wolfCrypt_Cleanup();
        return EXIT_FAILURE;
    }
#endif

#ifdef WC_RNG_SEED_CB
    wc_SetSeed_Cb(wc_GenerateSeed);
#endif

    bench_stats_init();

#if defined(DEBUG_WOLFSSL) && !defined(HAVE_VALGRIND)
    wolfSSL_Debugging_ON();
#endif

    printf("%swolfCrypt Benchmark (block bytes %d, min %.1f sec each)\n",
           info_prefix, (int)bench_size, BENCH_MIN_RUNTIME_SEC);
#ifndef GENERATE_MACHINE_PARSEABLE_REPORT
    if (csv_format == 1) {
        printf("This format allows you to easily copy the output to a csv file.");
    }
#endif

#ifdef HAVE_WNR
    ret = wc_InitNetRandom(wnrConfigFile, NULL, 5000);
    if (ret != 0) {
        printf("%sWhitewood netRandom config init failed %d\n",
               err_prefix, ret);
    }
#endif /* HAVE_WNR */

    return ret;
}

int benchmark_free(void)
{
    int ret;

#ifdef WC_ENABLE_BENCH_THREADING
    if (gPrintStats || devId != INVALID_DEVID) {
        bench_stats_print();
    }
#endif

    bench_stats_free();

#ifdef WOLF_CRYPTO_CB
#ifdef HAVE_INTEL_QA_SYNC
    wc_CryptoCb_CleanupIntelQa(&devId);
#endif
#ifdef HAVE_CAVIUM_OCTEON_SYNC
    wc_CryptoCb_CleanupOcteon(&devId);
#endif
#ifdef HAVE_RENESAS_SYNC
    wc_CryptoCb_CleanupRenesasCmn(&devId);
#endif
#endif

#ifdef WOLFSSL_ASYNC_CRYPT
    /* free event queue */
    wolfEventQueue_Free(&eventQueue);

    /* close device */
    wolfAsync_DevClose(&devId);
#endif

#ifdef HAVE_WNR
    ret = wc_FreeNetRandom();
    if (ret < 0) {
        printf("%sFailed to free netRandom context %d\n", err_prefix, ret);
    }
#endif

#ifdef WOLFSSL_SECO_CAAM
    if (wc_SECO_CloseHSM() != 0) {
        printf("%sError closing down the key store\n", err_prefix);
    }
#endif

    if ((ret = wolfCrypt_Cleanup()) != 0) {
        printf("%serror %d with wolfCrypt_Cleanup\n", err_prefix, ret);
    }

    return ret;
}


#if defined(WC_ENABLE_BENCH_THREADING) && !defined(WOLFSSL_ASYNC_CRYPT)
static THREAD_RETURN WOLFSSL_THREAD run_bench(void* args)
{
    benchmark_test(args);

    EXIT_TEST(0);
}

static int benchmark_test_threaded(void* args)
{
    int i;

    printf("%sThreads: %d\n", info_prefix, g_threadCount);

    g_threadData = (ThreadData*)XMALLOC(sizeof(ThreadData) * g_threadCount,
        HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (g_threadData == NULL) {
        printf("%sThread data alloc failed!\n", err_prefix);
        return EXIT_FAILURE;
    }

    for (i = 0; i < g_threadCount; i++) {
        PTHREAD_CHECK_RET(pthread_create(&g_threadData[i].thread_id, NULL, run_bench, args));
    }

    for (i = 0; i < g_threadCount; i++) {
        PTHREAD_CHECK_RET(pthread_join(g_threadData[i].thread_id, 0));
    }

    printf("\n");
    bench_stats_print();

    return 0;
}
#endif

/* so embedded projects can pull in tests on their own */
#ifdef HAVE_STACK_SIZE
THREAD_RETURN WOLFSSL_THREAD benchmark_test(void* args)
#else
int benchmark_test(void *args)
#endif
{
    int ret;

    (void)args;

#ifdef HAVE_FIPS
    wolfCrypt_SetCb_fips(myFipsCb);
#endif

    ret = benchmark_init();
    if (ret != 0)
        EXIT_TEST(ret);

#if defined(WOLFSSL_ASYNC_CRYPT) && !defined(WC_NO_ASYNC_THREADING)
{
    int i;

    if (g_threadCount == 0) {
    #ifdef WC_ASYNC_BENCH_THREAD_COUNT
        g_threadCount = WC_ASYNC_BENCH_THREAD_COUNT;
    #else
        g_threadCount = wc_AsyncGetNumberOfCpus();
        if (g_threadCount > 0) {
            g_threadCount /= 2; /* use physical core count */
        }
    #endif
    }
    if (g_threadCount <= 0) {
        g_threadCount = 1;
    }

    printf("%sCPUs: %d\n", info_prefix, g_threadCount);

    g_threadData = (ThreadData*)XMALLOC(sizeof(ThreadData) * g_threadCount,
        HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    if (g_threadData == NULL) {
        printf("%sThread data alloc failed!\n", err_prefix);
        EXIT_TEST(EXIT_FAILURE);
    }

    /* Create threads */
    for (i = 0; i < g_threadCount; i++) {
        ret = wc_AsyncThreadCreate(&g_threadData[i].thread_id,
            benchmarks_do, &g_threadData[i]);
        if (ret != 0) {
            printf("%sError creating benchmark thread %d\n", err_prefix, ret);
            EXIT_TEST(EXIT_FAILURE);
        }
    }

    /* Start threads */
    for (i = 0; i < g_threadCount; i++) {
        wc_AsyncThreadJoin(&g_threadData[i].thread_id);
    }

    XFREE(g_threadData, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
}
#else
    benchmarks_do(NULL);
#endif
    SLEEP_ON_ERROR(1);
    printf("%sBenchmark complete\n", info_prefix);

    ret = benchmark_free();

    EXIT_TEST(ret);
}


#ifndef WC_NO_RNG
void bench_rng(void)
{
    int    ret, i, count;
    double start;
    long   pos, len, remain;
    WC_RNG myrng;

#ifndef HAVE_FIPS
    ret = wc_InitRng_ex(&myrng, HEAP_HINT, devId);
#else
    ret = wc_InitRng(&myrng);
#endif
    if (ret < 0) {
        printf("InitRNG failed %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            /* Split request to handle large RNG request */
            pos = 0;
            remain = (int)bench_size;
            while (remain > 0) {
                len = remain;
                if (len > RNG_MAX_BLOCK_LEN)
                    len = RNG_MAX_BLOCK_LEN;
                ret = wc_RNG_GenerateBlock(&myrng, &bench_plain[pos], (word32)len);
                if (ret < 0)
                    goto exit_rng;

                remain -= len;
                pos += len;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_rng:
    bench_stats_sym_finish("RNG", 0, count, bench_size, start, ret);

    wc_FreeRng(&myrng);
}
#endif /* WC_NO_RNG */


#ifndef NO_AES

#ifdef HAVE_AES_CBC
static void bench_aescbc_internal(int useDeviceID, const byte* key, word32 keySz,
                                  const byte* iv, const char* encLabel,
                                  const char* decLabel)
{
    int    ret = 0, i, count = 0, times, pending = 0;
    Aes    enc[BENCH_MAX_PENDING];
    double start;

    /* clear for done cleanup */
    XMEMSET(enc, 0, sizeof(enc));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_AesInit(&enc[i], HEAP_HINT,
                                useDeviceID ? devId: INVALID_DEVID)) != 0) {
            printf("AesInit failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_AesSetKey(&enc[i], key, keySz, iv, AES_ENCRYPTION);
        if (ret != 0) {
            printf("AesSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                    ret = wc_AesCbcEncrypt(&enc[i], bench_plain, bench_cipher,
                        bench_size);

                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_enc;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_enc:
    bench_stats_sym_finish(encLabel, useDeviceID, count, bench_size, start, ret);

    if (ret < 0) {
        goto exit;
    }

#ifdef HAVE_AES_DECRYPT
    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        ret = wc_AesSetKey(&enc[i], key, keySz, iv, AES_DECRYPTION);
        if (ret != 0) {
            printf("AesSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                    ret = wc_AesCbcDecrypt(&enc[i], bench_cipher, bench_plain,
                        bench_size);

                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_dec;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_dec:
    bench_stats_sym_finish(decLabel, useDeviceID, count, bench_size, start, ret);

#endif /* HAVE_AES_DECRYPT */

    (void)decLabel;
exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_AesFree(&enc[i]);
    }
}

void bench_aescbc(int useDeviceID)
{
#ifdef WOLFSSL_AES_128
    bench_aescbc_internal(useDeviceID, bench_key, 16, bench_iv,
                 "AES-128-CBC-enc", "AES-128-CBC-dec");
#endif
#ifdef WOLFSSL_AES_192
    bench_aescbc_internal(useDeviceID, bench_key, 24, bench_iv,
                 "AES-192-CBC-enc", "AES-192-CBC-dec");
#endif
#ifdef WOLFSSL_AES_256
    bench_aescbc_internal(useDeviceID, bench_key, 32, bench_iv,
                 "AES-256-CBC-enc", "AES-256-CBC-dec");
#endif
}

#endif /* HAVE_AES_CBC */

#ifdef HAVE_AESGCM
static void bench_aesgcm_internal(int useDeviceID, const byte* key, word32 keySz,
                                  const byte* iv, word32 ivSz,
                                  const char* encLabel, const char* decLabel)
{
    int    ret = 0, i, count = 0, times, pending = 0;
    Aes    enc[BENCH_MAX_PENDING];
#ifdef HAVE_AES_DECRYPT
    Aes    dec[BENCH_MAX_PENDING];
#endif
    double start;

    WC_DECLARE_VAR(bench_additional, byte, AES_AUTH_ADD_SZ, HEAP_HINT);
    WC_DECLARE_VAR(bench_tag, byte, AES_AUTH_TAG_SZ, HEAP_HINT);
#ifdef WC_DECLARE_VAR_IS_HEAP_ALLOC
    if (bench_additional == NULL || bench_tag == NULL) {
        printf("bench_aesgcm_internal malloc failed\n");
        goto exit;
    }
#endif

    /* clear for done cleanup */
    XMEMSET(enc, 0, sizeof(enc));
#ifdef HAVE_AES_DECRYPT
    XMEMSET(dec, 0, sizeof(dec));
#endif
#ifdef WOLFSSL_ASYNC_CRYPT
    if (bench_additional)
#endif
        XMEMSET(bench_additional, 0, AES_AUTH_ADD_SZ);
#ifdef WOLFSSL_ASYNC_CRYPT
    if (bench_tag)
#endif
        XMEMSET(bench_tag, 0, AES_AUTH_TAG_SZ);

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_AesInit(&enc[i], HEAP_HINT,
                        useDeviceID ? devId: INVALID_DEVID)) != 0) {
            printf("AesInit failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_AesGcmSetKey(&enc[i], key, keySz);
        if (ret != 0) {
            printf("AesGcmSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    /* GCM uses same routine in backend for both encrypt and decrypt */
    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
#ifndef BENCHMARK_AESGCM_STREAM
                    ret = wc_AesGcmEncrypt(&enc[i], bench_cipher,
                        bench_plain, bench_size,
                        iv, ivSz, bench_tag, AES_AUTH_TAG_SZ,
                        bench_additional, aesAuthAddSz);
#else
                    ret = wc_AesGcmEncryptInit(&enc[i], NULL, 0, iv, ivSz);
                    if (ret == 0) {
                        ret = wc_AesGcmEncryptUpdate(&enc[i], bench_cipher,
                            bench_plain, bench_size, bench_additional,
                            aesAuthAddSz);
                    }
                    if (ret == 0) {
                        ret = wc_AesGcmEncryptFinal(&enc[i], bench_tag,
                            AES_AUTH_TAG_SZ);
                    }
#endif
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_gcm;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_gcm:
    bench_stats_sym_finish(encLabel, useDeviceID, count, bench_size, start, ret);

#ifdef HAVE_AES_DECRYPT
    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_AesInit(&dec[i], HEAP_HINT,
                        useDeviceID ? devId: INVALID_DEVID)) != 0) {
            printf("AesInit failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_AesGcmSetKey(&dec[i], key, keySz);
        if (ret != 0) {
            printf("AesGcmSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&dec[i]), 0,
                                                 &times, numBlocks, &pending)) {
#ifndef BENCHMARK_AESGCM_STREAM
                    ret = wc_AesGcmDecrypt(&dec[i], bench_plain,
                        bench_cipher, bench_size,
                        iv, ivSz, bench_tag, AES_AUTH_TAG_SZ,
                        bench_additional, aesAuthAddSz);
#else
                    ret = wc_AesGcmDecryptInit(&enc[i], NULL, 0, iv, ivSz);
                    if (ret == 0) {
                        ret = wc_AesGcmDecryptUpdate(&enc[i], bench_plain,
                            bench_cipher, bench_size, bench_additional,
                            aesAuthAddSz);
                    }
                    if (ret == 0) {
                        ret = wc_AesGcmDecryptFinal(&enc[i], bench_tag,
                            AES_AUTH_TAG_SZ);
                    }
#endif
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&dec[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_gcm_dec;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_gcm_dec:
    bench_stats_sym_finish(decLabel, useDeviceID, count, bench_size, start, ret);
#endif /* HAVE_AES_DECRYPT */

    (void)decLabel;

exit:

    if (ret < 0) {
        printf("bench_aesgcm failed: %d\n", ret);
    }
#ifdef HAVE_AES_DECRYPT
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_AesFree(&dec[i]);
    }
#endif
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_AesFree(&enc[i]);
    }

    WC_FREE_VAR(bench_additional, HEAP_HINT);
    WC_FREE_VAR(bench_tag, HEAP_HINT);
}

void bench_aesgcm(int useDeviceID)
{
#define AES_GCM_STRING(n, dir)  AES_AAD_STRING("AES-" #n "-GCM-" #dir)
#if defined(WOLFSSL_AES_128) && !defined(WOLFSSL_AFALG_XILINX_AES) \
        && !defined(WOLFSSL_XILINX_CRYPT) || defined(WOLFSSL_XILINX_CRYPT_VERSAL)
    bench_aesgcm_internal(useDeviceID, bench_key, 16, bench_iv, 12,
                          AES_GCM_STRING(128, enc), AES_GCM_STRING(128, dec));
#endif
#if defined(WOLFSSL_AES_192) && !defined(WOLFSSL_AFALG_XILINX_AES) \
        && !defined(WOLFSSL_XILINX_CRYPT)
    bench_aesgcm_internal(useDeviceID, bench_key, 24, bench_iv, 12,
                          AES_GCM_STRING(192, enc), AES_GCM_STRING(192, dec));
#endif
#ifdef WOLFSSL_AES_256
    bench_aesgcm_internal(useDeviceID, bench_key, 32, bench_iv, 12,
                          AES_GCM_STRING(256, enc), AES_GCM_STRING(256, dec));
#endif
#undef AES_GCM_STRING
}

/* GMAC */
void bench_gmac(void)
{
    int ret, count = 0;
    Gmac gmac;
    double start;
    byte tag[AES_AUTH_TAG_SZ];

    /* determine GCM GHASH method */
#ifdef GCM_SMALL
    const char* gmacStr = "GMAC Small";
#elif defined(GCM_TABLE)
    const char* gmacStr = "GMAC Table";
#elif defined(GCM_TABLE_4BIT)
    const char* gmacStr = "GMAC Table 4-bit";
#elif defined(GCM_WORD32)
    const char* gmacStr = "GMAC Word32";
#else
    const char* gmacStr = "GMAC Default";
#endif

    /* init keys */
    XMEMSET(bench_plain, 0, bench_size);
    XMEMSET(tag, 0, sizeof(tag));
    XMEMSET(&gmac, 0, sizeof(Gmac)); /* clear context */
    (void)wc_AesInit((Aes*)&gmac, HEAP_HINT, INVALID_DEVID);
    wc_GmacSetKey(&gmac, bench_key, 16);

    bench_stats_start(&count, &start);
    do {
        ret = wc_GmacUpdate(&gmac, bench_iv, 12, bench_plain, bench_size,
            tag, sizeof(tag));

        count++;
    } while (bench_stats_check(start));
    wc_AesFree((Aes*)&gmac);

    bench_stats_sym_finish(gmacStr, 0, count, bench_size, start, ret);
}

#endif /* HAVE_AESGCM */


#ifdef HAVE_AES_ECB
static void bench_aesecb_internal(int useDeviceID, const byte* key, word32 keySz,
                                  const char* encLabel, const char* decLabel)
{
    int    ret = 0, i, count = 0, times, pending = 0;
    Aes    enc[BENCH_MAX_PENDING];
    double start;
#ifdef HAVE_FIPS
    static const int benchSz = AES_BLOCK_SIZE;
#else
    static const int benchSz = BENCH_SIZE;
#endif

    /* clear for done cleanup */
    XMEMSET(enc, 0, sizeof(enc));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_AesInit(&enc[i], HEAP_HINT,
                                useDeviceID ? devId: INVALID_DEVID)) != 0) {
            printf("AesInit failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_AesSetKey(&enc[i], key, keySz, bench_iv, AES_ENCRYPTION);
        if (ret != 0) {
            printf("AesSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        int outer_loop_limit = ((bench_size / benchSz) * 10) + 1;
        for (times = 0;
             times < outer_loop_limit /* numBlocks */ || pending > 0;
            ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                #ifdef HAVE_FIPS
                    wc_AesEncryptDirect(&enc[i], bench_cipher, bench_plain);
                #else
                    wc_AesEcbEncrypt(&enc[i], bench_cipher, bench_plain,
                        benchSz);
                #endif
                    ret = 0;
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_enc;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_enc:
    bench_stats_sym_finish(encLabel, useDeviceID, count, benchSz,
                           start, ret);

#ifdef HAVE_AES_DECRYPT
    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        ret = wc_AesSetKey(&enc[i], key, keySz, bench_iv, AES_DECRYPTION);
        if (ret != 0) {
            printf("AesSetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        int outer_loop_limit = (10 * (bench_size / benchSz)) + 1;
        for (times = 0; times < outer_loop_limit || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                #ifdef HAVE_FIPS
                    wc_AesDecryptDirect(&enc[i], bench_plain, bench_cipher);
                #else
                    wc_AesEcbDecrypt(&enc[i], bench_plain, bench_cipher,
                        benchSz);
                #endif
                    ret = 0;
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_aes_dec;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_aes_dec:
    bench_stats_sym_finish(decLabel, useDeviceID, count, benchSz,
                           start, ret);

#endif /* HAVE_AES_DECRYPT */

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_AesFree(&enc[i]);
    }
}

void bench_aesecb(int useDeviceID)
{
#ifdef WOLFSSL_AES_128
    bench_aesecb_internal(useDeviceID, bench_key, 16,
                 "AES-128-ECB-enc", "AES-128-ECB-dec");
#endif
#ifdef WOLFSSL_AES_192
    bench_aesecb_internal(useDeviceID, bench_key, 24,
                 "AES-192-ECB-enc", "AES-192-ECB-dec");
#endif
#ifdef WOLFSSL_AES_256
    bench_aesecb_internal(useDeviceID, bench_key, 32,
                 "AES-256-ECB-enc", "AES-256-ECB-dec");
#endif
}
#endif /* HAVE_AES_ECB */

#ifdef WOLFSSL_AES_CFB
static void bench_aescfb_internal(const byte* key, word32 keySz, const byte* iv,
                                  const char* label)
{
    Aes    enc;
    double start;
    int    i, ret, count;

    ret = wc_AesSetKey(&enc, key, keySz, iv, AES_ENCRYPTION);
    if (ret != 0) {
        printf("AesSetKey failed, ret = %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            if((ret = wc_AesCfbEncrypt(&enc, bench_plain, bench_cipher,
                            bench_size)) != 0) {
                printf("wc_AesCfbEncrypt failed, ret = %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(label, 0, count, bench_size, start, ret);
}

void bench_aescfb(void)
{
#ifdef WOLFSSL_AES_128
    bench_aescfb_internal(bench_key, 16, bench_iv, "AES-128-CFB");
#endif
#ifdef WOLFSSL_AES_192
    bench_aescfb_internal(bench_key, 24, bench_iv, "AES-192-CFB");
#endif
#ifdef WOLFSSL_AES_256
    bench_aescfb_internal(bench_key, 32, bench_iv, "AES-256-CFB");
#endif
}
#endif /* WOLFSSL_AES_CFB */


#ifdef WOLFSSL_AES_OFB
static void bench_aesofb_internal(const byte* key, word32 keySz, const byte* iv,
                                  const char* label)
{
    Aes    enc;
    double start;
    int    i, ret, count;

    ret = wc_AesInit(&enc, NULL, INVALID_DEVID);
    if (ret != 0) {
        printf("AesInit failed, ret = %d\n", ret);
        return;
    }

    ret = wc_AesSetKey(&enc, key, keySz, iv, AES_ENCRYPTION);
    if (ret != 0) {
        printf("AesSetKey failed, ret = %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            if((ret = wc_AesOfbEncrypt(&enc, bench_plain, bench_cipher,
                            bench_size)) != 0) {
                printf("wc_AesCfbEncrypt failed, ret = %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(label, 0, count, bench_size, start, ret);

    wc_AesFree(&enc);
}

void bench_aesofb(void)
{
#ifdef WOLFSSL_AES_128
    bench_aesofb_internal(bench_key, 16, bench_iv, "AES-128-OFB");
#endif
#ifdef WOLFSSL_AES_192
    bench_aesofb_internal(bench_key, 24, bench_iv, "AES-192-OFB");
#endif
#ifdef WOLFSSL_AES_256
    bench_aesofb_internal(bench_key, 32, bench_iv, "AES-256-OFB");
#endif
}
#endif /* WOLFSSL_AES_CFB */


#ifdef WOLFSSL_AES_XTS
void bench_aesxts(void)
{
    XtsAes aes;
    double start;
    int    i, count, ret;

    static unsigned char k1[] = {
        0xa1, 0xb9, 0x0c, 0xba, 0x3f, 0x06, 0xac, 0x35,
        0x3b, 0x2c, 0x34, 0x38, 0x76, 0x08, 0x17, 0x62,
        0x09, 0x09, 0x23, 0x02, 0x6e, 0x91, 0x77, 0x18,
        0x15, 0xf2, 0x9d, 0xab, 0x01, 0x93, 0x2f, 0x2f
    };

    static unsigned char i1[] = {
        0x4f, 0xae, 0xf7, 0x11, 0x7c, 0xda, 0x59, 0xc6,
        0x6e, 0x4b, 0x92, 0x01, 0x3e, 0x76, 0x8a, 0xd5
    };

    ret = wc_AesXtsSetKey(&aes, k1, sizeof(k1), AES_ENCRYPTION,
            HEAP_HINT, devId);
    if (ret != 0) {
        printf("wc_AesXtsSetKey failed, ret = %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            if ((ret = wc_AesXtsEncrypt(&aes, bench_cipher, bench_plain,
                            bench_size, i1, sizeof(i1))) != 0) {
                printf("wc_AesXtsEncrypt failed, ret = %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("AES-XTS-enc", 0, count, bench_size, start, ret);
    wc_AesXtsFree(&aes);

    /* decryption benchmark */
    ret = wc_AesXtsSetKey(&aes, k1, sizeof(k1), AES_DECRYPTION,
            HEAP_HINT, devId);
    if (ret != 0) {
        printf("wc_AesXtsSetKey failed, ret = %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            if ((ret = wc_AesXtsDecrypt(&aes, bench_plain, bench_cipher,
                            bench_size, i1, sizeof(i1))) != 0) {
                printf("wc_AesXtsDecrypt failed, ret = %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("AES-XTS-dec", 0, count, bench_size, start, ret);
    wc_AesXtsFree(&aes);
}
#endif /* WOLFSSL_AES_XTS */


#ifdef WOLFSSL_AES_COUNTER
static void bench_aesctr_internal(const byte* key, word32 keySz, const byte* iv,
                                  const char* label)
{
    Aes    enc;
    double start;
    int    i, count, ret = 0;

    wc_AesSetKeyDirect(&enc, key, keySz, iv, AES_ENCRYPTION);

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            if((ret = wc_AesCtrEncrypt(&enc, bench_plain, bench_cipher,
                                                            bench_size)) != 0) {
                printf("wc_AesCtrEncrypt failed, ret = %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(label, 0, count, bench_size, start, ret);
}

void bench_aesctr(void)
{
#ifdef WOLFSSL_AES_128
    bench_aesctr_internal(bench_key, 16, bench_iv, "AES-128-CTR");
#endif
#ifdef WOLFSSL_AES_192
    bench_aesctr_internal(bench_key, 24, bench_iv, "AES-192-CTR");
#endif
#ifdef WOLFSSL_AES_256
    bench_aesctr_internal(bench_key, 32, bench_iv, "AES-256-CTR");
#endif
}
#endif /* WOLFSSL_AES_COUNTER */


#ifdef HAVE_AESCCM
void bench_aesccm(int useDeviceID)
{
    Aes    enc;
    double start;
    int    ret, i, count;

    WC_DECLARE_VAR(bench_additional, byte, AES_AUTH_ADD_SZ, HEAP_HINT);
    WC_DECLARE_VAR(bench_tag, byte, AES_AUTH_TAG_SZ, HEAP_HINT);

#ifdef WC_DECLARE_VAR_IS_HEAP_ALLOC
    if (bench_additional == NULL || bench_tag == NULL) {
        printf("bench_aesccm malloc failed\n");
        goto exit;
    }
#endif

    XMEMSET(bench_tag, 0, AES_AUTH_TAG_SZ);
    XMEMSET(bench_additional, 0, AES_AUTH_ADD_SZ);

    if ((ret = wc_AesInit(&enc, HEAP_HINT,
        useDeviceID ? devId : INVALID_DEVID)) != 0) {
        printf("wc_AesInit failed, ret = %d\n", ret);
        goto exit;
    }

    if ((ret = wc_AesCcmSetKey(&enc, bench_key, 16)) != 0) {
        printf("wc_AesCcmSetKey failed, ret = %d\n", ret);
        goto exit;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret |= wc_AesCcmEncrypt(&enc, bench_cipher, bench_plain, bench_size,
                bench_iv, 12, bench_tag, AES_AUTH_TAG_SZ,
                bench_additional, 0);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(AES_AAD_STRING("AES-CCM-enc"), useDeviceID, count,
        bench_size, start, ret);
    if (ret != 0) {
        printf("wc_AesCcmEncrypt failed, ret = %d\n", ret);
        goto exit;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret |= wc_AesCcmDecrypt(&enc, bench_plain, bench_cipher, bench_size,
                bench_iv, 12, bench_tag, AES_AUTH_TAG_SZ,
                bench_additional, 0);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(AES_AAD_STRING("AES-CCM-dec"), useDeviceID, count,
        bench_size, start, ret);
    if (ret != 0) {
        printf("wc_AesCcmEncrypt failed, ret = %d\n", ret);
        goto exit;
    }

  exit:

    WC_FREE_VAR(bench_additional, HEAP_HINT);
    WC_FREE_VAR(bench_tag, HEAP_HINT);
}
#endif /* HAVE_AESCCM */


#ifdef WOLFSSL_AES_SIV
static void bench_aessiv_internal(const byte* key, word32 keySz, const char*
                                  encLabel, const char* decLabel)
{
    int i;
    int ret = 0;
    byte assoc[AES_BLOCK_SIZE];
    byte nonce[AES_BLOCK_SIZE];
    byte siv[AES_BLOCK_SIZE];
    int count = 0;
    double start;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_AesSivEncrypt(key, keySz, assoc, AES_BLOCK_SIZE, nonce,
                                   AES_BLOCK_SIZE, bench_plain, bench_size,
                                   siv, bench_cipher);
            if (ret != 0) {
                printf("wc_AesSivEncrypt failed (%d)\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(encLabel, 0, count, bench_size, start, ret);

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_AesSivDecrypt(key, keySz, assoc, AES_BLOCK_SIZE, nonce,
                                   AES_BLOCK_SIZE, bench_cipher, bench_size,
                                   siv, bench_plain);
            if (ret != 0) {
                printf("wc_AesSivDecrypt failed (%d)\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(decLabel, 0, count, bench_size, start, ret);
}

void bench_aessiv(void)
{
    bench_aessiv_internal(bench_key, 32, "AES-256-SIV-enc", "AES-256-SIV-dec");
    bench_aessiv_internal(bench_key, 48, "AES-384-SIV-enc", "AES-384-SIV-dec");
    bench_aessiv_internal(bench_key, 64, "AES-512-SIV-enc", "AES-512-SIV-dec");
}
#endif /* WOLFSSL_AES_SIV */
#endif /* !NO_AES */


#ifdef HAVE_POLY1305
void bench_poly1305(void)
{
    Poly1305 enc;
    byte     mac[16];
    double   start;
    int      ret = 0, i, count;

    if (digest_stream) {
        ret = wc_Poly1305SetKey(&enc, bench_key, 32);
        if (ret != 0) {
            printf("Poly1305SetKey failed, ret = %d\n", ret);
            return;
        }

        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_Poly1305Update(&enc, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Poly1305Update failed: %d\n", ret);
                    break;
                }
            }
            wc_Poly1305Final(&enc, mac);
            count += i;
        } while (bench_stats_check(start));
        bench_stats_sym_finish("POLY1305", 0, count, bench_size, start, ret);
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_Poly1305SetKey(&enc, bench_key, 32);
                if (ret != 0) {
                    printf("Poly1305SetKey failed, ret = %d\n", ret);
                    return;
                }
                ret = wc_Poly1305Update(&enc, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Poly1305Update failed: %d\n", ret);
                    break;
                }
                wc_Poly1305Final(&enc, mac);
            }
            count += i;
        } while (bench_stats_check(start));
        bench_stats_sym_finish("POLY1305", 0, count, bench_size, start, ret);
    }
}
#endif /* HAVE_POLY1305 */


#ifdef HAVE_CAMELLIA
void bench_camellia(void)
{
    Camellia cam;
    double   start;
    int      ret, i, count;

    ret = wc_CamelliaSetKey(&cam, bench_key, 16, bench_iv);
    if (ret != 0) {
        printf("CamelliaSetKey failed, ret = %d\n", ret);
        return;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_CamelliaCbcEncrypt(&cam, bench_cipher, bench_plain,
                                                            bench_size);
            if (ret < 0) {
                printf("CamelliaCbcEncrypt failed: %d\n", ret);
                return;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("Camellia", 0, count, bench_size, start, ret);
}
#endif


#ifndef NO_DES3
void bench_des(int useDeviceID)
{
    int    ret = 0, i, count = 0, times, pending = 0;
    Des3   enc[BENCH_MAX_PENDING];
    double start;

    /* clear for done cleanup */
    XMEMSET(enc, 0, sizeof(enc));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_Des3Init(&enc[i], HEAP_HINT,
                                useDeviceID ? devId : INVALID_DEVID)) != 0) {
            printf("Des3Init failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_Des3_SetKey(&enc[i], bench_key, bench_iv, DES_ENCRYPTION);
        if (ret != 0) {
            printf("Des3_SetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                    ret = wc_Des3_CbcEncrypt(&enc[i], bench_cipher, bench_plain,
                        bench_size);
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_3des;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_3des:
    bench_stats_sym_finish("3DES", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Des3Free(&enc[i]);
    }
}
#endif /* !NO_DES3 */


#ifndef NO_RC4
void bench_arc4(int useDeviceID)
{
    int    ret = 0, i, count = 0, times, pending = 0;
    Arc4   enc[BENCH_MAX_PENDING];
    double start;

    /* clear for done cleanup */
    XMEMSET(enc, 0, sizeof(enc));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        if ((ret = wc_Arc4Init(&enc[i], HEAP_HINT,
                            useDeviceID ? devId : INVALID_DEVID)) != 0) {
            printf("Arc4Init failed, ret = %d\n", ret);
            goto exit;
        }

        ret = wc_Arc4SetKey(&enc[i], bench_key, 16);
        if (ret != 0) {
            printf("Arc4SetKey failed, ret = %d\n", ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&enc[i]), 0,
                                                 &times, numBlocks, &pending)) {
                    ret = wc_Arc4Process(&enc[i], bench_cipher, bench_plain,
                        bench_size);
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&enc[i]),
                                                         0, &times, &pending)) {
                        goto exit_arc4;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_arc4:
    bench_stats_sym_finish("ARC4", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Arc4Free(&enc[i]);
    }
}
#endif /* !NO_RC4 */


#ifdef HAVE_CHACHA
void bench_chacha(void)
{
    ChaCha enc;
    double start;
    int    i, count;

    wc_Chacha_SetKey(&enc, bench_key, 16);

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            wc_Chacha_SetIV(&enc, bench_iv, 0);
            wc_Chacha_Process(&enc, bench_cipher, bench_plain, bench_size);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("CHACHA", 0, count, bench_size, start, 0);
}
#endif /* HAVE_CHACHA*/

#if defined(HAVE_CHACHA) && defined(HAVE_POLY1305)
void bench_chacha20_poly1305_aead(void)
{
    double start;
    int    ret = 0, i, count;

    byte authTag[CHACHA20_POLY1305_AEAD_AUTHTAG_SIZE];
    XMEMSET(authTag, 0, sizeof(authTag));

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_ChaCha20Poly1305_Encrypt(bench_key, bench_iv, NULL, 0,
                bench_plain, bench_size, bench_cipher, authTag);
            if (ret < 0) {
                printf("wc_ChaCha20Poly1305_Encrypt error: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("CHA-POLY", 0, count, bench_size, start, ret);
}
#endif /* HAVE_CHACHA && HAVE_POLY1305 */


#ifndef NO_MD5
void bench_md5(int useDeviceID)
{
    wc_Md5 hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_MD5_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_MD5_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitMd5_ex(&hash[i], HEAP_HINT,
                        useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitMd5_ex failed, ret = %d\n", ret);
                goto exit;
            }
        #ifdef WOLFSSL_PIC32MZ_HASH
            wc_Md5SizeSet(&hash[i], numBlocks * bench_size);
        #endif
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Md5Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_md5;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);

                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Md5Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_md5;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitMd5_ex(hash, HEAP_HINT, INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Md5Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Md5Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_md5;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_md5:
    bench_stats_sym_finish("MD5", useDeviceID, count, bench_size, start, ret);

exit:

#ifdef WOLFSSL_ASYNC_CRYPT
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Md5Free(&hash[i]);
    }
#endif

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* !NO_MD5 */


#ifndef NO_SHA
void bench_sha(int useDeviceID)
{
    wc_Sha hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha_ex(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha failed, ret = %d\n", ret);
                goto exit;
            }
        #ifdef WOLFSSL_PIC32MZ_HASH
            wc_ShaSizeSet(&hash[i], numBlocks * bench_size);
        #endif
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_ShaUpdate(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);

                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_ShaFinal(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha_ex(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_ShaUpdate(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_ShaFinal(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha:
    bench_stats_sym_finish("SHA", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_ShaFree(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* NO_SHA */


#ifdef WOLFSSL_SHA224
void bench_sha224(int useDeviceID)
{
    wc_Sha224 hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA224_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA224_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha224_ex(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha224_ex failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha224Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha224;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha224Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha224;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha224_ex(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha224Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha224Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha224;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha224:
    bench_stats_sym_finish("SHA-224", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha224Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif

#ifndef NO_SHA256
void bench_sha256(int useDeviceID)
{
    wc_Sha256 hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA256_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA256_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha256_ex(&hash[i], HEAP_HINT,
                useDeviceID ? devId: INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha256_ex failed, ret = %d\n", ret);
                goto exit;
            }
        #ifdef WOLFSSL_PIC32MZ_HASH
            wc_Sha256SizeSet(&hash[i], numBlocks * bench_size);
        #endif
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha256Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha256;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha256Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha256;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha256_ex(hash, HEAP_HINT,
                    useDeviceID ? devId: INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha256Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha256Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha256;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha256:
    bench_stats_sym_finish("SHA-256", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha256Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif

#ifdef WOLFSSL_SHA384
void bench_sha384(int useDeviceID)
{
    wc_Sha384 hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA384_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA384_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha384_ex(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha384_ex failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha384Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha384;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha384Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha384;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha384_ex(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha384Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha384Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha384;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha384:
    bench_stats_sym_finish("SHA-384", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha384Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif

#ifdef WOLFSSL_SHA512
void bench_sha512(int useDeviceID)
{
    wc_Sha512 hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA512_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA512_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha512_ex(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha512_ex failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha512Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha512;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha512Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha512;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha512_ex(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha512Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha512Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha512;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha512:
    bench_stats_sym_finish("SHA-512", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha512Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif


#ifdef WOLFSSL_SHA3
#ifndef WOLFSSL_NOSHA3_224
void bench_sha3_224(int useDeviceID)
{
    wc_Sha3   hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_224_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_224_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha3_224(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha3_224 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_224_Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_224;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_224_Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_224;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha3_224(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha3_224_Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha3_224_Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha3_224;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha3_224:
    bench_stats_sym_finish("SHA3-224", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha3_224_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_NOSHA3_224 */

#ifndef WOLFSSL_NOSHA3_256
void bench_sha3_256(int useDeviceID)
{
    wc_Sha3   hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_256_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_256_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha3_256(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha3_256 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_256_Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_256;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_256_Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_256;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha3_256(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha3_256_Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha3_256_Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha3_256;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha3_256:
    bench_stats_sym_finish("SHA3-256", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha3_256_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_NOSHA3_256 */

#ifndef WOLFSSL_NOSHA3_384
void bench_sha3_384(int useDeviceID)
{
    wc_Sha3   hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_384_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_384_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha3_384(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha3_384 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_384_Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_384;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_384_Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_384;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha3_384(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha3_384_Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha3_384_Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha3_384;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha3_384:
    bench_stats_sym_finish("SHA3-384", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha3_384_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_NOSHA3_384 */

#ifndef WOLFSSL_NOSHA3_512
void bench_sha3_512(int useDeviceID)
{
    wc_Sha3   hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_512_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_512_DIGEST_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitSha3_512(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitSha3_512 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_512_Update(&hash[i], bench_plain,
                            bench_size);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_512;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Sha3_512_Final(&hash[i], digest[i]);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_sha3_512;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitSha3_512(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Sha3_512_Update(hash, bench_plain, bench_size);
                if (ret == 0)
                    ret = wc_Sha3_512_Final(hash, digest[0]);
                if (ret != 0)
                    goto exit_sha3_512;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_sha3_512:
    bench_stats_sym_finish("SHA3-512", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Sha3_512_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_NOSHA3_512 */

#ifndef WOLFSSL_NO_SHAKE128
void bench_shake128(int useDeviceID)
{
    wc_Shake hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_128_BLOCK_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_128_BLOCK_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitShake128(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitShake128 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Shake128_Update(&hash[i], bench_plain,
                            BENCH_SIZE);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_shake128;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Shake128_Final(&hash[i], digest[i],
                            WC_SHA3_128_BLOCK_SIZE);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_shake128;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitShake128(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Shake128_Update(hash, bench_plain, BENCH_SIZE);
                if (ret == 0)
                    ret = wc_Shake128_Final(hash, digest[0],
                        WC_SHA3_128_BLOCK_SIZE);
                if (ret != 0)
                    goto exit_shake128;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_shake128:
    bench_stats_sym_finish("SHAKE128", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Shake128_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_NO_SHAKE128 */

#ifdef WOLFSSL_SHAKE256
void bench_shake256(int useDeviceID)
{
    wc_Shake hash[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_256_BLOCK_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_SHA3_256_BLOCK_SIZE, HEAP_HINT);

    /* clear for done cleanup */
    XMEMSET(hash, 0, sizeof(hash));

    if (digest_stream) {
        /* init keys */
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            ret = wc_InitShake256(&hash[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("InitShake256 failed, ret = %d\n", ret);
                goto exit;
            }
        }

        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Shake256_Update(&hash[i], bench_plain,
                            BENCH_SIZE);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_shake256;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;

            times = 0;
            do {
                bench_async_poll(&pending);
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hash[i]),
                                              0, &times, numBlocks, &pending)) {
                        ret = wc_Shake256_Final(&hash[i], digest[i],
                            WC_SHA3_256_BLOCK_SIZE);
                        if (!bench_async_handle(&ret,
                            BENCH_ASYNC_GET_DEV(&hash[i]), 0, &times, &pending)) {
                            goto exit_shake256;
                        }
                    }
                } /* for i */
            } while (pending > 0);
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < numBlocks; times++) {
                ret = wc_InitShake256(hash, HEAP_HINT,
                    useDeviceID ? devId : INVALID_DEVID);
                if (ret == 0)
                    ret = wc_Shake256_Update(hash, bench_plain, BENCH_SIZE);
                if (ret == 0)
                    ret = wc_Shake256_Final(hash, digest[0],
                        WC_SHA3_256_BLOCK_SIZE);
                if (ret != 0)
                    goto exit_shake256;
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
    }
exit_shake256:
    bench_stats_sym_finish("SHAKE256", useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_Shake256_Free(&hash[i]);
    }

    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* WOLFSSL_SHAKE256 */
#endif


#ifdef WOLFSSL_RIPEMD
int bench_ripemd(void)
{
    RipeMd hash;
    byte   digest[RIPEMD_DIGEST_SIZE];
    double start;
    int    i, count, ret = 0;

    if (digest_stream) {
        ret = wc_InitRipeMd(&hash);
        if (ret != 0) {
            return ret;
        }

        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_RipeMdUpdate(&hash, bench_plain, bench_size);
                if (ret != 0) {
                    return ret;
                }
            }
            ret = wc_RipeMdFinal(&hash, digest);
            if (ret != 0) {
                return ret;
            }

            count += i;
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_InitRipeMd(&hash);
                if (ret != 0) {
                    return ret;
                }
                ret = wc_RipeMdUpdate(&hash, bench_plain, bench_size);
                if (ret != 0) {
                    return ret;
                }
                ret = wc_RipeMdFinal(&hash, digest);
                if (ret != 0) {
                    return ret;
                }
            }
            count += i;
        } while (bench_stats_check(start));
    }
    bench_stats_sym_finish("RIPEMD", 0, count, bench_size, start, ret);

    return 0;
}
#endif


#ifdef HAVE_BLAKE2
void bench_blake2b(void)
{
    Blake2b b2b;
    byte    digest[64];
    double  start;
    int     ret = 0, i, count;

    if (digest_stream) {
        ret = wc_InitBlake2b(&b2b, 64);
        if (ret != 0) {
            printf("InitBlake2b failed, ret = %d\n", ret);
            return;
        }

        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_Blake2bUpdate(&b2b, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Blake2bUpdate failed, ret = %d\n", ret);
                    return;
                }
            }
            ret = wc_Blake2bFinal(&b2b, digest, 64);
            if (ret != 0) {
                printf("Blake2bFinal failed, ret = %d\n", ret);
                return;
            }
            count += i;
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_InitBlake2b(&b2b, 64);
                if (ret != 0) {
                    printf("InitBlake2b failed, ret = %d\n", ret);
                    return;
                }
                ret = wc_Blake2bUpdate(&b2b, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Blake2bUpdate failed, ret = %d\n", ret);
                    return;
                }
                ret = wc_Blake2bFinal(&b2b, digest, 64);
                if (ret != 0) {
                    printf("Blake2bFinal failed, ret = %d\n", ret);
                    return;
                }
            }
            count += i;
        } while (bench_stats_check(start));
    }
    bench_stats_sym_finish("BLAKE2b", 0, count, bench_size, start, ret);
}
#endif

#if defined(HAVE_BLAKE2S)
void bench_blake2s(void)
{
    Blake2s b2s;
    byte    digest[32];
    double  start;
    int     ret = 0, i, count;

    if (digest_stream) {
        ret = wc_InitBlake2s(&b2s, 32);
        if (ret != 0) {
            printf("InitBlake2s failed, ret = %d\n", ret);
            return;
        }

        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_Blake2sUpdate(&b2s, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Blake2sUpdate failed, ret = %d\n", ret);
                    return;
                }
            }
            ret = wc_Blake2sFinal(&b2s, digest, 32);
            if (ret != 0) {
                printf("Blake2sFinal failed, ret = %d\n", ret);
                return;
            }
            count += i;
        } while (bench_stats_check(start));
    }
    else {
        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < numBlocks; i++) {
                ret = wc_InitBlake2s(&b2s, 32);
                if (ret != 0) {
                    printf("InitBlake2b failed, ret = %d\n", ret);
                    return;
                }
                ret = wc_Blake2sUpdate(&b2s, bench_plain, bench_size);
                if (ret != 0) {
                    printf("Blake2bUpdate failed, ret = %d\n", ret);
                    return;
                }
                ret = wc_Blake2sFinal(&b2s, digest, 32);
                if (ret != 0) {
                    printf("Blake2sFinal failed, ret = %d\n", ret);
                    return;
                }
            }
            count += i;
        } while (bench_stats_check(start));
    }
    bench_stats_sym_finish("BLAKE2s", 0, count, bench_size, start, ret);
}
#endif


#ifdef WOLFSSL_CMAC

static void bench_cmac_helper(int keySz, const char* outMsg, int useDeviceID)
{
    Cmac    cmac;
    byte    digest[AES_BLOCK_SIZE];
    word32  digestSz = sizeof(digest);
    double  start;
    int     ret, i, count;
#ifdef WOLFSSL_SECO_CAAM
    unsigned int keyID;
    int keyGroup = 1; /* group one was chosen arbitrarily */
    int keyInfo = CAAM_KEY_TRANSIENT;
    int keyType = CAAM_KEYTYPE_AES128;
    byte pubKey[AES_256_KEY_SIZE];

    if (keySz == AES_256_KEY_SIZE) {
        keyType = CAAM_KEYTYPE_AES256;
    }

    if (useDeviceID &&
            wc_SECO_GenerateKey(CAAM_GENERATE_KEY, keyGroup, pubKey, 0, keyType,
            keyInfo, &keyID) != 0) {
        printf("Error generating key in hsm\n");
        return;
    }
#endif
    (void)useDeviceID;

    bench_stats_start(&count, &start);
    do {
    #ifdef HAVE_FIPS
        ret = wc_InitCmac(&cmac, bench_key, keySz, WC_CMAC_AES, NULL);
    #else
        ret = wc_InitCmac_ex(&cmac, bench_key, keySz, WC_CMAC_AES, NULL,
            HEAP_HINT, useDeviceID ? devId : INVALID_DEVID);
    #endif
        if (ret != 0) {
            printf("InitCmac failed, ret = %d\n", ret);
            return;
        }
    #ifdef WOLFSSL_SECO_CAAM
        if (useDeviceID) {
            wc_SECO_CMACSetKeyID(&cmac, keyID);
        }
    #endif

        for (i = 0; i < numBlocks; i++) {
            ret = wc_CmacUpdate(&cmac, bench_plain, bench_size);
            if (ret != 0) {
                printf("CmacUpdate failed, ret = %d\n", ret);
                return;
            }
        }
        /* Note: final force zero's the Cmac struct */
        ret = wc_CmacFinal(&cmac, digest, &digestSz);
        if (ret != 0) {
            printf("CmacFinal failed, ret = %d\n", ret);
            return;
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish(outMsg, 0, count, bench_size, start, ret);
}

void bench_cmac(int useDeviceID)
{
#ifdef WOLFSSL_AES_128
    bench_cmac_helper(16, "AES-128-CMAC", useDeviceID);
#endif
#ifdef WOLFSSL_AES_256
    bench_cmac_helper(32, "AES-256-CMAC", useDeviceID);
#endif

}
#endif /* WOLFSSL_CMAC */

#ifdef HAVE_SCRYPT

void bench_scrypt(void)
{
    byte   derived[64];
    double start;
    int    ret, i, count;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < scryptCnt; i++) {
            ret = wc_scrypt(derived, (byte*)"pleaseletmein", 13,
                            (byte*)"SodiumChloride", 14, 14, 8, 1, sizeof(derived));
            if (ret != 0) {
                printf("scrypt failed, ret = %d\n", ret);
                goto exit;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit:
    bench_stats_asym_finish("scrypt", 17, "", 0, count, start, ret);
}

#endif /* HAVE_SCRYPT */

#ifndef NO_HMAC

static void bench_hmac(int useDeviceID, int type, int digestSz,
                       byte* key, word32 keySz, const char* label)
{
    Hmac   hmac[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
#ifdef WOLFSSL_ASYNC_CRYPT
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_MAX_DIGEST_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, WC_MAX_DIGEST_SIZE, HEAP_HINT);
#else
    byte digest[BENCH_MAX_PENDING][WC_MAX_DIGEST_SIZE];
#endif

    (void)digestSz;

    /* clear for done cleanup */
    XMEMSET(hmac, 0, sizeof(hmac));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        ret = wc_HmacInit(&hmac[i], HEAP_HINT,
                useDeviceID ? devId : INVALID_DEVID);
        if (ret != 0) {
            printf("wc_HmacInit failed for %s, ret = %d\n", label, ret);
            goto exit;
        }

        ret = wc_HmacSetKey(&hmac[i], type, key, keySz);
        if (ret != 0) {
            printf("wc_HmacSetKey failed for %s, ret = %d\n", label, ret);
            goto exit;
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < numBlocks || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hmac[i]), 0,
                                      &times, numBlocks, &pending)) {
                    ret = wc_HmacUpdate(&hmac[i], bench_plain, bench_size);
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&hmac[i]),
                                            0, &times, &pending)) {
                        goto exit_hmac;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;

        times = 0;
        do {
            bench_async_poll(&pending);

            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&hmac[i]), 0,
                                      &times, numBlocks, &pending)) {
                    ret = wc_HmacFinal(&hmac[i], digest[i]);
                    if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(&hmac[i]),
                                            0, &times, &pending)) {
                        goto exit_hmac;
                    }
                }
            } /* for i */
        } while (pending > 0);
    } while (bench_stats_check(start));
exit_hmac:
    bench_stats_sym_finish(label, useDeviceID, count, bench_size, start, ret);

exit:

    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_HmacFree(&hmac[i]);
    }

#ifdef WOLFSSL_ASYNC_CRYPT
    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
#endif
}

#ifndef NO_MD5

void bench_hmac_md5(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_MD5, WC_MD5_DIGEST_SIZE, key, sizeof(key),
               "HMAC-MD5");
}

#endif /* NO_MD5 */

#ifndef NO_SHA

void bench_hmac_sha(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_SHA, WC_SHA_DIGEST_SIZE, key, sizeof(key),
               "HMAC-SHA");
}

#endif /* NO_SHA */

#ifdef WOLFSSL_SHA224

void bench_hmac_sha224(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_SHA224, WC_SHA224_DIGEST_SIZE, key, sizeof(key),
               "HMAC-SHA224");
}

#endif /* WOLFSSL_SHA224 */

#ifndef NO_SHA256

void bench_hmac_sha256(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_SHA256, WC_SHA256_DIGEST_SIZE, key, sizeof(key),
               "HMAC-SHA256");
}

#endif /* NO_SHA256 */

#ifdef WOLFSSL_SHA384

void bench_hmac_sha384(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_SHA384, WC_SHA384_DIGEST_SIZE, key, sizeof(key),
               "HMAC-SHA384");
}

#endif /* WOLFSSL_SHA384 */

#ifdef WOLFSSL_SHA512

void bench_hmac_sha512(int useDeviceID)
{
    byte key[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b };

    bench_hmac(useDeviceID, WC_SHA512, WC_SHA512_DIGEST_SIZE, key, sizeof(key),
               "HMAC-SHA512");
}

#endif /* WOLFSSL_SHA512 */

#ifndef NO_PWDBASED
void bench_pbkdf2(void)
{
    double start;
    int    ret = 0, count = 0;
    const char* passwd32 = "passwordpasswordpasswordpassword";
    const byte salt32[] = { 0x78, 0x57, 0x8E, 0x5a, 0x5d, 0x63, 0xcb, 0x06,
                            0x78, 0x57, 0x8E, 0x5a, 0x5d, 0x63, 0xcb, 0x06,
                            0x78, 0x57, 0x8E, 0x5a, 0x5d, 0x63, 0xcb, 0x06,
                            0x78, 0x57, 0x8E, 0x5a, 0x5d, 0x63, 0xcb, 0x06 };
    byte derived[32];

    bench_stats_start(&count, &start);
    do {
        ret = wc_PBKDF2(derived, (const byte*)passwd32, (int)XSTRLEN(passwd32),
            salt32, (int)sizeof(salt32), 1000, 32, WC_SHA256);
        count++;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("PBKDF2", 32, count, 32, start, ret);
}
#endif /* !NO_PWDBASED */

#endif /* NO_HMAC */

#ifdef WOLFSSL_SIPHASH
void bench_siphash(void)
{
    double start;
    int    ret = 0, count;
    const char* passwd16 = "passwordpassword";
    byte out[16];
    int    i;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_SipHash((const byte*)passwd16, bench_plain, bench_size,
                out, 8);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("SipHash-8", 1, count, bench_size, start, ret);

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < numBlocks; i++) {
            ret = wc_SipHash((const byte*)passwd16, bench_plain, bench_size,
                out, 16);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_sym_finish("SipHash-16", 1, count, bench_size, start, ret);
}
#endif

#ifndef NO_RSA

#if defined(WOLFSSL_KEY_GEN)
static void bench_rsaKeyGen_helper(int useDeviceID, int keySz)
{
    RsaKey genKey[BENCH_MAX_PENDING];
    double start;
    int    ret = 0, i, count = 0, times, pending = 0;
    const long rsa_e_val = WC_RSA_EXPONENT;
    const char**desc = bench_desc_words[lng_index];

    /* clear for done cleanup */
    XMEMSET(genKey, 0, sizeof(genKey));

    bench_stats_start(&count, &start);
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < genTimes || pending > 0; ) {
            bench_async_poll(&pending);

            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&genKey[i]),
                                               0, &times, genTimes, &pending)) {

                    wc_FreeRsaKey(&genKey[i]);
                    ret = wc_InitRsaKey_ex(&genKey[i], HEAP_HINT, devId);
                    if (ret < 0) {
                        goto exit;
                    }

                    ret = wc_MakeRsaKey(&genKey[i], keySz, rsa_e_val, &gRng);
                    if (!bench_async_handle(&ret,
                        BENCH_ASYNC_GET_DEV(&genKey[i]), 0, &times, &pending)) {
                        goto exit;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit:
    bench_stats_asym_finish("RSA", keySz, desc[2], useDeviceID, count, start, ret);

    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_FreeRsaKey(&genKey[i]);
    }
}

void bench_rsaKeyGen(int useDeviceID)
{
    int    k, keySz;
#if !defined(WOLFSSL_SP_MATH) || defined(WOLFSSL_SP_MATH_ALL)
    const int  keySizes[2] = {1024, 2048};
#else
    const int  keySizes[1] = {2048};
#endif

    for (k = 0; k < (int)(sizeof(keySizes)/sizeof(int)); k++) {
        keySz = keySizes[k];
        bench_rsaKeyGen_helper(useDeviceID, keySz);
    }
}


void bench_rsaKeyGen_size(int useDeviceID, int keySz)
{
    bench_rsaKeyGen_helper(useDeviceID, keySz);
}
#endif /* WOLFSSL_KEY_GEN */

#if !defined(USE_CERT_BUFFERS_1024) && !defined(USE_CERT_BUFFERS_2048) && \
    !defined(USE_CERT_BUFFERS_3072) && !defined(USE_CERT_BUFFERS_4096)
    #if defined(WOLFSSL_MDK_SHELL)
        static char *certRSAname = "certs/rsa2048.der";
        /* set by shell command */
        static void set_Bench_RSA_File(char * cert) { certRSAname = cert ; }
    #elif defined(FREESCALE_MQX)
        static char *certRSAname = "a:\\certs\\rsa2048.der";
    #else
        static const char *certRSAname = "certs/rsa2048.der";
    #endif
#endif

#define RSA_BUF_SIZE 384  /* for up to 3072 bit */

#if defined(WOLFSSL_RSA_VERIFY_INLINE) || defined(WOLFSSL_RSA_PUBLIC_ONLY)
#if defined(USE_CERT_BUFFERS_2048)
static unsigned char rsa_2048_sig[] = {
    0x8c, 0x9e, 0x37, 0xbf, 0xc3, 0xa6, 0xba, 0x1c,
    0x53, 0x22, 0x40, 0x4b, 0x8b, 0x0d, 0x3c, 0x0e,
    0x2e, 0x8c, 0x31, 0x2c, 0x47, 0xbf, 0x03, 0x48,
    0x18, 0x46, 0x73, 0x8d, 0xd7, 0xdd, 0x17, 0x64,
    0x0d, 0x7f, 0xdc, 0x74, 0xed, 0x80, 0xc3, 0xe8,
    0x9a, 0x18, 0x33, 0xd4, 0xe6, 0xc5, 0xe1, 0x54,
    0x75, 0xd1, 0xbb, 0x40, 0xde, 0xa8, 0xb9, 0x1b,
    0x14, 0xe8, 0xc1, 0x39, 0xeb, 0xa0, 0x69, 0x8a,
    0xc6, 0x9b, 0xef, 0x53, 0xb5, 0x23, 0x2b, 0x78,
    0x06, 0x43, 0x37, 0x11, 0x81, 0x84, 0x73, 0x33,
    0x33, 0xfe, 0xf7, 0x5d, 0x2b, 0x84, 0xd6, 0x83,
    0xd6, 0xdd, 0x55, 0x33, 0xef, 0xd1, 0xf7, 0x12,
    0xb0, 0xc2, 0x0e, 0xb1, 0x78, 0xd4, 0xa8, 0xa3,
    0x25, 0xeb, 0xed, 0x9a, 0xb3, 0xee, 0xc3, 0x7e,
    0xce, 0x13, 0x18, 0x86, 0x31, 0xe1, 0xef, 0x01,
    0x0f, 0x6e, 0x67, 0x24, 0x74, 0xbd, 0x0b, 0x7f,
    0xa9, 0xca, 0x6f, 0xaa, 0x83, 0x28, 0x90, 0x40,
    0xf1, 0xb5, 0x10, 0x0e, 0x26, 0x03, 0x05, 0x5d,
    0x87, 0xb4, 0xe0, 0x4c, 0x98, 0xd8, 0xc6, 0x42,
    0x89, 0x77, 0xeb, 0xb6, 0xd4, 0xe6, 0x26, 0xf3,
    0x31, 0x25, 0xde, 0x28, 0x38, 0x58, 0xe8, 0x2c,
    0xf4, 0x56, 0x7c, 0xb6, 0xfd, 0x99, 0xb0, 0xb0,
    0xf4, 0x83, 0xb6, 0x74, 0xa9, 0x5b, 0x9f, 0xe8,
    0xe9, 0xf1, 0xa1, 0x2a, 0xbd, 0xf6, 0x83, 0x28,
    0x09, 0xda, 0xa6, 0xd6, 0xcd, 0x61, 0x60, 0xf7,
    0x13, 0x4e, 0x46, 0x57, 0x38, 0x1e, 0x11, 0x92,
    0x6b, 0x6b, 0xcf, 0xd3, 0xf4, 0x8b, 0x66, 0x03,
    0x25, 0xa3, 0x7a, 0x2f, 0xce, 0xc1, 0x85, 0xa5,
    0x48, 0x91, 0x8a, 0xb3, 0x4f, 0x5d, 0x98, 0xb1,
    0x69, 0x58, 0x47, 0x69, 0x0c, 0x52, 0xdc, 0x42,
    0x4c, 0xef, 0xe8, 0xd4, 0x4d, 0x6a, 0x33, 0x7d,
    0x9e, 0xd2, 0x51, 0xe6, 0x41, 0xbf, 0x4f, 0xa2
};
#elif defined(USE_CERT_BUFFERS_3072)
static unsigned char rsa_3072_sig[] = {
    0x1a, 0xd6, 0x0d, 0xfd, 0xe3, 0x41, 0x95, 0x76,
    0x27, 0x16, 0x7d, 0xc7, 0x94, 0x16, 0xca, 0xa8,
    0x26, 0x08, 0xbe, 0x78, 0x87, 0x72, 0x4c, 0xd9,
    0xa7, 0xfc, 0x33, 0x77, 0x2d, 0x53, 0x07, 0xb5,
    0x8c, 0xce, 0x48, 0x17, 0x9b, 0xff, 0x9f, 0x9b,
    0x17, 0xc4, 0xbb, 0x72, 0xed, 0xdb, 0xa0, 0x34,
    0x69, 0x5b, 0xc7, 0x4e, 0xbf, 0xec, 0x13, 0xc5,
    0x98, 0x71, 0x9a, 0x4e, 0x18, 0x0e, 0xcb, 0xe7,
    0xc6, 0xd5, 0x21, 0x31, 0x7c, 0x0d, 0xae, 0x14,
    0x2b, 0x87, 0x4f, 0x77, 0x95, 0x2e, 0x26, 0xe2,
    0x83, 0xfe, 0x49, 0x1e, 0x87, 0x19, 0x4a, 0x63,
    0x73, 0x75, 0xf1, 0xf5, 0x71, 0xd2, 0xce, 0xd4,
    0x39, 0x2b, 0xd9, 0xe0, 0x76, 0x70, 0xc8, 0xf8,
    0xed, 0xdf, 0x90, 0x57, 0x17, 0xb9, 0x16, 0xf6,
    0xe9, 0x49, 0x48, 0xce, 0x5a, 0x8b, 0xe4, 0x84,
    0x7c, 0xf3, 0x31, 0x68, 0x97, 0x45, 0x68, 0x38,
    0x50, 0x3a, 0x70, 0xbd, 0xb3, 0xd3, 0xd2, 0xe0,
    0x56, 0x5b, 0xc2, 0x0c, 0x2c, 0x10, 0x70, 0x7b,
    0xd4, 0x99, 0xf9, 0x38, 0x31, 0xb1, 0x86, 0xa0,
    0x07, 0xf1, 0xf6, 0x53, 0xb0, 0x44, 0x82, 0x40,
    0xd2, 0xab, 0x0e, 0x71, 0x5d, 0xe1, 0xea, 0x3a,
    0x77, 0xc9, 0xef, 0xfe, 0x54, 0x65, 0xa3, 0x49,
    0xfd, 0xa5, 0x33, 0xaa, 0x16, 0x1a, 0x38, 0xe7,
    0xaa, 0xb7, 0x13, 0xb2, 0x3b, 0xc7, 0x00, 0x87,
    0x12, 0xfe, 0xfd, 0xf4, 0x55, 0x6d, 0x1d, 0x4a,
    0x0e, 0xad, 0xd0, 0x4c, 0x55, 0x91, 0x60, 0xd9,
    0xef, 0x74, 0x69, 0x22, 0x8c, 0x51, 0x65, 0xc2,
    0x04, 0xac, 0xd3, 0x8d, 0xf7, 0x35, 0x29, 0x13,
    0x6d, 0x61, 0x7c, 0x39, 0x2f, 0x41, 0x4c, 0xdf,
    0x38, 0xfd, 0x1a, 0x7d, 0x42, 0xa7, 0x6f, 0x3f,
    0x3d, 0x9b, 0xd1, 0x97, 0xab, 0xc0, 0xa7, 0x28,
    0x1c, 0xc0, 0x02, 0x26, 0xeb, 0xce, 0xf9, 0xe1,
    0x34, 0x45, 0xaf, 0xbf, 0x8d, 0xb8, 0xe0, 0xff,
    0xd9, 0x6f, 0x77, 0xf3, 0xf7, 0xed, 0x6a, 0xbb,
    0x03, 0x52, 0xfb, 0x38, 0xfc, 0xea, 0x9f, 0xc9,
    0x98, 0xed, 0x21, 0x45, 0xaf, 0x43, 0x2b, 0x64,
    0x96, 0x82, 0x30, 0xe9, 0xb4, 0x36, 0x89, 0x77,
    0x07, 0x4a, 0xc6, 0x1f, 0x38, 0x7a, 0xee, 0xb6,
    0x86, 0xf6, 0x2f, 0x03, 0xec, 0xa2, 0xe5, 0x48,
    0xe5, 0x5a, 0xf5, 0x1c, 0xd2, 0xd9, 0xd8, 0x2d,
    0x9d, 0x06, 0x07, 0xc9, 0x8b, 0x5d, 0xe0, 0x0f,
    0x5e, 0x0c, 0x53, 0x27, 0xff, 0x23, 0xee, 0xca,
    0x5e, 0x4d, 0xf1, 0x95, 0x77, 0x78, 0x1f, 0xf2,
    0x44, 0x5b, 0x7d, 0x01, 0x49, 0x61, 0x6f, 0x6d,
    0xbf, 0xf5, 0x19, 0x06, 0x39, 0xe9, 0xe9, 0x29,
    0xde, 0x47, 0x5e, 0x2e, 0x1f, 0x68, 0xf4, 0x32,
    0x5e, 0xe9, 0xd0, 0xa7, 0xb4, 0x2a, 0x45, 0xdf,
    0x15, 0x7d, 0x0d, 0x5b, 0xef, 0xc6, 0x23, 0xac
};
#else
    #error Not Supported Yet!
#endif
#endif /* WOLFSSL_RSA_VERIFY_INLINE || WOLFSSL_RSA_PUBLIC_ONLY */

static void bench_rsa_helper(int useDeviceID, RsaKey rsaKey[BENCH_MAX_PENDING],
        int rsaKeySz)
{
    int         ret = 0, i, times, count = 0, pending = 0;
    word32      idx = 0;
#ifndef WOLFSSL_RSA_VERIFY_ONLY
    const char* messageStr = TEST_STRING;
    const int   len = (int)TEST_STRING_SZ;
#endif
    double      start = 0.0F;
    const char**desc = bench_desc_words[lng_index];
#ifndef WOLFSSL_RSA_VERIFY_ONLY
    WC_DECLARE_VAR(message, byte, TEST_STRING_SZ, HEAP_HINT);
#endif
    WC_DECLARE_ARRAY_DYNAMIC_DEC(enc, byte, BENCH_MAX_PENDING, rsaKeySz, HEAP_HINT);
    #if !defined(WOLFSSL_RSA_VERIFY_INLINE) && !defined(WOLFSSL_RSA_PUBLIC_ONLY)
        WC_DECLARE_ARRAY_DYNAMIC_DEC(out, byte, BENCH_MAX_PENDING, rsaKeySz, HEAP_HINT);
    #else
        byte* out[BENCH_MAX_PENDING];
    #endif

    WC_DECLARE_ARRAY_DYNAMIC_EXE(enc, byte, BENCH_MAX_PENDING, rsaKeySz, HEAP_HINT);
    #if !defined(WOLFSSL_RSA_VERIFY_INLINE) && !defined(WOLFSSL_RSA_PUBLIC_ONLY)
        WC_DECLARE_ARRAY_DYNAMIC_EXE(out, byte, BENCH_MAX_PENDING, rsaKeySz, HEAP_HINT);
        if (out[0] == NULL) {
            ret = MEMORY_E;
            goto exit;
        }
    #endif
    if (enc[0] == NULL) {
        ret = MEMORY_E;
        goto exit;
    }

#ifndef WOLFSSL_RSA_VERIFY_ONLY
    #ifdef WC_DECLARE_VAR_IS_HEAP_ALLOC
    if (message == NULL) {
        ret = MEMORY_E;
        goto exit;
    }
    #endif
    XMEMCPY(message, messageStr, len);
#endif

    if (!rsa_sign_verify) {
#ifndef WOLFSSL_RSA_VERIFY_ONLY
        /* begin public RSA */
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < ntimes || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                 1, &times, ntimes, &pending)) {
                        ret = wc_RsaPublicEncrypt(message, (word32)len, enc[i],
                                                  rsaKeySz/8, &rsaKey[i],
                                                  GLOBAL_RNG);
                        if (!bench_async_handle(&ret, BENCH_ASYNC_GET_DEV(
                                            &rsaKey[i]), 1, &times, &pending)) {
                            goto exit_rsa_verify;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
exit_rsa_verify:
        bench_stats_asym_finish("RSA", rsaKeySz, desc[0], useDeviceID, count,
                                                                    start, ret);
#endif /* !WOLFSSL_RSA_VERIFY_ONLY */

#ifndef WOLFSSL_RSA_PUBLIC_ONLY
        if (ret < 0) {
            goto exit;
        }

        /* capture resulting encrypt length */
        idx = (word32)(rsaKeySz/8);

        /* begin private async RSA */
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < ntimes || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                 1, &times, ntimes, &pending)) {
                        ret = wc_RsaPrivateDecrypt(enc[i], idx, out[i],
                                                       rsaKeySz/8, &rsaKey[i]);
                        if (!bench_async_handle(&ret,
                                                BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                1, &times, &pending)) {
                            goto exit_rsa_pub;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
exit_rsa_pub:
        bench_stats_asym_finish("RSA", rsaKeySz, desc[1], useDeviceID, count,
                                                                    start, ret);
#endif /* !WOLFSSL_RSA_PUBLIC_ONLY */
    }
    else {
#if !defined(WOLFSSL_RSA_PUBLIC_ONLY) && !defined(WOLFSSL_RSA_VERIFY_ONLY)
        /* begin RSA sign */
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < ntimes || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                 1, &times, ntimes, &pending)) {
                        ret = wc_RsaSSL_Sign(message, len, enc[i],
                                                rsaKeySz/8, &rsaKey[i], &gRng);
                        if (!bench_async_handle(&ret,
                                                BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                1, &times, &pending)) {
                            goto exit_rsa_sign;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
exit_rsa_sign:
        bench_stats_asym_finish("RSA", rsaKeySz, desc[4], useDeviceID, count, start,
                                                                           ret);

        if (ret < 0) {
            goto exit;
        }
#endif /* !WOLFSSL_RSA_PUBLIC_ONLY && !WOLFSSL_RSA_VERIFY_ONLY */

        /* capture resulting encrypt length */
        idx = rsaKeySz/8;

        /* begin RSA verify */
        bench_stats_start(&count, &start);
        do {
            for (times = 0; times < ntimes || pending > 0; ) {
                bench_async_poll(&pending);

                /* while free pending slots in queue, submit ops */
                for (i = 0; i < BENCH_MAX_PENDING; i++) {
                    if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                 1, &times, ntimes, &pending)) {
                    #if !defined(WOLFSSL_RSA_VERIFY_INLINE) && \
                        !defined(WOLFSSL_RSA_PUBLIC_ONLY)
                        ret = wc_RsaSSL_Verify(enc[i], idx, out[i],
                                                      rsaKeySz/8, &rsaKey[i]);
                    #elif defined(USE_CERT_BUFFERS_2048)
                        XMEMCPY(enc[i], rsa_2048_sig, sizeof(rsa_2048_sig));
                        idx = sizeof(rsa_2048_sig);
                        out[i] = NULL;
                        ret = wc_RsaSSL_VerifyInline(enc[i], idx, &out[i],
                                                                    &rsaKey[i]);
                        if (ret > 0)
                            ret = 0;
                    #elif defined(USE_CERT_BUFFERS_3072)
                        XMEMCPY(enc[i], rsa_3072_sig, sizeof(rsa_3072_sig));
                        idx = sizeof(rsa_3072_sig);
                        out[i] = NULL;
                        ret = wc_RsaSSL_VerifyInline(enc[i], idx, &out[i],
                                                                    &rsaKey[i]);
                        if (ret > 0)
                            ret = 0;
                    #endif
                        if (!bench_async_handle(&ret,
                                                BENCH_ASYNC_GET_DEV(&rsaKey[i]),
                                                1, &times, &pending)) {
                            goto exit_rsa_verifyinline;
                        }
                    }
                } /* for i */
            } /* for times */
            count += times;
        } while (bench_stats_check(start));
exit_rsa_verifyinline:
        bench_stats_asym_finish("RSA", rsaKeySz, desc[5], useDeviceID, count,
                                                                    start, ret);
    }

exit:

    WC_FREE_ARRAY_DYNAMIC(enc, BENCH_MAX_PENDING, HEAP_HINT);
#if !defined(WOLFSSL_RSA_VERIFY_INLINE) && !defined(WOLFSSL_RSA_PUBLIC_ONLY)
    WC_FREE_ARRAY_DYNAMIC(out, BENCH_MAX_PENDING, HEAP_HINT);
#endif
#ifndef WOLFSSL_RSA_VERIFY_ONLY
    WC_FREE_VAR(message, HEAP_HINT);
#endif
}

void bench_rsa(int useDeviceID)
{
    int         i;
    RsaKey      rsaKey[BENCH_MAX_PENDING];
    int         ret = 0;
    int         rsaKeySz = 0;
    const byte* tmp;
    size_t      bytes;
#if !defined(WOLFSSL_RSA_PUBLIC_ONLY) && !defined(WOLFSSL_RSA_VERIFY_ONLY)
    word32      idx;
#endif

#ifdef USE_CERT_BUFFERS_1024
    tmp = rsa_key_der_1024;
    bytes = (size_t)sizeof_rsa_key_der_1024;
    rsaKeySz = 1024;
#elif defined(USE_CERT_BUFFERS_2048)
    tmp = rsa_key_der_2048;
    bytes = (size_t)sizeof_rsa_key_der_2048;
    rsaKeySz = 2048;
#elif defined(USE_CERT_BUFFERS_3072)
    tmp = rsa_key_der_3072;
    bytes = (size_t)sizeof_rsa_key_der_3072;
    rsaKeySz = 3072;
#elif defined(USE_CERT_BUFFERS_4096)
    tmp = client_key_der_4096;
    bytes = (size_t)sizeof_client_key_der_4096;
    rsaKeySz = 4096;
#else
    #error "need a cert buffer size"
#endif /* USE_CERT_BUFFERS */

    /* clear for done cleanup */
    XMEMSET(rsaKey, 0, sizeof(rsaKey));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        /* setup an async context for each key */
        ret = wc_InitRsaKey_ex(&rsaKey[i], HEAP_HINT,
            useDeviceID ? devId : INVALID_DEVID);
        if (ret < 0) {
            goto exit_bench_rsa;
        }

#if !defined(WOLFSSL_RSA_PUBLIC_ONLY) && !defined(WOLFSSL_RSA_VERIFY_ONLY)
    #ifdef WC_RSA_BLINDING
        ret = wc_RsaSetRNG(&rsaKey[i], &gRng);
        if (ret != 0)
            goto exit_bench_rsa;
    #endif
#endif

#if !defined(WOLFSSL_RSA_PUBLIC_ONLY) && !defined(WOLFSSL_RSA_VERIFY_ONLY)
        /* decode the private key */
        idx = 0;
        if ((ret = wc_RsaPrivateKeyDecode(tmp, &idx, &rsaKey[i],
                                                        (word32)bytes)) != 0) {
            printf("wc_RsaPrivateKeyDecode failed! %d\n", ret);
            goto exit_bench_rsa;
        }
#elif defined(WOLFSSL_PUBLIC_MP)
        /* get offset to public portion of the RSA key */
    #ifdef USE_CERT_BUFFERS_1024
        bytes = 11;
    #elif defined(USE_CERT_BUFFERS_2048) || defined(USE_CERT_BUFFERS_3072)
        bytes = 12;
    #endif
        ret = mp_read_unsigned_bin(&rsaKey[i].n, &tmp[bytes], rsaKeySz/8);
        if (ret != 0) {
            printf("wc_RsaPrivateKeyDecode failed! %d\n", ret);
            goto exit_bench_rsa;
        }
        ret = mp_set_int(&rsaKey[i].e, WC_RSA_EXPONENT);
        if (ret != 0) {
            printf("wc_RsaPrivateKeyDecode failed! %d\n", ret);
            goto exit_bench_rsa;
        }
#else
        /* Note: To benchmark public only define WOLFSSL_PUBLIC_MP */
        rsaKeySz = 0;
#endif
    }

    if (rsaKeySz > 0) {
        bench_rsa_helper(useDeviceID, rsaKey, rsaKeySz);
    }

    (void)bytes;
    (void)tmp;

exit_bench_rsa:
    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_FreeRsaKey(&rsaKey[i]);
    }
}


#ifdef WOLFSSL_KEY_GEN
/* bench any size of RSA key */
void bench_rsa_key(int useDeviceID, int rsaKeySz)
{
    int     ret = 0, i, pending = 0;
    RsaKey  rsaKey[BENCH_MAX_PENDING];
    int     isPending[BENCH_MAX_PENDING];
    long    exp = 65537L;

    /* clear for done cleanup */
    XMEMSET(rsaKey, 0, sizeof(rsaKey));
    XMEMSET(isPending, 0, sizeof(isPending));

    /* init keys */
    do {
        pending = 0;
        for (i = 0; i < BENCH_MAX_PENDING; i++) {
            if (!isPending[i]) { /* if making the key is pending then just call
                                  * wc_MakeRsaKey again */
                /* setup an async context for each key */
                if (wc_InitRsaKey_ex(&rsaKey[i], HEAP_HINT,
                        useDeviceID ? devId : INVALID_DEVID) < 0) {
                    goto exit_bench_rsa_key;
                }

            #ifdef WC_RSA_BLINDING
                ret = wc_RsaSetRNG(&rsaKey[i], &gRng);
                if (ret != 0)
                    goto exit_bench_rsa_key;
            #endif
            }

            /* create the RSA key */
            ret = wc_MakeRsaKey(&rsaKey[i], rsaKeySz, exp, &gRng);
            if (ret == WC_PENDING_E) {
                isPending[i] = 1;
                pending      = 1;
            }
            else if (ret != 0) {
                printf("wc_MakeRsaKey failed! %d\n", ret);
                goto exit_bench_rsa_key;
            }
        } /* for i */
    } while (pending > 0);

    bench_rsa_helper(useDeviceID, rsaKey, rsaKeySz);
exit_bench_rsa_key:

    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_FreeRsaKey(&rsaKey[i]);
    }
}
#endif /* WOLFSSL_KEY_GEN */
#endif /* !NO_RSA */


#ifndef NO_DH

#if !defined(USE_CERT_BUFFERS_1024) && !defined(USE_CERT_BUFFERS_2048) && \
    !defined(USE_CERT_BUFFERS_3072) && !defined(USE_CERT_BUFFERS_4096)
    #if defined(WOLFSSL_MDK_SHELL)
        static char *certDHname = "certs/dh2048.der";
        /* set by shell command */
        void set_Bench_DH_File(char * cert) { certDHname = cert ; }
    #elif defined(FREESCALE_MQX)
        static char *certDHname = "a:\\certs\\dh2048.der";
    #elif defined(NO_ASN)
        /* do nothing, but don't need a file */
    #else
        static const char *certDHname = "certs/dh2048.der";
    #endif
#endif

#ifdef HAVE_FFDHE_4096
#define BENCH_DH_KEY_SIZE  512 /* for 4096 bit */
#else
#define BENCH_DH_KEY_SIZE  384 /* for 3072 bit */
#endif
#define BENCH_DH_PRIV_SIZE (BENCH_DH_KEY_SIZE/8)

void bench_dh(int useDeviceID)
{
    int    ret = 0, i;
    int    count = 0, times, pending = 0;
    const byte* tmp = NULL;
    double start = 0.0F;
    DhKey  dhKey[BENCH_MAX_PENDING];
    int    dhKeySz = BENCH_DH_KEY_SIZE * 8; /* used in printf */
    const char**desc = bench_desc_words[lng_index];
#ifndef NO_ASN
    size_t bytes = 0;
    word32 idx;
#endif
    word32 pubSz[BENCH_MAX_PENDING];
    word32 privSz[BENCH_MAX_PENDING];
    word32 pubSz2 = BENCH_DH_KEY_SIZE;
    word32 privSz2 = BENCH_DH_PRIV_SIZE;
    word32 agreeSz[BENCH_MAX_PENDING];
#if defined(HAVE_FFDHE_2048) || defined(HAVE_FFDHE_3072)
#ifdef HAVE_PUBLIC_FFDHE
    const DhParams *params = NULL;
#else
    int paramName = 0;
#endif
#endif

    WC_DECLARE_ARRAY(pub, byte, BENCH_MAX_PENDING, BENCH_DH_KEY_SIZE, HEAP_HINT);
    WC_DECLARE_VAR(pub2, byte, BENCH_DH_KEY_SIZE, HEAP_HINT);
    WC_DECLARE_ARRAY(agree, byte, BENCH_MAX_PENDING, BENCH_DH_KEY_SIZE, HEAP_HINT);
    WC_DECLARE_ARRAY(priv, byte, BENCH_MAX_PENDING, BENCH_DH_PRIV_SIZE, HEAP_HINT);
    WC_DECLARE_VAR(priv2, byte, BENCH_DH_PRIV_SIZE, HEAP_HINT);

    WC_INIT_ARRAY(pub, byte, BENCH_MAX_PENDING, BENCH_DH_KEY_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(agree, byte, BENCH_MAX_PENDING, BENCH_DH_KEY_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(priv, byte, BENCH_MAX_PENDING, BENCH_DH_PRIV_SIZE, HEAP_HINT);

#ifdef WC_DECLARE_VAR_IS_HEAP_ALLOC
    if (pub[0] == NULL || pub2 == NULL || agree[0] == NULL || priv[0] == NULL || priv2 == NULL) {
        ret = MEMORY_E;
        goto exit;
    }
#endif

    (void)tmp;

    if (!use_ffdhe) {
#if defined(NO_ASN)
        dhKeySz = 1024;
        /* do nothing, but don't use default FILE */
#elif defined(USE_CERT_BUFFERS_1024)
        tmp = dh_key_der_1024;
        bytes = (size_t)sizeof_dh_key_der_1024;
        dhKeySz = 1024;
#elif defined(USE_CERT_BUFFERS_2048)
        tmp = dh_key_der_2048;
        bytes = (size_t)sizeof_dh_key_der_2048;
        dhKeySz = 2048;
#elif defined(USE_CERT_BUFFERS_3072)
        tmp = dh_key_der_3072;
        bytes = (size_t)sizeof_dh_key_der_3072;
        dhKeySz = 3072;
#elif defined(USE_CERT_BUFFERS_4096)
        tmp = dh_key_der_4096;
        bytes = (size_t)sizeof_dh_key_der_4096;
        dhKeySz = 4096;
#else
    #error "need to define a cert buffer size"
#endif /* USE_CERT_BUFFERS */
    }
#ifdef HAVE_FFDHE_2048
    else if (use_ffdhe == 2048) {
#ifdef HAVE_PUBLIC_FFDHE
        params = wc_Dh_ffdhe2048_Get();
#else
        paramName = WC_FFDHE_2048;
#endif
        dhKeySz = 2048;
    }
#endif
#ifdef HAVE_FFDHE_3072
    else if (use_ffdhe == 3072) {
#ifdef HAVE_PUBLIC_FFDHE
        params = wc_Dh_ffdhe3072_Get();
#else
        paramName = WC_FFDHE_3072;
#endif
        dhKeySz = 3072;
    }
#endif
#ifdef HAVE_FFDHE_4096
    else if (use_ffdhe == 4096) {
#ifdef HAVE_PUBLIC_FFDHE
        params = wc_Dh_ffdhe4096_Get();
#else
        paramName = WC_FFDHE_4096;
#endif
        dhKeySz = 4096;
    }
#endif

    /* clear for done cleanup */
    XMEMSET(dhKey, 0, sizeof(dhKey));

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        /* setup an async context for each key */
        ret = wc_InitDhKey_ex(&dhKey[i], HEAP_HINT,
                        useDeviceID ? devId : INVALID_DEVID);
        if (ret != 0)
            goto exit;

        /* setup key */
        if (!use_ffdhe) {
    #ifdef NO_ASN
            ret = wc_DhSetKey(&dhKey[i], dh_p, sizeof(dh_p), dh_g,
                                                                  sizeof(dh_g));
    #else
            idx = 0;
            ret = wc_DhKeyDecode(tmp, &idx, &dhKey[i], (word32)bytes);
    #endif
        }
    #if defined(HAVE_FFDHE_2048) || defined(HAVE_FFDHE_3072)
    #ifdef HAVE_PUBLIC_FFDHE
        else if (params != NULL) {
            ret = wc_DhSetKey(&dhKey[i], params->p, params->p_len, params->g,
                                                                 params->g_len);
        }
    #else
        else if (paramName != 0) {
            ret = wc_DhSetNamedKey(&dhKey[i], paramName);
        }
    #endif
    #endif
        if (ret != 0) {
            printf("DhKeyDecode failed %d, can't benchmark\n", ret);
            goto exit;
        }
    }

    /* Key Gen */
    bench_stats_start(&count, &start);
    PRIVATE_KEY_UNLOCK();
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < genTimes || pending > 0; ) {
            bench_async_poll(&pending);

            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&dhKey[i]),
                                               0, &times, genTimes, &pending)) {
                    privSz[i] = BENCH_DH_PRIV_SIZE;
                    pubSz[i] = BENCH_DH_KEY_SIZE;
                    ret = wc_DhGenerateKeyPair(&dhKey[i], &gRng, priv[i], &privSz[i],
                        pub[i], &pubSz[i]);
                    if (!bench_async_handle(&ret,
                        BENCH_ASYNC_GET_DEV(&dhKey[i]), 0, &times, &pending)) {
                        goto exit_dh_gen;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
    PRIVATE_KEY_LOCK();
exit_dh_gen:
    bench_stats_asym_finish("DH", dhKeySz, desc[2], useDeviceID, count, start, ret);

    if (ret < 0) {
        goto exit;
    }

    /* Generate key to use as other public */
    PRIVATE_KEY_UNLOCK();
    ret = wc_DhGenerateKeyPair(&dhKey[0], &gRng, priv2, &privSz2, pub2, &pubSz2);
    PRIVATE_KEY_LOCK();
#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wc_AsyncWait(ret, &dhKey[0].asyncDev, WC_ASYNC_FLAG_NONE);
#endif

    /* Key Agree */
    bench_stats_start(&count, &start);
    PRIVATE_KEY_UNLOCK();
    do {
        for (times = 0; times < agreeTimes || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&dhKey[i]),
                                             0, &times, agreeTimes, &pending)) {
                    ret = wc_DhAgree(&dhKey[i], agree[i], &agreeSz[i], priv[i],
                        privSz[i], pub2, pubSz2);
                    if (!bench_async_handle(&ret,
                        BENCH_ASYNC_GET_DEV(&dhKey[i]), 0, &times, &pending)) {
                        goto exit;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
    PRIVATE_KEY_LOCK();
exit:
    bench_stats_asym_finish("DH", dhKeySz, desc[3], useDeviceID, count, start, ret);

    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_FreeDhKey(&dhKey[i]);
    }

    WC_FREE_ARRAY(pub, BENCH_MAX_PENDING, HEAP_HINT);
    WC_FREE_VAR(pub2, HEAP_HINT);
    WC_FREE_ARRAY(priv, BENCH_MAX_PENDING, HEAP_HINT);
    WC_FREE_VAR(priv2, HEAP_HINT);
    WC_FREE_ARRAY(agree, BENCH_MAX_PENDING, HEAP_HINT);
}
#endif /* !NO_DH */

#ifdef WOLFSSL_HAVE_KYBER
static void bench_kyber_keygen(int type, const char* name, int keySize,
    KyberKey* key)
{
    int ret = 0, times, count, pending = 0;
    double start;
    const char**desc = bench_desc_words[lng_index];

    /* KYBER Make Key */
    bench_stats_start(&count, &start);
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < agreeTimes || pending > 0; times++) {
            wc_KyberKey_Free(key);
            ret = wc_KyberKey_Init(type, key, HEAP_HINT, INVALID_DEVID);
            if (ret != 0)
                goto exit;

#ifdef KYBER_NONDETERMINISTIC
            ret = wc_KyberKey_MakeKey(key, &gRng);
#else
            unsigned char rand[KYBER_MAKEKEY_RAND_SZ] = {0,};
            ret = wc_KyberKey_MakeKeyWithRandom(key, rand, sizeof(rand));
#endif
            if (ret != 0)
                goto exit;
        } /* for times */
        count += times;
    }
    while (bench_stats_check(start));

exit:
    bench_stats_asym_finish(name, keySize, desc[2], 0, count, start, ret);
}

static void bench_kyber_encap(const char* name, int keySize, KyberKey* key)
{
    int ret = 0, times, count, pending = 0;
    double start;
    const char**desc = bench_desc_words[lng_index];
    byte ct[KYBER_MAX_CIPHER_TEXT_SIZE];
    byte ss[KYBER_SS_SZ];
    word32 ctSz;

    ret = wc_KyberKey_CipherTextSize(key, &ctSz);
    if (ret != 0) {
        return;
    }

    /* KYBER Encapsulate */
    bench_stats_start(&count, &start);
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < agreeTimes || pending > 0; times++) {
#ifdef KYBER_NONDETERMINISTIC
            ret = wc_KyberKey_Encapsulate(key, ct, ss, &gRng);
#else
            unsigned char rand[KYBER_ENC_RAND_SZ] = {0,};
            ret = wc_KyberKey_EncapsulateWithRandom(key, ct, ss, rand,
                sizeof(rand));
#endif
            if (ret != 0)
                goto exit_encap;
        } /* for times */
        count += times;
    }
    while (bench_stats_check(start));

exit_encap:
    bench_stats_asym_finish(name, keySize, desc[9], 0, count, start, ret);

    /* KYBER Decapsulate */
    bench_stats_start(&count, &start);
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < agreeTimes || pending > 0; times++) {
            ret = wc_KyberKey_Decapsulate(key, ss, ct, ctSz);
            if (ret != 0)
                goto exit_decap;
        } /* for times */
        count += times;
    }
    while (bench_stats_check(start));

exit_decap:
    bench_stats_asym_finish(name, keySize, desc[13], 0, count, start, ret);
}

void bench_kyber(int type)
{
    KyberKey key;
    const char* name = NULL;
    int keySize = 0;

    switch (type) {
#ifdef WOLFSSL_KYBER512
    case KYBER512:
        name = "KYBER512 ";
        keySize = 128;
        break;
#endif
#ifdef WOLFSSL_KYBER768
    case KYBER768:
        name = "KYBER768 ";
        keySize = 192;
        break;
#endif
#ifdef WOLFSSL_KYBER1024
    case KYBER1024:
        name = "KYBER1024";
        keySize = 256;
        break;
#endif
    }

    bench_kyber_keygen(type, name, keySize, &key);
    bench_kyber_encap(name, keySize, &key);

    wc_KyberKey_Free(&key);
}
#endif

#ifdef HAVE_ECC

/* +8 for 'ECDSA [%s]' and null terminator */
#define BENCH_ECC_NAME_SZ (ECC_MAXNAME + 8)

/* run all benchmarks on a curve */
void bench_ecc_curve(int curveId)
{
    if (bench_all || (bench_asym_algs & BENCH_ECC_MAKEKEY)) {
    #ifndef NO_SW_BENCH
        bench_eccMakeKey(0, curveId);
    #endif
    #if defined(BENCH_DEVID)
        bench_eccMakeKey(1, curveId);
    #endif
    }
    if (bench_all || (bench_asym_algs & BENCH_ECC)) {
    #ifndef NO_SW_BENCH
        bench_ecc(0, curveId);
    #endif
    #if defined(BENCH_DEVID)
        bench_ecc(1, curveId);
    #endif
    }
    #ifdef HAVE_ECC_ENCRYPT
    if (bench_all || (bench_asym_algs & BENCH_ECC_ENCRYPT))
        bench_eccEncrypt(curveId);
    #endif
}


void bench_eccMakeKey(int useDeviceID, int curveId)
{
    int ret = 0, i, times, count, pending = 0;
    int deviceID;
    int keySize;
    ecc_key genKey[BENCH_MAX_PENDING];
    char name[BENCH_ECC_NAME_SZ];
    double start;
    const char**desc = bench_desc_words[lng_index];

    deviceID = useDeviceID ? devId : INVALID_DEVID;
    keySize = wc_ecc_get_curve_size_from_id(curveId);

    /* clear for done cleanup */
    XMEMSET(&genKey, 0, sizeof(genKey));

    /* ECC Make Key */
    bench_stats_start(&count, &start);
    do {
        /* while free pending slots in queue, submit ops */
        for (times = 0; times < agreeTimes || pending > 0; ) {
            bench_async_poll(&pending);

            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&genKey[i]), 0,
                            &times, agreeTimes, &pending)) {

                    wc_ecc_free(&genKey[i]);
                    ret = wc_ecc_init_ex(&genKey[i], HEAP_HINT, deviceID);
                    if (ret < 0) {
                        goto exit;
                    }

                    ret = wc_ecc_make_key_ex(&gRng, keySize, &genKey[i],
                            curveId);
                    if (!bench_async_handle(&ret,
                                BENCH_ASYNC_GET_DEV(&genKey[i]), 0, &times,
                                &pending)) {
                        goto exit;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit:
    (void)XSNPRINTF(name, BENCH_ECC_NAME_SZ, "ECC   [%15s]",
            wc_ecc_get_name(curveId));
    bench_stats_asym_finish(name, keySize * 8, desc[2], useDeviceID, count, start,
            ret);

    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_ecc_free(&genKey[i]);
    }
}


void bench_ecc(int useDeviceID, int curveId)
{
    int ret = 0, i, times, count, pending = 0;
    int deviceID;
    int  keySize;
    char name[BENCH_ECC_NAME_SZ];
    ecc_key genKey[BENCH_MAX_PENDING];
#ifdef HAVE_ECC_DHE
    ecc_key genKey2[BENCH_MAX_PENDING];
#endif
#if !defined(NO_ASN) && defined(HAVE_ECC_SIGN)
#ifdef HAVE_ECC_VERIFY
    int    verify[BENCH_MAX_PENDING];
#endif
#endif
    word32 x[BENCH_MAX_PENDING];
    double start = 0;
    const char**desc = bench_desc_words[lng_index];

#ifdef HAVE_ECC_DHE
    WC_DECLARE_ARRAY(shared, byte, BENCH_MAX_PENDING, MAX_ECC_BYTES, HEAP_HINT);
#endif
#if !defined(NO_ASN) && defined(HAVE_ECC_SIGN)
    WC_DECLARE_ARRAY(sig, byte, BENCH_MAX_PENDING, ECC_MAX_SIG_SIZE, HEAP_HINT);
    WC_DECLARE_ARRAY(digest, byte, BENCH_MAX_PENDING, MAX_ECC_BYTES, HEAP_HINT);
#endif

#ifdef HAVE_ECC_DHE
    WC_INIT_ARRAY(shared, byte, BENCH_MAX_PENDING, MAX_ECC_BYTES, HEAP_HINT);
#endif
#if !defined(NO_ASN) && defined(HAVE_ECC_SIGN)
    WC_INIT_ARRAY(sig, byte, BENCH_MAX_PENDING, ECC_MAX_SIG_SIZE, HEAP_HINT);
    WC_INIT_ARRAY(digest, byte, BENCH_MAX_PENDING, MAX_ECC_BYTES, HEAP_HINT);
#endif
    deviceID = useDeviceID ? devId : INVALID_DEVID;

    /* clear for done cleanup */
    XMEMSET(&genKey, 0, sizeof(genKey));
#ifdef HAVE_ECC_DHE
    XMEMSET(&genKey2, 0, sizeof(genKey2));
#endif
    keySize = wc_ecc_get_curve_size_from_id(curveId);

    /* init keys */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        /* setup an context for each key */
        if ((ret = wc_ecc_init_ex(&genKey[i], HEAP_HINT, deviceID)) < 0) {
            goto exit;
        }
        ret = wc_ecc_make_key_ex(&gRng, keySize, &genKey[i], curveId);
    #ifdef WOLFSSL_ASYNC_CRYPT
        ret = wc_AsyncWait(ret, &genKey[i].asyncDev, WC_ASYNC_FLAG_NONE);
    #endif
        if (ret < 0) {
            goto exit;
        }

    #ifdef HAVE_ECC_DHE
        if ((ret = wc_ecc_init_ex(&genKey2[i], HEAP_HINT, deviceID)) < 0) {
            goto exit;
        }
        if ((ret = wc_ecc_make_key_ex(&gRng, keySize, &genKey2[i],
                    curveId)) > 0) {
            goto exit;
        }
    #endif
    }

#ifdef HAVE_ECC_DHE
#if defined(ECC_TIMING_RESISTANT) && (!defined(HAVE_FIPS) || \
    (!defined(HAVE_FIPS_VERSION) || (HAVE_FIPS_VERSION != 2))) && \
    !defined(HAVE_SELFTEST)
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        (void)wc_ecc_set_rng(&genKey[i], &gRng);
    }
#endif

    /* ECC Shared Secret */
    bench_stats_start(&count, &start);
    PRIVATE_KEY_UNLOCK();
    do {
        for (times = 0; times < agreeTimes || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&genKey[i]), 1,
                            &times, agreeTimes, &pending)) {
                    x[i] = (word32)keySize;
                    ret = wc_ecc_shared_secret(&genKey[i], &genKey2[i],
                            shared[i], &x[i]);
                    if (!bench_async_handle(&ret,
                                BENCH_ASYNC_GET_DEV(&genKey[i]), 1, &times,
                                &pending)) {
                        goto exit_ecdhe;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
    PRIVATE_KEY_UNLOCK();
exit_ecdhe:
    (void)XSNPRINTF(name, BENCH_ECC_NAME_SZ, "ECDHE [%15s]", wc_ecc_get_name(curveId));

    bench_stats_asym_finish(name, keySize * 8, desc[3], useDeviceID, count, start,
            ret);

    if (ret < 0) {
        goto exit;
    }
#endif /* HAVE_ECC_DHE */

#if !defined(NO_ASN) && defined(HAVE_ECC_SIGN)

    /* Init digest to sign */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        for (count = 0; count < keySize; count++) {
            digest[i][count] = (byte)count;
        }
    }

    /* ECC Sign */
    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < agreeTimes || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&genKey[i]), 1,
                            &times, agreeTimes, &pending)) {
                    if (genKey[i].state == 0)
                        x[i] = ECC_MAX_SIG_SIZE;
                    ret = wc_ecc_sign_hash(digest[i], (word32)keySize, sig[i],
                            &x[i], &gRng, &genKey[i]);
                    if (!bench_async_handle(&ret,
                                BENCH_ASYNC_GET_DEV(&genKey[i]), 1, &times,
                                &pending)) {
                        goto exit_ecdsa_sign;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_ecdsa_sign:
    (void)XSNPRINTF(name, BENCH_ECC_NAME_SZ, "ECDSA [%15s]", wc_ecc_get_name(curveId));

    bench_stats_asym_finish(name, keySize * 8, desc[4], useDeviceID, count, start,
            ret);

    if (ret < 0) {
        goto exit;
    }

#ifdef HAVE_ECC_VERIFY

    /* ECC Verify */
    bench_stats_start(&count, &start);
    do {
        for (times = 0; times < agreeTimes || pending > 0; ) {
            bench_async_poll(&pending);

            /* while free pending slots in queue, submit ops */
            for (i = 0; i < BENCH_MAX_PENDING; i++) {
                if (bench_async_check(&ret, BENCH_ASYNC_GET_DEV(&genKey[i]), 1,
                            &times, agreeTimes, &pending)) {
                    if (genKey[i].state == 0)
                        verify[i] = 0;
                    ret = wc_ecc_verify_hash(sig[i], x[i], digest[i],
                                       (word32)keySize, &verify[i], &genKey[i]);
                    if (!bench_async_handle(&ret,
                                BENCH_ASYNC_GET_DEV(&genKey[i]), 1, &times,
                                &pending)) {
                        goto exit_ecdsa_verify;
                    }
                }
            } /* for i */
        } /* for times */
        count += times;
    } while (bench_stats_check(start));
exit_ecdsa_verify:
    (void)XSNPRINTF(name, BENCH_ECC_NAME_SZ, "ECDSA [%15s]", wc_ecc_get_name(curveId));

    bench_stats_asym_finish(name, keySize * 8, desc[5], useDeviceID, count, start,
            ret);
#endif /* HAVE_ECC_VERIFY */
#endif /* !NO_ASN && HAVE_ECC_SIGN */

exit:

    /* cleanup */
    for (i = 0; i < BENCH_MAX_PENDING; i++) {
        wc_ecc_free(&genKey[i]);
    #ifdef HAVE_ECC_DHE
        wc_ecc_free(&genKey2[i]);
    #endif
    }

#ifdef HAVE_ECC_DHE
    WC_FREE_ARRAY(shared, BENCH_MAX_PENDING, HEAP_HINT);
#endif
#if !defined(NO_ASN) && defined(HAVE_ECC_SIGN)
    WC_FREE_ARRAY(sig, BENCH_MAX_PENDING, HEAP_HINT);
    WC_FREE_ARRAY(digest, BENCH_MAX_PENDING, HEAP_HINT);
#endif

    (void)useDeviceID;
    (void)pending;
    (void)x;
    (void)count;
    (void)times;
    (void)desc;
    (void)start;
    (void)name;
}


#ifdef HAVE_ECC_ENCRYPT
void bench_eccEncrypt(int curveId)
{
    ecc_key userA, userB;
    int     keySize;
    byte    msg[48];
    byte    out[sizeof(msg) + WC_SHA256_DIGEST_SIZE + (MAX_ECC_BITS+3)/4 + 2];
    word32  outSz   = sizeof(out);
    word32  bench_plainSz = bench_size;
    int     ret, i, count;
    double start;
    const char**desc = bench_desc_words[lng_index];
    char name[BENCH_ECC_NAME_SZ];

    keySize = wc_ecc_get_curve_size_from_id(curveId);
    ret = wc_ecc_init_ex(&userA, HEAP_HINT, devId);
    if (ret != 0) {
        printf("wc_ecc_encrypt make key A failed: %d\n", ret);
        return;
    }

    ret = wc_ecc_init_ex(&userB, HEAP_HINT, devId);
    if (ret != 0) {
        printf("wc_ecc_encrypt make key B failed: %d\n", ret);
        wc_ecc_free(&userA);
        return;
    }

#if defined(ECC_TIMING_RESISTANT) && (!defined(HAVE_FIPS) || \
    (!defined(HAVE_FIPS_VERSION) || (HAVE_FIPS_VERSION != 2))) && \
    !defined(HAVE_SELFTEST)
    ret = wc_ecc_set_rng(&userA, &gRng);
    if (ret != 0) {
        goto exit;
    }
    ret = wc_ecc_set_rng(&userB, &gRng);
    if (ret != 0) {
        goto exit;
    }
#endif

    ret = wc_ecc_make_key_ex(&gRng, keySize, &userA, curveId);
#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wc_AsyncWait(ret, &userA.asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        goto exit;
    ret = wc_ecc_make_key_ex(&gRng, keySize, &userB, curveId);
#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wc_AsyncWait(ret, &userB.asyncDev, WC_ASYNC_FLAG_NONE);
#endif
    if (ret != 0)
        goto exit;

    for (i = 0; i < (int)sizeof(msg); i++)
        msg[i] = i;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < ntimes; i++) {
            /* encrypt msg to B */
            ret = wc_ecc_encrypt(&userA, &userB, msg, sizeof(msg), out, &outSz,
                    NULL);
            if (ret != 0) {
                printf("wc_ecc_encrypt failed! %d\n", ret);
                goto exit_enc;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_enc:
    (void)XSNPRINTF(name, BENCH_ECC_NAME_SZ, "ECC   [%15s]", wc_ecc_get_name(curveId));
    bench_stats_asym_finish(name, keySize * 8, desc[6], 0, count, start, ret);

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < ntimes; i++) {
            /* decrypt msg from A */
            ret = wc_ecc_decrypt(&userB, &userA, out, outSz, bench_plain,
                    &bench_plainSz, NULL);
            if (ret != 0) {
                printf("wc_ecc_decrypt failed! %d\n", ret);
                goto exit_dec;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_dec:
    bench_stats_asym_finish(name, keySize * 8, desc[7], 0, count, start, ret);

exit:

    /* cleanup */
    wc_ecc_free(&userB);
    wc_ecc_free(&userA);
}
#endif
#endif /* HAVE_ECC */

#ifdef HAVE_CURVE25519
void bench_curve25519KeyGen(int useDeviceID)
{
    curve25519_key genKey;
    double start;
    int    ret = 0, i, count;
    const char**desc = bench_desc_words[lng_index];

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_curve25519_init_ex(&genKey, HEAP_HINT,
                                        useDeviceID ? devId : INVALID_DEVID);
            if (ret != 0) {
                printf("wc_curve25519_init_ex failed: %d\n", ret);
                break;
            }

            ret = wc_curve25519_make_key(&gRng, 32, &genKey);
            wc_curve25519_free(&genKey);
            if (ret != 0) {
                printf("wc_curve25519_make_key failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("CURVE", 25519, desc[2], useDeviceID, count, start,
        ret);
}

#ifdef HAVE_CURVE25519_SHARED_SECRET
void bench_curve25519KeyAgree(int useDeviceID)
{
    curve25519_key genKey, genKey2;
    double start;
    int    ret, i, count;
    byte   shared[32];
    const char**desc = bench_desc_words[lng_index];
    word32 x = 0;

    wc_curve25519_init_ex(&genKey,  HEAP_HINT,
        useDeviceID ? devId : INVALID_DEVID);
    wc_curve25519_init_ex(&genKey2, HEAP_HINT,
        useDeviceID ? devId : INVALID_DEVID);

    ret = wc_curve25519_make_key(&gRng, 32, &genKey);
    if (ret != 0) {
        printf("curve25519_make_key failed\n");
        return;
    }
    ret = wc_curve25519_make_key(&gRng, 32, &genKey2);
    if (ret != 0) {
        printf("curve25519_make_key failed: %d\n", ret);
        wc_curve25519_free(&genKey);
        return;
    }

    /* Shared secret */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            x = sizeof(shared);
            ret = wc_curve25519_shared_secret(&genKey, &genKey2, shared, &x);
            if (ret != 0) {
                printf("curve25519_shared_secret failed: %d\n", ret);
                goto exit;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit:
    bench_stats_asym_finish("CURVE", 25519, desc[3], useDeviceID, count, start,
        ret);

    wc_curve25519_free(&genKey2);
    wc_curve25519_free(&genKey);
}
#endif /* HAVE_CURVE25519_SHARED_SECRET */
#endif /* HAVE_CURVE25519 */

#ifdef HAVE_ED25519
void bench_ed25519KeyGen(void)
{
    ed25519_key genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            wc_ed25519_init(&genKey);
            (void)wc_ed25519_make_key(&gRng, 32, &genKey);
            wc_ed25519_free(&genKey);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ED", 25519, desc[2], 0, count, start, 0);
}


void bench_ed25519KeySign(void)
{
    int    ret;
    ed25519_key genKey;
#ifdef HAVE_ED25519_SIGN
    double start;
    int    i, count;
    byte   sig[ED25519_SIG_SIZE];
    byte   msg[512];
    word32 x = 0;
    const char**desc = bench_desc_words[lng_index];
#endif

    wc_ed25519_init(&genKey);

    ret = wc_ed25519_make_key(&gRng, ED25519_KEY_SIZE, &genKey);
    if (ret != 0) {
        printf("ed25519_make_key failed\n");
        return;
    }

#ifdef HAVE_ED25519_SIGN
    /* make dummy msg */
    for (i = 0; i < (int)sizeof(msg); i++)
        msg[i] = (byte)i;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            x = sizeof(sig);
            ret = wc_ed25519_sign_msg(msg, sizeof(msg), sig, &x, &genKey);
            if (ret != 0) {
                printf("ed25519_sign_msg failed\n");
                goto exit_ed_sign;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_ed_sign:
    bench_stats_asym_finish("ED", 25519, desc[4], 0, count, start, ret);

#ifdef HAVE_ED25519_VERIFY
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            int verify = 0;
            ret = wc_ed25519_verify_msg(sig, x, msg, sizeof(msg), &verify,
                                        &genKey);
            if (ret != 0 || verify != 1) {
                printf("ed25519_verify_msg failed\n");
                goto exit_ed_verify;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_ed_verify:
    bench_stats_asym_finish("ED", 25519, desc[5], 0, count, start, ret);
#endif /* HAVE_ED25519_VERIFY */
#endif /* HAVE_ED25519_SIGN */

    wc_ed25519_free(&genKey);
}
#endif /* HAVE_ED25519 */

#ifdef HAVE_CURVE448
void bench_curve448KeyGen(void)
{
    curve448_key genKey;
    double start;
    int    ret = 0, i, count;
    const char**desc = bench_desc_words[lng_index];

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_curve448_make_key(&gRng, 56, &genKey);
            wc_curve448_free(&genKey);
            if (ret != 0) {
                printf("wc_curve448_make_key failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("CURVE", 448, desc[2], 0, count, start, ret);
}

#ifdef HAVE_CURVE448_SHARED_SECRET
void bench_curve448KeyAgree(void)
{
    curve448_key genKey, genKey2;
    double start;
    int    ret, i, count;
    byte   shared[56];
    const char**desc = bench_desc_words[lng_index];
    word32 x = 0;

    wc_curve448_init(&genKey);
    wc_curve448_init(&genKey2);

    ret = wc_curve448_make_key(&gRng, 56, &genKey);
    if (ret != 0) {
        printf("curve448_make_key failed\n");
        return;
    }
    ret = wc_curve448_make_key(&gRng, 56, &genKey2);
    if (ret != 0) {
        printf("curve448_make_key failed: %d\n", ret);
        wc_curve448_free(&genKey);
        return;
    }

    /* Shared secret */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            x = sizeof(shared);
            ret = wc_curve448_shared_secret(&genKey, &genKey2, shared, &x);
            if (ret != 0) {
                printf("curve448_shared_secret failed: %d\n", ret);
                goto exit;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit:
    bench_stats_asym_finish("CURVE", 448, desc[3], 0, count, start, ret);

    wc_curve448_free(&genKey2);
    wc_curve448_free(&genKey);
}
#endif /* HAVE_CURVE448_SHARED_SECRET */
#endif /* HAVE_CURVE448 */

#ifdef HAVE_ED448
void bench_ed448KeyGen(void)
{
    ed448_key genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            wc_ed448_init(&genKey);
            (void)wc_ed448_make_key(&gRng, ED448_KEY_SIZE, &genKey);
            wc_ed448_free(&genKey);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ED", 448, desc[2], 0, count, start, 0);
}


void bench_ed448KeySign(void)
{
    int    ret;
    ed448_key genKey;
#ifdef HAVE_ED448_SIGN
    double start;
    int    i, count;
    byte   sig[ED448_SIG_SIZE];
    byte   msg[512];
    word32 x = 0;
    const char**desc = bench_desc_words[lng_index];
#endif

    wc_ed448_init(&genKey);

    ret = wc_ed448_make_key(&gRng, ED448_KEY_SIZE, &genKey);
    if (ret != 0) {
        printf("ed448_make_key failed\n");
        return;
    }

#ifdef HAVE_ED448_SIGN
    /* make dummy msg */
    for (i = 0; i < (int)sizeof(msg); i++)
        msg[i] = (byte)i;

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            x = sizeof(sig);
            ret = wc_ed448_sign_msg(msg, sizeof(msg), sig, &x, &genKey,
                                    NULL, 0);
            if (ret != 0) {
                printf("ed448_sign_msg failed\n");
                goto exit_ed_sign;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_ed_sign:
    bench_stats_asym_finish("ED", 448, desc[4], 0, count, start, ret);

#ifdef HAVE_ED448_VERIFY
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            int verify = 0;
            ret = wc_ed448_verify_msg(sig, x, msg, sizeof(msg), &verify,
                                      &genKey, NULL, 0);
            if (ret != 0 || verify != 1) {
                printf("ed448_verify_msg failed\n");
                goto exit_ed_verify;
            }
        }
        count += i;
    } while (bench_stats_check(start));
exit_ed_verify:
    bench_stats_asym_finish("ED", 448, desc[5], 0, count, start, ret);
#endif /* HAVE_ED448_VERIFY */
#endif /* HAVE_ED448_SIGN */

    wc_ed448_free(&genKey);
}
#endif /* HAVE_ED448 */

#ifdef WOLFCRYPT_HAVE_ECCSI
#ifdef WOLFCRYPT_ECCSI_KMS
void bench_eccsiKeyGen(void)
{
    EccsiKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    int    ret;

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            wc_InitEccsiKey(&genKey, NULL, INVALID_DEVID);
            ret = wc_MakeEccsiKey(&genKey, &gRng);
            if (ret != 0) {
                printf("wc_MakeEccsiKey failed: %d\n", ret);
                break;
            }
            wc_FreeEccsiKey(&genKey);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ECCSI", 256, desc[2], 0, count, start, 0);
}

void bench_eccsiPairGen(void)
{
    EccsiKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    mp_int ssk;
    ecc_point* pvt;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    int ret;

    (void)mp_init(&ssk);
    pvt = wc_ecc_new_point();
    wc_InitEccsiKey(&genKey, NULL, INVALID_DEVID);
    (void)wc_MakeEccsiKey(&genKey, &gRng);

    /* RSK Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_MakeEccsiPair(&genKey, &gRng, WC_HASH_TYPE_SHA256, id,
                                   sizeof(id), &ssk, pvt);
            if (ret != 0) {
                printf("wc_MakeEccsiPair failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ECCSI", 256, desc[12], 0, count, start, 0);

    wc_FreeEccsiKey(&genKey);
    wc_ecc_del_point(pvt);
    mp_free(&ssk);
}
#endif

#ifdef WOLFCRYPT_ECCSI_CLIENT
void bench_eccsiValidate(void)
{
    EccsiKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    mp_int ssk;
    ecc_point* pvt;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    int valid;
    int ret;

    (void)mp_init(&ssk);
    pvt = wc_ecc_new_point();
    wc_InitEccsiKey(&genKey, NULL, INVALID_DEVID);
    (void)wc_MakeEccsiKey(&genKey, &gRng);
    (void)wc_MakeEccsiPair(&genKey, &gRng, WC_HASH_TYPE_SHA256, id, sizeof(id),
                           &ssk, pvt);

    /* Validation of RSK */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_ValidateEccsiPair(&genKey, WC_HASH_TYPE_SHA256, id,
                                       sizeof(id), &ssk, pvt, &valid);
            if (ret != 0 || !valid) {
                printf("wc_ValidateEccsiPair failed: %d (valid=%d))\n", ret,
                       valid);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ECCSI", 256, desc[11], 0, count, start, 0);

    wc_FreeEccsiKey(&genKey);
    wc_ecc_del_point(pvt);
    mp_free(&ssk);
}

void bench_eccsi(void)
{
    EccsiKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    mp_int ssk;
    ecc_point* pvt;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    byte msg[] = { 0x01, 0x23, 0x34, 0x45 };
    byte hash[WC_SHA256_DIGEST_SIZE];
    byte hashSz = (byte)sizeof(hash);
    byte sig[257];
    word32 sigSz = sizeof(sig);
    int ret;
    int verified;

    (void)mp_init(&ssk);
    pvt = wc_ecc_new_point();
    (void)wc_InitEccsiKey(&genKey, NULL, INVALID_DEVID);
    (void)wc_MakeEccsiKey(&genKey, &gRng);
    (void)wc_MakeEccsiPair(&genKey, &gRng, WC_HASH_TYPE_SHA256, id, sizeof(id),
                           &ssk, pvt);
    (void)wc_HashEccsiId(&genKey, WC_HASH_TYPE_SHA256, id, sizeof(id), pvt,
                         hash, &hashSz);
    (void)wc_SetEccsiHash(&genKey, hash, hashSz);
    (void)wc_SetEccsiPair(&genKey, &ssk, pvt);

    /* Encapsulate */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_SignEccsiHash(&genKey, &gRng, WC_HASH_TYPE_SHA256, msg,
                                   sizeof(msg), sig, &sigSz);
            if (ret != 0) {
                printf("wc_SignEccsiHash failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ECCSI", 256, desc[4], 0, count, start, 0);

    /* Derive */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_VerifyEccsiHash(&genKey, WC_HASH_TYPE_SHA256, msg,
                                     sizeof(msg), sig, sigSz, &verified);
            if (ret != 0 || !verified) {
                printf("wc_VerifyEccsiHash failed: %d (verified: %d)\n", ret,
                       verified);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("ECCSI", 256, desc[5], 0, count, start, 0);

    wc_FreeEccsiKey(&genKey);
    wc_ecc_del_point(pvt);
}
#endif /* WOLFCRYPT_ECCSI_CLIENT */
#endif /* WOLFCRYPT_HAVE_ECCSI */

#ifdef WOLFCRYPT_HAVE_SAKKE
#ifdef WOLFCRYPT_SAKKE_KMS
void bench_sakkeKeyGen(void)
{
    SakkeKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    int    ret;

    /* Key Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            wc_InitSakkeKey_ex(&genKey, 128, ECC_SAKKE_1, NULL, INVALID_DEVID);
            ret = wc_MakeSakkeKey(&genKey, &gRng);
            if (ret != 0) {
                printf("wc_MakeSakkeKey failed: %d\n", ret);
                break;
            }
            wc_FreeSakkeKey(&genKey);
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("SAKKE", 1024, desc[2], 0, count, start, 0);
}

void bench_sakkeRskGen(void)
{
    SakkeKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    ecc_point* rsk;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    int ret;

    rsk = wc_ecc_new_point();
    wc_InitSakkeKey_ex(&genKey, 128, ECC_SAKKE_1, NULL, INVALID_DEVID);
    (void)wc_MakeSakkeKey(&genKey, &gRng);

    /* RSK Gen */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_MakeSakkeRsk(&genKey, id, sizeof(id), rsk);
            if (ret != 0) {
                printf("wc_MakeSakkeRsk failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("SAKKE", 1024, desc[8], 0, count, start, 0);

    wc_FreeSakkeKey(&genKey);
    wc_ecc_del_point(rsk);
}
#endif

#ifdef WOLFCRYPT_SAKKE_CLIENT
void bench_sakkeValidate(void)
{
    SakkeKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    ecc_point* rsk;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    int valid;
    int ret;

    rsk = wc_ecc_new_point();
    (void)wc_InitSakkeKey_ex(&genKey, 128, ECC_SAKKE_1, NULL, INVALID_DEVID);
    (void)wc_MakeSakkeKey(&genKey, &gRng);
    (void)wc_MakeSakkeRsk(&genKey, id, sizeof(id), rsk);
    (void)wc_ValidateSakkeRsk(&genKey, id, sizeof(id), rsk, &valid);

    /* Validation of RSK */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_ValidateSakkeRsk(&genKey, id, sizeof(id), rsk, &valid);
            if (ret != 0 || !valid) {
                printf("wc_ValidateSakkeRsk failed: %d (valid=%d))\n", ret,
                       valid);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish("SAKKE", 1024, desc[11], 0, count, start, 0);

    wc_FreeSakkeKey(&genKey);
    wc_ecc_del_point(rsk);
}

void bench_sakke(void)
{
    SakkeKey genKey;
    double start;
    int    i, count;
    const char**desc = bench_desc_words[lng_index];
    ecc_point* rsk;
    byte id[] = { 0x01, 0x23, 0x34, 0x45 };
    byte ssv[] = { 0x01, 0x23, 0x34, 0x45 };
    byte derSSV[sizeof(ssv)];
    byte auth[257];
    word16 authSz = sizeof(auth);
    int ret = 0;
    byte* table = NULL;
    word32 len = 0;
    byte* iTable = NULL;
    word32 iTableLen = 0;

    rsk = wc_ecc_new_point();
    (void)wc_InitSakkeKey_ex(&genKey, 128, ECC_SAKKE_1, NULL, INVALID_DEVID);
    (void)wc_MakeSakkeKey(&genKey, &gRng);
    (void)wc_MakeSakkeRsk(&genKey, id, sizeof(id), rsk);
    (void)wc_SetSakkeRsk(&genKey, rsk, NULL, 0);
    (void)wc_SetSakkeIdentity(&genKey, id, sizeof(id));

    /* Encapsulate */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_MakeSakkeEncapsulatedSSV(&genKey, WC_HASH_TYPE_SHA256, ssv,
                                              sizeof(ssv), auth, &authSz);
            if (ret != 0) {
                printf("wc_MakeSakkeEncapsulatedSSV failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[9], "-1",
                               0, count, start, 0);

    /* Derive */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            XMEMCPY(derSSV, ssv, sizeof(ssv));
            ret = wc_DeriveSakkeSSV(&genKey, WC_HASH_TYPE_SHA256, derSSV,
                                    sizeof(derSSV), auth, authSz);
            if (ret != 0) {
                printf("wc_DeriveSakkeSSV failed: %d\n", ret);
                break;
            }
        }
        if (ret != 0) break;
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[10], "-1",
                               0, count, start, 0);

    /* Calculate Point I and generate table. */
    (void)wc_MakeSakkePointI(&genKey, id, sizeof(id));
    iTableLen = 0;
    (void)wc_GenerateSakkePointITable(&genKey, NULL, &iTableLen);
    if (iTableLen != 0) {
        iTable = (byte*)XMALLOC(iTableLen, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        (void)wc_GenerateSakkePointITable(&genKey, iTable, &iTableLen);
    }

    /* Encapsulate with Point I table */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            ret = wc_MakeSakkeEncapsulatedSSV(&genKey, WC_HASH_TYPE_SHA256, ssv,
                sizeof(ssv), auth, &authSz);
            if (ret != 0) {
                printf("wc_MakeSakkeEncapsulatedSSV failed: %d\n", ret);
                break;
            }
        }
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[9], "-2", 0,
                               count, start, 0);

    (void)wc_SetSakkeRsk(&genKey, rsk, table, len);

    /* Derive with Point I table */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            XMEMCPY(derSSV, ssv, sizeof(ssv));
            ret = wc_DeriveSakkeSSV(&genKey, WC_HASH_TYPE_SHA256, derSSV,
                                    sizeof(derSSV), auth, authSz);
            if (ret != 0) {
                printf("wc_DeriveSakkeSSV failed: %d\n", ret);
                break;
            }
        }
        if (ret != 0) break;
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[10], "-2", 0,
                               count, start, 0);

    len = 0;
    (void)wc_GenerateSakkeRskTable(&genKey, rsk, NULL, &len);
    if (len > 0) {
        table = (byte*)XMALLOC(len, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        (void)wc_GenerateSakkeRskTable(&genKey, rsk, table, &len);
    }
    (void)wc_SetSakkeRsk(&genKey, rsk, table, len);

    /* Derive with Point I table and RSK table */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            XMEMCPY(derSSV, ssv, sizeof(ssv));
            ret = wc_DeriveSakkeSSV(&genKey, WC_HASH_TYPE_SHA256, derSSV,
                                    sizeof(derSSV), auth, authSz);
            if (ret != 0) {
                printf("wc_DeriveSakkeSSV failed: %d\n", ret);
                break;
            }
        }
        if (ret != 0) break;
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[10], "-3",
                               0, count, start, 0);

    wc_ClearSakkePointITable(&genKey);
    /* Derive with RSK table */
    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
            XMEMCPY(derSSV, ssv, sizeof(ssv));
            ret = wc_DeriveSakkeSSV(&genKey, WC_HASH_TYPE_SHA256, derSSV,
                                    sizeof(derSSV), auth, authSz);
            if (ret != 0) {
                printf("wc_DeriveSakkeSSV failed: %d\n", ret);
                break;
            }
        }
        if (ret != 0) break;
        count += i;
    } while (bench_stats_check(start));
    bench_stats_asym_finish_ex("SAKKE", 1024, desc[10], "-4", 0,
                               count, start, 0);

    wc_FreeSakkeKey(&genKey);
    wc_ecc_del_point(rsk);
}
#endif /* WOLFCRYPT_SAKKE_CLIENT */
#endif /* WOLFCRYPT_HAVE_SAKKE */

#if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
static void bench_pqcKemInit(word32 alg, byte **priv_key, byte **pub_key,
                   const char **wolf_name, OQS_KEM **kem)
{
    int i;
    const char *pqc_name = NULL;

    *pub_key = NULL;
    *priv_key = NULL;

    for (i=0; bench_pq_asym_opt[i].str != NULL; i++) {
        if (alg ==  bench_pq_asym_opt[i].val) {
#ifdef HAVE_LIBOQS
            pqc_name = bench_pq_asym_opt[i].pqc_name;
#endif
            *wolf_name = bench_pq_asym_opt[i].str;
            break;
        }
    }

#ifdef HAVE_LIBOQS
    if (pqc_name == NULL) {
        printf("Bad OQS Alg specified\n");
        return;
    }

    *kem = OQS_KEM_new(pqc_name);
    if (*kem == NULL) {
        printf("OQS_KEM_new() failed\n");
        return;
    }

    *pub_key = (byte*)XMALLOC((*kem)->length_public_key, HEAP_HINT,
                              DYNAMIC_TYPE_TMP_BUFFER);

    *priv_key = (byte*)XMALLOC((*kem)->length_secret_key, HEAP_HINT,
                               DYNAMIC_TYPE_TMP_BUFFER);

#endif
#ifdef HAVE_PQM4
    *pub_key = (byte*)XMALLOC(PQM4_PUBLIC_KEY_LENGTH, HEAP_HINT,
                              DYNAMIC_TYPE_TMP_BUFFER);

    *priv_key = (byte*)XMALLOC(PQM4_PRIVATE_KEY_LENGTH, HEAP_HINT,
                               DYNAMIC_TYPE_TMP_BUFFER);
    (void)pqc_name;
#endif
}

void bench_pqcKemKeygen(word32 alg)
{
    const char *wolf_name = NULL;
    OQS_KEM* kem = NULL;
    double start;
    int i, count, ret;
    byte *priv_key;
    byte *pub_key;

    bench_pqcKemInit(alg, &priv_key, &pub_key, &wolf_name, &kem);

    if (wolf_name == NULL || pub_key == NULL ||
#ifdef HAVE_LIBOQS
        kem == NULL ||
#endif
        priv_key == NULL) {
        printf("bench_pqcKemInit() failed\n");
        goto exit;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < genTimes; i++) {
#ifdef HAVE_LIBOQS
            ret = OQS_KEM_keypair(kem, pub_key, priv_key);
            if (ret != OQS_SUCCESS) {
                printf("OQS_KEM_keypair() failed: %d\n", ret);
                goto exit;
            }
#endif
#ifdef HAVE_PQM4
            ret = crypto_kem_keypair(pub_key, priv_key);
            if (ret != 0) {
                printf("crypto_kem_keypair() failed: %d\n", ret);
                goto exit;
            }
#endif
        }
        count += i;
    } while (bench_stats_check(start));

    /* + 1 gets rid of the leading dash (-) */
    bench_stats_pq_asym_finish(wolf_name + 1, 0, count, start, 0);

exit:
    XFREE(priv_key, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(pub_key, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
#ifdef HAVE_LIBOQS
    OQS_KEM_free(kem);
#endif
}

void bench_pqcKemEncapDecap(word32 alg)
{
    const char *wolf_name = NULL;
    OQS_KEM* kem = NULL;
    double start;
    int i, count, ret;
    byte *priv_key;
    byte *pub_key;
    byte *ciphertext = NULL;
    byte *shared_secret = NULL;

    bench_pqcKemInit(alg, &priv_key, &pub_key, &wolf_name, &kem);

    if (wolf_name == NULL || pub_key == NULL ||
#ifdef HAVE_LIBOQS
        kem == NULL ||
#endif
        priv_key == NULL) {
        printf("bench_pqcKemInit() failed\n");
        goto exit;
    }

#ifdef HAVE_LIBOQS
    ret = OQS_KEM_keypair(kem, pub_key, priv_key);
    if (ret != OQS_SUCCESS) {
        printf("OQS_KEM_keypair() failed: %d\n", ret);
        goto exit;
    }

    shared_secret = (byte*)XMALLOC(kem->length_shared_secret, HEAP_HINT,
                                   DYNAMIC_TYPE_TMP_BUFFER);

    ciphertext = (byte*)XMALLOC(kem->length_ciphertext, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER);
#endif
#ifdef HAVE_PQM4
    ret = crypto_kem_keypair(pub_key, priv_key);
    if (ret != 0) {
        printf("crypto_kem_keypair() failed: %d\n", ret);
        goto exit;
    }

    shared_secret = (byte*)XMALLOC(PQM4_SHARED_SECRET_LENGTH, HEAP_HINT,
                                   DYNAMIC_TYPE_TMP_BUFFER);

    ciphertext = (byte*)XMALLOC(PQM4_CIPHERTEXT_LENGTH, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER);
#endif

    if (shared_secret == NULL || ciphertext == NULL) {
        printf("XMALLOC() failed\n");
        goto exit;
    }

#ifdef HAVE_LIBOQS
    if (ret == OQS_SUCCESS)
#endif
#ifdef HAVE_PQM4
    if (ret == 0)
#endif
    {
        bench_stats_start(&count, &start);
        do {
            for (i = 0; i < agreeTimes; i++) {
#ifdef HAVE_LIBOQS
                ret = OQS_KEM_encaps(kem, ciphertext, shared_secret, pub_key);
                if (ret != OQS_SUCCESS) {
                    printf("OQS_KEM_encaps() failed: %d\n", ret);
                    goto exit;
                }

                ret = OQS_KEM_decaps(kem, shared_secret, ciphertext, priv_key);
                if (ret != OQS_SUCCESS) {
                    printf("OQS_KEM_decaps() failed: %d\n", ret);
                    goto exit;
                }
#endif
#ifdef HAVE_PQM4
                ret = crypto_kem_enc(ciphertext, shared_secret, pub_key);
                if (ret != 0){
                    printf("crypto_kem_enc() failed: %d\n", ret);
                    goto exit;
                }

                ret = crypto_kem_dec(shared_secret, ciphertext, priv_key);
                if (ret != 0){
                    printf("crypto_kem_dec() failed: %d\n", ret);
                    goto exit;
                }
#endif
            }
            count += i;
        } while (bench_stats_check(start));

        /* + 1 gets rid of the leading dash (-) */
        bench_stats_pq_asym_finish(wolf_name + 1, 0, count, start, ret);
    }

exit:
    XFREE(ciphertext, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(shared_secret, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(priv_key, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(pub_key, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
#ifdef HAVE_LIBOQS
    OQS_KEM_free(kem);
#endif
}

#ifdef HAVE_FALCON
void bench_falconKeySign(byte level)
{
    int    ret = 0;
    falcon_key key;
    double start;
    int    i, count;
    byte   sig[FALCON_MAX_SIG_SIZE];
    byte   msg[512];
    word32 x = 0;
    const char**desc = bench_desc_words[lng_index];

    ret = wc_falcon_init(&key);
    if (ret != 0) {
        printf("wc_falcon_init failed %d\n", ret);
        return;
    }

    ret = wc_falcon_set_level(&key, level);
    if (ret != 0) {
        printf("wc_falcon_set_level failed %d\n", ret);
    }

    if (ret == 0) {
        if (level == 1) {
            ret = wc_falcon_import_private_key(bench_falcon_level1_key,
                                               sizeof_bench_falcon_level1_key,
                                               NULL, 0, &key);
        }
        else {
            ret = wc_falcon_import_private_key(bench_falcon_level5_key,
                                               sizeof_bench_falcon_level5_key,
                                               NULL, 0, &key);
        }

        if (ret != 0) {
            printf("wc_falcon_import_private_key failed %d\n", ret);
        }
    }

    /* make dummy msg */
    for (i = 0; i < (int)sizeof(msg); i++) {
        msg[i] = (byte)i;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                if (level == 1) {
                    x = FALCON_LEVEL1_SIG_SIZE;
                }
                else {
                    x = FALCON_LEVEL5_SIG_SIZE;
                }

                ret = wc_falcon_sign_msg(msg, sizeof(msg), sig, &x, &key);
                if (ret != 0) {
                    printf("wc_falcon_sign_msg failed\n");
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        bench_stats_asym_finish("FALCON", level, desc[4], 0, count, start, ret);
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                int verify = 0;
                ret = wc_falcon_verify_msg(sig, x, msg, sizeof(msg), &verify,
                                           &key);
                if (ret != 0 || verify != 1) {
                    printf("wc_falcon_verify_msg failed %d, verify %d\n",
                           ret, verify);
                    ret = -1;
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        bench_stats_asym_finish("FALCON", level, desc[5], 0, count, start, ret);
    }

    wc_falcon_free(&key);
}
#endif /* HAVE_FALCON */

#ifdef HAVE_DILITHIUM
void bench_dilithiumKeySign(byte level, byte sym)
{
    int    ret = 0;
    dilithium_key key;
    double start;
    int    i, count;
    byte   sig[DILITHIUM_MAX_SIG_SIZE];
    byte   msg[512];
    word32 x = 0;
    const char**desc = bench_desc_words[lng_index];

    ret = wc_dilithium_init(&key);
    if (ret != 0) {
        printf("wc_dilithium_init failed %d\n", ret);
        return;
    }

    ret = wc_dilithium_set_level_and_sym(&key, level, sym);
    if (ret != 0) {
        printf("wc_dilithium_set_level_and_sym() failed %d\n", ret);
    }

    if (ret == 0) {
        ret = -1;
        if ((level == 2) && (sym == SHAKE_VARIANT)) {
            ret = wc_dilithium_import_private_key(bench_dilithium_level2_key,
                      sizeof_bench_dilithium_level2_key, NULL, 0, &key);
        }
        else if ((level == 3) && (sym == SHAKE_VARIANT)) {
            ret = wc_dilithium_import_private_key(bench_dilithium_level3_key,
                      sizeof_bench_dilithium_level3_key, NULL, 0, &key);
        }
        else if ((level == 5) && (sym == SHAKE_VARIANT)) {
            ret = wc_dilithium_import_private_key(bench_dilithium_level5_key,
                      sizeof_bench_dilithium_level5_key, NULL, 0, &key);
        }
        else if ((level == 2) && (sym == AES_VARIANT)) {
            ret = wc_dilithium_import_private_key(
                      bench_dilithium_aes_level2_key,
                      sizeof_bench_dilithium_level2_key, NULL, 0, &key);
        }
        else if ((level == 3) && (sym == AES_VARIANT)) {
            ret = wc_dilithium_import_private_key(
                      bench_dilithium_aes_level3_key,
                      sizeof_bench_dilithium_level3_key, NULL, 0, &key);
        }
        else if ((level == 5) && (sym == AES_VARIANT)) {
            ret = wc_dilithium_import_private_key(
                      bench_dilithium_aes_level5_key,
                      sizeof_bench_dilithium_level5_key, NULL, 0, &key);
        }

        if (ret != 0) {
            printf("wc_dilithium_import_private_key failed %d\n", ret);
        }
    }

    /* make dummy msg */
    for (i = 0; i < (int)sizeof(msg); i++) {
        msg[i] = (byte)i;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                if (level == 2) {
                    x = DILITHIUM_LEVEL2_SIG_SIZE;
                }
                else if (level == 3) {
                    x = DILITHIUM_LEVEL3_SIG_SIZE;
                }
                else {
                    x = DILITHIUM_LEVEL5_SIG_SIZE;
                }

                ret = wc_dilithium_sign_msg(msg, sizeof(msg), sig, &x, &key);
                if (ret != 0) {
                    printf("wc_dilithium_sign_msg failed\n");
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        if (sym == SHAKE_VARIANT) {
            bench_stats_asym_finish("DILITHIUM", level, desc[4], 0, count,
                                    start, ret);
        }
        else {
            bench_stats_asym_finish("DILITHIUM-AES", level, desc[4], 0, count,
                                    start, ret);
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                int verify = 0;
                ret = wc_dilithium_verify_msg(sig, x, msg, sizeof(msg), &verify,
                                              &key);

                if (ret != 0 || verify != 1) {
                    printf("wc_dilithium_verify_msg failed %d, verify %d\n",
                           ret, verify);
                    ret = -1;
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        if (sym == SHAKE_VARIANT) {
            bench_stats_asym_finish("DILITHIUM", level, desc[5], 0, count,
                                    start, ret);
        }
        else {
            bench_stats_asym_finish("DILITHIUM-AES", level, desc[5], 0, count,
                                    start, ret);
        }
    }

    wc_dilithium_free(&key);
}
#endif /* HAVE_DILITHIUM */

#ifdef HAVE_SPHINCS
void bench_sphincsKeySign(byte level, byte optim)
{
    int    ret = 0;
    sphincs_key key;
    double start;
    int    i, count;
    byte   sig[SPHINCS_MAX_SIG_SIZE];
    byte   msg[512];
    word32 x = 0;
    const char**desc = bench_desc_words[lng_index];

    ret = wc_sphincs_init(&key);
    if (ret != 0) {
        printf("wc_sphincs_init failed %d\n", ret);
        return;
    }

    ret = wc_sphincs_set_level_and_optim(&key, level, optim);
    if (ret != 0) {
        printf("wc_sphincs_set_level_and_optim() failed %d\n", ret);
    }

    if (ret == 0) {
        ret = -1;
        if ((level == 1) && (optim == FAST_VARIANT)) {
            ret = wc_sphincs_import_private_key(bench_sphincs_fast_level1_key,
                      sizeof_bench_sphincs_fast_level1_key, NULL, 0, &key);
        }
        else if ((level == 3) && (optim == FAST_VARIANT)) {
            ret = wc_sphincs_import_private_key(bench_sphincs_fast_level3_key,
                      sizeof_bench_sphincs_fast_level3_key, NULL, 0, &key);
        }
        else if ((level == 5) && (optim == FAST_VARIANT)) {
            ret = wc_sphincs_import_private_key(bench_sphincs_fast_level5_key,
                      sizeof_bench_sphincs_fast_level5_key, NULL, 0, &key);
        }
        else if ((level == 1) && (optim == SMALL_VARIANT)) {
            ret = wc_sphincs_import_private_key(
                      bench_sphincs_small_level1_key,
                      sizeof_bench_sphincs_small_level1_key, NULL, 0, &key);
        }
        else if ((level == 3) && (optim == SMALL_VARIANT)) {
            ret = wc_sphincs_import_private_key(
                      bench_sphincs_small_level3_key,
                      sizeof_bench_sphincs_small_level3_key, NULL, 0, &key);
        }
        else if ((level == 5) && (optim == SMALL_VARIANT)) {
            ret = wc_sphincs_import_private_key(
                      bench_sphincs_small_level5_key,
                      sizeof_bench_sphincs_small_level5_key, NULL, 0, &key);
        }

        if (ret != 0) {
            printf("wc_sphincs_import_private_key failed %d\n", ret);
        }
    }

    /* make dummy msg */
    for (i = 0; i < (int)sizeof(msg); i++) {
        msg[i] = (byte)i;
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                if ((level == 1) && (optim == FAST_VARIANT)) {
                    x = SPHINCS_FAST_LEVEL1_SIG_SIZE;
                }
                else if ((level == 3) && (optim == FAST_VARIANT)) {
                    x = SPHINCS_FAST_LEVEL3_SIG_SIZE;
                }
                else if ((level == 5) && (optim == FAST_VARIANT)) {
                    x = SPHINCS_FAST_LEVEL5_SIG_SIZE;
                }
                else if ((level == 1) && (optim == SMALL_VARIANT)) {
                    x = SPHINCS_SMALL_LEVEL1_SIG_SIZE;
                }
                else if ((level == 3) && (optim == SMALL_VARIANT)) {
                    x = SPHINCS_SMALL_LEVEL3_SIG_SIZE;
                }
                else if ((level == 5) && (optim == SMALL_VARIANT)) {
                    x = SPHINCS_SMALL_LEVEL5_SIG_SIZE;
                }

                ret = wc_sphincs_sign_msg(msg, sizeof(msg), sig, &x, &key);
                if (ret != 0) {
                    printf("wc_sphincs_sign_msg failed\n");
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        if (optim == FAST_VARIANT) {
            bench_stats_asym_finish("SPHINCS-FAST", level, desc[4], 0, count,
                                    start, ret);
        }
        else {
            bench_stats_asym_finish("SPHINCS-SMALL", level, desc[4], 0, count,
                                    start, ret);
        }
    }

    bench_stats_start(&count, &start);
    do {
        for (i = 0; i < agreeTimes; i++) {
            if (ret == 0) {
                int verify = 0;
                ret = wc_sphincs_verify_msg(sig, x, msg, sizeof(msg), &verify,
                                            &key);

                if (ret != 0 || verify != 1) {
                    printf("wc_sphincs_verify_msg failed %d, verify %d\n",
                           ret, verify);
                    ret = -1;
                }
            }
        }
        count += i;
    } while (bench_stats_check(start));

    if (ret == 0) {
        if (optim == FAST_VARIANT) {
            bench_stats_asym_finish("SPHINCS-FAST", level, desc[5], 0, count,
                                    start, ret);
        }
        else {
            bench_stats_asym_finish("SPHINCS-SMALL", level, desc[5], 0, count,
                                    start, ret);
        }
    }

    wc_sphincs_free(&key);
}
#endif /* HAVE_SPHINCS */
#endif /* HAVE_PQC */

#if defined(_WIN32) && !defined(INTIME_RTOS)

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    double current_time(int reset)
    {
        static int init = 0;
        static LARGE_INTEGER freq;

        LARGE_INTEGER count;

        (void)reset;

        if (!init) {
            QueryPerformanceFrequency(&freq);
            init = 1;
        }

        QueryPerformanceCounter(&count);

        return (double)count.QuadPart / freq.QuadPart;
    }

#elif defined MICROCHIP_PIC32
    #if defined(WOLFSSL_MICROCHIP_PIC32MZ)
        #define CLOCK 80000000.0
    #else
        #define CLOCK 40000000.0
    #endif
    extern void WriteCoreTimer(word32 t);
    extern word32 ReadCoreTimer(void);
    double current_time(int reset)
    {
        unsigned int ns;

        if (reset) {
            WriteCoreTimer(0);
        }

        /* get timer in ns */
        ns = ReadCoreTimer();

        /* return seconds as a double */
        return ( ns / CLOCK * 2.0);
    }

#elif defined(WOLFSSL_IAR_ARM_TIME) || defined (WOLFSSL_MDK_ARM) || \
      defined(WOLFSSL_USER_CURRTIME) || defined(WOLFSSL_CURRTIME_REMAP)
    /* declared above at line 239 */
    /* extern   double current_time(int reset); */

#elif defined(FREERTOS)

    #include "task.h"
#if defined(WOLFSSL_ESPIDF)
    /* proto type definition */
    int construct_argv();
    extern char* __argv[22];
#endif
    double current_time(int reset)
    {
        portTickType tickCount;

        (void) reset;

        /* tick count == ms, if configTICK_RATE_HZ is set to 1000 */
        tickCount = xTaskGetTickCount();
        return (double)tickCount / 1000;
    }

#elif defined (WOLFSSL_TIRTOS)

    extern double current_time(int reset);

#elif defined(FREESCALE_MQX)

    double current_time(int reset)
    {
        TIME_STRUCT tv;
        _time_get(&tv);

        return (double)tv.SECONDS + (double)tv.MILLISECONDS / 1000;
    }

#elif defined(FREESCALE_KSDK_BM)

    double current_time(int reset)
    {
        return (double)OSA_TimeGetMsec() / 1000;
    }

#elif defined(WOLFSSL_EMBOS)

    #include "RTOS.h"

    double current_time(int reset)
    {
        double time_now;
        double current_s = OS_GetTime() / 1000.0;
        double current_us = OS_GetTime_us() / 1000000.0;
        time_now = (double)( current_s + current_us);

        (void) reset;

        return time_now;
    }
#elif defined(WOLFSSL_SGX)
    double current_time(int reset);

#elif defined(WOLFSSL_DEOS)
    double current_time(int reset)
    {
        const uint32_t systemTickTimeInHz = 1000000 / systemTickInMicroseconds();
        const volatile uint32_t *systemTickPtr = systemTickPointer();

        (void)reset;

        return (double) *systemTickPtr/systemTickTimeInHz;
    }

#elif defined(MICRIUM)
    double current_time(int reset)
    {

#if (OS_VERSION < 50000)
        CPU_ERR err;
        (void)reset;
        return (double) CPU_TS_Get32()/CPU_TS_TmrFreqGet(&err);
#else
        RTOS_ERR  err;
        double ret = 0;
        OS_TICK tick = OSTimeGet(&err);
        OS_RATE_HZ rate = OSTimeTickRateHzGet(&err);
        (void)reset;

        if (RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE) {
            ret = ((double)tick)/rate;
        }
        return ret;
#endif
    }
#elif defined(WOLFSSL_ZEPHYR)

    #include <time.h>

    double current_time(int reset)
    {
        (void)reset;

     #if defined(CONFIG_ARCH_POSIX)
         k_cpu_idle();
     #endif

        return (double)k_uptime_get() / 1000;
    }

#elif defined(WOLFSSL_NETBURNER)
    #include <predef.h>
    #include <utils.h>
    #include <constants.h>

    double current_time(int reset)
    {
        DWORD ticks = TimeTick; /* ticks since system start */
        (void)reset;

        return (double) ticks/TICKS_PER_SECOND;
    }

#elif defined(THREADX)
    #include "tx_api.h"
    double current_time(int reset)
    {
        (void)reset;
        return (double) tx_time_get() / TX_TIMER_TICKS_PER_SECOND;
    }

#elif defined(WOLFSSL_XILINX)
    #ifdef XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_CORTEXA72_0_TIMESTAMP_CLK_FREQ
    #define COUNTS_PER_SECOND     XPAR_VERSAL_CIPS_0_PSPMC_0_PSV_CORTEXA72_0_TIMESTAMP_CLK_FREQ
    #else
    #define COUNTS_PER_SECOND     XPAR_CPU_CORTEXA53_0_TIMESTAMP_CLK_FREQ
    #endif

    double current_time(int reset)
    {
        double timer;
        uint64_t cntPct = 0;
        asm volatile("mrs %0, CNTPCT_EL0" : "=r" (cntPct));

        /* Convert to milliseconds */
        timer = (double)(cntPct / (COUNTS_PER_SECOND / 1000));
        /* Convert to seconds.millisecond */
        timer /= 1000;
        return timer;
    }

#elif defined(LINUX_RUSAGE_UTIME)

    #include <sys/time.h>
    #include <sys/resource.h>

    static struct rusage base_rusage;
    static struct rusage cur_rusage;

    double current_time(int reset)
    {
        struct rusage rusage;

        (void)reset;

        LIBCALL_CHECK_RET(getrusage(RUSAGE_SELF, &rusage));

        if (reset)
            base_rusage = rusage;
        else
            cur_rusage = rusage;

        /* only consider user time, as system time is host-related overhead
         * outside wolfcrypt.
         */
        return (double)rusage.ru_utime.tv_sec +
            (double)rusage.ru_utime.tv_usec / 1000000.0;
    }

    static void check_for_excessive_stime(const char *desc,
                                          const char *desc_extra)
    {
        double start_utime = (double)base_rusage.ru_utime.tv_sec +
            (double)base_rusage.ru_utime.tv_usec / 1000000.0;
        double start_stime = (double)base_rusage.ru_stime.tv_sec +
            (double)base_rusage.ru_stime.tv_usec / 1000000.0;
        double cur_utime = (double)cur_rusage.ru_utime.tv_sec +
            (double)cur_rusage.ru_utime.tv_usec / 1000000.0;
        double cur_stime = (double)cur_rusage.ru_stime.tv_sec +
            (double)cur_rusage.ru_stime.tv_usec / 1000000.0;
        double stime_utime_ratio =
            (cur_stime - start_stime) / (cur_utime - start_utime);
        if (stime_utime_ratio > .1)
            printf("%swarning, "
                   "excessive system time ratio for %s%s (%.3f%%).\n",
                   err_prefix, desc, desc_extra, stime_utime_ratio * 100.0);
    }

#else

    #include <sys/time.h>

    double current_time(int reset)
    {
        struct timeval tv;

        (void)reset;

        LIBCALL_CHECK_RET(gettimeofday(&tv, 0));

        return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
    }

#endif /* _WIN32 */

#if defined(HAVE_GET_CYCLES)

static WC_INLINE word64 get_intel_cycles(void)
{
    unsigned int lo_c, hi_c;
    __asm__ __volatile__ (
        "cpuid\n\t"
        "rdtsc"
            : "=a"(lo_c), "=d"(hi_c)   /* out */
            : "a"(0)                   /* in */
            : "%ebx", "%ecx");         /* clobber */
    return ((word64)lo_c) | (((word64)hi_c) << 32);
}

#endif /* HAVE_GET_CYCLES */

void benchmark_configure(int block_size)
{
    /* must be greater than 0 */
    if (block_size > 0) {
        numBlocks = numBlocks * bench_size / block_size;
        bench_size = (word32)block_size;
    }
}

#ifndef NO_MAIN_DRIVER

#ifndef MAIN_NO_ARGS

#ifndef WOLFSSL_BENCHMARK_ALL
/* Display the algorithm string and keep to 80 characters per line.
 *
 * str   Algorithm string to print.
 * line  Length of line used so far.
 */
static void print_alg(const char* str, int* line)
{
    int optLen;

    optLen = (int)XSTRLEN(str) + 1;
    if (optLen + *line > 80) {
        printf("\n             ");
        *line = 13;
    }
    *line += optLen;
    printf(" %s", str);
}
#endif

/* Display the usage options of the benchmark program. */
static void Usage(void)
{
    int e = 0;
#ifndef WOLFSSL_BENCHMARK_ALL
    int i;
    int line;
#endif

    printf("benchmark\n");
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -? */
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -csv */
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -base10 */
#if defined(HAVE_AESGCM) || defined(HAVE_AESCCM)
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -no_aad */
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -aad_size */
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -all_aad */
#else
    e += 3;
#endif
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -dgst_full */
#ifndef NO_RSA
    printf("%s", bench_Usage_msg1[lng_index][e++]);    /* option -ras_sign */
    #ifdef WOLFSSL_KEY_GEN
    printf("%s", bench_Usage_msg1[lng_index][e]);    /* option -rsa-sz */
    #endif
    e++;
#else
    e += 2;
#endif
#if !defined(NO_DH) && defined(HAVE_FFDHE_2048)
    printf("%s", bench_Usage_msg1[lng_index][e]);    /* option -ffdhe2048 */
#endif
    e++;
#if !defined(NO_DH) && defined(HAVE_FFDHE_3072)
    printf("%s", bench_Usage_msg1[lng_index][e]);    /* option -ffdhe3072 */
#endif
    e++;
#if defined(HAVE_ECC) && !defined(NO_ECC256)
    printf("%s", bench_Usage_msg1[lng_index][e]);    /* option -p256 */
#endif
    e++;
#if defined(HAVE_ECC) && defined(HAVE_ECC384)
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -p384 */
#endif
    e++;
#if defined(HAVE_ECC) && defined(HAVE_ECC521)
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -p521 */
#endif
    e++;
#if defined(HAVE_ECC)
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -ecc-all */
#endif
    e++;
#ifndef WOLFSSL_BENCHMARK_ALL
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -<alg> */
    printf("             ");
    line = 13;
    for (i=0; bench_cipher_opt[i].str != NULL; i++)
        print_alg(bench_cipher_opt[i].str + 1, &line);
    printf("\n             ");
    line = 13;
    for (i=0; bench_digest_opt[i].str != NULL; i++)
        print_alg(bench_digest_opt[i].str + 1, &line);
    printf("\n             ");
    line = 13;
    for (i=0; bench_mac_opt[i].str != NULL; i++)
        print_alg(bench_mac_opt[i].str + 1, &line);
    printf("\n             ");
    line = 13;
    for (i=0; bench_asym_opt[i].str != NULL; i++)
        print_alg(bench_asym_opt[i].str + 1, &line);
    printf("\n             ");
    line = 13;
    for (i=0; bench_other_opt[i].str != NULL; i++)
        print_alg(bench_other_opt[i].str + 1, &line);
    printf("\n             ");
#if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
    line = 13;
    for (i=0; bench_pq_asym_opt[i].str != NULL; i++)
        print_alg(bench_pq_asym_opt[i].str + 1, &line);
#if defined(HAVE_LIBOQS)
    for (i=0; bench_pq_asym_opt2[i].str != NULL; i++)
        print_alg(bench_pq_asym_opt2[i].str + 1, &line);
    printf("\n");
#endif /* HAVE_LIBOQS */
#endif /* HAVE_PQC */
#endif /* !WOLFSSL_BENCHMARK_ALL */
    e++;
    printf("%s", bench_Usage_msg1[lng_index][e++]);   /* option -lng */
    printf("%s", bench_Usage_msg1[lng_index][e++]);   /* option <num> */
    printf("%s", bench_Usage_msg1[lng_index][e++]);   /* option -blocks <num> */
#ifdef WC_ENABLE_BENCH_THREADING
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -threads <num> */
#endif
    e++;
    printf("%s", bench_Usage_msg1[lng_index][e]);   /* option -print */
}

/* Match the command line argument with the string.
 *
 * arg  Command line argument.
 * str  String to check for.
 * return 1 if the command line argument matches the string, 0 otherwise.
 */
static int string_matches(const char* arg, const char* str)
{
    int len = (int)XSTRLEN(str) + 1;
    return XSTRNCMP(arg, str, len) == 0;
}
#endif /* MAIN_NO_ARGS */

#if !defined(NO_MAIN_DRIVER) && !defined(NO_MAIN_FUNCTION)
#if defined(WOLFSSL_ESPIDF) || defined(_WIN32_WCE)
int wolf_benchmark_task(void)
#elif defined(MAIN_NO_ARGS)
int main()
#else
int main(int argc, char** argv)
#endif
{
#ifdef WOLFSSL_ESPIDF
    int argc = construct_argv();
    char** argv = (char**)__argv;
#endif
    return wolfcrypt_benchmark_main(argc, argv);
}
#endif /* NO_MAIN_DRIVER && NO_MAIN_FUNCTION */

int wolfcrypt_benchmark_main(int argc, char** argv)
{
    int ret = 0;
#ifndef MAIN_NO_ARGS
    int optMatched;
#ifndef WOLFSSL_BENCHMARK_ALL
    int i;
#endif
#endif

    benchmark_static_init(1);

    printf("%s------------------------------------------------------------------------------\n",
           info_prefix);
    printf("%s wolfSSL version %s\n", info_prefix, LIBWOLFSSL_VERSION_STRING);
    printf("%s------------------------------------------------------------------------------\n",
           info_prefix);

#ifndef MAIN_NO_ARGS
    while (argc > 1) {
        if (string_matches(argv[1], "-?")) {
            if(--argc>1){
                lng_index = XATOI((++argv)[1]);
                if(lng_index<0||lng_index>1) {
                    lng_index = 0;
                }
            }
            Usage();
            return 0;
        }
        else if (string_matches(argv[1], "-lng")) {
            argc--;
            argv++;
            if(argc>1) {
                lng_index = XATOI(argv[1]);
                if(lng_index<0||lng_index>1){
                    printf("invalid number(%d) is specified. [<num> :0-1]\n",
                        lng_index);
                    lng_index = 0;
                }
            }
        }
        else if (string_matches(argv[1], "-base10"))
            base2 = 0;
#if defined(HAVE_AESGCM) || defined(HAVE_AESCCM)
        else if (string_matches(argv[1], "-no_aad"))
            aes_aad_options = AAD_SIZE_ZERO;
        else if (string_matches(argv[1], "-all_aad"))
            aes_aad_options |= AAD_SIZE_ZERO | AAD_SIZE_DEFAULT;
        else if (string_matches(argv[1], "-aad_size")) {
            argc--;
            argv++;
            if (argc > 1) {
                aes_aad_size = XATOI(argv[1]);
                aes_aad_options |= AAD_SIZE_CUSTOM;
            }
        }
#endif
        else if (string_matches(argv[1], "-dgst_full"))
            digest_stream = 0;
#ifndef NO_RSA
        else if (string_matches(argv[1], "-rsa_sign"))
            rsa_sign_verify = 1;
#endif
#if !defined(NO_DH) && defined(HAVE_FFDHE_2048)
        else if (string_matches(argv[1], "-ffdhe2048"))
            use_ffdhe = 2048;
#endif
#if !defined(NO_DH) && defined(HAVE_FFDHE_3072)
        else if (string_matches(argv[1], "-ffdhe3072"))
            use_ffdhe = 3072;
#endif
#if !defined(NO_DH) && defined(HAVE_FFDHE_4096)
        else if (string_matches(argv[1], "-ffdhe4096"))
            use_ffdhe = 4096;
#endif
#if defined(HAVE_ECC) && !defined(NO_ECC256)
        else if (string_matches(argv[1], "-p256"))
            bench_asym_algs |= BENCH_ECC_P256;
#endif
#if defined(HAVE_ECC) && defined(HAVE_ECC384)
        else if (string_matches(argv[1], "-p384"))
            bench_asym_algs |= BENCH_ECC_P384;
#endif
#if defined(HAVE_ECC) && defined(HAVE_ECC521)
        else if (string_matches(argv[1], "-p521"))
            bench_asym_algs |= BENCH_ECC_P521;
#endif
#ifdef BENCH_ASYM
        else if (string_matches(argv[1], "-csv")) {
            csv_format = 1;
        }
#endif
#ifdef WC_ENABLE_BENCH_THREADING
        else if (string_matches(argv[1], "-threads")) {
            argc--;
            argv++;
            if (argc > 1) {
                g_threadCount = XATOI(argv[1]);
                if (g_threadCount < 1 || lng_index > 128){
                    printf("invalid number(%d) is specified. [<num> :1-128]\n",
                        g_threadCount);
                    g_threadCount = 0;
                }
            }
        }
        else if (string_matches(argv[1], "-print")) {
            gPrintStats = 1;
        }
#endif
        else if (string_matches(argv[1], "-blocks")) {
            argc--;
            argv++;
            if (argc > 1)
                numBlocks = XATOI(argv[1]);
        }
        else if (argv[1][0] == '-') {
            optMatched = 0;
#ifndef WOLFSSL_BENCHMARK_ALL
            /* Check known algorithm choosing command line options. */
            /* Known cipher algorithms */
            for (i=0; !optMatched && bench_cipher_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_cipher_opt[i].str)) {
                    bench_cipher_algs |= bench_cipher_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
            /* Known digest algorithms */
            for (i=0; !optMatched && bench_digest_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_digest_opt[i].str)) {
                    bench_digest_algs |= bench_digest_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
            /* Known MAC algorithms */
            for (i=0; !optMatched && bench_mac_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_mac_opt[i].str)) {
                    bench_mac_algs |= bench_mac_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
            /* Known asymmetric algorithms */
            for (i=0; !optMatched && bench_asym_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_asym_opt[i].str)) {
                    bench_asym_algs |= bench_asym_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
        #if defined(HAVE_PQC) && (defined(HAVE_LIBOQS) || defined(HAVE_PQM4))
            /* Known asymmetric post-quantum algorithms */
            for (i=0; !optMatched && bench_pq_asym_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_pq_asym_opt[i].str)) {
                    bench_pq_asym_algs |= bench_pq_asym_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
        #if defined(HAVE_LIBOQS)
            /* Both bench_pq_asym_opt and bench_pq_asym_opt2 are looking for
             * -pq, so we need to do a special case for -pq since optMatched
             * was set to 1 just above. */
            if (string_matches(argv[1], bench_pq_asym_opt[0].str)) {
                bench_pq_asym_algs2 |= bench_pq_asym_opt2[0].val;
                bench_all = 0;
                optMatched = 1;
            }
            for (i=1; !optMatched && bench_pq_asym_opt2[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_pq_asym_opt2[i].str)) {
                    bench_pq_asym_algs2 |= bench_pq_asym_opt2[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
        #endif /* HAVE_LIBOQS*/
        #endif /* HAVE_PQC */
            /* Other known cryptographic algorithms */
            for (i=0; !optMatched && bench_other_opt[i].str != NULL; i++) {
                if (string_matches(argv[1], bench_other_opt[i].str)) {
                    bench_other_algs |= bench_other_opt[i].val;
                    bench_all = 0;
                    optMatched = 1;
                }
            }
#endif
            if (!optMatched) {
                printf("Option not recognized: %s\n", argv[1]);
                Usage();
                return 1;
            }
        }
        else {
            /* parse for block size */
            benchmark_configure(XATOI(argv[1]));
        }
        argc--;
        argv++;
    }
#endif /* MAIN_NO_ARGS */

#if defined(WC_ENABLE_BENCH_THREADING) && !defined(WOLFSSL_ASYNC_CRYPT)
    if (g_threadCount > 1) {
        ret = benchmark_test_threaded(NULL);
    }
    else
#endif
    {
    #ifdef HAVE_STACK_SIZE
        ret = StackSizeCheck(NULL, benchmark_test);
    #else
        ret = benchmark_test(NULL);
    #endif
    }

    return ret;
}
#endif /* !NO_MAIN_DRIVER */

#else
    #if !defined(NO_MAIN_DRIVER) && !defined(NO_MAIN_FUNCTION)
        int main(void) { return 0; }
    #endif
#endif /* !NO_CRYPT_BENCHMARK */
