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

class Util {
  static bool isFirstTime() {
    return (! Directory("${G.dataPath}/bin").existsSync()) || File("${G.dataPath}/xao").existsSync();
  }

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

  static Future<void> execute(String str) async {
    Pty pty = Pty.start(
      "/system/bin/sh"
    );
    pty.write(const Utf8Encoder().convert("$str\nexit\n"));
    await pty.exitCode;
  }

  static void termWrite(String str) {
    G.pty.write(const Utf8Encoder().convert("$str\n"));
  }
}

// Global variables
class G {
  static late final String dataPath;
  static late Terminal terminal;
  static late Pty pty;
  static late WebViewController controller;
  static late BuildContext homePageStateContext;

  static const String vncUrl = "http://localhost:36080/vnc.html?host=localhost&port=36080&autoconnect=true&resize=remote";
}

class Workflow {

  static Future<void> grantPermissions() async {
    Permission.storage.request();
    Permission.manageExternalStorage.request();
  }

  static Future<void> initData() async {


  }

  static Future<void> initTerminal() async {

    G.dataPath = (await getApplicationSupportDirectory()).path;

    G.controller = WebViewController()..setJavaScriptMode(JavaScriptMode.unrestricted);

    G.terminal = Terminal();

    G.pty = Pty.start(
      "/system/bin/sh",
      workingDirectory: G.dataPath,
      columns: G.terminal.viewWidth,
      rows: G.terminal.viewHeight,
    );
    G.pty.output
        .cast<List<int>>()
        .transform(const Utf8Decoder())
        .listen(G.terminal.write);
    G.pty.exitCode.then((code) {
      G.terminal.write('the process exited with exit code $code');
      //TO_DO: Singal 9 hint
      if (code == -9) {
        Navigator.push(G.homePageStateContext, MaterialPageRoute(builder: (context) {
          const TextStyle ts = TextStyle(fontSize: 16, color: Colors.white, fontWeight: FontWeight.normal);
          return const Scaffold(backgroundColor: Colors.deepPurple,
            body: Center(
            child: Scrollbar(child:
              SingleChildScrollView(
                child: Column(children: [
                  Text("发生了什么？", textScaleFactor: 2, style: ts, textAlign: TextAlign.center,),
                  Text("终端异常退出, 返回错误码9\n此错误通常是高版本安卓系统(12+)限制进程造成的, \n可以使用以下工具修复:", style: ts, textAlign: TextAlign.center),
                  SelectableText("https://www.vmos.cn/zhushou.htm", style: ts, textAlign: TextAlign.center),
                  Text("(复制链接到浏览器查看)", style: ts, textAlign: TextAlign.center),
                ]),
              )
            )
          ));
        }));
      }
    });
    G.terminal.onOutput = (data) {
      G.pty.write(const Utf8Encoder().convert(data));
    };
    G.terminal.onResize = (w, h, pw, ph) {
      G.pty.resize(h, w);
    };

  }

  
  static Future<void> setupBootstrap() async {
    Util.createDirFromString("${G.dataPath}/share");
    Util.createDirFromString("${G.dataPath}/debian");
    Util.createDirFromString("${G.dataPath}/tmp");
    await Util.copyAsset(
    "assets/assets.zip",
    "${G.dataPath}/assets.zip",
    );
    for (String name in ["xaa", "xab", "xac", "xad", "xae", "xaf", "xag", "xah", "xai", "xaj", "xak", "xal", "xam", "xan", "xao"]) {
    //for (String name in ["xaa"]) {
      await Util.copyAsset("assets/$name", "${G.dataPath}/$name");
    }
    await Util.copyAsset(
    "assets/busybox",
    "${G.dataPath}/busybox",
    );
    await Util.execute(
"""
cd ${G.dataPath}
chmod +x busybox
${G.dataPath}/busybox unzip assets.zip
chmod -R +x bin/*
chmod -R +x libexec/proot/*
cat xa* | ${G.dataPath}/busybox tar x -J -v -C debian
${G.dataPath}/busybox rm -rf assets.zip xa*
""");
  }

