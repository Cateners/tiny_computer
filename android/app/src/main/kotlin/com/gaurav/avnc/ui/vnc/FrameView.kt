/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Build
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.PointerIcon
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import com.gaurav.avnc.ui.vnc.gl.Renderer
import com.gaurav.avnc.viewmodel.VncViewModel
import com.gaurav.avnc.vnc.VncClient

/**
 * This class renders the VNC framebuffer on screen.
 *
 * It derives from [GLSurfaceView], which creates an EGL Display, where we can
 * render the framebuffer using OpenGL ES. See [GLSurfaceView] for more details.
 *
 * Actual rendering is done by [Renderer], which is executed on a dedicated
 * thread by [GLSurfaceView].
 *
 *
 *-   +-------------------+          +--------------------+         +--------------------+
 *-   |   [FrameView]     |          |  [VncViewModel]    |         |   [VncClient]      |
 *-   +--------+----------+          +----------+---------+         +----------+---------+
 *-            |                                |                              |
 *-            |                                |                              |
 *-            | Render Request                 | [FrameState]                 | Framebuffer
 *-            |                                v                              |
 *-            |                     +----------+---------+                    |
 *-            +-------------------> |     [Renderer]     | <------------------+
 *-                                  +--------------------+

 */
class FrameView(context: Context?, attrs: AttributeSet? = null) : GLSurfaceView(context, attrs) {

    private lateinit var touchHandler: TouchHandler
    private lateinit var keyHandler: KeyHandler

    /**
     * Input connection used for intercepting key events
     */
    inner class InputConnection : BaseInputConnection(this, false) {
        override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
            return keyHandler.onCommitText(text) || super.commitText(text, newCursorPosition)
        }

        override fun sendKeyEvent(event: KeyEvent): Boolean {
            return keyHandler.onKeyEvent(event) || super.sendKeyEvent(event)
        }
    }

    /**
     * Should be called from [VncActivity.onCreate].
     */
    fun initialize(activity: VncActivity) {
        val viewModel = activity.viewModel

        touchHandler = activity.touchHandler
        keyHandler = activity.keyHandler

        setEGLContextClientVersion(2)
        setRenderer(Renderer(viewModel))
        renderMode = RENDERMODE_WHEN_DIRTY

        // Hide local cursor if requested and supported
        if (Build.VERSION.SDK_INT >= 24 && viewModel.pref.input.hideLocalCursor)
            pointerIcon = PointerIcon.getSystemIcon(context, PointerIcon.TYPE_NULL)
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.imeOptions = outAttrs.imeOptions or
                EditorInfo.IME_FLAG_NO_EXTRACT_UI or
                EditorInfo.IME_FLAG_NO_FULLSCREEN
        return InputConnection()
    }

    override fun onCheckIsTextEditor(): Boolean {
        return true
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        return touchHandler.onTouchEvent(event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        return touchHandler.onGenericMotionEvent(event)
    }

    override fun onHoverEvent(event: MotionEvent): Boolean {
        return touchHandler.onHoverEvent(event)
    }
}