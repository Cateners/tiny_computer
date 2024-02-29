#ifndef RFB_H
#define RFB_H
/**
 * @defgroup libvncserver_api LibVNCServer API Reference
 * @{
 */

/**
 * @file rfb.h
 */

/*
 *  Copyright (C) 2005 Rohit Kumar <rokumar@novell.com>,
 *                     Johannes E. Schindelin <johannes.schindelin@gmx.de>
 *  Copyright (C) 2002 RealVNC Ltd.
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
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

#if(defined __cplusplus)
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rfb/rfbproto.h>

#if defined(ANDROID) || defined(LIBVNCSERVER_HAVE_ANDROID)
#include <arpa/inet.h>
#include <sys/select.h>
#endif

#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef WIN32
typedef UINT32 in_addr_t;
#include <winsock2.h>
#ifdef LIBVNCSERVER_HAVE_WS2TCPIP_H
#undef socklen_t
#include <ws2tcpip.h>
#endif
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif
#endif

#include <rfb/threading.h>

/* if you use pthreads, but don't define LIBVNCSERVER_HAVE_LIBPTHREAD, the structs
   get all mixed up. So this gives a linker error reminding you to compile
   the library and your application (at least the parts including rfb.h)
   with the same support for pthreads. */
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
#ifdef LIBVNCSERVER_HAVE_LIBZ
#define rfbInitServer rfbInitServerWithPthreadsAndZRLE
#else
#define rfbInitServer rfbInitServerWithPthreadsButWithoutZRLE
#endif
#else
#ifdef LIBVNCSERVER_HAVE_LIBZ
#define rfbInitServer rfbInitServerWithoutPthreadsButWithZRLE
#else
#define rfbInitServer rfbInitServerWithoutPthreadsAndZRLE
#endif
#endif

struct _rfbClientRec;
struct _rfbScreenInfo;
struct rfbCursor;

enum rfbNewClientAction {
	RFB_CLIENT_ACCEPT,
	RFB_CLIENT_ON_HOLD,
	RFB_CLIENT_REFUSE
};

enum rfbSocketState {
	RFB_SOCKET_INIT,
	RFB_SOCKET_READY,
	RFB_SOCKET_SHUTDOWN
};

typedef void (*rfbKbdAddEventProcPtr) (rfbBool down, rfbKeySym keySym, struct _rfbClientRec* cl);
typedef void (*rfbKbdReleaseAllKeysProcPtr) (struct _rfbClientRec* cl);
typedef void (*rfbPtrAddEventProcPtr) (int buttonMask, int x, int y, struct _rfbClientRec* cl);
typedef void (*rfbSetXCutTextProcPtr) (char* str,int len, struct _rfbClientRec* cl);
#ifdef LIBVNCSERVER_HAVE_LIBZ
typedef void (*rfbSetXCutTextUTF8ProcPtr) (char* str,int len, struct _rfbClientRec* cl);
#endif
typedef struct rfbCursor* (*rfbGetCursorProcPtr) (struct _rfbClientRec* pScreen);
typedef rfbBool (*rfbSetTranslateFunctionProcPtr)(struct _rfbClientRec* cl);
typedef rfbBool (*rfbPasswordCheckProcPtr)(struct _rfbClientRec* cl,const char* encryptedPassWord,int len);
typedef enum rfbNewClientAction (*rfbNewClientHookPtr)(struct _rfbClientRec* cl);
typedef void (*rfbDisplayHookPtr)(struct _rfbClientRec* cl);
typedef void (*rfbDisplayFinishedHookPtr)(struct _rfbClientRec* cl, int result);
/** support the capability to view the caps/num/scroll states of the X server */
typedef int  (*rfbGetKeyboardLedStateHookPtr)(struct _rfbScreenInfo* screen);
typedef rfbBool (*rfbXvpHookPtr)(struct _rfbClientRec* cl, uint8_t, uint8_t);
typedef int (*rfbSetDesktopSizeHookPtr)(int width, int height, int numScreens, struct rfbExtDesktopScreen* extDesktopScreens, struct _rfbClientRec* cl);
typedef int (*rfbNumberOfExtDesktopScreensPtr)(struct _rfbClientRec* cl);
typedef rfbBool (*rfbGetExtDesktopScreenPtr)(int seqnumber, struct rfbExtDesktopScreen *extDesktopScreen, struct _rfbClientRec* cl);
/**
 * If x==1 and y==1 then set the whole display
 * else find the window underneath x and y and set the framebuffer to the dimensions
 * of that window
 */
typedef void (*rfbSetSingleWindowProcPtr) (struct _rfbClientRec* cl, int x, int y);
/**
 * Status determines if the X11 server permits input from the local user
 * status==0 or 1
 */
typedef void (*rfbSetServerInputProcPtr) (struct _rfbClientRec* cl, int status);
/**
 * Permit the server to allow or deny filetransfers.   This is defaulted to deny
 * It is called when a client initiates a connection to determine if it is permitted.
 */
typedef int  (*rfbFileTransferPermitted) (struct _rfbClientRec* cl);
/** Handle the textchat messages */
typedef void (*rfbSetTextChat) (struct _rfbClientRec* cl, int length, char *string);

typedef struct {
  uint32_t count;
  rfbBool is16; /**< is the data format short? */
  union {
    uint8_t* bytes;
    uint16_t* shorts;
  } data; /**< there have to be count*3 entries */
} rfbColourMap;

/**
 * Security handling (RFB protocol version 3.7)
 */

typedef struct _rfbSecurity {
	uint8_t type;
	void (*handler)(struct _rfbClientRec* cl);
	struct _rfbSecurity* next;
} rfbSecurityHandler;

/**
 * Protocol extension handling.
 */

