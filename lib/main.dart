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
//import 'dart:io';
import 'dart:math';
//import 'dart:convert';

//import 'package:flutter/services.dart';

import 'package:clipboard/clipboard.dart';
import 'package:dynamic_color/dynamic_color.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter_pty/flutter_pty.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:flutter/services.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:flutter/material.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
import 'package:xterm/xterm.dart';
//import 'package:xterm/flutter.dart';
import 'package:tiny_computer/workflow.dart';

import 'package:ffmpeg_kit_flutter_minimal/ffmpeg_kit.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return DynamicColorBuilder(
        builder: (ColorScheme? lightDynamic, ColorScheme? darkDynamic) {
          return MaterialApp(
            title: 'Tiny Computer',
            theme: ThemeData(
              colorScheme: lightDynamic,
              useMaterial3: true,
              //fontFamily: "FiraCode",
            ),
            darkTheme: ThemeData(
              colorScheme: darkDynamic,
              useMaterial3: true,
              //fontFamily: "FiraCode",
            ),
            home: const MyHomePage(title: 'Tiny Computer'),
          );
        }
    );
  }
}


//限制最大宽高比1:1
//Limit maximum aspect ratio to 1:1
class AspectRatioMax1To1 extends StatelessWidget {
  final Widget child;
  //final double aspectRatio;

  const AspectRatioMax1To1({super.key, required this.child/*, required this.aspectRatio*/});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final s = MediaQuery.of(context).size;
        //double size = (s.width < s.height * aspectRatio) ? s.width : (s.height * aspectRatio);
        double size = s.width < s.height ? constraints.maxWidth : s.height;

        return Center(
          child: SizedBox(
            width: size,
            height: constraints.maxHeight,
            child: child,
          ),
        );
      },
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

class SettingPage extends StatefulWidget {
  const SettingPage({super.key});

  @override
  State<SettingPage> createState() => _SettingPageState();
}

class _SettingPageState extends State<SettingPage> {

  final List<bool> _expandState = [false, false, false, false, false, false, false];

