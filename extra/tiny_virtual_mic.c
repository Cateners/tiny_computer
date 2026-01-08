#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define BUFSIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <socket_path> <target_device>\n", argv[0]);
        return 1;
    }

    // 1. Setup PulseAudio
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 1
    };

    pa_buffer_attr attr;
    attr.maxlength = (uint32_t) -1;
    attr.tlength = pa_usec_to_bytes(60000, &ss); // 目标延迟设为 60ms
    attr.prebuf = (uint32_t) -1;
    attr.minreq = (uint32_t) -1;
    attr.fragsize = (uint32_t) -1;

    int error;
    pa_simple *s = pa_simple_new(NULL, "AndroidStream", PA_STREAM_PLAYBACK, argv[2], "live_audio", &ss, NULL, &attr, &error);
    if (!s) {
        fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
        return 1;
    }

    // 2. Connect to Android Socket
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, argv[1], sizeof(addr.sun_path) - 1);

    printf("Connecting to %s...\n", argv[1]);
    while (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Waiting for server...\n");
        sleep(1); // Retry logic since Android might start later
    }
    printf("Connected! Playing audio...\n");

    // 3. Stream Loop
    uint8_t buf[BUFSIZE];
    while (1) {
        ssize_t r = read(sock_fd, buf, sizeof(buf));
        if (r <= 0) break;

        if (pa_simple_write(s, buf, (size_t)r, &error) < 0) {
            fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
            break;
        }
    }

    // Cleanup
    pa_simple_free(s);
    close(sock_fd);
    return 0;
}