typedef struct _rfbProtocolExtension {
	/** returns FALSE if extension should be deactivated for client.
	   if newClient == NULL, it is always deactivated. */
	rfbBool (*newClient)(struct _rfbClientRec* client, void** data);
	/** returns FALSE if extension should be deactivated for client.
	   if init == NULL, it stays activated. */
	rfbBool (*init)(struct _rfbClientRec* client, void* data);
	/** if pseudoEncodings is not NULL, it contains a 0 terminated
	   list of the pseudo encodings handled by this extension. */
	int *pseudoEncodings;
	/** returns TRUE if that pseudo encoding is handled by the extension.
	   encodingNumber==0 means "reset encodings". */
	rfbBool (*enablePseudoEncoding)(struct _rfbClientRec* client,
			void** data, int encodingNumber);
	/** returns TRUE if message was handled */
	rfbBool (*handleMessage)(struct _rfbClientRec* client,
				void* data,
				const rfbClientToServerMsg* message);
	void (*close)(struct _rfbClientRec* client, void* data);
	void (*usage)(void);
	/** processArguments returns the number of handled arguments */
	int (*processArgument)(int argc, char *argv[]);
	struct _rfbProtocolExtension* next;
} rfbProtocolExtension;

typedef struct _rfbExtensionData {
	rfbProtocolExtension* extension;
	void* data;
	struct _rfbExtensionData* next;
} rfbExtensionData;

/**
 * Per-screen (framebuffer) structure.  There can be as many as you wish,
 * each serving different clients. However, you have to call
 * rfbProcessEvents for each of these.
 */

typedef struct _rfbScreenInfo
{
    /** this structure has children that are scaled versions of this screen */
    struct _rfbScreenInfo *scaledScreenNext;
    int scaledScreenRefCount;

    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;

    rfbPixel blackPixel;
    rfbPixel whitePixel;

    /**
     * some screen specific data can be put into a struct where screenData
     * points to. You need this if you have more than one screen at the
     * same time while using the same functions.
     */
    void* screenData;

    /* additions by libvncserver */

    rfbPixelFormat serverFormat;
    rfbColourMap colourMap; /**< set this if rfbServerFormat.trueColour==FALSE */
    const char* desktopName;
    char thisHost[255];

    rfbBool autoPort;
    int port;
    rfbSocket listenSock;
    int maxSock;
    int maxFd;
#ifdef WIN32
    struct fd_set allFds;
#else
    fd_set allFds;
#endif

    enum rfbSocketState socketState;
    rfbSocket inetdSock;
    rfbBool inetdInitDone;

    int udpPort;
    rfbSocket udpSock;
    struct _rfbClientRec* udpClient;
    rfbBool udpSockConnected;
    struct sockaddr_in udpRemoteAddr;

    int maxClientWait;

    /* http stuff */
    rfbBool httpInitDone;
    rfbBool httpEnableProxyConnect;
    int httpPort;
    char* httpDir;
    rfbSocket httpListenSock;
    rfbSocket httpSock;

    rfbPasswordCheckProcPtr passwordCheck;
    void* authPasswdData;
    /** If rfbAuthPasswdData is given a list, this is the first
        view only password. */
    int authPasswdFirstViewOnly;

    /** send only this many rectangles in one update */
    int maxRectsPerUpdate;
    /** this is the amount of milliseconds to wait at least before sending
     * an update. */
    int deferUpdateTime;
#ifdef TODELETE
    char* screen;
#endif
    rfbBool alwaysShared;
    rfbBool neverShared;
    rfbBool dontDisconnect;
    struct _rfbClientRec* clientHead;
    struct _rfbClientRec* pointerClient;  /**< "Mutex" for pointer events */


    /* cursor */
    int cursorX, cursorY,underCursorBufferLen;
    char* underCursorBuffer;
    rfbBool dontConvertRichCursorToXCursor;
    struct rfbCursor* cursor;

    /**
     * the frameBuffer has to be supplied by the serving process.
     * The buffer will not be freed by
     */
    char* frameBuffer;
    rfbKbdAddEventProcPtr kbdAddEvent;
    rfbKbdReleaseAllKeysProcPtr kbdReleaseAllKeys;
    rfbPtrAddEventProcPtr ptrAddEvent;
    rfbSetXCutTextProcPtr setXCutText;
    rfbGetCursorProcPtr getCursorPtr;
    rfbSetTranslateFunctionProcPtr setTranslateFunction;
    rfbSetSingleWindowProcPtr setSingleWindow;
    rfbSetServerInputProcPtr  setServerInput;
    rfbFileTransferPermitted  getFileTransferPermission;
    rfbSetTextChat            setTextChat;

    /** newClientHook is called just after a new client is created */
    rfbNewClientHookPtr newClientHook;
    /** displayHook is called just before a frame buffer update */
    rfbDisplayHookPtr displayHook;

    /** These hooks are called to pass keyboard state back to the client */
    rfbGetKeyboardLedStateHookPtr getKeyboardLedStateHook;

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    MUTEX(cursorMutex);
    rfbBool backgroundLoop;
#endif

    /** if TRUE, an ignoring signal handler is installed for SIGPIPE */
    rfbBool ignoreSIGPIPE;

    /** if not zero, only a slice of this height is processed every time
     * an update should be sent. This should make working on a slow
     * link more interactive. */
    int progressiveSliceHeight;

    in_addr_t listenInterface;
    int deferPtrUpdateTime;

    /** handle as many input events as possible (default off) */
    rfbBool handleEventsEagerly;

    /** rfbEncodingServerIdentity */
    char *versionString;

    /** What does the server tell the new clients which version it supports */
    int protocolMajorVersion;
    int protocolMinorVersion;

    /** command line authorization of file transfers */
    rfbBool permitFileTransfer;

    /** displayFinishedHook is called just after a frame buffer update */
    rfbDisplayFinishedHookPtr displayFinishedHook;
    /** xvpHook is called to handle an xvp client message */
    rfbXvpHookPtr xvpHook;
    char *sslkeyfile;
    char *sslcertfile;
    int ipv6port; /**< The port to listen on when using IPv6.  */
    char* listen6Interface;
    /* We have an additional IPv6 listen socket since there are systems that
       don't support dual binding sockets under *any* circumstances, for
       instance OpenBSD */
    rfbSocket listen6Sock;
    int http6Port;
    rfbSocket httpListen6Sock;
    /** hook to let client set resolution */
    rfbSetDesktopSizeHookPtr setDesktopSizeHook;
    /** Optional hooks to query ExtendedDesktopSize screen information.
     * If not set it is assumed only one screen is present spanning entire fb */
    rfbNumberOfExtDesktopScreensPtr numberOfExtDesktopScreensHook;
    rfbGetExtDesktopScreenPtr getExtDesktopScreenHook;
    /** This value between 0 and 1.0 defines which fraction of the maximum number
	of file descriptors LibVNCServer uses before denying new client connections.
	It is set to 0.5 per default. */
    float fdQuota;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    pthread_t listener_thread;
    int pipe_notify_listener_thread[2];
#elif defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    uintptr_t listener_thread;
#endif
#ifdef LIBVNCSERVER_HAVE_LIBZ
    rfbSetXCutTextUTF8ProcPtr setXCutTextUTF8;
#endif
} rfbScreenInfo, *rfbScreenInfoPtr;


