/* esp32_mp.c
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
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif
#include <wolfssl/wolfcrypt/settings.h>

#include "wolfssl/wolfcrypt/logging.h"

#if !defined(NO_RSA) || defined(HAVE_ECC)

#if defined(WOLFSSL_ESP32WROOM32_CRYPT_RSA_PRI) && \
   !defined(NO_WOLFSSL_ESP32WROOM32_CRYPT_RSA_PRI)

#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif
#include <wolfssl/wolfcrypt/tfm.h>

static const char* const TAG = "wolfssl_mp";

#define ESP_HW_RSAMAX_BIT           4096
#define ESP_HW_MULTI_RSAMAX_BITS    2048
#define ESP_HW_RSAMIN_BIT           512
#define BYTE_TO_WORDS(s)            (((s+3)>>2))           /* (s+(4-1))/ 4    */
#define BITS_TO_WORDS(s)            (((s+31)>>3)>>2)       /* (s+(32-1))/ 8/ 4*/

#define MP_NG   -1

#define ESP_TIMEOUT(cnt)         (cnt >= ESP_RSA_TIMEOUT_CNT)

/* mutex */
static wolfSSL_Mutex mp_mutex;
static int espmp_CryptHwMutexInit = 0;
/*
* check if the hw is ready before accessing it
*
* When the RSA Accelerator is released from reset, the register RSA_CLEAN_REG
* reads 0 and an initialization process begins. Hardware initializes the four
* memory blocks by setting them to 0. After initialization is complete,
* RSA_CLEAN_REG reads 1. For this reason, software should query RSA_CLEAN_REG
* after being released from reset, and before writing to any RSA Accelerator
* memory blocks or registers for the first time.
*/
static int esp_mp_hw_wait_clean()
{
    word32 timeout = 0;

    while(!ESP_TIMEOUT(++timeout) &&
                DPORT_REG_READ(RSA_CLEAN_REG) != 1) {
        /*  wait. expected delay 1 to 2 uS  */
    }

    if (ESP_TIMEOUT(timeout)) {
        ESP_LOGE(TAG, "waiting hw ready is timed out.");
        return MP_NG;
    }
    return MP_OKAY;
}

/*
* lock hw engine.
* this should be called before using engine.
*
* returns 0 if the hw lock was initialized and mutex lock
*
* See Chapter 24:
*  https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
*
* The RSA Accelerator is activated by enabling the corresponding peripheral
* clock, and by clearing the DPORT_RSA_PD bit in the DPORT_RSA_PD_CTRL_REG
* register. This releases the RSA Accelerator from reset.
*
* When the RSA Accelerator is released from reset, the register RSA_CLEAN_REG
* reads 0 and an initialization process begins. Hardware initializes the four
* memory blocks by setting them to 0. After initialization is complete,
* RSA_CLEAN_REG reads 1. For this reason, software should query RSA_CLEAN_REG
* after being released from reset, and before writing to any RSA Accelerator
* memory blocks or registers for the first time.
*/
static int esp_mp_hw_lock()
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_mp_hw_lock");

    if (espmp_CryptHwMutexInit == 0) {
        ret = esp_CryptHwMutexInit(&mp_mutex);
        if (ret == 0) {
            /* flag esp mp as initialized */
            espmp_CryptHwMutexInit = 1;
        }
        else {
            ESP_LOGE(TAG, "mp mutx initialization failed.");
            return MP_NG;
        }
    }
    else {
        /* esp aes has already been iniitlized */
    }

    /* lock hardware */
    ret = esp_CryptHwMutexLock(&mp_mutex, portMAX_DELAY);

    if (ret != 0) {
        ESP_LOGE(TAG, "mp engine lock failed.");
        return MP_NG;
    }

    /* Enable RSA hardware */
    periph_module_enable(PERIPH_RSA_MODULE);

    /* clear bit to enable hardware operation; (set to disable)
     */
    DPORT_REG_CLR_BIT(DPORT_RSA_PD_CTRL_REG, DPORT_RSA_PD);

    /* remionder: wait until RSA_CLEAN_REG reads 1
     *  see esp_mp_hw_wait_clean()
     */

    ESP_LOGV(TAG, "leave esp_mp_hw_lock");
    return ret;
}
/*
*   Release hw engine
*/
static void esp_mp_hw_unlock( void )
{
    /* set bit to disabled hardware operation; (clear to enable)
     */
    DPORT_REG_SET_BIT(DPORT_RSA_PD_CTRL_REG, DPORT_RSA_PD);

    /* Disable RSA hardware */
    periph_module_disable(PERIPH_RSA_MODULE);

    /* unlock */
    esp_CryptHwMutexUnLock(&mp_mutex);
}

