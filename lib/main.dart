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

import 'package:clipboard/clipboard.dart';
import 'package:dynamic_color/dynamic_color.dart';
import 'package:flutter/gestures.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:flutter/services.dart';
import 'package:url_launcher/url_launcher.dart';
import 'package:flutter/material.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
import 'package:xterm/xterm.dart';

import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:tiny_computer/l10n/app_localizations.dart';

import 'package:tiny_computer/workflow.dart';

import 'package:avnc_flutter/avnc_flutter.dart';
import 'package:x11_flutter/x11_flutter.dart';

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
            localizationsDelegates: const [
              AppLocalizations.delegate,
              GlobalMaterialLocalizations.delegate,
              GlobalWidgetsLocalizations.delegate,
              GlobalCupertinoLocalizations.delegate,
            ],
            supportedLocales: const [
              Locale('en'),
              Locale.fromSubtags(languageCode: 'zh'), // generic Chinese 'zh'
              Locale.fromSubtags(
                languageCode: 'zh',
                scriptCode: 'Hans',
              ), // generic simplified Chinese 'zh_Hans'
              Locale.fromSubtags(
                languageCode: 'zh',
                scriptCode: 'Hant',
              ), // generic traditional Chinese 'zh_Hant'
              Locale.fromSubtags(
                languageCode: 'zh',
                scriptCode: 'Hans',
                countryCode: 'CN',
              ), // 'zh_Hans_CN'
              Locale.fromSubtags(
                languageCode: 'zh',
                scriptCode: 'Hant',
                countryCode: 'TW',
              ), // 'zh_Hant_TW'
              Locale.fromSubtags(
                languageCode: 'zh',
                scriptCode: 'Hant',
                countryCode: 'HK',
              ), // 'zh_Hant_HK'
            ],
            theme: ThemeData(
              colorScheme: lightDynamic,
              useMaterial3: true,
            ),
            darkTheme: ThemeData(
              colorScheme: darkDynamic,
              useMaterial3: true,
            ),
            home: MyHomePage(title: "Tiny Computer"),
          );
        }
    );
  }
}


