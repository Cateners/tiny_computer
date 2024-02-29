/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.vnc

enum class PointerButton(val bitMask: Int) {
    None(0),
    Left(1),
    Middle(2),
    Right(4),
    WheelUp(8),
    WheelDown(16),
    WheelLeft(32),
    WheelRight(64)
}