/**
 * rfbTranslateFnType is the type of translation functions.
 */

typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr, char *optr,
                                   int bytesBetweenInputLines,
                                   int width, int height);


/* region stuff */

struct sraRegion;
typedef struct sraRegion* sraRegionPtr;

/*
 * Per-client structure.
 */

typedef void (*ClientGoneHookPtr)(struct _rfbClientRec* cl);
typedef void (*ClientFramebufferUpdateRequestHookPtr)(struct _rfbClientRec* cl, rfbFramebufferUpdateRequestMsg* furMsg);

typedef struct _rfbFileTransferData {
  int fd;
  int compressionEnabled;
  int fileSize;
  int numPackets;
  int receiving;
  int sending;
} rfbFileTransferData;


typedef struct _rfbStatList {
    uint32_t type;
    uint32_t sentCount;
    uint32_t bytesSent;
    uint32_t bytesSentIfRaw;
    uint32_t rcvdCount;
    uint32_t bytesRcvd;
    uint32_t bytesRcvdIfRaw;
    struct _rfbStatList *Next;
} rfbStatList;

typedef struct _rfbSslCtx rfbSslCtx;
typedef struct _wsCtx wsCtx;

typedef struct _rfbClientRec {

    /** back pointer to the screen */
    rfbScreenInfoPtr screen;

     /** points to a scaled version of the screen buffer in cl->scaledScreenList */
     rfbScreenInfoPtr scaledScreen;
     /** how did the client tell us it wanted the screen changed?  Ultra style or palm style? */
     rfbBool PalmVNC;


    /** private data. You should put any application client specific data
     * into a struct and let clientData point to it. Don't forget to
     * free the struct via clientGoneHook!
     *
     * This is useful if the IO functions have to behave client specific.
     */
    void* clientData;
    ClientGoneHookPtr clientGoneHook;

    rfbSocket sock;
    char *host;

    /* RFB protocol minor version number */
    int protocolMajorVersion;
    int protocolMinorVersion;

#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    pthread_t client_thread;
#elif defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    uintptr_t client_thread;
#endif

    /* Note that the RFB_INITIALISATION_SHARED state is provided to support
       clients that under some circumstances do not send a ClientInit message.
       In particular the Mac OS X built-in VNC client (with protocolMinorVersion
       == 889) is one of those.  However, it only requires this support under
       special circumstances that can only be determined during the initial
       authentication.  If the right conditions are met this state will be
       set (see the auth.c file) when rfbProcessClientInitMessage is called.

       If the state is RFB_INITIALISATION_SHARED we should not expect to receive
       any ClientInit message, but instead should proceed to the next stage
       of initialisation as though an implicit ClientInit message was received
       with a shared-flag of true.  (There is currently no corresponding
       RFB_INITIALISATION_NOTSHARED state to represent an implicit ClientInit
       message with a shared-flag of false because no known existing client
       requires such support at this time.)

       Note that software using LibVNCServer to provide a VNC server will only
       ever have a chance to see the state field set to
       RFB_INITIALISATION_SHARED if the software is multi-threaded and manages
       to examine the state field during the extremely brief window after the
       'None' authentication type selection has been received from the built-in
       OS X VNC client and before the rfbProcessClientInitMessage function is
       called -- control cannot return to the caller during this brief window
       while the state field is set to RFB_INITIALISATION_SHARED. */

                                /** Possible client states: */
    enum {
        RFB_PROTOCOL_VERSION,   /**< establishing protocol version */
	RFB_SECURITY_TYPE,      /**< negotiating security (RFB v.3.7) */
        RFB_AUTHENTICATION,     /**< authenticating */
        RFB_INITIALISATION,     /**< sending initialisation messages */
        RFB_NORMAL,             /**< normal protocol messages */

        /* Ephemeral internal-use states that will never be seen by software
         * using LibVNCServer to provide services: */

        RFB_INITIALISATION_SHARED, /**< sending initialisation messages with implicit shared-flag already true */
        RFB_SHUTDOWN            /**< Client is shutting down */
    } state;

    rfbBool reverseConnection;
    rfbBool onHold;
    rfbBool readyForSetColourMapEntries;
    rfbBool useCopyRect;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;

    rfbBool viewOnly;

    /* The following member is only used during VNC authentication */
    uint8_t authChallenge[CHALLENGESIZE];

    /* The following members represent the update needed to get the client's
       framebuffer from its present state to the current state of our
       framebuffer.

       If the client does not accept CopyRect encoding then the update is
       simply represented as the region of the screen which has been modified
       (modifiedRegion).

       If the client does accept CopyRect encoding, then the update consists of
       two parts.  First we have a single copy from one region of the screen to
       another (the destination of the copy is copyRegion), and second we have
       the region of the screen which has been modified in some other way
       (modifiedRegion).

       Although the copy is of a single region, this region may have many
       rectangles.  When sending an update, the copyRegion is always sent
       before the modifiedRegion.  This is because the modifiedRegion may
       overlap parts of the screen which are in the source of the copy.

       In fact during normal processing, the modifiedRegion may even overlap
       the destination copyRegion.  Just before an update is sent we remove
       from the copyRegion anything in the modifiedRegion. */

    sraRegionPtr copyRegion;	/**< the destination region of the copy */
    int copyDX, copyDY;		/**< the translation by which the copy happens */

    sraRegionPtr modifiedRegion;

    /** As part of the FramebufferUpdateRequest, a client can express interest
       in a subrectangle of the whole framebuffer.  This is stored in the
       requestedRegion member.  In the normal case this is the whole
       framebuffer if the client is ready, empty if it's not. */

    sraRegionPtr requestedRegion;

    /** The following member represents the state of the "deferred update" timer
       - when the framebuffer is modified and the client is ready, in most
       cases it is more efficient to defer sending the update by a few
       milliseconds so that several changes to the framebuffer can be combined
       into a single update. */

      struct timeval startDeferring;
      struct timeval startPtrDeferring;
      int lastPtrX;
      int lastPtrY;
      int lastPtrButtons;

    /** translateFn points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;
    char *translateLookupTable;
    rfbPixelFormat format;

    /**
     * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
     * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
     * means 8K minimum.
     */

#define UPDATE_BUF_SIZE 32768

    char updateBuf[UPDATE_BUF_SIZE];
    int ublen;

    /* statistics */
    struct _rfbStatList *statEncList;
    struct _rfbStatList *statMsgList;
    int rawBytesEquivalent;
    int bytesSent;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    /* zlib encoding -- necessary compression state info per client */

    struct z_stream_s compStream;
    rfbBool compStreamInited;
    uint32_t zlibCompressLevel;
#endif
#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
    /** the quality level is also used by ZYWRLE and TightPng */
    int tightQualityLevel;

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    /* tight encoding -- preserve zlib streams' state for each client */
    z_stream zsStruct[4];
    rfbBool zsActive[4];
    int zsLevel[4];
    int tightCompressLevel;
#endif
#endif

    /* Ultra Encoding support */
    rfbBool compStreamInitedLZO;
    char *lzoWrkMem;

    rfbFileTransferData fileTransfer;

    int     lastKeyboardLedState;     /**< keep track of last value so we can send *change* events */
    rfbBool enableSupportedMessages;  /**< client supports SupportedMessages encoding */
    rfbBool enableSupportedEncodings; /**< client supports SupportedEncodings encoding */
    rfbBool enableServerIdentity;     /**< client supports ServerIdentity encoding */
    rfbBool enableKeyboardLedState;   /**< client supports KeyboardState encoding */
    rfbBool enableLastRectEncoding;   /**< client supports LastRect encoding */
    rfbBool enableCursorShapeUpdates; /**< client supports cursor shape updates */
    rfbBool enableCursorPosUpdates;   /**< client supports cursor position updates */
    rfbBool useRichCursorEncoding;    /**< rfbEncodingRichCursor is preferred */
    rfbBool cursorWasChanged;         /**< cursor shape update should be sent */
    rfbBool cursorWasMoved;           /**< cursor position update should be sent */
    int cursorX,cursorY;	      /**< the coordinates of the cursor,
					 if enableCursorShapeUpdates = FALSE */

    rfbBool useNewFBSize;             /**< client supports NewFBSize encoding */
    rfbBool newFBSizePending;         /**< framebuffer size was changed */

    struct _rfbClientRec *prev;
    struct _rfbClientRec *next;

#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    /** whenever a client is referenced, the refCount has to be incremented
       and afterwards decremented, so that the client is not cleaned up
       while being referenced.
       Use the functions rfbIncrClientRef(cl) and rfbDecrClientRef(cl);
    */
    int refCount;
    MUTEX(refCountMutex);
    COND(deleteCond);

    MUTEX(outputMutex);
    MUTEX(updateMutex);
    COND(updateCond);
#endif

#ifdef LIBVNCSERVER_HAVE_LIBZ
    void* zrleData;
    int zywrleLevel;
    int zywrleBuf[rfbZRLETileWidth * rfbZRLETileHeight];
#endif

    /** if progressive updating is on, this variable holds the current
     * y coordinate of the progressive slice. */
    int progressiveSliceY;

    rfbExtensionData* extensions;

    /** for threaded zrle */
    char *zrleBeforeBuf;
    void *paletteHelper;

    /** for thread safety for rfbSendFBUpdate() */
#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)
#define LIBVNCSERVER_SEND_MUTEX
    MUTEX(sendMutex);
#endif

  /* buffers to hold pixel data before and after encoding.
     per-client for thread safety */
  char *beforeEncBuf;
  int beforeEncBufSize;
  char *afterEncBuf;
  int afterEncBufSize;
  int afterEncBufLen;
#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
    uint32_t tightEncoding;  /* rfbEncodingTight or rfbEncodingTightPng */
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    /* TurboVNC Encoding support (extends TightVNC) */
    int turboSubsampLevel;
    int turboQualityLevel;  /* 1-100 scale */
#endif
#endif
    rfbSslCtx *sslctx;
    wsCtx     *wsctx;
    char *wspath;                          /* Requests path component */
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    int pipe_notify_client_thread[2];
#endif

    /**
     * clientFramebufferUpdateRequestHook is called when a client requests a frame
     * buffer update.
     */
    ClientFramebufferUpdateRequestHookPtr clientFramebufferUpdateRequestHook;

    rfbBool useExtDesktopSize;
    int requestedDesktopSizeChange;
    int lastDesktopSizeChangeError;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    rfbBool enableExtendedClipboard;
    uint32_t extClipboardUserCap;
    uint32_t extClipboardMaxUnsolicitedSize;
    char *extClipboardData;
    int extClipboardDataSize;

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    /* Tight encoding internal variables, stored per-client for thread safety */
    rfbBool tightUsePixelFormat24;
    void *tightTJ;
    int tightPngDstDataLen;
#endif
#endif
} rfbClientRec, *rfbClientPtr;

