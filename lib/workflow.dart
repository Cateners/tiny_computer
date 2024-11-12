// workflow.dart  --  This file is part of tiny_computer.               
                                                                        
// Copyright (C) 2023 Caten Hu                                          
                                                                        
// Tiny Computer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published    
// by the Free Software Foundation, either version 3 of the License,    
// or any later version.                               
                                                                         
// Tiny Computer is distributed in the hope that it will be useful,          
// but WITHOUT ANY WARRANTY; without even the implied warranty          
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              
// See the GNU General Public License for more details.                 
                                                                     
// You should have received a copy of the GNU General Public License    
// along with this program.  If not, see http://www.gnu.org/licenses/.

import 'dart:io';
import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:intl/intl.dart';

import 'package:http/http.dart' as http;
import 'package:retry/retry.dart';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:webview_flutter/webview_flutter.dart';

import 'package:xterm/xterm.dart';
import 'package:flutter_pty/flutter_pty.dart';

import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';

import 'package:shared_preferences/shared_preferences.dart';

import 'package:wakelock_plus/wakelock_plus.dart';
import 'package:device_info_plus/device_info_plus.dart';

class Util {

  static Future<void> copyAsset(String src, String dst) async {
    await File(dst).writeAsBytes((await rootBundle.load(src)).buffer.asUint8List());
  }
  static Future<void> copyAsset2(String src, String dst) async {
    ByteData data = await rootBundle.load(src);
    await File(dst).writeAsBytes(data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes));
  }
  static void createDirFromString(String dir) {
    Directory.fromRawPath(const Utf8Encoder().convert(dir)).createSync(recursive: true);
  }

  static Future<int> execute(String str) async {
    Pty pty = Pty.start(
      "/system/bin/sh"
    );
    pty.write(const Utf8Encoder().convert("$str\nexit \$?\n"));
    return await pty.exitCode;
  }

  static void termWrite(String str) {
    G.termPtys[G.currentContainer]!.pty.write(const Utf8Encoder().convert("$str\n"));
  }



  //所有key
  //int defaultContainer = 0: 默认启动第0个容器
  //int defaultAudioPort = 4718: 默认pulseaudio端口(为了避免和其它软件冲突改成4718了，原默认4713)
  //bool autoLaunchVnc = true: 是否自动启动图形界面并跳转 以前只支持VNC就这么起名了
  //String lastDate: 上次启动软件的日期，yyyy-MM-dd
  //bool isTerminalWriteEnabled = false
  //bool isTerminalCommandsEnabled = false 
  //int termMaxLines = 4095 终端最大行数
  //double termFontScale = 1 终端字体大小
  //bool isStickyKey = true 终端ctrl, shift, alt键是否粘滞
  //String defaultFFmpegCommand 默认推流命令
  //String defaultVirglCommand 默认virgl参数
  //String defaultVirglOpt 默认virgl环境变量
  //bool reinstallBootstrap = false 下次启动是否重装引导包
  //bool getifaddrsBridge = false 下次启动是否桥接getifaddrs
  //bool uos = false 下次启动是否伪装UOS
  //bool virgl = false 下次启动是否启用virgl
  //bool wakelock = false 屏幕常亮
  //bool isHidpiEnabled = false 是否开启高分辨率
  //bool isJpEnabled = false 是否切换系统到日语
  //bool useAvnc = false 是否默认使用AVNC
  //String defaultHidpiOpt 默认HiDPI环境变量
  //? int bootstrapVersion: 启动包版本
  //String[] containersInfo: 所有容器信息(json)
  //{name, boot:"\$DATA_DIR/bin/proot ...", vnc:"startnovnc", vncUrl:"...", commands:[{name:"更新和升级", command:"apt update -y && apt upgrade -y"},
  // bind:[{name:"U盘", src:"/storage/xxxx", dst:"/media/meow"}]...]}
  //TODO: 这么写还是不对劲，有空改成类试试？
  static dynamic getGlobal(String key) {
    bool b = G.prefs.containsKey(key);
    switch (key) {
      case "defaultContainer" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(0);
      case "defaultAudioPort" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(4718);
      case "autoLaunchVnc" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(true);
      case "lastDate" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("1970-01-01");
      case "isTerminalWriteEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isTerminalCommandsEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "termMaxLines" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(4095);
      case "termFontScale" : return b ? G.prefs.getDouble(key)! : (value){G.prefs.setDouble(key, value); return value;}(1.0);
      case "isStickyKey" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(true);
      case "reinstallBootstrap" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "getifaddrsBridge" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "uos" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "virgl" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "turnip" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "dri3" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "wakelock" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isHidpiEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isJpEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "useAvnc" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(true);
      case "useX11" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "defaultFFmpegCommand" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("-hide_banner -an -max_delay 1000000 -r 30 -f android_camera -camera_index 0 -i 0:0 -vf scale=iw/2:-1 -rtsp_transport udp -f rtsp rtsp://127.0.0.1:8554/stream");
      case "defaultVirglCommand" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("--socket-path=\$CONTAINER_DIR/tmp/.virgl_test");
      case "defaultVirglOpt" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("GALLIUM_DRIVER=virpipe");
      case "defaultTurnipOpt" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("MESA_LOADER_DRIVER_OVERRIDE=zink VK_ICD_FILENAMES=/home/tiny/.local/share/tiny/extra/freedreno_icd.aarch64.json TU_DEBUG=noconform");
      case "defaultHidpiOpt" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("GDK_SCALE=2 QT_FONT_DPI=192");
      case "containersInfo" : return G.prefs.getStringList(key)!;
    }
  }

