/*
  Fuzzing server for LibVNCServer.

  This is used by OSS-Fuzz at https://android.googlesource.com/platform/external/oss-fuzz/+/refs/heads/upstream-master/projects/libvnc
  which is integrated into our CI at `.github/workflows/cifuzz.yaml`.
  OSS-Fuzz basically runs every executable in the $OUT dir with LLVMFuzzerTestOneInput in it,
  so other fuzzers can be added later on as well.

  If you want to run the fuzzer locally, you have to build like that:

  ```
  mkdir build
  cd build
  CC=clang LIB_FUZZING_ENGINE="-fsanitize=fuzzer" CFLAGS="-fsanitize=address,fuzzer-no-link -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1" cmake ..
  cmake --build .
  ```

  and then execute `build/fuzz_server`. You can add some command line options, based on
  the fuzzing engine you have used to compile it, see https://llvm.org/docs/LibFuzzer.html

 */


#include <rfb/rfb.h>

static int initialized = 0;
rfbScreenInfoPtr server;
char *fakeargv[] = {"fuzz_server"};

extern size_t fuzz_offset;
extern size_t fuzz_size;
extern const uint8_t *fuzz_data;


int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (initialized == 0) {
        int fakeargc=1;
        server=rfbGetScreen(&fakeargc,fakeargv,400,300,8,3,4);
        server->frameBuffer=malloc(400*300*4);
        rfbInitServer(server);
        initialized = 1;
    }
    rfbClientPtr cl = rfbNewClient(server, RFB_INVALID_SOCKET - 1);

    fuzz_data = Data;
    fuzz_offset = 0;
    fuzz_size = Size;
    while (cl->sock != RFB_INVALID_SOCKET) {
        rfbProcessClientMessage(cl);
    }
    rfbClientConnectionGone(cl);
    return 0;
}

