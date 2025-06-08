# build-tiny-rootfs
Instructions for creating the rootfs for the Tiny Computer project.

### Why not just create a script?

Because I don't know how, I can only record the production steps in natural language.

## Production Steps (xfce and lxqt)

### Install Debian Container

- Install Termux
- Install tmoe in Termux
- Install Debian Bookworm proot container in tmoe
  - Create new sudo user - Yes - Username tiny - Password tiny
  - Set tiny as default user - Yes
  - Configure zsh for root - No
  - Delete zsh.sh etc. - Yes
  - Start tmoe tools - Yes
  - For the remaining dialog boxes, press Enter directly by default
  - When you get to the tmoe tool interface, cancel and exit

### Install Other Software

The xfce installation part is written from memory, please point out any errors.

Only install one desktop environment.

#### Install Desktop Environment (lxqt)

- Enter debian-i to enter tmoe tools
- Graphical Interface - rootless - lxqt-core
- Do not install electron apps
- Do not install chromium

- Adjust as needed

#### Install Desktop Environment (xfce)

The previous part is the same as lxqt, except that xfce-lite was chosen when selecting the desktop environment.

The following is an additional beautification part. It is recommended to install the software before doing this, because there may be dependency errors when using kali-undercover, but I forgot which dependencies they were. However, a certain software installed later will help us supplement the dependencies.

- xfce Beautification
  - Go to the kali source to download the kali-undercover package and install it with apt install
  - Modify the place in the kali-undercover script that detects the xfce environment to force allow it
    - That is, comment out exit 1 in the first if
  - Execute kali-undercover
  - Adjust as needed
    - Comment out the statement in .bashrc that changes the bash style to windows style
    - Adjust status bar
    - ......

#### Install VNC

This step will be performed automatically after installing the desktop environment. Just install everything using tmoe tools.

- Select tigervnc
- Password 12345678

After installation, enter debian-i to return to tmoe to continue modifying some parameters, the main purpose is to avoid conflicts with the termux container port

- Modify display port to 5904
  - Remote Desktop - tigervnc - Display Port - 4
- Modify novnc port to 36082
  - Remote Desktop - novnc - Port - 36082
- Modify startnovnc startup script (to avoid opening the browser every time novnc starts, although it is not windows)
  - Comment out start_win10_edge_novnc_addr (probably in the fifth line from the bottom)

Next, apply a patch to novnc to add functions such as "modify resolution via slider"