//     await G.prefs.setStringList("containersInfo", ["""{
// "name":"Debian Bookworm",
// "boot":"${D.boot}",
// "vnc":"startnovnc &",
// "vncUrl":"http://localhost:36082/vnc.html?host=localhost&port=36082&autoconnect=true&resize=remote&password=12345678",
// "commands":${jsonEncode(D.commands)}
// }"""]);
// case "lastDate" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("1970-01-01");

  static dynamic getCurrentProp(String key) {
    dynamic m = jsonDecode(Util.getGlobal("containersInfo")[G.currentContainer]);
    if (m.containsKey(key)) {
      return m[key];
    }
    switch (key) {
      case "name" : return (value){addCurrentProp(key, value); return value;}("Debian Bookworm");
      case "boot" : return (value){addCurrentProp(key, value); return value;}(D.boot);
      case "vnc" : return (value){addCurrentProp(key, value); return value;}("startnovnc &");
      case "vncUrl" : return (value){addCurrentProp(key, value); return value;}("http://localhost:36082/vnc.html?host=localhost&port=36082&autoconnect=true&resize=remote&password=12345678");
      case "vncUri" : return (value){addCurrentProp(key, value); return value;}("vnc://127.0.0.1:5904?VncPassword=12345678&SecurityType=2");
      case "commands" : return (value){addCurrentProp(key, value); return value;}(jsonDecode(jsonEncode(D.commands)));
    }
  }

  //用来设置name, boot, vnc, vncUrl等
  static Future<void> setCurrentProp(String key, dynamic value) async {
    await G.prefs.setStringList("containersInfo",
      Util.getGlobal("containersInfo")..setAll(G.currentContainer,
        [jsonEncode((jsonDecode(
          Util.getGlobal("containersInfo")[G.currentContainer]
        ))..update(key, (v) => value))]
      )
    );
  }

  //用来添加不存在的key等
  static Future<void> addCurrentProp(String key, dynamic value) async {
    await G.prefs.setStringList("containersInfo",
      Util.getGlobal("containersInfo")..setAll(G.currentContainer,
        [jsonEncode((jsonDecode(
          Util.getGlobal("containersInfo")[G.currentContainer]
        ))..addAll({key : value}))]
      )
    );
  }

  //限定字符串在min和max之间, 给文本框的validator
  static String? validateBetween(String? value, int min, int max, Function opr) {
    if (value == null || value.isEmpty) {
      return "请输入数字";
    }
    int? parsedValue = int.tryParse(value);
    if (parsedValue == null) {
      return "请输入有效的数字";
    }
    if (parsedValue < min || parsedValue > max) {
      return "请输入$min到$max之间的数字";
    }
    opr();
    return null;
  }

}

//来自xterms关于操作ctrl, shift, alt键的示例
//这个类应该只能有一个实例G.keyboard
class VirtualKeyboard extends TerminalInputHandler with ChangeNotifier {
  final TerminalInputHandler _inputHandler;

  VirtualKeyboard(this._inputHandler);

  bool _ctrl = false;

  bool get ctrl => _ctrl;

  set ctrl(bool value) {
    if (_ctrl != value) {
      _ctrl = value;
      notifyListeners();
    }
  }

  bool _shift = false;

  bool get shift => _shift;

  set shift(bool value) {
    if (_shift != value) {
      _shift = value;
      notifyListeners();
    }
  }

  bool _alt = false;

  bool get alt => _alt;

  set alt(bool value) {
    if (_alt != value) {
      _alt = value;
      notifyListeners();
    }
  }

  @override
  String? call(TerminalKeyboardEvent event) {
    final ret = _inputHandler.call(event.copyWith(
      ctrl: event.ctrl || _ctrl,
      shift: event.shift || _shift,
      alt: event.alt || _alt,
    ));
    G.maybeCtrlJ = event.key.name == "keyJ"; //这个是为了稍后区分按键到底是Enter还是Ctrl+J
    if (!(Util.getGlobal("isStickyKey") as bool)) {
      G.keyboard.ctrl = false;
      G.keyboard.shift = false;
      G.keyboard.alt = false;
    }
    return ret;
  }
}

//一个结合terminal和pty的类
class TermPty {
  late final Terminal terminal;
  late final Pty pty;

  TermPty() {
    terminal = Terminal(inputHandler: G.keyboard, maxLines: Util.getGlobal("termMaxLines") as int);
    pty = Pty.start(
      "/system/bin/sh",
      workingDirectory: G.dataPath,
      columns: terminal.viewWidth,
      rows: terminal.viewHeight,
    );
    pty.output
        .cast<List<int>>()
        .transform(const Utf8Decoder())
        .listen(terminal.write);
    pty.exitCode.then((code) {
      terminal.write('the process exited with exit code $code');
      if (code == 0) {
        SystemChannels.platform.invokeMethod("SystemNavigator.pop");
      }
      //Signal 9 hint
      if (code == -9) {
        D.androidChannel.invokeMethod("launchSignal9Page", {});
      }
    });
    terminal.onOutput = (data) {
      if (!(Util.getGlobal("isTerminalWriteEnabled") as bool)) {
        return;
      }
      //由于对回车的处理似乎存在问题，所以拿出来单独处理
      data.split("").forEach((element) {
        if (element == "\n" && !G.maybeCtrlJ) {
          terminal.keyInput(TerminalKey.enter);
          return;
        }
        G.maybeCtrlJ = false;
        pty.write(const Utf8Encoder().convert(element));
      });
    };
    terminal.onResize = (w, h, pw, ph) {
      pty.resize(h, w);
    };
  }

}