  @override
  Widget build(BuildContext context) {
    return ExpansionPanelList(
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
          return const ListTile(title: Text("Advanced Settings"), subtitle: Text("Restart to apply changes"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Reset Boot Command"), onPressed: () {
              showDialog(context: context, builder: (context) {
                return AlertDialog(title: const Text("Note"), content: const Text("Are you sure you want to reset the boot command?"), actions: [
                  TextButton(onPressed:() {
                    Navigator.of(context).pop();
                  }, child: const Text("Cancel")),
                  TextButton(onPressed:() async {
                    await Util.setCurrentProp("boot", D.boot);
                    G.bootTextChange.value = !G.bootTextChange.value;
                    if (!context.mounted) return;
                    Navigator.of(context).pop();
                  }, child: const Text("Yes")),
                ]);
              });
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Signal9 Error Page"), onPressed: () async {
              await D.androidChannel.invokeMethod("launchSignal9Page", {});
            }),
          ]),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("name"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Container Name"), onChanged: (value) async {
            await Util.setCurrentProp("name", value);
            //setState(() {});
          }),
          const SizedBox.square(dimension: 8),
          ValueListenableBuilder(valueListenable: G.bootTextChange, builder:(context, v, child) {
            return TextFormField(maxLines: null, initialValue: Util.getCurrentProp("boot"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Boot Command"), onChanged: (value) async {
              await Util.setCurrentProp("boot", value);
            });
          }),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vnc"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "VNC Boot Command"), onChanged: (value) async {
            await Util.setCurrentProp("vnc", value);
          }),
          const SizedBox.square(dimension: 8),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("You can use Tiny Computer on all devices on the current network (e.g., phones, computers connected to the same WiFi).\n\nClick the button below to share the link to other devices and open it with a browser."),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Copy Share Link"), onPressed: () async {
              final String? ip = await NetworkInfo().getWifiIP();
              if (!context.mounted) return;
              if (G.wasX11Enabled) {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text("This function is not available when using X11"))
                );
                return;
              }
              if (ip == null) {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text("Unable to get IP address"))
                );
                return;
              }
              FlutterClipboard.copy((Util.getCurrentProp("vncUrl") as String).replaceAll(RegExp.escape("localhost"), ip)).then((value) {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text("Share link copied"))
                );
              });
            }),
          ]),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vncUrl"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Web Redirect Address"), onChanged: (value) async {
            await Util.setCurrentProp("vncUrl", value);
          }),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vncUri"), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "VNC Link"), onChanged: (value) async {
            await Util.setCurrentProp("vncUri", value);
          }),
          const SizedBox.square(dimension: 8),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[1],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Global Settings"), subtitle: Text("Enable terminal editing here"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: (Util.getGlobal("termMaxLines") as int).toString(), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Terminal Max Lines (Restart app to apply)"),
            keyboardType: TextInputType.number,
            validator: (value) {
              return Util.validateBetween(value, 1024, 2147483647, () async {
                await G.prefs.setInt("termMaxLines", int.parse(value!));
              });
            },),
          const SizedBox.square(dimension: 16),
          TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: (Util.getGlobal("defaultAudioPort") as int).toString(), decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "PulseAudio Listening Port"),
            keyboardType: TextInputType.number,
            validator: (value) {
              return Util.validateBetween(value, 0, 65535, () async {
                await G.prefs.setInt("defaultAudioPort", int.parse(value!));
              });
            }
          ),
          const SizedBox.square(dimension: 16),
          SwitchListTile(title: const Text("Enable Terminal"), value: Util.getGlobal("isTerminalWriteEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isTerminalWriteEnabled", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Enable Terminal Keypad"), value: Util.getGlobal("isTerminalCommandsEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isTerminalCommandsEnabled", value);
            setState(() {
              G.terminalPageChange.value = !G.terminalPageChange.value;
            });
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Terminal Sticky Keys"), value: Util.getGlobal("isStickyKey") as bool, onChanged:(value) {
            G.prefs.setBool("isStickyKey", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Keep Screen On"), value: Util.getGlobal("wakelock") as bool, onChanged:(value) {
            G.prefs.setBool("wakelock", value);
            WakelockPlus.toggle(enable: value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("The following options will take effect the next time the software is started."),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Start GUI on Launch"), value: Util.getGlobal("autoLaunchVnc") as bool, onChanged:(value) {
            G.prefs.setBool("autoLaunchVnc", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Reinstall Bootstrap Package"), value: Util.getGlobal("reinstallBootstrap") as bool, onChanged:(value) {
            G.prefs.setBool("reinstallBootstrap", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("getifaddrs Bridge"), subtitle: const Text("Fix getifaddrs no permission on Android 13 devices"), value: Util.getGlobal("getifaddrsBridge") as bool, onChanged:(value) {
            G.prefs.setBool("getifaddrsBridge", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Spoof system as UOS"), subtitle: const Text("Fix UOS WeChat not starting"), value: Util.getGlobal("uos") as bool, onChanged:(value) {
            G.prefs.setBool("uos", value);
            setState(() {});
          },),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[2],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Display Settings"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const SizedBox.square(dimension: 16),
          const Text("""AVNC can provide a better control experience compared to noVNC;
Such as touchpad control, two-finger click to pop up keyboard, automatic clipboard, picture-in-picture mode, etc. This is an experimental feature."""),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("AVNC Settings"), onPressed: () async {
              await D.androidChannel.invokeMethod("launchPrefsPage", {});
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("About AVNC"), onPressed: () async {
              await D.androidChannel.invokeMethod("launchAboutPage", {});
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("AVNC Startup Resolution Settings"), onPressed: () async {
              final s = WidgetsBinding.instance.platformDispatcher.views.first.physicalSize;
              final w0 = max(s.width, s.height);
              final h0 = min(s.width, s.height);
              String w = (w0 * 0.75).round().toString();
              String h = (h0 * 0.75).round().toString();
              showDialog(context: context, builder: (context) {
                return AlertDialog(title: const Text("Resolution Settings"), content: SingleChildScrollView(child: Column(children: [
                  Text("Your device screen resolution is ${w0.round()}x${h0.round()}"),
                  const SizedBox.square(dimension: 8),
                  TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: w, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Width"), keyboardType: TextInputType.number,
                    validator: (value) {
                      return Util.validateBetween(value, 200, 7680, () {
                        w = value!;
                      });
                    }
                  ),
                  const SizedBox.square(dimension: 8),
                  TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: h, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Height"), keyboardType: TextInputType.number,
                    validator: (value) {
                      return Util.validateBetween(value, 200, 7680, () {
                        h = value!;
                      });
                    }
                  ),
                ])), actions: [
                  TextButton(onPressed:() {
                    Navigator.of(context).pop();
                  }, child: const Text("Cancel")),
                  TextButton(onPressed:() async {
                    Util.termWrite("""sed -i -E "s@(geometry)=.*@\\1=${w}x${h}@" /etc/tigervnc/vncserver-config-tmoe
sed -i -E "s@^(VNC_RESOLUTION)=.*@\\1=${w}x${h}@" \$(command -v startvnc)""");
                    ScaffoldMessenger.of(context).hideCurrentSnackBar();
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text("${w}x${h}. Takes effect on next startup"))
                    );
                    if (!context.mounted) return;
                    Navigator.of(context).pop();
                  }, child: const Text("Save")),
                ]);
              });
            }),
          ]),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Use AVNC by default"), subtitle: const Text("Takes effect on next startup"), value: Util.getGlobal("useAvnc") as bool, onChanged:(value) {
            G.prefs.setBool("useAvnc", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("""Termux X11 can provide faster speed than VNC, and compatibility may be better in some cases.
Supports DRI3 (needs to be enabled in graphics acceleration), which can bring considerable performance improvements.
With version iterations, Termux X11 now also supports features like bidirectional clipboard.
This is an experimental feature! If you get a black screen, please try completely closing this application and restarting it."""),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Termux X11 Preferences"), onPressed: () async {
              await D.androidChannel.invokeMethod("launchX11PrefsPage", {});
            }),
          ]),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Use Termux X11 by default"), subtitle: const Text("Do not use VNC. Takes effect on restart"), value: Util.getGlobal("useX11") as bool, onChanged:(value) {
            G.prefs.setBool("useX11", value);
            if (!value && Util.getGlobal("dri3")) {
              G.prefs.setBool("dri3", false);
            }
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("""High-resolution support can bring a clearer experience for devices with high-resolution screens!

Note:
The display will become very large after this option is enabled, please set an appropriate resolution.

Some software may have display problems or display speed may slow down."""),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultHidpiOpt") as String, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "HiDPI Environment Variables"),
            onChanged: (value) async {
              await G.prefs.setString("defaultHidpiOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("High Resolution Support"), subtitle: const Text("Takes effect on next startup"), value: Util.getGlobal("isHidpiEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isHidpiEnabled", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[3],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Camera Streaming"), subtitle: Text("Experimental feature"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const Text("After successfully starting the stream, you can click the shortcut command \"Test Stream Pull\" and go to the graphical interface to see the effect.\nNote that this does not create a virtual camera for the system;\nIn addition, using the camera is a high power consumption behavior and should be turned off in time when not in use."),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Request Camera Permission"), onPressed: () {
              Permission.camera.request();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Request Microphone Permission"), onPressed: () {
              Permission.microphone.request();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("View Output"), onPressed: () {
              if (G.streamingOutput == "") {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text("No output"))
                );
                return;
              }
              showDialog(context: context, builder: (context) {
                return AlertDialog(content: SingleChildScrollView(child:
                  Text(G.streamingOutput)), actions: [
                TextButton(onPressed:() {
                  FlutterClipboard.copy(G.streamingOutput).then(( value ) {
                    ScaffoldMessenger.of(context).hideCurrentSnackBar();
                    ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Copied")));
                  });
                  Navigator.of(context).pop();
                }, child: const Text("Copy")),
                TextButton(onPressed:() {
                  Navigator.of(context).pop();
                }, child: const Text("Cancel")),
              ]);
              });
            }),
          ]),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultFFmpegCommand") as String, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "ffmpeg Streaming Command"),
            onChanged: (value) async {
              await G.prefs.setString("defaultFFmpegCommand", value);
            },
          ),
          const SizedBox.square(dimension: 16),
          SwitchListTile(title: const Text("Start Streaming Server"), subtitle: const Text("mediamtx"), value: G.isStreamServerStarted, onChanged:(value) {
            switch (value) {
              case true: {
                G.streamServerPty = Pty.start("/system/bin/sh");
                G.streamServerPty.write(const Utf8Encoder().convert("${G.dataPath}/bin/mediamtx ${G.dataPath}/bin/mediamtx.yml\nexit\n"));
                G.streamServerPty.exitCode.then((value) {
                  G.isStreamServerStarted = false;
                  setState(() {});
                });
              }
              break;
              case false: {
                G.streamServerPty.write(const Utf8Encoder().convert("\x03exit\n"));
              }
              break;
            }
            G.isStreamServerStarted = value;
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Start Streaming"), value: G.isStreaming, onChanged:(value) {
            switch (value) {
              case true: {
                FFmpegKit.execute(Util.getGlobal("defaultFFmpegCommand") as String).then((session) {
                  session.getOutput().then((value) async {
                    G.isStreaming = false;
                    G.streamingOutput = value??"";
                    setState(() {});
                  });
                });
              }
              break;
              case false: {
                FFmpegKit.cancel();
              }
              break;
            }
            G.isStreaming = value;
            setState(() {});
          },),
          const SizedBox.square(dimension: 8)
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[4],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("File Access"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const Text("Get more file permissions here to access special directories."),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Request Storage Permission"), onPressed: () {
              Permission.storage.request();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Request All Files Access Permission"), onPressed: () {
              Permission.manageExternalStorage.request();
            }),
          ]),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[5],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Graphics Acceleration"), subtitle: Text("Experimental feature"));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const Text("""Graphics acceleration can partially utilize the device's GPU to improve system graphics performance, but due to device differences, it may also lead to unstable operation or even abnormal exit of the container system and software.