  static Future<void> launchDefaultContainer() async {
    Util.termWrite(
"""
cd ${G.dataPath}/..
export TMPDIR=\$PWD/cache
cd ${G.dataPath}
export HOME=\$PWD/share
export LD_LIBRARY_PATH=\$PWD/bin
\$PWD/bin/pulseaudio -F \$PWD/bin/pulseaudio.conf >/dev/null 2>&1 & 
export PROOT_TMP_DIR=\$PWD/tmp
export PROOT_LOADER=\$PWD/libexec/proot/loader
export PROOT_LOADER_32=\$PWD/libexec/proot/loader32
${G.dataPath}/bin/proot --mute-setxid --tcsetsf2tcsetsw --root-id --pwd=/root --rootfs=${G.dataPath}/debian --mount=/system --mount=/apex --kill-on-exit --mount=/storage:/storage --mount=${G.dataPath}/share:/media/share -L --link2symlink --mount=/proc:/proc --mount=/dev:/dev --mount=${G.dataPath}/debian/tmp:/dev/shm --mount=/dev/urandom:/dev/random --mount=/proc/self/fd:/dev/fd --mount=/proc/self/fd/0:/dev/stdin --mount=/proc/self/fd/1:/dev/stdout --mount=/proc/self/fd/2:/dev/stderr --mount=/dev/null:/dev/tty0 --mount=/dev/null:/proc/sys/kernel/cap_last_cap --mount=/storage/self/primary/Fonts:/usr/share/fonts/wpsm --mount=/storage/self/primary/AppFiles/Fonts:/usr/share/fonts/yozom --mount=/storage/self/primary:/media/storage/shared --mount=/storage/self/primary/Pictures:/media/storage/Pictures --mount=/storage/self/primary/Music:/media/storage/Music --mount=/storage/self/primary/Movies:/media/storage/Movies --mount=/storage/self/primary/Download:/media/storage/Download --mount=/storage/self/primary/DCIM:/media/storage/DCIM --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.stat:/proc/stat --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/.tmoe-container.version:/proc/version --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/bus:/proc/bus --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/buddyinfo:/proc/buddyinfo --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/cgroups:/proc/cgroups --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/consoles:/proc/consoles --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/crypto:/proc/crypto --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/devices:/proc/devices --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/diskstats:/proc/diskstats --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/execdomains:/proc/execdomains --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/fb:/proc/fb --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/filesystems:/proc/filesystems --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/interrupts:/proc/interrupts --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/iomem:/proc/iomem --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/ioports:/proc/ioports --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/kallsyms:/proc/kallsyms --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/keys:/proc/keys --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/key-users:/proc/key-users --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/kpageflags:/proc/kpageflags --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/loadavg:/proc/loadavg --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/locks:/proc/locks --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/misc:/proc/misc --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/modules:/proc/modules --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/pagetypeinfo:/proc/pagetypeinfo --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/partitions:/proc/partitions --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/sched_debug:/proc/sched_debug --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/softirqs:/proc/softirqs --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/timer_list:/proc/timer_list --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/uptime:/proc/uptime --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/vmallocinfo:/proc/vmallocinfo --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/vmstat:/proc/vmstat --mount=${G.dataPath}/debian/usr/local/etc/tmoe-linux/proot_proc/zoneinfo:/proc/zoneinfo /usr/bin/env -i HOSTNAME=TINY HOME=/root USER=root TERM=xterm-256color SDL_IM_MODULE=fcitx XMODIFIERS=\\@im=fcitx QT_IM_MODULE=fcitx GTK_IM_MODULE=fcitx TMOE_CHROOT=false TMOE_PROOT=true TMPDIR=/tmp DISPLAY=:2 PULSE_SERVER=tcp:127.0.0.1:4713 LANG=zh_CN.UTF-8 SHELL=/bin/zsh PATH=/usr/local/sbin:/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin:/usr/games:/usr/local/games /bin/zsh -l
startnovnc""");
  }

  static Future<void> waitForConnection() async {
    // Future<bool> testConnection(String url) async {
    //   try {
    //     return (await http.get(Uri.parse(url))).statusCode == 200;
    //   } catch (e) {
    //     return false;
    //   }
    // }
    // for (;;) {
    //   await Future.delayed(const Duration(milliseconds: 1000), () async {
    //     print("meow");
    //     if (await testConnection(G.vncUrl)) {
    //       return;
    //     }
    //   }
    //   );
    // }
    await retry(
      // Make a GET request
      () => http.get(Uri.parse(G.vncUrl)).timeout(const Duration(milliseconds: 250)),
      // Retry on SocketException or TimeoutException
      retryIf: (e) => e is SocketException || e is TimeoutException,
    );
  }

  static Future<void> launchBrowser() async {
    G.controller.loadRequest(Uri.parse(G.vncUrl));
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
              LogicalKeyboardKey.arrowDown
            }.contains(event.logicalKey)) {
              return KeyEventResult.skipRemainingHandlers;
            }
          }
          return KeyEventResult.ignored;
        },
        child: WebViewWidget(controller: G.controller),
      );
    }));
  }

  static Future<void> workflow() async {
    grantPermissions();
    await initData();
    await initTerminal();
    if (Util.isFirstTime()) {
      await setupBootstrap();
    }
    launchDefaultContainer();
    waitForConnection().then((value) => launchBrowser());
  }
}