//default values
class D {

  //帮助信息
  static const faq = [
    {"q":"安卓12及以上注意事项:错误码9", "a":"""如果你的系统版本大于等于android 12
可能会在使用过程中异常退出(返回错误码9)
届时本软件会提供方案指引你修复
并不难
但是软件没有权限
不能帮你修复

你也可以在高级设置里手动前往错误页面"""},
    {"q":"安卓13注意事项", "a":"""如果你的系统版本大于等于android 13
那么很可能一些网页应用如jupyter notebook
bilibili客户端等等不可用
可以去全局设置开启getifaddrs桥接"""},
    {"q":"用一会就断掉", "a":"""这应该是出现了错误9的情况
下次出现此情况时
按设备的返回键(或用返回手势)
应该能看到软件提供的修复引导"""},
    {"q":"如何访问设备文件？", "a":"""如果你给了存储权限
那么通过主目录下的文件夹
就可以访问设备存储
要访问整个设备存储可以访问sd文件夹
此外主文件夹的很多文件夹与设备文件夹绑定
比如主文件夹的下载文件夹就是设备的下载文件夹

另外！任何支持SAF的安卓软件
都可以在不打开小小电脑的情况下直接访问小小电脑的文件"""},
    {"q":"如何访问SD卡文件？", "a":"""首先用其他文件管理器查看SD卡路径
（通常为/storage/xxxx...）
然后把该地址输入到小小电脑的文件管理器回车即可

注意不要只输/storage
或者从/storage文件夹点进去
那样是没有权限的"""},
    {"q":"自带的火狐浏览器无法下载文件", "a":"""检查是否授予小小电脑存储权限

火狐下载的文件会保存在设备的下载文件夹
如果不想授予存储权限也可在火狐的设置里更改下载文件保存位置"""},
    {"q":"安装更多软件？", "a":"""本软件的初衷是作为PC应用引擎的平替
所以我不会提供安装除WPS等软件外的帮助
另外你需要一些Linux系统使用经验

如果你想安装其他软件
可以使用容器自带的tmoe
但并不保证安装了能用哦
(事实上, 目前容器里的
VSCode、输入法
都是用tmoe安装的
就连系统本身也是用tmoe安装的)

也可以在网上搜索
"ubuntu安装xxx教程"
"linux安装xxx教程"等等

要注意容器环境和完整Linux有不同
你可能需要做一些修补工作
比如基于Electron的软件通常需要添加--no-sandbox参数才能使用"""},
    {"q":"WPS没有常用字体？", "a":"""如果你需要更多字体
在给了存储权限的情况下
直接将字体复制到设备存储的Fonts文件夹即可
一些常用的办公字体
可以在Windows电脑的C:\\Windows\\Fonts文件夹找到
由于可能的版权问题
软件不能帮你做"""},
    {"q":"中文输入法？", "a":"""关于中文输入的问题
强烈建议不要使用安卓中文输入法直接输入中文
而是使用英文键盘通过容器的输入法(Ctrl+空格切换)输入中文
避免丢字错字"""},
    {"q":"外接鼠标移不到边缘", "a":"""最可能的情况是
你的设备使用手势控制而不是三大金刚键
边缘是系统留给你手势操作用的
大概没什么办法
不过也许可以双指捏合使屏幕范围稍微变小一点？"""},
    {"q":"镜像正在同步", "a":"""偶尔会出现这种情况
一段时间后就会同步完成

请几个小时后再试一次"""},
    {"q":"找不到sys/cdefs.h", "a":"""点击上面无法编译C语言程序的快捷指令"""},
    {"q":"安装一些软件很慢", "a":"""请尝试使用魔法"""},
    {"q":"联发科处理器可以用吗？", "a":"""联发科处理器只是没有成熟的开源图形驱动供加速而已
不考虑速度的话都能用的"""},
  ];

