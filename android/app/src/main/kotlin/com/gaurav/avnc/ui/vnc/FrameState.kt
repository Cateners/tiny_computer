/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.graphics.PointF
import android.graphics.RectF
import com.gaurav.avnc.ui.vnc.FrameState.Snapshot
import kotlin.math.max
import kotlin.math.min

/**
 * Represents current 'view' state of the frame.
 *
 * Terminology
 * ===========
 *
 * Framebuffer: This is the buffer holding pixel data. It resides in native memory.
 *
 * Frame: This is the actual content rendered on screen. It can be thought of as
 * 'rendered framebuffer'. Its size changes based on current [scale] and its position
 * is stored in [frameX] & [frameY].
 *
 * Window: Top-level window of the application/activity.
 *
 * Viewport: This is the area of window where frame is rendered. It is denoted by [FrameView].
 *
 * Safe area: Area inside viewport which is safe for interaction with frame, maintained in [safeArea].
 *
 *     Window denotes 'total' area available to our activity, viewport denotes 'visible to user'
 *     area, and safe area denotes 'able to click' area. Most of the time all three will be equal,
 *     but viewport can be smaller than window (e.g. if soft keyboard is visible), and safe area
 *     can be smaller than viewport (e.g. due to display cutout).
 *
 *     +---------------------------+   -   -
 *     |         \Cutout/          |   |   | Viewport
 *     |                           |   |   |   -
 *     |                           |   |   |   | SafeArea
 *     +---------------------------+   |   -   -
 *     |      Soft Keyboard        |   | Window
 *     +---------------------------+   -
 *
 *     Differentiating between these allows us to handle layout changes more easily and cleanly.
 *     We use window size to calculate base scale because we don't want to change scale when
 *     keyboard is shown/hidden. Viewport size is used for rendering the frame, fully immersive.
 *     Safe area is used to coerce frame position so that user can pan every part of frame inside
 *     safe area to interact with it.
 *
 * See [LayoutManager] for more information about these values.
 *
 * State & Coordinates
 * ===================
 *
 * Both frame & viewport are in same coordinate space. Viewport is assumed to be fixed
 * in its place with [0,0] represented by top-left corner. Only frame is scaled/moved.
 * To make sure frame does not move off-screen, after each state change, values are
 * coerced within range by [coerceValues].
 *
 * Rendering is done by [com.gaurav.avnc.ui.vnc.gl.Renderer] based on these values.
 *
 *
 * Scaling
 * =======
 *
 * Scaling controls the 'size' of rendered frame. It involves multiple factors, like window size,
 * framebuffer size, user choice etc. To achieve best experience, we split scaling in two parts.
 * One automatic, and one user controlled.
 *
 * 1. Base Scale [baseScale] :
 * Motivation behind base scale is to start with the most optimal frame size. It is automatically
 * calculated (and updated) using window size & framebuffer size. When orientation of local
 * device is such that longer edge of the window is aligned with longer edge of the frame,
 * base scale will satisfy following constraints (see [calculateBaseScale]):
 *
 * - Frame is completely visible
 * - Frame's aspect ratio is maintained
 * - Maximum window space is utilized
 *
 * 2. Zoom Scale [zoomScale] :
 * This is the user controlled part. It is updated only in response to pinch gestures.
 * To allow different zoom in different orientations, two separate zoom scales are maintained
 * in [zoomScale1] & [zoomScale2]. Based on user preference and current orientation, [zoomScale]
 * will be delegated to one of these.
 *
 * Conceptually, zoom scale works 'on top of' the base scale.
 * Effective scale [scale] is calculated as the product of these two parts, so:

 *      FrameSize = (FramebufferSize * BaseScale) * ZoomScale
 *
 *
 * Thread safety
 * =============
 *
 * Frame state is accessed from two threads: Its properties are updated in UI thread
 * and consumed by the renderer thread. There is a chance that Renderer thread may see
 * half-updated state (e.g. [frameX] is changed inside [pan] but [coerceValues] is not yet called).
 * This half-updated state can cause flickering issues.
 *
 * To avoid this we use [Snapshot]. All updates to frame state are guarded by [lock].
 * Render thread uses [getSnapshot] to retrieve a consistent state to render the frame.
 */
