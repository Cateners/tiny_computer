/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.model

import android.os.Parcelable
import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import kotlinx.parcelize.IgnoredOnParcel
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import kotlin.reflect.KProperty

/**
 * This class holds connection configuration of a remote VNC server.
 *
 * Some fields remain unused until that feature is implemented.
 */
@Parcelize
@Serializable
@Entity(tableName = "profiles")
data class ServerProfile(

        @PrimaryKey(autoGenerate = true)
        var ID: Long = 0,

        /**
         * Descriptive name of the server (e.g. 'Kitchen PC').
         */
        var name: String = "",

        /**
         * Internet address of the server (without port number).
         * This can be hostname or IP address.
         */
        var host: String = "",

        /**
         * Port number of the server.
         */
        var port: Int = 5900,

        /**
         * Username used for authentication.
         */
        var username: String = "",

        /**
         * Password used for authentication.
         * Note: Username & password may not be used for all security types.
         */
        var password: String = "",

        /**
         * Security type to use when connecting to this server (e.g. VncAuth).
         * 0 enables all supported types.
         */
        var securityType: Int = 0,

        /**
         * Transport channel to be used for communicating with the server.
         * e.g. TCP, SSH Tunnel
         */
        var channelType: Int = CHANNEL_TCP,

        /**
         * Specifies the color level of received frames.
         * This value determines the pixel-format used for framebuffer.
         */
        var colorLevel: Int = 0,

        /**
         * Specifies the image quality of the frames.
         * This mainly affects the compression level used by some encodings.
         */
        var imageQuality: Int = 5,

        /**
         * Use raw encoding for framebuffer.
         * This can improve performance when server is running on localhost.
         */
        @ColumnInfo(defaultValue = "0")
        var useRawEncoding: Boolean = false,

        /**
         * Initial zoom for the viewer.
         * This will be used in portrait orientation, or when per-orientation zooming is disabled.
         */
        @ColumnInfo(defaultValue = "1.0")
        var zoom1: Float = 1f,

        /**
         * This will be used in landscape orientation if per-orientation zooming is enabled.
         */
        @ColumnInfo(defaultValue = "1.0")
        var zoom2: Float = 1f,

        /**
         * Specifies whether 'View Only' mode should be used.
         * In this mode client does not send any input messages to remote server.
         */
        var viewOnly: Boolean = false,

        /**
         * Whether the cursor should be drawn by client instead of server.
         * It's value is currently ignored, and hardcoded to true.
         * See [com.gaurav.avnc.viewmodel.VncViewModel.preConnect]
         */
        var useLocalCursor: Boolean = true,

        /**
         * Server type hint received from user, e.g. tigervnc, tightvnc, vino
         * Can be used in future to handle known server quirks.
         */
        @ColumnInfo(defaultValue = "")
        var serverTypeHint: String = "",

        /**
         * Composite field for various flags.
         * This is accessed via individual members like [fLegacyKeySym].
         */
        var flags: Long = FLAG_LEGACY_KEYSYM,

        /**
         * Preferred style to use for gesture handling.
         * Possible values: auto, touchscreen, touchpad
         */
        @ColumnInfo(defaultValue = "auto")
        var gestureStyle: String = "auto",

        /**
         * Preferred screen orientation.
         * Possible values: auto, portrait, landscape
         */
        @ColumnInfo(defaultValue = "auto")
        var screenOrientation: String = "auto",

        /**
         * Usage count tracks how many times user has connected to a server.
         * Can be used to put frequent servers on top.
         */
        var useCount: Int = 0,

        /**
         * Whether UltraVNC Repeater is used for connections.
         * When repeater is used, [host] & [port] identifies the repeater.
         */
        var useRepeater: Boolean = false,

        /**
         * When using a repeater, this value identifies the VNC server.
         * Valid IDs: [0, 999999999].
         */
        var idOnRepeater: Int = 0,

        /**
         * Resize remote desktop to match with local window size.
         */
        @ColumnInfo(defaultValue = "0")
        var resizeRemoteDesktop: Boolean = false,

        /**
         * These values are used for SSH Tunnel
         */
        var sshHost: String = "",
        var sshPort: Int = 22,
        var sshUsername: String = "",
        var sshAuthType: Int = SSH_AUTH_KEY,
        var sshPassword: String = "",
        var sshPrivateKey: String = "",
        var sshPrivateKeyPassword: String = ""

) : Parcelable {

    companion object {
        // Channel types (from RFC 7869)
        const val CHANNEL_TCP = 1
        const val CHANNEL_SSH_TUNNEL = 24

        // SSH auth types
        const val SSH_AUTH_KEY = 1
        const val SSH_AUTH_PASSWORD = 2

        // Flag masks
        private const val FLAG_LEGACY_KEYSYM = 0x01L
        private const val FLAG_BUTTON_UP_DELAY = 0x02L
        private const val FLAG_ZOOM_LOCKED = 0x04L
    }

    /**
     * Delegated property builder for [flags] field.
     */
    private class Flag(val flag: Long) {
        operator fun getValue(p: ServerProfile, kp: KProperty<*>) = (p.flags and flag) != 0L
        operator fun setValue(p: ServerProfile, kp: KProperty<*>, value: Boolean) {
            p.flags = if (value) p.flags or flag else p.flags and flag.inv()
        }
    }

    /**
     * Flag to emit legacy X KeySym events in certain cases.
     */
    @IgnoredOnParcel
    var fLegacyKeySym by Flag(FLAG_LEGACY_KEYSYM)

    /**
     * Flag to insert artificial delay before UP event of left-click.
     */
    @IgnoredOnParcel
    var fButtonUpDelay by Flag(FLAG_BUTTON_UP_DELAY)

    /**
     * If zoom is locked, user requests to change [zoom1] & [zoom2]
     * should be ignored.
     */
    @IgnoredOnParcel
    var fZoomLocked by Flag(FLAG_ZOOM_LOCKED)
}
