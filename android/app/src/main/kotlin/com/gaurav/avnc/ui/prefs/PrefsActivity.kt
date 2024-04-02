/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.prefs

import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.annotation.Keep
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import com.example.tiny_computer.R
import com.gaurav.avnc.util.DeviceAuthPrompt
import com.google.android.material.appbar.MaterialToolbar

class PrefsActivity : AppCompatActivity(), PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {

    override fun onCreate(savedInstanceState: Bundle?) {
        DeviceAuthPrompt.applyFingerprintDialogFix(supportFragmentManager)
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.settings_main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        if (savedInstanceState == null) {
            supportFragmentManager
                    .beginTransaction()
                    .replace(R.id.fragment_host, Main())
                    .commit()
        }

        setSupportActionBar(findViewById<MaterialToolbar>(R.id.toolbar))
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressedDispatcher.onBackPressed()
        return true
    }

    /**
     * Starts new fragment corresponding to given [pref].
     */
    override fun onPreferenceStartFragment(caller: PreferenceFragmentCompat, pref: Preference): Boolean {
        val fragment = supportFragmentManager.fragmentFactory.instantiate(classLoader, pref.fragment!!)

        supportFragmentManager.beginTransaction()
                .replace(R.id.fragment_host, fragment)
                .addToBackStack(null)
                .commit()

        return true
    }


    /**************************************************************************
     * Preference Fragments
     **************************************************************************/

    abstract class PrefFragment(private val prefResource: Int) : PreferenceFragmentCompat() {

        override fun onResume() {
            super.onResume()
            activity?.title = preferenceScreen.title
        }

        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            setPreferencesFromResource(prefResource, rootKey)
        }
    }

    @Keep class Main : PrefFragment(R.xml.pref_main)
    @Keep class Appearance : PrefFragment(R.xml.pref_appearance)

    @Keep class Viewer : PrefFragment(R.xml.pref_viewer) {
        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)

            // If system does not support PiP, disable its preference
            val hasPiPSupport = Build.VERSION.SDK_INT >= 26 &&
                                requireActivity().packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)

            if (!hasPiPSupport) {
                findPreference<SwitchPreference>("pip_enabled")!!.apply {
                    isEnabled = false
                    summary = getString(R.string.msg_pip_not_supported)
                }
            }
        }
    }

    @Keep class Input : PrefFragment(R.xml.pref_input) {
        private var invertScrollingUpdater: OnSharedPreferenceChangeListener? = null

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)

            val canChangePtrIcon = Build.VERSION.SDK_INT >= 24

            if (!canChangePtrIcon) {
                findPreference<SwitchPreference>("hide_local_cursor")!!.apply {
                    isEnabled = false
                    summary = getString(R.string.msg_ptr_hiding_not_supported)
                }
            }

            val style = findPreference<ListPreferenceEx>("gesture_style")!!
            val swipe1 = findPreference<ListPreferenceEx>("gesture_swipe1")!!
            val longPressSwipe = findPreference<ListPreferenceEx>("gesture_long_press_swipe")!!

            swipe1.disabledStateSummary = getString(R.string.pref_gesture_action_move_pointer)
            longPressSwipe.helpMessage = getText(R.string.msg_drag_gesture_help)

            swipe1.isEnabled = style.value != "touchpad"
            style.setOnPreferenceChangeListener { _, value -> swipe1.isEnabled = value != "touchpad"; true }

            val styleHelp = "<b>${getString(R.string.pref_gesture_style_touchscreen)}</b><br/>" +
                            getString(R.string.pref_gesture_style_touchscreen_summary) + "<br/><br/>" +
                            "<b>${getString(R.string.pref_gesture_style_touchpad)}</b><br/>" +
                            getString(R.string.pref_gesture_style_touchpad_summary)

            style.helpMessage = HtmlCompat.fromHtml(styleHelp, 0)

            // To reduce clutter & avoid 'UI overload', pref to invert vertical scrolling is
            // only visible when 'Scroll remote content' option is used.
            invertScrollingUpdater = OnSharedPreferenceChangeListener { prefs, _ ->
                findPreference<SwitchPreference>("invert_vertical_scrolling")!!.apply {
                    isVisible = prefs.all.values.contains("remote-scroll")
                }
            }
            invertScrollingUpdater?.onSharedPreferenceChanged(swipe1.sharedPreferences, null) //Initial update
            swipe1.sharedPreferences?.registerOnSharedPreferenceChangeListener(invertScrollingUpdater)

        }
    }

    @Keep class Server : PrefFragment(R.xml.pref_server)
}
