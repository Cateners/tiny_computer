# xxx-xx-xx: Version 0.9.15

## Overall changes:

  * Added fuzzing with OSS-Fuzz thanks to Catena Cyber.

## LibVNCServer/LibVNCClient:

  * Fixed building with OpenSSL >= 3.0.0.

## LibVNCClient:

  * Fixed LibVNCClient handling of UltraVNC MSLogonII when built with OpenSSL.
  * Added UTF8 clipboard handling.

## LibVNCServer:

  * Added a proof-of-concept X11 example server.


# 2022-12-18: Version 0.9.14

0.9.14 represents a gradual improvement over 0.9.13 with lots of developments all over
the place. [40 issues and pull requests](https://github.com/LibVNC/libvncserver/issues?q=milestone%3A%22Release+0.9.14%22+is%3Aclosed)
were closed/merged with this release. Highlights on the LibVNCServer side are a
refined multi-threaded implementation, support for Unicode clipboard data 📋❗ and an
outbound-connection machinery that works for real world use cases. LibVNCClient received
support for [SetDesktopSize](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#setdesktopsize)
(which had been added server-side in 0.9.13) and a properly documented SSH-tunneling
setup.

## Overall changes:

  * Added more documentation (build system integration, repeater setup) and a legal FAQ.
  * Added [contribution guidelines](CONTRIBUTING.md).
  * Ported the TravisCI continous integration machinery to GitHub workflows.

## LibVNCServer/LibVNCClient:

  * Added [qemu extended key event](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#qemu-extended-key-event-message)
    support.
  * Fixed several potential multiplication overflows.

## LibVNCClient:

  * Fixes of several memory leaks and buffer overflows.
  * Added UltraVNC's MSLogonII authentication scheme.
  * Fixed TLS interoperability with GnuTLS servers.
  * Fixed detection of newer UltraVNC and TightVNC servers.
  * Added support for [SetDesktopSize](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#setdesktopsize).
  * Added SSH tunneling example using libssh2.
  * Added some extensions to VeNCrypt in order to be compatible with a wider range of servers.

## LibVNCServer:

  * Fixes to the multi-threaded server implementation which should be a lot more sound now.
  * Fixed TightVNC-filetransfer file upload for 64-bit systems.
  * Fixes of crashes in the zlib compression.
  * Added support for [UTF8 clipboard data](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#extended-clipboard-pseudo-encoding).
  * Fixed visual artifacts in framebuffer on ARM platforms.
  * Fixed several WebSockets bugs.
  * Fixed the UltraVNC-style repeater example. 
  * Added support for larger framebuffers (two 4k screens possible now).
  * Added support for timeouts for outbound connections (to repeaters for instance).
  * Fixed out-of-bounds memory access in Tight encoding.


# 2020-06-13: Version 0.9.13

0.9.13 truly is a cross-platform release, the best we've ever done in that respect:
Out of the [49 issues](https://github.com/LibVNC/libvncserver/issues?q=is%3Aclosed+milestone%3A%22Release+0.9.13%22)
closed with this release, 20 alone were related to MS Windows. The result is that 0.9.13
is the first release with full support for Microsoft Windows! The cross-platform focused
work did not end there tough: MacOS support was brought up from barebones to a fully working
production-grade VNC server application. Other highlights are improvements regarding TLS
in LibVNCClient, [SetDesktopSize](https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#setdesktopsize)
support in LibVNCServer and a major cleanup of the project's documentation. Last but not
least, 0.9.13 comes with the usual assortment of bugfixes and security improvements.

## Overall changes:
  * Small tweaks to the CMake build system.
  * The macOS server example was overhauled and is now the most feature-complete sample
	application of the project, ready for real-world use.
  * Lots of documentation updates and markdownifying.
  * The TravisCI continuous integration now also build-checks cross-compilation from
	Linux to Windows.
  * Setup a [Gitter community chat](https://gitter.im/LibVNC/libvncserver) for the project.

## LibVNCServer/LibVNCClient:
  * Both LibVNCServer and LibVNCClient now support an additional platform, namely
	Microsoft Windows. Building is supported with Visual Studio as well as MingGW.
  * The separate crypto routines used by LibVNCClient and LibVNCServer were refactored
	into an implementation common to both libraries.
  * Several security issues got fixed, namely:
     - CVE-2018-21247: When connecting to a repeater, only send initialised string
     - CVE-2019-20839: libvncclient: bail out if unix socket name would overflow
     - CVE-2019-20840: fix crash because of unaligned accesses in hybiReadAndDecode()
     - CVE-2020-14396: libvncclient/tls_openssl: do not deref a NULL pointer
     - CVE-2020-14397: libvncserver: add missing NULL pointer checks
     - CVE-2020-14398: libvncclient: handle half-open TCP connections
     - CVE-2020-14399: libvncclient: fix pointer aliasing/alignment issue
     - CVE-2020-14400: libvncserver: fix pointer aliasing/alignment issue
     - CVE-2020-14401: libvncserver: scale: cast to 64 bit before shifting
     - CVE-2020-14402: libvncserver: encodings: prevent OOB accesses
     - CVE-2020-14403: encodings: prevent OOB accesses
     - CVE-2020-14404: libvncserver: encodings: prevent OOB accesses
     - CVE-2020-14405: libvncclient/rfbproto: limit max textchat size
  * The bundled noVNC client is now at version 1.1.0 and included via a git submodule.

## LibVNCClient:
  * Added connect timeout as well as read timeout support thanks to Tobias Junghans.
  * Both TLS backends now do proper locking of network operations when multi-threaded
    thanks to Gaurav Ujjwal.
  * Fixed regression in Tight/Raw decoding introduced in 0.9.12 thanks to DRC.
  * Fixed encrypted connections to AnonTLS servers when using the OpenSSL back-end.
	Made possible by the profound research done by Gaurav Ujjwal.

## LibVNCServer:
  * Added a hooking function (`clientFramebufferUpdateRequestHook`) to deliver
	rfbFramebufferUpdateRequest messages from clients to the frame producer
	thanks to Jae Hyun Yoo.
  * Added SetDesktopSize/ExtendedDesktopSize support thanks to Floris Bos.
  * Added multi-threading support for MS Windows.
  * Fixed VNC repeater/proxy functionality that was broken in 0.9.12.
  * Fixed unstable WebSockets connections thanks to Sebastian Kranz.

# 2019-01-06: Version 0.9.12

Over two years of work have now culminated in 0.9.12. We have ditched
the legacy Autotools build system in favour of the truly cross-platform
CMake and extended the continuous integration tests to run MS Windows
builds as well. LibVNCServer saw quite some memory management issues
fixed, LibVNCClient received X509 server certificate verification, Tight
decoding optimizations, support for overriding the default rectangle
decode handlers and a port of the SDL-based VNC viewer to SDL 2.0. [42
issues](https://github.com/LibVNC/libvncserver/issues?q=is%3Aclosed+milestone%3A%22Release+0.9.12%22)
were fixed with this release.

## Overall changes:
   * CMake now is the default build system, Autotools were removed.
   * In addition to TravisCI, all commits are now build-tested by AppVeyorCI.

## LibVNCServer/LibVNCClient:
   * Numerous build fixes for Visual Studio compilers to the extent that
     one can now _build_ the project with these. The needed changes for
     successfully _running_ stuff will be implemented in 0.9.13.
   * Fixed building for Android and added build instructions.
   * Removed the unused PolarSSL wrapper.
   * Updated the bundled noVNC to latest release 1.0.0.
   * Allowed to use global LZO library instead of miniLZO.

## LibVNCClient:
   * Support for OpenSSL 1.1.x.
   * Support for overriding the default rectangle decode handlers (with
     hardware-accelerated ones for instance) thanks to Balazs Ludmany.
   * vnc2mpg updated.
   * Added support for X509 server certificate verification as part of the
     handshake process thanks to Simon Waterman.
   * Added a TRLE decoder thanks to Wiki Wang.
   * Included Tight decoding optimizations from TurboVNC thanks to DRC.
   * Ported the SDL viewer from SDL 1.2 to SDL 2.0.
   * Numerous security fixes.
   * Added support for custom auth handlers in order to support additional
     security types.

## LibVNCServer:
   * Websockets rework to remove obsolete code thanks to Andreas Weigel.
   * Ensured compatibility with gtk-vnc 0.7.0+ thanks to Michał Kępień.
   * The built-in webserver now sends correct MIME type for Javascript.
   * Numerous memory management issues fixed.
   * Made the TightVNC-style file transfer more stable.

# 2016-12-30: Version 0.9.11

After quite some time finally a major release featuring continous
integration to make sure the code builds on all supported platforms.
LibVNCClient saw a lot of robustness fixes making it more stable when
dealing with broken or malicious servers. LibVNCServer received
WebSocket improvements, its built-in webserver got more secure and
systemd support was added.

## Overall changes:
   * LibVNCServer/LibVNCClient development now uses continous intregration,
     provided by TravisCI.

## LibVNCClient:
   * Now initializes libgcrypt before use if the application did not do it.
     Fixes a crash when connection to Mac hosts
     (https://github.com/LibVNC/libvncserver/issues/45).
   * Various fixes that result in more stable handling of malicious or broken
     servers.
   * Removed broken and unmaintained H264 decoding.
   * Some documentation fixes.
   * Added hooks to WriteToTLS() for optional protection by mutex.

## LibVNCServer:
   * Stability fixes for the WebSocket implementation.
   * Replaced SHA1 implementation with the one from RFC 6234.
   * The built-in HTTP server does not allow directory traversals anymore.
   * The built-in HTTP now sends correct MIME types for CSS and SVG.
   * Added support for systemd socket activation.
   * Made it possible to get autoPort behavior with either ipv4 or ipv6
     disabled.
   * Fixed starting of an onHold-client in threaded mode.

# 2014-10-21: Version 0.9.10

Another major release that saw a massive code re-organisation, merged
some Debian patches, addressed some security issues and fixed building
on Windows 8.

## Overall changes:
   * Moved the whole project from sourceforge to https://libvnc.github.io/.
   * Cleaned out the autotools build system which now uses autoreconf.
   * Updated noVNC HTML5 client to latest version.
   * Split out x11vnc sources into separate repository at
     https://github.com/LibVNC/x11vnc
   * Split out vncterm sources into separate repository at
     https://github.com/LibVNC/vncterm
   * Split out VisualNaCro sources into separate repository at
     https://github.com/LibVNC/VisualNaCro
   * Merged Debian patches.

## LibVNCServer/LibVNCClient:
   * Fixed some security-related buffer overflow cases.
   * Added compatibility headers to make LibVNCServer/LibVNCClient build on native
     Windows 8.
   * Update LZO to version 2.07, fixing CVE-2014-4607.

## LibVNCServer:
   * Merged patches from KDE/krfb.
   * Can now do IPv6 without IPv4.
   * Fixed a use-after-free issue in scale.c.

# 2012-05-04: Version 0.9.9

This is a major release that contains numerous bugfixes and a nice bag
of shiny new features, mainly full IPv6 support, the new TurboVNC
encoder and support for WebSockets.

## Overall changes:
   * Added noVNC HTML5 VNC viewer (http://kanaka.github.com/noVNC/) connect possibility
     to our http server. Pure JavaScript, no Java plugin required anymore! (But a
     recent browser...)
   * Added a GTK+ VNC viewer example.

## LibVNCServer/LibVNCClient:
   * Added support to build for Google Android.
   * Complete IPv6 support in both LibVNCServer and LibVNCClient.

## LibVNCServer:
   * Split two event-loop related functions out of the rfbProcessEvents() mechanism.
     This is required to be able to do proper event loop integration with Qt. Idea was
     taken from Vino's libvncserver fork.
   * Added TightPNG (http://wiki.qemu.org/VNC_Tight_PNG) encoding support. Like the
     original Tight encoding, this still uses JPEG, but ZLIB encoded rects are encoded
     with PNG here.
   * Added suport for serving VNC sessions through WebSockets
     (http://en.wikipedia.org/wiki/WebSocket), a web technology providing for multiplexing
     bi-directional, full-duplex communications channels over a single TCP connection.
   * Support connections from the Mac OS X built-in VNC client to LibVNCServer
     instances running with no password.
   * Replaced the Tight encoder with a TurboVNC one which is tremendously faster in most
     cases, especially with high-color video or 3D workloads.
     (http://www.virtualgl.org/pmwiki/uploads/About/tighttoturbo.pdf)

## LibVNCClient:
   * Added support to only listen for reverse connections on a specific IP address.
   * Support for using OpenSSL instead of GnuTLS. This could come in handy on embedded
     devices where only this TLS implementation is available.
   * Added support to connect to UltraVNC Single Click servers.

# 2011-11-09: Version 0.9.8.2

This is a maintenance release that fixes a regression in libvncclient
that crept in with Apple Remote Desktop support added with 0.9.8.
Viewers that were not adapted to the new functionality would fail
connecting to ARD servers before.

# 2011-10-12: Version 0.9.8.1

This is a maintenance release that fixes an ABI compatibility issue
introduced with 0.9.8.

# 2011-03-30: Version 0.9.8

## Overall changes:
   * Automagically generated API documentation using doxygen.
   * Added support for pkg-config.
   * Fixed Mingw32 cross compilation.
   * Fixed CMake build system.

## LibVNCServer/LibVNCClient:
   * All files used by _both_ LibVNCServer and LibVNCClient were put into
     a 'common' directory, reducing code duplication.
   * Implemented xvp VNC extension.
   * Updated minilzo library used for Ultra encoding to ver 2.04.
     According to the minilzo README, this brings a significant
     speedup on 64-bit architectures.

## LibVNCServer:
   * Thread safety for ZRLE, Zlib, Tight, RRE, CoRRE and Ultra encodings.
     This makes all VNC encodings safe to use with a multithreaded server.
   * A DisplayFinishedHook for LibVNCServer. If set, this hook gets called
     just before rfbSendFrameBufferUpdate() returns.
   * Fix for tight security type for RFB 3.8 in TightVNC file transfer
     (Debian Bug #517422).

## LibVNCClient:
   * Unix sockets support.
   * Anonymous TLS security type support.
   * VeNCrypt security type support.
   * MSLogon security type support.
   * ARD (Apple Remote Desktop) security type support.
   * UltraVNC Repeater support.
   * A new FinishedFrameBufferUpdate callback that is invoked after each
     complete framebuffer update.
   * A new non-forking listen (reverse VNC) function that works under
     Windows.
   * IPv6 support. LibVNCClient is now able to connect to IPv6 VNC servers.
   * IP QoS support. This enables setting the DSCP/Traffic Class field of
     IP/IPv6 packets sent by a client. For example starting a client with
     -qosdscp 184 marks all outgoing traffic for expedited forwarding.
     Implementation for Win32 is still a TODO, though.
   * Fixed hostname resolution problems under Windows.

## SDLvncviewer
   * Is now resizable and can do key repeat, mouse wheel scrolling
     and clipboard copy and paste.

## LinuxVNC:
   * Fix for no input possible because of ctrl key being stuck.
     Issue was reported as Debian bug #555988.


# Version 0.9.7
   * Mark sent me patches to no longer need C++ for ZRLE encoding!
     added --disable-cxx Option for configure
   * x11vnc changes from Karl Runge:
      - Changed all those whimpy printf(...)'s into manly fprintf(stdxxx,...)'s.

      - Added -q switch (quiet) to suppress printing all the debug-looking output.

      - Added -bg switch to fork into background after everything is set up.
        (checks for LIBVNCSERVER_HAVE_FORK and LIBVNCSERVER_HAVE_SETSID)

      - Print this string out to stdout:  'PORT=XXXX' (usually XXXX = 5900).
        Combining with -bg, easy to write a ssh/rsh wrapper with something like:
        port=`ssh $host "x11vnc -bg .."` then run vncviewer based on $port output.
        (tunneling the vnc traffic through ssh a bit more messy, but doable)

      - Quite a bit of code to be more careful when doing 8bpp indexed color, e.g.
        not assuming NCOLORS is 256, handling 8bit TrueColor and Direct Color, etc
        (I did all this probably in April, not quite clear in my mind now, but
        I did test it out a fair amount on my old Sparcstation 20 wrt a user's
        questions).
		
   * introduce rfbErr for Errors (Erik)
   * make rfbLog overridable (suggested by Erik)
   * don't reutrn on EINTR in WriteExact()/ReadExact() (suggested by Erik)
   * use AX_PREFIX_CONFIG_H to prefix constants in config.h to avoid
	 name clashes (also suggested by Erik)
   * transformed Bool, KeySym, Pixel to rfbBool, rfbKeySym, rfbPixel
	 (as suggested by Erik)
   * purged exit() calls (suggested by Erik)
   * fixed bug with maxRectsPerUpdate and Tight Encoding (these are incompatible)
   * checked sync with TightVNC 1.2.8:
   * viewonly/full passwords; if given a list, only the first is a full one
   * vncRandomBytes is a little more secure now
   * new weights for tight encoding
   * checked sync with RealVNC 3.3.7
   * introduced maxRectsPerUpdate
   * added first alpha version of LibVNCClient
   * added simple and simple15 example (really simple examples)
   * finally got around to fix configure in CVS
   * long standing http bug (.jar was sent twice) fixed by a friend of Karl named Mike
   * http options in cargs
   * when closing a client and no longer listening for new ones, don't crash
   * fixed a bug with ClientConnectionGone
   * endianness is checked at configure time
   * fixed a bug that prevented the first client from being closed
   * fixed that annoying "libvncserver-config --link" bug
   * make rfbReverseByte public (for rdp2vnc)
   * fixed compilation on OS X, IRIX, Solaris
   * install target for headers is now ${prefix}/include/rfb ("#include <rfb/rfb.h>")
   * renamed "sraRegion.h" to "rfbregion.h"
   * CARD{8,16,32} are more standard uint{8,16,32}_t now
   * fixed LinuxVNC colour handling
   * fixed a bug with pthreads where the connection was not closed
   * moved vncterm to main package (LinuxVNC included)
   * portability fixes (IRIX, OSX, Solaris)
   * more portable way to determine endianness and types of a given size
	 through autoconf based methods

# 2005-09-29

LibVNCServer now sports a brand new method to extend the protocol,
thanks to Rohit Kumar! He also extended the library to support RFB 3.7.
Furthermore, he contributed TightVNC file transfer protocol support to
LibVNCServer!

# 2005-05-25

LibVNCClient now features ZRLE decoding!

# 2005-05-15

Another round of valgrinding completed. This time it is augmented by
changes instigated by using Linus' sparse. In the course, the complete
sources were converted to ANSI C.

# 2005-05-07

The member socketInitDone was renamed to socketState, and no longer
contains a bool value. This allows us to quit a server cleanly from the
event loop via rfbShutdownServer(), so that the structures can be
cleaned up properly. This is demonstrated in examples/example.c.

# 2005-01-21

The function rfbMakeMaskFromAlphaSource() applies a Floyd-Steinberg
dither to approximate a binary mask from a cursor with alpha channel. A
demonstration can be found in test/cursortest.c.

# 2005-01-16

Renamed this page to reflect that LibVNCClient is actually very usable.

# 2005-01-16

Karl Runge has done awesome work to support cursors with alpha blending!
You can try it with x11vnc as in CVS, or wait a few more days for x11vnc
to be released officially!

# 2005-01-15

Happy new year! It begins with a new macro recorder based on
LibVNCServer/LibVNCClient using perl as script language. The macro
recorder is itself written in perl, and writes out perl scripts, acting
as a VNC proxy, so that you can connect a vncviewer to it, and it
records all your input, possibly looking for a certain button, image,
word, etc. before continuing. I called it VisualNaCro, and it's in CVS.

# 2004-12-20: Version 0.7

Just before christmas, a new release! Version 0.7 brings you the first
non-beta of LibVNCServer...

# 2004-12-02

Finally MinGW32 support. I only had problems with a vncviewer which
wouldn't connect to localhost: I use SDLvncviewer...

# 2004-12-01

LibVNCClient is getting better and better... Expect a very powerful
client soon!

# 2004-10-16

LibVNCServer has automated test, thanks to LibVNCClient (included). It
doesn't do ZRLE yet, and exposed some bugs, the only remaining of these
is CoRRE (not sure yet if it's a bug in the client or the server).

# 2004-09-14

Added success stories.

# 2004-09-07

The API was cleaned up. The structures and functions now have a prefix
(mostly "rfb", sometimes "zrle" or "sra") in order not to clutter
the namespace, while the structure's members don't need such a prefix.

# 2004-08-17

I finally came around to fix mouse behaviour in QEMU\'s VNC frontend for
Windows 98. Please find the patch [here](https://libvnc.github.io/oldstuff/qemu.tar.gz).
If mouse behaves strangely, try to wiggle the pointer to a free spot on the
desktop, hit Ctrl+Shift and release them. After that, the mouse should
behave nicely.

# 2004-06-07

After silently being added almost a year ago, libvncclient's API was
modified for real use, and three examples were added: ppmtest (a very
simple demo), SDLvncviewer, and vnc2mpg (which lets you record your VNC
session to a movie). Automated regression tests of the libraries are
planned.

# 2004-06-02

[x11vnc](http://www.karlrunge.com/x11vnc/)-0.6.1 was released! This
reflects the long way the original, small example has gone, improved in
many possible ways and having a broad user base.

# 2004-05-29

Some [patches](https://libvnc.github.io/oldstuff/qemu.tar.gz) were created for
[QEMU](http://qemu.org/), a FAST! emulator by Fabrice Bellard, to
control those sessions with a vncviewer.

# 2004-02-29

LibVNCServer is listed as a project using
[Valgrind](http://valgrind.org/)!

# 2003-11-07: Version 0.6

Version 0.6 is out! x11vnc performance boosts! You no longer need a c++
compiler in order to have ZRLE coding! LinuxVNC was added (This is to
the text console what x11vnc is to X11)!

# 2003-02-21

rdp2vnc is in rdesktop's CVS.

# 2003-02-19

A preliminary patch for rdesktop (CVS) to make rdp2vnc, a translator
from Windows Terminal Server's protocol to VNC's protocol, is
[available](https://libvnc.github.io/oldstuff/rdesktop-cvs+vnc.diff.gz). It needs a new version of
libvncserver; try CVS until I release 0.6.


# 2003-02-09: Version 0.5

Version 0.5 is out! Features include autoconf based configure, rpm
package (YMMV), cleanup of directory structure, NEW x11vnc! ZRLE
encoding! HTTP tunnelling through LibVNCServer's HTTP support! Many bug
fixes!

   * rpm packaging through autoconf
   * autoconf'ed the whole package (including optional support for zlib,
	 pthreads and libjpeg as well as zrle/c++)
   * moved appropriate files to contrib/ and examples/ respectively
   * fixed long standing cargs bug (Justin "Zippy" Dearing)
   * Even better x11vnc from Karl J. Runge! (supports different kbd layouts of
	 client/server)
   * Better x11vnc from Karl J. Runge!
   * fixed severe bug (Const Kaplinsky)
   * got patch from Const Kaplisnky with CursorPosUpdate encoding and some Docs
   * sync'ed with newest RealVNC (ZRLE encoding)
   * a HTTP request for tunnelling was added (to fool strict web proxies)
   * sync'ed with TightVNC 1.2.5


# 2002-07-28: Version 0.4

Version 0.4 is out! Biggest feature: NewFB encoding. Quite a few
bugfixes also (Thanks to all!).

   * support for NewFB from Const Kaplinsky
   * memory leaks squashed (localtime pseudo leak is still there :-)
   * small improvements for OSXvnc (still not working correctly)
   * synced with TightVNC 1.2.3
   * solaris compile cleanups
   * many x11vnc improvements
   * added backchannel, an encoding which needs special clients to pass
	 arbitrary data to the client
   * changes from Tim Jansen regarding multi threading and client blocking
	 as well as C++ compliancy
   * x11vnc can be controlled by starting again with special options if compiling
	 with LOCAL_CONTROL defined

# Version 0.3
   * added x11vnc, a x0rfbserver clone
   * regard deferUpdateTime in processEvents, if usec<0
   * initialize deferUpdateTime (memory "leak"!)
   * changed command line handling (arguments are parsed and then removed)
   * added very simple example: zippy
   * added rfbDrawLine, rfbDrawPixel

# 2001-12-14

A new version of [rdesktop+vnc](https://libvnc.github.io/oldstuff/rdesktop-1.1.0+vnc-0.2.tar.gz) is
available! (Includes support for other platforms keyboard mapping with
plain rdesktop!)

# 2001-10-23

Added a link to my homepage at the end.

# 2001-10-18

I released the rdp2vnc extensions as well as patches for general
keyboard handling, working inside Xvnc and `process_text2` (the famous
"font:" error) to rdesktop. Please find it on the [download
page](http://sourceforge.net/project/showfiles.php?group_id=32584).

# Version 0.2
   * inserted a deferUpdate mechanism (X11 independent).
   * removed deletion of requestedRegion
   * added rfbLoadConsoleFont
   * fixed font colour handling.
   * added rfbSelectBox
   * added rfbDrawCharWithClip to allow for clipping and a background colour.
   * fixed font colours
   * added rfbFillRect
   * added IO function to check password.
   * rfbNewClient now sets the socket in the fd_set (for the select() call)
   * when compiling the library with HAVE_PTHREADS and an application
	 which includes "rfb.h" without, the structures got mixed up.
	 So, the pthreads section is now always at the end, and also
	 you get a linker error for rfbInitServer when using two different
	 pthread setups.
   * fixed two deadlocks: when setting a cursor and when using CopyRect
   * fixed CopyRect when copying modified regions (they lost the modified
	 property)
   * WIN32 target compiles and works for example :-)
   * fixed CopyRect (was using the wrong order of rectangles...)
   	 should also work with pthreads, because copyrects are
	 always sent immediately (so that two consecutive copy rects
	 cannot conflict).
   * changed rfbUndrawCursor(rfbClientPtr) to (rfbScreenInfoPtr), because
   	 this makes more sense!
   * flag backgroundLoop in rfbScreenInfo (if having pthreads)
   * CopyRect & CopyRegion were implemented.
	 if you use a rfbDoCopyR* function, it copies the data in the
	 framebuffer. If you prefer to do that yourself, use rfbScheduleCopyR*
	 instead; this doesn't modify the frameBuffer.
   * added flag to optionally not send XCursor updates, but only RichCursor,
	 or if that is not possible, fall back to server side cursor.
	 This is useful if your cursor has many nice colours.
   * fixed java viewer on server side:
	 SendCursorUpdate would send data even before the client pixel format
	 was set, but the java applet doesn't like the server's format.
   * fixed two pthread issues:
	 rfbSendFramebuffer was sent by a ProcessClientMessage function
	 (unprotected by updateMutex).
	 cursor coordinates were set without protection by cursorMutex
   * source is now equivalent to TridiaVNC 1.2.1
   * pthreads now work (use iterators!)
   * cursors are supported (rfbSetCursor automatically undraws cursor)
   * support for 3 bytes/pixel (slow!)
   * server side colourmap support
   * fixed rfbCloseClient not to close the connection (pthreads!)
	 this is done lazily (and with proper signalling).
   * cleaned up mac.c (from original OSXvnc); now compiles (untested!)
   * compiles cleanly on Linux, IRIX, BSD, Apple (Darwin)
   * fixed prototypes

# 2001-10-13

A snapshot of
[LibVNCServer](http://sourceforge.net/project/showfiles.php?group_id=32584)
and
[RDP2VNC](http://sourceforge.net/project/showfiles.php?group_id=32584)
is now available. You can also download the
[diff](http://sourceforge.net/project/showfiles.php?group_id=32584)
against rdesktop-1.1.0. rdp2vnc also contains the patches for keyboards
other than PC keyboards, and you can specify \"-k fr\" again.

# Version 0.1

   * rewrote API to use pseudo-methods instead of required functions.
   * lots of clean up.
   * Example can show symbols now.
   * All encodings
   * HTTP
