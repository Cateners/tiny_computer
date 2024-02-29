#ifndef RFBCLIENT_H
#define RFBCLIENT_H

/**
 * @defgroup libvncclient_api LibVNCClient API Reference
 * @{
 */

/*
 *  Copyright (C) 2017 D. R. Commander.  All Rights Reserved.
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/**
 * @file rfbclient.h
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN /* Prevent loading any Winsock 1.x headers from windows.h */
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif
#endif

#if defined(ANDROID) || defined(LIBVNCSERVER_HAVE_ANDROID)
#include <arpa/inet.h>
#include <sys/select.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if LIBVNCSERVER_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <rfb/rfbproto.h>
#include <rfb/keysym.h>
#include <rfb/threading.h>

#ifdef LIBVNCSERVER_HAVE_SASL
#include <sasl/sasl.h>
#endif /* LIBVNCSERVER_HAVE_SASL */

#define rfbClientSwap16IfLE(s) \
    (*(char *)&client->endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define rfbClientSwap32IfLE(l) \
    (*(char *)&client->endianTest ? ((((l) >> 24) & 0x000000ff) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24))  : (l))

#define rfbClientSwap64IfLE(l) \
    (*(char *)&client->endianTest ? ((((l) >> 56 ) & 0x00000000000000ffULL) | \
			     (((l) & 0x00ff000000000000ULL) >> 40)  | \
			     (((l) & 0x0000ff0000000000ULL) >> 24)  | \
			     (((l) & 0x000000ff00000000ULL) >> 8)  | \
			     (((l) & 0x00000000ff000000ULL) << 8)  | \
			     (((l) & 0x0000000000ff0000ULL) << 24)  | \
			     (((l) & 0x000000000000ff00ULL) << 40)  | \
			     (((l) & 0x00000000000000ffULL) << 56))  : (l))

#define FLASH_PORT_OFFSET 5400
#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#define DEFAULT_CONNECT_TIMEOUT 60
#define DEFAULT_READ_TIMEOUT 0

#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")

