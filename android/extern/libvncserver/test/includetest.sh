#!/bin/sh
#
# This tests if using the **installed** headers works.
#

# expects install prefix like /usr as an argument
PREFIX=$1
CMAKE_MAKE_PROGRAM=$2

TMPDIR=$(mktemp -d)

DESTDIR="$TMPDIR" $CMAKE_MAKE_PROGRAM install

echo \
"
#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

int main()
{
    return 0;
}
" > "$TMPDIR"/includetest.c

cc -I "$TMPDIR/$PREFIX" "$TMPDIR"/includetest.c
