# 小小电脑

<img decoding="async" src="readme/cover0.png" width="50%">

即开即用的类PC环境，内置火狐浏览器和fcitx输入法等常用软件

Click-to-run debian bookworm xfce on android for Chinese users, with fcitx pinyin input method preinstalled. No termux required.

## 原理

使用proot运行debian环境

内置[noVNC](https://github.com/novnc/noVNC)显示图形界面

初次启动由于解压的缘故要点时间
以后点开就能用

只支持arm64安卓

## 项目结构

assets的文件来源如下:

- [proot](https://github.com/termux/proot/), 使用[build-proot-android](https://github.com/green-green-avk/build-proot-android)脚本编译
- [busybox](https://github.com/meefik/busybox)
- [Xserver XSDL, pulseaudio相关文件](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [Tmoe Linux, debian包来源](https://github.com/2moe/tmoe)

其中busybox和pulseaudio相关文件都是直接用了二进制文件。

（pulseaudio我真的编译不来，如果你会的话请教教我吧）

对debian容器进行了如下修改：
- 使用tmoe安装了xfce环境和全套VNC；
- 使用kali-undercover提供的Win10主题美化xfce；
- (使用tmoe)安装了fcitx输入法和云拼音组件。按<Ctrl+空格>切换输入法。
  - 强烈建议**不要**使用安卓中文输入法直接输入中文，而是使用英文键盘通过容器的输入法输入中文，避免丢字错字。
- 对noVNC进行[修改](https://github.com/Cateners/noVNC) (scale_factor分支)，添加了scale factor滑块控制缩放，添加了上下左右shift等按键
- 在主目录下可以方便地访问手机存储(如果提供了存储权限的话)
- 启动时会尝试挂载手机的一些字体目录(AppFiles/Fonts、Fonts和/system/fonts), 如果这些目录下有字体文件的话会一并加载到系统中，无需额外安装
- 最后采用tar.xz压缩，用split命令分成了xa*等多个文件

数据包不再在assets中更新，而是随releases提供，主要是为了避免git越来越大

lib目录：

- main.dart文件，页面布局，老实说已经有点乱了
- workflow.dart文件，逻辑部分，目前也还可以理解
  - Util 工具类
  - TermPty 一个终端
  - G 全局变量类
  - Workflow 从软件点开到容器启动的所有步骤

## 目前已知bug

多用户/分身情形无法sudo, 其它见issue

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