//限制最大宽高比1:1
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

  final List<bool> _expandState = [false, false, false, false, false, false];

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
          return ListTile(title: Text(AppLocalizations.of(context)!.advancedSettings), subtitle: Text(AppLocalizations.of(context)!.restartAfterChange));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.resetStartupCommand), onPressed: () {
              showDialog(context: context, builder: (context) {
                return AlertDialog(title: Text(AppLocalizations.of(context)!.attention), content: Text(AppLocalizations.of(context)!.confirmResetCommand), actions: [
                  TextButton(onPressed:() {
                    Navigator.of(context).pop();
                  }, child: Text(AppLocalizations.of(context)!.cancel)),
                  TextButton(onPressed:() async {
                    await Util.setCurrentProp("boot", D.boot);
                    G.bootTextChange.value = !G.bootTextChange.value;
                    if (!context.mounted) return;
                    Navigator.of(context).pop();
                  }, child: Text(AppLocalizations.of(context)!.yes)),
                ]);
              });
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.signal9ErrorPage), onPressed: () async {
              await D.androidChannel.invokeMethod("launchSignal9Page", {});
            }),
          ]),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("name"), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.containerName), onChanged: (value) async {
            await Util.setCurrentProp("name", value);
          }),
          const SizedBox.square(dimension: 8),
          ValueListenableBuilder(valueListenable: G.bootTextChange, builder:(context, v, child) {
            return TextFormField(maxLines: null, initialValue: Util.getCurrentProp("boot"), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.startupCommand), onChanged: (value) async {
              await Util.setCurrentProp("boot", value);
            });
          }),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vnc"), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.vncStartupCommand), onChanged: (value) async {
            await Util.setCurrentProp("vnc", value);
          }),
          const SizedBox.square(dimension: 8),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.shareUsageHint),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.copyShareLink), onPressed: () async {
              final String? ip = await NetworkInfo().getWifiIP();
              if (!context.mounted) return;
              if (G.wasX11Enabled) {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text(AppLocalizations.of(context)!.x11InvalidHint))
                );
                return;
              }
              if (ip == null) {
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text(AppLocalizations.of(context)!.cannotGetIpAddress))
                );
                return;
              }
              FlutterClipboard.copy((Util.getCurrentProp("vncUrl") as String).replaceAll(RegExp.escape("localhost"), ip)).then((value) {
                if (!context.mounted) return;
                ScaffoldMessenger.of(context).hideCurrentSnackBar();
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text(AppLocalizations.of(context)!.shareLinkCopied))
                );
              });
            }),
          ]),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vncUrl"), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.webRedirectUrl), onChanged: (value) async {
            await Util.setCurrentProp("vncUrl", value);
          }),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getCurrentProp("vncUri"), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.vncLink), onChanged: (value) async {
            await Util.setCurrentProp("vncUri", value);
          }),
          const SizedBox.square(dimension: 8),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[1],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.globalSettings), subtitle: Text(AppLocalizations.of(context)!.enableTerminalEditing));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: (Util.getGlobal("termMaxLines") as int).toString(), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.terminalMaxLines),
            keyboardType: TextInputType.number,
            validator: (value) {
              return Util.validateBetween(value, 1024, 2147483647, () async {
                await G.prefs.setInt("termMaxLines", int.parse(value!));
              });
            },),
          const SizedBox.square(dimension: 16),
          TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: (Util.getGlobal("defaultAudioPort") as int).toString(), decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.pulseaudioPort),
            keyboardType: TextInputType.number,
            validator: (value) {
              return Util.validateBetween(value, 0, 65535, () async {
                await G.prefs.setInt("defaultAudioPort", int.parse(value!));
              });
            }
          ),
          const SizedBox.square(dimension: 16),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.enableTerminal), value: Util.getGlobal("isTerminalWriteEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isTerminalWriteEnabled", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.enableTerminalKeypad), value: Util.getGlobal("isTerminalCommandsEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isTerminalCommandsEnabled", value);
            setState(() {
              G.terminalPageChange.value = !G.terminalPageChange.value;
            });
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.terminalStickyKeys), value: Util.getGlobal("isStickyKey") as bool, onChanged:(value) {
            G.prefs.setBool("isStickyKey", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.keepScreenOn), value: Util.getGlobal("wakelock") as bool, onChanged:(value) {
            G.prefs.setBool("wakelock", value);
            WakelockPlus.toggle(enable: value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.restartRequiredHint),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.startWithGUI), value: Util.getGlobal("autoLaunchVnc") as bool, onChanged:(value) {
            G.prefs.setBool("autoLaunchVnc", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.reinstallBootPackage), value: Util.getGlobal("reinstallBootstrap") as bool, onChanged:(value) {
            G.prefs.setBool("reinstallBootstrap", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.getifaddrsBridge), subtitle: Text(AppLocalizations.of(context)!.fixGetifaddrsPermission), value: Util.getGlobal("getifaddrsBridge") as bool, onChanged:(value) {
            G.prefs.setBool("getifaddrsBridge", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.fakeUOSSystem), value: Util.getGlobal("uos") as bool, onChanged:(value) {
            G.prefs.setBool("uos", value);
            setState(() {});
          },),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[2],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.displaySettings));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.avncAdvantages),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.avncSettings), onPressed: () async {
              await AvncFlutter.launchPrefsPage();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.aboutAVNC), onPressed: () async {
              await AvncFlutter.launchAboutPage();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.avncResolution), onPressed: () async {
              final s = WidgetsBinding.instance.platformDispatcher.views.first.physicalSize;
              final w0 = max(s.width, s.height);
              final h0 = min(s.width, s.height);
              String w = (w0 * 0.75).round().toString();
              String h = (h0 * 0.75).round().toString();
              showDialog(context: context, builder: (context) {
                return AlertDialog(title: Text(AppLocalizations.of(context)!.resolutionSettings), content: SingleChildScrollView(child: Column(children: [
                  Text("${AppLocalizations.of(context)!.deviceScreenResolution} ${w0.round()}x${h0.round()}"),
                  const SizedBox.square(dimension: 8),
                  TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: w, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.width), keyboardType: TextInputType.number,
                    validator: (value) {
                      return Util.validateBetween(value, 200, 7680, () {
                        w = value!;
                      });
                    }
                  ),
                  const SizedBox.square(dimension: 8),
                  TextFormField(autovalidateMode: AutovalidateMode.onUserInteraction, initialValue: h, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.height), keyboardType: TextInputType.number,
                    validator: (value) {
                      return Util.validateBetween(value, 200, 7680, () {
                        h = value!;
                      });
                    }
                  ),
                ])), actions: [
                  TextButton(onPressed:() {
                    Navigator.of(context).pop();
                  }, child: Text(AppLocalizations.of(context)!.cancel)),
                  TextButton(onPressed:() async {
                    Util.termWrite("""sed -i -E "s@(geometry)=.*@\\1=${w}x${h}@" /etc/tigervnc/vncserver-config-tmoe
sed -i -E "s@^(VNC_RESOLUTION)=.*@\\1=${w}x${h}@" \$(command -v startvnc)""");
                    ScaffoldMessenger.of(context).hideCurrentSnackBar();
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text("${w}x${h}. ${AppLocalizations.of(context)!.applyOnNextLaunch}"))
                    );
                    if (!context.mounted) return;
                    Navigator.of(context).pop();
                  }, child: Text(AppLocalizations.of(context)!.save)),
                ]);
              });
            }),
          ]),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.useAVNCByDefault), subtitle: Text(AppLocalizations.of(context)!.applyOnNextLaunch), value: Util.getGlobal("useAvnc") as bool, onChanged:(value) {
            G.prefs.setBool("useAvnc", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.termuxX11Advantages),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.termuxX11Preferences), onPressed: () async {
              await X11Flutter.launchX11PrefsPage();
            }),
          ]),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.useTermuxX11ByDefault), subtitle: Text(AppLocalizations.of(context)!.disableVNC), value: Util.getGlobal("useX11") as bool, onChanged:(value) {
            G.prefs.setBool("useX11", value);
            if (!value && Util.getGlobal("dri3")) {
              G.prefs.setBool("dri3", false);
            }
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.hidpiAdvantages),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultHidpiOpt") as String, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.hidpiEnvVar),
            onChanged: (value) async {
              await G.prefs.setString("defaultHidpiOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.hidpiSupport), subtitle: Text(AppLocalizations.of(context)!.applyOnNextLaunch), value: Util.getGlobal("isHidpiEnabled") as bool, onChanged:(value) {
            G.prefs.setBool("isHidpiEnabled", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[3],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.fileAccess));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          Text(AppLocalizations.of(context)!.fileAccessHint),
          const SizedBox.square(dimension: 16),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.requestStoragePermission), onPressed: () {
              Permission.storage.request();
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.requestAllFilesAccess), onPressed: () {
              Permission.manageExternalStorage.request();
            }),
          ]),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[4],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.graphicsAcceleration), subtitle: Text(AppLocalizations.of(context)!.experimentalFeature));
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          Text(AppLocalizations.of(context)!.graphicsAccelerationHint),
          const SizedBox.square(dimension: 16),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultVirglCommand") as String, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.virglServerParams),
            onChanged: (value) async {
              await G.prefs.setString("defaultVirglCommand", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultVirglOpt") as String, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.virglEnvVar),
            onChanged: (value) async {
              await G.prefs.setString("defaultVirglOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.enableVirgl), subtitle: Text(AppLocalizations.of(context)!.applyOnNextLaunch), value: Util.getGlobal("virgl") as bool, onChanged:(value) {
            G.prefs.setBool("virgl", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.turnipAdvantages),
          const SizedBox.square(dimension: 8),
          TextFormField(maxLines: null, initialValue: Util.getGlobal("defaultTurnipOpt") as String, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.turnipEnvVar),
            onChanged: (value) async {
              await G.prefs.setString("defaultTurnipOpt", value);
            },
          ),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.enableTurnipZink), subtitle: Text(AppLocalizations.of(context)!.applyOnNextLaunch), value: Util.getGlobal("turnip") as bool, onChanged:(value) async {
            G.prefs.setBool("turnip", value);
            if (!value && Util.getGlobal("dri3")) {
              G.prefs.setBool("dri3", false);
            }
            setState(() {});
          },),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.enableDRI3), subtitle: Text(AppLocalizations.of(context)!.applyOnNextLaunch), value: Util.getGlobal("dri3") as bool, onChanged:(value) async {
            if (value && !(Util.getGlobal("turnip") && Util.getGlobal("useX11"))) {
              if (!context.mounted) return;
              ScaffoldMessenger.of(context).hideCurrentSnackBar();
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text(AppLocalizations.of(context)!.dri3Requirement))
              );
              return;
            }
            G.prefs.setBool("dri3", value);
            setState(() {});
          },),
          const SizedBox.square(dimension: 16),
        ],))),
      ExpansionPanel(
        isExpanded: _expandState[5],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.windowsAppSupport), subtitle: Text(AppLocalizations.of(context)!.experimentalFeature),);
        }), body: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
          Text(AppLocalizations.of(context)!.hangoverDescription),
          const SizedBox.square(dimension: 8),
          Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
            OutlinedButton(style: D.commandButtonStyle, child: Text("${AppLocalizations.of(context)!.installHangoverStable}（10.11）"), onPressed: () async {
              Util.termWrite("bash ~/.local/share/tiny/extra/install-hangover-stable");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.installHangoverLatest), onPressed: () async {
              Util.termWrite("bash ~/.local/share/tiny/extra/install-hangover");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.uninstallHangover), onPressed: () async {
              Util.termWrite("sudo apt autoremove --purge -y hangover*");
              G.pageIndex.value = 0;
            }),
            OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.clearWineData), onPressed: () async {
              Util.termWrite("rm -rf ~/.wine");
              G.pageIndex.value = 0;
            }),
          ]),
          const SizedBox.square(dimension: 16),
          const Divider(height: 2, indent: 8, endIndent: 8),
          const SizedBox.square(dimension: 16),
          Text(AppLocalizations.of(context)!.wineCommandsHint),
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
          Text(AppLocalizations.of(context)!.restartRequiredHint),
          const SizedBox.square(dimension: 8),
          SwitchListTile(title: Text(AppLocalizations.of(context)!.switchToJapanese), subtitle: const Text("システムを日本語に切り替える"), value: Util.getGlobal("isJpEnabled") as bool, onChanged:(value) async {
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
  final List<bool> _expandState = [false, false, false, false];
  
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
          return ListTile(title: Text(AppLocalizations.of(context)!.userManual));
        },
        body: Padding(padding: const EdgeInsets.all(8), child: Column(
          children: [
            Text(AppLocalizations.of(context)!.firstLoadInstructions),
            const SizedBox.square(dimension: 16),
            Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: [
              OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.requestStoragePermission), onPressed: () {
                Permission.storage.request();
              }),
              OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.requestAllFilesAccess), onPressed: () {
                Permission.manageExternalStorage.request();
              }),
              OutlinedButton(style: D.commandButtonStyle, child: Text(AppLocalizations.of(context)!.ignoreBatteryOptimization), onPressed: () {
                Permission.ignoreBatteryOptimizations.request();
              }),
            ]),
            const SizedBox.square(dimension: 16),
            Wrap(alignment: WrapAlignment.center, spacing: 4.0, runSpacing: 4.0, children: D.links
            .asMap().entries.map<Widget>((e) {
              return OutlinedButton(style: D.commandButtonStyle, child: Text(Util.getl10nText(e.value["name"]!, context)), onPressed: () {
                launchUrl(Uri.parse(e.value["value"]!), mode: LaunchMode.externalApplication);
              });
            }).toList()),
          ],
        )),
        isExpanded: _expandState[0],
      ),
      ExpansionPanel(
        isExpanded: _expandState[1],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.openSourceLicenses));
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
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
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
          return ListTile(title: Text(AppLocalizations.of(context)!.permissionUsage));
        }), body: Padding(padding: EdgeInsets.all(8), child: Text(AppLocalizations.of(context)!.privacyStatement))),
      ExpansionPanel(
        isExpanded: _expandState[3],
        headerBuilder: ((context, isExpanded) {
          return ListTile(title: Text(AppLocalizations.of(context)!.supportAuthor));
        }), body: Column(
        children: [
          Padding(padding: EdgeInsets.all(8), child: Text(AppLocalizations.of(context)!.recommendApp)),
          ElevatedButton(
            onPressed: () {
              launchUrl(Uri.parse("https://github.com/Cateners/tiny_computer"), mode: LaunchMode.externalApplication);
            },
            child: Text(AppLocalizations.of(context)!.projectUrl),
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
          return AlertDialog(title: Text(AppLocalizations.of(context)!.commandEdit), content: SingleChildScrollView(child: Column(children: [
            TextFormField(initialValue: name, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.commandName), onChanged: (value) {
              name = value;
            }),
            const SizedBox.square(dimension: 8),
            TextFormField(maxLines: null, initialValue: command, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.commandContent), onChanged: (value) {
              command = value;
            }),
          ])), actions: [
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..removeAt(e.key));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: Text(AppLocalizations.of(context)!.deleteItem)),
            TextButton(onPressed:() {
              Navigator.of(context).pop();
            }, child: Text(AppLocalizations.of(context)!.cancel)),
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..setAll(e.key, [{"name": name, "command": command}]));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: Text(AppLocalizations.of(context)!.save)),
          ]);
        },);
      },);
    }).toList()..add(OutlinedButton(style: D.commandButtonStyle, onPressed:() {
        String name = "";
        String command = "";
        showDialog(context: context, builder: (context) {
          return AlertDialog(title: Text(AppLocalizations.of(context)!.commandEdit), content: SingleChildScrollView(child: Column(children: [
            TextFormField(initialValue: name, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.commandName), onChanged: (value) {
              name = value;
            }),
            const SizedBox.square(dimension: 8),
            TextFormField(maxLines: null, initialValue: command, decoration: InputDecoration(border: OutlineInputBorder(), labelText: AppLocalizations.of(context)!.commandContent), onChanged: (value) {
              command = value;
            }),
          ])), actions: [
            TextButton(onPressed:() {
              Navigator.of(context).pop();
            }, child: Text(AppLocalizations.of(context)!.cancel)),
            TextButton(onPressed:() async {
              await Util.setCurrentProp("commands", Util.getCurrentProp("commands")
                ..add({"name": name, "command": command}));
              setState(() {});
              if (!context.mounted) return;
              Navigator.of(context).pop();
            }, child: Text(AppLocalizations.of(context)!.add)),
          ]);
        },);
    }, onLongPress: () {
      showDialog(context: context, builder: (context) {
        return AlertDialog(title: Text(AppLocalizations.of(context)!.resetCommand), content: Text(AppLocalizations.of(context)!.confirmResetAllCommands), actions: [
          TextButton(onPressed:() {
            Navigator.of(context).pop();
          }, child: Text(AppLocalizations.of(context)!.cancel)),
          TextButton(onPressed:() async {
            await Util.setCurrentProp("commands", D.commands);
            setState(() {});
            if (!context.mounted) return;
            Navigator.of(context).pop();
          }, child: Text(AppLocalizations.of(context)!.yes)),
        ]);
      });
    }, child: Text(AppLocalizations.of(context)!.addShortcutCommand))));
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
  bool isLoadingComplete = false;

  @override
  void initState() {
    super.initState();
    _initializeWorkflow();
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky, overlays: []);
  }

  Future<void> _initializeWorkflow() async {
    await Workflow.workflow();
    if (mounted) {
      setState(() {
        isLoadingComplete = true;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    G.homePageStateContext = context;

    return Scaffold(
      appBar: AppBar(
        title: Text(isLoadingComplete ? Util.getCurrentProp("name") : widget.title),
      ),
      body: isLoadingComplete
          ? ValueListenableBuilder(
              valueListenable: G.pageIndex,
              builder: (context, value, child) {
                return IndexedStack(
                  index: G.pageIndex.value,
                  children: const [
                    TerminalPage(),
                    Padding(
                      padding: EdgeInsets.all(8),
                      child: AspectRatioMax1To1(
                        child: Scrollbar(
                          child: SingleChildScrollView(
                            restorationId: "control-scroll",
                            child: Column(
                              children: [
                                Padding(
                                  padding: EdgeInsets.all(16),
                                  child: FractionallySizedBox(
                                    widthFactor: 0.4,
                                    child: Image(image: AssetImage("images/icon.png")),
                                  ),
                                ),
                                FastCommands(),
                                Padding(
                                  padding: EdgeInsets.all(8),
                                  child: Card(
                                    child: Padding(
                                      padding: EdgeInsets.all(8),
                                      child: Column(
                                        children: [
                                          SettingPage(),
                                          SizedBox.square(dimension: 8),
                                          InfoPage(openFirstInfo: false),
                                        ],
                                      ),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                  ],
                );
              },
            )
          : const LoadingPage(),
      bottomNavigationBar: ValueListenableBuilder(
        valueListenable: G.pageIndex,
        builder: (context, value, child) {
          return Visibility(
            visible: isLoadingComplete,
            child: NavigationBar(
              selectedIndex: G.pageIndex.value,
              destinations: [
                NavigationDestination(icon: const Icon(Icons.monitor), label: AppLocalizations.of(context)!.terminal),
                NavigationDestination(icon: const Icon(Icons.video_settings), label: AppLocalizations.of(context)!.control),
              ],
              onDestinationSelected: (index) {
                G.pageIndex.value = index;
              },
            ),
          );
        },
      ),
      floatingActionButton: ValueListenableBuilder(
        valueListenable: G.pageIndex,
        builder: (context, value, child) {
          return Visibility(
            visible: isLoadingComplete && (value == 0),
            child: FloatingActionButton(
              tooltip: AppLocalizations.of(context)!.enterGUI,
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
            ),
          );
        },
      ),
    );
  }
}
