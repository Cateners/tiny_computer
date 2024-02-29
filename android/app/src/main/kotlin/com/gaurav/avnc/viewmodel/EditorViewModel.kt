/*
 * Copyright (c) 2024  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.viewmodel

import android.app.Application
import androidx.lifecycle.SavedStateHandle
import com.gaurav.avnc.model.ServerProfile

/**
 * ViewModel for profile editor
 */
class EditorViewModel(app: Application, state: SavedStateHandle, initialProfile: ServerProfile) : BaseViewModel(app) {

    /**
     * Profile being edited
     */
    val profile = state["profile"] ?: initialProfile.copy()

    init {
        state["profile"] = profile
    }

    /**
     * While most fields of [profile] are straightforward to edit, some require
     * more complex handling, and live feedback in UI.
     * For these, we have to use dedicated LiveData fields.
     */
    val useRepeater = state.getLiveData("useRepeater", profile.useRepeater)
    val idOnRepeater = state.getLiveData("idOnRepeater", if (profile.useRepeater) profile.idOnRepeater.toString() else "")
    val useRawEncoding = state.getLiveData("useRawEncoding", profile.useRawEncoding)
    val useSshTunnel = state.getLiveData("useSshTunnel", profile.channelType == ServerProfile.CHANNEL_SSH_TUNNEL)
    val sshUsePassword = state.getLiveData("sshUsePassword", profile.sshAuthType == ServerProfile.SSH_AUTH_PASSWORD)
    val sshUsePrivateKey = state.getLiveData("sshUsePrivateKey", profile.sshAuthType == ServerProfile.SSH_AUTH_KEY)
    val hasSshPrivateKey = state.getLiveData("hasSshPrivateKey", profile.sshPrivateKey.isNotBlank())


    fun prepareProfileForSave(): ServerProfile {
        profile.useRepeater = useRepeater.value ?: false
        profile.idOnRepeater = idOnRepeater.value?.toIntOrNull() ?: 0
        profile.useRawEncoding = useRawEncoding.value ?: false
        profile.channelType = if (useSshTunnel.value == true) ServerProfile.CHANNEL_SSH_TUNNEL else ServerProfile.CHANNEL_TCP
        profile.sshAuthType = if (sshUsePassword.value == true) ServerProfile.SSH_AUTH_PASSWORD else ServerProfile.SSH_AUTH_KEY
        return profile
    }
}