/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.net.Uri
import android.os.Build
import com.example.tiny_computer.BuildConfig

/**
 * Utilities to aid in debugging
 */
object Debugging {

    /**
     * Returns logcat output.
     * Should not be called from main thread.
     */
    fun logcat(): String {
        try {
            return ProcessBuilder("logcat", "-d", "*")
                    .redirectErrorStream(true)
                    .start()
                    .inputStream
                    .reader()
                    .readText()
        } catch (t: Throwable) {
            return "Error getting logs: ${t.message}"
        }
    }

    fun clearLogs() {
        try {
            ProcessBuilder("logcat", "-c").start()
        } catch (t: Throwable) {
            //Ignore
        }
    }

    /**
     * Generates a query parameter string which can be used with GitHub issue url.
     * Currently, only `body` parameter is generated
     */
    fun bugReportUrlParams(): String {
        val body = """
            |**Description**
            |
            |
            |
            |
            |**Additional Info**
            |- App Version: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})
            |- Android Version: ${Build.VERSION.RELEASE} (${Build.VERSION.SDK_INT})
        """.trimMargin()

        return "?body=${Uri.encode(body)}"
    }

    /**
     * Wraps given logs into a <details> element.
     * This is useful for GitHub comments.
     */
    fun wrapLogs(title: String, logs: String): String {
        return """
            <details>
            <summary>$title</summary>
            <p>
            
            ```python
            {logs}
            ```
            
            </p>
            </details>
            """.trimIndent().replace("{logs}", logs)
    }
}