Virgl can provide acceleration for applications using OpenGL ES."""),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultVirglCommand") as String, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "virgl Server Parameters"),
            onChanged: (value) async {
              await G.prefs.setString("defaultVirglCommand", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultVirglOpt") as String, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "virgl Environment Variables"),
            onChanged: (value) async {
              await G.prefs.setString("defaultVirglOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Enable Virgl Acceleration"), subtitle: const Text("Takes effect on next startup"), value: Util.getGlobal("virgl") as bool, onChanged:(value) {
            G.prefs.setBool("virgl", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("""Devices equipped with Adreno GPUs can usually use the Turnip driver to accelerate software using Vulkan. Combined with the Zink driver, it can accelerate software using OpenGL.

(i.e., devices equipped with Snapdragon processors that are not too new or too old)"""),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultTurnipOpt") as String, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Turnip Environment Variables"),
            onChanged: (value) async {
              await G.prefs.setString("defaultTurnipOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Enable Turnip+Zink Driver"), subtitle: const Text("Takes effect on next startup"), value: Util.getGlobal("turnip") as bool, onChanged:(value) async {
            G.prefs.setBool("turnip", value);
            if (!value && Util.getGlobal("dri3")) {
              G.prefs.setBool("dri3", false);
            }
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Enable DRI3"), subtitle: const Text("Takes effect on next startup"), value: Util.getGlobal("dri3") as bool, onChanged:(value) async {
            if (value && !(Util.getGlobal("turnip") && Util.getGlobal("useX11"))) {
              if (!context.mounted) return;
              ScaffoldMessenger.of(context).hideCurrentSnackBar();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text("DRI3 must be used with Termux X11 and Turnip"))
              );
              return;
            }
            G.prefs.setBool("dri3", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[6],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Windows Application Support"), subtitle: Text("Experimental feature"),);
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const Text("""Run Windows applications using Hangover (running cross-architecture applications in native Wine)!

