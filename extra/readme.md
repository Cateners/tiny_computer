## 这个readme介绍assets文件夹中文件的制作方式。

### assets.zip中的文件：

- [busybox](https://github.com/meefik/busybox)
- [mediamtx相关](https://github.com/bluenviron/mediamtx)
- [tar](https://github.com/Rprop/tar-android-static)
- [Xserver XSDL, pulseaudio相关文件](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)
- [virgl](https://github.com/termux/termux-packages/tree/master/x11-packages/virglrenderer-android)

以上文件没有经过更改。
一般是使用了仓库直接提供的二进制文件，或者是使用了仓库提供的patch编译而来。

- [proot](https://github.com/Cateners/proot), 使用[build-proot-android](https://github.com/green-green-avk/build-proot-android)脚本编译
- [Tmoe Linux, debian包来源](https://github.com/2moe/tmoe)，制作了[容器文件xa*](build-tiny-rootfs.md)
- getifaddrs_bridge_server，见下面的介绍和getifaddrs_bridge子文件夹

### patch.tar.gz中的文件：

#### extra/getifaddrs_bridge_client_lib.so:

在安卓13以上的系统中，proot容器无权使用默认的getifaddrs，而这个库包含了一个getifaddrs实现。

linux在需要数据时，使用socket通知位于安卓的getifaddrs_bridge_server，让getifaddrs_bridge_server执行getifaddrs函数，并将结构体数据序列化后发送回linux端，这边接收数据并反序列化还原成指针结构体。简单来说就是用安卓的getifaddrs代替linux的getifaddrs。

源码和编译信息在getifaddrs_bridge文件夹查看。

#### extra/install-hangover, extra/install-hangover-stable:

这些是用于Windows应用支持的Hangover安装脚本。

#### extra/chn_fonts.reg:

修复wine显示方块字的注册表文件。

#### extra/libvulkan_freedreno.so, extra/freedreno_icd.aarch64.json:

Turnip驱动。根据[这里](https://github.com/xDoge26/proot-setup/issues/26#issuecomment-1712404849)和[这里](https://github.com/MastaG/mesa-turnip-ppa)编译

#### extra/cmatrix

快捷指令的彩蛋。原本放在容器里，但显然放这里更为合适

#### caj, edraw

这些分别是cajviewer，亿图图示的补丁

- 亿图图示补丁的库文件是在小小电脑上下载了Qt对应版本源码后编译得到的；
- 编译进行了两次，第一次直接编译，可以得到Gui和Widgets两个库。第二次编译带上XcbQpa，虽然会编译失败，但在这之前就可以得到XcbQpa的库。

#### wechat

微信的补丁。license, uos-lsb和uos-release来自星火的微信包或arch的wechat-uos打包（嗯，我忘记到底是哪的了。不过都差不多）。

libssl1.1来自debian官方源。deepin-elf-verifier是我打的空包。

#### font

[小赖字体](https://github.com/lxgw/kose-font)用于修复wine的方块字
其他字体用于避免wps报字体缺失的错误