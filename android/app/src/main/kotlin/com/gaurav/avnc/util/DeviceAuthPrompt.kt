/*
 * Copyright (c) 2022  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.util

import android.util.Log
import androidx.activity.viewModels
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.biometric.FingerprintDialogFragment
import androidx.biometric.auth.AuthPromptCallback
import androidx.biometric.auth.startClass2BiometricOrCredentialAuthentication
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.FragmentFactory
import androidx.fragment.app.FragmentManager
import androidx.lifecycle.ViewModel

/**
 * Wrapper around AndroidX Biometrics library.
 *
 * - Allow checking for auth availability
 * - Provide simplified Kotlin-style callback setup
 * - Provide consistent handling for activity restarts
 * - Apply workarounds for library bugs
 *
 * Usage:
 * 1. Call [init] to setup callbacks
 * 2. Call [launch] to start auth session
 */
class DeviceAuthPrompt(private val activity: FragmentActivity) {

    class PromptViewModel : ViewModel() {
        var isPromptShown = false
        var promptTitle = ""
    }

    private val viewModel by activity.viewModels<PromptViewModel>()
    private var onAuthSuccess: (() -> Unit)? = null
    private var onAuthFail: ((String) -> Unit)? = null


    /**
     * Setup auth callbacks.
     * Should be called from onCreate() of the host activity/fragment.
     * If an auth session is active, it will be updated with given callbacks.
     */
    fun init(onSuccess: () -> Unit, onFail: (String) -> Unit) {
        onAuthSuccess = onSuccess
        onAuthFail = onFail

        if (viewModel.isPromptShown)
            launch(viewModel.promptTitle)
    }

    /**
     * Whether user can be authenticated using Biometric or Device credentials (e.g. PIN, Password)
     */
    fun canLaunch(): Boolean {
        val types = BiometricManager.Authenticators.BIOMETRIC_WEAK or BiometricManager.Authenticators.DEVICE_CREDENTIAL
        return BiometricManager.from(activity).canAuthenticate(types) == BiometricManager.BIOMETRIC_SUCCESS
    }

    /**
     * Launch auth prompt.
     */
    fun launch(title: String) {
        check(onAuthSuccess != null)
        check(onAuthFail != null)

        activity.startClass2BiometricOrCredentialAuthentication(
                title = title,
                confirmationRequired = false,
                callback = PromptCallback()
        )

        viewModel.isPromptShown = true
        viewModel.promptTitle = title
    }

    private fun onAuthFinished() {
        viewModel.isPromptShown = false
    }

    private inner class PromptCallback : AuthPromptCallback() {
        override fun onAuthenticationSucceeded(activity: FragmentActivity?, result: BiometricPrompt.AuthenticationResult) {
            onAuthSuccess?.invoke()
            onAuthFinished()
        }

        override fun onAuthenticationError(activity: FragmentActivity?, errorCode: Int, errString: CharSequence) {
            Log.e(javaClass.simpleName, "Authentication error: $errString [$errorCode] ")
            onAuthFail?.invoke(errString.toString())
            onAuthFinished()
        }
    }


    companion object {
        /**
         * The constructor of [FingerprintDialogFragment] is currently marked private.
         * When fragment manager tries to re-instantiate it after activity restart,
         * it will fail and crash the app. So we install a custom [FragmentFactoryWrapper]
         * which instantiates [FingerprintDialogFragment] via reflection.
         *
         * Issue: https://issuetracker.google.com/issues/181805603
         *
         * TODO: Remove this after issue is fixed in library.
         */
        fun applyFingerprintDialogFix(fm: FragmentManager) {
            fm.fragmentFactory = FragmentFactoryWrapper(fm.fragmentFactory)
        }

        class FragmentFactoryWrapper(private val realFactory: FragmentFactory) : FragmentFactory() {
            private val fpClassName = FingerprintDialogFragment::class.java.name

            override fun instantiate(classLoader: ClassLoader, className: String): Fragment {
                if (className == fpClassName) {
                    return FingerprintDialogFragment::class.java.getDeclaredConstructor().let {
                        it.isAccessible = true
                        it.newInstance()
                    }
                }
                return realFactory.instantiate(classLoader, className)
            }
        }
    }
}