// Compile with LIBS := -lvncserver -lxcb -lxcb-xtest -lxcb-keysyms
// Need CMake 3.24.0 to find these libraries. see https://cmake.org/cmake/help/v3.24/module/FindX11.html
// XWayland not support to read screen, because wayland not allow it.
// Read screen in wayland need use XDG desktop portals' interface `org.freedesktop.portal.Screenshot` and `org.freedesktop.portal.ScreenCast`
// Under some environment, this code not work well, see https://github.com/LibVNC/libvncserver/pull/503#issuecomment-1064472566

#include <rfb/rfb.h>
#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_keysyms.h>

void dirty_copy(rfbScreenInfoPtr rfbScreen, const uint8_t* data, int width, int height, int nbytes);
void convert_bgrx_to_rgb(const uint8_t* in, uint16_t width, uint16_t height, uint8_t* buff);
void get_window_size(xcb_connection_t* conn, xcb_window_t window, uint16_t* width, uint16_t* height);
void get_window_image(xcb_connection_t* conn, xcb_window_t window, uint8_t* buff);
void send_keycode(xcb_connection_t *conn, xcb_keycode_t keycode, int press);
void send_keysym(xcb_connection_t *conn, xcb_keysym_t keysym, int press);
void send_button(xcb_connection_t *conn, xcb_button_t button, int press);
void send_motion(xcb_connection_t *conn, int16_t x, int16_t y);

// global 
xcb_connection_t* conn;

static void keyCallback(rfbBool down, rfbKeySym keySym, rfbClientPtr client)
{
    (void)(client);
    send_keysym(conn, keySym, (int)down);
}

#define VNC_BUTTON_MASK_LEFT        rfbButton1Mask
#define VNC_BUTTON_MASK_MIDDLE      rfbButton2Mask
#define VNC_BUTTON_MASK_RIGHT       rfbButton3Mask
#define VNC_BUTTON_MASK_UP          rfbWheelUpMask
#define VNC_BUTTON_MASK_DOWN        rfbWheelDownMask

#define X11_BUTTON_LEFT     XCB_BUTTON_INDEX_1
#define X11_BUTTON_MIDDLE   XCB_BUTTON_INDEX_2
#define X11_BUTTON_RIGHT    XCB_BUTTON_INDEX_3
#define X11_BUTTON_UP       XCB_BUTTON_INDEX_4
#define X11_BUTTON_DOWN     XCB_BUTTON_INDEX_5

static void mouseCallback(int buttonMask, int x, int y, rfbClientPtr client)
{
    (void)(client);

    send_button(conn, X11_BUTTON_LEFT, !!(buttonMask & VNC_BUTTON_MASK_LEFT));
    send_button(conn, X11_BUTTON_MIDDLE, !!(buttonMask & VNC_BUTTON_MASK_MIDDLE));
    send_button(conn, X11_BUTTON_RIGHT, !!(buttonMask & VNC_BUTTON_MASK_RIGHT));
    send_button(conn, X11_BUTTON_UP, !!(buttonMask & VNC_BUTTON_MASK_UP));
    send_button(conn, X11_BUTTON_DOWN, !!(buttonMask & VNC_BUTTON_MASK_DOWN));

    send_motion(conn, (int16_t)x, (int16_t)y);
}

int main(int argc, char* argv[])
{
    conn = xcb_connect(NULL, NULL);
    const xcb_setup_t* setup = xcb_get_setup(conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t* screen = iter.data;
    xcb_window_t root = screen->root;

    int16_t width;
    int16_t height;
    get_window_size(conn, root, &width, &height);
    void* frameBuffer = malloc(4UL * width * height);

    rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc, argv, (int)width, (int)height, 8, 3, 4);
    rfbScreen->desktopName = "LibVNCServer X11 Example";
    rfbScreen->frameBuffer = (char*)malloc(4UL * width * height);
    rfbScreen->alwaysShared = TRUE;
    rfbScreen->kbdAddEvent = keyCallback;
    rfbScreen->ptrAddEvent = mouseCallback;
    rfbInitServer(rfbScreen);
    rfbRunEventLoop(rfbScreen, 10000, TRUE);
    
    while (TRUE)
    {
        get_window_image(conn, root, (uint8_t*)frameBuffer);
        dirty_copy(rfbScreen, (uint8_t*)frameBuffer, (int)width, (int)height, 4);
    }

    free(rfbScreen->frameBuffer);
    free(frameBuffer);
    xcb_disconnect(conn);
    return EXIT_SUCCESS;
}