class FrameState(
        private val minZoomScale: Float = 0.5F,
        private val maxZoomScale: Float = 5F,
        private val usePerOrientationZoom: Boolean = false
) {

    //Frame position, relative to top-left corner (0,0)
    var frameX = 0F; private set
    var frameY = 0F; private set

    //VNC framebuffer size
    var fbWidth = 0F; private set
    var fbHeight = 0F; private set

    //Viewport/FrameView size
    var vpWidth = 0F; private set
    var vpHeight = 0F; private set

    //Size of activity window
    var windowWidth = 0F; private set
    var windowHeight = 0F; private set

    var safeArea = RectF(); private set

    //Scaling
    var zoomScale1 = 1F; private set
    var zoomScale2 = 1F; private set
    private val useZoomScale1 get() = (!usePerOrientationZoom || windowHeight > windowWidth)

    var baseScale = 1F; private set
    var zoomScale
        get() = if (useZoomScale1) zoomScale1 else zoomScale2
        private set(value) = if (useZoomScale1) zoomScale1 = value else zoomScale2 = value

    val scale get() = baseScale * zoomScale

    /**
     * Immutable wrapper for frame state
     */
    data class Snapshot(
            val frameX: Float,
            val frameY: Float,
            val fbWidth: Float,
            val fbHeight: Float,
            val vpWidth: Float,
            val vpHeight: Float,
            val scale: Float
    )

    private val lock = Any()
    private inline fun <T> withLock(block: () -> T) = synchronized(lock) { block() }

    fun setFramebufferSize(w: Float, h: Float) = withLock {
        fbWidth = w
        fbHeight = h
        calculateBaseScale()
        coerceValues()
    }

    fun setViewportSize(w: Float, h: Float) = withLock {
        vpWidth = w
        vpHeight = h
        coerceValues()
    }

    fun setWindowSize(w: Float, h: Float) = withLock {
        windowWidth = w
        windowHeight = h
        calculateBaseScale()
        coerceValues()
    }

    fun setSafeArea(rect: RectF) = withLock {
        safeArea = RectF(rect)
        coerceValues()
    }

    /**
     * Adjust zoom scale according to give [scaleFactor].
     *
     * Returns 'how much' scale factor is actually applied (after coercing).
     */
    fun updateZoom(scaleFactor: Float): Float = withLock {
        val oldScale = zoomScale

        zoomScale *= scaleFactor
        coerceValues()

        return zoomScale / oldScale //Applied scale factor
    }

    fun setZoom(zoom1: Float, zoom2: Float) = withLock {
        zoomScale1 = zoom1
        zoomScale2 = zoom2
        coerceValues()
    }

    /**
     * Shift frame by given delta.
     */
    fun pan(deltaX: Float, deltaY: Float) = withLock {
        frameX += deltaX
        frameY += deltaY
        coerceValues()
    }

    /**
     * Move frame to given position.
     */
    fun moveTo(x: Float, y: Float) = withLock {
        frameX = x
        frameY = y
        coerceValues()
    }

    /**
     * Checks if given point is inside of framebuffer.
     */
    fun isValidFbPoint(x: Float, y: Float) = (x >= 0F && x < fbWidth) && (y >= 0F && y < fbHeight)

    /**
     * Converts given viewport point to corresponding framebuffer point.
     * Returns null if given point lies outside of framebuffer.
     */
    fun toFb(vpPoint: PointF): PointF? {
        val fbX = (vpPoint.x - frameX) / scale
        val fbY = (vpPoint.y - frameY) / scale

        if (isValidFbPoint(fbX, fbY))
            return PointF(fbX, fbY)
        else
            return null
    }

    /**
     * Converts given framebuffer point to corresponding point in viewport.
     */
    fun toVP(fbPoint: PointF): PointF {
        return PointF(fbPoint.x * scale + frameX, fbPoint.y * scale + frameY)
    }

    /**
     * Returns immutable & consistent snapshot of frame state.
     */
    fun getSnapshot(): Snapshot = withLock {
        return Snapshot(frameX = frameX, frameY = frameY,
                        fbWidth = fbWidth, fbHeight = fbHeight,
                        vpWidth = vpWidth, vpHeight = vpHeight, scale = scale)
    }

    private fun calculateBaseScale() {
        if (fbHeight == 0F || fbWidth == 0F || windowHeight == 0F)
            return  //Not enough info yet

        val s1 = max(windowWidth, windowHeight) / max(fbWidth, fbHeight)
        val s2 = min(windowWidth, windowHeight) / min(fbWidth, fbHeight)

        baseScale = min(s1, s2)
    }

    /**
     * Makes sure state values are within constraints.
     */
    private fun coerceValues() {
        zoomScale1 = zoomScale1.coerceIn(minZoomScale, maxZoomScale)
        zoomScale2 = zoomScale2.coerceIn(minZoomScale, maxZoomScale)

        if (safeArea.isEmpty || !safeArea.intersect(0f, 0f, vpWidth, vpHeight))
            safeArea.set(0f, 0f, vpWidth, vpHeight)

        frameX = coercePosition(frameX, safeArea.left, safeArea.right, fbWidth)
        frameY = coercePosition(frameY, safeArea.top, safeArea.bottom, fbHeight)
    }

    /**
     * Coerce position value in a direction (horizontal/vertical).
     */
    private fun coercePosition(current: Float, safeMin: Float, safeMax: Float, fb: Float): Float {
        val scaledFb = (fb * scale)
        val diff = (safeMax - safeMin) - scaledFb

        return if (diff >= 0) diff / 2 + safeMin       //Frame will be smaller than safe area, so center it
        else current.coerceIn(diff + safeMin, safeMin) //otherwise, make sure safe area is completely filled.
    }
}