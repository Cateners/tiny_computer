/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.util.Log

/**
 * Utilities related to Samsung DeX
 */
object SamsungDex {

    /**
     * Returns true, if DeX mode is enabled.
     */
    private fun isInDexMode(context: Context) = runCatching {
        val config = context.resources.configuration
        val configClass = config.javaClass

        val flag = configClass.getField("SEM_DESKTOP_MODE_ENABLED").getInt(configClass)
        val value = configClass.getField("semDesktopModeEnabled").getInt(config)

        value == flag
    }.getOrDefault(false)


    /**
     * Enables/disables meta-key event capturing.
     */
    fun setMetaKeyCapture(activity: Activity, isEnabled: Boolean) {
        if (!isInDexMode(activity))
            return

        runCatching {
            val managerClass = Class.forName("com.samsung.android.view.SemWindowManager")
            val instanceMethod = managerClass.getMethod("getInstance")
            val manager = instanceMethod.invoke(null)

            val requestMethod = managerClass.getDeclaredMethod("requestMetaKeyEvent",
                                                               ComponentName::class.java,
                                                               Boolean::class.java)
            requestMethod.invoke(manager, activity.componentName, isEnabled)
        }.onFailure { Log.d("DeX Support", "Meta key capture error", it) }
    }
}