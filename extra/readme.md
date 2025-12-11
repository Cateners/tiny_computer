## 这个readme介绍assets文件夹中文件的制作方式。

### assets.zip中的文件：

- [Xserver XSDL, pulseaudio相关文件](https://github.com/pelya/commandergenius/tree/sdl_android/project/jni/application/xserver)。直接从Xserver XSDL的apk中的lib解包获得，并还原了名称。
- [Tmoe Linux, debian包来源](https://github.com/2moe/tmoe)，制作了[容器文件xa*](build-tiny-rootfs.md)
- getifaddrs_bridge_server，见下面的介绍和getifaddrs_bridge子文件夹

### jniLibs中的文件

除libexec_pulseaudio.so(pulseaudio可执行文件)来自Xserver XSDL的apk外，所有文件均通过[termux-packages](https://github.com/termux-play-store/termux-packages)构建。[见这个修改后的仓库](https://github.com/tiny-computer/termux-packages)

运行scripts/generate-bootstraps.sh即可获得bootstraps压缩包，其中会包含busybox、proot、tar、virglrenderer的可执行文件和依赖库。将可执行文件全部重命名为libexec_xxx.so的格式，将依赖库全部抹去版本号，放到jniLibs/arm64-v8a。

运行build-package.sh proot，可在output文件夹找到loader和loader32，重命名为libproot-loader.so和libproot-loader32.so，放到jniLibs/arm64-v8a。


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

#### font

[小赖字体](https://github.com/lxgw/kose-font)用于修复wine的方块字
其他字体用于避免wps报字体缺失的错误