  //默认快捷指令
  static const commands = [{"name":"检查更新并升级", "command":"sudo dpkg --configure -a && sudo apt update && sudo apt full-upgrade -y && sudo apt autoremove -y && sudo localedef -c -i zh_CN -f UTF-8 zh_CN.UTF-8"},
    {"name":"查看系统信息", "command":"neofetch -L && neofetch --off"},
    {"name":"清屏", "command":"clear"},
    {"name":"中断任务", "command":"\x03"},
    {"name":"安装图形处理软件Krita", "command":"sudo apt update && sudo apt install -y krita krita-l10n"},
    {"name":"卸载Krita", "command":"sudo apt autoremove --purge -y krita krita-l10n"},
    {"name":"安装视频剪辑软件Kdenlive", "command":"sudo apt update && sudo apt install -y kdenlive"},
    {"name":"卸载Kdenlive", "command":"sudo apt autoremove --purge -y kdenlive"},
    {"name":"安装科学计算软件Octave", "command":"sudo apt update && sudo apt install -y octave"},
    {"name":"卸载Octave", "command":"sudo apt autoremove --purge -y octave"},
    {"name":"安装WPS", "command":r"""cat << 'EOF' | sh && sudo dpkg --configure -a && sudo apt update && sudo apt install -y /tmp/wps.deb
wget https://mirrors.sdu.edu.cn/spark-store-repository/aarch64-store/office/wps-office/wps-office_11.1.0.11720-fix1_arm64.deb -O /tmp/wps.deb
EOF
rm /tmp/wps.deb"""},
    {"name":"卸载WPS", "command":"sudo apt autoremove --purge -y wps-office"},
    {"name":"安装CAJViewer", "command":"wget https://download.cnki.net/net.cnki.cajviewer_1.3.20-1_arm64.deb -O /tmp/caj.deb && sudo apt update && sudo apt install -y /tmp/caj.deb && bash /home/tiny/.local/share/tiny/caj/postinst; rm /tmp/caj.deb"},
    {"name":"卸载CAJViewer", "command":"sudo apt autoremove --purge -y net.cnki.cajviewer && bash /home/tiny/.local/share/tiny/caj/postrm"},
    {"name":"安装亿图图示", "command":"wget https://www.edrawsoft.cn/2download/aarch64/edrawmax_12.6.1-1_arm64_binner.deb -O /tmp/edraw.deb && sudo apt update && sudo apt install -y /tmp/edraw.deb && bash /home/tiny/.local/share/tiny/edraw/postinst; rm /tmp/edraw.deb"},
    {"name":"卸载亿图图示", "command":"sudo apt autoremove --purge -y edrawmax libldap-2.4-2"},
    {"name":"安装QQ", "command":"""wget \$(curl -L https://cdn-go.cn/qq-web/im.qq.com_new/latest/rainbow/linuxQQDownload.js | grep -oP '(?<=armDownloadUrl":\\{"deb":")[^"]+') -O /tmp/qq.deb && sudo apt update && sudo apt install -y /tmp/qq.deb && sed -i 's#Exec=/opt/QQ/qq %U#Exec=/opt/QQ/qq --no-sandbox %U#g' /usr/share/applications/qq.desktop; rm /tmp/qq.deb"""},
    {"name":"卸载QQ", "command":"sudo apt autoremove --purge -y linuxqq"},
    {"name":"安装微信", "command":"wget https://dldir1v6.qq.com/weixin/Universal/Linux/WeChatLinux_arm64.deb -O /tmp/wechat.deb && sudo apt update && sudo apt install -y /tmp/wechat.deb && echo '安装完成。如果你使用微信只是为了传输文件，那么可以考虑使用支持SAF的文件管理器（如：质感文件），直接访问小小电脑所有文件。'; rm /tmp/wechat.deb"},
    {"name":"卸载微信", "command":"sudo apt autoremove --purge -y wechat"},
    {"name":"安装钉钉", "command":"""wget \$(curl -L https://g.alicdn.com/dingding/h5-home-download/0.2.4/js/index.js | grep -oP 'url:"\\K[^"]*arm64\\.deb' | head -n 1) -O /tmp/dingtalk.deb && sudo apt update && sudo apt install -y /tmp/dingtalk.deb libglut3.12 libglu1-mesa && sed -i 's#\\./com.alibabainc.dingtalk#\\./com.alibabainc.dingtalk --no-sandbox#g' /opt/apps/com.alibabainc.dingtalk/files/Elevator.sh; rm /tmp/dingtalk.deb"""},
    {"name":"卸载钉钉", "command":"sudo apt autoremove --purge -y com.alibabainc.dingtalk"},
    {"name":"修复无法编译C语言程序", "command":"sudo apt update && sudo apt reinstall -y libc6-dev"},
    {"name":"修复系统语言到中文", "command":"sudo localedef -c -i zh_CN -f UTF-8 zh_CN.UTF-8 # 重启后完全生效"},
    {"name":"启用回收站", "command":"sudo apt update && sudo apt install -y gvfs && echo '安装完成, 重启软件即可使用回收站。'"},
    {"name":"拉流测试", "command":"ffplay rtsp://127.0.0.1:8554/stream &"},
    {"name":"清理包管理器缓存", "command":"sudo apt clean"},
    {"name":"关机", "command":"stopvnc\nexit\nexit"},
    {"name":"???", "command":"timeout 8 cmatrix"}
  ];

  //默认wine快捷指令
  static const wineCommands = [{"name":"Wine配置", "command":"winecfg"},
    {"name":"修复方块字", "command":"regedit Z:\\\\home\\\\tiny\\\\.local\\\\share\\\\tiny\\\\extra\\\\chn_fonts.reg && wine reg delete \"HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes\" /va /f"},
    {"name":"开始菜单文件夹", "command":"wine explorer \"C:\\\\ProgramData\\\\Microsoft\\\\Windows\\\\Start Menu\\\\Programs\""},
    {"name":"我的电脑", "command":"wine explorer"},
    {"name":"记事本", "command":"notepad"},
    {"name":"扫雷", "command":"winemine"},
    {"name":"注册表", "command":"regedit"},
    {"name":"控制面板", "command":"wine control"},
    {"name":"文件管理器", "command":"winefile"},
    {"name":"任务管理器", "command":"wine taskmgr"},
    {"name":"IE浏览器", "command":"wine iexplore"},
    {"name":"强制关闭Wine", "command":"wineserver -k"}
  ];

  //默认小键盘
  static const termCommands = [
    {"name": "Esc", "key": TerminalKey.escape},
    {"name": "Tab", "key": TerminalKey.tab},
    {"name": "↑", "key": TerminalKey.arrowUp},
    {"name": "↓", "key": TerminalKey.arrowDown},
    {"name": "←", "key": TerminalKey.arrowLeft},
    {"name": "→", "key": TerminalKey.arrowRight},
    {"name": "Del", "key": TerminalKey.delete},
    {"name": "PgUp", "key": TerminalKey.pageUp},
    {"name": "PgDn", "key": TerminalKey.pageDown},
    {"name": "Home", "key": TerminalKey.home},
    {"name": "End", "key": TerminalKey.end},
    {"name": "F1", "key": TerminalKey.f1},
    {"name": "F2", "key": TerminalKey.f2},
    {"name": "F3", "key": TerminalKey.f3},
    {"name": "F4", "key": TerminalKey.f4},
    {"name": "F5", "key": TerminalKey.f5},
    {"name": "F6", "key": TerminalKey.f6},
    {"name": "F7", "key": TerminalKey.f7},
    {"name": "F8", "key": TerminalKey.f8},
    {"name": "F9", "key": TerminalKey.f9},
    {"name": "F10", "key": TerminalKey.f10},
    {"name": "F11", "key": TerminalKey.f11},
    {"name": "F12", "key": TerminalKey.f12},
  ];

