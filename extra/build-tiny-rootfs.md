# build-tiny-rootfs
 对小小电脑项目rootfs制作的说明

### 为什么不直接制作一个脚本呢？

因为我不会，所以只能用自然语言记录一下制作步骤。

## 制作步骤（xfce和lxqt）

### 安装Debian容器

- 安装Termux
- 在Termux内安装tmoe
- 在tmoe内安装Debian Bookworm的proot容器
  - 是否新建sudo用户-是-用户名tiny-密码tiny
  - 是否设置tiny为默认用户-是
  - 是否为root配置zsh-否
  - 是否删除zsh.sh等-是
  - 是否启动tmoe tools-是
  - 其余对话框默认直接按回车
  - 来到tmoe tool界面时取消，退出

### 安装其他软件

安装xfce部分是根据记忆写的，如果有误请指出。

桌面环境只安装一个。

#### 安装桌面环境(lxqt)

- 输入debian-i进入tmoe tools
- 图形界面-rootless-lxqt-core
- 不安装electron apps
- 不安装chromium

- 按需调整

#### 安装桌面环境(xfce)

前面的部分和lxqt一致，只是选桌面环境时选了xfce-lite。

下面是额外的美化部分。推荐先安装软件再做这个，因为使用kali-undercover时可能有依赖报错，但我忘记是哪些依赖了。但后面安装的某个软件会帮我们把依赖补上。

- xfce美化
  - 前往kali源下载kali-undercover包并apt install安装
  - 修改kali-undercover脚本中检测xfce环境的地方，强制允许
    - 即注释第一个if里的exit 1
  - 执行kali-undercover
  - 按需调整
    - 注释.bashrc中把bash风格改为windows风格的语句
    - 调整状态栏
    - ......

#### 安装VNC

安装桌面环境后会自动进行这一步，使用tmoe tools全部安装即可。

- 选择tigervnc
- 密码12345678

安装完成后，输入debian-i回到tmoe继续修改一些参数，主要目的是避免与termux的容器端口一致产生冲突

- 修改显示端口到5904
  - 远程桌面-tigervnc-显示端口-4
- 修改novnc端口到36082
  - 远程桌面-novnc-端口-36082
- 修改startnovnc启动脚本(避免每次启动novnc时打开浏览器，虽然不是windows)
  - 注释start_win10_edge_novnc_addr(大概在倒数第五行)

接下来对novnc应用补丁，以添加"通过滑块修改分辨率"等功能

