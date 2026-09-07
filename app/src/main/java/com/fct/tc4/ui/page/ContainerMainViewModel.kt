// ContainerMainViewModel.kt -- This file is part of tiny_container.
//
// Copyright (C) 2026 Caten Hu
//
// Tiny Container is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or any later version.
//
// Tiny Container is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.

package com.fct.tc4.ui.page

import android.app.Application
import android.content.Intent
import android.system.Os
import android.system.OsConstants
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.fct.tc4.TinyAudio
import com.fct.tc4.TinyMicrophone
import com.fct.tc4.R
import com.fct.tc4.TinyIpp
import com.fct.tc4.TinyStorage
import com.fct.tc4.ui.misc.ConfigManager
import com.fct.tc4.ui.misc.Global
import com.fct.tc4.ui.misc.UpdateChecker
import com.fct.tc4.ui.misc.UpdateResult
import com.fct.tc4.x11.XServerService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.io.File
import java.net.BindException
import java.net.DatagramSocket
import java.net.ServerSocket

sealed interface GuiNavigationEvent {
    data class OpenWebView(val link: String) : GuiNavigationEvent
    data class OpenAvnc(
        val link: String,
        val adaptToScreenSize: Boolean,
        val scaleRatio: Double,
        val useUnixSocket: Boolean
    ) : GuiNavigationEvent
    data object OpenX11 : GuiNavigationEvent
}

