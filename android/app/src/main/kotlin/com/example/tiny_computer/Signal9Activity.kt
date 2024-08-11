package com.example.tiny_computer

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat.startActivity

class Signal9Activity : AppCompatActivity() {

    private val helperLink = "https://www.vmos.cn/zhushou.htm"
    private val helperLink2 = "https://b23.tv/WwqOqW6"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val rootLayout = LinearLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
            setBackgroundColor(Color.parseColor("#4A148C"))
        }

        val scrollView = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        }

        val fullScreen = LinearLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#4A148C"))
        }

        val text1 = TextView(this).apply {
            text = ":(\n发生了什么？"
            textSize = 32f
            setTextColor(Color.WHITE)
            textAlignment = View.TEXT_ALIGNMENT_CENTER
        }

        val text2 = TextView(this).apply {
            text = "终端异常退出, 返回错误码9\n此错误通常是高版本安卓系统(12+)限制进程造成的, \n可以使用以下工具修复:"
            textSize = 16f
            setTextColor(Color.WHITE)
            textAlignment = View.TEXT_ALIGNMENT_CENTER
            setPadding(0, 16, 0, 0)
        }

        val helperLinkText = TextView(this).apply {
            text = helperLink
            textSize = 16f
            setTextColor(Color.WHITE)
            textAlignment = View.TEXT_ALIGNMENT_CENTER
            setPadding(0, 16, 0, 0)
            setOnClickListener { copyToClipboard(helperLink) }
        }

        val copyHintText = TextView(this).apply {
            text = "(复制链接到浏览器查看)"
            textSize = 16f
            setTextColor(Color.WHITE)
            textAlignment = View.TEXT_ALIGNMENT_CENTER
            setPadding(0, 8, 0, 0)
        }

        val copyButton = Button(this).apply {
            text = "复制"
            textSize = 16f
            setOnClickListener { copyToClipboard(helperLink) }
        }

        val tutorialText = TextView(this).apply {
            text = "如果你的设备版本大于等于安卓14，可以在开发者选项里开启“停止限制子进程”选项即可，无需额外修复。\n\n如果不能解决请参考此教程: "
            textSize = 16f
            setTextColor(Color.WHITE)
            textAlignment = View.TEXT_ALIGNMENT_CENTER
            setPadding(0, 16, 0, 0)
        }

        val viewButton = Button(this).apply {
            text = "查看"
            textSize = 16f
            setOnClickListener { copyToClipboard(helperLink2) }
        }

        rootLayout.addView(text1)
        rootLayout.addView(text2)
        rootLayout.addView(helperLinkText)
        rootLayout.addView(copyHintText)
        rootLayout.addView(copyButton)
        rootLayout.addView(tutorialText)
        rootLayout.addView(viewButton)

        scrollView.addView(rootLayout)
        fullScreen.addView(scrollView)

        setContentView(fullScreen)
    }

    private fun copyToClipboard(text: String) {
        val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("Copied Text", text)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(this, "已复制", Toast.LENGTH_SHORT).show()
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(text))
        startActivity(this, intent, null)
    }
}
