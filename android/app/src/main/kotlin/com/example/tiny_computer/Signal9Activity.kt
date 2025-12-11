package com.example.tiny_computer

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import com.example.tiny_computer.databinding.ActivitySignal9Binding

class Signal9Activity : AppCompatActivity() {

    private lateinit var binding: ActivitySignal9Binding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySignal9Binding.inflate(layoutInflater)
        setContentView(binding.root)
        
        // 设置状态栏和导航栏颜色匹配蓝屏背景
        window.statusBarColor = ContextCompat.getColor(this, R.color.tc_s9a_blue_screen_blue)
        window.navigationBarColor = ContextCompat.getColor(this, R.color.tc_s9a_blue_screen_blue)
        
        setupContent()
    }

    private fun setupContent() {
        // 设置错误信息
        binding.errorDetails.text = getString(R.string.tc_s9a_error_message)
        
        // 根据Android版本显示不同的解决方案
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            // Android 14以下版本
            binding.preAndroid14Layout.isVisible = true
            binding.solutionIntro.text = getString(R.string.tc_s9a_solution_intro)
            binding.solutionAlternative.text = getString(R.string.tc_s9a_solution_alternative)
            binding.toolButton.text = getString(R.string.tc_s9a_tool_button)
            binding.tutorialButton.text = getString(R.string.tc_s9a_tutorial_button)

            binding.toolButton.setOnClickListener {
                openBrowserLink("https://www.vmos.cn/zhushou.htm")
            }

            binding.tutorialButton.setOnClickListener {
                openBrowserLink("https://gitee.com/caten/tc-hints/blob/master/pool/signal9fix.md")
            }
        } else {
            // Android 14及以上版本
            binding.solutionAndroid14.isVisible = true
            binding.solutionAndroid14.text = getString(R.string.tc_s9a_solution_android14)
        }
    }

    private fun openBrowserLink(url: String) {
        if (url.isNotEmpty()) {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
            startActivity(intent)
        }
        // 如果URL为空，则不执行任何操作（等待后续补充链接）
    }
}