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
import 'package:url_launcher/url_launcher.dart';
import 'package:webview_flutter/webview_flutter.dart';

import 'package:xterm/xterm.dart';
import 'package:flutter_pty/flutter_pty.dart';

import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';

import 'package:shared_preferences/shared_preferences.dart';

import 'package:unity_ads_plugin/unity_ads_plugin.dart';

import 'package:clipboard/clipboard.dart';

import 'package:wakelock_plus/wakelock_plus.dart';

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
  //bool autoLaunchVnc = true: 是否自动启动VNC并跳转
  //String lastDate: 上次启动软件的日期，yyyy-MM-dd
  //int adsWatchedToday: 今日视频广告观看数量
  //int adsWatchedTotal: 视频广告观看数量
  //bool isBannerAdsClosed = false
  //bool isTerminalWriteEnabled = false
  //bool isTerminalCommandsEnabled = false 
  //int termMaxLines = 4095 终端最大行数
  //double termFontScale = 1 终端字体大小
  //int vip = 0 用户等级，vip免广告，你要改吗？(ToT)
  //bool isStickyKey = true 终端ctrl, shift, alt键是否粘滞
  //String defaultFFmpegCommand 默认推流命令
  //String defaultVirglCommand 默认virgl参数
  //String defaultVirglOpt 默认virgl环境变量
  //bool reinstallBootstrap = false 下次启动是否重装引导包
  //bool getifaddrsBridge = false 下次启动是否桥接getifaddrs
  //bool uos = false 下次启动是否伪装UOS
  //bool isBoxEnabled = false 下次启动是否开启box86/box64
  //bool isWineEnabled = false 下次启动是否开启wine
  //bool virgl = false 下次启动是否启用virgl
  //bool wakelock = false 屏幕常亮
  //bool isHidpiEnabled = false 是否开启高分辨率
  //String defaultHidpiOpt 默认HiDPI环境变量
  //? int bootstrapVersion: 启动包版本
  //String[] containersInfo: 所有容器信息(json)
  //{name, boot:"\$DATA_DIR/bin/proot ...", vnc:"startnovnc", vncUrl:"...", commands:[{name:"更新和升级", command:"apt update -y && apt upgrade -y"},
  // bind:[{name:"U盘", src:"/storage/xxxx", dst:"/media/meow"}]...]}
  //String[] adsBonus: 观看广告获取的奖励(json)
  //{name: "xxx", amount: xxx}
  //TODO: 这么写还是不对劲，有空改成类试试？
  static dynamic getGlobal(String key) {
    bool b = G.prefs.containsKey(key);
    switch (key) {
      case "defaultContainer" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(0);
      case "defaultAudioPort" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(4718);
      case "autoLaunchVnc" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(true);
      case "lastDate" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("1970-01-01");
      case "adsWatchedToday" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(0);
      case "adsWatchedTotal" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(0);
      case "isBannerAdsClosed" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isTerminalWriteEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isTerminalCommandsEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "termMaxLines" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(4095);
      case "termFontScale" : return b ? G.prefs.getDouble(key)! : (value){G.prefs.setDouble(key, value); return value;}(1.0);
      case "vip" : return b ? G.prefs.getInt(key)! : (value){G.prefs.setInt(key, value); return value;}(0);
      case "isStickyKey" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(true);
      case "reinstallBootstrap" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "getifaddrsBridge" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "uos" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isBoxEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isWineEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "virgl" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "wakelock" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "isHidpiEnabled" : return b ? G.prefs.getBool(key)! : (value){G.prefs.setBool(key, value); return value;}(false);
      case "defaultFFmpegCommand" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("-hide_banner -an -max_delay 1000000 -r 30 -f android_camera -camera_index 0 -i 0:0 -vf scale=iw/2:-1 -rtsp_transport udp -f rtsp rtsp://127.0.0.1:8554/stream");
      case "defaultVirglCommand" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("--socket-path=\$CONTAINER_DIR/tmp/.virgl_test");
      case "defaultVirglOpt" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("GALLIUM_DRIVER=virpipe MESA_GL_VERSION_OVERRIDE=4.0");
      case "defaultHidpiOpt" : return b ? G.prefs.getString(key)! : (value){G.prefs.setString(key, value); return value;}("GDK_SCALE=2 QT_FONT_DPI=192");
      case "containersInfo" : return G.prefs.getStringList(key)!;
      case "adsBonus" : return b ? G.prefs.getStringList(key)! : (value){G.prefs.setStringList(key, value); return value;}([].cast<String>());
    }
  }

  static dynamic getCurrentProp(String key) {
    return jsonDecode(Util.getGlobal("containersInfo")[G.currentContainer])[key];
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

  //返回单个G.bonusTable定义的item
  static Map<String, dynamic> getRandomBonus() {
    final random = Random();
    final totalWeight = D.bonusTable.fold(0.0, (sum, item) => sum + item['weight']);
    final randomIndex = random.nextDouble() * totalWeight;
    var cumulativeWeight = 0.0;
    for (final item in D.bonusTable) {
      cumulativeWeight += item['weight'];
      if (randomIndex <= cumulativeWeight) {
        return item;
      }
    }
    return D.bonusTable[0];
  }

  //由getRandomBonus返回的数据
  static Future<void> applyBonus(Map<String, dynamic> bonus) async {
    bool flag = false;
    List<String> ret = Util.getGlobal("adsBonus").map((e) {
      Map<String, dynamic> item = jsonDecode(e);
      return (item["name"] == bonus["name"])?
        jsonEncode(item..update("amount", (v) {
          flag = true;
          return v + bonus["amount"];
        })):e;
    }).toList().cast<String>();
    if (!flag) {
      ret.add("""{"name": "${bonus["name"]}", "amount": ${bonus["amount"]}}""");
    }
    await G.prefs.setStringList("adsBonus", ret);
  }

  //根据已看广告量判断是否应该继续看广告
  static bool shouldWatchAds(int expectNum) {
    return ((Util.getGlobal("adsWatchedTotal") as int) < expectNum) && ((Util.getGlobal("vip") as int) < 1) && ((Util.getGlobal("adsWatchedToday") as int) < D.adsRequired["unlockToday"]!) && (G.adsWatchedThisTime < D.adsRequired["unlockOnce"]!);
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
        Navigator.push(G.homePageStateContext, MaterialPageRoute(builder: (context) {
          const TextStyle ts = TextStyle(fontSize: 16, color: Colors.white, fontWeight: FontWeight.normal);
          const String helperLink = "https://www.vmos.cn/zhushou.htm";
          const String helperLink2 = "https://b23.tv/WwqOqW6";
          return Scaffold(backgroundColor: Colors.deepPurple,
            body: Center(
            child: Scrollbar(child:
              SingleChildScrollView(
                child: Column(children: [
                  const Text(":(\n发生了什么？", textScaler: TextScaler.linear(2), style: ts, textAlign: TextAlign.center,),
                  const Text("终端异常退出, 返回错误码9\n此错误通常是高版本安卓系统(12+)限制进程造成的, \n可以使用以下工具修复:", style: ts, textAlign: TextAlign.center),
                  const SelectableText(helperLink, style: ts, textAlign: TextAlign.center),
                  const Text("(复制链接到浏览器查看)", style: ts, textAlign: TextAlign.center),
                  OutlinedButton(onPressed: () {
                    FlutterClipboard.copy(helperLink).then(( value ) {
                      ScaffoldMessenger.of(context).hideCurrentSnackBar();
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(content: const Text("已复制"), action: SnackBarAction(label: "跳转", onPressed: () {
                          launchUrl(Uri.parse(helperLink), mode: LaunchMode.externalApplication);
                        },))
                      );
                    });
                  }, child: const Text("复制", style: ts, textAlign: TextAlign.center)), 
                  const Text("如果不能解决请参考此教程: ", style: ts, textAlign: TextAlign.center),
                  OutlinedButton(onPressed: () {
                    FlutterClipboard.copy(helperLink2).then(( value ) {
                      ScaffoldMessenger.of(context).hideCurrentSnackBar();
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(content: const Text("已复制"), action: SnackBarAction(label: "跳转", onPressed: () {
                          launchUrl(Uri.parse(helperLink2), mode: LaunchMode.externalApplication);
                        },))
                      );
                    });
                  }, child: const Text("查看", style: ts, textAlign: TextAlign.center))
                ]),
              )
            )
          ));
        }));
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
  //默认快捷指令
  static const commands = [{"name":"检查更新并升级", "command":"sudo apt update && sudo apt upgrade -y"},
    {"name":"查看系统信息", "command":"neofetch -L && neofetch --off"},
    {"name":"清屏", "command":"clear"},
    {"name":"查看IP", "command":"hostname -I # 如果显示无权限(Permission denied)，请在全局设置里开启getifaddrs桥接"},
    {"name":"中断任务", "command":"\x03"},
    {"name":"安装图形处理软件Krita", "command":"sudo apt update && sudo apt install -y krita krita-l10n"},
    {"name":"卸载Krita", "command":"sudo apt autoremove --purge -y krita krita-l10n"},
    {"name":"安装视频剪辑软件Kdenlive", "command":"sudo apt update && sudo apt install -y kdenlive"},
    {"name":"卸载Kdenlive", "command":"sudo apt autoremove --purge -y kdenlive"},
    {"name":"安装科学计算软件Octave", "command":"sudo apt update && sudo apt install -y octave"},
    {"name":"卸载Octave", "command":"sudo apt autoremove --purge -y octave"},
    {"name":"安装WPS", "command":"""wget --referer="https://www.wps.cn/product/wpslinux" \$(curl -L https://linux.wps.cn/ | grep -oP 'href="\\K[^"]*arm64\\.deb' | head -n 1) -O /tmp/wps.deb && sudo apt update && sudo apt install -y /tmp/wps.deb; rm /tmp/wps.deb"""},
    {"name":"卸载WPS", "command":"sudo apt autoremove --purge -y wps-office"},
    {"name":"安装CAJViewer", "command":"wget https://download.cnki.net/net.cnki.cajviewer_1.3.20-1_arm64.deb -O /tmp/caj.deb && sudo apt update && sudo apt install -y /tmp/caj.deb && bash /home/tiny/.local/share/tiny/caj/postinst; rm /tmp/caj.deb"},
    {"name":"卸载CAJViewer", "command":"sudo apt autoremove --purge -y net.cnki.cajviewer && bash /home/tiny/.local/share/tiny/caj/postrm"},
    {"name":"安装亿图图示", "command":"wget https://www.edrawsoft.cn/2download/aarch64/edrawmax_11.5.6-3_arm64.deb -O /tmp/edraw.deb && sudo apt update && sudo apt install -y /tmp/edraw.deb && bash /home/tiny/.local/share/tiny/edraw/postinst; rm /tmp/edraw.deb"},
    {"name":"卸载亿图图示", "command":"sudo apt autoremove --purge -y edrawmax libldap-2.4-2"},
    {"name":"安装QQ", "command":"""wget \$(curl -L https://cdn-go.cn/qq-web/im.qq.com_new/latest/rainbow/linuxQQDownload.js | grep -oP 'deb":"\\K[^"]*arm64\\.deb' | head -n 1) -O /tmp/qq.deb && sudo apt update && sudo apt install -y /tmp/qq.deb && sed -i 's#Exec=/opt/QQ/qq %U#Exec=/opt/QQ/qq --no-sandbox %U#g' /usr/share/applications/qq.desktop; rm /tmp/qq.deb"""},
    {"name":"卸载QQ", "command":"sudo apt autoremove --purge -y linuxqq"},
    {"name":"安装UOS微信", "command":"wget https://home-store-packages.uniontech.com/appstore/pool/appstore/c/com.tencent.weixin/com.tencent.weixin_2.1.9_arm64.deb -O /tmp/wechat.deb && sudo apt update && sudo apt install -y /tmp/wechat.deb /home/tiny/.local/share/tiny/wechat/deepin-elf-verify_all.deb /home/tiny/.local/share/tiny/wechat/libssl1.1_1.1.1n-0+deb10u6_arm64.deb && sed -i 's#/opt/apps/com.tencent.weixin/files/weixin/weixin#/opt/apps/com.tencent.weixin/files/weixin/weixin --no-sandbox#g' /opt/apps/com.tencent.weixin/files/weixin/weixin.sh && echo '该微信为UOS特供版，只有账号实名且在UOS系统上运行时可用。在使用前请前往全局设置开启UOS伪装。\n如果你使用微信只是为了传输文件，那么可以考虑使用支持SAF的文件管理器（如：质感文件），直接访问小小电脑所有文件。'; rm /tmp/wechat.deb"},
    {"name":"卸载UOS微信", "command":"sudo apt autoremove --purge -y com.tencent.weixin libssl1.1 deepin-elf-verify"},
    {"name":"安装钉钉", "command":"""wget \$(curl -L https://g.alicdn.com/dingding/h5-home-download/0.2.4/js/index.js | grep -oP 'url:"\\K[^"]*arm64\\.deb' | head -n 1) -O /tmp/dingtalk.deb && sudo apt update && sudo apt install -y /tmp/dingtalk.deb && sed -i 's#\\./com.alibabainc.dingtalk#\\./com.alibabainc.dingtalk --no-sandbox#g' /opt/apps/com.alibabainc.dingtalk/files/Elevator.sh; rm /tmp/dingtalk.deb"""},
    {"name":"卸载钉钉", "command":"sudo apt autoremove --purge -y com.alibabainc.dingtalk"},
    {"name":"修复无法编译C语言程序", "command":"sudo apt update && sudo apt reinstall -y libc6-dev"},
    {"name":"修复系统语言到中文", "command":"sudo localedef -c -i zh_CN -f UTF-8 zh_CN.UTF-8"},
    {"name":"启用回收站", "command":"sudo apt update && sudo apt install -y gvfs && echo '安装完成, 重启软件即可使用回收站。'"},
    {"name":"拉流测试", "command":"ffplay rtsp://127.0.0.1:8554/stream &"},
    {"name":"关机", "command":"stopvnc\nexit\nexit"},
    {"name":"???", "command":"timeout 8 cmatrix"}
  ];

  //默认wine快捷指令
  static const wineCommands = [{"name":"wine配置", "command":"wine64 winecfg"},
    {"name":"我的电脑", "command":"wine64 explorer"},
    {"name":"记事本", "command":"wine64 notepad"},
    {"name":"扫雷", "command":"wine64 winemine"},
    {"name":"注册表", "command":"wine64 regedit"},
    {"name":"控制面板", "command":"wine64 control"},
    {"name":"文件管理器", "command":"wine64 winefile"},
    {"name":"任务管理器", "command":"wine64 taskmgr"},
    {"name":"ie浏览器", "command":"wine64 iexplore"},
    {"name":"强制关闭wine", "command":"wineserver -k"}
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

  //看广告可以获得的奖励。
  //weight抽奖权重，singleUse使用一次花费的数量，amount抽中可以获得的数量
  static const List<Map<String, dynamic>> bonusTable = [
    {"name": "开发者的祝福", "subtitle": "支持开发者的证明", "description": "(*'v'*)\n开发者由衷地感谢你!", "weight": 10000, "amount": 1, "singleUse": 0},
    {"name": "记忆晶片", "subtitle": "看上去像平行四边形", "description": "组成记忆空间的基本元素。\n是从哪里掉下来的呢?", "weight": 50, "amount": 1, "singleUse": 0},
    {"name": "Wishes Flower Part", "subtitle": "为1个人献上祝福", "description": "希望之花的花瓣。在想好为谁祝福后, 点击使用", "weight": 500, "amount": 1, "singleUse": 1},
    {"name": "Wishes Flower Part", "subtitle": "为1个人献上祝福", "description": "希望之花的花瓣。在想好为谁祝福后, 点击使用", "weight": 100, "amount": 3, "singleUse": 1},
    {"name": "Wishes Flower", "subtitle": "为3个人献上祝福", "description": "希望之花。在想好为谁祝福后, 点击使用", "weight": 50, "amount": 1, "singleUse": 1},
    {"name": "Bonus Reward", "subtitle": "会有极好的事情发生", "description": "来自记忆空间的传说。\n使用后一天内必有极好的事情...\n就是你想象的那种事情...\n就会发生。\n不过, 大概只是个传说吧。", "weight": 10, "amount": 0.01, "singleUse": 1},
    {"name": "Bonus Reward", "subtitle": "会有极好的事情发生", "description": "来自记忆空间的传说。\n使用后一天内必有极好的事情...\n就是你想象的那种事情...\n就会发生。\n不过, 大概只是个传说吧。", "weight": 1, "amount": 0.1, "singleUse": 1},
    {"name": "Bonus Reward", "subtitle": "会有极好的事情发生", "description": "来自记忆空间的传说。\n使用后一天内必有极好的事情...\n就是你想象的那种事情...\n就会发生。\n不过, 大概只是个传说吧。", "weight": 1, "amount": 1, "singleUse": 1},
  ];

  //某项功能开启需要的广告数。
  static const Map<String, int> adsRequired = {
    "closeBannerAds" : 5,
    "enableTerminalWrite" : 2,
    "enableTerminalCommands" : 3,
    "changeTermMaxLines" : 6,
    "changeFFmpegCommand" : 8,
    "enableVirgl" : 10,
    "changeHidpiOpt" : 12,
    
    "unlockOnce" : 1, //临时解锁需要看的广告数
    "unlockToday" : 2, //当日解锁需要看的广告数

  };

  static const String boot = "\$DATA_DIR/bin/proot -H --change-id=1000:1000 --pwd=/home/tiny --rootfs=\$CONTAINER_DIR --mount=/system --mount=/apex --mount=/sys --kill-on-exit --mount=/storage:/storage --sysvipc -L --link2symlink --mount=/proc:/proc --mount=/dev:/dev --mount=\$CONTAINER_DIR/tmp:/dev/shm --mount=/dev/urandom:/dev/random --mount=/proc/self/fd:/dev/fd --mount=/proc/self/fd/0:/dev/stdin --mount=/proc/self/fd/1:/dev/stdout --mount=/proc/self/fd/2:/dev/stderr --mount=/dev/null:/dev/tty0 --mount=/dev/null:/proc/sys/kernel/cap_last_cap --mount=/storage/self/primary:/media/sd --mount=\$DATA_DIR/share:/home/tiny/公共 --mount=\$DATA_DIR/tiny:/home/tiny/.local/share/tiny --mount=/storage/self/primary/Fonts:/usr/share/fonts/wpsm --mount=/storage/self/primary/AppFiles/Fonts:/usr/share/fonts/yozom --mount=/system/fonts:/usr/share/fonts/androidm --mount=/storage/self/primary/Pictures:/home/tiny/图片 --mount=/storage/self/primary/Music:/home/tiny/音乐 --mount=/storage/self/primary/Movies:/home/tiny/视频 --mount=/storage/self/primary/Download:/home/tiny/下载 --mount=/storage/self/primary/DCIM:/home/tiny/照片 --mount=/storage/self/primary/Documents:/home/tiny/文档 --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.stat:/proc/stat --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.version:/proc/version --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/bus:/proc/bus --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/buddyinfo:/proc/buddyinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/cgroups:/proc/cgroups --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/consoles:/proc/consoles --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/crypto:/proc/crypto --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/devices:/proc/devices --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/diskstats:/proc/diskstats --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/execdomains:/proc/execdomains --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/fb:/proc/fb --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/filesystems:/proc/filesystems --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/interrupts:/proc/interrupts --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/iomem:/proc/iomem --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/ioports:/proc/ioports --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/kallsyms:/proc/kallsyms --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/keys:/proc/keys --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/key-users:/proc/key-users --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/kpageflags:/proc/kpageflags --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/loadavg:/proc/loadavg --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/locks:/proc/locks --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/misc:/proc/misc --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/modules:/proc/modules --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/pagetypeinfo:/proc/pagetypeinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/partitions:/proc/partitions --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/sched_debug:/proc/sched_debug --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/softirqs:/proc/softirqs --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/timer_list:/proc/timer_list --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/uptime:/proc/uptime --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/vmallocinfo:/proc/vmallocinfo --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/vmstat:/proc/vmstat --mount=\$CONTAINER_DIR/usr/local/etc/tmoe-linux/proot_proc/zoneinfo:/proc/zoneinfo \$EXTRA_MOUNT /usr/bin/env -i HOSTNAME=TINY HOME=/home/tiny USER=tiny TERM=xterm-256color SDL_IM_MODULE=fcitx XMODIFIERS=@im=fcitx QT_IM_MODULE=fcitx GTK_IM_MODULE=fcitx TMOE_CHROOT=false TMOE_PROOT=true TMPDIR=/tmp MOZ_FAKE_NO_SANDBOX=1 DISPLAY=:4 PULSE_SERVER=tcp:127.0.0.1:4718 LANG=zh_CN.UTF-8 SHELL=/bin/bash PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games \$EXTRA_OPT /bin/bash -l";

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

}

