#!/usr/bin/env python3
# Copyright (C)2017 Andreas Weigel.  All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import websockets
import base64

'''
    Create websocket frames for the wstest websocket decoding unit test.

    Generates c ws_frame_test structure definitions
    included by wstest.c.
'''


def add_field(s, name, value, first=False):
    deli = ",\n\t\t"
    if first:
        deli = "\t\t"
    s += "{2}.{0}={1}".format(name, value, deli)
    return s


class Testframe():
    def __init__(self, frame, descr, modify_bytes={}, experrno=0, mask=True, opcode_overwrite=False):
        self.frame = frame
        self.descr = descr
        self.modify_bytes = modify_bytes
        self.experrno = experrno
        self.b64 = True if frame.opcode == 1 or opcode_overwrite == 1 else False
        self.mask = mask

    def to_carray_initializer(self, buf):
        values = []
        for i in range(len(buf)):
            values.append("0X{0:02X}".format(buf[i]))

        if self.modify_bytes != {}:
            for k in self.modify_bytes:
                values[k] = "0X{0:02X}".format(self.modify_bytes[k])

        return "{{{0}}}".format(",".join(values))


    def set_frame_buf(self, buf):
        self.frame_carray = self.to_carray_initializer(buf)
        self.framelen = len(buf)

    def __str__(self):
        print("processing frame: {0}".format(self.descr))
        the_frame = self.frame
        if self.b64:
            olddata = self.frame.data
            newdata = base64.b64encode(self.frame.data)
            #print("converting\n{0}\nto{1}\n".format(olddata, newdata))
            the_frame = websockets.framing.Frame(self.frame.fin, self.frame.opcode, base64.b64encode(olddata))
        websockets.framing.write_frame(the_frame, self.set_frame_buf, self.mask)
        s = "\t{\n"
        s = add_field(s, "frame", "{0}".format(self.frame_carray), True)
        s = add_field(s, "expectedDecodeBuf", self.to_carray_initializer(self.frame.data))
        s = add_field(s, "frame_len", self.framelen)
        s = add_field(s, "raw_payload_len", len(self.frame.data))
        s = add_field(s, "expected_errno", self.experrno)
        s = add_field(s, "descr", "\"{0}\"".format(self.descr))
        s = add_field(s, "i", "0")
        s = add_field(s, "simulate_sock_malfunction_at", "0")
        s = add_field(s, "errno_val", "0")
        s = add_field(s, "close_sock_at", "0")
        s += "\n\t}"
        return s

### create test frames
flist = []
### standard text frames with different lengths
flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray("Testit", encoding="utf-8")), "Short valid text frame"))
flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray("Frame2 does contain much more text and even goes beyond the 126 byte len field. Frame2 does contain much more text and even goes beyond the 126 byte len field.", encoding="utf-8")),
    "Mid-long valid text frame"))
#flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray([(x % 26) + 65 for x in range(100000)])), "100k text frame (ABC..YZABC..)"))

### standard binary frames with different lengths
flist.append(Testframe(websockets.framing.Frame(1, 2, bytearray("Testit", encoding="utf-8")), "Short valid binary frame"))
flist.append(Testframe(websockets.framing.Frame(1, 2, bytearray("Frame2 does contain much more text and even goes beyond the 126 byte len field. Frame2 does contain much more text and even goes beyond the 126 byte len field.", encoding="utf-8")),
    "Mid-long valid binary frame"))
#flist.append(Testframe(websockets.framing.Frame(1, 2, bytearray([(x % 26) + 65 for x in range(100000)])), "100k binary frame (ABC..YZABC..)"))

### some conn reset frames, one with no close message, one with close message
flist.append(Testframe(websockets.framing.Frame(1, 8, bytearray(list([0x03, 0xEB]))), "Close frame (Reason 1003)", experrno="ECONNRESET"))
flist.append(Testframe(websockets.framing.Frame(1, 8, bytearray(list([0x03, 0xEB])) + bytearray("I'm a close reason and much more than that!", encoding="utf-8")), "Close frame (Reason 1003) and msg", experrno="ECONNRESET"))

### invalid header values
flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray("Testit", encoding="utf-8")), "Invalid frame: Wrong masking", experrno="EPROTO", mask=False))
flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray("..Lore Ipsum", encoding="utf-8")), "Invalid frame: Length of < 126 with add. 16 bit len field", experrno="EPROTO", modify_bytes={ 1: 0xFE, 2: 0x00, 3: 0x0F}))
flist.append(Testframe(websockets.framing.Frame(1, 1, bytearray("........Lore Ipsum", encoding="utf-8")), "Invalid frame: Length of < 126 with add. 64 bit len field", experrno="EPROTO", modify_bytes={ 1: 0xFF, 2: 0x00, 3: 0x00, 4: 0x00, 5: 0x00, 6: 0x00, 7: 0x00, 8: 0x80, 9: 0x40}))

frag1 = websockets.framing.Frame(0, 1, bytearray("This is a fragmented websocket...", encoding="utf-8"))
frag2 = websockets.framing.Frame(0, 0, bytearray("... and it goes on...", encoding="utf-8"))
frag3 = websockets.framing.Frame(1, 0, bytearray("and on and stop", encoding="utf-8"))
flist.append(Testframe(frag1, "Continuation test frag1"))
flist.append(Testframe(frag2, "Continuation test frag2", opcode_overwrite=1))
flist.append(Testframe(frag3, "Continuation test frag3", opcode_overwrite=1))

s = "struct ws_frame_test tests[] = {\n"
for i in range(len(flist)):
    s += flist[i].__str__()
    if (i + 1 < len(flist)):
        s += ","
    s += "\n"
s += "};\n"

with open("wstestdata.inc", "w") as cdatafile:
    cdatafile.write(s)
