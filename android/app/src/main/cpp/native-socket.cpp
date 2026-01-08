#include <jni.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <android/log.h>
#include <cerrno>

#define LOG_TAG "NativeAudio"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

int server_fd = -1;
int client_fd = -1;

extern "C" JNIEXPORT jint JNICALL
Java_com_example_tiny_1computer_AudioStream_nativeInit(JNIEnv *env, jobject thiz, jstring path) {
    const char *socket_path = env->GetStringUTFChars(path, 0);
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        LOGE("Socket creation failed");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path); // Remove existing file if any

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOGE("Bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 1) == -1) {
        LOGE("Listen failed");
        close(server_fd);
        return -1;
    }

    env->ReleaseStringUTFChars(path, socket_path);
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_tiny_1computer_AudioStream_nativeAccept(JNIEnv *env, jobject thiz) {
    if (server_fd == -1) return -1;
    // Blocks here until Linux connects
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        LOGE("Accept failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_tiny_1computer_AudioStream_nativeSend(JNIEnv *env, jobject thiz, jbyteArray data, jint size) {
    if (client_fd == -1) return -1;
    
    jbyte *buffer = env->GetByteArrayElements(data, NULL);
    ssize_t sent = write(client_fd, buffer, size);
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
    
    if (sent == -1 && errno != EAGAIN) {
        LOGE("Write failed (Broken Pipe?)");
        return -1;
    }
    return sent;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_tiny_1computer_AudioStream_nativeClose(JNIEnv *env, jobject thiz) {
    if (client_fd != -1) { close(client_fd); client_fd = -1; }
    if (server_fd != -1) { close(server_fd); server_fd = -1; }
}