// Global variables
class G {
  static late final String dataPath;
  static Pty? audioPty;
  static late WebViewController controller;
  static late BuildContext homePageStateContext;
  static late int currentContainer; //目前运行第几个容器
  static late Map<int, TermPty> termPtys; //为容器<int>存放TermPty数据
  static late AdManager ads; //广告实例
  static late VirtualKeyboard keyboard; //存储ctrl, shift, alt状态
  static bool maybeCtrlJ = false; //为了区分按下的ctrl+J和enter而准备的变量
  static ValueNotifier<double> termFontScale = ValueNotifier(1); //终端字体大小，存储为G.prefs的termFontScale
  static int adsWatchedThisTime = 0; //本次启动应用看的广告数
  static bool isStreamServerStarted = false;
  static bool isStreaming = false;
  //static int? streamingPid;
  static String streamingOutput = "";
  static late Pty streamServerPty;
  static bool isVirglServerStarted = false;
  static late Pty virglServerPty;
  //static int? virglPid;
  static ValueNotifier<int> pageIndex = ValueNotifier(0); //主界面索引
  static ValueNotifier<bool> terminalPageChange = ValueNotifier(true); //更改值，用于刷新小键盘
  static ValueNotifier<bool> bannerAdsChange = ValueNotifier(true); //更改值，用于刷新banner广告
  static ValueNotifier<bool> bootTextChange = ValueNotifier(true); //更改值，用于刷新启动命令
  static ValueNotifier<String> updateText = ValueNotifier("小小电脑"); //加载界面的说明文字

