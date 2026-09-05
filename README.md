[中文版](README.zh.md)

# Tiny Container

> [!NOTE]
> Linux is powerful. Linux can also be easy to use.

### Install Tiny Container and instantly get a Linux computer — run PC software right on your phone or tablet!

![Preview image](doc/tc4.png)

## Features

### Designed for ordinary users. You don't need to know Linux...

1. Install the app and open it — no manual steps required*. After a 5-minute initialization, you'll be at the desktop.
2. Installation commands for common software are already prepared for you. Just tap, and the app handles the rest.
3. The UI is designed to be as friendly as possible, not just "good enough to have a UI". AI-powered translations are available in many languages. Even RTL layout is supported!
    - (Admittedly, I don't use RTL myself and can't verify the AI translations, so no guarantees on accuracy! But better to have it than not, right?)

### Also a toy for geeks — built-in terminal, rich container configuration options!

4. Containers and configurations can be freely shared!
5. Cutting-edge features from the Termux community are ready to go! No need to learn how to install containers, start graphical sessions, configure audio, and all that tedious stuff. The following features work out of the box:
    - Import containers via the import button, with graphical session startup included (if supported);
    - Built-in AVNC and Termux:X11 frontends — no extra app installation needed;
    - Audio and microphone forwarding;
    - virglrenderer and turnip+zink graphics acceleration...
6. Features only possible in a single app!
    - Audio and VNC transmitted over unix sockets, bypassing the network stack;
    - Browse container files via the SAF file manager;
    - Add .desktop files or even arbitrary commands as shortcuts to your Android launcher!
7. Won't conflict with Termux!

The goal of this project is to let ordinary users enjoy Linux as effortlessly as possible.  
Termux is widely known as a treasure trove for developers, massively expanding what your device can do.  
Yet few people outside the developer community realize the potential sitting in their devices, so I hope this project lowers the barrier to entry.  

> [!CAUTION]  
> This project makes heavy use of AI. Given the level of completion, it's kind of unavoidable... isn't it? ^_^  
> This includes code logic, layouts, translations, and more. There may be unexpected bugs in some places. Please be aware.

What I won't do: 
- Support chroot containers. For the first three years of this project, people occasionally asked me to support it, usually citing "better performance".  
  While that's true in theory, no one has ever provided actual benchmark data showing under what scenarios proot's slowness becomes a performance bottleneck. Since I have no experience developing apps that use root (and managing root seems like a huge hassle), I won't be supporting chroot.  
  In short, if you really need it, use Termux! If you need automated setup, try Kali Nethunter or similar.
- Support Mali GPU acceleration; Wayland; Docker, systemd; GNOME; some specific distro; camera, serial port, etc... (^-^*)  
  No wish-listing please! If the community comes up with solutions, I may add them in.  
  As for other distros, you can actually check out the [images repo](https://github.com/tiny-computer/images) and give it a try yourself.

## Download

APKs are available on the [releases](https://github.com/Cateners/tiny_container/releases) page.

## Build

After cloning the repository, download the prebuilt library jniLibs.zip from the releases page and extract it to app/src/main/jniLibs/arm64-v8a. Then you can open and build the project normally in Android Studio.  
For automatic container installation on first launch, rename your container to rootfs.tar.zst and place it in app/src/main/assets.  
For container information, including how to make your own containers and some pre-built containers I've already prepared, check out [this](https://github.com/tiny-computer/images) repository.

The jniLibs.zip prebuilt libraries come from my fork of [termux-packages](https://github.com/tiny-computer/termux-packages).  
My main modifications there are:
- scripts/generate-bootstraps.sh, to directly generate the required packages. I manually renamed the needed files to .so and packaged them. You should be able to easily see which .so files correspond to which files in the bootstraps.
- packages/proot/build.sh, to use my fork of [proot](https://github.com/tiny-computer/proot-termux) (the changes there are a bit messy, nothing worth looking at).

Other files may have minor modifications too, but those are usually just to get things compiling cleanly or to remove some Termux hard-coded paths.

## Documentation?

Not ready yet — please wait just a little bit longer...

- If the screen appears too large or too small, adjust the display scaling in Features -> AVNC.
- Android storage is mounted at /mnt/sdcard by default. Don't forget to grant storage access permission.
- If a Linux app can't detect the microphone, check that the microphone is selected (Tiny Microphone Input), or try restarting that Linux app.

## What about the old code?

The Tiny Computer code is preserved on the v1_tiny_computer branch.
Tiny Container is a complete rewrite of Tiny Computer, and I likely won't maintain Tiny Computer going forward. So, use Tiny Container!

## Acknowledgments
Thanks to the [termux](https://github.com/termux) community for porting so many excellent programs to Android and providing countless solutions for container configuration scenarios.  
Thanks to the [tmoe](https://github.com/2moe/tmoe) project for making container configuration easier. Although Tiny Container doesn't directly use tmoe this time, some scripts still bear its influence. tmoe was my introduction to Linux.  
Thanks to the [avnc](https://github.com/gujjwal00/avnc) and [termux-x11](https://github.com/termux/termux-x11) projects — the Linux graphical interfaces used by Tiny Container.  
Thanks to vinceliuice's [Fluent](https://github.com/vinceliuice/Fluent-gtk-theme) project — the default theme used by the Xfce container.  
Thanks to lxgw's [Xiaolai Font](https://github.com/lxgw/kose-font) project — the default font used by containers.  
Thanks to the [debian](https://www.debian.org/) project and all the open-source maintainers who form the foundation of the containers! All containers currently used in this project are based on Debian.  
Thanks to all Tiny Computer (and Tiny Container) contributors, users, and everyone who believes in this project.