void dirty_copy(rfbScreenInfoPtr rfbScreen, const uint8_t* data, int width, int height, int nbytes)
{
    // check dirty by line, because it is convenient to copy
    for (int y = 0; y < height; y++)
    {
        rfbBool dirty = FALSE;
        for (int x = 0; x < width; x++)
        {
            const void* s1 = &rfbScreen->frameBuffer[(y*width+x)*nbytes];
            const void* s2 = &data[(y*width+x)*nbytes];
            if (memcmp(s1, s2, nbytes) != 0)
            {
                dirty = TRUE;
                break;
            }
        }

        if (dirty)
        {
            memcpy(&rfbScreen->frameBuffer[y*width*nbytes], &data[y*width*nbytes], width*nbytes);
            rfbMarkRectAsModified(rfbScreen, 0, y, width, y+1);
        }
    }
}

void convert_bgrx_to_rgb(const uint8_t* in, uint16_t width, uint16_t height, uint8_t* buff)
{
    for (uint16_t y = 0; y < height; y++)
    {
        for(uint16_t x = 0; x < width; x++)
        {
            buff[(y*width+x)*4] = in[(y*width+x)*4 + 2];
            buff[(y*width+x)*4 + 1] = in[(y*width+x)*4 + 1];
            buff[(y*width+x)*4 + 2] = in[(y*width+x)*4];
        }
    }
}


void get_window_size(xcb_connection_t* conn, xcb_window_t window, uint16_t* width, uint16_t* height)
{
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, window);
    xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(conn, cookie, NULL);

    *width = reply->width;
    *height = reply->height;
    free(reply);
}

void get_window_image(xcb_connection_t* conn, xcb_window_t window, uint8_t* buff)
{
    uint16_t width = 0;
    uint16_t height = 0;
    get_window_size(conn, window, &width, &height);

    // will failed in wayland, xcb_get_image_data will return NULL, convert_bgrx_to_rgb will abort
    xcb_get_image_cookie_t cookie = xcb_get_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, window, 0, 0, width, height, UINT32_MAX);
    xcb_get_image_reply_t* reply = xcb_get_image_reply(conn, cookie, NULL);
    convert_bgrx_to_rgb(xcb_get_image_data(reply), width, height, buff);
    free(reply);
}


void send_keycode(xcb_connection_t *conn, xcb_keycode_t keycode, int press) 
{
    xcb_test_fake_input(conn, press ? XCB_KEY_PRESS : XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_flush(conn);
}


void send_keysym(xcb_connection_t *conn, xcb_keysym_t keysym, int press) 
{
    xcb_key_symbols_t* symbols = xcb_key_symbols_alloc(conn);
    xcb_keycode_t* code = xcb_key_symbols_get_keycode(symbols, keysym);
    for (; code != NULL && *code != XCB_NO_SYMBOL; code++)
    {
        send_keycode(conn, *code, press);
    }
    xcb_key_symbols_free(symbols);
}

void send_button(xcb_connection_t *conn, xcb_button_t button, int press)
{
    xcb_test_fake_input(conn, press ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_flush(conn);
}

void send_motion(xcb_connection_t *conn, int16_t x, int16_t y)
{
    xcb_test_fake_input(conn, XCB_MOTION_NOTIFY, 0, XCB_CURRENT_TIME, XCB_NONE, x, y, 0);
    xcb_flush(conn);
}