/* this is based on an article by Cetin Kaya Koc, A New Algorithm for Inversion*/
/* mod p^k, June 28 2017.                                                     */
static int esp_calc_Mdash(mp_int *M, word32 k, mp_digit* md)
{
    int i;
    int xi;
    int b0 = 1;
    int bi;
    word32  N = 0;
    word32  x;

    N = M->dp[0];
    bi = b0;
    x  = 0;

    for (i = 0; i < k; i++) {
        xi = bi % 2;
        if (xi < 0) {
            xi *= -1;
        }
        bi = (bi - N * xi) / 2;
        x |= (xi << i);
    }
    /* 2's complement */
    *md = ~x + 1;
    return MP_OKAY;
}

/* start hw process */
static void process_start(word32 reg)
{
     /* clear interrupt */
    DPORT_REG_WRITE(RSA_INTERRUPT_REG, 1);
    /* start process  */
    DPORT_REG_WRITE(reg, 1);
}

/* wait until done */
static int wait_uitil_done(word32 reg)
{
    word32 timeout = 0;
    /* wait until done && not timeout */
    while (!ESP_TIMEOUT(++timeout) &&
                DPORT_REG_READ(reg) != 1) {
        /* wait */
    }

    /* clear interrupt */
    DPORT_REG_WRITE(RSA_INTERRUPT_REG, 1);

    if (ESP_TIMEOUT(timeout)) {
        ESP_LOGE(TAG, "rsa operation is timed out.");
        return MP_NG;
    }

    return MP_OKAY;
}

/* read data from memory into mp_init          */
static void esp_memblock_to_mpint(word32 mem_address,
                                  mp_int* mp,
                                  word32 numwords)
{
    esp_dport_access_read_buffer((uint32_t*)mp->dp, mem_address, numwords);
    mp->used = numwords;
}

/* write mp_init into memory block
 */
static void esp_mpint_to_memblock(word32 mem_address, const mp_int* mp,
                                                      const word32 bits,
                                                      const word32 hwords)
{
    /* init */
    word32 i;
    word32 len = (bits / 8 + ((bits & 7) != 0 ? 1 : 0));

    len = (len + sizeof(word32)-1) / sizeof(word32);

    for (i=0; i < hwords; i++) {
        if (i < len) {
            DPORT_REG_WRITE(mem_address + (i * sizeof(word32)), mp->dp[i]);
        }
        else {
            DPORT_REG_WRITE(mem_address + (i * sizeof(word32)), 0);
        }
    }
}
/* return needed hw words.
 * supported words length
 *  words : {16 ,  32,  48,    64,   80,   96, 112,   128}
 *  bits  : {512,1024, 1536, 2048, 2560, 3072, 3584, 4096}
 */
static word32 words2hwords(word32 wd)
{
    const word32 bit_shift  = 4;

    return (((wd + 0xf) >> bit_shift) << bit_shift);
}

/* count the number of words is needed for bits */
static word32 bits2words(word32 bits)
{
    /* 32 bits */
    const word32 d = sizeof(word32) * WOLFSSL_BIT_SIZE;

    return ((bits + (d - 1)) / d);
}

/* get rinv */
static int esp_get_rinv(mp_int *rinv, mp_int *M, word32 exp)
{
    int ret = 0;

    /* 2^(exp)*/
    if ((ret = mp_2expt(rinv, exp)) != MP_OKAY) {
        ESP_LOGE(TAG, "failed to calculate mp_2expt()");
        return ret;
    }

    /* r_inv = R^2 mod M(=P) */
    if (ret == 0 && (ret = mp_mod(rinv, M, rinv)) != MP_OKAY) {
        ESP_LOGE(TAG, "failed to calculate mp_mod()");
        return ret;
    }

    return ret;
}