/**
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)                                              \
     (((cl)->enableCursorShapeUpdates && (cl)->cursorWasChanged) ||        \
     (((cl)->enableCursorShapeUpdates == FALSE &&                          \
       ((cl)->cursorX != (cl)->screen->cursorX ||                          \
	(cl)->cursorY != (cl)->screen->cursorY))) ||                       \
     ((cl)->useNewFBSize && (cl)->newFBSizePending) ||                     \
     ((cl)->enableCursorPosUpdates && (cl)->cursorWasMoved) ||             \
     !sraRgnEmpty((cl)->copyRegion) || !sraRgnEmpty((cl)->modifiedRegion))

/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap24(l) ((((l) & 0xff) << 16) | (((l) >> 16) & 0xff) | \
                   (((l) & 0x00ff00)))

#define Swap32(l) ((((l) >> 24) & 0x000000ff)| \
                   (((l) & 0x00ff0000) >> 8)  | \
                   (((l) & 0x0000ff00) << 8)  | \
                   (((l) & 0x000000ff) << 24))


extern char rfbEndianTest;

#define Swap16IfLE(s) (rfbEndianTest ? Swap16(s) : (s))
#define Swap24IfLE(l) (rfbEndianTest ? Swap24(l) : (l))
#define Swap32IfLE(l) (rfbEndianTest ? Swap32(l) : (l))

/* UltraVNC uses some windows structures unmodified, so the viewer expects LittleEndian Data */
#define Swap16IfBE(s) (rfbEndianTest ? (s) : Swap16(s))
#define Swap24IfBE(l) (rfbEndianTest ? (l) : Swap24(l))
#define Swap32IfBE(l) (rfbEndianTest ? (l) : Swap32(l))

