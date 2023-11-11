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

- [proot](https://github.com/Cateners/proot), 使用[build-proot-android](https://github.com/green-green-avk/build-proot-android)脚本编译
- [busybox](https://github.com/meefik/busybox)
- [mediamtx相关](https://github.com/bluenviron/mediamtx)
- [tar](https://github.com/Rprop/tar-android-static)
- [Xserver XSDL, pulseaudio相关文件](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [Tmoe Linux, debian包来源](https://github.com/2moe/tmoe)

其中tar、busybox和pulseaudio相关文件都是直接用了二进制文件。

更多信息可以在[这里](extra/readme)找到。

对debian容器进行了如下修改：
- 使用tmoe安装了xfce环境和全套VNC；
- 使用kali-undercover提供的Win10主题美化xfce；
- (使用tmoe)安装了fcitx输入法和云拼音组件。按<Ctrl+空格>切换输入法。
  - 强烈建议**不要**使用安卓中文输入法直接输入中文，而是使用英文键盘通过容器的输入法输入中文，避免丢字错字。
- 对noVNC进行[修改](https://github.com/Cateners/noVNC)，添加了scale factor滑块控制缩放(scale_factor分支)，添加了上下左右shift等按键(arrow_key分支)，添加了强制显示原系统光标的功能(force_cursor分支)，添加了中文翻译(translation_zh_cn分支)；
- 在主目录下可以方便地访问手机存储(如果提供了存储权限的话)；
- 启动时会尝试挂载手机的一些字体目录(AppFiles/Fonts、Fonts和/system/fonts), 如果这些目录下有字体文件的话会一并加载到系统中，无需额外安装；
- 最后采用tar.xz压缩，用split命令分成了xa*等多个文件(低内存设备一次性拷贝大文件会导致软件闪退)。

完整的容器制作过程可以在[这里](extra/build-tiny-rootfs.md)看到。

数据包不再在assets中更新，而是随releases提供，主要是为了避免git越来越大

lib目录：

- main.dart文件，页面布局，老实说已经有点乱了
- workflow.dart文件，逻辑部分，目前也还可以理解
  - Util 工具类
  - TermPty 一个终端
  - G 全局变量类
  - Workflow 从软件点开到容器启动的所有步骤

## 编译

你需要配置好flutter和安卓sdk，然后克隆此项目。

在编译之前，需要在release中下载系统rootfs(或者[自行制作](extra/build-tiny-rootfs.md))，之后使用split命令分割，拷贝到assets。一般我将其分为98MB。

`split -b 98M debian.tar.xz`

然后修改workflow的代码，找到复制资源的部分，把生成的xa\*名字写进去(我还不知道怎么写代码识别有多少个xa*文件)

接下来就可以编译了。我使用的命令如下：

`flutter build apk --target-platform android-arm64 --split-per-abi --obfuscate  --split-debug-info=tiny_computer/sdi`

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
