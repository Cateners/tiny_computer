package com.example.tiny_computer

import android.content.Context
import com.google.android.material.color.DynamicColors
import io.flutter.app.FlutterApplication
import me.weishu.reflection.Reflection

class MainApplication : FlutterApplication() {

    override fun onCreate() {
        super.onCreate()
        DynamicColors.applyToActivitiesIfAvailable(this@MainApplication)
    }

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        Reflection.unseal(base)
    }
}