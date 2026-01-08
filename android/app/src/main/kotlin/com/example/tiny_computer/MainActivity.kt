package com.example.tiny_computer

import android.system.Os.setenv

import android.content.Intent
import androidx.annotation.NonNull
import androidx.annotation.Keep
import androidx.appcompat.app.AppCompatDelegate
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel


class MainActivity: FlutterActivity() {

    override fun configureFlutterEngine(@NonNull flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "android").setMethodCallHandler {
            // 注册通道并设置方法调用处理器
            call, result ->
            // 判断方法名
            when (call.method) {
                "launchSignal9Page" -> {
                    startActivity(Intent(this, Signal9Activity::class.java))
                    result.success(0)
                }
                "getNativeLibraryPath" -> {
                    result.success(getApplicationInfo().nativeLibraryDir)
                }
                "startStreaming" -> {
                    AudioStream.startStreaming(call.argument("path")!!)
                }
                "stopStreaming" -> {
                    AudioStream.stopStreaming()
                }
                else -> {
                    // 不支持的方法名
                    result.notImplemented()
                }
            }
        }
    }

}
