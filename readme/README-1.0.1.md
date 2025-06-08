This folder contains outdated readmes

# Tiny Computer

<img decoding="async" src="cover0.png" width="50%">

Open the software and it's a computer

Click-to-run debian 12 xfce on android for Chinese users, with fcitx pinyin input method and wps office preinstalled. No termux required.

## Principle

Uses proot to run the debian environment

Built-in [noVNC](https://github.com/novnc/noVNC) to display the graphical interface

The first startup takes some time due to decompression
After that, you can use it by clicking on it

Only supports arm64 Android

**Currently, newly installed software cannot read or write files, but can access phone storage. The reason is unknown.**

(I might investigate whether it's a proot or container issue next.
By the way, I'll learn how containers are made.
After all, my modifications might have caused problems.)

## Project Structure

The sources of the files in assets are as follows:

- [build-proot-android, proot binary file](https://github.com/green-green-avk/build-proot-android)
- [busybox](https://github.com/meefik/busybox)
- [Xserver XSDL, pulseaudio related files](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [Tmoe Linux, debian package source](https://github.com/2moe/tmoe)

Among them, proot, busybox, and pulseaudio related files are all directly used binary files.

(I really can't compile pulseaudio. If you know how, please teach me.)

The following modifications were made to the debian container:
- Installed xfce environment and full VNC using tmoe tool;
- Installed wps office, and made the following modifications to wps office:
  - The interface was changed to multiple components to avoid being unable to open wps;
  - Created a libtiff soft link according to [this article](https://forums.debiancn.org/t/topic/4015/8) to avoid being unable to open wpspdf
  - Added missing fonts;
- Installed VS Code and Chinese plugins;
- Installed fcitx input method and cloud pinyin components. Press <Ctrl+Space> to switch input methods.
  - It is strongly recommended **not** to use the Android Chinese input method to directly input Chinese, but to use an English keyboard to input Chinese through the container's input method to avoid missing or incorrect characters.
- Modified the VNC startup script to remove tigerVNC password verification;
  - Although it is unlikely, if you are still asked for a password, enter 12345678
- Modified the noVNC script (/usr/local/etc/tmoe-linux/novnc/core/rfb.js) to add the userScale variable to control scaling
  - The default display was too large, and many windows opened beyond the screen range. Currently, I have reduced the display by userScale=1.5 times
- Changed some Termux hard links in the container. Some in the .git folder were not changed, which should be harmless =v=
- Finally, it was compressed with tar.xz and split into multiple files like xa* using the split command

lib directory:

- main.dart file, page layout, currently only one page, very simple
- workflow.dart file, logic part, currently relatively simple
  - Util utility class
  - G global variable class
  - Workflow All steps from opening the software to starting the container

## Some Links

This is my first flutter software, thanks to these projects for guiding me

- Requires some basics [《Flutter实战·第二版》](https://book.flutterchina.club)
- Perhaps a zero-based Flutter video course [freeCodeCamp Flutter Course](https://www.youtube.com/watch?v=wFn-m-OgKPU&list=PL6yRaaP0WPkVtoeNIGqILtRAgd3h2CNpT)

- VS Code on Android [Code FA](https://github.com/nightmare-space/vscode_for_android)

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.
