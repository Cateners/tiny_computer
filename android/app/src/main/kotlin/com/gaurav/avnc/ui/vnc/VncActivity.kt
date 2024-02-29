/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.app.Activity
import android.app.PictureInPictureParams
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import android.os.SystemClock
import android.util.Log
import android.util.Rational
import android.view.InputDevice
import android.view.KeyEvent
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.activity.viewModels
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.os.BundleCompat
import androidx.core.view.isVisible
import androidx.databinding.DataBindingUtil
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import com.example.tiny_computer.R
import com.example.tiny_computer.databinding.ActivityVncBinding
import com.gaurav.avnc.model.ServerProfile
import com.gaurav.avnc.util.DeviceAuthPrompt
import com.gaurav.avnc.util.SamsungDex
import com.gaurav.avnc.viewmodel.VncViewModel
import com.gaurav.avnc.viewmodel.VncViewModel.State.Companion.isConnected
import com.gaurav.avnc.viewmodel.VncViewModel.State.Companion.isDisconnected
import com.gaurav.avnc.vnc.VncUri
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.parcelize.Parcelize
import java.lang.ref.WeakReference

/********** [VncActivity] startup helpers *********************************/

private const val PROFILE_KEY = "com.gaurav.avnc.server_profile"
private const val FRAME_STATE_KEY = "com.gaurav.avnc.frame_state"

fun createVncIntent(context: Context, profile: ServerProfile): Intent {
    return Intent(context, VncActivity::class.java).apply {
        putExtra(PROFILE_KEY, profile)
    }
}

fun startVncActivity(source: Activity, profile: ServerProfile) {
    source.startActivity(createVncIntent(source, profile))
}

fun startVncActivity(source: Activity, uri: VncUri) {
    startVncActivity(source, uri.toServerProfile())
}

@Parcelize
private data class SavedFrameState(val frameX: Float, val frameY: Float, val zoom1: Float, val zoom2: Float) : Parcelable

private fun startVncActivity(source: Activity, profile: ServerProfile, frameState: SavedFrameState) {
    source.startActivity(createVncIntent(source, profile).also { it.putExtra(FRAME_STATE_KEY, frameState) })
}
/**************************************************************************/


/**
 * This activity handles the connection to a VNC server.
 */
class VncActivity : AppCompatActivity() {

    lateinit var viewModel: VncViewModel
    lateinit var binding: ActivityVncBinding
    private val dispatcher by lazy { Dispatcher(this) }
    val touchHandler by lazy { TouchHandler(viewModel, dispatcher) }
    val keyHandler by lazy { KeyHandler(dispatcher, viewModel.profile.fLegacyKeySym, viewModel.pref) }
    val virtualKeys by lazy { VirtualKeys(this) }
    private val serverUnlockPrompt = DeviceAuthPrompt(this)
    private val layoutManager by lazy { LayoutManager(this) }
    private val toolbar by lazy { Toolbar(this, dispatcher) }
    private var restoredFromBundle = false
    private var wasConnectedWhenStopped = false
    private var onStartTime = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        DeviceAuthPrompt.applyFingerprintDialogFix(supportFragmentManager)

        super.onCreate(savedInstanceState)
        if (!loadViewModel(savedInstanceState)) {
            finish()
            return
        }

        viewModel.initConnection()

        //Main UI
        binding = DataBindingUtil.setContentView(this, R.layout.activity_vnc)
        binding.viewModel = viewModel
        binding.lifecycleOwner = this
        binding.frameView.initialize(this)
        viewModel.frameViewRef = WeakReference(binding.frameView)
        toolbar.initialize()

        setupLayout()
        setupServerUnlock()

        //Observers
        binding.reconnectBtn.setOnClickListener { retryConnection() }
        viewModel.loginInfoRequest.observe(this) { showLoginDialog() }
        viewModel.sshHostKeyVerifyRequest.observe(this) { showHostKeyDialog() }
        viewModel.state.observe(this) { onClientStateChanged(it) }