Running Windows programs requires two layers of emulation (architecture and system), so don't expect high speeds!

For speed, try using it with graphics acceleration. Of course, program crashes or failure to open are also normal.

It is recommended to move the Windows program to be run, along with its program folder, to the desktop.

You need patience. Even if the graphical interface shows nothing. Check the terminal, is it still outputting? Or has it stopped at an error?

Alternatively, search if the official Windows software provides a Linux arm64 version."""),
          const SizedBox.square(dimension: 8),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Install Hangover Stable (10.4)"), onPressed: () async {
              Util.termWrite("bash ~/.local/share/tiny/extra/install-hangover-stable");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Install Hangover Latest (may cause errors)"), onPressed: () async {
              Util.termWrite("bash ~/.local/share/tiny/extra/install-hangover");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Uninstall Hangover"), onPressed: () async {
              Util.termWrite("sudo apt autoremove --purge -y hangover-wine hangover-libarm64ecfex");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: const Text("Clear Wine Data"), onPressed: () async {
              Util.termWrite("rm -rf ~/.wine");
              G.pageIndex.value = 0;
            }),
          ]),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("""Common Wine commands. Click and wait patiently in the graphical interface.

Reference startup time for any program:
Tiger Ben T7510 6GB: Over a minute
Snapdragon 870 12GB: About 10 seconds
"""),
          const SizedBox.square(dimension: 8),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: D.wineCommands.asMap().entries.map<Widget>(
            (e) {
              return OutlinedButton(style: D.commandButtonStyle, child: Text(e.value["name"]!), onPressed: () {
                Util.termWrite("${e.value["command"]!} &");
                G.pageIndex.value = 0;
              });
            }
          ).toList()),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          const Text("The following options will take effect the next time the software is started."),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: const Text("Switch system to Japanese"), subtitle: const Text("Switch system to Japanese"), value: Util.getGlobal("isJpEnabled") as bool, onChanged:(value) async {
            if (value) {
                Util.termWrite("sudo localedef -c -i ja_JP -f UTF-8 ja_JP.UTF-8");
                G.pageIndex.value = 0;
            }
            G.prefs.setBool("isJpEnabled", value);
            setState(() {});
          },),
        ],))),
    ],);
  }
}

class InfoPage extends StatefulWidget {
  final bool openFirstInfo;

  const InfoPage({super.key, this.openFirstInfo=false});

  @override
  State<InfoPage> createState() => _InfoPageState();
}

class _InfoPageState extends State<InfoPage> {
  final List<bool> _expandState = [false, false, false, false, false];
  
  @override
  void initState() {
    super.initState();
    _expandState[0] = widget.openFirstInfo;
  }

  @override
  Widget build(BuildContext context) {
    return ExpansionPanelList(
      elevation: 1,
      expandedHeaderPadding: const EdgeInsets.all(0),
      expansionCallback: (panelIndex, isExpanded) {
        _expandState[panelIndex] = isExpanded;
        WidgetsBinding.instance.addPostFrameCallback((_) => setState(() {}));
      },
    children: [
      ExpansionPanel(
        headerBuilder: (context, isExpanded) {
          return const ListTile(title: Text("Instructions"));
        },
        body: Padding(padding: const EdgeInsets.all(8), child: Column(
          children: [
            ValueListenableBuilder(valueListenable: G.helpText, builder:(context, value, child) {
              return Text(value);
            }),
            const SizedBox.square(dimension: 16),
            Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: D.faq
            .asMap().entries.map<Widget>((e) {
            return OutlinedButton(style: D.commandButtonStyle, child: Text(e.value["q"]!), onPressed: () {
              G.helpText.value = e.value["a"]!;
            });
          }).toList())],
        )),
        isExpanded: _expandState[0],
      ),
      ExpansionPanel(
        isExpanded: _expandState[1],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Open Source Licenses"));
        }), body: const Padding(padding: EdgeInsets.all(8), child: Text("""
