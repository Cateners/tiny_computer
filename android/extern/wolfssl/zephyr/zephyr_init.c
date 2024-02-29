/* zephyr_init.c
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

/** @file
 * @brief wolfSSL initialization
 *
 * Initialize the wolfSSL library.
 */

#include <init.h>

#include "user_settings.h"
#include "wolfssl/ssl.h"

static int _wolfssl_init(const struct device *device)
{
    ARG_UNUSED(device);

    return 0;
}

SYS_INIT(_wolfssl_init, POST_KERNEL, 0);
