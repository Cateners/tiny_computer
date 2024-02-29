/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.ui.vnc

import android.os.Build
import android.view.KeyCharacterMap
import android.view.KeyEvent
import com.gaurav.avnc.util.AppPreferences
import com.gaurav.avnc.vnc.XKeySym
import com.gaurav.avnc.vnc.XKeySymAndroid
import com.gaurav.avnc.vnc.XKeySymAndroid.updateKeyMap
import com.gaurav.avnc.vnc.XKeySymUnicode
import com.gaurav.avnc.vnc.XTKeyCode

/**
 * Handler for key events
 *
 * Key handling in RFB protocol works on 'key symbols' instead of key-codes/scan-codes
 * which makes it dependent on keyboard layout. VNC servers implement various heuristics
 * to compensate for this & maximize portability. Our implementation is derived after
 * testing with some popular servers. It might not handle all the edge cases.
 *
 * There is an extension to RFB protocol (ExtendedKeyEvent) implemented by some servers.
 * It includes support for sending XT keycodes along with key symbol. This extension
 * greatly reduces the key handling complexity. Unfortunately, as soft keyboards are
 * more common on Android, most [KeyEvent]s don't provide raw scan codes.
 *
 * Basically, job of this class is to convert the received [KeyEvent] into a 'KeySym'.
 * That KeySym will be sent to the server.
 *
 *-      [KeyEvent]     +----------------+    KeySym     +----------------+
 *-   ----------------> |  [KeyHandler]  | ------------> |  [Dispatcher]  |
 *-                     +----------------+               +----------------+
 *
 * This class emits (conceptually) three types of key symbols:
 *
 * 1. X KeySym         - Individual symbols defined by X Windows System
 * 2. Unicode KeySym   - Unicode code points encoded as X KeySym
 * 3. Legacy X KeySym  - Old KeySyms which are now superseded by their Unicode KeySym equivalents
 *
 *
 * To decide which one to emit, we look at following things:
 *
 * a. Key code of [KeyEvent]               (may not be available, e.g. in case of [KeyEvent.ACTION_MULTIPLE])
 * b. Unicode character of [KeyEvent]      (may not be available, e.g. in case of [KeyEvent.KEYCODE_F1])
 * c. Current [cfLegacyKeysym]
 *
 *
 *-                                 [KeyEvent]
 *-                                     |
 *-                                     v
 *-                       +----------------------------+
 *-                       | Is Unicode Char Available? |
 *-                       +-------------+--------------+
 *-                                     |
 *-                           Yes       |      No
 *-                       +-------------+--------------+
 *-                       |                            |
 *-             +---------v----------+         +-------v-------+
 *-             |  Use Unicode Char  |         |  Use Key Code |
 *-             +---------+----------+         +-------+-------+
 *-                       |                            |
 *-             +---------v----------+                 |
 *-             |   In compat mode?  |                 |
 *-             +---------+----------+                 |
 *-                       |                            |
 *-                Yes    |    No                      |
 *-             +---------+----------+                 |
 *-             |                    |                 |
 *-             v                    v                 v
 *-     (Legacy X KeySym)    (Unicode KeySym)      (X KeySym)
 *
 * See [handleKeyEvent] as a starting point.
 *
 *
 * Reference:
 * [X Windows System Protocol](https://www.x.org/releases/X11R7.7/doc/xproto/x11protocol.html#keysym_encoding)
 *
 */
class KeyHandler(private val dispatcher: Dispatcher, private val cfLegacyKeysym: Boolean, prefs: AppPreferences) {

    /**
     * Pre-KeyEvent hook.
     * This is NOT triggered for all characters.
     */
    fun onCommitText(text: CharSequence?): Boolean {
        return handleCCedilla(text)
    }

    /**
     * Shortcut to send both up & down events. Useful for Virtual Keys.
     */
    fun onKey(keyCode: Int) {
        onKeyEvent(keyCode, true)
        onKeyEvent(keyCode, false)
    }

    fun onKeyEvent(keyCode: Int, isDown: Boolean) {
        val action = if (isDown) KeyEvent.ACTION_DOWN else KeyEvent.ACTION_UP
        onKeyEvent(KeyEvent(action, keyCode))
    }

    fun onKeyEvent(event: KeyEvent): Boolean {
        if (shouldIgnoreEvent(event))
            return false

        return handleKeyEvent(preProcessEvent(event))
    }


    /**
     * This will parse the [event] and call [emitForKeyEvent] appropriately.
     */
    private fun handleKeyEvent(event: KeyEvent): Boolean {

        //Deprecated action types are still received for non-ASCII characters
        @Suppress("DEPRECATION")
        when (event.action) {

            KeyEvent.ACTION_DOWN -> return emitForKeyEvent(event.keyCode, getUnicodeChar(event), true, event.scanCode)
            KeyEvent.ACTION_UP -> return emitForKeyEvent(event.keyCode, getUnicodeChar(event), false, event.scanCode)

            KeyEvent.ACTION_MULTIPLE -> {
                if (event.keyCode == KeyEvent.KEYCODE_UNKNOWN) {

                    // Here, only Unicode characters are available.
                    for (uChar in toCodePoints(event.characters)) {
                        emitForKeyEvent(0, uChar, true)
                        emitForKeyEvent(0, uChar, false)
                    }

                } else {

                    // Here, only keyCode is available.
                    // According to Android docs, this case doesn't happen anymore.
                    for (i in 1..event.repeatCount) {
                        emitForKeyEvent(event.keyCode, 0, true)
                        emitForKeyEvent(event.keyCode, 0, false)
                    }
                }
                return true
            }
        }
        return false
    }