/* Z = X * Y;  */
int esp_mp_mul(fp_int* X, fp_int* Y, fp_int* Z)
{
    int ret = 0;
    int neg = (X->sign == Y->sign)? MP_ZPOS : MP_NEG;

    word32 Xs;
    word32 Ys;
    word32 Zs;
    word32 maxWords_sz;
    word32 hwWords_sz;

    /* ask bits number */
    Xs = mp_count_bits(X);
    Ys = mp_count_bits(Y);
    Zs = Xs + Ys;

    /* maximum bits and words for writing to hw */
    maxWords_sz = bits2words(max(Xs, Ys));
    hwWords_sz  = words2hwords(maxWords_sz);

    /* sanity check */
    if((hwWords_sz<<5) > ESP_HW_MULTI_RSAMAX_BITS) {
        ESP_LOGW(TAG, "exceeds max bit length(2048)");
        return -2;
    }

    /*Steps to use hw in the following order:
    * 1. wait until clean hw engine
    * 2. Write(2*N/512bits - 1 + 8) to MULT_MODE_REG
    * 3. Write X and Y to memory blocks
    *    need to write data to each memory block only according to the length
    *    of the number.
    * 4. Write 1  to MUL_START_REG
    * 5. Wait for the first operation to be done. Poll INTERRUPT_REG until it reads 1.
    *    (Or until the INTER interrupt is generated.)
    * 6. Write 1 to RSA_INTERRUPT_REG to clear the interrupt.
    * 7. Read the Z from RSA_Z_MEM
    * 8. Write 1 to RSA_INTERUPT_REG to clear the interrupt.
    * 9. Release the hw engine
    */
    /* lock hw for use */
    if ((ret = esp_mp_hw_lock()) != MP_OKAY) {
        return ret;
    }

    if((ret = esp_mp_hw_wait_clean()) != MP_OKAY) {
        return ret;
    }

    /* step.1  (2*N/512) => N/256. 512 bits => 16 words */
    DPORT_REG_WRITE(RSA_MULT_MODE_REG, (hwWords_sz >> 3) - 1 + 8);
    /* step.2 write X, M and r_inv into memory */
    esp_mpint_to_memblock(RSA_MEM_X_BLOCK_BASE,
                          X,
                          Xs,
                          hwWords_sz);
    /* Y(let-extend)                          */
    esp_mpint_to_memblock(RSA_MEM_Z_BLOCK_BASE + (hwWords_sz<<2),
                          Y,
                          Ys,
                          hwWords_sz);
    /* step.3 start process                           */
    process_start(RSA_MULT_START_REG);

    /* step.4,5 wait until done                       */
    ret = wait_uitil_done(RSA_INTERRUPT_REG);
    if (ret != MP_OKAY) {
        ESP_LOGE(TAG, "wait_uitil_done failed.");
        return ret;
    }
    /* step.6 read the result form MEM_Z              */
    esp_memblock_to_mpint(RSA_MEM_Z_BLOCK_BASE, Z, BITS_TO_WORDS(Zs));

    /* step.7 clear and release hw                    */
    esp_mp_hw_unlock();

    Z->sign = (Z->used > 0) ? neg : MP_ZPOS;

    return ret;
}