/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbShutdownSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbDisconnectUDPSock(rfbScreenInfoPtr rfbScreen);
extern void rfbCloseClient(rfbClientPtr cl);
extern int rfbReadExact(rfbClientPtr cl, char *buf, int len);
extern int rfbReadExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout);
extern int rfbPeekExactTimeout(rfbClientPtr cl, char *buf, int len,int timeout);
extern int rfbWriteExact(rfbClientPtr cl, const char *buf, int len);
extern int rfbCheckFds(rfbScreenInfoPtr rfbScreen,long usec);
extern rfbSocket rfbConnect(rfbScreenInfoPtr rfbScreen, char* host, int port);
extern rfbSocket rfbConnectToTcpAddr(char* host, int port);
extern rfbSocket rfbListenOnTCPPort(int port, in_addr_t iface);
extern rfbSocket rfbListenOnTCP6Port(int port, const char* iface);
extern rfbSocket rfbListenOnUDPPort(int port, in_addr_t iface);
extern int rfbStringToAddr(char* string,in_addr_t* addr);
extern rfbBool rfbSetNonBlocking(rfbSocket sock);

#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
/* websockets.c */

extern rfbBool webSocketsCheck(rfbClientPtr cl);
extern rfbBool webSocketCheckDisconnect(rfbClientPtr cl);
extern int webSocketsEncode(rfbClientPtr cl, const char *src, int len, char **dst);
extern int webSocketsDecode(rfbClientPtr cl, char *dst, int len);
extern rfbBool webSocketsHasDataInBuffer(rfbClientPtr cl);
#endif

/* rfbserver.c */

/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(rfbScreenInfoPtr rfbScreen);
extern rfbClientIteratorPtr rfbGetClientIterator(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);
extern void rfbIncrClientRef(rfbClientPtr cl);
extern void rfbDecrClientRef(rfbClientPtr cl);

extern void rfbNewClientConnection(rfbScreenInfoPtr rfbScreen,rfbSocket sock);
extern rfbClientPtr rfbNewClient(rfbScreenInfoPtr rfbScreen,rfbSocket sock);
extern rfbClientPtr rfbNewUDPClient(rfbScreenInfoPtr rfbScreen);
extern rfbClientPtr rfbReverseConnection(rfbScreenInfoPtr rfbScreen,char *host, int port);
extern void rfbClientConnectionGone(rfbClientPtr cl);
extern void rfbProcessClientMessage(rfbClientPtr cl);
extern void rfbClientConnFailed(rfbClientPtr cl, const char *reason);
extern void rfbNewUDPConnection(rfbScreenInfoPtr rfbScreen,rfbSocket sock);
extern void rfbProcessUDPInput(rfbScreenInfoPtr rfbScreen);
extern rfbBool rfbSendFramebufferUpdate(rfbClientPtr cl, sraRegionPtr updateRegion);
extern rfbBool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendUpdateBuf(rfbClientPtr cl);
extern void rfbSendServerCutText(rfbScreenInfoPtr rfbScreen,char *str, int len);
#ifdef LIBVNCSERVER_HAVE_LIBZ
extern void rfbSendServerCutTextUTF8(rfbScreenInfoPtr rfbScreen,char *str, int len, char *fallbackLatin1Str, int latin1Len);
#endif
extern rfbBool rfbSendCopyRegion(rfbClientPtr cl,sraRegionPtr reg,int dx,int dy);
extern rfbBool rfbSendLastRectMarker(rfbClientPtr cl);
extern rfbBool rfbSendNewFBSize(rfbClientPtr cl, int w, int h);
extern rfbBool rfbSendExtDesktopSize(rfbClientPtr cl, int w, int h);
extern rfbBool rfbSendSetColourMapEntries(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSendBell(rfbScreenInfoPtr rfbScreen);

extern char *rfbProcessFileTransferReadBuffer(rfbClientPtr cl, uint32_t length);
extern rfbBool rfbSendFileTransferChunk(rfbClientPtr cl);
extern rfbBool rfbSendDirContent(rfbClientPtr cl, int length, char *buffer);
extern rfbBool rfbSendFileTransferMessage(rfbClientPtr cl, uint8_t contentType, uint8_t contentParam, uint32_t size, uint32_t length, const char *buffer);
extern char *rfbProcessFileTransferReadBuffer(rfbClientPtr cl, uint32_t length);
extern rfbBool rfbProcessFileTransfer(rfbClientPtr cl, uint8_t contentType, uint8_t contentParam, uint32_t size, uint32_t length);

void rfbGotXCutText(rfbScreenInfoPtr rfbScreen, char *str, int len);

/* translate.c */

extern rfbBool rfbEconomicTranslate;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern rfbBool rfbSetTranslateFunction(rfbClientPtr cl);
extern rfbBool rfbSetClientColourMap(rfbClientPtr cl, int firstColour, int nColours);
extern void rfbSetClientColourMaps(rfbScreenInfoPtr rfbScreen, int firstColour, int nColours);

/* httpd.c */

extern void rfbHttpInitSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbHttpShutdownSockets(rfbScreenInfoPtr rfbScreen);
extern void rfbHttpCheckFds(rfbScreenInfoPtr rfbScreen);



/* auth.c */

extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbProcessClientSecurityType(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);
extern void rfbRegisterSecurityHandler(rfbSecurityHandler* handler);
extern void rfbUnregisterSecurityHandler(rfbSecurityHandler* handler);

/* rre.c */

extern rfbBool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern rfbBool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern rfbBool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
                                       int h);

/* ultra.c */

/* Set maximum ultra rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ULTRA_MAX_RECT_SIZE (128*256)
#define ULTRA_MAX_SIZE(min) ((( min * 2 ) > ULTRA_MAX_RECT_SIZE ) ? \
                            ( min * 2 ) : ULTRA_MAX_RECT_SIZE )

extern rfbBool rfbSendRectEncodingUltra(rfbClientPtr cl, int x,int y,int w,int h);


#ifdef LIBVNCSERVER_HAVE_LIBZ
/* zlib.c */