  static const String boot = "\$DATA_DIR/bin/proot -H --change-id=1000:1000 --pwd=/home/tiny --rootfs=\$CONTAINER_DIR --mount=/system --mount=/apex --mount=/sys --mount=/data --kill-on-exit --mount=/storage --sysvipc -L --link2symlink --mount=/proc --mount=/dev --mount=\$CONTAINER_DIR/tmp:/dev/shm --mount=/dev/urandom:/dev/random --mount=/proc/self/fd:/dev/fd --mount=/proc/self/fd/0:/dev/stdin --mount=/proc/self/fd/1:/dev/stdout --mount=/proc/self/fd/2:/dev/stderr --mount=/dev/null:/dev/tty0 --mount=/dev/null:/proc/sys/kernel/cap_last_cap --mount=/storage/self/primary:/media/sd --mount=\$DATA_DIR/share:/home/tiny/公共 --mount=\$DATA_DIR/tiny:/home/tiny/.local/share/tiny --mount=/storage/self/primary/Fonts:/usr/share/fonts/wpsm --mount=/storage/self/primary/AppFiles/Fonts:/usr/share/fonts/yozom --mount=/system/fonts:/usr/share/fonts/androidm --mount=/storage/self/primary/Pictures:/home/tiny/图片 --mount=/storage/self/primary/Music:/home/tiny/音乐 --mount=/storage/self/primary/Movies:/home/tiny/视频 --mount=/storage/self/primary/Download:/home/tiny/下载 --mount=/storage/self/primary/DCIM:/home/tiny/照片 --mount=/storage/self/primary/Documents:/home/tiny/文档 --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.stat:/proc/stat --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.version:/proc/version --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/bus:/proc/bus --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/buddyinfo:/proc/buddyinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/cgroups:/proc/cgroups --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/consoles:/proc/consoles --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/crypto:/proc/crypto --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/devices:/proc/devices --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/diskstats:/proc/diskstats --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/execdomains:/proc/execdomains --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/fb:/proc/fb --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/filesystems:/proc/filesystems --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/interrupts:/proc/interrupts --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/iomem:/proc/iomem --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/ioports:/proc/ioports --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/kallsyms:/proc/kallsyms --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/keys:/proc/keys --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/key-users:/proc/key-users --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/kpageflags:/proc/kpageflags --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/loadavg:/proc/loadavg --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/locks:/proc/locks --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/misc:/proc/misc --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/modules:/proc/modules --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/pagetypeinfo:/proc/pagetypeinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/partitions:/proc/partitions --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/sched_debug:/proc/sched_debug --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/softirqs:/proc/softirqs --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/timer_list:/proc/timer_list --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/uptime:/proc/uptime --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/vmallocinfo:/proc/vmallocinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/vmstat:/proc/vmstat --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/zoneinfo:/proc/zoneinfo \$EXTRA_MOUNT /usr/bin/env -i HOSTNAME=TINY HOME=/home/tiny USER=tiny TERM=xterm-256color SDL_IM_MODULE=fcitx XMODIFIERS=@im=fcitx QT_IM_MODULE=fcitx GTK_IM_MODULE=fcitx TMOE_CHROOT=false TMOE_PROOT=true TMPDIR=/tmp MOZ_FAKE_NO_SANDBOX=1 DISPLAY=:4 PULSE_SERVER=tcp:127.0.0.1:4718 LANG=zh_CN.UTF-8 SHELL=/bin/bash PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games \$EXTRA_OPT /bin/bash -l";

  static final ButtonStyle commandButtonStyle = OutlinedButton.styleFrom(
    tapTargetSize: MaterialTapTargetSize.shrinkWrap,
    minimumSize: const Size(0, 0),
    padding: const EdgeInsets.fromLTRB(4, 2, 4, 2)
  );

  
  static final ButtonStyle controlButtonStyle = OutlinedButton.styleFrom(
    textStyle: const TextStyle(fontWeight: FontWeight.w400),
    side: const BorderSide(color: Color(0x1F000000)),
    tapTargetSize: MaterialTapTargetSize.shrinkWrap,
    minimumSize: const Size(0, 0),
    padding: const EdgeInsets.fromLTRB(8, 4, 8, 4)
  );

  static const MethodChannel androidChannel = MethodChannel("android");

}