/* Z = X * Y (mod M)                                  */
int esp_mp_mulmod(fp_int* X, fp_int* Y, fp_int* M, fp_int* Z)
{
    int ret = 0;
    int negcheck = 0;
    word32 Xs;
    word32 Ys;
    word32 Ms;
    word32 maxWords_sz;
    word32 hwWords_sz;
    word32 zwords;

    mp_int r_inv;
    mp_int tmpZ;
    mp_digit mp;

    /* neg check */
    if (X->sign != Y->sign) {
        /* X*Y becomes negative */
        negcheck = 1;
    }
    /* ask bits number */
    Xs = mp_count_bits(X);
    Ys = mp_count_bits(Y);
    Ms = mp_count_bits(M);

    /* maximum bits and words for writing to hw */
    maxWords_sz = bits2words(max(Xs, max(Ys, Ms)));
    zwords      = bits2words(min(Ms, Xs + Ys));
    hwWords_sz  = words2hwords(maxWords_sz);

    if ((hwWords_sz << 5) > ESP_HW_RSAMAX_BIT) {
        ESP_LOGE(TAG, "exceeds hw maximum bits");
        return -2;
    }
    /* calculate r_inv = R^2 mode M
    *    where: R = b^n, and b = 2^32
    *    accordingly R^2 = 2^(n*32*2)
    */
    ret = mp_init_multi(&tmpZ, &r_inv, NULL, NULL, NULL, NULL);
    if (ret == 0 && (ret = esp_get_rinv(&r_inv, M, (hwWords_sz << 6))) != MP_OKAY) {
        ESP_LOGE(TAG, "calculate r_inv failed.");
        mp_clear(&tmpZ);
        mp_clear(&r_inv);
        return ret;
    }
    /* lock hw for use */
    if ((ret = esp_mp_hw_lock()) != MP_OKAY) {
        mp_clear(&tmpZ);
        mp_clear(&r_inv);
        return ret;
    }
    /* Calculate M' */
    if ((ret = esp_calc_Mdash(M, 32/* bits */, &mp)) != MP_OKAY) {
        ESP_LOGE(TAG, "failed to calculate M dash");
        mp_clear(&tmpZ);
        mp_clear(&r_inv);
        return -1;
    }
    /*Steps to use hw in the following order:
    * 1. wait until clean hw engine
    * 2. Write(N/512bits - 1) to MULT_MODE_REG
    * 3. Write X,M(=G, X, P) to memory blocks
    *    need to write data to each memory block only according to the length
    *    of the number.
    * 4. Write M' to M_PRIME_REG
    * 5. Write 1  to MODEXP_START_REG
    * 6. Wait for the first operation to be done. Poll INTERRUPT_REG until it reads 1.
    *    (Or until the INTER interrupt is generated.)
    * 7. Write 1 to RSA_INTERRUPT_REG to clear the interrupt.
    * 8. Write Y to RSA_X_MEM
    * 9. Write 1 to RSA_MULT_START_REG
    * 10. Wait for the second operation to be completed. Poll INTERRUPT_REG until it reads 1.
    * 11. Read the Z from RSA_Z_MEM
    * 12. Write 1 to RSA_INTERUPT_REG to clear the interrupt.
    * 13. Release the hw engine
    */

    if ((ret = esp_mp_hw_wait_clean()) != MP_OKAY) {
        return ret;
    }
    /* step.1                     512 bits => 16 words */
    DPORT_REG_WRITE(RSA_MULT_MODE_REG, (hwWords_sz >> 4) - 1);

    /* step.2 write X, M and r_inv into memory */
    esp_mpint_to_memblock(RSA_MEM_X_BLOCK_BASE, X, Xs, hwWords_sz);
    esp_mpint_to_memblock(RSA_MEM_M_BLOCK_BASE, M, Ms, hwWords_sz);
    esp_mpint_to_memblock(RSA_MEM_Z_BLOCK_BASE,
                          &r_inv,
                          mp_count_bits(&r_inv),
                          hwWords_sz);

    /* step.3 write M' into memory                   */
    DPORT_REG_WRITE(RSA_M_DASH_REG, mp);
    /* step.4 start process                           */
    process_start(RSA_MULT_START_REG);

    /* step.5,6 wait until done                       */
    wait_uitil_done(RSA_INTERRUPT_REG);
    /* step.7 Y to MEM_X                              */
    esp_mpint_to_memblock(RSA_MEM_X_BLOCK_BASE, Y, Ys, hwWords_sz);

    /* step.8 start process                           */
    process_start(RSA_MULT_START_REG);

    /* step.9,11 wait until done                      */
    wait_uitil_done(RSA_INTERRUPT_REG);

    /* step.12 read the result from MEM_Z             */
    esp_memblock_to_mpint(RSA_MEM_Z_BLOCK_BASE, &tmpZ, zwords);

    /* step.13 clear and release hw                   */
    esp_mp_hw_unlock();

    /* additional steps                               */
    /* this needs for known issue when Z is greater than M */
    if (mp_cmp(&tmpZ, M) == FP_GT) {
        /*  Z -= M  */
        mp_sub(&tmpZ, M, &tmpZ);
    }
    if (negcheck) {
        mp_sub(M, &tmpZ, &tmpZ);
    }

    mp_copy(&tmpZ, Z);

    mp_clear(&tmpZ);
    mp_clear(&r_inv);

    return ret;
}

/* Large Number Modular Exponentiation
 *
 *    Z = X^Y mod M
 *
 * See Chapter 24:
 *  https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
 *
 * The operation is based on Montgomery multiplication. Aside from the
 * arguments X, Y , and M, two additional ones are needed —r and M′
.* These arguments are calculated in advance by software.
.*
.* The RSA Accelerator supports operand lengths of N ∈ {512, 1024, 1536, 2048,
.* 2560, 3072, 3584, 4096} bits. The bit length of arguments Z, X, Y , M,
.* and r can be any one from the N set, but all numbers in a calculation must
.* be of the same length. The bit length of M′ is always 32.
.*
.* Note some DH references may use: Y = (G ^ X) mod P
 */
