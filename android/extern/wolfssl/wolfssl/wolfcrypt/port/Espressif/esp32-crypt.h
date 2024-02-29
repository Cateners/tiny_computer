/* esp32-crypt.h
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
#ifndef __ESP32_CRYPT_H__

#define __ESP32_CRYPT_H__

#include "wolfssl/wolfcrypt/settings.h"

#include "esp_idf_version.h"
#include "esp_types.h"
#include "esp_log.h"

#ifdef WOLFSSL_ESP32WROOM32_CRYPT_DEBUG
    #undef LOG_LOCAL_LEVEL
    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#else
    #undef LOG_LOCAL_LEVEL
    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#include <freertos/FreeRTOS.h>
#include "soc/dport_reg.h"
#include "soc/hwcrypto_reg.h"

#if ESP_IDF_VERSION_MAJOR < 5
    #include "soc/cpu.h"
#endif

#if ESP_IDF_VERSION_MAJOR >= 5
    #include "esp_private/periph_ctrl.h"
#else
    #include "driver/periph_ctrl.h"
#endif

#if ESP_IDF_VERSION_MAJOR >= 4
    #include <esp32/rom/ets_sys.h>
#else
    #include <rom/ets_sys.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

int esp_CryptHwMutexInit(wolfSSL_Mutex* mutex);
int esp_CryptHwMutexLock(wolfSSL_Mutex* mutex, TickType_t xBloxkTime);
int esp_CryptHwMutexUnLock(wolfSSL_Mutex* mutex);

#ifndef NO_AES

    #if ESP_IDF_VERSION_MAJOR >= 4
        #include "esp32/rom/aes.h"
    #else
        #include "rom/aes.h"
    #endif

    typedef enum tagES32_AES_PROCESS {
        ESP32_AES_LOCKHW = 1,
        ESP32_AES_UPDATEKEY_ENCRYPT = 2,
        ESP32_AES_UPDATEKEY_DECRYPT = 3,
        ESP32_AES_UNLOCKHW          = 4
    } ESP32_AESPROCESS;

    struct Aes; /* see aes.h */
    int wc_esp32AesCbcEncrypt(struct Aes* aes, byte* out, const byte* in, word32 sz);
    int wc_esp32AesCbcDecrypt(struct Aes* aes, byte* out, const byte* in, word32 sz);
    int wc_esp32AesEncrypt(struct Aes *aes, const byte* in, byte* out);
    int wc_esp32AesDecrypt(struct Aes *aes, const byte* in, byte* out);

#endif

#ifdef WOLFSSL_ESP32WROOM32_CRYPT_DEBUG

    void wc_esp32TimerStart();
    uint64_t  wc_esp32elapsedTime();

#endif /* WOLFSSL_ESP32WROOM32_CRYPT_DEBUG */

#if (!defined(NO_SHA) || !defined(NO_SHA256) || defined(WOLFSSL_SHA384) || \
      defined(WOLFSSL_SHA512)) && \
    !defined(NO_WOLFSSL_ESP32WROOM32_CRYPT_HASH)

    /* RAW hash function APIs are not implemented with esp32 hardware acceleration*/
    #define WOLFSSL_NO_HASH_RAW
    #define SHA_CTX ETS_SHAContext

    #if ESP_IDF_VERSION_MAJOR >= 4
        #include "esp32/rom/sha.h"
    #else
        #include "rom/sha.h"
    #endif

    #undef SHA_CTX

    typedef enum {
        ESP32_SHA_INIT = 0,
        ESP32_SHA_HW = 1,
        ESP32_SHA_SW = 2,
        ESP32_SHA_FAIL_NEED_UNROLL = -1
    } ESP32_MODE;

    typedef struct {
        byte isfirstblock;

        ESP32_MODE mode; /* typically 0 init, 1 HW, 2 SW */

        /* we'll keep track of our own locks.
         * actual enable/disable only occurs for ref_counts[periph] == 0 */
        int lockDepth; /* see ref_counts[periph] in periph_ctrl.c */

        enum SHA_TYPE sha_type;
    } WC_ESP32SHA;

    int esp_sha_try_hw_lock(WC_ESP32SHA* ctx);
    int esp_sha_hw_unlock(WC_ESP32SHA* ctx);

    struct wc_Sha;
    int esp_sha_digest_process(struct wc_Sha* sha, byte blockprocess);
    int esp_sha_process(struct wc_Sha* sha, const byte* data);

    #ifndef NO_SHA256
        struct wc_Sha256;
        int esp_sha256_digest_process(struct wc_Sha256* sha, byte blockprocess);
        int esp_sha256_process(struct wc_Sha256* sha, const byte* data);
        int esp32_Transform_Sha256_demo(struct wc_Sha256* sha256, const byte* data);


    #endif

    /* TODO do we really call esp_sha512_process for WOLFSSL_SHA384 ? */
    #if defined(WOLFSSL_SHA512) || defined(WOLFSSL_SHA384)
        struct wc_Sha512;
        int esp_sha512_process(struct wc_Sha512* sha);
        int esp_sha512_digest_process(struct wc_Sha512* sha, byte blockproc);
    #endif

#endif /* NO_SHA && */


#if !defined(NO_RSA) || defined(HAVE_ECC)

    #if !defined(ESP_RSA_TIMEOUT_CNT)
        #define ESP_RSA_TIMEOUT_CNT     0x249F00
    #endif

    /* operands can be up to 4096 bits long.
     * here we store the bits in wolfSSL fp_int struct.
     * see wolfCrypt tfm.h
     */
    struct fp_int;


    /*
     * The parameter names in the Espressif implementation are arbitrary.
     *
     * The wolfSSL names come from DH: Y=G^x mod M  (see wolfcrypt/tfm.h)
     *
     * G=base, X is the private exponent, Y is the public value w
     **/

    /* Z = (X ^ Y) mod M   : Espressif generic notation    */
    /* Y = (G ^ X) mod P   : wolfSSL DH reference notation */
    int esp_mp_exptmod(struct fp_int* X,    /* G  */
                       struct fp_int* Y,    /* X  */
                              word32 Xbits, /* Ys   typically = fp_count_bits (X) */
                       struct fp_int* M,    /* P  */
                       struct fp_int* Z);   /* Y  */

    /* Z = X * Y */
    int esp_mp_mul(struct fp_int* X,
                   struct fp_int* Y,
                   struct fp_int* Z);


    /* Z = X * Y (mod M) */
    int esp_mp_mulmod(struct fp_int* X,
                      struct fp_int* Y,
                      struct fp_int* M,
                      struct fp_int* Z);

#endif /* NO_RSA || HAVE_ECC*/

/* end c++ wrapper */
#ifdef __cplusplus
}
#endif

#endif  /* __ESP32_CRYPT_H__ */
