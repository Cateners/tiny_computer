/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import androidx.lifecycle.LiveData
import androidx.preference.PreferenceManager

/**
 * Utility class for accessing app preferences
 */
class AppPreferences(context: Context) {

    private val prefs = PreferenceManager.getDefaultSharedPreferences(context)

    inner class UI {
        val theme = LivePref("theme", "system")
    }

    inner class Viewer {
        val orientation; get() = prefs.getString("viewer_orientation", "landscape")
        val fullscreen; get() = prefs.getBoolean("fullscreen_display", true)
        val pipEnabled; get() = prefs.getBoolean("pip_enabled", true)
        val drawBehindCutout; get() = prefs.getBoolean("viewer_draw_behind_cutout", false)
        val keepScreenOn; get() = prefs.getBoolean("keep_screen_on", true)
        val toolbarAlignment; get() = prefs.getString("toolbar_alignment", "start")
        val toolbarOpenWithSwipe; get() = prefs.getBoolean("toolbar_open_with_swipe", true)
        val zoomMax; get() = prefs.getInt("zoom_max", 500) / 100F
        val zoomMin; get() = prefs.getInt("zoom_min", 50) / 100F
        val perOrientationZoom; get() = prefs.getBoolean("per_orientation_zoom", true)
        val toolbarShowGestureStyleToggle; get() = prefs.getBoolean("toolbar_show_gesture_style_toggle", true)
    }

    inner class Gesture {
        val style; get() = prefs.getString("gesture_style", "touchpad")!!
        val tap1 = "left-click" //Preference UI was removed
        val tap2; get() = prefs.getString("gesture_tap2", "open-keyboard")!!
        val doubleTap; get() = prefs.getString("gesture_double_tap", "double-click")!!
        val longPress; get() = prefs.getString("gesture_long_press", "right-click")!!
        val swipe1; get() = prefs.getString("gesture_swipe1", "pan")!!
        val swipe2; get() = prefs.getString("gesture_swipe2", "remote-scroll")!!
        val doubleTapSwipe; get() = prefs.getString("gesture_double_tap_swipe", "remote-drag")!!
        val longPressSwipe; get() = prefs.getString("gesture_long_press_swipe", "none")!!
        val longPressSwipeEnabled; get() = (longPressSwipe != "none")
        val swipeSensitivity; get() = prefs.getInt("gesture_swipe_sensitivity", 10) / 10f
        val invertVerticalScrolling; get() = prefs.getBoolean("invert_vertical_scrolling", false)
    }

    inner class Input {
        val gesture = Gesture()

        val vkOpenWithKeyboard; get() = prefs.getBoolean("vk_open_with_keyboard", false)
        val vkShowAll; get() = prefs.getBoolean("vk_show_all", true)

        val mousePassthrough; get() = prefs.getBoolean("mouse_passthrough", true)
        val hideLocalCursor; get() = prefs.getBoolean("hide_local_cursor", true)
        val hideRemoteCursor; get() = prefs.getBoolean("hide_remote_cursor", false)
        val mouseBack; get() = prefs.getString("mouse_back", "right-click")!!
        val interceptMouseBack; get() = mouseBack != "default"

        val kmLanguageSwitchToSuper; get() = prefs.getBoolean("km_language_switch_to_super", false)
        val kmRightAltToSuper; get() = prefs.getBoolean("km_right_alt_to_super", false)
        val kmBackToEscape; get() = prefs.getBoolean("km_back_to_escape", false)
    }

    inner class Server {
        val clipboardSync; get() = prefs.getBoolean("clipboard_sync", true)
        val autoReconnect; get() = prefs.getBoolean("auto_reconnect", false)
    }

    /**
     * These are used for one-time features/tips.
     * These are not exposed to user.
     */
    inner class RunInfo {
        var hasConnectedSuccessfully: Boolean
            get() = prefs.getBoolean("run_info_has_connected_successfully", false)
            set(value) = prefs.edit { putBoolean("run_info_has_connected_successfully", value) }

        var hasShownV2WelcomeMsg
            get() = prefs.getBoolean("run_info_has_shown_v2_welcome_msg", false)
            set(value) = prefs.edit { putBoolean("run_info_has_shown_v2_welcome_msg", value) }
    }

    val ui = UI()
    val viewer = Viewer()
    val input = Input()
    val server = Server()
    val runInfo = RunInfo()


    /**
     * For some preference changes we want to provide live feedback to user.
     * This class is used for such scenarios. Based on [LiveData], it notifies
     * the observers whenever the value of given preference is changed.
     *
     * For now, each [LivePref] creates a separate change listener, but if
     * number of [LivePref]s grow, we can optimize by sharing a single listener.
     */
    inner class LivePref<T>(val key: String, private val defValue: T) : LiveData<T>() {
        private val prefChangeListener = SharedPreferences.OnSharedPreferenceChangeListener { _, changedKey ->
            if (key == changedKey)
                updateValue()
        }

        private var initialized = false

        override fun onActive() {
            if (!initialized) {
                initialized = true
                updateValue()
                prefs.registerOnSharedPreferenceChangeListener(prefChangeListener)
            }
        }

        private fun updateValue() {
            @Suppress("UNCHECKED_CAST")
            when (defValue) {
                is Boolean -> value = prefs.getBoolean(key, defValue) as T
                is String -> value = prefs.getString(key, defValue) as T
                is Int -> value = prefs.getInt(key, defValue) as T
                is Long -> value = prefs.getLong(key, defValue) as T
                is Float -> value = prefs.getFloat(key, defValue) as T
            }
        }
    }


    /****************************** Migrations *******************************/
    init {
        if (!prefs.getBoolean("gesture_direct_touch", true)) prefs.edit {
            remove("gesture_direct_touch")
            putString("gesture_style", "touchpad")
        }

        if (!prefs.getBoolean("natural_scrolling", true)) prefs.edit {
            remove("natural_scrolling")
            putBoolean("invert_vertical_scrolling", true)
        }

        prefs.getString("gesture_drag", null)?.let {
            prefs.edit {
                remove("gesture_drag")
                putString("gesture_long_press_swipe", it)
            }
        }
    }
}