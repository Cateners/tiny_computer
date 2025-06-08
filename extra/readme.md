## This readme describes how the files in the assets folder are made.

### Files in assets.zip:

- [busybox](https://github.com/meefik/busybox)
- [mediamtx related](https://github.com/bluenviron/mediamtx)
- [tar](https://github.com/Rprop/tar-android-static)
- [Xserver XSDL, pulseaudio related files](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [virgl](https://github.com/termux/termux-packages/tree/master/x11-packages/virglrenderer-android)

The above files have not been modified.
Generally, binary files directly provided by the repository were used, or they were compiled using patches provided by the repository.

- [proot](https://github.com/Cateners/proot), compiled using the [build-proot-android](https://github.com/green-green-avk/build-proot-android) script
- [Tmoe Linux, debian package source](https://github.com/2moe/tmoe), created [container files xa*](build-tiny-rootfs.md)
- getifaddrs_bridge_server, see the introduction below and the getifaddrs_bridge subfolder

### Files in patch.tar.gz:

#### extra/getifaddrs_bridge_client_lib.so:

On systems Android 13 and above, the proot container does not have permission to use the default getifaddrs, and this library contains a getifaddrs implementation.

When linux needs data, it uses a socket to notify getifaddrs_bridge_server located in Android, letting getifaddrs_bridge_server execute the getifaddrs function, and serialize the structure data before sending it back to the linux side, which then receives the data and deserializes it back into a pointer structure. Simply put, it uses Android's getifaddrs to replace linux's getifaddrs.

Source code and compilation information can be found in the getifaddrs_bridge folder.

#### extra/install-hangover, extra/install-hangover-stable:

These are Hangover installation scripts for Windows application support.

#### extra/chn_fonts.reg:

Registry file to fix wine displaying square characters.

#### extra/libvulkan_freedreno.so, extra/freedreno_icd.aarch64.json:

Turnip driver. Compiled according to [here](https://github.com/xDoge26/proot-setup/issues/26#issuecomment-1712404849) and [here](https://github.com/MastaG/mesa-turnip-ppa)

#### extra/cmatrix

Easter egg for shortcut command. Originally placed in the container, but it is clearly more appropriate to put it here.

#### caj, edraw

These are patches for cajviewer and EdrawMax respectively.

- The library files for the EdrawMax patch were compiled after downloading the corresponding Qt version source code on Tiny Computer;
- Compilation was performed twice. The first time, it was compiled directly, yielding the Gui and Widgets libraries. The second time, it was compiled with XcbQpa. Although the compilation failed, the XcbQpa library could be obtained before that.

#### wechat

WeChat patch. license, uos-lsb, and uos-release come from Spark Store's WeChat package or Arch's wechat-uos packaging (well, I forgot which one. But they are pretty much the same).

libssl1.1 comes from the official Debian source. deepin-elf-verifier is an empty package I created.

#### font

[Xiaolai Font](https://github.com/lxgw/kose-font) is used to fix wine's square characters.
Other fonts are used to prevent wps from reporting missing font errors.