// Global variables
class G {
  static late final String dataPath;
  static Pty? audioPty;
  static late WebViewController controller;
  static late BuildContext homePageStateContext;
  static late int currentContainer; //目前运行第几个容器
  static late Map<int, TermPty> termPtys; //为容器<int>存放TermPty数据
  static late VirtualKeyboard keyboard; //存储ctrl, shift, alt状态
  static bool maybeCtrlJ = false; //为了区分按下的ctrl+J和enter而准备的变量
  static ValueNotifier<double> termFontScale = ValueNotifier(1); //终端字体大小，存储为G.prefs的termFontScale
  static bool isStreamServerStarted = false;
  static bool isStreaming = false;
  //static int? streamingPid;
  static String streamingOutput = "";
  static late Pty streamServerPty;
  //static int? virglPid;
  static ValueNotifier<int> pageIndex = ValueNotifier(0); //主界面索引
  static ValueNotifier<bool> terminalPageChange = ValueNotifier(true); //更改值，用于刷新小键盘
  static ValueNotifier<bool> bootTextChange = ValueNotifier(true); //更改值，用于刷新启动命令
  static ValueNotifier<String> updateText = ValueNotifier("小小电脑"); //加载界面的说明文字
  static ValueNotifier<String> helpText = ValueNotifier("""
第一次加载大概需要5到10分钟...
正常情况下，加载完成后软件会自动跳转到图形界面

在图形界面时，点击即鼠标左键
长按为鼠标右键
双指点击弹出键盘
双指划动为鼠标滚轮

!!!在图形界面返回，可以回到终端界面和控制界面!!!
你可以在控制界面安装更多软件或者阅读帮助信息

请不要在安装时退出软件

如果过了很长时间都没有加载完成
可以去设置里看看小小电脑占用空间是不是一直没变
如果是说明卡在什么地方了
建议清除本软件数据重来一次

(有一位网友提到过
自己无论怎么清软件数据都装不上
但在重启手机之后就装上了)

一些注意事项：
此软件以GPL协议免费开源
如果是买的就是被骗了, 请举报
源代码在这里: https://github.com/Cateners/tiny_computer
软件也会第一时间在这里更新
请尽可能在这里下载软件, 确保是正版

如果你遇到了问题
可以去https://github.com/Cateners/tiny_computer/issues/
留言反馈

如果软件里有程序正在正常运行
请不要强行关闭本软件
否则可能会损坏容器
(如dpkg被中断)
特别是在安装WPS的时候
可能以为卡20%了
其实耐心等待就好

感谢使用!

(顺带一提, 全部解压完大概需要4~5GB空间
解压途中占用空间可能更多
请确保有足够的空间
(这样真的Tiny吗><))

常见问题："""); //帮助页的说明文字
  static String postCommand = ""; //第一次进入容器时额外运行的命令
  
  static bool wasAvncEnabled = false;
  static bool wasX11Enabled = false;


  static late SharedPreferences prefs;
}

class Workflow {

  static Future<void> grantPermissions() async {
    Permission.storage.request();
    //Permission.manageExternalStorage.request();
  }

  static Future<void> setupBootstrap() async {
    //用来共享数据文件的文件夹
    Util.createDirFromString("${G.dataPath}/share");
    //挂载到/dev/shm的文件夹
    Util.createDirFromString("${G.dataPath}/tmp");
    //给proot的tmp文件夹，虽然我不知道为什么proot要这个
    Util.createDirFromString("${G.dataPath}/proot_tmp");
    //给pulseaudio的tmp文件夹
    Util.createDirFromString("${G.dataPath}/pulseaudio_tmp");
    //解压后得到bin文件夹和libexec文件夹
    //bin存放了proot, pulseaudio, tar等
    //libexec存放了proot loader
    await Util.copyAsset(
    "assets/assets.zip",
    "${G.dataPath}/assets.zip",
    );
    //patch.tar.gz存放了tiny文件夹
    //里面是一些补丁，会被挂载到~/.local/share/tiny
    await Util.copyAsset(
    "assets/patch.tar.gz",
    "${G.dataPath}/patch.tar.gz",
    );
    //dddd
    await Util.copyAsset(
    "assets/busybox",
    "${G.dataPath}/busybox",
    );
    await Util.execute(
"""
export DATA_DIR=${G.dataPath}
cd \$DATA_DIR
chmod +x busybox
\$DATA_DIR/busybox unzip -o assets.zip
chmod -R +x bin/*
chmod -R +x libexec/proot/*
chmod 1777 tmp
ln -sf \$DATA_DIR/busybox \$DATA_DIR/bin/xz
ln -sf \$DATA_DIR/busybox \$DATA_DIR/bin/gzip
\$DATA_DIR/bin/tar zxf patch.tar.gz
\$DATA_DIR/busybox rm -rf assets.zip patch.tar.gz
""");
  }

