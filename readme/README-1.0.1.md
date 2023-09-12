这个文件夹存放过时的readme

# 小小电脑

<img decoding="async" src="cover0.png" width="50%">

点开软件就是电脑

Click-to-run debian 12 xfce on android for Chinese users, with fcitx pinyin input method and wps office preinstalled. No termux required.

## 原理

使用proot运行debian环境

内置[noVNC](https://github.com/novnc/noVNC)显示图形界面

初次启动由于解压的缘故要点时间
以后点开就能用

只支持arm64安卓

**目前新安装的软件无法读写文件，但可以访问手机存储，原因未知**

（我接下来可能会排查一下是proot还是容器的问题
顺便学习一下容器是怎么做的
毕竟我的修改可能出了问题）

## 项目结构

assets的文件来源如下:

- [build-proot-android, proot二进制文件](https://github.com/green-green-avk/build-proot-android)
- [busybox](https://github.com/meefik/busybox)
- [Xserver XSDL, pulseaudio相关文件](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [Tmoe Linux, debian包来源](https://github.com/2moe/tmoe)

其中proot、busybox和pulseaudio相关文件都是直接用了二进制文件。

（pulseaudio我真的编译不来，如果你会的话请教教我吧）

对debian容器进行了如下修改：
- 使用tmoe工具安装了xfce环境和全套VNC；
- 安装了wps office, 对wps office进行了如下修改：
  - 界面改成了多组件，避免无法打开wps；
  - 根据[这篇文章](https://forums.debiancn.org/t/topic/4015/8)创建了libtiff软链，避免无法打开wpspdf
  - 补上了缺失的字体；
- 安装了VS Code和中文插件；
- 安装了fcitx输入法和云拼音组件。按<Ctrl+空格>切换输入法。
  - 强烈建议**不要**使用安卓中文输入法直接输入中文，而是使用英文键盘通过容器的输入法输入中文，避免丢字错字。
- 对VNC启动脚本进行修改，删除了tigerVNC密码验证；
  - 虽然不太可能，但如果还是被问到密码的话输12345678
- 对noVNC脚本(/usr/local/etc/tmoe-linux/novnc/core/rfb.js)进行修改，添加了userScale变量控制缩放
  - 默认显示太大了，很多窗口点开都超出了屏幕范围，目前我使显示缩小了userScale=1.5倍
- 改掉了一些容器里的Termux硬链接，有一些.git文件夹里的没改，应该无伤大雅吧=v=
- 最后采用tar.xz压缩，用split命令分成了xa*等多个文件

lib目录：

- main.dart文件，页面布局，目前只有一个页面，非常简单
- workflow.dart文件，逻辑部分，目前也还算简单
  - Util 工具类
  - G 全局变量类
  - Workflow 从软件点开到容器启动的所有步骤

## 一些链接

这是我的第一个flutter软件，感谢这些项目为我指路

- 要一点基础的 [《Flutter实战·第二版》](https://book.flutterchina.club)
- 也许是零基础的Flutter视频课程 [freeCodeCamp Flutter Course](https://www.youtube.com/watch?v=wFn-m-OgKPU&list=PL6yRaaP0WPkVtoeNIGqILtRAgd3h2CNpT)

- 安卓上的VS Code [Code FA](https://github.com/nightmare-space/vscode_for_android)

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.