- [Download novnc.patch](https://github.com/Cateners/noVNC/releases/tag/1.2)
- Switch directory to /usr/local/etc/tmoe-linux/novnc
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

#### Fix tmoe not being able to download software

In the xfce version I released, I added the --async-dns=false parameter to every aria2c call.

First switch to the tmoe directory `/usr/local/etc/tmoe-linux/git/share`, then execute the script `./replace.sh old-version`:
```shell
#!/bin/bash
# Usage: ./replace.sh directory
# This script recursively replaces text in all files in the given directory
# Original text: aria2c --console-log-level
# New text: aria2c --async-dns=false --console-log-level

# Check if the parameters are correct
if [ $# -ne 1 ]; then
  echo "Error: A directory is required as a parameter"
  exit 1
fi

# Check if the directory exists
if [ ! -d "$1" ]; then
  echo "Error: Directory $1 does not exist"
  exit 2
fi

# Traverse all files in the directory
find "$1" -type f | while read file; do
  # Use sed command to replace text
  sed -i 's/aria2c --console-log-level/aria2c --async-dns=false --console-log-level/g' "$file"
  echo
done
```

Delete replace.sh after use;

In addition, tmoe official has now given a [solution](https://gitee.com/mo2/linux/issues/I8BQG3), but my test seems to still not work, so I'll leave it like this for now

#### Modify apt source

Modify /etc/apt/sources.list as needed, and also change non-free to non-free-firmware

#### Install Firefox Browser

`sudo apt install firefox-esr firefox-esr-l10n-zh-cn`

#### Install Input Method

- debian-i
- 03 Secret Garden - 10 Input Method - fcitx4 - Install 4libpinyin and 6 Cloud Pinyin modules
- In the graphical interface application, find fcitx configuration - Add-ons - Cloud Pinyin - Configuration - Cloud Pinyin source, change Google to Baidu, confirm
  - Start the graphical interface: enter startnovnc, a URL similar to xxx.xxx.xxx.xxx:36082/vnc.html will appear, copy it to the browser of the local machine and enter the vnc password 12345678 to access it.

#### Install gdebi

This software package allows users to install deb packages through a graphical interface

Installation: `sudo apt install gdebi`

Modify launcher: Add sudo after Exec= in /usr/share/applications/gdebi.desktop

#### Install VSCode

VSCode is installed using tmoe, just to test whether the problem of not being able to download software exists

- 2 Software - 2 Development - 1 VSCode - 1 Official

tmoe will also install gnome-keyring. Since it caused VSCode to repeatedly pop up windows to update the keyring when I was making the xfce package, I uninstalled it. You can decide whether to keep it as needed.

#### Install ffmpeg

This is for previewing streaming, install as needed

`sudo apt install ffmpeg`

### Other Patches

#### cmatrix

**（20241112）Note, this step can be skipped because cmatrix has been built into patch.tar.gz**

This is an Easter egg for the shortcut command. Download the cmatrix package, extract the cmatrix file and put it in /home/tiny/.local/bin, remember to add execution permission

#### WPS

**（20241112）Note, the new version of wps no longer needs to change the integration mode to multi-component mode to be used normally, so the software setting modification step can be skipped**

- Software Settings Modification
  - Download the WPS linux arm64 deb installation package from the official website, and install it directly by clicking on it in the graphical interface with gdebi (just to test if gdebi can be used)
  - Open WPS - Settings in the upper right corner - Other - Switch window management mode - Change integration mode to multi-component mode (otherwise some devices will freeze when creating new documents, etc., the reason is currently unknown)
  - Uninstall WPS using gdebi (or by yourself)
- libtiff.so.5 library patch
  - Switch to the /lib/aarch64-linux-gnu folder, create a soft link to link libtiff.so.6 to libtiff.so.5
  - Or find the libtiff.so.5 package and install it, which may be better
- Pre-install ttf-mscorefonts-installer
  - This package is a dependency of WPS and will download fonts from sourceforge, which may be very slow, so install it with apt in advance


### Extra Steps

- Fix the system turning into English during updates (v1.0.19): Uncomment the line containing zh_CN.UTF-8 in the /etc/locale.gen file
- Fixed high CPU usage when xfce uses Termux:X11 (v1.0.19): Remove the power management plugin from the bottom panel (Right click - Panel - Panel Preferences - Items)
- Do not pop up terminal window (v1.0.18): Delete the open_terminal in the second to last line of the /etc/X11/xinit/Xsession file
- Disable vertical sync to use Turnip+Zink (v1.0.17): Change the vblank_mode value from auto to off in the file ~/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml
- The xfce version has the image viewer ristretto and the archive manager xarchiver installed (v1.0.16)

### Packaging

- First, exit the container, cancel the mounting of sd and termux in the container mount options, then enter the container and delete the termux soft link
  - When using tar for packaging later, even if exclude is specified, tar will try to package them
  - This is likely because I didn't use the parameters correctly. If you are very confident, you don't need to do this, just package it yourself =v=
- Download and extract the busybox executable file [here](https://github.com/meefik/busybox/releases) and place it in the system root directory
  - I use busybox's tar for packaging, not the container's own tar, because the container's own tar will package hard links into separate files, causing an extra 1GB of space to be occupied after packaging and unpacking
  - This is also likely because I didn't use the parameters correctly. If you are very confident, you don't need to do this......
- Delete as many usage traces as possible, including but not limited to
  - apt clean
  - Files under /tmp, delete after exiting the container
  - Under tiny and root directories
    - .cache
    - .vnc/vnc.log, .vnc/x.log
    - .bash_history
    - .ICEauthority
    - .Xauthority
    - etc.
- Switch to root user, switch to root directory, `/busybox tar -Jcpvf /debian.tar.xz --exclude=debian.tar.xz --exclude=dev --exclude=proc --exclude=system --exclude=storage --exclude=apex --exclude=sys --exclude=media/sd --exclude=busybox --exclude=".l2s.*" /`


## Production Steps (GXDE OS)

### Coo Coo Coo (placeholder/coming soon)

Actually, the process is similar to the previous one. Basically, it is Install graphical interface -> Fix Chinese -> Fix tmoe -> Fix non-free-firmware -> (Casually check disk space usage, omit) -> Fix wps -> Prepare busybox for packaging -> Add Xsession file for startup

Please see VCR:
```
    1  exit
    2  sudo apt install sd/Download/gxde-source_1.0.1_all.deb
    3  sudo apt install ./sd/Download/gxde-source_1.0.1_all.deb
    4  sudo apt update
    5  sudo apt install gxde-testing-source
    6  sudo apt update
    7  sudo apt install gxde-desktop-android --no-install-recommends
    8  nano /etc/locale.gen
    9  cd /usr/local/etc/tmoe-linux/git/share
   10  nano replace.sh
   11  ./replace.sh old-version
   12  chmod +x replace.sh
   13  ./replace.sh old-version
   14  rm replace.sh
   15  cd
   16  tmoe
   17  nano /etc/apt/sources.list
   18  sudo apt update
   19  nano /etc/apt/sources.list
   20  sudo apt update
   21  cd /var/log
   22  ls -l
   23  du -h --max-depth=1 | sort -h
   24  cd ..
   25  du -h --max-depth=1 | sort -h
   26  cd cache/
   27  ls -l
   28  sudo apt update ttf-mscorefonts-installer
   29  sudo apt install ttf-mscorefonts-installer
   30  cd /usr/lib/aarch64-linux-gnu/
   31  ln -s libtiff.so.6 libtiff.so.5
   32  history
   33  cd /
   34  cp home/tiny/termux/home/.local/share/tmoe-linux/containers/proot/debian-bookworm_arm64/busybox .
   35  cd /etc/X11/xinit/
   36  ls
   37  cp ~/termux/home/.local/share/tmoe-linux/containers/proot/debian-bookworm_arm64/etc/X11/xinit/Xsession .
   38  ls -l Xsession
   39  cd /
   40  ls -l busybox
   41  exit
   42  sudo apt clean;sudo apt autoclean;sudo apt autoremove --purge || sudo apt autoremove
   43  history
   44  history > /sd/history.txt
```

About Xsession file:

Because the current Tiny Computer code hardcodes the startup of the X11 graphical interface by executing /etc/X11/xinit/Xsession, if you install the graphical interface through tmoe, this file is included. However, GXDE is not installed through tmoe, so I just wrote one casually:
```
rm -rf /run/dbus/pid
sudo dbus-daemon --system
export $(dbus-launch)
startgxde_android
```