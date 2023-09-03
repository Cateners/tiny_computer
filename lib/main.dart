// main.dart  --  This file is part of tiny_computer.               
                                                                        
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


import 'dart:async';
import 'dart:math';

import 'package:url_launcher/url_launcher.dart';
import 'package:flutter/material.dart';
import 'package:xterm/xterm.dart';
//import 'package:xterm/flutter.dart';
import 'package:tiny_computer/workflow.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Tiny Computer',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
        //fontFamily: "FiraCode",
      ),
      home: const MyHomePage(title: 'Tiny Computer'),
    );
  }
}


class FakeLoadingStatus extends StatefulWidget {
  const FakeLoadingStatus({super.key});

  @override
  State<FakeLoadingStatus> createState() => _FakeLoadingStatusState();
}

class _FakeLoadingStatusState extends State<FakeLoadingStatus> {

  double _progressT = 0;
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    _timer = Timer.periodic(const Duration(milliseconds: 100), (timer) {
      setState(() {
        _progressT += 0.1;
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return LinearProgressIndicator(value: 1 - pow(10, _progressT / -300).toDouble());
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}

class InfoPage extends StatefulWidget {
  const InfoPage({super.key});

  @override
  State<InfoPage> createState() => _InfoPageState();
}

class _InfoPageState extends State<InfoPage> {
  final List<bool> _expandState = [true, false, false, false];
  @override
  Widget build(BuildContext context) {
    return ExpansionPanelList(
      elevation: 1,
      expandedHeaderPadding: const EdgeInsets.all(0),
      expansionCallback: (panelIndex, isExpanded) {
        setState(() {
          _expandState[panelIndex] = isExpanded;
        });
      },
    children: [
      ExpansionPanel(
        headerBuilder: (context, isExpanded) {
          return const ListTile(title: Text("使用说明"));
        },
        body: const Padding(padding: EdgeInsets.all(8), child: Text("""
Loading... Please wait about 5 to 10 minutes for first time
第一次加载，大概需要几分钟...
请不要在安装时退出软件
你可以在等待的同时阅读“隐私政策”和“服务条款”,
阅读完后可以关闭广告
...不过我还没写(广告也没放)

一些注意事项：
此软件免费开源
项目地址: https://github.com/Cateners/tiny_computer
如果是买的就是被骗了, 请举报!
((然后请我喝水!!!!!!)(不是))

如果遇到android 12的signal 9问题
请自行查找教程修复
并不难
此软件因为没有权限
所以不能帮你修复
一般只要你以前修复过(Tmoe脚本、Vmos助手、全手动adb等等)
现在就不用再次修复

这个项目没有使用Termux
因为我不太喜欢Termux的路径硬编码
路径硬编码会导致软件在多用户/分身等场景无法使用
当然这样一来就用不了Termux的软件生态了

项目采用proot运行tmoe的debian12(xfce)
debian系统里预装了WPS, VSCode和fcitx输入法
界面是webview+noVNC

目前不能安装其他软件
安装的其他软件无法读写容器
但可以访问手机存储
我也不太清楚原因

如果你给了存储权限
那么可以从storage目录访问手机目录
所以任何时候都不要尝试rm -rf /*

(顺带一提, 全部解压完大概需要7GB空间
解压途中占用空间可能达到9GB
请确保有足够的空间
(这样真的Tiny吗><))

"""
        )),
        isExpanded: _expandState[0],
      ),
      ExpansionPanel(
        isExpanded: _expandState[1],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("隐私政策"));
        }), body: const Padding(padding: EdgeInsets.all(8), child: Text("不知道怎么写"))),
      ExpansionPanel(
        isExpanded: _expandState[2],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("服务条款"));
        }), body: const Padding(padding: EdgeInsets.all(8), child: Text("要写什么"))),
      ExpansionPanel(
        isExpanded: _expandState[3],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("支持作者"));
        }), body: Column(
        children: [
          const Padding(padding: EdgeInsets.all(8), child: Text("请我喝一杯水吧")),
          const FractionallySizedBox(
            widthFactor: 0.8,
            child: Image(image: AssetImage("images/alipay.png"))
          ),
          ElevatedButton(
            onPressed: () {
              launchUrl(Uri.parse("https://github.com/Cateners/tiny_computer"));
            },
            child: const Text("项目地址"),
          ),
        ]
      )),
    ],
  );
  }
}

class LoadingPage extends StatelessWidget {
  const LoadingPage({super.key});
  @override
  Widget build(BuildContext context) {
    return const Padding(
              padding: EdgeInsets.all(8),
              child: Column(
                children: [
                  Padding(
                    padding: EdgeInsets.fromLTRB(16, 16, 16, 16),
                    child: FractionallySizedBox(
                      widthFactor: 0.4,
                      child: Image(
                        image: AssetImage("images/icon.png")
                      )
                    ),
                  ),
                  Padding(
                    padding: EdgeInsets.fromLTRB(0, 0, 0, 8),
                    child: Text("小小电脑", textScaleFactor: 2),
                  ),
                  FakeLoadingStatus(),
                  Expanded(child:
                  Padding(padding: EdgeInsets.all(8), child: Card(child: Padding(padding: EdgeInsets.all(8), child: 
                  
                    Scrollbar(child:
                      SingleChildScrollView(
                        child: InfoPage()
                      )
                    )
                  ))
                  ,))
                ]
              )
            );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {

  //安装完成了吗？
  //完成后从加载界面切换到主界面
  bool isLoadingComplete = false;
  //主界面索引
  int pageIndex = 0;

  @override
  Widget build(BuildContext context) {

  G.homePageStateContext = context;

  if (!isLoadingComplete) {
    Workflow.workflow().then((value) {
      setState(() {
        isLoadingComplete = true;
      });
    });
  }


    return Scaffold(
      appBar: AppBar(
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        title: Text(widget.title),
      ),
      body: isLoadingComplete?TerminalView(G.terminal):const LoadingPage(),
      bottomNavigationBar: Visibility(visible: isLoadingComplete,
        child: BottomNavigationBar(currentIndex: pageIndex, 
          onTap: (index) {
            setState(() {
              pageIndex = index;
            });
          },
          items: const [
            BottomNavigationBarItem(icon: Icon(Icons.monitor), label: "终端"),
            BottomNavigationBarItem(icon: Icon(Icons.video_settings), label: "控制"),
          ],
        )
      ),
      floatingActionButton: Visibility(visible: isLoadingComplete && (pageIndex == 0),
        child: FloatingActionButton(
          onPressed: () => Workflow.launchBrowser(),
          tooltip: "进入图形界面",
          child: const Icon(Icons.play_arrow),
        ),
      ), // This trailing comma makes auto-formatting nicer for build methods.
    );
  }
}
