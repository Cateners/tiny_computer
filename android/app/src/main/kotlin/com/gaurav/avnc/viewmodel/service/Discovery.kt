/*
 * Copyright (c) 2024  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

package com.gaurav.avnc.viewmodel.service

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.net.wifi.WifiManager.MulticastLock
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import com.gaurav.avnc.model.ServerProfile
import java.util.concurrent.Executors

/**
 * Discovers VNC servers advertising themselves on the network.
 */
object Discovery {
    private const val TAG = "VncServiceDiscovery"

    /**
     * List of servers found by Discovery.
     */
    val servers = MutableLiveData<List<ServerProfile>>()

    /**
     * Status of discovery.
     */
    val isRunning = MutableLiveData(false)

    private val impl by lazy { Impl() }

    /**
     * Starts discovery.
     */
    fun start(context: Context) = impl.start(context)

    /**
     * Stops discovery.
     */
    fun stop() = impl.stop()


    /**
     * Due to Android API limitations,  service discovery is more complicated than necessary:
     *
     * - [NsdManager] is asynchronous, which means every command's result is communicated later
     *   on a separate thread. Also, [NsdManager] throws up if more than one request is made by same
     *   listener. You can't call [NsdManager.discoverServices] even if discovery is already started
     *   So to avoid race conditions (and keep my sanity, because its one of those APIs in Android
     *   where I wish some day the API designers are forced to use this crap themselves), all callbacks
     *   of [Impl] are run on a dedicated [executor], and [startRequested] is used to track pending start.
     *
     * - Only one service can resolved at a time via [NsdManager.resolveService]. To handle this,
     *   newly found service is first added to [pendingResolves]. When resolution finishes for a
     *   service, we remove that service form [pendingResolves], and start resolution for the next.
     *
     * - Android can filter/drop multicast WiFi packets to save power. Devices, like Pixel phone, enable
     *   this feature. This can be turned off by acquiring a multicast lock, but [NsdManager] doesn't
     *   doesn't do this automatically. So we have to acquire it manually.
     */
    @Suppress("DEPRECATION")  // Yeah, f**k you too Google
    private class Impl {
        private val serviceType = "_rfb._tcp"
        private var wifiManager: WifiManager? = null
        private var multicastLock: MulticastLock? = null
        private var nsdManager: NsdManager? = null
        private val listener = DiscoveryListener()
        private val executor = Executors.newSingleThreadExecutor()

        private var started = false
        private var startRequested = false
        private val pendingResolves = mutableMapOf<ResolveListener, NsdServiceInfo>()
        private val resolvedProfiles = mutableSetOf<ServerProfile>()

        private fun execute(action: Runnable) {
            runCatching { executor.execute(action) }.onFailure { Log.e(TAG, "Cannot execute action", it) }
        }

        private fun postResolvedProfiles() {
            servers.postValue(resolvedProfiles.toList())
        }

        fun start(context: Context) = execute {
            if (startRequested || started)
                return@execute

            val appContext = context.applicationContext // Need app context to avoid possibility of WiFiManager leaks
            wifiManager = wifiManager ?: ContextCompat.getSystemService(appContext, WifiManager::class.java)
            nsdManager = nsdManager ?: ContextCompat.getSystemService(appContext, NsdManager::class.java)
            nsdManager!!.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, listener)
            startRequested = true

            // Forget old profiles
            resolvedProfiles.clear()
            postResolvedProfiles()
        }

        fun stop() = execute {
            if (started)
                nsdManager?.stopServiceDiscovery(listener)
        }

        fun onStarted() = execute {
            started = true
            startRequested = false
            isRunning.postValue(true)

            multicastLock = wifiManager?.createMulticastLock(TAG)
            multicastLock?.acquire()
        }

        fun onStopped() = execute {
            started = false
            isRunning.postValue(false)
            multicastLock?.release()
            multicastLock = null
        }

        fun onStartFailed() = execute {
            startRequested = false
        }

        fun onServiceFound(serviceInfo: NsdServiceInfo) = execute {
            val listener = ResolveListener()
            pendingResolves[listener] = serviceInfo
            if (pendingResolves.size == 1) // Kick-start the resolution chain
                nsdManager?.resolveService(serviceInfo, listener)
        }

        fun onServiceLost(serviceInfo: NsdServiceInfo) = execute {
            resolvedProfiles.removeAll { it.name == serviceInfo.serviceName }
            postResolvedProfiles()
        }

        fun onResolved(si: NsdServiceInfo) = execute {
            resolvedProfiles.add(ServerProfile(name = si.serviceName, host = si.host.hostAddress!!, port = si.port))
            postResolvedProfiles()
        }

        fun onResolveFinished(finishedResolve: ResolveListener) = execute {
            pendingResolves.remove(finishedResolve)
            pendingResolves.keys.firstOrNull()?.let { nsdManager?.resolveService(pendingResolves[it], it) }
        }
    }

    /**
     * Listener for discovery process.
     */
    private class DiscoveryListener : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String?) = impl.onStarted()
        override fun onDiscoveryStopped(serviceType: String?) = impl.onStopped()
        override fun onServiceFound(serviceInfo: NsdServiceInfo) = impl.onServiceFound(serviceInfo)
        override fun onServiceLost(serviceInfo: NsdServiceInfo) = impl.onServiceLost(serviceInfo)

        override fun onStartDiscoveryFailed(serviceType: String?, errorCode: Int) {
            Log.e(TAG, "Service discovery failed to start [E: $errorCode ]")
            impl.onStartFailed()
        }

        override fun onStopDiscoveryFailed(serviceType: String?, errorCode: Int) {
            Log.w(TAG, "Service discovery failed to stop [E: $errorCode ]")
            // From our perspective, this is same as onDiscoveryStopped().
            // We can't retry stopping it because NsdManager will clear the listener
            // before invoking this callback.
            impl.onStopped()
        }
    }

    /**
     * Listener for service resolution result.
     */
    private class ResolveListener : NsdManager.ResolveListener {
        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
            impl.onResolved(serviceInfo)
            impl.onResolveFinished(this)
        }

        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
            Log.e(TAG, "Service resolution failed for '${serviceInfo}' [E: $errorCode]")
            impl.onResolveFinished(this)
        }
    }
}