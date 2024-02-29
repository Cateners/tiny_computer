/*
 * Copyright (c) 2021  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.annotation.SuppressLint
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import androidx.databinding.BindingAdapter
import com.example.tiny_computer.databinding.VirtualKeysBinding

/**
 * Virtual keys allow the user to input keys which are not normally found on
 * keyboards but can be useful for controlling remote server.
 *
 * This class manages the inflation & visibility of virtual keys.
 */
class VirtualKeys(activity: VncActivity) {

    private val pref = activity.viewModel.pref.input
    private val keyHandler = activity.keyHandler
    private val stub = activity.binding.virtualKeysStub
    private var openedWithKb = false

    val container: View? get() = stub.root

    fun show() {
        init()
        container?.visibility = View.VISIBLE
    }

    fun hide() {
        container?.visibility = View.GONE
        openedWithKb = false //Reset flag
    }

    fun onKeyboardOpen() {
        if (pref.vkOpenWithKeyboard && container?.visibility != View.VISIBLE) {
            show()
            openedWithKb = true
        }
    }

    fun onKeyboardClose() {
        if (openedWithKb) {
            hide()
            openedWithKb = false
        }
    }

    fun releaseMetaKeys() {
        val binding = stub.binding as? VirtualKeysBinding
        binding?.apply {
            superBtn.isChecked = false
            shiftBtn.isChecked = false
            ctrlBtn.isChecked = false
            altBtn.isChecked = false
        }
    }

    private fun init() {
        if (stub.isInflated)
            return

        stub.viewStub?.inflate()
        val binding = stub.binding as VirtualKeysBinding
        binding.h = keyHandler
        binding.showAll = pref.vkShowAll
        binding.hideBtn.setOnClickListener { hide() }
    }
}

/**
 * When a View is touched, we schedule a callback to to simulate a click.
 * As long as finger stays on the view, we keep repeating this callback.
 *
 * Another option here is to send VNC KeyEvent(down) on [MotionEvent.ACTION_DOWN]
 * and then send VNC KeyEvent(up) on [MotionEvent.ACTION_UP].
 */
@BindingAdapter("isRepeatable")
fun repeatableKeyBinding(keyView: View, repeatable: Boolean) {
    if (!repeatable)
        return

    keyView.setOnTouchListener(object : View.OnTouchListener {
        private var doRepeat = false

        private fun repeat(v: View) {
            if (doRepeat) {
                v.performClick()
                v.postDelayed({ repeat(v) }, ViewConfiguration.getKeyRepeatDelay().toLong())
            }
        }

        @SuppressLint("ClickableViewAccessibility")
        override fun onTouch(v: View, event: MotionEvent): Boolean {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    doRepeat = true
                    v.postDelayed({ repeat(v) }, ViewConfiguration.getKeyRepeatTimeout().toLong())
                }

                MotionEvent.ACTION_POINTER_DOWN,
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_CANCEL -> {
                    doRepeat = false
                }
            }
            return false
        }
    })
}