  //初次启动要做的事情
  static Future<void> initForFirstTime() async {
    //首先设置bootstrap
    G.updateText.value = "正在安装引导包";
    await setupBootstrap();
    
    G.updateText.value = "正在复制容器系统";
    //存放容器的文件夹0和存放硬链接的文件夹.l2s
    Util.createDirFromString("${G.dataPath}/containers/0/.l2s");
    //这个是容器rootfs，被split命令分成了xa*，放在assets里
    //首次启动，就用这个，别让用户另选了
    for (String name in jsonDecode(await rootBundle.loadString('AssetManifest.json')).keys.where((String e) => e.startsWith("assets/xa")).map((String e) => e.split("/").last).toList()) {
      await Util.copyAsset("assets/$name", "${G.dataPath}/$name");
    }
    //-J
    G.updateText.value = "正在安装容器系统";
    await Util.execute(
"""
export DATA_DIR=${G.dataPath}
export CONTAINER_DIR=\$DATA_DIR/containers/0
export EXTRA_OPT=""
cd \$DATA_DIR
export PATH=\$DATA_DIR/bin:\$PATH
export PROOT_TMP_DIR=\$DATA_DIR/proot_tmp
export PROOT_LOADER=\$DATA_DIR/libexec/proot/loader
export PROOT_LOADER_32=\$DATA_DIR/libexec/proot/loader32
#export PROOT_L2S_DIR=\$CONTAINER_DIR/.l2s
\$DATA_DIR/bin/proot --link2symlink sh -c "cat xa* | \$DATA_DIR/bin/tar x -J --delay-directory-restore --preserve-permissions -v -C containers/0"
#Script from proot-distro
chmod u+rw "\$CONTAINER_DIR/etc/passwd" "\$CONTAINER_DIR/etc/shadow" "\$CONTAINER_DIR/etc/group" "\$CONTAINER_DIR/etc/gshadow"
echo "aid_\$(id -un):x:\$(id -u):\$(id -g):Termux:/:/sbin/nologin" >> "\$CONTAINER_DIR/etc/passwd"
echo "aid_\$(id -un):*:18446:0:99999:7:::" >> "\$CONTAINER_DIR/etc/shadow"
id -Gn | tr ' ' '\\n' > tmp1
id -G | tr ' ' '\\n' > tmp2
\$DATA_DIR/busybox paste tmp1 tmp2 > tmp3
local group_name group_id
cat tmp3 | while read -r group_name group_id; do
	echo "aid_\${group_name}:x:\${group_id}:root,aid_\$(id -un)" >> "\$CONTAINER_DIR/etc/group"
	if [ -f "\$CONTAINER_DIR/etc/gshadow" ]; then
		echo "aid_\${group_name}:*::root,aid_\$(id -un)" >> "\$CONTAINER_DIR/etc/gshadow"
	fi
done
\$DATA_DIR/busybox rm -rf xa* tmp1 tmp2 tmp3
""");
    //一些数据初始化
    //$DATA_DIR是数据文件夹, $CONTAINER_DIR是容器根目录
    //Termux:X11的启动命令并不在这里面，而是写死了。这下成💩山代码了:P
    await G.prefs.setStringList("containersInfo", ["""{
"name":"Debian Bookworm",
"boot":"${D.boot}",
"vnc":"startnovnc &",
"vncUrl":"http://localhost:36082/vnc.html?host=localhost&port=36082&autoconnect=true&resize=remote&password=12345678",
"commands":${jsonEncode(D.commands)}
}"""]);
    G.updateText.value = "安装完成";
  }

  static Future<void> initData() async {

    G.dataPath = (await getApplicationSupportDirectory()).path;

    G.termPtys = {};

    G.keyboard = VirtualKeyboard(defaultInputHandler);
    
    G.prefs = await SharedPreferences.getInstance();

    //限制一天内观看视频广告不超过5次
    final String currentDate = DateFormat("yyyy-MM-dd").format(DateTime.now());
    if (currentDate != (Util.getGlobal("lastDate") as String)) {
      await G.prefs.setString("lastDate", currentDate);
      //await G.prefs.setInt("adsWatchedToday", 0);
    }

    //如果没有这个key，说明是初次启动
    if (!G.prefs.containsKey("defaultContainer")) {
      await initForFirstTime();
      //根据用户的屏幕调整分辨率
      final s = WidgetsBinding.instance.platformDispatcher.views.first.physicalSize;
      final String w = (max(s.width, s.height) * 0.75).round().toString();
      final String h = (min(s.width, s.height) * 0.75).round().toString();
      G.postCommand = """sed -i -E "s@(geometry)=.*@\\1=${w}x${h}@" /etc/tigervnc/vncserver-config-tmoe
sed -i -E "s@^(VNC_RESOLUTION)=.*@\\1=${w}x${h}@" \$(command -v startvnc)""";
      await G.prefs.setBool("getifaddrsBridge", (await DeviceInfoPlugin().androidInfo).version.sdkInt >= 31);
    }
    G.currentContainer = Util.getGlobal("defaultContainer") as int;

    //是否需要重新安装引导包?
    if (Util.getGlobal("reinstallBootstrap")) {
      G.updateText.value = "正在重新安装引导包";
      await setupBootstrap();
      G.prefs.setBool("reinstallBootstrap", false);
    }

    //开启了什么图形界面？
    if (Util.getGlobal("useX11")) {
      G.wasX11Enabled = true;
      Workflow.launchXServer();
    } else if (Util.getGlobal("useAvnc")) {
      G.wasAvncEnabled = true;
    }

    G.termFontScale.value = Util.getGlobal("termFontScale") as double;

    G.controller = WebViewController()..setJavaScriptMode(JavaScriptMode.unrestricted);

    //设置屏幕常亮
    WakelockPlus.toggle(enable: Util.getGlobal("wakelock"));
  }

  static Future<void> initTerminalForCurrent() async {
    if (!G.termPtys.containsKey(G.currentContainer)) {
      G.termPtys[G.currentContainer] = TermPty();
    }
  }

  static Future<void> setupAudio() async {
    G.audioPty?.kill();
    G.audioPty = Pty.start(
      "/system/bin/sh"
    );
    G.audioPty!.write(const Utf8Encoder().convert("""
export DATA_DIR=${G.dataPath}
\$DATA_DIR/busybox sed "s/4713/${Util.getGlobal("defaultAudioPort") as int}/g" \$DATA_DIR/bin/pulseaudio.conf > \$DATA_DIR/bin/pulseaudio.conf.tmp
rm -rf \$DATA_DIR/pulseaudio_tmp/*
TMPDIR=\$DATA_DIR/pulseaudio_tmp HOME=\$DATA_DIR/pulseaudio_tmp XDG_CONFIG_HOME=\$DATA_DIR/pulseaudio_tmp LD_LIBRARY_PATH=\$DATA_DIR/bin \$DATA_DIR/bin/pulseaudio -F \$DATA_DIR/bin/pulseaudio.conf.tmp
exit
"""));
  await G.audioPty?.exitCode;
  }