/** Minimum zlib rectangle size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIB_MIN_COMP_SIZE (17)

/* Set maximum zlib rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ZLIB_MAX_RECT_SIZE (128*256)
#define ZLIB_MAX_SIZE(min) ((( min * 2 ) > ZLIB_MAX_RECT_SIZE ) ? \
			    ( min * 2 ) : ZLIB_MAX_RECT_SIZE )

extern rfbBool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
				    int h);

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
/* tight.c */

#define TIGHT_DEFAULT_COMPRESSION  6
#define TURBO_DEFAULT_SUBSAMP 0

extern int rfbNumCodedRectsTight(rfbClientPtr cl, int x,int y,int w,int h);

extern rfbBool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);
extern rfbBool rfbSendTightHeader(rfbClientPtr cl, int x, int y, int w, int h);
extern rfbBool rfbSendCompressedDataTight(rfbClientPtr cl, char *buf, int compressedLen);

#if defined(LIBVNCSERVER_HAVE_LIBPNG)
extern rfbBool rfbSendRectEncodingTightPng(rfbClientPtr cl, int x,int y,int w,int h);
#endif

#endif
#endif


/* cursor.c */

typedef struct rfbCursor {
    /** set this to true if LibVNCServer has to free this cursor */
    rfbBool cleanup, cleanupSource, cleanupMask, cleanupRichSource;
    unsigned char *source;			/**< points to bits */
    unsigned char *mask;			/**< points to bits */
    unsigned short width, height, xhot, yhot;	/**< metrics */
    unsigned short foreRed, foreGreen, foreBlue; /**< device-independent colour */
    unsigned short backRed, backGreen, backBlue; /**< device-independent colour */
    unsigned char *richSource; /**< source bytes for a rich cursor */
    unsigned char *alphaSource; /**< source for alpha blending info */
    rfbBool alphaPreMultiplied; /**< if richSource already has alpha applied */
} rfbCursor, *rfbCursorPtr;
extern unsigned char rfbReverseByte[0x100];

extern rfbBool rfbSendCursorShape(rfbClientPtr cl/*, rfbScreenInfoPtr pScreen*/);
extern rfbBool rfbSendCursorPos(rfbClientPtr cl);
extern void rfbConvertLSBCursorBitmapOrMask(int width,int height,unsigned char* bitmap);
extern rfbCursorPtr rfbMakeXCursor(int width,int height,char* cursorString,char* maskString);
extern char* rfbMakeMaskForXCursor(int width,int height,char* cursorString);
extern char* rfbMakeMaskFromAlphaSource(int width,int height,unsigned char* alphaSource);
extern void rfbMakeXCursorFromRichCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbMakeRichCursorFromXCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor);
extern void rfbFreeCursor(rfbCursorPtr cursor);
extern void rfbSetCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr c);

/** cursor handling for the pointer */
extern void rfbDefaultPtrAddEvent(int buttonMask,int x,int y,rfbClientPtr cl);

/* zrle.c */
#ifdef LIBVNCSERVER_HAVE_LIBZ
extern rfbBool rfbSendRectEncodingZRLE(rfbClientPtr cl, int x, int y, int w,int h);
#endif

/* stats.c */

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);

/* font.c */

typedef struct rfbFontData {
  unsigned char* data;
  /**
    metaData is a 256*5 array:
    for each character
    (offset,width,height,x,y)
  */
  int* metaData;
} rfbFontData,* rfbFontDataPtr;

int rfbDrawChar(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,unsigned char c,rfbPixel colour);
void rfbDrawString(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,const char* string,rfbPixel colour);
/** if colour==backColour, background is transparent */
int rfbDrawCharWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,unsigned char c,int x1,int y1,int x2,int y2,rfbPixel colour,rfbPixel backColour);
void rfbDrawStringWithClip(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,int x,int y,const char* string,int x1,int y1,int x2,int y2,rfbPixel colour,rfbPixel backColour);
int rfbWidthOfString(rfbFontDataPtr font,const char* string);
int rfbWidthOfChar(rfbFontDataPtr font,unsigned char c);
void rfbFontBBox(rfbFontDataPtr font,unsigned char c,int* x1,int* y1,int* x2,int* y2);
/** this returns the smallest box enclosing any character of font. */
void rfbWholeFontBBox(rfbFontDataPtr font,int *x1, int *y1, int *x2, int *y2);

/** dynamically load a linux console font (4096 bytes, 256 glyphs a 8x16 */
rfbFontDataPtr rfbLoadConsoleFont(char *filename);
/** free a dynamically loaded font */
void rfbFreeFont(rfbFontDataPtr font);

/* draw.c */

void rfbFillRect(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col);
void rfbDrawPixel(rfbScreenInfoPtr s,int x,int y,rfbPixel col);
void rfbDrawLine(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2,rfbPixel col);

/* selbox.c */

/** this opens a modal select box. list is an array of strings, the end marked
   with a NULL.
   It returns the index in the list or -1 if cancelled or something else
   wasn't kosher. */
typedef void (*SelectionChangedHookPtr)(int _index);
extern int rfbSelectBox(rfbScreenInfoPtr rfbScreen,
			rfbFontDataPtr font, char** list,
			int x1, int y1, int x2, int y2,
			rfbPixel foreColour, rfbPixel backColour,
			int border,SelectionChangedHookPtr selChangedHook);

/* cargs.c */

extern void rfbUsage(void);
extern void rfbPurgeArguments(int* argc,int* position,int count,char *argv[]);
extern rfbBool rfbProcessArguments(rfbScreenInfoPtr rfbScreen,int* argc, char *argv[]);
extern rfbBool rfbProcessSizeArguments(int* width,int* height,int* bpp,int* argc, char *argv[]);

/* main.c */

extern void rfbLogEnable(int enabled);
typedef void (*rfbLogProc)(const char *format, ...);
extern rfbLogProc rfbLog, rfbErr;
extern void rfbLogPerror(const char *str);

void rfbScheduleCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbScheduleCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbDoCopyRect(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2,int dx,int dy);
void rfbDoCopyRegion(rfbScreenInfoPtr rfbScreen,sraRegionPtr copyRegion,int dx,int dy);

