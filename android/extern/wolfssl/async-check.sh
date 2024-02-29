#!/bin/bash

# async-check.sh

# This script creates symbolic links to the required asynchronous
# file for using the asynchronous simulator and make check
#
#     $ ./async-check [keep]
#
#     - keep: (default off) ./async and links kept around for inspection
#

function Usage() {
    printf '\n%s\n' "Usage: $0 [keep]"
    printf '\n%s\n\n' "Where \"keep\" means keep (default off) async files around for inspection"
    printf '%s\n' "EXAMPLE:"
    printf '%s\n' "---------------------------------"
    printf '%s\n' "./async-check.sh keep"
    printf '%s\n\n' "---------------------------------"
}

ASYNC_REPO=git@github.com:wolfSSL/wolfAsyncCrypt.git
#ASYNC_REPO=../wolfAsyncCrypt

# Optionally keep async files
if [ "x$1" == "xkeep" ]; then KEEP="yes"; else KEEP="no"; fi


if [ -d ./async ];
then
    echo "\n\nUsing existing async repo\n\n"
else
    # make a clone of the wolfAsyncCrypt repository
    git clone --depth 1 $ASYNC_REPO async
    [ $? -ne 0 ] && echo "\n\nCouldn't checkout the wolfAsyncCrypt repository\n\n" && exit 1
fi

# setup auto-conf
./autogen.sh


# link files
ln -s -F ../../async/wolfcrypt/src/async.c ./wolfcrypt/src/async.c
ln -s -F ../../async/wolfssl/wolfcrypt/async.h ./wolfssl/wolfcrypt/async.h
ln -s -F ../../../../async/wolfcrypt/src/port/intel/quickassist.c ./wolfcrypt/src/port/intel/quickassist.c
ln -s -F ../../../../async/wolfcrypt/src/port/intel/quickassist_mem.c ./wolfcrypt/src/port/intel/quickassist_mem.c
ln -s -F ../../../../async/wolfcrypt/src/port/intel/README.md ./wolfcrypt/src/port/intel/README.md
ln -s -F ../../../../async/wolfssl/wolfcrypt/port/intel/quickassist.h ./wolfssl/wolfcrypt/port/intel/quickassist.h
ln -s -F ../../../../async/wolfssl/wolfcrypt/port/intel/quickassist_mem.h ./wolfssl/wolfcrypt/port/intel/quickassist_mem.h
ln -s -F ../../../../async/wolfcrypt/src/port/cavium/cavium_nitrox.c ./wolfcrypt/src/port/cavium/cavium_nitrox.c
ln -s -F ../../../../async/wolfssl/wolfcrypt/port/cavium/cavium_nitrox.h ./wolfssl/wolfcrypt/port/cavium/cavium_nitrox.h
ln -s -F ../../../../async/wolfcrypt/src/port/cavium/README.md ./wolfcrypt/src/port/cavium/README.md


./configure --enable-asynccrypt --enable-all
make check
[ $? -ne 0 ] && echo "\n\nMake check failed. Debris left for analysis." && exit 1


# Clean up
popd
if [ "x$KEEP" == "xno" ];
then
    unlink ./wolfcrypt/src/async.c
    unlink ./wolfssl/wolfcrypt/async.h
    unlink ./wolfcrypt/src/port/intel/quickassist.c
    unlink ./wolfcrypt/src/port/intel/quickassist_mem.c
    unlink ./wolfcrypt/src/port/intel/README.md
    unlink ./wolfssl/wolfcrypt/port/intel/quickassist.h
    unlink ./wolfssl/wolfcrypt/port/intel/quickassist_mem.h
    unlink ./wolfcrypt/src/port/cavium/cavium_nitrox.c
    unlink ./wolfssl/wolfcrypt/port/cavium/cavium_nitrox.h
    unlink ./wolfcrypt/src/port/cavium/README.md

    rm -rf ./async

    # restore original README.md files
    git checkout -- wolfcrypt/src/port/cavium/README.md
    git checkout -- wolfcrypt/src/port/intel/README.md
fi