  static bool wasBoxEnabled = false; //本次启动时是否启用了box86/64
  static bool wasWineEnabled = false; //本次启动时是否启用了wine


  static late SharedPreferences prefs;
}

class AdManager {
  
  static Map<String, bool> placements = {
    interstitialVideoAdPlacementId: false,
    rewardedVideoAdPlacementId: false,
  };

  static void loadAds() {
    for (var placementId in placements.keys) {
      loadAd(placementId);
    }
  }

  static void loadAd(String placementId) {
    UnityAds.load(
      placementId: placementId,
      onComplete: (placementId) {
        debugPrint('Load Complete $placementId');
        placements[placementId] = true;
      },
      onFailed: (placementId, error, message) => debugPrint('Load Failed $placementId: $error $message'),
    );
  }

  static void showAd(String placementId, Function completeExtra, Function full) {

    if ((Util.getGlobal("adsWatchedToday") as int) >= 5) {
      full();
      return;
    }

    placements[placementId] = false;
    UnityAds.showVideoAd(
      placementId: placementId,
      onComplete: (placementId) async {
        debugPrint('Video Ad $placementId completed');
        loadAd(placementId);
        G.adsWatchedThisTime++;
        await G.prefs.setInt("adsWatchedTotal", (Util.getGlobal("adsWatchedTotal") as int)+1);
        await G.prefs.setInt("adsWatchedToday", (Util.getGlobal("adsWatchedToday") as int)+1);
        completeExtra();
      },
      onFailed: (placementId, error, message) {
        debugPrint('Video Ad $placementId failed: $error $message');
        loadAd(placementId);
      },
      onStart: (placementId) => debugPrint('Video Ad $placementId started'),
      onClick: (placementId) => debugPrint('Video Ad $placementId click'),
      onSkipped: (placementId) {
        debugPrint('Video Ad $placementId skipped');
        loadAd(placementId);
      },
    );
  }