#if(defined __cplusplus)
extern "C"
{
#endif

/** vncrec */

typedef struct {
  FILE* file;
  struct timeval tv;
  rfbBool readTimestamp;
  rfbBool doNotSleep;
} rfbVNCRec;

/** client data */

typedef struct rfbClientData {
	void* tag;
	void* data;
	struct rfbClientData* next;
} rfbClientData;

/** app data (belongs into rfbClient?) */

typedef struct {
  rfbBool shareDesktop;
  rfbBool viewOnly;

  const char* encodingsString;

  rfbBool useBGR233;
  int nColours;
  rfbBool forceOwnCmap;
  rfbBool forceTrueColour;
  int requestedDepth;

  int compressLevel;
  int qualityLevel;
  rfbBool enableJPEG;
  rfbBool useRemoteCursor;
  rfbBool palmVNC;  /**< use palmvnc specific SetScale (vs ultravnc) */
  int scaleSetting; /**< 0 means no scale set, else 1/scaleSetting */
} AppData;

/** For GetCredentialProc callback function to return */
typedef union _rfbCredential
{
  /** X509 (VeNCrypt) */
  struct
  {
    char *x509CACertFile;
    char *x509CACrlFile;
    char *x509ClientCertFile;
    char *x509ClientKeyFile;
    uint8_t x509CrlVerifyMode; /* Only required for OpenSSL - see meanings below */
  } x509Credential;
  /** Plain (VeNCrypt), MSLogon (UltraVNC) */
  struct
  {
    char *username;
    char *password;
  } userCredential;
} rfbCredential;

#define rfbCredentialTypeX509 1
#define rfbCredentialTypeUser 2

/* When using OpenSSL, CRLs can be included in both the x509CACrlFile and appended
   to the x509CACertFile as is common with OpenSSL.  When rfbX509CrlVerifyAll is
   specified the CRL list must include CRLs for all certificates in the chain */
#define rfbX509CrlVerifyNone   0    /* No CRL checking is performed */
#define rfbX509CrlVerifyClient 1    /* Only the leaf server certificate is checked */
#define rfbX509CrlVerifyAll    2    /* All certificates in the server chain are checked */

struct _rfbClient;

/**
 * Handles a text chat message. If your application should accept text messages
 * from the server, define a function with this prototype and set
 * client->HandleTextChat to a pointer to that function subsequent to your
 * rfbGetClient() call.
 * @param client The client which called the text chat handler
 * @param value  text length if text != NULL, or one of rfbTextChatOpen,
 * rfbTextChatClose, rfbTextChatFinished if text == NULL
 * @param text The text message from the server
 */
typedef void (*HandleTextChatProc)(struct _rfbClient* client, int value, char *text);
/**
 * Handles XVP server messages. If your application sends XVP messages to the
 * server, you'll want to handle the server's XVP_FAIL and XVP_INIT responses.
 * Define a function with this prototype and set client->HandleXvpMsg to a
 * pointer to that function subsequent to your rfbGetClient() call.
 * @param client The client which called the XVP message handler
 * @param version The highest XVP extension version that the server supports
 * @param opcode The opcode. 0 is XVP_FAIL, 1 is XVP_INIT
 */
typedef void (*HandleXvpMsgProc)(struct _rfbClient* client, uint8_t version, uint8_t opcode);
typedef void (*HandleKeyboardLedStateProc)(struct _rfbClient* client, int value, int pad);
typedef rfbBool (*HandleCursorPosProc)(struct _rfbClient* client, int x, int y);
typedef void (*SoftCursorLockAreaProc)(struct _rfbClient* client, int x, int y, int w, int h);
typedef void (*SoftCursorUnlockScreenProc)(struct _rfbClient* client);
/**
   Callback indicating that a rectangular area of the client's framebuffer was updated.
   As a server will usually send several rects per rfbFramebufferUpdate message, this
   callback is usually called multiple times per rfbFramebufferUpdate message.
   @param client The client whose framebuffer was (partially) updated
   @param x The x-coordinate of the upper left corner of the updated rectangle
   @param y The y-coordinate of the upper left corner of the updated rectangle
   @param w The width of the updated rectangle
   @param h The heigth of the updated rectangle
 */
typedef void (*GotFrameBufferUpdateProc)(struct _rfbClient* client, int x, int y, int w, int h);
/**
   Callback indicating that a client has completely processed an rfbFramebufferUpdate
   message sent by a server.
   This is called exactly once per each handled rfbFramebufferUpdate message.
   @param client The client which finished processing an rfbFramebufferUpdate
 */
typedef void (*FinishedFrameBufferUpdateProc)(struct _rfbClient* client);
typedef char* (*GetPasswordProc)(struct _rfbClient* client);
typedef rfbCredential* (*GetCredentialProc)(struct _rfbClient* client, int credentialType);
typedef rfbBool (*MallocFrameBufferProc)(struct _rfbClient* client);
typedef void (*GotXCutTextProc)(struct _rfbClient* client, const char *text, int textlen);
typedef void (*GotXCutTextUTF8Proc)(struct _rfbClient* client, const char* buffer, int buffer_len);
typedef void (*BellProc)(struct _rfbClient* client);
/**
    Called when a cursor shape update was received from the server. The decoded cursor shape
    will be in client->rcSource. It's up to the application to do something with this, e.g. draw
    into a viewer's window. If you want the server to draw the cursor into the framebuffer, be
    careful not to announce remote cursor support, i.e. not include rfbEncodingXCursor or
    rfbEncodingRichCursor in SetFormatAndEncodings().
*/
typedef void (*GotCursorShapeProc)(struct _rfbClient* client, int xhot, int yhot, int width, int height, int bytesPerPixel);
typedef void (*GotCopyRectProc)(struct _rfbClient* client, int src_x, int src_y, int w, int h, int dest_x, int dest_y);
typedef void (*GotFillRectProc)(struct _rfbClient* client, int x, int y, int w, int h, uint32_t colour);
typedef void (*GotBitmapProc)(struct _rfbClient* client, const uint8_t* buffer, int x, int y, int w, int h);
typedef rfbBool (*GotJpegProc)(struct _rfbClient* client, const uint8_t* buffer, int length, int x, int y, int w, int h);
typedef rfbBool (*LockWriteToTLSProc)(struct _rfbClient* client);   /** @deprecated */
typedef rfbBool (*UnlockWriteToTLSProc)(struct _rfbClient* client); /** @deprecated */

#ifdef LIBVNCSERVER_HAVE_SASL
typedef char* (*GetUserProc)(struct _rfbClient* client);
typedef char* (*GetSASLMechanismProc)(struct _rfbClient* client, char* mechlist);
#endif /* LIBVNCSERVER_HAVE_SASL */

typedef struct _rfbClient {
	uint8_t* frameBuffer;
	int width, height;

	int endianTest;

	AppData appData;

	const char* programName;
	char* serverHost;
	int serverPort; /**< if -1, then use file recorded by vncrec */
	rfbBool listenSpecified;
	int listenPort, flashPort;

	struct {
		int x, y, w, h;
	} updateRect;

	/** Note that the CoRRE encoding uses this buffer and assumes it is big enough
	   to hold 255 * 255 * 32 bits -> 260100 bytes.  640*480 = 307200 bytes.
	   Hextile also assumes it is big enough to hold 16 * 16 * 32 bits.
	   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define RFB_BUFFER_SIZE (640*480)
	char buffer[RFB_BUFFER_SIZE];

	/* rfbproto.c */

	rfbSocket sock;
	rfbBool canUseCoRRE;
	rfbBool canUseHextile;
	char *desktopName;
	rfbPixelFormat format;
	rfbServerInitMsg si;

	/* sockets.c */
#define RFB_BUF_SIZE 8192
	char buf[RFB_BUF_SIZE];
	char *bufoutptr;
	unsigned int buffered;

	/* The zlib encoding requires expansion/decompression/deflation of the
	   compressed data in the "buffer" above into another, result buffer.
	   However, the size of the result buffer can be determined precisely
	   based on the bitsPerPixel, height and width of the rectangle.  We
	   allocate this buffer one time to be the full size of the buffer. */

	/* Ultra Encoding uses this buffer too */
	
	int ultra_buffer_size;
	char *ultra_buffer;

	int raw_buffer_size;
	char *raw_buffer;

#ifdef LIBVNCSERVER_HAVE_LIBZ
	z_stream decompStream;
	rfbBool decompStreamInited;
#endif


#ifdef LIBVNCSERVER_HAVE_LIBZ
	/*
	 * Variables for the ``tight'' encoding implementation.
	 */

	/** Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 30000
	char zlib_buffer[ZLIB_BUFFER_SIZE];

	/* Four independent compression streams for zlib library. */
	z_stream zlibStream[4];
	rfbBool zlibStreamActive[4];

	/* Filter stuff. Should be initialized by filter initialization code. */
	rfbBool cutZeros;
	int rectWidth, rectColors;
	char tightPalette[256*4];
	uint8_t tightPrevRow[2048*3*sizeof(uint16_t)];

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	/** JPEG decoder state (obsolete-- do not use). */
	rfbBool jpegError;

	struct jpeg_source_mgr* jpegSrcManager;
	void* jpegBufferPtr;
	size_t jpegBufferLen;

#endif
#endif


	/* cursor.c */
	/** Holds cursor shape data when received from server. */
	uint8_t *rcSource, *rcMask;

	/** private data pointer */
	rfbClientData* clientData;

	rfbVNCRec* vncRec;

	/* Keyboard State support (is 'Caps Lock' set on the remote display???) */
	int KeyboardLedStateEnabled;
	int CurrentKeyboardLedState;

	int canHandleNewFBSize;

	/* hooks */
	HandleTextChatProc         HandleTextChat;
	HandleKeyboardLedStateProc HandleKeyboardLedState;
	HandleCursorPosProc HandleCursorPos;
	SoftCursorLockAreaProc SoftCursorLockArea;
	SoftCursorUnlockScreenProc SoftCursorUnlockScreen;
	GotFrameBufferUpdateProc GotFrameBufferUpdate;
	/** the pointer returned by GetPassword will be freed after use! */
	GetPasswordProc GetPassword;
	MallocFrameBufferProc MallocFrameBuffer;
	GotXCutTextProc GotXCutText;
	BellProc Bell;

	GotCursorShapeProc GotCursorShape;
	GotCopyRectProc GotCopyRect;

	/** Which messages are supported by the server
	 * This is a *guess* for most servers.
	 * (If we can even detect the type of server)
	 *
	 * If the server supports the "rfbEncodingSupportedMessages"
	 * then this will be updated when the encoding is received to
	 * accurately reflect the servers capabilities.
	 */
	rfbSupportedMessages supportedMessages;

	/** negotiated protocol version */
	int major, minor;

	/** The selected security types */
	uint32_t authScheme, subAuthScheme;

	/** The TLS session for Anonymous TLS and VeNCrypt */
	void* tlsSession;

	/** To support security types that requires user input (except VNC password
	 * authentication), for example VeNCrypt and MSLogon, this callback function
	 * must be set before the authentication. Otherwise, it implicates that the
	 * caller application does not support it and related security types should
	 * be bypassed.
	 */
	GetCredentialProc GetCredential;

	/** The 0-terminated security types supported by the client.
	 * Set by function SetClientAuthSchemes() */
	uint32_t *clientAuthSchemes;

	/** When the server is a repeater, this specifies the final destination */
	char *destHost;
	int destPort;

        /** the QoS IP DSCP for this client */
        int QoS_DSCP;

        /** hook to handle xvp server messages */
	HandleXvpMsgProc           HandleXvpMsg;

	/* listen.c */
        rfbSocket listenSock;

	FinishedFrameBufferUpdateProc FinishedFrameBufferUpdate;

	char *listenAddress;
        /* IPv6 listen socket, address and port*/
        rfbSocket listen6Sock;
        char* listen6Address;
        int listen6Port;

        /* Output Window ID. When set, client application enables libvncclient to perform direct rendering in its window */
        unsigned long outputWindow;

	/** 
	 * These lock/unlock hooks are not used anymore. LibVNCClient will now use 
	 * platform-specific synchronization library to protect concurrent TLS R/W.
	 *  
	 * @deprecated
	 */
	LockWriteToTLSProc LockWriteToTLS;
	UnlockWriteToTLSProc UnlockWriteToTLS;

        /** Hooks for custom rendering
         *
         * VNC rendering boils down to 3 activities:
         * - GotCopyRect: copy an area of the framebuffer
         * - GotFillRect: fill an area of the framebuffer with a solid color
         * - GotBitmap: copy the bitmap in the buffer into the framebuffer
         * The client application should either set all three of these or none!
         */
        GotFillRectProc GotFillRect;
        GotBitmapProc GotBitmap;
        /** Hook for custom JPEG decoding and rendering */
        GotJpegProc GotJpeg;

#ifdef LIBVNCSERVER_HAVE_SASL
        sasl_conn_t *saslconn;
        const char *saslDecoded;
        unsigned int saslDecodedLength;
        unsigned int saslDecodedOffset;
        sasl_secret_t *saslSecret;

        /* Callback to allow the client to choose a preferred mechanism. The string returned will
           be freed once no longer required. */
        GetSASLMechanismProc GetSASLMechanism;
        GetUserProc GetUser;

#endif /* LIBVNCSERVER_HAVE_SASL */

#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
	/** JPEG decoder state. */
	void *tjhnd;

#endif
#endif
	/* timeout in seconds for select() after connect() */
	unsigned int connectTimeout;

	/* timeout in seconds when reading from half-open connections in
	 * ReadFromRFBServer() - keep at 0 to disable timeout detection and handling */
	unsigned int readTimeout;

	/**
	 * Mutex to protect concurrent TLS read/write.
	 * For internal use only.
	 */
	MUTEX(tlsRwMutex);

	rfbBool requestedResize;
        /**
         * Used for intended dimensions, rfbClient.width and rfbClient.height are used to manage the real framebuffer dimensions.
	 */
	rfbExtDesktopScreen screen;

#ifdef LIBVNCSERVER_HAVE_LIBZ
	uint32_t extendedClipboardServerCapabilities;
#endif
	/**
 * Callback fired when "Extended Clipboard" UTF-8 text data is received.
 */
	GotXCutTextUTF8Proc GotXCutTextUTF8;


	rfbBool automaticUpdateRequests;
	int serverMajor;
	int serverMinor;


} rfbClient;

/* cursor.c */
/**
 * Handles XCursor and RichCursor shape updates from the server.
 * We emulate cursor operating on the frame buffer (that is
 * why we call it "software cursor"). This decodes the received cursor
 * shape and hands it over to GotCursorShapeProc, if set.
 */
extern rfbBool HandleCursorShape(rfbClient* client,int xhot, int yhot, int width, int height, uint32_t enc);

/* listen.c */

extern void listenForIncomingConnections(rfbClient* viewer);
extern int listenForIncomingConnectionsNoFork(rfbClient* viewer, int usec_timeout);

/* rfbproto.c */

extern rfbBool rfbEnableClientLogging;
typedef void (*rfbClientLogProc)(const char *format, ...);
extern rfbClientLogProc rfbClientLog,rfbClientErr;
extern rfbBool ConnectToRFBServer(rfbClient* client,const char *hostname, int port);
extern rfbBool ConnectToRFBRepeater(rfbClient* client,const char *repeaterHost, int repeaterPort, const char *destHost, int destPort);
extern void SetClientAuthSchemes(rfbClient* client,const uint32_t *authSchemes, int size);
extern rfbBool InitialiseRFBConnection(rfbClient* client);
/**
 * Sends format and encoding parameters to the server. Your application can
 * modify the 'client' data structure directly. However some changes to this
 * structure must be communicated back to the server. For instance, if you
 * change the encoding to hextile, the server needs to know that it should send
 * framebuffer updates in hextile format. Likewise if you change the pixel
 * format of the framebuffer, the server must be notified about this as well.
 * Call this function to propagate your changes of the local 'client' structure
 * over to the server.
 * @li Encoding type
 * @li RFB protocol extensions announced via pseudo-encodings
 * @li Framebuffer pixel format (like RGB vs ARGB)
 * @li Remote cursor support
 * @param client The client in which the format or encodings have been changed
 * @return true if the format or encodings were sent to the server successfully,
 * false otherwise
 */
extern rfbBool SetFormatAndEncodings(rfbClient* client);
extern rfbBool SendIncrementalFramebufferUpdateRequest(rfbClient* client);
/**
 * Sends a framebuffer update request to the server. A VNC client may request an
 * update from the server at any time. You can also specify which portions of
 * the screen you want updated. This can be handy if a pointer is at certain
 * location and the user pressed a mouse button, for instance. Then you can
 * immediately request an update of the region around the pointer from the
 * server.
 * @note The coordinate system is a left-handed Cartesian coordinate system with
 * the Z axis (unused) pointing out of the screen. Alternately you can think of
 * it as a right-handed Cartesian coordinate system with the Z axis pointing
 * into the screen. The origin is at the upper left corner of the framebuffer.
 * @param client The client through which to send the request
 * @param x The horizontal position of the update request rectangle
 * @param y The vertical position of the update request rectangle
 * @param w The width of the update request rectangle
 * @param h The height of the update request rectangle
 * @param incremental false: server sends rectangle even if nothing changed.
 * true: server only sends changed parts of rectangle.
 * @return true if the update request was sent successfully, false otherwise
 */
extern rfbBool SendFramebufferUpdateRequest(rfbClient* client,
					 int x, int y, int w, int h,
					 rfbBool incremental);
extern rfbBool SendScaleSetting(rfbClient* client,int scaleSetting);
/**
 * Sends a pointer event to the server. A pointer event includes a cursor
 * location and a button mask. The button mask indicates which buttons on the
 * pointing device are pressed. Each button is represented by a bit in the
 * button mask. A 1 indicates the button is pressed while a 0 indicates that it
 * is not pressed. You may use these pre-defined button masks by ORing them
 * together: rfbButton1Mask, rfbButton2Mask, rfbButton3Mask, rfbButton4Mask
 * rfbButton5Mask
 * @note  The cursor location is relative to the client's framebuffer, not the
 * client's screen itself.
 * @note The coordinate system is a left-handed Cartesian coordinate system with
 * the Z axis (unused) pointing out of the screen. Alternately you can think of
 * it as a right-handed Cartesian coordinate system with the Z axis pointing
 * into the screen. The origin is at the upper left corner of the screen.
 * @param client The client through which to send the pointer event
 * @param x the horizontal location of the cursor
 * @param y the vertical location of the cursor
 * @param buttonMask the button mask indicating which buttons are pressed
 * @return true if the pointer event was sent successfully, false otherwise
 */
extern rfbBool SendPointerEvent(rfbClient* client,int x, int y, int buttonMask);
/**
 * Sends a SetDesktopSize event to the server.
 * @param client The client through which to send the SetDesktopSize event
 * @param width The width of the update request rectangle
 * @param height The height of the update request rectangle
 * @return true if the SetDesktopSize event was send successfully, false otherwise
 */
extern rfbBool SendExtDesktopSize(rfbClient* client, uint16_t width, uint16_t height);
/**
 * Sends a key event to the server. If your application is not merely a VNC
 * viewer (i.e. it controls the server), you'll want to send the keys that the
 * user presses to the server. Use this function to do that.
 * @param client The client through which to send the key event
 * @param key An rfbKeySym defined in rfb/keysym.h
 * @param down true if this was a key down event, false otherwise
 * @return true if the key event was send successfully, false otherwise
 */
extern rfbBool SendKeyEvent(rfbClient* client,uint32_t key, rfbBool down);
/**
 * The same as SendKeyEvent, except a key code will be sent along with the
 * symbol if the server supports extended key events.
 * @param client The client through which to send the key event
 * @param keysym An rfbKeySym defined in rfb/keysym.h
 * @param keycode An XT key code
 * @param down true if this was a key down event, false otherwise
 * @return true if the extended key event is supported and was sent
 * successfully, false otherwise
 */
extern rfbBool SendExtendedKeyEvent(rfbClient* client, uint32_t keysym, uint32_t keycode, rfbBool down);
/**
 * Places a Latin-1-encoded string on the server's clipboard. Use this function if you want to
 * be able to copy and paste between the server and your application. For
 * instance, when your application is notified that the user copied some text
 * onto the clipboard, you would call this function to synchronize the server's
 * clipboard with your local clipboard.
 * @param client The client structure through which to send the client cut text
 * message
 * @param str The string to send (needs to be Latin-1-encoded, doesn't need to be NULL terminated)
 * @param len The length of the string
 * @return true if the client cut message was sent successfully, false otherwise
 */
extern rfbBool SendClientCutText(rfbClient* client,char *str, int len);
/**
 * Places a UTF-8-encoded string on the server's clipboard if the server supports it.
 * Use this function if you want to be able to copy and paste between the server and your application. For
 * instance, when your application is notified that the user copied some text
 * onto the clipboard, you would call this function to synchronize the server's
 * clipboard with your local clipboard. This is the more modern equivalent of
 * SendClientCutText().
 * @param client The client structure through which to send the client cut text
 * message
 * @param str The string to send (needs to be UTF-8-encoded, doesn't need to be NULL terminated)
 * @param len The length of the string in bytes
 * @return true if the message was sent successfully, false otherwise - you might want to
 * fall back to SendClientCutText() in this case.
 */
extern rfbBool SendClientCutTextUTF8(rfbClient* client, char *str, int len);
/**
 * Handles messages from the RFB server. You must call this function
 * intermittently so LibVNCClient can parse messages from the server. For
 * example, if your app has a draw loop, you could place a call to this
 * function within that draw loop.
 * @note You must call WaitForMessage() before you call this function.
 * @param client The client which will handle the RFB server messages
 * @return true if the client was able to handle the RFB server messages, false
 * otherwise
 */
extern rfbBool HandleRFBServerMessage(rfbClient* client);

/**
 * Sends a text chat message to the server.
 * @param client The client through which to send the message
 * @param text The text to send
 * @return true if the text was sent successfully, false otherwise
 */
extern rfbBool TextChatSend(rfbClient* client, char *text);
/**
 * Opens a text chat window on the server.
 * @param client The client through which to send the message
 * @return true if the window was opened successfully, false otherwise
 */
extern rfbBool TextChatOpen(rfbClient* client);
/**
 * Closes the text chat window on the server.
 * @param client The client through which to send the message
 * @return true if the window was closed successfully, false otherwise
 */
extern rfbBool TextChatClose(rfbClient* client);
extern rfbBool TextChatFinish(rfbClient* client);
extern rfbBool PermitServerInput(rfbClient* client, int enabled);
extern rfbBool SendXvpMsg(rfbClient* client, uint8_t version, uint8_t code);

extern void PrintPixelFormat(rfbPixelFormat *format);

extern rfbBool SupportsClient2Server(rfbClient* client, int messageType);
extern rfbBool SupportsServer2Client(rfbClient* client, int messageType);

/* client data */

/**
 * Associates a client data tag with the given pointer. LibVNCClient has
 * several events to which you can associate your own handlers. These handlers
 * have the client structure as one of their parameters. Sometimes, you may want
 * to make data from elsewhere in your application available to these handlers
 * without using a global variable. To do this, you call
 * rfbClientSetClientData() and associate the data with a tag. Then, your
 * handler can call rfbClientGetClientData() and get the a pointer to the data
 * associated with that tag.
 * @param client The client in which to set the client data
 * @param tag A unique tag which identifies the data
 * @param data A pointer to the data to associate with the tag
 */
void rfbClientSetClientData(rfbClient* client, void* tag, void* data);
/**
 * Returns a pointer to the client data associated with the given tag. See the
 * the documentation for rfbClientSetClientData() for a discussion of how you
 * can use client data.
 * @param client The client from which to get the client data
 * @param tag The tag which identifies the client data
 * @return a pointer to the client data
 */
void* rfbClientGetClientData(rfbClient* client, void* tag);

/* protocol extensions */

typedef struct _rfbClientProtocolExtension {
	int* encodings;
	/** returns TRUE if the encoding was handled */
	rfbBool (*handleEncoding)(rfbClient* cl,
		rfbFramebufferUpdateRectHeader* rect);
	/** returns TRUE if it handled the message */
	rfbBool (*handleMessage)(rfbClient* cl,
		 rfbServerToClientMsg* message);
	struct _rfbClientProtocolExtension* next;
	uint32_t const* securityTypes;
	/** returns TRUE if it handled the authentication */
	rfbBool (*handleAuthentication)(rfbClient* cl, uint32_t authScheme);
} rfbClientProtocolExtension;

void rfbClientRegisterExtension(rfbClientProtocolExtension* e);

/* sockets.c */

extern rfbBool errorMessageOnReadFailure;

extern rfbBool ReadFromRFBServer(rfbClient* client, char *out, unsigned int n);
extern rfbBool WriteToRFBServer(rfbClient* client, const char *buf, unsigned int n);
extern int FindFreeTcpPort(void);
extern rfbSocket ListenAtTcpPort(int port);
extern rfbSocket ListenAtTcpPortAndAddress(int port, const char *address);
/**
   Tries to connect to an IPv4 host.
   @param host Binary IPv4 address
   @param port Port
   @return A blocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToTcpAddr(unsigned int host, int port);
/**
   Tries to connect to an IPv4 or IPv6 host.
   @param hostname A hostname or IP address
   @param port Port
   @return A blocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToTcpAddr6(const char *hostname, int port);
/**
   Tries to connect to a Unix socket.
   @param sockFile Path of the socket file
   @return A blocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToUnixSock(const char *sockFile);
/**
   Tries to connect to an IPv4 host using the given timeout value.
   @param host Binary IPv4 address
   @param port Port
   @param timeout The time in seconds to wait for a connection
   @return A nonblocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToTcpAddrWithTimeout(unsigned int host, int port, unsigned int timeout);
/**
   Tries to connect to an IPv4 or IPv6 host using the given timeout value.
   @param hostname A hostname or IP address
   @param port Port
   @param timeout The time in seconds to wait for a connection
   @return A nonblocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToTcpAddr6WithTimeout(const char *hostname, int port, unsigned int timeout);
/**
   Tries to connect to a Unix socket using the given timeout value.
   @param sockFile Path of the socket file
   @param timeout The time in seconds to wait for a connection
   @return A nonblocking socket or RFB_INVALID_SOCKET if the connection failed
*/
extern rfbSocket ConnectClientToUnixSockWithTimeout(const char *sockFile, unsigned int timeout);
extern rfbSocket AcceptTcpConnection(rfbSocket listenSock);
extern rfbBool SetNonBlocking(rfbSocket sock);
extern rfbBool SetBlocking(rfbSocket sock);
extern rfbBool SetDSCP(rfbSocket sock, int dscp);

extern rfbBool StringToIPAddr(const char *str, unsigned int *addr);
extern rfbBool SameMachine(rfbSocket sock);
/**
 * Waits for an RFB message to arrive from the server. Before handling a message
 * with HandleRFBServerMessage(), you must wait for your client to receive one.
 * This function blocks until a message is received. You may specify a timeout
 * in microseconds. Once this number of microseconds have elapsed, the function
 * will return.
 * @param client The client to cause to wait until a message is received
 * @param usecs The timeout in microseconds
 * @return the return value of the underlying select() call
 */
extern int WaitForMessage(rfbClient* client,unsigned int usecs);

/* vncviewer.c */
/**
 * Allocates and returns a pointer to an rfbClient structure. This will probably
 * be the first LibVNCClient function your client code calls. Most libVNCClient
 * functions operate on an rfbClient structure, and this function allocates
 * memory for that structure. When you're done with the rfbClient structure
 * pointer this function returns, you should free the memory rfbGetClient()
 * allocated by calling rfbClientCleanup().
 *
 * A pixel is one dot on the screen. The number of bytes in a pixel will depend
 * on the number of samples in that pixel and the number of bits in each sample.
 * A sample represents one of the primary colors in a color model. The RGB
 * color model uses red, green, and blue samples respectively. Suppose you
 * wanted to use 16-bit RGB color: You would have three samples per pixel (one
 * for each primary color), five bits per sample (the quotient of 16 RGB bits
 * divided by three samples), and two bytes per pixel (the smallest multiple of
 * eight bits in which the 16-bit pixel will fit). If you wanted 32-bit RGB
 * color, you would have three samples per pixel again, eight bits per sample
 * (since that's how 32-bit color is defined), and four bytes per pixel (the
 * smallest multiple of eight bits in which the 32-bit pixel will fit.
 * @param bitsPerSample The number of bits in a sample
 * @param samplesPerPixel The number of samples in a pixel
 * @param bytesPerPixel The number of bytes in a pixel
 * @return a pointer to the allocated rfbClient structure
 */
rfbClient* rfbGetClient(int bitsPerSample,int samplesPerPixel,int bytesPerPixel);
/**
 * Initializes the client. The format is {PROGRAM_NAME, [OPTIONS]..., HOST}. This
 * function does not initialize the program name if the rfbClient's program
 * name is set already. The options are as follows:
 * <table>
 * <tr><th>Option</th><th>Description</th></tr>
 * <tr><td>-listen</td><td>Listen for incoming connections.</td></tr>
 * <tr><td>-listennofork</td><td>Listen for incoming connections without forking.
 * </td></tr>
 * <tr><td>-play</td><td>Set this client to replay a previously recorded session.</td></tr>
 * <tr><td>-encodings</td><td>Set the encodings to use. The next item in the
 * argv array is the encodings string, consisting of comma separated encodings like 'tight,ultra,raw'.</td></tr>
 * <tr><td>-compress</td><td>Set the compression level. The next item in the
 * argv array is the compression level as an integer. Ranges from 0 (lowest) to 9 (highest).
 * </td></tr>
 * <tr><td>-scale</td><td>Set the scaling level. The next item in the
 * argv array is the scaling level as an integer. The screen will be scaled down by this factor.</td></tr>
 * <tr><td>-qosdscp</td><td>Set the Quality of Service Differentiated Services
 * Code Point (QoS DSCP). The next item in the argv array is the code point as
 * an integer.</td></tr>
 * <tr><td>-repeaterdest</td><td>Set a VNC repeater address. The next item in the argv array is
 * the repeater's address as a string.</td></tr>
 * </table>
 *
 * The host may include a port number (delimited by a ':').
 * @param client The client to initialize
 * @param argc The number of arguments to the initializer
 * @param argv The arguments to the initializer as an array of NULL terminated
 * strings
 * @return true if the client was initialized successfully, false otherwise.
 */
rfbBool rfbInitClient(rfbClient* client,int* argc,char** argv);
/**
 * Cleans up the client structure and releases the memory allocated for it. You
 * should call this when you're done with the rfbClient structure that you
 * allocated with rfbGetClient().
 * @note rfbClientCleanup() does not touch client->frameBuffer.
 * @param client The client to clean up
 */
void rfbClientCleanup(rfbClient* client);

#if(defined __cplusplus)
}
#endif

/**
 * @}
 */

/**
 @page libvncclient_doc LibVNCClient Documentation
 @section example_code Example Code
 See SDLvncviewer.c for a rather complete client example.
*/

#endif
