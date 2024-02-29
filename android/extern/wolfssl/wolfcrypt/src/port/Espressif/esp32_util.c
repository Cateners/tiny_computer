/* esp32_util.c
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
#include <wolfssl/wolfcrypt/settings.h>

#if defined(WOLFSSL_ESP32WROOM32_CRYPT) && \
  (!defined(NO_AES)        || !defined(NO_SHA) || !defined(NO_SHA256) ||\
   defined(WOLFSSL_SHA384) || defined(WOLFSSL_SHA512))

#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/logging.h>
/*
 * initialize our mutex used to lock hardware access
 *
 * returns:
 *   0 upon success,
 *   BAD_MUTEX_E for null mutex
 *   other value from wc_InitMutex()
 *
 */
int esp_CryptHwMutexInit(wolfSSL_Mutex* mutex) {
    if (mutex == NULL) {
        return BAD_MUTEX_E;
    }

    return wc_InitMutex(mutex);
}

/*
 * call the ESP-IDF mutex lock; xSemaphoreTake
 *
 */
int esp_CryptHwMutexLock(wolfSSL_Mutex* mutex, TickType_t xBlockTime) {
    if (mutex == NULL) {
        WOLFSSL_ERROR_MSG("esp_CryptHwMutexLock called with null mutex");
        return BAD_MUTEX_E;
    }

#ifdef SINGLE_THREADED
    return wc_LockMutex(mutex); /* xSemaphoreTake take with portMAX_DELAY */
#else
    return ((xSemaphoreTake( *mutex, xBlockTime ) == pdTRUE) ? 0 : BAD_MUTEX_E);
#endif
}

/*
 * call the ESP-IDF mutex UNlock; xSemaphoreGive
 *
 */
int esp_CryptHwMutexUnLock(wolfSSL_Mutex* mutex) {
    if (mutex == NULL) {
        WOLFSSL_ERROR_MSG("esp_CryptHwMutexLock called with null mutex");
        return BAD_MUTEX_E;
    }

#ifdef SINGLE_THREADED
    return wc_UnLockMutex(mutex);
#else
    xSemaphoreGive(*mutex);
    return 0;
#endif
}

#endif

#ifdef WOLFSSL_ESP32WROOM32_CRYPT_DEBUG

#include "esp_timer.h"
#include "esp_log.h"

static uint64_t startTime = 0;


void wc_esp32TimerStart()
{
    startTime = esp_timer_get_time();
}

uint64_t  wc_esp32elapsedTime()
{
    /* return elapsed time since wc_esp32AesTimeStart() is called in us */
    return esp_timer_get_time() - startTime;
}

#endif /*WOLFSSL_ESP32WROOM32_CRYPT_DEBUG */