  static String get gameId {
    if (defaultTargetPlatform == TargetPlatform.android) {
      return '5403132';
    }
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      return '5403133';
    }
    return '';
  }

  static String bannerAdPlacementId = 'Banner_Android';

  static String interstitialVideoAdPlacementId = 'Interstitial_Android';

  static String rewardedVideoAdPlacementId = 'Rewarded_Android';
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
      await G.prefs.setInt("adsWatchedToday", 0);
    }

    //如果没有这个key，说明是初次启动
    if (!G.prefs.containsKey("defaultContainer")) {
      await initForFirstTime();
    }
    G.currentContainer = Util.getGlobal("defaultContainer") as int;

    //是否需要重新安装引导包?
    if (Util.getGlobal("reinstallBootstrap")) {
      G.updateText.value = "正在重新安装引导包";
      await setupBootstrap();
      G.prefs.setBool("reinstallBootstrap", false);
    }

    G.termFontScale.value = Util.getGlobal("termFontScale") as double;

    G.controller = WebViewController()..setJavaScriptMode(JavaScriptMode.unrestricted);

    //恢复临时开启的功能
    if (Util.shouldWatchAds(D.adsRequired["changeFFmpegCommand"]!)) {
      await G.prefs.remove("defaultFFmpegCommand");
    }
    if (Util.shouldWatchAds(D.adsRequired["changeHidpiOpt"]!)) {
      await G.prefs.remove("defaultHidpiOpt");
    }
    if (Util.shouldWatchAds(D.adsRequired["changeTermMaxLines"]!)) {
      await G.prefs.setInt("termMaxLines", 4095);
    }
    if (Util.shouldWatchAds(D.adsRequired["closeBannerAds"]!)) {
      await G.prefs.setBool("isBannerAdsClosed", false);
    }
    if (Util.shouldWatchAds(D.adsRequired["enableTerminalWrite"]!)) {
      await G.prefs.setBool("isTerminalWriteEnabled", false);
    }
    if (Util.shouldWatchAds(D.adsRequired["enableTerminalCommands"]!)) {
      await G.prefs.setBool("isTerminalCommandsEnabled", false);
    }
    if (Util.shouldWatchAds(D.adsRequired["enableVirgl"]!)) {
      await G.prefs.setBool("virgl", false);
    }

    //设置屏幕常亮
    WakelockPlus.toggle(enable: Util.getGlobal("wakelock"));
  }

  static Future<void> initTerminalForCurrent() async {
    if (!G.termPtys.containsKey(G.currentContainer)) {
      G.termPtys[G.currentContainer] = TermPty();
    }
  }

  static Future<void> initAds() async {
    UnityAds.init(
      gameId: AdManager.gameId,
      testMode: false,
      onComplete: () {
        debugPrint('Initialization Complete');
        AdManager.loadAds();
      },
      onFailed: (error, message) => debugPrint('Initialization Failed: $error $message'),
    );
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
    String box86BinPath = "";
    String box64BinPath = "";
    String box86LibraryPath = "";
    String box64LibraryPath = "";
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
    if (Util.getGlobal("isBoxEnabled")) {
      G.wasBoxEnabled = true;
      extraMount += "--x86=/home/tiny/.local/bin/box86 --x64=/home/tiny/.local/bin/box64 ";
      extraMount += "--mount=\$DATA_DIR/tiny/cross/box86:/home/tiny/.local/bin/box86 --mount=\$DATA_DIR/tiny/cross/box64:/home/tiny/.local/bin/box64 ";
      extraOpt += "BOX86_NOBANNER=1 BOX64_NOBANNER=1 ";
    }
    if (Util.getGlobal("isWineEnabled")) {
      G.wasWineEnabled = true;
      box86BinPath += "/home/tiny/.local/share/tiny/cross/wine/bin:";
      box64BinPath += "/home/tiny/.local/share/tiny/cross/wine/bin:";
      box86LibraryPath += "/home/tiny/.local/share/tiny/cross/wine/lib/wine/i386-unix:";
      box64LibraryPath += "/home/tiny/.local/share/tiny/cross/wine/lib/wine/x86_64-unix:";
      extraMount += "--wine=/home/tiny/.local/bin/wine64 ";
      extraMount += "--mount=\$DATA_DIR/tiny/cross/wine.desktop:/usr/share/applications/wine.desktop ";
      //extraMount += "--mount=\$DATA_DIR/tiny/cross/winetricks:/home/tiny/.local/bin/winetricks --mount=\$DATA_DIR/tiny/cross/winetricks.desktop:/usr/share/applications/winetricks.desktop ";
    }
    if (G.wasBoxEnabled) {
      extraOpt += "BOX86_PATH=$box86BinPath/home/tiny/.local/share/tiny/cross/bin ";
      extraOpt += "BOX64_PATH=$box64BinPath/home/tiny/.local/share/tiny/cross/bin ";
      extraOpt += "BOX86_LD_LIBRARY_PATH=$box86LibraryPath/home/tiny/.local/share/tiny/cross/x86lib ";
      extraOpt += "BOX64_LD_LIBRARY_PATH=$box64LibraryPath/home/tiny/.local/share/tiny/cross/x64lib ";
    }
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
${(Util.getGlobal("autoLaunchVnc") as bool)?Util.getCurrentProp("vnc"):""}
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
      SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky,overlays: []);
      SystemChrome.setSystemUIChangeCallback((systemOverlaysAreVisible) async {
        await Future.delayed(const Duration(seconds: 1));
        SystemChrome.restoreSystemUIOverlays();
      });
      return Focus(
        onKey: (node, event) {
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

  static Future<void> workflow() async {
    grantPermissions();
    await initData();
    await initAds();
    await initTerminalForCurrent();
    setupAudio();
    launchCurrentContainer();
    if (Util.getGlobal("autoLaunchVnc") as bool) {
      waitForConnection().then((value) => launchBrowser());
    }
  }
}