void rfbMarkRectAsModified(rfbScreenInfoPtr rfbScreen,int x1,int y1,int x2,int y2);
void rfbMarkRegionAsModified(rfbScreenInfoPtr rfbScreen,sraRegionPtr modRegion);
void rfbDoNothingWithClient(rfbClientPtr cl);
enum rfbNewClientAction defaultNewClientHook(rfbClientPtr cl);
void rfbRegisterProtocolExtension(rfbProtocolExtension* extension);
void rfbUnregisterProtocolExtension(rfbProtocolExtension* extension);
struct _rfbProtocolExtension* rfbGetExtensionIterator(void);
void rfbReleaseExtensionIterator(void);
rfbBool rfbEnableExtension(rfbClientPtr cl, rfbProtocolExtension* extension,
	void* data);
rfbBool rfbDisableExtension(rfbClientPtr cl, rfbProtocolExtension* extension);
void* rfbGetExtensionClientData(rfbClientPtr cl, rfbProtocolExtension* extension);

/** to check against plain passwords */
rfbBool rfbCheckPasswordByList(rfbClientPtr cl,const char* response,int len);

/* functions to make a vnc server */
extern rfbScreenInfoPtr rfbGetScreen(int* argc,char** argv,
 int width,int height,int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);
extern void rfbInitServer(rfbScreenInfoPtr rfbScreen);
extern void rfbShutdownServer(rfbScreenInfoPtr rfbScreen,rfbBool disconnectClients);
extern void rfbNewFramebuffer(rfbScreenInfoPtr rfbScreen,char *framebuffer,
 int width,int height, int bitsPerSample,int samplesPerPixel,
 int bytesPerPixel);

extern void rfbScreenCleanup(rfbScreenInfoPtr screenInfo);
extern void rfbSetServerVersionIdentity(rfbScreenInfoPtr screen, char *fmt, ...);

/* functions to accept/refuse a client that has been put on hold
   by a NewClientHookPtr function. Must not be called in other
   situations. */
extern void rfbStartOnHoldClient(rfbClientPtr cl);
extern void rfbRefuseOnHoldClient(rfbClientPtr cl);

/* call one of these two functions to service the vnc clients.
 usec are the microseconds the select on the fds waits.
 if you are using the event loop, set this to some value > 0, so the
 server doesn't get a high load just by listening.
 rfbProcessEvents() returns TRUE if an update was pending. */

extern void rfbRunEventLoop(rfbScreenInfoPtr screenInfo, long usec, rfbBool runInBackground);
extern rfbBool rfbProcessEvents(rfbScreenInfoPtr screenInfo,long usec);
extern rfbBool rfbIsActive(rfbScreenInfoPtr screenInfo);

/**
 * Register the TightVNC-1.3.x file transfer extension.
 * NB That TightVNC-2.x uses a different, incompatible file transfer protocol.
 */
void rfbRegisterTightVNCFileTransferExtension(void);
/**
 * Unregister the TightVNC-1.3.x file transfer extension.
 * NB That TightVNC-2.x uses a different, incompatible file transfer protocol.
 */
void rfbUnregisterTightVNCFileTransferExtension(void);

/* Statistics */
extern char *messageNameServer2Client(uint32_t type, char *buf, int len);
extern char *messageNameClient2Server(uint32_t type, char *buf, int len);
extern char *encodingName(uint32_t enc, char *buf, int len);

extern rfbStatList *rfbStatLookupEncoding(rfbClientPtr cl, uint32_t type);
extern rfbStatList *rfbStatLookupMessage(rfbClientPtr cl, uint32_t type);

/* Each call to rfbStatRecord* adds one to the rect count for that type */
extern void rfbStatRecordEncodingSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
extern void rfbStatRecordEncodingSentAdd(rfbClientPtr cl, uint32_t type, int byteCount); /* Specifically for tight encoding */
extern void rfbStatRecordEncodingRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
extern void rfbStatRecordMessageSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
extern void rfbStatRecordMessageRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);

extern int rfbStatGetSentBytes(rfbClientPtr cl);
extern int rfbStatGetSentBytesIfRaw(rfbClientPtr cl);
extern int rfbStatGetRcvdBytes(rfbClientPtr cl);
extern int rfbStatGetRcvdBytesIfRaw(rfbClientPtr cl);
extern int rfbStatGetMessageCountSent(rfbClientPtr cl, uint32_t type);
extern int rfbStatGetMessageCountRcvd(rfbClientPtr cl, uint32_t type);
extern int rfbStatGetEncodingCountSent(rfbClientPtr cl, uint32_t type);
extern int rfbStatGetEncodingCountRcvd(rfbClientPtr cl, uint32_t type);

/** Set which version you want to advertise 3.3, 3.6, 3.7 and 3.8 are currently supported*/
extern void rfbSetProtocolVersion(rfbScreenInfoPtr rfbScreen, int major_, int minor_);

/** send a TextChat message to a client */
extern rfbBool rfbSendTextChatMessage(rfbClientPtr cl, uint32_t length, char *buffer);


/*
 * Additions for Qt event loop integration
 * Original idea taken from vino.
 */
rfbBool rfbProcessNewConnection(rfbScreenInfoPtr rfbScreen);
rfbBool rfbUpdateClient(rfbClientPtr cl);


#if(defined __cplusplus)
}
#endif

/**
 * @}
 */


