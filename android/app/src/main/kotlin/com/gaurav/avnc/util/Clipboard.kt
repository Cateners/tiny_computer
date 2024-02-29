/*
 * Copyright (c) 2024  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.util.Log
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Clipboard access is slightly more complex in AVNC, because clip data can be
 * unusually large. It includes stuff coming from server, and app logs.
 * As clipboard access involves binder IPC, it can lead to ANR issues. So we
 * use a background thread for accessing clipboard.
 */

/**
 * Puts given text on the clipboard.
 */
suspend fun setClipboardText(context: Context, text: String): Boolean {
    var success = false
    try {
        getClipboardManager(context)?.let {
            withContext(Dispatchers.IO) {
                it.setPrimaryClip(ClipData.newPlainText(null, text))
                success = true
            }
        }
    } catch (e: CancellationException) {
        throw e
    } catch (t: Throwable) {
        Log.e("ClipboardUtil", "Could not copy text to clipboard.", t)
    }
    return success
}


/**
 * Returns current clipboard text.
 */
suspend fun getClipboardText(context: Context): String? {
    var result: String? = null
    try {
        getClipboardManager(context)?.let {
            withContext(Dispatchers.IO) {
                result = it.primaryClip?.getItemAt(0)?.text?.toString()
            }
        }
    } catch (e: CancellationException) {
        throw e
    } catch (t: Throwable) {
        Log.e("ClipboardUtil", "Could not retrieve text from clipboard.", t)
    }
    return result
}

/**
 * [ClipboardManager] has to be created on a thread where Looper has been initialized.
 */
private suspend fun getClipboardManager(context: Context) = withContext(Dispatchers.Main.immediate) {
    ContextCompat.getSystemService(context, ClipboardManager::class.java)
}