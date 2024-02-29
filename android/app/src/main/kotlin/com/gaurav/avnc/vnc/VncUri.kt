/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.vnc

import android.net.Uri
import com.gaurav.avnc.model.ServerProfile
import java.net.URI

/**
 * This class implements the `vnc` URI scheme.
 * Reference: https://tools.ietf.org/html/rfc7869
 *
 * If host in given URI string is an IPv6 address, it MUST be wrapped in square brackets.
 * (This requirement come from using Java [URI] internally.)
 *
 * If given URI doesn't start with 'vnc://' scheme, it will be automatically added.
 */
class VncUri(str: String) {

    /**
     * Add scheme if missing.
     * It is also common for users to accidentally type 'vnc:host' instead of 'vnc://host',
     * so we gracefully handle that case too.
     */
    private val uriString = str.replaceFirst(Regex("^(vnc:/?/?)?", RegexOption.IGNORE_CASE), "vnc://")

    private val uri = Uri.parse(uriString)

    /**
     * Older versions of Android [Uri] does not support IPv6, so we need to use Java [URI] for host & port.
     * It also serves as a validation step because [URI] verifies that address is well-formed.
     */
    private val javaUri = runCatching { URI(uriString) }.getOrNull()


    val host = javaUri?.host?.trim('[', ']') ?: ""
    val port = if (javaUri?.port == -1) 5900 else javaUri?.port ?: 5900
    val connectionName = uri.getQueryParameter("ConnectionName") ?: ""
    val username = uri.getQueryParameter("VncUsername") ?: ""
    val password = uri.getQueryParameter("VncPassword") ?: ""
    val securityType = uri.getQueryParameter("SecurityType")?.toIntOrNull() ?: 0
    val channelType = uri.getQueryParameter("ChannelType")?.toIntOrNull() ?: ServerProfile.CHANNEL_TCP
    val colorLevel = uri.getQueryParameter("ColorLevel")?.toIntOrNull() ?: 7
    val viewOnly = uri.getBooleanQueryParameter("ViewOnly", false)
    val saveConnection = uri.getBooleanQueryParameter("SaveConnection", false)
    val sshHost = uri.getQueryParameter("SshHost") ?: host
    val sshPort = uri.getQueryParameter("SshPort")?.toIntOrNull() ?: 22
    val sshUsername = uri.getQueryParameter("SshUsername") ?: ""
    val sshPassword = uri.getQueryParameter("SshPassword") ?: ""

    /**
     * Generates a [ServerProfile] using this instance.
     */
    fun toServerProfile() = ServerProfile(
            name = connectionName,
            host = host,
            port = port,
            username = username,
            password = password,
            securityType = securityType,
            channelType = channelType,
            colorLevel = colorLevel,
            viewOnly = viewOnly,
            sshHost = sshHost,
            sshPort = sshPort,
            sshUsername = sshUsername,
            sshAuthType = ServerProfile.SSH_AUTH_PASSWORD,
            sshPassword = sshPassword
    )

    override fun toString() = uriString
}