Flutter, path_provider, webview_flutter, url_launcher, shared_preferences

Copyright 2014 The Flutter Authors. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Google Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

------------
xterm

The MIT License (MIT)

Copyright (c) 2020 xuty

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------
flutter_pty

The MIT License (MIT)

Copyright (c) 2022 xuty

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------
permission_handler

MIT License

Copyright (c) 2018 Baseflow

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------
http

Copyright 2014, the Dart project authors. 

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Google LLC nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

------------
retry

Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

------------
intl

Copyright 2013, the Dart project authors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Google LLC nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""))),
      ExpansionPanel(
        isExpanded: _expandState[2],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Privacy Policy"));
        }), body: const Padding(padding: EdgeInsets.all(8), child: Text("""
This software does not collect your private information.

Of course, the behavior of software you install or use within the container system (including through shortcut commands) is not under my control, and I am not responsible for it.

The permissions requested by this software are used for the following purposes:
File-related permissions: Used for the system to access phone directories;
Camera and microphone: Used for streaming, not enabled by default.
Notifications and accessibility: Required by Termux X11.
"""))),
      ExpansionPanel(
        isExpanded: _expandState[3],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Terms of Service"));
        }), body: const Padding(padding: EdgeInsets.all(8), child: Text("""
Tiny Computer: A ready-to-use PC-like environment

Copyright (C) 2023 Caten Hu

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
"""))),
      ExpansionPanel(
        isExpanded: _expandState[4],
        headerBuilder: ((context, isExpanded) {
          return const ListTile(title: Text("Support the Author"));
        }), body: Column(
        children: [
          const Padding(padding: EdgeInsets.all(8), child: Text("""
If you find it useful, please recommend it to others!
""")),
          ElevatedButton(
            onPressed: () {
              launchUrl(Uri.parse("https://github.com/Cateners/tiny_computer"), mode: LaunchMode.externalApplication);
            },
            child: const Text("Project Address"),
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
    return Padding(
      padding: const EdgeInsets.all(8),
      child: AspectRatioMax1To1(child:
        Column(
          children: [
            const Padding(
              padding: EdgeInsets.fromLTRB(16, 16, 16, 16),
              child: FractionallySizedBox(
                widthFactor: 0.4,
                child: Image(
                  image: AssetImage("images/icon.png")
                )
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(0, 0, 0, 8),
              child: ValueListenableBuilder(valueListenable: G.updateText, builder:(context, value, child) {
                return Text(value, textScaler: const TextScaler.linear(2));
              }),
            ),
            const FakeLoadingStatus(),
            const Expanded(child: Padding(padding: EdgeInsets.all(8), child: Card(child: Padding(padding: EdgeInsets.all(8), child: 
              Scrollbar(child:
                SingleChildScrollView(
                  child: InfoPage(openFirstInfo: true)
                )
              )
            ))
            ,))
          ]
        )
      )
    );
  }
}

class ForceScaleGestureRecognizer extends ScaleGestureRecognizer {
  @override
  void rejectGesture(int pointer) {
    super.acceptGesture(pointer);
  }
}

RawGestureDetector forceScaleGestureDetector({
  GestureScaleUpdateCallback? onScaleUpdate,
  GestureScaleEndCallback? onScaleEnd,
  Widget? child,
}) {
  return RawGestureDetector(
    gestures: {
      ForceScaleGestureRecognizer:GestureRecognizerFactoryWithHandlers<ForceScaleGestureRecognizer>(() {
        return ForceScaleGestureRecognizer();
      }, (detector) {
        detector.onUpdate = onScaleUpdate;
        detector.onEnd = onScaleEnd;
      })
    },
    child: child,
  );
}

class TerminalPage extends StatelessWidget {
  const TerminalPage({super.key});

  @override
  Widget build(BuildContext context) {
    return Column(children: [Expanded(child: forceScaleGestureDetector(onScaleUpdate: (details) {
        G.termFontScale.value = (details.scale * (Util.getGlobal("termFontScale") as double)).clamp(0.2, 5);
      }, onScaleEnd: (details) async {
        await G.prefs.setDouble("termFontScale", G.termFontScale.value);
      }, child: ValueListenableBuilder(valueListenable: G.termFontScale, builder:(context, value, child) {
        return TerminalView(G.termPtys[G.currentContainer]!.terminal, textScaler: TextScaler.linear(G.termFontScale.value), keyboardType: TextInputType.multiline);
      },) )), 
      ValueListenableBuilder(valueListenable: G.terminalPageChange, builder:(context, value, child) {
      return (Util.getGlobal("isTerminalCommandsEnabled") as bool)?Padding(padding: const EdgeInsets.all(8), child: Row(children: [AnimatedBuilder(
          animation: G.keyboard,
          builder: (context, child) => ToggleButtons(
            constraints: const BoxConstraints(minWidth: 32, minHeight: 24),
            tapTargetSize: MaterialTapTargetSize.shrinkWrap,
            borderRadius: const BorderRadius.all(Radius.circular(8)),
            isSelected: [G.keyboard.ctrl, G.keyboard.alt, G.keyboard.shift],
            onPressed: (index) {
              switch (index) {
                case 0:
                  G.keyboard.ctrl = !G.keyboard.ctrl;
                  break;
                case 1:
                  G.keyboard.alt = !G.keyboard.alt;
                  break;
                case 2:
                  G.keyboard.shift = !G.keyboard.shift;
                  break;
              }
            },
            children: const [Text('Ctrl'), Text('Alt'), Text('Shift')],
          ),
        ),
        const SizedBox.square(dimension: 8), 
        Expanded(child: SizedBox(height: 24, child: ListView.separated(scrollDirection: Axis.horizontal, itemBuilder:(context, index) {
          return OutlinedButton(style: D.controlButtonStyle, onPressed: () {
            G.termPtys[G.currentContainer]!.terminal.keyInput(D.termCommands[index]["key"]! as TerminalKey);
          }, child: Text(D.termCommands[index]["name"]! as String));
        }, separatorBuilder:(context, index) {
          return const SizedBox.square(dimension: 4);
        }, itemCount: D.termCommands.length))), SizedBox.fromSize(size: const Size(72, 0))])):const SizedBox.square(dimension: 0);
      })
    ]);
  }
}

class FastCommands extends StatefulWidget {
  const FastCommands({super.key});

  @override
  State<FastCommands> createState() => _FastCommandsState();
}

class _FastCommandsState extends State<FastCommands> {

  @override
  Widget build(BuildContext context) {
    return Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: Util.getCurrentProp("commands")
      .asMap().entries.map<Widget>((e) {
      return OutlinedButton(style: D.commandButtonStyle, child: Text(e.value["name"]!), onPressed: () {
        Util.termWrite(e.value["command"]!);
        G.pageIndex.value = 0;
      }, onLongPress: () {
        String name = e.value["name"]!;
        String command = e.value["command"]!;
        showDialog(context: context, builder: (context) {
          return AlertDialog(title: const Text("Edit Command"), content: SingleChildScrollView(child: Column(children: [
            TextFormField(initialValue: name, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Command Name"), onChanged: (value) {
              name = value;
            }),
            const SizedBox.square(dimension: 8),
            TextFormField(maxLines: null, initialValue: command, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Command Content"), onChanged: (value) {
              command = value;
            }),
          ])), actions: [
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..removeAt(e.key));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: const Text("Delete This Item")),
            TextButton(onPressed:() {
              Navigator.of(context).pop();
            }, child: const Text("Cancel")),
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..setAll(e.key, [{"name": name, "command": command}]));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: const Text("Save")),
          ]);
        },);
      },);
    }).toList()..add(OutlinedButton(style: D.commandButtonStyle, onPressed:() {
        String name = "";
        String command = "";
        showDialog(context: context, builder: (context) {
          return AlertDialog(title: const Text("Edit Command"), content: SingleChildScrollView(child: Column(children: [
            TextFormField(initialValue: name, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Command Name"), onChanged: (value) {
              name = value;
            }),
            const SizedBox.square(dimension: 8),
            TextFormField(maxLines: null, initialValue: command, decoration: const InputDecoration(border: OutlineInputBorder(), labelText: "Command Content"), onChanged: (value) {
              command = value;
            }),
          ])), actions: [
            TextButton(onPressed:() {
              Navigator.of(context).pop();
            }, child: const Text("Cancel")),
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..add({"name": name, "command": command}));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: const Text("Add")),
          ]);
        },);
    }, onLongPress: () {
      showDialog(context: context, builder: (context) {
        return AlertDialog(title: const Text("Reset Commands"), content: const Text("Are you sure you want to reset all shortcut commands?"), actions: [
          TextButton(onPressed:() {
            Navigator.of(context).pop();
          }, child: const Text("Cancel")),
          TextButton(onPressed:() async {
            await Util.setCurrentProp("commands", D.commands);
            setState(() {});
            if (!context.mounted) return;
            Navigator.of(context).pop();
          }, child: const Text("Yes")),
        ]);
      });
    }, child: const Text("Add Shortcut Command"))));
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title});

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {

  bool bannerAdsFailedToLoad = false;

  //Is installation complete?
  //Switch from loading interface to main interface after completion
  bool isLoadingComplete = false;

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


    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky,overlays: []);

    return Scaffold(
      appBar: AppBar(
        title: Text(isLoadingComplete?Util.getCurrentProp("name"):widget.title),
      ),
      body: isLoadingComplete?
        ValueListenableBuilder(valueListenable: G.pageIndex, builder: (context, value, child) {
          return IndexedStack(index: G.pageIndex.value, children: const [TerminalPage(), Padding(
              padding: EdgeInsets.all(8),
              child: AspectRatioMax1To1(child: Scrollbar(child: SingleChildScrollView(restorationId: "control-scroll", child: Column(
                children: [
                  Padding(
                    padding: EdgeInsets.all(16),
                    child: FractionallySizedBox(
                      widthFactor: 0.4,
                      child: Image(
                        image: AssetImage("images/icon.png")
                      )
                    ),
                  ),
                  FastCommands(),
                  Padding(padding: EdgeInsets.all(8), child: Card(child: Padding(padding: EdgeInsets.all(8), child: 
                    Column(children: [
                      SettingPage(),
                      SizedBox.square(dimension: 8),
                      InfoPage(openFirstInfo: false)
                    ])
                  )))
                ]
              ))))
          )]);
        }):const LoadingPage(),
      bottomNavigationBar: ValueListenableBuilder(valueListenable: G.pageIndex, builder:(context, value, child) {
        return Visibility(visible: isLoadingComplete,
          // child: BottomNavigationBar(currentIndex: G.pageIndex.value,
          //   onTap: (index) {
          //     G.pageIndex.value = index;
          //   },
          //   items: const [
          //     BottomNavigationBarItem(icon: Icon(Icons.monitor), label: "Terminal"),
          //     BottomNavigationBarItem(icon: Icon(Icons.video_settings), label: "Control"),
          //   ],
          // )
          child: NavigationBar(
            selectedIndex: G.pageIndex.value,
            destinations: const [
              NavigationDestination(icon: Icon(Icons.monitor), label: "Terminal"),
              NavigationDestination(icon: Icon(Icons.video_settings), label: "Control")
            ],
            onDestinationSelected: (index) {
              G.pageIndex.value = index;
            },
          ),
        );}
      ),
      floatingActionButton: ValueListenableBuilder(valueListenable: G.pageIndex, builder:(context, value, child) {
        return Visibility(visible: isLoadingComplete && (value == 0),
          child: FloatingActionButton(
            tooltip: "Enter Graphical Interface",
            onPressed: () {
              if (G.wasX11Enabled) {
                Workflow.launchX11();
              } else if (G.wasAvncEnabled) {
                Workflow.launchAvnc();
              } else {
                Workflow.launchBrowser();
              }
            },
            child: const Icon(Icons.play_arrow),
          )
        );
      }), // This trailing comma makes auto-formatting nicer for build methods.
    );
  }
}
