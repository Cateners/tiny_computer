/*
 * Copyright (c) 2020  Gaurav Ujjwal.
 *
 * SPDX-License-Identifier:  GPL-3.0-or-later
 *
 * See COPYING.txt for more details.
 */

#include <jni.h>
#include <GLES2/gl2.h>
#include <rfb/rfbclient.h>

#include "ClientEx.h"
#include "Utility.h"


/******************************************************************************
 * Library Initialization
 *****************************************************************************/

struct JniContext {
    JavaVM *vm;                     //JVM Instance
    jclass managedCls;              //Managed `VncClient` class
    jmethodID cbFramebufferUpdated; //Cached reference to managed callback

    JNIEnv *getEnv() const {
        JNIEnv *env = nullptr;

        if (vm != nullptr && vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK)
            return env;

        return nullptr; //Should not happen
    }
};

static JniContext context{};

/**
 * Called when our library is loaded.
 */
JNIEXPORT jint
JNI_OnLoad(JavaVM *vm, void *unused) {
    context.vm = vm;

    if (context.getEnv() == nullptr)
        return JNI_ERR;

    return JNI_VERSION_1_6;
}

JNIEXPORT void
JNI_OnUnload(JavaVM *vm, void *reserved) {
    if (context.managedCls != nullptr)
        context.getEnv()->DeleteGlobalRef(context.managedCls);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_initLibrary(JNIEnv *env, jclass clazz) {
    context.managedCls = (jclass) env->NewGlobalRef(clazz);
    context.cbFramebufferUpdated = env->GetMethodID(clazz, "cbFinishedFrameBufferUpdate", "()V");
    //TODO: Cache more method IDs so we don't have to repeatedly search them

    rfbClientLog = &log_info;
    rfbClientErr = &log_error;
}


/******************************************************************************
 * rfbClient Callbacks
 *****************************************************************************/

static char *onGetPassword(rfbClient *client) {
    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    auto mid = env->GetMethodID(cls, "cbGetPassword", "()Ljava/lang/String;");
    auto jPassword = (jstring) env->CallObjectMethod(obj, mid);

    return getNativeStrCopy(env, jPassword);
}

static rfbCredential *onGetCredential(rfbClient *client, int credentialType) {
    if (credentialType != rfbCredentialTypeUser) {
        //Only user credentials (i.e. username & password) are currently supported
        rfbClientErr("Unsupported credential type requested");
        return nullptr;
    }

    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    //Retrieve credentials
    jmethodID mid = env->GetMethodID(cls, "cbGetCredential",
                                     "()Lcom/gaurav/avnc/vnc/UserCredential;");
    jobject jCredential = env->CallObjectMethod(obj, mid);
    if (jCredential == nullptr) {
        return nullptr;
    }

    //Extract username & password
    auto jCredentialCls = env->GetObjectClass(jCredential);
    auto usernameField = env->GetFieldID(jCredentialCls, "username", "Ljava/lang/String;");
    auto jUsername = env->GetObjectField(jCredential, usernameField);

    auto passwordField = env->GetFieldID(jCredentialCls, "password", "Ljava/lang/String;");
    auto jPassword = env->GetObjectField(jCredential, passwordField);

    //Create native rfbCredential
    auto credential = (rfbCredential *) malloc(sizeof(rfbCredential));
    credential->userCredential.username = getNativeStrCopy(env, (jstring) jUsername);
    credential->userCredential.password = getNativeStrCopy(env, (jstring) jPassword);

    return credential;
}

static void onBell(rfbClient *client) {
    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    jmethodID mid = env->GetMethodID(cls, "cbBell", "()V");
    env->CallVoidMethod(obj, mid);
}

static void onGotXCutText(rfbClient *client, const char *text, int len, bool is_utf8) {
    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    jmethodID mid = env->GetMethodID(cls, "cbGotXCutText", "([BZ)V");
    jbyteArray bytes = env->NewByteArray(len);
    env->SetByteArrayRegion(bytes, 0, len, reinterpret_cast<const jbyte *>(text));
    env->CallVoidMethod(obj, mid, bytes, is_utf8);
}

static void onGotXCutTextLatin1(rfbClient *client, const char *text, int len) {
    onGotXCutText(client, text, len, false);
}

static void onGotXCutTextUTF8(rfbClient *client, const char *text, int len) {
    onGotXCutText(client, text, len, true);
}

static rfbBool onHandleCursorPos(rfbClient *client, int x, int y) {
    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    jmethodID mid = env->GetMethodID(cls, "cbHandleCursorPos", "(II)V");
    env->CallVoidMethod(obj, mid, x, y);

    return TRUE;
}

static void onFinishedFrameBufferUpdate(rfbClient *client) {
    auto obj = getManagedClient(client);
    auto env = context.getEnv();

    env->CallVoidMethod(obj, context.cbFramebufferUpdated);
}

/**
 * We need to use our own allocator to know when frame size has changed.
 * and to acquire framebuffer lock during modification.
 */
static rfbBool onMallocFrameBuffer(rfbClient *client) {

    const auto width = client->width;
    const auto height = client->height;
    const auto requestedSize = (uint64_t) width * height * client->format.bitsPerPixel / 8;

    if (requestedSize >= SIZE_MAX) {
        rfbClientErr("CRITICAL: cannot allocate frameBuffer, requested size is too large\n");
        return FALSE;
    }

    auto allocSize = (size_t) requestedSize;
    auto ex = getClientExtension(client);

    LOCK(ex->mutex);
    {

        if (client->frameBuffer)
            free(client->frameBuffer);

        client->frameBuffer = static_cast<uint8_t *>(malloc(allocSize));

        if (client->frameBuffer) {
            ex->fbRealWidth = width;
            ex->fbRealHeight = height;
            memset(client->frameBuffer, 0, allocSize); //Clear any garbage
        } else {
            ex->fbRealWidth = 0;
            ex->fbRealHeight = 0;
        }
    }
    UNLOCK(ex->mutex);

    if (client->frameBuffer == nullptr) {
        rfbClientErr("CRITICAL: frameBuffer allocation failed\n");
        return FALSE;
    }

    auto obj = getManagedClient(client);
    auto env = context.getEnv();
    auto cls = context.managedCls;

    auto mid = env->GetMethodID(cls, "cbFramebufferSizeChanged", "(II)V");
    env->CallVoidMethod(obj, mid, width, height);

    return TRUE;
}

static void onGotCursorShape(rfbClient *client, int xHot, int yHot, int width, int height, int bytesPerPixel) {
    auto ex = getClientExtension(client);

    LOCK(ex->mutex);

    //Steel buffers from rfbClient
    updateCursor(ex->cursor, client->rcSource, client->rcMask, (uint16_t) width, (uint16_t) height,
                 (uint16_t) xHot, (uint16_t) yHot);
    client->rcSource = NULL;
    client->rcMask = NULL;

    UNLOCK(ex->mutex);

    //Fake framebuffer update to trigger rendering
    onFinishedFrameBufferUpdate(client);
}

/**
 * Hooks callbacks to rfbClient.
 */
static void setCallbacks(rfbClient *client) {
    client->GetPassword = onGetPassword;
    client->GetCredential = onGetCredential;
    client->Bell = onBell;
    client->GotXCutText = onGotXCutTextLatin1;
    client->GotXCutTextUTF8 = onGotXCutTextUTF8;
    client->HandleCursorPos = onHandleCursorPos;
    client->FinishedFrameBufferUpdate = onFinishedFrameBufferUpdate;
    client->MallocFrameBuffer = onMallocFrameBuffer;
    client->GotCursorShape = onGotCursorShape;
}


/******************************************************************************
 * Native method Implementation
 *****************************************************************************/

extern "C"
JNIEXPORT jlong JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeClientCreate(JNIEnv *env, jobject thiz) {
    rfbClient *client = rfbGetClient(8, 3, 4);
    if (client == nullptr)
        return 0;

    if (!assignClientExtension(client))
        return 0;

    setCallbacks(client);
    client->canHandleNewFBSize = TRUE;

    //Attach reference to managed object
    auto obj = env->NewGlobalRef(thiz);
    setManagedClient(client, obj);

    return (jlong) client;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeConfigure(JNIEnv *env, jobject thiz, jlong client_ptr,
                                                   jint securityType, jboolean use_local_cursor, jint image_quality,
                                                   jboolean use_raw_encoding) {
    auto client = (rfbClient *) client_ptr;

    // 0 means all auth types
    if (securityType != 0) {
        uint32_t auth[1] = {static_cast<uint32_t>(securityType)};
        SetClientAuthSchemes(client, auth, 1);
    }

    if (use_local_cursor) {
        client->appData.useRemoteCursor = TRUE;
        getClientExtension(client)->cursor = newCursor();
    }

    client->appData.qualityLevel = image_quality;
    if (use_raw_encoding)
        client->appData.encodingsString = "raw";
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSetDest(JNIEnv *env, jobject thiz, jlong client_ptr,
                                                 jstring host, jint port) {
    auto client = (rfbClient *) client_ptr;
    client->destHost = getNativeStrCopy(env, host);
    client->destPort = port;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeInit(JNIEnv *env, jobject thiz, jlong client_ptr,
                                              jstring host, jint port) {
    auto client = (rfbClient *) client_ptr;

    client->serverHost = getNativeStrCopy(env, host);
    client->serverPort = port < 100 ? port + 5900 : port;

    if (rfbInitClient(client, nullptr, nullptr)) {
        return JNI_TRUE;
    }

    return JNI_FALSE;

}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeIsServerMacOS(JNIEnv *env, jobject thiz, jlong client_ptr) {
    auto client = (rfbClient *) client_ptr;
    return client->serverMajor == 3 && client->serverMinor == 889;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeCleanup(JNIEnv *env, jobject thiz,
                                                 jlong client_ptr) {
    auto client = (rfbClient *) client_ptr;

    if (client->frameBuffer) {
        free(client->frameBuffer);
        client->frameBuffer = nullptr;
    }

    auto managedClient = getManagedClient(client);
    env->DeleteGlobalRef(managedClient);

    freeClientExtension(client);
    rfbClientCleanup(client);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeProcessServerMessage(JNIEnv *env, jobject thiz,
                                                              jlong client_ptr,
                                                              jint u_sec_timeout) {
    auto client = (rfbClient *) client_ptr;

    auto waitResult = WaitForMessage(client, static_cast<unsigned int>(u_sec_timeout));

    if (waitResult == 0) // Timeout
        return JNI_TRUE;

    if (waitResult > 0 && HandleRFBServerMessage(client))
        return JNI_TRUE;

    return JNI_FALSE;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeGetLastErrorStr(JNIEnv *env, jobject thiz) {
    auto str = errnoToStr(errno);
    return env->NewStringUTF(str);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSendKeyEvent(JNIEnv *env, jobject thiz, jlong client_ptr,
                                                      jint key_sym, jint xt_code, jboolean is_down) {
    auto client = (rfbClient *) client_ptr;
    rfbBool down = is_down ? TRUE : FALSE;

    if (xt_code > 0 && SendExtendedKeyEvent(client, key_sym, xt_code, down))
        return JNI_TRUE;
    else
        return SendKeyEvent(client, key_sym, down);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSendPointerEvent(JNIEnv *env, jobject thiz, jlong client_ptr, jint x, jint y,
                                                          jint mask) {
    return (jboolean) SendPointerEvent((rfbClient *) client_ptr, x, y, mask);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSendCutText(JNIEnv *env, jobject thiz, jlong client_ptr, jbyteArray bytes,
                                                     jboolean is_utf8) {
    auto client = (rfbClient *) client_ptr;
    auto textBuffer = env->GetByteArrayElements(bytes, nullptr);
    auto textLen = env->GetArrayLength(bytes);
    auto textChars = reinterpret_cast<char *>(textBuffer);

    rfbBool result = is_utf8
                     ? SendClientCutTextUTF8(client, textChars, textLen)
                     : SendClientCutText(client, textChars, textLen);

    env->ReleaseByteArrayElements(bytes, textBuffer, JNI_ABORT);
    return (jboolean) result;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeIsUTF8CutTextSupported(JNIEnv *env, jobject thiz, jlong client_ptr) {
    return (jboolean) (((rfbClient *) client_ptr)->extendedClipboardServerCapabilities != 0);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSetDesktopSize(JNIEnv *env, jobject thiz, jlong client_ptr, jint width,
                                                        jint height) {
    return (jboolean) SendExtDesktopSize((rfbClient *) client_ptr, width, height);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeRefreshFrameBuffer(JNIEnv *env, jobject thiz, jlong clientPtr) {
    auto client = (rfbClient *) clientPtr;
    return (jboolean) SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, TRUE);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeSetAutomaticFramebufferUpdates(JNIEnv *env, jobject thiz, jlong client_ptr,
                                                                        jboolean enabled) {
    auto client = ((rfbClient *) client_ptr);
    client->automaticUpdateRequests = enabled ? TRUE : FALSE;
    if (enabled) SendIncrementalFramebufferUpdateRequest(client);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeGetDesktopName(JNIEnv *env, jobject thiz, jlong client_ptr) {
    auto client = (rfbClient *) client_ptr;
    return env->NewStringUTF(client->desktopName ? client->desktopName : "");
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeGetWidth(JNIEnv *env, jobject thiz, jlong client_ptr) {
    return ((rfbClient *) client_ptr)->width;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeGetHeight(JNIEnv *env, jobject thiz, jlong client_ptr) {
    return ((rfbClient *) client_ptr)->height;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeIsEncrypted(JNIEnv *env, jobject thiz, jlong client_ptr) {
    return static_cast<jboolean>(((rfbClient *) client_ptr)->tlsSession ? JNI_TRUE : JNI_FALSE);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeUploadFrameTexture(JNIEnv *env, jobject thiz,
                                                            jlong client_ptr) {
    auto client = (rfbClient *) client_ptr;
    auto ex = getClientExtension(client);

    LOCK(ex->mutex);

    if (client->frameBuffer) {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     ex->fbRealWidth,
                     ex->fbRealHeight,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     client->frameBuffer);
    }

    UNLOCK(ex->mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_gaurav_avnc_vnc_VncClient_nativeUploadCursor(JNIEnv *env, jobject thiz, jlong client_ptr, jint px, jint py) {

    auto client = (rfbClient *) client_ptr;
    auto ex = getClientExtension(client);
    auto cursor = ex->cursor;

    if (!cursor)
        return;

    //Current algo for cursor rendering is slightly weird. Main issue is that
    //glTexSubImage2D() does not perform any composition with target texture.
    //So, we have to manually blend transparent/invalid pixels of the cursor
    //with corresponding pixels from framebuffer. scratchBuffer is used for
    //this composition.

    LOCK(ex->mutex);

    //Effective cursor position in framebuffer
    int32_t fbCursorX = px - cursor->xHot;
    int32_t fbCursorY = py - cursor->yHot;

    //Rectangular portion of the framebuffer to be updated.
    //Cursor can overflow outside the framebuffer if moved near the edges,
    //but glTexSubImage2D() doesn't allow values outside target texture,
    //so we need to only update the intersection of framebuffer & cursor.
    int32_t left = -1, top = -1, right = -1, bottom = -1;

    auto fb = (uint32_t *) client->frameBuffer;
    auto buffer = (uint32_t *) cursor->buffer;
    auto scratch = (uint32_t *) cursor->scratchBuffer;
    auto mask = cursor->mask;

    //Scratch buffer index
    int32_t z = 0;

    for (int32_t y = 0; y < cursor->height; ++y) {
        for (int32_t x = 0; x < cursor->width; ++x) {

            //Corresponding pixel in framebuffer
            auto fbX = fbCursorX + x;
            auto fbY = fbCursorY + y;

            if (fbX >= 0 && fbX < ex->fbRealWidth && fbY >= 0 && fbY < ex->fbRealHeight) {
                auto isValidPixel = mask[y * cursor->width + x];
                if (isValidPixel)
                    scratch[z++] = buffer[y * cursor->width + x];
                else
                    scratch[z++] = fb[fbY * ex->fbRealWidth + fbX];

                if (left == -1 && top == -1) {
                    left = fbX;
                    top = fbY;
                }
                right = fbX;
                bottom = fbY;
            }
        }
    }

    if (left >= 0 && top >= 0)
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        left,
                        top,
                        right - left + 1,
                        bottom - top + 1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        scratch);

    UNLOCK(ex->mutex);
}
