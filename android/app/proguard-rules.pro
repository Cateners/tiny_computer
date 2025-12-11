# Please add these rules to your existing keep rules in order to suppress warnings.
# This is generated automatically by the Android Gradle plugin.
-dontwarn android.app.ActivityThread
-dontwarn android.app.ContextImpl
-dontwarn android.app.IActivityManager
-dontwarn android.content.IIntentReceiver$Stub
-dontwarn android.content.IIntentReceiver
-dontwarn android.content.IIntentSender
-dontwarn android.content.pm.IPackageManager
-dontwarn com.google.errorprone.annotations.CanIgnoreReturnValue
-dontwarn com.google.errorprone.annotations.Immutable
# 保持 Termux X11 所有内容
-keep class com.termux.x11.** { *; }
-keepclassmembers class com.termux.x11.** { *; }