    /**
     * Emits an event for given details.
     * It will call [emitForAndroidKeyCode] or [emitForUnicodeChar] depending on arguments.
     */
    private fun emitForKeyEvent(keyCode: Int, unicodeChar: Int, isDown: Boolean, scanCode: Int = 0): Boolean {
        val xtCode = if (scanCode == 0) 0 else XTKeyCode.fromAndroidScancode(scanCode)

        if (handleDiacritics(keyCode, unicodeChar, isDown))
            return true

        // Always emit using keyCode for these because Android returns a unicodeChar
        // for these but most servers don't handle their Unicode characters.
        when (keyCode) {
            KeyEvent.KEYCODE_ENTER,
            KeyEvent.KEYCODE_NUMPAD_ENTER,
            KeyEvent.KEYCODE_SPACE,
            KeyEvent.KEYCODE_TAB ->
                return emitForAndroidKeyCode(keyCode, isDown, xtCode)
        }

        // We prefer to use unicodeChar even when keyCode is available because
        // most servers ignore previously sent SHIFT/CAPS_LOCK keys.
        // As Android takes meta keys into account when calculating unicodeChar,
        // it works well with these servers.

        if (unicodeChar != 0)
            return emitForUnicodeChar(unicodeChar, isDown, xtCode)
        else
            return emitForAndroidKeyCode(keyCode, isDown, xtCode)
    }

    /**
     * Emits X KeySym corresponding to [keyCode]
     */
    private fun emitForAndroidKeyCode(keyCode: Int, isDown: Boolean, xtCode: Int = 0): Boolean {
        val keySym = XKeySymAndroid.getKeySymForAndroidKeyCode(keyCode)
        return emit(keySym, isDown, xtCode)
    }

    /**
     * Emits either Unicode KeySym or legacy KeySym for [uChar], depending on [cfLegacyKeysym].
     */
    private fun emitForUnicodeChar(uChar: Int, isDown: Boolean, xtCode: Int = 0): Boolean {
        var uKeySym = 0

        if (cfLegacyKeysym)
            uKeySym = XKeySymUnicode.getLegacyKeySymForUnicodeChar(uChar)

        if (uKeySym == 0)
            uKeySym = XKeySymUnicode.getKeySymForUnicodeChar(uChar)


        // If we are generating legacy KeySym and the character is uppercase,
        // we need to fake press the Shift key. Otherwise, most servers can't
        // handle them. This is just a compat shim and ideally server should
        // support Unicode KeySym.
        val shouldFakeShift = uKeySym in 0x100..0xfffe && uChar.toChar().isUpperCase()
        if (shouldFakeShift)
            emitForAndroidKeyCode(KeyEvent.KEYCODE_SHIFT_LEFT, true)

        emit(uKeySym, isDown, xtCode)

        if (shouldFakeShift)
            emitForAndroidKeyCode(KeyEvent.KEYCODE_SHIFT_LEFT, false)

        return true
    }


    /**
     * Sends given X key to [dispatcher].
     */
    private fun emit(keySym: Int, isDown: Boolean, xtCode: Int = 0): Boolean {
        if (keySym == 0)
            return false

        return dispatcher.onXKey(keySym, xtCode, isDown)
    }


    /**
     * Returns unicode character for given event.
     * Normally [KeyEvent.getUnicodeChar] is sufficient for our need, but sometimes
     * we have to fiddle with meta state to extract a suitable character.
     *
     * Consider Ctrl+Shift+A: [KeyEvent.getUnicodeChar] returns 0 for this case,
     * because there is no character mapping for A when Ctrl & Shift both are pressed.
     * But we want to obtain capital 'A' here, so that we can send it to server.
     * This ensures proper working of keyboard shortcuts.
     */
    private fun getUnicodeChar(event: KeyEvent): Int {
        val uChar = event.unicodeChar
        if (uChar != 0 || event.metaState == 0)
            return uChar

        // Try without Alt/Ctrl
        val altCtrl = KeyEvent.META_ALT_MASK or KeyEvent.META_CTRL_MASK
        return event.getUnicodeChar(event.metaState and altCtrl.inv())
    }