- [下载novnc.patch](https://github.com/Cateners/noVNC/releases/tag/1.2)
- 切换目录到/usr/local/etc/tmoe-linux/novnc
- `patch -p1 < novnc.patch`
- ```bash
    find . '(' \
    -name \*-baseline -o \
    -name \*-merge -o \
    -name \*-original -o \
    -name \*.orig -o \
    -name \*.rej \
    ')' -delete
    ```


#### 修复tmoe不能下载软件

在我发布的xfce版本中，我给每个aria2c调用都添加了--async-dns=false参数。

先切换到tmoe目录`/usr/local/etc/tmoe-linux/git/share`，然后执行脚本`./replace.sh old-version`：
```shell
#!/bin/bash
# 用法: ./replace.sh 目录
# 该脚本会递归地在给定目录下的所有文件中替换文本
# 原文本: aria2c --console-log-level
# 新文本: aria2c --async-dns=false --console-log-level

# 检查参数是否正确
if [ $# -ne 1 ]; then
  echo "错误: 需要一个目录作为参数"
  exit 1
fi

# 检查目录是否存在
if [ ! -d "$1" ]; then
  echo "错误: 目录 $1 不存在"
  exit 2
fi

# 遍历目录下的所有文件
find "$1" -type f | while read file; do
  # 使用sed命令替换文本
  sed -i 's/aria2c --console-log-level/aria2c --async-dns=false --console-log-level/g' "$file"
  echo
done
```

用完后删除replace.sh；

另外现在tmoe官方给出了[解决办法](https://gitee.com/mo2/linux/issues/I8BQG3)，不过我测试似乎还是不行，所以就先这样了

#### 修改apt源

按需修改/etc/apt/sources.list，另外把non-free改为non-free-firmware

#### 安装火狐浏览器

`sudo apt install firefox-esr firefox-esr-l10n-zh-cn`

#### 安装输入法

- debian-i
- 03秘密花园-10输入法-fcitx4-安装4libpinyin和6云拼音模块
- 在图形界面应用找到fcitx配置-附加组件-云拼音-配置-云拼音来源，把Google改为百度，确认
  - 启动图形界面：输入startnovnc，会出现一个类似xxx.xxx.xxx.xxx:36082/vnc.html的网址，复制到本机的浏览器中输入vnc密码12345678就可以访问了。

#### 安装gdebi

这个软件包能使用户通过图形界面安装deb安装包

安装：`sudo apt install gdebi`

修改启动器：在/usr/share/applications/gdebi.desktop的Exec=后加上sudo

#### 安装VSCode

VSCode使用tmoe安装，正好测试一下不能下载软件的问题是否存在

- 2软件-2开发-1VSCode-1Official

tmoe还会安装gnome-keyring，由于之前我做xfce包时会造成VSCode反复弹窗更新密钥环所以被我卸载了，这个按需决定是否保留吧

#### 安装ffmpeg

这个是为了预览推流用的，按需安装

`sudo apt install ffmpeg`

### 其他修补

#### cmatrix

这个是给快捷指令的彩蛋。下载cmatrix的包，并将cmatrix文件提取放到/home/tiny/.local/bin里即可，记得添加执行权限

#### WPS

**（20241112）注意，新版wps不再需要将整合模式改为多组件模式也能正常使用，所以可以跳过软件设置修改的步骤**

- 软件设置修改
  - 从官网下载WPS linux arm64 deb安装包，直接在图形界面点开用gdebi安装(正好测试一下gdebi是否能用)
  - 打开WPS-右上角设置-其他-切换窗口管理模式-整合模式改为多组件模式(否则一些设备在新建文档等操作时卡死，目前原因不明)
  - 使用gdebi(或自行)卸载WPS
- libtiff.so.5库修补
  - 切换到/lib/aarch64-linux-gnu文件夹，创建软链把libtiff.so.6链接到libtiff.so.5
  - 或者找libtiff.so.5的包并安装，这样可能更好一些
- 预装ttf-mscorefonts-installer
  - 这个包是WPS的依赖，会在sourceforge下载字体，可能会非常慢，所以提前apt装好


### 额外步骤

- 将桌面壁纸的配置从monitorBuiltinDisplay改为monitorbuiltin（xfce，v1.0.99），疑似Termux:X11显示器名称改变导致壁纸失效
- 修复用vscode打开文件时只打开了vscode本身（v1.0.25）：去掉/usr/share/applications/code-no-sandbox.desktop的Exec的--unity-launch
- 修复选择文本时会把文本发送到剪切板（v1.0.25）：在/usr/local/bin/startvnc文件的start_tmoe_xvnc()的start_win10_tigervnc行前面加入`vncconfig -set SendPrimary=0 SetPrimary=0`
- 修复系统更新时变英文（v1.0.19）：把/etc/locale.gen文件里包含zh_CN.UTF-8的那行代码解除注释
- 修复了xfce使用Termux:X11时占用过高（v1.0.19）：把底部面板的电量管理插件移除（右键-面板-面板首选项-项目）
- 不弹出终端窗口（v1.0.18）：把/etc/X11/xinit/Xsession文件倒数第二行open_terminal删掉
- 关闭垂直同步以使用Turnip+Zink（v1.0.17）：把文件~/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml内vblank_mode值从auto改为off
- xfce版本安装了图片查看器ristretto和压缩文件管理器xarchiver（v1.0.16）

### 打包

- 首先退出容器，在容器挂载选项里取消对sd和termux的挂载，之后进入容器删除termux软连接
  - 在后面使用tar打包时，即使指定了exclude，tar也会尝试把它们打包进去
  - 这个很可能因为我自己没用对参数，如果你非常自信的话就不需要这么做，自行打包即可=v=
- 在[这里](https://github.com/meefik/busybox/releases)下载提取busybox的可执行文件，并放到系统根目录
  - 我使用busybox的tar来打包，而不是容器自带的tar，原因是容器自带的tar会把硬链接打包成单独的文件，导致打包解包后占用多出1GB
  - 这个也很可能是我自己没用对参数，如果你非常自信就不用这么做......
- 尽可能多地删除使用痕迹，包括但不限于
  - apt clean
  - /tmp下的文件，退出容器后删
  - tiny和root目录下的
    - .cache
    - .vnc/vnc.log, .vnc/x.log
    - .bash_history
    - .ICEauthority
    - .Xauthority
    - 等等
- 切换到root用户，切换到根目录，`/busybox tar -Jcpvf /debian.tar.xz --exclude=".l2s.*" bin boot etc home lib media mnt opt root run sbin sd srv tmp usr var`


## 制作步骤（GXDE OS）

### 咕咕咕

其实流程和前面差不多。基本上就是 装图形界面->修复中文->修复tmoe->修non-free-firmware->（随便看看空间占用，略）->修wps->准备busybox以便打包->添加Xsession文件以便启动

请看VCR：
```
    1  exit
    2  sudo apt install sd/Download/gxde-source_1.0.1_all.deb 
    3  sudo apt install ./sd/Download/gxde-source_1.0.1_all.deb 
    4  sudo apt update
    5  sudo apt install gxde-testing-source
    6  sudo apt update
    7  sudo apt install gxde-desktop-android --no-install-recommends
    8  sudo apt install spark-store gxde-terminal
    9  nano /etc/locale.gen
   10  cd /usr/local/etc/tmoe-linux/git/share
   11  nano replace.sh
   12  ./replace.sh old-version
   13  chmod +x replace.sh 
   14  ./replace.sh old-version
   15  rm replace.sh 
   16  cd
   17  tmoe
   18  nano /etc/apt/sources.list
   19  sudo apt update
   20  nano /etc/apt/sources.list
   21  sudo apt update
   22  cd /var/log
   23  ls -l
   24  du -h --max-depth=1 | sort -h
   25  cd ..
   26  du -h --max-depth=1 | sort -h
   27  cd cache/
   28  ls -l
   29  sudo apt update ttf-mscorefonts-installer
   30  sudo apt install ttf-mscorefonts-installer
   31  cd /usr/lib/aarch64-linux-gnu/
   32  ln -s libtiff.so.6 libtiff.so.5
   33  history
   34  cd /
   35  cp home/tiny/termux/home/.local/share/tmoe-linux/containers/proot/debian-bookworm_arm64/busybox .
   36  cd /etc/X11/xinit/
   37  ls
   38  cp ~/termux/home/.local/share/tmoe-linux/containers/proot/debian-bookworm_arm64/etc/X11/xinit/Xsession .
   39  ls -l Xsession 
   40  cd /
   41  ls -l busybox 
   42  exit
   43  sudo apt clean;sudo apt autoclean;sudo apt autoremove --purge || sudo apt autoremove
   44  history
   45  history > /sd/history.txt
```

关于Xsession文件：

因为当前小小电脑代码写死了启动X11图形界面就通过执行/etc/X11/xinit/Xsession，如果通过tmoe安装图形界面这个文件是自带的，但安装GXDE没有通过tmoe，所以随便写了个：
```
rm -rf /run/dbus/pid
sudo dbus-daemon --system
export $(dbus-launch)
startgxde_android
```
