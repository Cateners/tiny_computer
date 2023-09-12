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
import 'dart:convert';
import 'dart:math';
//import 'dart:convert';

import 'package:url_launcher/url_launcher.dart';
import 'package:flutter/material.dart';
import 'package:xterm/xterm.dart';
//import 'package:xterm/flutter.dart';
import 'package:tiny_computer/workflow.dart';

import 'package:unity_ads_plugin/unity_ads_plugin.dart';

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
  final List<bool> _expandState = [false, false, false, false];
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
第一次加载, 大概需要5到10分钟...
请不要在安装时退出软件

一些注意事项：
此软件以GPL协议免费开源
如果是买的就是被骗了, 请举报
源代码在这里: https://github.com/Cateners/tiny_computer
软件也会第一时间在这里更新
请尽可能在这里下载软件, 确保是正版

常见问题：
如果你的系统版本大于等于android 12
可能会在使用过程中异常退出(返回错误码9)
届时本软件会提供方案指引你修复
并不难
此软件因为没有权限
所以不能帮你修复

如果你给了存储权限
那么可以从主目录下的
storage目录访问手机存储

如果认为界面大小比例不合适
可以通过调整左栏设置-高级设置里的scale
快捷调整界面缩放
这个功能是原本的noVNC里没有的哦!
具体的改动可以在这里看到:
https://github.com/Cateners/noVNC/tree/scale_factor

其余两个选项是
quality(图像质量)和compression(压缩等级)
...是noVNC中本来就有的选项。
如果感觉界面卡卡的
可以适当调低

如果你想安装其他软件
可以在网上搜索
"ubuntu安装xxx教程"
"linux安装xxx教程"等等
本软件也提供一些基本软件安装按钮
包括图形处理, 视频剪辑, 科学计算相关的软件
稍后你就会看到

如果你想安装更多字体
在给了存储权限的情况下
直接将字体复制到手机存储的Fonts文件夹即可
一些常用的办公字体
可以在Windows电脑的C:\\Windows\\Fonts文件夹找到
由于可能的版权问题
软件不能帮你做

关于中文输入的问题
强烈建议不要使用安卓中文输入法直接输入中文
而是使用英文键盘通过容器的输入法(Ctrl+空格切换)输入中文
避免丢字错字

在之前的版本中有网友反馈过这些问题
还请注意：
三星Galaxy S21 Ultra, 安卓13, 黑屏
红米Note 12, 安卓13（miui14）, 黑屏
红米Note 11T Pro+， miui13.0.4，“无法连接”
Vivo Pad，安卓13，看不见鼠标移动
关于这个
我目前没有什么好的解决办法
(毕竟我没有这些设备
也不方便定位原因)
如果你遇到了类似问题
不管解没解决
都可以去https://github.com/Cateners/tiny_computer/issues/1留个言

感谢使用!

项目原理：
项目采用proot运行ubuntu虚拟容器系统
图形界面是经过kali-undercover提供的Win10主题美化的xfce
系统预装了WPS, VSCode、火狐浏览器和fcitx输入法

这个项目没有使用Termux
因为我不太喜欢Termux的路径硬编码
路径硬编码会导致软件在多用户/分身等场景无法使用
当然这样一来就用不了Termux的软件生态了

