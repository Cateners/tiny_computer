// XServerService.kt -- This file is part of tiny_container.
//
// Copyright (C) 2026 Caten Hu
//
// Tiny Container is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or any later version.

package com.fct.tc4.x11

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import com.fct.tc4.R
import com.fct.tc4.ui.main.MainActivity
import com.termux.x11.CmdEntryPointService

/** Runs the embedded Termux:X11 server as a foreground service in the :xserver process. */
class XServerService : CmdEntryPointService() {

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_START) {
            promoteToForeground()
        }

        return super.onStartCommand(intent, flags, startId)
    }

    private fun promoteToForeground() {
        createNotificationChannel()

        val openAppIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            openAppIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val notification = NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setSmallIcon(R.drawable.screen_share_24dp_1f1f1f_fill0_wght400_grad0_opsz24)
            .setContentTitle(getString(R.string.tc4_app_name))
            .setContentText(getString(R.string.tc4_xserver_notification_text))
            .setContentIntent(contentIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setOngoing(true)
            .setSilent(true)
            .setOnlyAlertOnce(true)
            .build()

        val foregroundServiceType =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE
            } else {
                0
            }
        ServiceCompat.startForeground(
            this,
            NOTIFICATION_ID,
            notification,
            foregroundServiceType
        )
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            getString(R.string.tc4_app_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            setShowBadge(false)
        }
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "xserver"
        private const val NOTIFICATION_ID = 1001

        fun start(
            context: Context,
            args: Array<String>,
            envKeys: Array<String>,
            envValues: Array<String>
        ) {
            val intent = Intent(context, XServerService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_ARGS, args)
                putExtra(EXTRA_ENV_KEYS, envKeys)
                putExtra(EXTRA_ENV_VALUES, envValues)
            }
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            context.startService(Intent(context, XServerService::class.java).apply {
                action = ACTION_STOP
            })
        }
    }
}
