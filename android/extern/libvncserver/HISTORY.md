This is the original author Dscho's account on the project's initial history.
If you want to know what happened afterwards, have a look at [NEWS](NEWS.md).

History
=======

LibVNCServer is based on Tridia VNC and OSXvnc, which in turn are based on
the original code from ORL/AT&T.

When I began hacking with computers, my first interest was speed. So, when I
got around assembler, I programmed the floppy to do much of the work, because
its clock rate was higher than that of my C64. This was my first experience
with client/server techniques.

When I came around Xwindows (much later), I was at once intrigued by the
elegance of such connectedness between the different computers. I used it
a lot - not the least priority lay on games. However, when I tried it over
modem from home, it was no longer that much fun.

When I started working with ASP (Application Service Provider) programs, I
tumbled across Tarantella and Citrix. Being a security fanatic, the idea of
running a server on windows didn't appeal to me, so Citrix went down the
basket. However, Tarantella has its own problems (security as well as the
high price). But at the same time somebody told me about this "great little
administrator's tool" named VNC. Being used to windows programs' sizes, the
surprise was reciprocal inverse to the size of VNC!

At the same time, the program "rdesktop" (a native Linux client for the
Terminal Services of Windows servers) came to my attention. There where even
works under way to make a protocol converter "rdp2vnc" out of this. However,
my primary goal was a slow connection and rdp2vnc could only speak RRE
encoding, which is not that funny with just 5kB/s. Tim Edmonds, the original
author of rdp2vnc, suggested that I adapt it to Hextile Encoding, which is
better. I first tried that, but had no success at all (crunchy pictures).

Also, I liked the idea of an HTTP server included and possibly other
encodings like the Tight Encodings from Const Kaplinsky. So I started looking
for libraries implementing a VNC server where I could steal what I can't make.
I found some programs based on the demo server from AT&T, which was also the
basis for rdp2vnc (can only speak Raw and RRE encoding). There were some
rumors that GGI has a VNC backend, but I didn't find any code, so probably
there wasn't a working version anyway.

All of a sudden, everything changed: I read on freshmeat that "OSXvnc" was
released. I looked at the code and it was not much of a problem to work out
a simple server - using every functionality there is in Xvnc. It became clear
to me that I *had* to build a library out of it, so everybody can use it.
Every change, every new feature can propagate to every user of it.

It also makes everything easier:
 You don't care about the cursor, once set (or use the standard cursor).
You don't care about those sockets. You don't care about encodings.
You just change your frame buffer and inform the library about it. Every once
in a while you call rfbProcessEvents and that's it.
