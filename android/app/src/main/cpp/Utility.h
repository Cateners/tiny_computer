/*
 * Copyright (c) 2022  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

#ifndef AVNC_UTILITY_H
#define AVNC_UTILITY_H

#include <stdarg.h>
#include <netdb.h>
#include <errno.h>
#include <android/log.h>


/******************************************************************************
 * Utilities
 *****************************************************************************/

/**
 * Returns a native copy of the given jstring.
 * Caller is responsible for releasing the memory.
 */
static char *getNativeStrCopy(JNIEnv *env, jstring jStr) {
    const char *cStr = env->GetStringUTFChars(jStr, nullptr);
    char *str = strdup(cStr);
    env->ReleaseStringUTFChars(jStr, cStr);
    return str;
}

/******************************************************************************
 * Logging
 *****************************************************************************/
const char *LOG_TAG = "NativeVnc";

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, fmt, args);
    va_end(args);
}

/**
 * Converts given errno value to its description.
 */
static const char *errnoToStr(int e) {

    // LibVNC is patched to report `getaddrinfo` errors as negative 'errno'.
    // See ConnectClientToTcpAddr6WithTimeout() in sockets.c
    if (e < -1000) {
        return gai_strerror((-e) - 1000);
    }

    switch (e) {
        case ENETDOWN:
        case ENETRESET:
        case ENETUNREACH:
        case ECONNABORTED:
        case EHOSTDOWN:
        case EHOSTUNREACH:
        case ETIMEDOUT:
        case ENOMEM:
        case EPROTO:
        case EIO:
            return strerror(e);

        case ECONNREFUSED:
            return "Connection refused! Server may be down or running on different port";

        case ECONNRESET:
            return "Connection closed by server";

        case EACCES:
            return "Authentication failed";

        default:
            // In this case we don't want to display errno description to user
            // because it is more likely to be misleading (e.g. EINTR, EAGAIN).
            // BUT add it to logs in case LibVNC didn't.
            log_error("errnoToStr: (%d %s)", errno, strerror(errno));
            return "";
    }
}

#endif //AVNC_UTILITY_H
