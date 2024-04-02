package com.example.tiny_computer

import com.google.android.material.color.DynamicColors
import io.flutter.app.FlutterApplication

class MainApplication : FlutterApplication() {

    override fun onCreate() {
        super.onCreate()
        DynamicColors.applyToActivitiesIfAvailable(this@MainApplication)
    }
}