class ContainerMainViewModel(
    application: Application,
    private val code: String
) : AndroidViewModel(application) {

    companion object {
        private const val TAG = "ContainerMainVM"
    }

    /** 是否启动过 X server（避免未启动时就发送 ACTION_STOP 导致创建 :xserver 进程） */
    private var xserverStarted = false

    private lateinit var config: Map<String, Any>

    private val _configState = MutableStateFlow<Map<String, Any>?>(null)
    val configState: StateFlow<Map<String, Any>?> = _configState.asStateFlow()

    private val _isInitialized = MutableStateFlow(false)
    val isInitialized: StateFlow<Boolean> = _isInitialized.asStateFlow()

    private val _errorMessage = MutableStateFlow<String?>(null)
    val errorMessage: StateFlow<String?> = _errorMessage.asStateFlow()

    private val _navigationEvents = MutableSharedFlow<GuiNavigationEvent>(extraBufferCapacity = 64)
    val navigationEvents: SharedFlow<GuiNavigationEvent> = _navigationEvents.asSharedFlow()

    private val _updateEvent = MutableSharedFlow<String>(extraBufferCapacity = 1)
    val updateEvent: SharedFlow<String> = _updateEvent.asSharedFlow()

    init {
        viewModelScope.launch {
            val loaded = ConfigManager.load(code)
            if (loaded == null) {
                _errorMessage.value = getApplication<Application>().getString(R.string.tc4_container_config_lost, code)
                _isInitialized.value = true
                return@launch
            }
            config = loaded
            _configState.value = config
            setupEnvironment()
            withContext(Dispatchers.IO) {
                launchContainer()
            }
            _isInitialized.value = true

            // 自动检测更新
            if (Global.autoCheckUpdate) {
                viewModelScope.launch(Dispatchers.IO) {
                    when (val result = UpdateChecker.check(getApplication())) {
                        is UpdateResult.NewVersion -> _updateEvent.tryEmit(result.version)
                        else -> {}
                    }
                }
            }

            // 检查是否有待执行的捷径命令
            if (code == com.fct.tc4.ui.main.MainViewModel.pendingCommandCode) {
                val cmd = com.fct.tc4.ui.main.MainViewModel.pendingCommandText
                if (cmd != null) {
                    delay(2000)
                    Global.sendCommand(cmd)
                }
                com.fct.tc4.ui.main.MainViewModel.clearPendingCommand()
            }
        }
    }

    fun exitContainer() {
        Global.onSessionSignal9 = null // 主动退出，不触发 signal 9 跳转
        killXServer()
        TinyAudio.stop()
        TinyMicrophone.stop()
        TinyIpp.stop()
        TinyStorage.stop()
        val pid = Global.terminalSession?.pid ?: 0
        if (pid > 0) {
            Os.kill(-pid, OsConstants.SIGQUIT)
        }
        Global.newSession()
        Global.setupEnvironment()
        Global.sendCommand("rm ${getApplication<Application>().filesDir}/boot_${code}.sh")
        val merged = collectEnabledOptions()
        for (cmd in merged.postEndHostCommands) {
            Global.sendCommand(cmd)
        }
    }

    @Suppress("UNCHECKED_CAST")
    fun enterGui() {
        viewModelScope.launch {
            val features = config["feature"] as? List<Map<String, Any>> ?: emptyList()
            for (feature in features) {
                val type = feature["type"] as? String ?: continue
                if (feature["enabled"] as? Boolean != true) continue
                when (type) {
                    "webview" -> {
                        val link = feature["link"] as? String ?: return@launch
                        _navigationEvents.tryEmit(GuiNavigationEvent.OpenWebView(link))
                    }
                    "avnc" -> {
                        val link = feature["link"] as? String ?: return@launch
                        val adaptToScreenSize = feature["adapt_to_screen_size"] as? Boolean ?: return@launch
                        val scaleRatio = (feature["scale_ratio"] as? Number)?.toDouble() ?: return@launch
                        val useUnixSocket = feature["use_unix_socket"] as? Boolean ?: true
                        _navigationEvents.tryEmit(GuiNavigationEvent.OpenAvnc(link, adaptToScreenSize, scaleRatio, useUnixSocket))
                    }
                    "x11" -> {
                        _navigationEvents.tryEmit(GuiNavigationEvent.OpenX11)
                    }
                }
            }
        }
    }

    private fun setupEnvironment() {
        val app = getApplication<Application>()
        Global.terminalSession?.finishIfRunning()
        Global.newSession(onFinished = { exitCode ->
            if (exitCode == -9) {
                Global.onSessionSignal9?.invoke()
            }
        })
        Global.onSessionSignal9 = {
            val intent = Intent(getApplication(), Signal9Activity::class.java)
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            getApplication<Application>().startActivity(intent)
        }
        Global.setupEnvironment()
        val containerDir = "${app.dataDir.absolutePath}/$code"
        Global.sendCommand("export CONTAINER_DIR=$containerDir")
        val prootVariant = if (Global.useLegacyProot) "proot-classic" else "proot-latest"
        Global.sendCommand(
            "ln -sf ${app.filesDir.absolutePath}/applib/lib__bin__${prootVariant}__.so" +
            " ${app.filesDir.absolutePath}/bootstrap/bin/proot"
        )
    }

    @Suppress("UNCHECKED_CAST")
    private suspend fun launchContainer() {

        val merged = collectEnabledOptions()
        // 处理 lstat-cache feature：解析路径，生成 --assured-path= 参数
        merged.args.addAll(collectLstatCacheArgs())
        // 处理 storage feature：生成 --tiny-storage 和初始 --bind= 参数
        collectStorageArgs(merged.args)
        for (cmd in merged.preStartHostCommands) {
            Global.sendCommand(cmd)
        }

        val bootCmd = config["boot_command"] as? String
        if (bootCmd == null) {
            _errorMessage.value = getApplication<Application>().getString(R.string.tc4_container_missing_boot_cmd, code)
            return
        }
        val resolvedBootCmd = bootCmd
            .replace("\$EXTRA_ARGS", merged.args.joinToString(" "))
            .replace("\$EXTRA_PATH", merged.path.joinToString(":"))
            .replace("\$EXTRA_LD_LIBRARY_PATH", merged.ldLibraryPath.joinToString(":"))
            .replace("\$EXTRA_LD_PRELOAD", merged.ldPreload.joinToString(" "))
            .replace("\$EXTRA_ENV", merged.env.joinToString(" "))

        // 写入临时脚本文件，绕过 PTY 单行 4096 字节限制
        val bootScript = File("${getApplication<Application>().filesDir}/boot_${code}.sh")
        bootScript.writeText("exec $resolvedBootCmd")
        Global.sendCommand("source ${bootScript.absolutePath}")

        for (cmd in merged.postStartContainerCommands) {
            Global.sendCommand(cmd)
        }

        launchEnabledFeature()
    }

    // ===================== 图形界面处理 =====================

    @Suppress("UNCHECKED_CAST")
    private suspend fun launchEnabledFeature() {
        val features = config["feature"] as? List<Map<String, Any>> ?: emptyList()
        for (feature in features) {
            val type = feature["type"] as? String ?: continue
            if (feature["enabled"] as? Boolean != true) continue
            when (type) {
                "audio" -> {
                    TinyAudio.start()
                }
                "print" -> {
                    TinyIpp.start()
                }
                "storage" -> {
                    TinyStorage.start()
                }
                "webview" -> {
                    if (!Global.autoLaunchGui) continue
                    val link = feature["link"] as? String ?: continue
                    val command = feature["command"] as? String ?: continue
                    Global.sendCommand(command)
                    val port = extractPort(link)
                    Log.d(TAG, "webview: link=$link port=$port waiting for port...")
                    if (port == null) continue
                    if (!waitForPort(port)) {
                        Log.e(TAG, "webview: port=$port not ready, giving up")
                        continue
                    }
                    _navigationEvents.tryEmit(GuiNavigationEvent.OpenWebView(link))
                }
                "avnc" -> {
                    if (!Global.autoLaunchGui) continue
                    val link = feature["link"] as? String ?: continue
                    val command = feature["command"] as? String ?: continue
                    val adaptToScreenSize = feature["adapt_to_screen_size"] as? Boolean ?: continue
                    val scaleRatio = (feature["scale_ratio"] as? Number)?.toDouble() ?: continue
                    val useUnixSocket = feature["use_unix_socket"] as? Boolean ?: true
                    Global.sendCommand(command)
                    if (useUnixSocket) {
                        if (!waitForFile("${getApplication<Application>().filesDir}/tmp/.tiny.vnc")) {
                            continue
                        }
                    } else {
                        val port = extractPort(link)
                        Log.d(TAG, "avnc: link=$link port=$port waiting for port...")
                        if (port == null) continue
                        if (!waitForPort(port)) {
                            Log.e(TAG, "avnc: port=$port not ready, giving up")
                            continue
                        }
                    }
                    _navigationEvents.tryEmit(GuiNavigationEvent.OpenAvnc(link, adaptToScreenSize, scaleRatio, useUnixSocket))
                }
                "x11" -> {
                    if (!Global.autoLaunchGui) continue
                    val args = feature["args"] as? List<String> ?: continue
                    val command = feature["command"] as? String ?: continue
                    viewModelScope.launch {
                        launchXServer(args)
                    }
                    if (!waitForFile("${getApplication<Application>().filesDir}/tmp/.X11-unix/X${extractDisplay(args)}")) {
                        continue
                    }
                    Global.sendCommand(command)
                    _navigationEvents.tryEmit(GuiNavigationEvent.OpenX11)
                }
            }
        }
    }

    private fun launchXServer(xserverArgs: List<String>) {
        xserverStarted = true
        val app = getApplication<Application>()
        val envKeys = arrayOf(
            "TMPDIR", "XKB_CONFIG_ROOT",
            "TERMUX_X11_DEBUG", "TERMUX_X11_OVERRIDE_PACKAGE"
        )
        val envVals = arrayOf(
            "${app.filesDir.absolutePath}/tmp",
            "${app.dataDir.absolutePath}/$code/usr/share/X11/xkb",
            "1",
            app.packageName
        )
        XServerService.start(app, xserverArgs.toTypedArray(), envKeys, envVals)
    }

    private fun killXServer() {
        if (xserverStarted) {
            XServerService.stop(getApplication())
            xserverStarted = false
        }
    }

    override fun onCleared() {
        super.onCleared()
        Global.onSessionSignal9 = null // 防止回调泄漏
        killXServer()
    }

    private fun extractDisplay(args: List<String>): String {
        for (arg in args) {
            if (arg.startsWith(":")) return arg.drop(1)
        }
        return "0"
    }

    /**
     * 收集所有启用的 lstat-cache feature 的路径，解析通配符和变量，
     * 生成 --assured-path= 格式的 proot 参数列表。
     */
    @Suppress("UNCHECKED_CAST")
    private fun collectLstatCacheArgs(): List<String> {
        val features = config["feature"] as? List<Map<String, Any>> ?: return emptyList()
        val app = getApplication<Application>()
        val cacheDir = app.filesDir.absolutePath
        val containerDir = "${app.dataDir.absolutePath}/$code"

        val result = mutableListOf<String>()
        for (feature in features) {
            val type = feature["type"] as? String ?: continue
            if (type != "lstat-cache") continue
            if (feature["enabled"] as? Boolean != true) continue
            val paths = feature["path"] as? List<String> ?: continue

            for (rawPath in paths) {
                // 替换变量
                val resolved = rawPath
                    .replace("\$CACHE_DIR", cacheDir)
                    .replace("\$CONTAINER_DIR", containerDir)

                // 处理通配符
                val expanded = expandLstatPath(resolved)
                for (p in expanded) {
                    val escaped = p.replace(" ", "\\ ")
                    result.add("--assured-path=$escaped")
                }
            }
        }
        return result
    }

    private fun expandLstatPath(path: String): List<String> {
        val starIdx = path.indexOf('*')
        if (starIdx == -1) return listOf(path)

        // * 前后最近的两个 / 切出父目录和剩余路径
        val slashBeforeStar = path.lastIndexOf('/', starIdx)
        val slashAfterStar = path.indexOf('/', starIdx)

        val parentDir = if (slashBeforeStar >= 0) {
            path.substring(0, slashBeforeStar)
        } else {
            Log.w(TAG, "lstat-cache: * 前没有 /，无法定位父目录 : $path")
            return emptyList()
        }

        val suffix = if (slashAfterStar >= 0) path.substring(slashAfterStar) else ""

        val wildcardSegment = path.substring(slashBeforeStar + 1,
            if (slashAfterStar >= 0) slashAfterStar else path.length)

        val dir = File(parentDir)
        if (!dir.isDirectory) {
            Log.w(TAG, "lstat-cache: 父目录不存在 : $parentDir")
            return emptyList()
        }

        val pattern = globToRegex(wildcardSegment)
        return try {
            val files = dir.listFiles()
            if (files == null) {
                Log.w(TAG, "lstat-cache: 无法列出目录 : $parentDir")
                return emptyList()
            }
            val matched = files
                .filter { it.name != "." && it.name != ".." && pattern.matches(it.name) }
                .map { File(it.absolutePath + suffix) }
                .filter { candidate ->
                    val parent = candidate.parentFile ?: return@filter false
                    try {
                        parent.listFiles()?.any { it.name == candidate.name } ?: false
                    } catch (_: Exception) {
                        false
                    }
                }
                .map { it.absolutePath }
            if (matched.isNotEmpty()) {
                Log.i(TAG, "lstat-cache: $parentDir/*$suffix → ${matched.size} 个路径")
            } else {
                Log.w(TAG, "lstat-cache: 无匹配, $parentDir/*$suffix, ${files.size} 个候选")
            }
            matched
        } catch (e: SecurityException) {
            Log.w(TAG, "lstat-cache: 无权限 : $parentDir", e)
            emptyList()
        }
    }

    /**
     * 如果 storage feature 启用，检测已插入的外部存储设备，
     * 将 --tiny-storage 和初始 --bind= 参数注入 args 列表。
     */
    @Suppress("UNCHECKED_CAST")
    private fun collectStorageArgs(args: MutableList<String>) {
        val features = config["feature"] as? List<Map<String, Any>> ?: return
        val storageFeature = features.firstOrNull {
            (it["type"] as? String) == "storage" && it["enabled"] == true
        } ?: return

        // 通知 proot 启动 socket listener
        args.add("--tiny-storage")

        // 检测当前已挂载的外部存储，添加静态 bind
        val devices = TinyStorage.detectExternalStorage()
        for ((path, name) in devices) {
            args.add("--bind=$path:/mnt/$name")
        }
    }

    /** glob → regex，* 通配任意字符 */
    private fun globToRegex(glob: String): Regex {
        val parts = glob.split("*").map { Regex.escape(it) }
        return Regex("^${parts.joinToString(".*")}$")
    }

    @Suppress("UNCHECKED_CAST")
    private fun collectEnabledOptions(): MergedOptions {
        val rawOptions = config["options"] as? List<Map<String, Any>> ?: emptyList()
        return collectOptionsRecursive(rawOptions)
    }

    @Suppress("UNCHECKED_CAST")
    private fun collectOptionsRecursive(options: List<Map<String, Any>>): MergedOptions {
        val result = MergedOptions()
        for (option in options) {
            val type = option["type"] as? String ?: continue
            if (type == "options") {
                val subs = option["options"] as? List<Map<String, Any>> ?: emptyList()
                result += collectOptionsRecursive(subs)
            } else if (type == "option") {
                if (option["enabled"] as? Boolean == true) {
                    result += option
                }
            }
        }
        return result
    }

    private data class MergedOptions(
        val env: MutableList<String> = mutableListOf(),
        val path: MutableList<String> = mutableListOf(),
        val ldLibraryPath: MutableList<String> = mutableListOf(),
        val ldPreload: MutableList<String> = mutableListOf(),
        val args: MutableList<String> = mutableListOf(),
        val preStartHostCommands: MutableList<String> = mutableListOf(),
        val postStartContainerCommands: MutableList<String> = mutableListOf(),
        val postEndHostCommands: MutableList<String> = mutableListOf()
    ) {
        @Suppress("UNCHECKED_CAST")
        operator fun plusAssign(option: Map<String, Any>) {
            (option["env"] as? List<String>)?.let { env.addAll(it) }
            (option["path"] as? List<String>)?.let { path.addAll(it) }
            (option["ld_library_path"] as? List<String>)?.let { ldLibraryPath.addAll(it) }
            (option["ld_preload"] as? List<String>)?.let { ldPreload.addAll(it) }
            (option["args"] as? List<String>)?.let { args.addAll(it) }
            (option["pre_start_host_command"] as? String)?.let { preStartHostCommands.add(it) }
            (option["post_start_container_command"] as? String)?.let { postStartContainerCommands.add(it) }
            (option["post_end_host_command"] as? String)?.let { postEndHostCommands.add(it) }
        }

        operator fun plusAssign(other: MergedOptions) {
            env.addAll(other.env)
            path.addAll(other.path)
            ldLibraryPath.addAll(other.ldLibraryPath)
            ldPreload.addAll(other.ldPreload)
            args.addAll(other.args)
            preStartHostCommands.addAll(other.preStartHostCommands)
            postStartContainerCommands.addAll(other.postStartContainerCommands)
            postEndHostCommands.addAll(other.postEndHostCommands)
        }
    }

    private fun extractPort(link: String): Int? {
        val portRegex = Regex(":(\\d+)(?:/|$)")
        return portRegex.find(link)?.groupValues?.get(1)?.toIntOrNull()
    }

    private suspend fun waitForPort(port: Int, timeoutMs: Long = 15_000): Boolean {
        return withTimeoutOrNull(timeoutMs) {
            while (true) {
                if (checkTcpPort(port) || checkUdpPort(port)) {
                    return@withTimeoutOrNull true
                }
                delay(500)
            }
            return@withTimeoutOrNull false
        } ?: false
    }

    private fun checkTcpPort(port: Int): Boolean {
        return try {
            ServerSocket(port).close()
            false
        } catch (e: BindException) {
            true
        }
    }

    private fun checkUdpPort(port: Int): Boolean {
        return try {
            DatagramSocket(port).close()
            false
        } catch (e: BindException) {
            true
        }
    }

    private suspend fun waitForFile(filePath: String, timeoutMs: Long = 15_000): Boolean {
        val file = File(filePath)
        var timer = 0
        while (timeoutMs > timer) {
            if (file.exists()) return true
            delay(500)
            timer += 500
        }
        return false
    }


    fun saveConfig() {
        ConfigManager.save(code, config)
    }

    class Factory(private val application: Application, private val code: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T {
            return ContainerMainViewModel(application, code) as T
        }
    }
}