...如果你不知道什么是Termux
那也无所谓
即使完全不懂原理也不影响使用本软件
但假如有一天你有了其他高级需求
比如想换系统、换架构等等
那么请学习并使用Termux
届时本软件的使命已经达成...

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
          const Padding(padding: EdgeInsets.all(8), child: Text("""
这个软件预计会有一些广告
之前的版本中说过
如果完整地看了"隐私政策"与"服务条款"的话
就可以选择关闭广告
但因为那两个玩意一直都不知道怎么写
想想还是算了
但软件里的广告还是可以关闭的

本软件的广告分为横幅广告和视频广告
横幅广告在终端和控制页面的顶端出现
只需完整观看一次视频广告即可永久关闭
视频广告目前只在"关闭横幅广告"和"启用终端"两个功能中出现
看一个视频即可永久启用对应功能
我认为还是比较良心的...吧?

总之为了良好的体验
在图形界面是不会出现广告的
这点还请放心
""")),
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

  //高级设置，全局设置
  final List<bool> _expandState = [false, false, false];

  bool bannerAdsFailedToLoad = false;

  //安装完成了吗？
  //完成后从加载界面切换到主界面
  bool isLoadingComplete = false;
  //主界面索引
  int pageIndex = 0;

  final ButtonStyle buttonStyle = OutlinedButton.styleFrom(
    tapTargetSize: MaterialTapTargetSize.shrinkWrap,
     minimumSize: const Size(0, 0), padding: const EdgeInsets.fromLTRB(4, 2, 4, 2)
   );

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
        title: Text(isLoadingComplete?Util.getCurrentProp("name"):widget.title),
      ),
      body: isLoadingComplete?Column(mainAxisSize: MainAxisSize.min, children: [
        G.prefs.getBool("isBannerAdsClosed")!||bannerAdsFailedToLoad?SizedBox.fromSize(size: const Size.square(0),):UnityBannerAd(
          placementId: AdManager.bannerAdPlacementId,
          onLoad: (placementId) => print('Banner loaded: $placementId'),
          onClick: (placementId) => print('Banner clicked: $placementId'),
          onFailed: (placementId, error, message) {
            print('Banner Ad $placementId failed: $error $message');
            setState(() {
              bannerAdsFailedToLoad = true;
            });
          },
        ),Expanded(flex: 1, child: AnimatedSwitcher(
        duration: const Duration(milliseconds: 256),
        child: [TerminalView(G.termPtys[G.currentContainer]!.terminal), Padding(
              padding: const EdgeInsets.all(8),
              child: Scrollbar(child: SingleChildScrollView(child: Column(
                children: [
                  const Padding(
                    padding: EdgeInsets.all(16),
                    child: FractionallySizedBox(
                      widthFactor: 0.4,
                      child: Image(
                        image: AssetImage("images/icon.png")
                      )
                    ),
                  ),
                  /*Padding(
                    padding: const EdgeInsets.fromLTRB(0, 0, 0, 8),
                    child: Text(Util.getCurrentProp("name"), textScaleFactor: 2),
                  ),*/
                  Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: Util.getCurrentProp("commands")
                    .asMap().entries.map<Widget>((e) {
                    return OutlinedButton(style: buttonStyle, child: Text(e.value["name"]!), onPressed: () {
                      setState(() {
                        Util.termWrite(e.value["command"]!);
                        pageIndex = 0;
                      });
                    }, onLongPress: () {
                      String name = e.value["name"]!;
                      String command = e.value["command"]!;
                      showDialog(context: context, builder: (context) {
                        return AlertDialog(title: const Text("指令编辑"), content: SingleChildScrollView(child: Column(children: [
                          TextFormField(initialValue: name, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "指令名称"), onChanged: (value) {
                            name = value;
                          }),
                          SizedBox.fromSize(size: const Size.square(8)),
                          TextFormField(maxLines: null, initialValue: command, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "指令内容"), onChanged: (value) {
                            command = value;
                          }),
                        ])), actions: [
                          TextButton(onPressed:() async {
                            await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                              ..removeAt(e.key));
                            setState(() {});
                            if (!context.mounted) return;
                            Navigator.of(context).pop();
                          }, child: const Text("删除该项")),
                          TextButton(onPressed:() {
                            Navigator.of(context).pop();
                          }, child: const Text("取消")),
                          TextButton(onPressed:() async {
                            await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                              ..setAll(e.key, [{"name": name, "command": command}]));
                            setState(() {});
                            if (!context.mounted) return;
                            Navigator.of(context).pop();
                          }, child: const Text("保存")),
                        ]);
                      },);
                    },);
                  }).toList()..add(OutlinedButton(style: buttonStyle, onPressed:() {
                      String name = "";
                      String command = "";
                      showDialog(context: context, builder: (context) {
                        return AlertDialog(title: const Text("指令编辑"), content: SingleChildScrollView(child: Column(children: [
                          TextFormField(initialValue: name, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "指令名称"), onChanged: (value) {
                            name = value;
                          }),
                          SizedBox.fromSize(size: const Size.square(8)),
                          TextFormField(maxLines: null, initialValue: command, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "指令内容"), onChanged: (value) {
                            command = value;
                          }),
                        ])), actions: [
                          TextButton(onPressed:() {
                            Navigator.of(context).pop();
                          }, child: const Text("取消")),
                          TextButton(onPressed:() async {
                            await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                              ..add({"name": name, "command": command}));
                            setState(() {});
                            if (!context.mounted) return;
                            Navigator.of(context).pop();
                          }, child: const Text("添加")),
                        ]);
                      },);
                  }, child: const Text("添加快捷指令")))),
                  Padding(padding: const EdgeInsets.all(8), child: Card(child: Padding(padding: const EdgeInsets.all(8), child: 
                        Column(children: [
                          ExpansionPanelList(
                            elevation: 1,
                            expandedHeaderPadding: const EdgeInsets.all(0),
                            expansionCallback: (panelIndex, isExpanded) {
                            setState(() {
                              _expandState[panelIndex] = isExpanded;
                            });
                          },children: [
                            ExpansionPanel(
                              isExpanded: _expandState[0],
                              headerBuilder: ((context, isExpanded) {
                                return const ListTile(title: Text("高级设置"), subtitle: Text("修改后重启生效"));
                              }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
                                TextFormField(maxLines: null, initialValue: Util.getCurrentProp("name"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "容器名称"), onChanged: (value) async {
                                  await Util.setCurrentProp("name", value);
                                  setState(() {});
                                }),
                                SizedBox.fromSize(size: const Size.square(8)),
                                TextFormField(maxLines: null, initialValue: Util.getCurrentProp("boot"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "启动命令"), onChanged: (value) async {
                                  await Util.setCurrentProp("boot", value);
                                }),
                                SizedBox.fromSize(size: const Size.square(8)),
                                TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vnc"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "vnc启动命令"), onChanged: (value) async {
                                  await Util.setCurrentProp("vnc", value);
                                }),
                                SizedBox.fromSize(size: const Size.square(8)),
                                TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vncUrl"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "网页跳转地址"), onChanged: (value) async {
                                  await Util.setCurrentProp("vncUrl", value);
                                }),
                              ],))),
                            ExpansionPanel(
                              isExpanded: _expandState[1],
                              headerBuilder: ((context, isExpanded) {
                                return const ListTile(title: Text("全局设置"), subtitle: Text("在这里关广告、开启终端编辑"));
                              }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
                                TextFormField(maxLines: null, initialValue: G.prefs.getString("defaultAudioPort"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "pulseaudio接收端口"), onChanged: (value) async {
                                  await G.prefs.setString("defaultAudioPort", value);
                                }),
                                SizedBox.fromSize(size: const Size.square(8)),
                                SwitchListTile(title: const Text("关闭横幅广告"), value: G.prefs.getBool("isBannerAdsClosed")!, onChanged:(value) {
                                  if (value && (G.prefs.getInt("adsWatchedTotal")! == 0)) {
                                    ScaffoldMessenger.of(context).hideCurrentSnackBar();
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      const SnackBar(content: Text("观看一个视频广告解锁><"))
                                    );
                                    return;
                                  }
                                  G.prefs.setBool("isBannerAdsClosed", value);
                                  setState(() {});
                                },),
                                SizedBox.fromSize(size: const Size.square(8)),
                                SwitchListTile(title: const Text("启用终端"), value: G.prefs.getBool("isTerminalWriteEnabled")!, onChanged:(value) {
                                  if (value && (G.prefs.getInt("adsWatchedTotal")! == 0)) {
                                    ScaffoldMessenger.of(context).hideCurrentSnackBar();
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      SnackBar(content: const Text("观看一个视频广告解锁><"), action: SnackBarAction(label: "啊?", onPressed: () {
                                        G.prefs.setBool("isTerminalWriteEnabled", value);
                                        setState(() {});
                                      },))
                                    );
                                    return;
                                  }
                                  G.prefs.setBool("isTerminalWriteEnabled", value);
                                  setState(() {});
                                },),
                                SizedBox.fromSize(size: const Size.square(8)),
                                SwitchListTile(title: const Text("开启时启动图形界面"), value: G.prefs.getBool("autoLaunchVnc")!, onChanged:(value) {
                                  G.prefs.setBool("autoLaunchVnc", value);
                                  setState(() {});
                                },),
                              ],))),
                            ExpansionPanel(
                              isExpanded: _expandState[2],
                              headerBuilder: ((context, isExpanded) {
                                return const ListTile(title: Text("广告记录"));
                              }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
                                OutlinedButton(child: const Text("看一个广告"), onPressed: () {
                                   if (AdManager.placements[AdManager.rewardedVideoAdPlacementId]!) {
                                    AdManager.showAd(AdManager.rewardedVideoAdPlacementId, () {
                                      final bonus = Util.getRandomBonus();
                                      Util.applyBonus(bonus);
                                      ScaffoldMessenger.of(context).hideCurrentSnackBar();
                                      ScaffoldMessenger.of(context).showSnackBar(
                                        SnackBar(content: Text("你获得了 ${bonus["name"]}*${bonus["amount"]}"))
                                      );
                                      setState(() {
                                        
                                      });
                                    }, () {
                                      ScaffoldMessenger.of(context).hideCurrentSnackBar();
                                      ScaffoldMessenger.of(context).showSnackBar(
                                        const SnackBar(content: Text("已经看5个广告了, 今天也非常感谢><"))
                                      );
                                    });
                                   }
                                  }),
                                  const SizedBox.square(dimension: 8),
                                  Text(G.prefs.getStringList("adsBonus")!.map((element) {
                                    final e = jsonDecode(element);
                                    return e["amount"]==0?"":"${e["name"]}*${e["amount"]}\n";
                                  }).join())
                              ],))),
                          ],),
                          SizedBox.fromSize(size: const Size.square(8)),
                          const InfoPage()
                        ]
                    )
                  ))
                  ,)
                ]
              )))
            )][pageIndex],
        transitionBuilder: (child, animation) {
          return FadeTransition(opacity: animation, child: child);
        },))]):const LoadingPage(),
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