        savedInstanceState?.let {
            restoredFromBundle = true
            wasConnectedWhenStopped = it.getBoolean("wasConnectedWhenStopped")
        }
    }

    override fun onStart() {
        super.onStart()
        binding.frameView.onResume()
        viewModel.resumeFrameBufferUpdates()
        onStartTime = SystemClock.uptimeMillis()

        // Refresh framebuffer on activity restart:
        // - It forces read/write on the socket. This allows us to verify the socket, which might have
        //   been closed by the server while app process was frozen in background
        // - It also attempts to fix some unusual cases of old updates requests being lost while AVNC
        //   was frozen by the system
        if (wasConnectedWhenStopped) viewModel.refreshFrameBuffer()
    }

    override fun onStop() {
        super.onStop()
        virtualKeys.releaseMetaKeys()
        binding.frameView.onPause()
        viewModel.pauseFrameBufferUpdates()
        wasConnectedWhenStopped = viewModel.state.value.isConnected
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putParcelable(PROFILE_KEY, viewModel.profile)
        outState.putBoolean("wasConnectedWhenStopped", wasConnectedWhenStopped || viewModel.state.value.isConnected)
    }

    private fun loadViewModel(savedState: Bundle?): Boolean {
        @Suppress("DEPRECATION")
        val profile = savedState?.getParcelable(PROFILE_KEY)
                      ?: intent.getParcelableExtra<ServerProfile?>(PROFILE_KEY)

        if (profile == null) {
            Toast.makeText(this, "Error: Missing Server Info", Toast.LENGTH_LONG).show()
            return false
        }

        val factory = viewModelFactory { initializer { VncViewModel(profile, application) } }
        viewModel = viewModels<VncViewModel> { factory }.value
        return true
    }

    private fun retryConnection(seamless: Boolean = false) {
        //We simply create a new activity to force creation of new ViewModel
        //which effectively restarts the connection.
        if (!isFinishing) {
            val savedFrameState = viewModel.frameState.let {
                SavedFrameState(frameX = it.frameX, frameY = it.frameY, zoom1 = it.zoomScale1, zoom2 = it.zoomScale2)
            }

            startVncActivity(this, viewModel.profile, savedFrameState)

            if (seamless) {
                @Suppress("DEPRECATION")
                overridePendingTransition(0, 0)
            }
            finish()
        }
    }

    private fun setupServerUnlock() {
        serverUnlockPrompt.init(
                onSuccess = { viewModel.serverUnlockRequest.offerResponse(true) },
                onFail = { viewModel.serverUnlockRequest.offerResponse(false) }
        )

        viewModel.serverUnlockRequest.observe(this) {
            if (serverUnlockPrompt.canLaunch())
                serverUnlockPrompt.launch(getString(R.string.title_unlock_dialog))
            else
                viewModel.serverUnlockRequest.offerResponse(true)
        }
    }

    private fun showLoginDialog() {
        LoginFragment().show(supportFragmentManager, "LoginDialog")
    }

    private fun showHostKeyDialog() {
        HostKeyFragment().show(supportFragmentManager, "HostKeyFragment")
    }

    fun showKeyboard() {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager

        binding.frameView.requestFocus()
        imm.showSoftInput(binding.frameView, 0)

        virtualKeys.onKeyboardOpen()
    }

    private fun onClientStateChanged(newState: VncViewModel.State) {
        val isConnected = newState.isConnected

        binding.frameView.isVisible = isConnected
        binding.frameView.keepScreenOn = isConnected && viewModel.pref.viewer.keepScreenOn
        SamsungDex.setMetaKeyCapture(this, isConnected)
        layoutManager.onConnectionStateChanged()
        updateStatusContainerVisibility(isConnected)
        autoReconnect(newState)

        if (isConnected && !restoredFromBundle) {
            incrementUseCount()
            restoreFrameState()
        }
    }

    private fun incrementUseCount() {
        viewModel.profile.useCount += 1
        viewModel.saveProfile()
    }

    private fun updateStatusContainerVisibility(isConnected: Boolean) {
        binding.statusContainer.isVisible = true
        binding.statusContainer
                .animate()
                .alpha(if (isConnected) 0f else 1f)
                .withEndAction { binding.statusContainer.isVisible = !isConnected }
    }

    private fun restoreFrameState() {
        intent.extras?.let { extras ->
            BundleCompat.getParcelable(extras, FRAME_STATE_KEY, SavedFrameState::class.java)?.let {
                viewModel.setZoom(it.zoom1, it.zoom2)
                viewModel.panFrame(it.frameX, it.frameY)
            }
        }
    }

    private var autoReconnecting = false
    private fun autoReconnect(state: VncViewModel.State) {
        if (!state.isDisconnected)
            return

        // If disconnected when coming back from background, try to reconnect immediately
        if (wasConnectedWhenStopped && (SystemClock.uptimeMillis() - onStartTime) in 0..2000) {
            Log.d(javaClass.simpleName, "Disconnected while in background, reconnecting ...")
            retryConnection(true)
        }

        if (autoReconnecting || !viewModel.pref.server.autoReconnect)
            return

        autoReconnecting = true
        lifecycleScope.launch {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                val timeout = 5 //seconds, must be >1
                repeat(timeout) {
                    binding.autoReconnectProgress.setProgressCompat((100 * it) / (timeout - 1), true)
                    delay(1000)
                    if (it >= (timeout - 1))
                        retryConnection()
                }
            }
        }
    }

    /************************************************************************************
     * Layout handling.
     ************************************************************************************/
    private fun setupLayout() {
        setupOrientation()
        layoutManager.initialize()

        if (Build.VERSION.SDK_INT >= 28 && viewModel.pref.viewer.drawBehindCutout) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
    }

    private fun setupOrientation() {
        val choice = viewModel.profile.screenOrientation.let {
            if (it != "auto") it else viewModel.pref.viewer.orientation
        }

        requestedOrientation = when (choice) {
            "portrait" -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            "landscape" -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            else -> ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        layoutManager.onWindowFocusChanged(hasFocus)
        if (hasFocus) viewModel.sendClipboardText()
    }


    /************************************************************************************
     * Picture-in-Picture support
     ************************************************************************************/

    override fun onUserLeaveHint() {
        super.onUserLeaveHint()
        enterPiPMode()
    }

    @RequiresApi(26)
    override fun onPictureInPictureModeChanged(inPiP: Boolean, newConfig: Configuration) {
        super.onPictureInPictureModeChanged(inPiP, newConfig)
        if (inPiP) {
            toolbar.close()
            viewModel.resetZoom()
            virtualKeys.hide()
        }
    }

    private fun enterPiPMode() {
        val canEnter = viewModel.pref.viewer.pipEnabled && viewModel.client.connected

        if (canEnter && Build.VERSION.SDK_INT >= 26) {

            var w = viewModel.frameState.fbWidth
            var h = viewModel.frameState.fbHeight
            if (w <= 0 || h <= 0)
                return

            // Android require aspect ratio to be less than 2.39
            w = w.coerceIn(1f, 2.3f * h)
            h = h.coerceIn(1f, 2.3f * w)

            val aspectRatio = Rational(w.toInt(), h.toInt())
            val param = PictureInPictureParams.Builder().setAspectRatio(aspectRatio).build()

            try {
                enterPictureInPictureMode(param)
            } catch (e: IllegalStateException) {
                Log.w(javaClass.simpleName, "Cannot enter PiP mode", e)
            }
        }
    }

    /************************************************************************************
     * Input
     ************************************************************************************/

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        return keyHandler.onKeyEvent(event) || workarounds(event) || super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        return keyHandler.onKeyEvent(event) || workarounds(event) || super.onKeyUp(keyCode, event)
    }

    override fun onKeyMultiple(keyCode: Int, repeatCount: Int, event: KeyEvent): Boolean {
        return keyHandler.onKeyEvent(event) || super.onKeyMultiple(keyCode, repeatCount, event)
    }

    private fun workarounds(keyEvent: KeyEvent): Boolean {

        //It seems that some device manufacturers are hell-bent on making developers'
        //life miserable. In their infinite wisdom, they decided that Android apps don't
        //need Mouse right-click events. It is hardcoded to act as back-press, without
        //giving apps a chance to handle it. For better or worse, they set the 'source'
        //for such key events to Mouse, enabling the following workarounds.
        if (keyEvent.keyCode == KeyEvent.KEYCODE_BACK &&
            InputDevice.getDevice(keyEvent.deviceId)?.supportsSource(InputDevice.SOURCE_MOUSE) == true &&
            viewModel.pref.input.interceptMouseBack) {
            if (keyEvent.action == KeyEvent.ACTION_DOWN)
                touchHandler.onMouseBack()
            return true
        }
        return false
    }
}