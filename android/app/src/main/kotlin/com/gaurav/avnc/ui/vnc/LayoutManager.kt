/*
 * Copyright (c) 2022  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.graphics.Rect
import android.graphics.RectF
import android.os.Build.VERSION.SDK_INT
import android.view.RoundedCorner
import android.view.View
import android.view.WindowManager
import androidx.annotation.RequiresApi
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsCompat.Type
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import kotlin.math.max

/**
 * This class is responsible for managing fullscreen layout & insets handling.
 * Layout handling is quite complex because of Android APIs (or lack thereof).
 * It gets way worse here because AVNC shows fullscreen content.
 *
 * Situation is so bad, it's almost funny.
 *
 * Almost every Android version has introduced some new APIs, deprecated others,
 * or downright changed existing behaviour (sometimes just for ONE version).
 * AndroidX compat library has been the only respite. It has at least hidden a
 * bunch of if-else statement from our own code. But all that mess is still there.
 */
class LayoutManager(activity: VncActivity) {
    private val viewModel = activity.viewModel
    private val rootView = activity.binding.root
    private val frameView = activity.binding.frameView
    private val virtualKeys = activity.virtualKeys
    private val window = activity.window
    private val insetController = WindowCompat.getInsetsController(window, window.decorView)


    fun initialize() {
        if (SDK_INT >= 30)
            hookInsetsListener()
        else
            hookSystemUiChangeListener()

        hookGlobalLayoutListener()
    }

    fun onConnectionStateChanged() {
        updateFullscreen()
    }

    fun onWindowFocusChanged(hasFocus: Boolean) {
        if (hasFocus && SDK_INT < 30)
            updateFullscreen()
    }

    @RequiresApi(30)
    private fun hookInsetsListener() {
        ViewCompat.setOnApplyWindowInsetsListener(window.decorView) { v, insets ->
            updateWindowInsets(insets)
            updateNavigationBarVisibility(insets)
            ViewCompat.onApplyWindowInsets(v, insets)
        }
    }

    private fun hookGlobalLayoutListener() {
        rootView.viewTreeObserver.addOnGlobalLayoutListener {
            viewModel.frameState.setWindowSize(rootView.width.toFloat(), rootView.height.toFloat())
            viewModel.frameState.setViewportSize(frameView.width.toFloat(), frameView.height.toFloat())
            virtualKeys.container?.let { updateVirtualKeyInsets(it) }

            if (SDK_INT < 30)
                manuallyGenerateWindowInsets()

            applyInsets()
            viewModel.resizeRemoteDesktop()
        }
    }

    /**
     * System's WindowInsets are basically useless for us on API < 30. Neither are
     * they dispatched in all cases, nor they always include necessary information.
     * So, on these platforms, insets are generated manually.
     *
     * We could apply required padding directly here, but simulating insets means that
     * [applyInsets] has a single implementation for all platforms.
     */
    private fun manuallyGenerateWindowInsets() {
        val decorView = window.decorView
        val visibleFrame = Rect()
        decorView.getWindowVisibleDisplayFrame(visibleFrame)

        // Transform from screen coordinates to window coordinates
        // Both can be different, e.g. when in PiP mode
        val locationOnScreen = intArrayOf(0, 0)
        decorView.getLocationOnScreen(locationOnScreen)
        visibleFrame.offset(-locationOnScreen[0], -locationOnScreen[1])

        // Generate insets (left & top are always 0)
        val right = max(0, decorView.right - visibleFrame.right)
        val bottom = max(0, decorView.bottom - visibleFrame.bottom)
        val insets = WindowInsetsCompat.Builder()
                .setInsets(Type.ime(), Insets.of(0, 0, 0, bottom))
                .setInsets(Type.navigationBars(), Insets.of(0, 0, right, 0))
                .build()

        updateWindowInsets(insets)
    }

