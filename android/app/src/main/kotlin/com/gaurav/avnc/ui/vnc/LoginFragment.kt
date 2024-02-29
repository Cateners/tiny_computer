/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.app.Dialog
import android.os.Bundle
import android.util.ArrayMap
import android.widget.ArrayAdapter
import androidx.core.view.isVisible
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import com.example.tiny_computer.R
import com.example.tiny_computer.databinding.FragmentCredentialBinding
import com.gaurav.avnc.model.LoginInfo
import com.gaurav.avnc.model.ServerProfile
import com.gaurav.avnc.viewmodel.VncViewModel
import com.gaurav.avnc.viewmodel.VncViewModel.State.Companion.isConnected
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputLayout

/**
 * Allows user to enter login information.
 *
 * There are different types of login information ([LoginInfo.Type]),
 * but all of them basically boils down to a username/password combo.
 *
 * User can choose to "remember" the information, in which case it will be
 * saved in the profile.
 *
 */
class LoginFragment : DialogFragment() {
    private lateinit var binding: FragmentCredentialBinding
    private val viewModel by activityViewModels<VncViewModel>()
    private val loginType by lazy { viewModel.loginInfoRequest.value!! }
    private val loginInfo by lazy { getLoginInfoFromProfile(viewModel.profile) }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = FragmentCredentialBinding.inflate(layoutInflater, null, false)

        binding.loginInfo = loginInfo
        binding.usernameLayout.isVisible = loginInfo.username.isBlank() && loginType == LoginInfo.Type.VNC_CREDENTIAL
        binding.passwordLayout.isVisible = loginInfo.password.isBlank()
        binding.remember.isVisible = viewModel.profile.ID != 0L && loginType != LoginInfo.Type.SSH_KEY_PASSWORD

        if (loginType == LoginInfo.Type.SSH_KEY_PASSWORD) {
            binding.passwordLayout.setHint(R.string.hint_key_password)
            binding.pkPasswordMsg.isVisible = viewModel.profile.sshPrivateKeyPassword.isNotBlank()
        }

        setupAutoComplete()
        isCancelable = false

        return MaterialAlertDialogBuilder(requireContext())
                .setTitle(getTitle())
                .setView(binding.root)
                .setPositiveButton(android.R.string.ok) { _, _ -> onOk() }
                .setNegativeButton(android.R.string.cancel) { _, _ -> onCancel() }
                .create()
    }

    private fun getTitle() = when (loginType) {
        LoginInfo.Type.VNC_PASSWORD,
        LoginInfo.Type.VNC_CREDENTIAL -> R.string.title_vnc_login
        LoginInfo.Type.SSH_PASSWORD -> R.string.title_ssh_login
        LoginInfo.Type.SSH_KEY_PASSWORD -> R.string.title_unlock_private_key
    }

    private fun getLoginInfoFromProfile(p: ServerProfile): LoginInfo {
        return when (loginType) {
            LoginInfo.Type.VNC_PASSWORD -> LoginInfo(p.name, p.host, "", p.password)
            LoginInfo.Type.VNC_CREDENTIAL -> LoginInfo(p.name, p.host, p.username, p.password)
            LoginInfo.Type.SSH_PASSWORD -> LoginInfo(p.name, p.sshHost, "", p.sshPassword)
            LoginInfo.Type.SSH_KEY_PASSWORD -> LoginInfo(p.name, p.sshHost, "", "" /*p.sshPrivateKeyPassword*/)
        }
    }

    private fun setLoginInfoInProfile(p: ServerProfile, l: LoginInfo) {
        when (loginType) {
            LoginInfo.Type.VNC_PASSWORD -> p.password = l.password
            LoginInfo.Type.VNC_CREDENTIAL -> {
                p.username = l.username
                p.password = l.password
            }
            LoginInfo.Type.SSH_PASSWORD -> p.sshPassword = l.password
            LoginInfo.Type.SSH_KEY_PASSWORD -> p.sshPrivateKeyPassword = "" /* key password is not saved anymore */
        }
    }

    private fun onOk() {
        loginInfo.password = getRealPassword(loginInfo.password)
        viewModel.loginInfoRequest.offerResponse(loginInfo)
        if (binding.remember.isChecked || binding.pkPasswordMsg.isVisible /* to forget saved password */)
            saveLoginInfo(loginInfo)
    }

    private fun onCancel() {
        viewModel.loginInfoRequest.cancelRequest()
        requireActivity().finish()
    }

    /**
     * If user has asked to remember credentials, we need to save them
     * to database. But we don't want to save them immediately because
     * user might have mistyped them. So, we wait until successful
     * connection before saving them.
     */
    private fun saveLoginInfo(loginInfo: LoginInfo) {
        // Use activity as owner because this fragment will likely be destroyed before connecting
        viewModel.state.observe(requireActivity(), object : Observer<VncViewModel.State> {
            override fun onChanged(value: VncViewModel.State) {
                if (value.isConnected) {
                    setLoginInfoInProfile(viewModel.profile, loginInfo)
                    viewModel.saveProfile()
                    viewModel.state.removeObserver(this)
                }
            }
        })
    }

    /**
     * Hooks completion adapters
     *
     * This feature might not be that useful to end-users, but it saves a lot of time
     * during development because I have to frequently install/uninstall app, test
     * different servers running on different addresses/ports.
     */
    private fun setupAutoComplete() {

        viewModel.savedProfiles.observe(this) { profiles ->
            val logins = profiles.map { getLoginInfoFromProfile(it) }
            val usernames = logins.map { it.username }.filter { it.isNotEmpty() }.distinct()
            val passwords = preparePasswordSuggestions(logins)

            if (usernames.isNotEmpty()) {
                val usernameAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_list_item_1, usernames)
                binding.username.setAdapter(usernameAdapter)
                binding.usernameLayout.endIconMode = TextInputLayout.END_ICON_DROPDOWN_MENU
            }

            if (passwords.isNotEmpty()) {
                val passwordAdapter = ArrayAdapter(requireContext(), android.R.layout.simple_list_item_1, passwords)
                binding.password.setAdapter(passwordAdapter)
                binding.passwordLayout.endIconMode = TextInputLayout.END_ICON_DROPDOWN_MENU
            }
        }
    }

    /**
     * Instead of showing plaintext passwords, we show server name & host in suggestion
     * list. When user taps OK, we convert the suggestion back to real password.
     */
    private val passwordMap = ArrayMap<String, String>()

    private fun preparePasswordSuggestions(list: List<LoginInfo>): List<String> {
        list.filter { it.password.isNotEmpty() }
                .map { Pair("from: ${it.name} [${it.host}]", it.password) }
                .distinct()
                .toMap(passwordMap)
                .removeAll(passwordMap.values) //Guard against (very unlikely) clash with real password

        return passwordMap.keys.toList()
    }

    private fun getRealPassword(typedPassword: String) = passwordMap[typedPassword] ?: typedPassword
}