/**
 @page libvncserver_doc LibVNCServer Documentation
 @tableofcontents
 @section create_server Creating a server instance
 To make a server, you just have to initialise a server structure using the
 function rfbGetScreen(), like
 @code
   rfbScreenInfoPtr screen =
     rfbGetScreen(argc,argv,screenwidth,screenheight,8,3,bpp);
 @endcode
 where byte per pixel should be 1, 2 or 4. If performance doesn't matter,
 you may try bpp=3 (internally one cannot use native data types in this
 case; if you want to use this, look at pnmshow24.c).

 You then can set hooks and io functions (see @ref making_it_interactive) or other
 options (see @ref server_options).

 And you allocate the frame buffer like this:
 @code
     screen->frameBuffer = (char*)malloc(screenwidth*screenheight*bpp);
 @endcode
 After that, you initialize the server, like
 @code
   rfbInitServer(screen);
 @endcode
 You can use a blocking event loop, a background (pthread based) event loop,
 or implement your own using the rfbProcessEvents() function.

 @subsection server_options Optional Server Features

 These options have to be set between rfbGetScreen() and rfbInitServer().

 If you already have a socket to talk to, just set rfbScreenInfo::inetdSock
 (originally this is for inetd handling, but why not use it for your purpose?).

 To also start an HTTP server (running on port 5800+display_number), you have
 to set rfbScreenInfo::httpDir to a directory containing vncviewer.jar and
 index.vnc (like the included "webclients" directory).

 @section making_it_interactive Making it interactive

 Whenever you draw something, you have to call
 @code
  rfbMarkRectAsModified(screen,x1,y1,x2,y2).
 @endcode
 This tells LibVNCServer to send updates to all connected clients.

 There exist the following IO functions as members of rfbScreen:
 rfbScreenInfo::kbdAddEvent(), rfbScreenInfo::kbdReleaseAllKeys(), rfbScreenInfo::ptrAddEvent(),
 rfbScreenInfo::setXCutText() and rfbScreenInfo::setXCutTextUTF8()

 rfbScreenInfo::kbdAddEvent()
   is called when a key is pressed.
 rfbScreenInfo::kbdReleaseAllKeys()
   is not called at all (maybe in the future).
 rfbScreenInfo::ptrAddEvent()
   is called when the mouse moves or a button is pressed.
   WARNING: if you want to have proper cursor handling, call
	rfbDefaultPtrAddEvent()
   in your own function. This sets the coordinates of the cursor.
 rfbScreenInfo::setXCutText()
   is called when the selection changes.
 rfbScreenInfo::setXCutTextUTF8()
   is called when the selection changes and the ExtendedClipboard extension is enabled.

 There are only two hooks:
 rfbScreenInfo::newClientHook()
   is called when a new client has connected.
 rfbScreenInfo::displayHook()
   is called just before a frame buffer update is sent.

 You can also override the following methods:
 rfbScreenInfo::getCursorPtr()
   This could be used to make an animated cursor (if you really want ...)
 rfbScreenInfo::setTranslateFunction()
   If you insist on colour maps or something more obscure, you have to
   implement this. Default is a trueColour mapping.

 @section cursor_handling Cursor handling

 The screen holds a pointer
  rfbScreenInfo::cursor
 to the current cursor. Whenever you set it, remember that any dynamically
 created cursor (like return value from rfbMakeXCursor()) is not free'd!

 The rfbCursor structure consists mainly of a mask and a source. The rfbCursor::mask
 describes, which pixels are drawn for the cursor (a cursor needn't be
 rectangular). The rfbCursor::source describes, which colour those pixels should have.

 The standard is an XCursor: a cursor with a foreground and a background
 colour (stored in backRed,backGreen,backBlue and the same for foreground
 in a range from 0-0xffff). Therefore, the arrays "mask" and "source"
 contain pixels as single bits stored in bytes in MSB order. The rows are
 padded, such that each row begins with a new byte (i.e. a 10x4
 cursor's mask has 2x4 bytes, because 2 bytes are needed to hold 10 bits).

 It is however very easy to make a cursor like this:
 @code
 char* cur="    "
           " xx "
	   " x  "
	   "    ";
 char* mask="xxxx"
            "xxxx"
            "xxxx"
            "xxx ";
 rfbCursorPtr c=rfbMakeXCursor(4,4,cur,mask);
 @endcode
 You can even set rfbCursor::mask to NULL in this call and LibVNCServer will calculate
 a mask for you (dynamically, so you have to free it yourself).

 There is also an array named rfbCursor::richSource for colourful cursors. They have
 the same format as the frameBuffer (i.e. if the server is 32 bit,
 a 10x4 cursor has 4x10x4 bytes).

 @section screen_client_difference What is the difference between rfbScreenInfoPtr and rfbClientPtr?

 The rfbScreenInfoPtr is a pointer to a rfbScreenInfo structure, which
 holds information about the server, like pixel format, io functions,
 frame buffer etc. The rfbClientPtr is a pointer to an rfbClientRec structure, which holds
 information about a client, like pixel format, socket of the
 connection, etc. A server can have several clients, but needn't have any. So, if you
 have a server and three clients are connected, you have one instance
 of a rfbScreenInfo and three instances of rfbClientRec's.

 The rfbClientRec structure holds a member rfbClientRec::screen which points to the server.
 So, to access the server from the client structure, you use client->screen.

 To access all clients from a server be sure to use the provided iterator
  rfbGetClientIterator()
 with
  rfbClientIteratorNext()
 and
  rfbReleaseClientIterator()
 to prevent thread clashes.

 @section example_code Example Code

 There are two documented examples included:
  - example.c, a shared scribble sheet
  - pnmshow.c, a program to show PNMs (pictures) over the net.

 The examples are not too well documented, but easy straight forward and a
 good starting point.

 Try example.c: it outputs on which port it listens (default: 5900), so it is
 display 0. To view, call @code	vncviewer :0 @endcode
 You should see a sheet with a gradient and "Hello World!" written on it. Try
 to paint something. Note that every time you click, there is some bigger blot,
 whereas when you drag the mouse while clicked you draw a line. The size of the
 blot depends on the mouse button you click. Open a second vncviewer with
 the same parameters and watch it as you paint in the other window. This also
 works over internet. You just have to know either the name or the IP of your
 machine. Then it is @code vncviewer machine.where.example.runs.com:0 @endcode
 or similar for the remote client. Now you are ready to type something. Be sure
 that your mouse sits still, because every time the mouse moves, the cursor is
 reset to the position of the pointer! If you are done with that demo, press
 the down or up arrows. If your viewer supports it, then the dimensions of the
 sheet change. Just press Escape in the viewer. Note that the server still
 runs, even if you closed both windows. When you reconnect now, everything you
 painted and wrote is still there. You can press "Page Up" for a blank page.

 The demo pnmshow.c is much simpler: you either provide a filename as argument
 or pipe a file through stdin. Note that the file has to be a raw pnm/ppm file,
 i.e. a truecolour graphics. Only the Escape key is implemented. This may be
 the best starting point if you want to learn how to use LibVNCServer. You
 are confronted with the fact that the bytes per pixel can only be 8, 16 or 32.
*/

#endif