    /**
     * Flags to hide system bars are not 'sticky' on API < 30. System will automatically
     * clear them in a lot of situations, e.g. when IME is shown. So we have to keep
     * reminding it that we still want to remain fullscreen.
     * Same reason why [onWindowFocusChanged] exists.
     */
    private fun hookSystemUiChangeListener() {
        @Suppress("DEPRECATION")
        window.decorView.setOnSystemUiVisibilityChangeListener { updateFullscreen() }
    }


    /************************************************************************************
     * Fullscreen
     ************************************************************************************/
    private val fullscreenEnabled = viewModel.pref.viewer.fullscreen
    private val defaultSystemBarBehaviour = insetController.systemBarsBehavior

    private fun updateFullscreen() {
        if (!fullscreenEnabled)
            return

        if (viewModel.client.connected)
            enterFullscreen()
        else
            leaveFullscreen()
    }

    private fun enterFullscreen() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        insetController.hide(Type.systemBars())
        insetController.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        if (SDK_INT < 30) {
            // This flag is required to keep the status bar hidden when IME is visible
            @Suppress("DEPRECATION")
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
        }
    }

    private fun leaveFullscreen() {
        WindowCompat.setDecorFitsSystemWindows(window, true)
        insetController.show(Type.systemBars())
        insetController.systemBarsBehavior = defaultSystemBarBehaviour
    }

    @RequiresApi(30)
    private fun updateNavigationBarVisibility(insets: WindowInsetsCompat) {
        // On API 30, Android doesn't automatically show the navigation bar with keyboard.
        // So to hide the keyboard, user has to first swipe to un-hide the navigation bar
        // and then tap on Back button. To avoid this, we manually show the navigation bar
        // whenever keyboard is visible. Although only API 30 seems to be affected, fix is
        // applied on 30+ APIs to ensure consistency.
        if (insets.isVisible(Type.ime()))
            insetController.show(Type.navigationBars())
        else if (fullscreenEnabled && viewModel.client.connected) {
            insetController.hide(Type.navigationBars())
        }
    }

    /************************************************************************************
     * Insets
     *
     * Insets are Android's way of telling us when our window is obscured by something.
     * To provide optimal experience, insets are divided into two categories:
     *
     * Opaque Insets: These insets don't allow interaction with app's window behind them.
     * IME & nav bar are such insets. These are handled by adding padding to [rootView],
     * effectively resizing [frameView] to visible portion of the screen.
     *
     * Safe Area Insets: These obstructs [frameView] only partially, e.g. display cutout.
     * We want the frame to be rendered behind these cutouts to provide fully immersive
     * experience, but also allow user to pan out of these insets when they need to
     * interact with remote content. These are implemented as 'safe area' in [FrameState].
     *
     * All insets are in window coordinates.
     *
     ************************************************************************************/
    private var windowInsets = WindowInsetsCompat.Builder().build()
    private var virtualKeyInsets = Insets.NONE   // Insets caused by virtual keys

    private fun updateWindowInsets(insetsCompat: WindowInsetsCompat) {
        // Copy constructor of WindowInsetsCompat is partially broken on API below 30.
        // We are manually generating insets on API <30 anyway, so don't need to create a copy.
        windowInsets = if (SDK_INT < 30) insetsCompat else WindowInsetsCompat(insetsCompat)
    }

    private fun updateVirtualKeyInsets(vkRoot: View) {
        if (vkRoot.isVisible) {
            val vkLocation = intArrayOf(0, 0)
            vkRoot.getLocationInWindow(vkLocation)
            val b = max(0, window.decorView.height - vkLocation[1])

            if (virtualKeyInsets.bottom != b)
                virtualKeyInsets = Insets.of(0, 0, 0, b)
        } else {
            virtualKeyInsets = Insets.NONE
        }
    }

    private fun applyInsets() {
        val opaqueInsets = listOf(windowInsets.getInsets(Type.ime()), windowInsets.getInsets(Type.navigationBars()))
        val maxOpaqueInsets = opaqueInsets.fold(Insets.NONE) { a, i -> Insets.max(a, i) }
        applyOpaqueInsets(maxOpaqueInsets)

        //val cutoutInsets = windowInsets.getInsets(Type.displayCutout())
        //val cornerInsets = calculateCornerInsets(windowInsets)
        val safeAreaInsets = listOf(maxOpaqueInsets, /*cutoutInsets, cornerInsets,*/ virtualKeyInsets)
        val maxSafeAreaInsets = safeAreaInsets.fold(Insets.NONE) { a, i -> Insets.max(a, i) }
        applySafeAreaInsets(maxSafeAreaInsets)

        // TODO: Apply insets to drawer
    }

    private fun applyOpaqueInsets(opaqueInsets: Insets) {
        // Guess if IME is closing
        if (!windowInsets.isVisible(Type.ime()) && rootView.paddingBottom != 0)
            virtualKeys.onKeyboardClose()

        val insets = windowInsetsToViewInsets(opaqueInsets, rootView)
        if (rootView.paddingRight != insets.right || rootView.paddingBottom != insets.bottom)
            rootView.updatePadding(0, 0, insets.right, insets.bottom)
    }

    private fun applySafeAreaInsets(safeAreaInsets: Insets) {
        val insets = windowInsetsToViewInsets(safeAreaInsets, frameView)
        val safeArea = Rect(insets.left,
                            insets.top,
                            frameView.width - insets.right,
                            frameView.height - insets.bottom)

        viewModel.setSafeArea(RectF(safeArea))
    }

    /**
     * Returns insets caused by rounded corners.
     *
     * Android does not provide pre-calculated insets for rounded screen corners.
     * Information about corners is available since API 31. We use this information
     * to calculate effective insets.
     */
    private fun calculateCornerInsets(windowInsetsCompat: WindowInsetsCompat): Insets {
        val windowInsets = windowInsetsCompat.toWindowInsets()
        if (SDK_INT < 31 || windowInsets == null)
            return Insets.NONE

        val wWidth = window.decorView.width
        val wHeight = window.decorView.height

        var l = 0
        var t = 0
        var r = 0
        var b = 0

        windowInsets.getRoundedCorner(RoundedCorner.POSITION_TOP_LEFT)?.center?.let {
            t = max(t, it.y)
            l = max(l, it.x)
        }
        windowInsets.getRoundedCorner(RoundedCorner.POSITION_TOP_RIGHT)?.center?.let {
            t = max(t, it.y)
            r = max(r, wWidth - it.x)
        }
        windowInsets.getRoundedCorner(RoundedCorner.POSITION_BOTTOM_LEFT)?.center?.let {
            b = max(b, wHeight - it.y)
            l = max(l, it.x)
        }
        windowInsets.getRoundedCorner(RoundedCorner.POSITION_BOTTOM_RIGHT)?.center?.let {
            b = max(b, wHeight - it.y)
            r = max(r, wWidth - it.x)
        }

        // Corner insets are only applied along longer axis. Applying along both axis is unnecessary.
        if (wWidth > wHeight)
            return Insets.of(l, 0, r, 0)
        else
            return Insets.of(0, t, 0, b)
    }

    /**
     * Transforms given insets [insets] from Window's coordinates to [view]'s coordinates
     */
    private fun windowInsetsToViewInsets(insets: Insets, view: View): Insets {
        val viewLocation = intArrayOf(0, 0)
        view.getLocationInWindow(viewLocation)

        // View frame in window coordinates
        val vl = viewLocation[0]
        val vt = viewLocation[1]
        val vr = vl + view.width
        val vb = vt + view.height

        // Insets applicable to given view
        val l = max(0, insets.left - vl)
        val t = max(0, insets.top - vt)
        val r = max(0, insets.right - (window.decorView.width - vr))
        val b = max(0, insets.bottom - (window.decorView.height - vb))

        return Insets.of(l, t, r, b)
    }
}