int esp_mp_exptmod(fp_int* X, fp_int* Y, word32 Ys, fp_int* M, fp_int* Z)
{
    int ret = 0;

    word32 Xs;
    word32 Ms;
    word32 maxWords_sz;
    word32 hwWords_sz;

    mp_int r_inv;
    mp_digit mp;

    /* ask bits number */
    Xs = mp_count_bits(X);
    Ms = mp_count_bits(M);
    /* maximum bits and words for writing to hw */
    maxWords_sz = bits2words(max(Xs, max(Ys, Ms)));
    hwWords_sz  = words2hwords(maxWords_sz);

    if ((hwWords_sz << 5) > ESP_HW_RSAMAX_BIT) {
        ESP_LOGE(TAG, "exceeds hw maximum bits");
        return -2;
    }
    /* calculate r_inv = R^2 mode M
    *    where: R = b^n, and b = 2^32
    *    accordingly R^2 = 2^(n*32*2)
    */
    ret = mp_init(&r_inv);
    if (ret == 0 && (ret = esp_get_rinv(&r_inv, M, (hwWords_sz << 6))) != MP_OKAY) {
        ESP_LOGE(TAG, "calculate r_inv failed.");
        mp_clear(&r_inv);
        return ret;
    }
    /* lock and init the hw                           */
    if ((ret = esp_mp_hw_lock()) != MP_OKAY) {
        mp_clear(&r_inv);
        return ret;
    }
    /* calc M' */
    /* if Pm is odd, uses mp_montgomery_setup() */
    if ((ret = esp_calc_Mdash(M, 32/* bits */, &mp)) != MP_OKAY) {
        ESP_LOGE(TAG, "failed to calculate M dash");
        mp_clear(&r_inv);
        return -1;
    }

    /*Steps to use hw in the following order:
    * 1. Write(N/512bits - 1) to MODEXP_MODE_REG
    * 2. Write X, Y, M and r_inv to memory blocks
    *    need to write data to each memory block only according to the length
    *    of the number.
    * 3. Write M' to M_PRIME_REG
    * 4. Write 1  to MODEXP_START_REG
    * 5. Wait for the operation to be done. Poll INTERRUPT_REG until it reads 1.
    *    (Or until the INTER interrupt is generated.)
    * 6. Read the result Z(=Y) from Z_MEM
    * 7. Write 1 to INTERRUPT_REG to clear the interrupt.
    */
    if ((ret = esp_mp_hw_wait_clean()) != MP_OKAY) {
        return ret;
    }

    /* step.1                                         */
    DPORT_REG_WRITE(RSA_MODEXP_MODE_REG, (hwWords_sz >> 4) - 1);
    /* step.2 write G, X, P, r_inv and M' into memory */
    esp_mpint_to_memblock(RSA_MEM_X_BLOCK_BASE, X, Xs, hwWords_sz);
    esp_mpint_to_memblock(RSA_MEM_Y_BLOCK_BASE, Y, Ys, hwWords_sz);
    esp_mpint_to_memblock(RSA_MEM_M_BLOCK_BASE, M, Ms, hwWords_sz);
    esp_mpint_to_memblock(RSA_MEM_Z_BLOCK_BASE,
                          &r_inv,
                          mp_count_bits(&r_inv),
                          hwWords_sz);
    /* step.3 write M' into memory                    */
    DPORT_REG_WRITE(RSA_M_DASH_REG, mp);
    /* step.4 start process                           */
    process_start(RSA_START_MODEXP_REG);

    /* step.5 wait until done                         */
    wait_uitil_done(RSA_INTERRUPT_REG);
    /* step.6 read a result form memory               */
    esp_memblock_to_mpint(RSA_MEM_Z_BLOCK_BASE, Z, BITS_TO_WORDS(Ms));
    /* step.7 clear and release hw                    */
    esp_mp_hw_unlock();

    mp_clear(&r_inv);

    return ret;
}
#endif /* !NO_RSA || HAVE_ECC */

#endif /* (WOLFSS_ESP32WROOM32_CRYPT) && (NO_WOLFSSL_ESP32WROOM32_CRYPT_RES_PRI)*/
