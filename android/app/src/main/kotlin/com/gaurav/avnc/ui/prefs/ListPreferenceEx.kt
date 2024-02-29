/*
 * Copyright (c) 2022  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.prefs

import android.content.Context
import android.util.AttributeSet
import android.widget.ImageButton
import androidx.fragment.app.FragmentActivity
import androidx.preference.ListPreference
import androidx.preference.PreferenceViewHolder
import com.example.tiny_computer.R
import com.gaurav.avnc.util.MsgDialog

/**
 * List preference with some extra features.
 */
class ListPreferenceEx(context: Context, attrs: AttributeSet) : ListPreference(context, attrs) {

    /**
     * Summary used when preference is disabled.
     */
    var disabledStateSummary: CharSequence? = null

    override fun getSummary(): CharSequence? {
        if (!isEnabled && disabledStateSummary != null)
            return disabledStateSummary
        return super.getSummary()
    }


    /**
     * Message shown in a dialog, when help button of the preference is clicked.
     * This will only work if [R.layout.help_btn] is used as widget layout.
     */
    var helpMessage: CharSequence? = null

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        (holder.findViewById(R.id.help_btn) as? ImageButton)?.setOnClickListener { showHelp() }
    }

    private fun showHelp() {
        helpMessage?.let { helpMessage ->
            (context as? FragmentActivity)?.let { fragmentActivity ->
                MsgDialog.show(fragmentActivity.supportFragmentManager,
                               fragmentActivity.getString(R.string.desc_help_btn),
                               helpMessage)
            }
        }
    }
}