  static Future<void> launchCurrentContainer() async {
    String extraMount = ""; //mount options and other proot options
    String extraOpt = "";
    if (Util.getGlobal("getifaddrsBridge")) {
      Util.execute("${G.dataPath}/bin/getifaddrs_bridge_server ${G.dataPath}/containers/${G.currentContainer}/tmp/.getifaddrs-bridge");
      extraOpt += "LD_PRELOAD=/home/tiny/.local/share/tiny/extra/getifaddrs_bridge_client_lib.so ";
    }
    if (Util.getGlobal("isHidpiEnabled")) {
      extraOpt += "${Util.getGlobal("defaultHidpiOpt")} ";
    }
    if (Util.getGlobal("uos")) {
      extraMount += "--mount=\$DATA_DIR/tiny/wechat/uos-lsb:/etc/lsb-release --mount=\$DATA_DIR/tiny/wechat/uos-release:/usr/lib/os-release ";
      extraMount += "--mount=\$DATA_DIR/tiny/wechat/license/var/uos:/var/uos --mount=\$DATA_DIR/tiny/wechat/license/var/lib/uos-license:/var/lib/uos-license ";
    }
    if (Util.getGlobal("virgl")) {
      Util.execute("""
export DATA_DIR=${G.dataPath}
export CONTAINER_DIR=\$DATA_DIR/containers/${G.currentContainer}
${G.dataPath}/bin/virgl_test_server ${Util.getGlobal("defaultVirglCommand")}""");
      extraOpt += "${Util.getGlobal("defaultVirglOpt")} ";
    }
    if (Util.getGlobal("turnip")) {
      extraOpt += "${Util.getGlobal("defaultTurnipOpt")} ";
      if (!(Util.getGlobal("dri3"))) {
        extraOpt += "MESA_VK_WSI_DEBUG=sw ";
      }
    }
    if (Util.getGlobal("isJpEnabled")) {
      extraOpt += "LANG=ja_JP.UTF-8 ";
    }
    extraMount += "--mount=\$DATA_DIR/tiny/font:/usr/share/fonts/tiny ";
    extraMount += "--mount=\$DATA_DIR/tiny/extra/cmatrix:/home/tiny/.local/bin/cmatrix ";
    Util.termWrite(
"""
export DATA_DIR=${G.dataPath}
export CONTAINER_DIR=\$DATA_DIR/containers/${G.currentContainer}
export EXTRA_MOUNT="$extraMount"
export EXTRA_OPT="$extraOpt"
#export PROOT_L2S_DIR=\$DATA_DIR/containers/0/.l2s
cd \$DATA_DIR
export PROOT_TMP_DIR=\$DATA_DIR/proot_tmp
export PROOT_LOADER=\$DATA_DIR/libexec/proot/loader
export PROOT_LOADER_32=\$DATA_DIR/libexec/proot/loader32
${Util.getCurrentProp("boot")}
${G.postCommand}
${(Util.getGlobal("autoLaunchVnc") as bool)?((Util.getGlobal("useX11") as bool)?"""mkdir -p "\$HOME/.vnc" && bash /etc/X11/xinit/Xsession &> "\$HOME/.vnc/x.log" &""":Util.getCurrentProp("vnc")):""}
clear""");
  }

  static Future<void> waitForConnection() async {
    await retry(
      // Make a GET request
      () => http.get(Uri.parse(Util.getCurrentProp("vncUrl"))).timeout(const Duration(milliseconds: 250)),
      // Retry on SocketException or TimeoutException
      retryIf: (e) => e is SocketException || e is TimeoutException,
    );
  }

  static Future<void> launchBrowser() async {
    G.controller.loadRequest(Uri.parse(Util.getCurrentProp("vncUrl")));
    Navigator.push(G.homePageStateContext, MaterialPageRoute(builder: (context) {
      return Focus(
        onKeyEvent: (node, event) {
          // Allow webview to handle cursor keys. Without this, the
          // arrow keys seem to get "eaten" by Flutter and therefore
          // never reach the webview.
          // (https://github.com/flutter/flutter/issues/102505).
          if (!kIsWeb) {
            if ({
              LogicalKeyboardKey.arrowLeft,
              LogicalKeyboardKey.arrowRight,
              LogicalKeyboardKey.arrowUp,
              LogicalKeyboardKey.arrowDown,
              LogicalKeyboardKey.tab
            }.contains(event.logicalKey)) {
              return KeyEventResult.skipRemainingHandlers;
            }
          }
          return KeyEventResult.ignored;
        },
        child: GestureDetector(onSecondaryTap: () {
        }, child: WebViewWidget(controller: G.controller))
      );
    }));
  }

  static Future<void> launchAvnc() async {
    await D.androidChannel.invokeMethod("launchUsingUri", {"vncUri": Util.getCurrentProp("vncUri") as String});
  }

  static Future<void> launchXServer() async {
    await D.androidChannel.invokeMethod("launchXServer", {"tmpdir":"${G.dataPath}/containers/${G.currentContainer}/tmp", "xkb":"${G.dataPath}/containers/${G.currentContainer}/usr/share/X11/xkb"});
  }

  static Future<void> launchX11() async {
    await D.androidChannel.invokeMethod("launchX11Page", {});
  }

  static Future<void> workflow() async {
    grantPermissions();
    await initData();
    await initTerminalForCurrent();
    setupAudio();
    launchCurrentContainer();
    if (Util.getGlobal("autoLaunchVnc") as bool) {
      if (G.wasX11Enabled) {
        launchX11();
        return;
      }
      waitForConnection().then((value) => G.wasAvncEnabled?launchAvnc():launchBrowser());
    }
  }
}


