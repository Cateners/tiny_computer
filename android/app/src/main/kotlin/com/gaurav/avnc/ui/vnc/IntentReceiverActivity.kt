/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */
package com.gaurav.avnc.ui.vnc

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.example.tiny_computer.R
import com.gaurav.avnc.model.db.MainDb
import com.gaurav.avnc.vnc.VncUri
import kotlinx.coroutines.launch

/**
 * Handles "external" intents and launches [VncActivity] with appropriate profiles.
 *
 * Current intent types:
 *  - vnc:// URIs
 *  - App shortcuts
 */
class IntentReceiverActivity : AppCompatActivity() {

    companion object {
        private const val SHORTCUT_PROFILE_ID_KEY = "com.gaurav.avnc.shortcut_profile_id"

        fun createShortcutIntent(context: Context, profileId: Long): Intent {
            check(profileId != 0L) { "Cannot create shortcut with profileId = 0." }
            return Intent(context, IntentReceiverActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                putExtra(SHORTCUT_PROFILE_ID_KEY, profileId)
            }
        }
    }

    private val profileDao by lazy { MainDb.getInstance(this).serverProfileDao }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        handleIntent()
    }

    private fun handleIntent() = lifecycleScope.launch {
        if (intent.data?.scheme == "vnc")
            launchFromVncUri(VncUri(intent.data!!.toString()))
        else if (intent.hasExtra(SHORTCUT_PROFILE_ID_KEY))
            launchFromProfileId(intent.getLongExtra(SHORTCUT_PROFILE_ID_KEY, 0))
        else
            toast("Invalid intent: Server info is missing!")

        finish()
    }

    private suspend fun launchFromVncUri(uri: VncUri) {
        if (uri.connectionName.isNotBlank()) launchFromProfileName(uri.connectionName)
        else launchVncUri(uri)
    }

    private fun launchVncUri(uri: VncUri) {
        if (uri.host.isEmpty()) toast(getString(R.string.msg_invalid_vnc_uri))
        else startVncActivity(this, uri)
    }

    private suspend fun launchFromProfileName(name: String) {
        val profile = profileDao.getByName(name).firstOrNull()
        if (profile == null) toast("No server found with name '$name'")
        else startVncActivity(this, profile)
    }

    private suspend fun launchFromProfileId(profileId: Long) {
        val profile = profileDao.getByID(profileId)
        if (profile == null) toast(getString(R.string.msg_shortcut_server_deleted))
        else startVncActivity(this, profile)
    }

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
}