    /**
     * Some cases where we want to ignore events.
     */
    private fun shouldIgnoreEvent(event: KeyEvent): Boolean {
        val keyCode = event.keyCode

        // As if our key-handling wasn't already complex enough, Android
        // decided to mess-up NumLock handling. When any numpad number-key
        // is pressed (e.g. 7) and NumLock is off, it will _still_ send
        // the number keycode (e.g. KEYCODE_NUMPAD_7) first. And if apps don't
        // handle that, it will fallback to secondary action (e.g. KEYCODE_MOVE_HOME).
        // So we have to ignore the first events when NumLock is off.
        return (keyCode in KeyEvent.KEYCODE_NUMPAD_0..KeyEvent.KEYCODE_NUMPAD_9
                || keyCode == KeyEvent.KEYCODE_NUMPAD_DOT) && !event.isNumLockOn
    }

    /************************************************************************************
     * Diacritics (Accents) Support
     *
     * Instead of sending diacritics directly to server, we handle their composition here.
     * This is done because:
     *
     * - Android does not report real 'combining' accents to us. Instead we get the
     *   corresponding 'printing' characters, with the `COMBINING_ACCENT` flag set.
     *   See the source code of [KeyCharacterMap] for more details.
     *
     * - Although most servers don't support diacritics directly, some of them can
     *   handle the final composed characters (e.g. TightVNC).
     *
     *
     * Behaviour:
     *
     * - Until an accent is received, all events are ignored by [handleDiacritics].
     * - When first accent is received, we start tracking by adding it to [accentSequence].
     * - When next key is received (can be another accent), we add it to [accentSequence].
     * - Then we try to compose a printable character from [accentSequence], and if successful,
     *   the composed character is sent to the server.
     * - If composition was successful, or we received non-accent key, we stop tracking
     *   by clearing [accentSequence].
     *
     ************************************************************************************/
    private var accentSequence = ArrayList<Int>(2)

    private fun handleDiacritics(keyCode: Int, uChar: Int, isDown: Boolean): Boolean {
        val isUp = !isDown
        val isAccent = uChar and KeyCharacterMap.COMBINING_ACCENT != 0
        val maskedChar = uChar and KeyCharacterMap.COMBINING_ACCENT_MASK

        if (!isAccent && accentSequence.size == 0) return false  // No tracking yet (most common case)
        if (!isAccent && isUp && !accentSequence.contains(maskedChar)) return false  // Spurious key-ups
        if (KeyEvent.isModifierKey(keyCode)) return false // Modifier keys are passed-on to the server

        if (isDown)
            accentSequence.add(maskedChar)

        if (accentSequence.size <= 1) // Nothing to compose yet
            return true

        var composed = accentSequence.last()
        for (i in 0 until accentSequence.lastIndex)
            composed = KeyEvent.getDeadChar(accentSequence[i], composed)

        if (composed != 0)
            emitForUnicodeChar(composed, isDown)

        if (isUp && (composed != 0 || !isAccent))
            accentSequence.clear()

        return true
    }

    /**
     * 'Ç' & 'ç' requires special handling because Android generates them with extra ALT key press,
     * and gives no indication in KeyEvents that accents are involved. So we have to handle these
     * before events are synthesized by InputConnection in FrameView.
     */
    private fun handleCCedilla(text: CharSequence?): Boolean {
        if (text == "ç") {
            emitForUnicodeChar('ç'.code, true)
            emitForUnicodeChar('ç'.code, false)
            return true
        }

        if (text == "Ç") {
            emitForAndroidKeyCode(KeyEvent.KEYCODE_SHIFT_LEFT, true)
            emitForUnicodeChar('Ç'.code, true)
            emitForUnicodeChar('Ç'.code, false)
            emitForAndroidKeyCode(KeyEvent.KEYCODE_SHIFT_LEFT, false)
            return true
        }

        return false
    }

    /************************************************************************************
     * Custom key-mappings
     ***********************************************************************************/

    init {
        if (prefs.input.kmLanguageSwitchToSuper) updateKeyMap(KeyEvent.KEYCODE_LANGUAGE_SWITCH, XKeySym.XK_Super_L)
        if (prefs.input.kmRightAltToSuper) updateKeyMap(KeyEvent.KEYCODE_ALT_RIGHT, XKeySym.XK_Super_L)
    }

    // We can't map Back key to Escape inside init because we don't
    // want to affect Back key events coming from Navigation Bar.
    // So we have to test each event.
    private val kmBackToEscape = prefs.input.kmBackToEscape

    private fun preProcessEvent(event: KeyEvent): KeyEvent {
        if (event.keyCode == KeyEvent.KEYCODE_BACK && kmBackToEscape
            && (event.flags and KeyEvent.FLAG_VIRTUAL_HARD_KEY == 0))
            return KeyEvent(event.action, KeyEvent.KEYCODE_ESCAPE)

        return event
    }

    /************************************************************************************
     * Convert String to Array of Unicode code-points
     ***********************************************************************************/

    private val cpCache = intArrayOf(0)

    private fun toCodePoints(string: String): IntArray {
        //Handle simple & most probable case
        if (string.length == 1)
            return cpCache.apply { this[0] = string[0].code }

        if (Build.VERSION.SDK_INT >= 24)
            return string.codePoints().toArray()

        //Otherwise, do simple conversion (will be incorrect non-MBP code points)
        return string.map { it.code }.toIntArray()
    }
}