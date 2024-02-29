/*
 * Copyright (c) 2005 Novell, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail,
 * you may find current contact information at www.novell.com 
 *
 * Author		: Rohit Kumar
 * Email ID	: rokumar@novell.com
 * Date		: 25th August 2005
 */

#ifndef RFBTIGHTPROTO_H
#define RFBTIGHTPROTO_H

#include <rfb/rfb.h>
#include <limits.h>

/* PATH_MAX is not defined in limits.h on some platforms */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


#define rfbSecTypeTight 16

void rfbTightUsage(void);
int rfbTightProcessArgs(int argc, char *argv[]);

/*-----------------------------------------------------------------------------
 * Negotiation of Tunneling Capabilities (protocol version 3.7t)
 *
 * If the chosen security type is rfbSecTypeTight, the server sends a list of
 * supported tunneling methods ("tunneling" refers to any additional layer of
 * data transformation, such as encryption or external compression.)
 *
 * nTunnelTypes specifies the number of following rfbCapabilityInfo structures
 * that list all supported tunneling methods in the order of preference.
 *
 * NOTE: If nTunnelTypes is 0, that tells the client that no tunneling can be
 * used, and the client should not send a response requesting a tunneling
 * method.
 */

typedef struct _rfbTunnelingCapsMsg {
    uint32_t nTunnelTypes;
    /* followed by nTunnelTypes * rfbCapabilityInfo structures */
} rfbTunnelingCapsMsg;

#define sz_rfbTunnelingCapsMsg 4

/*-----------------------------------------------------------------------------
 * Tunneling Method Request (protocol version 3.7t)
 *
 * If the list of tunneling capabilities sent by the server was not empty, the
 * client should reply with a 32-bit code specifying a particular tunneling
 * method.  The following code should be used for no tunneling.
 */

#define rfbNoTunneling 0
#define sig_rfbNoTunneling "NOTUNNEL"


/*-----------------------------------------------------------------------------
 * Negotiation of Authentication Capabilities (protocol version 3.7t)
 *
 * After setting up tunneling, the server sends a list of supported
 * authentication schemes.
 *
 * nAuthTypes specifies the number of following rfbCapabilityInfo structures
 * that list all supported authentication schemes in the order of preference.
 *
 * NOTE: If nAuthTypes is 0, that tells the client that no authentication is
 * necessary, and the client should not send a response requesting an
 * authentication scheme.
 */

typedef struct _rfbAuthenticationCapsMsg {
    uint32_t nAuthTypes;
    /* followed by nAuthTypes * rfbCapabilityInfo structures */
} rfbAuthenticationCapsMsg;

#define sz_rfbAuthenticationCapsMsg 4

/*-----------------------------------------------------------------------------
 * Authentication Scheme Request (protocol version 3.7t)
 *
 * If the list of authentication capabilities sent by the server was not empty,
 * the client should reply with a 32-bit code specifying a particular
 * authentication scheme.  The following codes are supported.
 */

#define rfbAuthNone 1
#define rfbAuthVNC 2
#define rfbAuthUnixLogin 129
#define rfbAuthExternal 130

#define sig_rfbAuthNone "NOAUTH__"
#define sig_rfbAuthVNC "VNCAUTH_"
#define sig_rfbAuthUnixLogin "ULGNAUTH"
#define sig_rfbAuthExternal "XTRNAUTH"

/*-----------------------------------------------------------------------------
 * Structure used to describe protocol options such as tunneling methods,
 * authentication schemes and message types (protocol version 3.7t).
 */

typedef struct _rfbCapabilityInfo {

    uint32_t  code;		/* numeric identifier */
    uint8_t    vendorSignature[4];	/* vendor identification */
    uint8_t    nameSignature[8];	/* abbreviated option name */

} rfbCapabilityInfo;

#define sz_rfbCapabilityInfoVendor 4
#define sz_rfbCapabilityInfoName 8
#define sz_rfbCapabilityInfo 16

/*
 * Vendors known by TightVNC: standard VNC/RealVNC, TridiaVNC, and TightVNC.
 */

#define rfbStandardVendor "STDV"
#define rfbTridiaVncVendor "TRDV"
#define rfbTightVncVendor "TGHT"


/* It's a good idea to keep these values a bit greater than required. */
#define MAX_TIGHT_ENCODINGS 10
#define MAX_TUNNELING_CAPS 16
#define MAX_AUTH_CAPS 16

typedef struct _rfbClientFileDownload {
	char fName[PATH_MAX];
	int downloadInProgress;
	unsigned long mTime;
	int downloadFD;
	pthread_t downloadThread;
} rfbClientFileDownload ;

typedef struct _rfbClientFileUpload {
	char fName[PATH_MAX];
	int uploadInProgress;
	unsigned long mTime;
	unsigned long fSize;
	int uploadFD;
} rfbClientFileUpload ;

typedef struct _rfbClientFileTransfer {
	rfbClientFileDownload rcfd;
	rfbClientFileUpload rcfu;
} rfbClientFileTransfer;


typedef struct _rfbTightClientRec {

    /* Lists of capability codes sent to clients. We remember these
       lists to restrict clients from choosing those tunneling and
       authentication types that were not advertised. */

    int nAuthCaps;
    uint32_t authCaps[MAX_AUTH_CAPS];

    /* This is not useful while we don't support tunneling:
    int nTunnelingCaps;
    uint32_t tunnelingCaps[MAX_TUNNELING_CAPS]; */

    rfbClientFileTransfer rcft;

} rfbTightClientRec, *rfbTightClientPtr;

/*
 * Macro to fill in an rfbCapabilityInfo structure (protocol 3.7t).
 * Normally, using macros is no good, but this macro saves us from
 * writing constants twice -- it constructs signature names from codes.
 * Note that "code_sym" argument should be a single symbol, not an expression.
 */

#define SetCapInfo(cap_ptr, code_sym, vendor)		\
{							\
    rfbCapabilityInfo *pcap;				\
    pcap = (cap_ptr);					\
    pcap->code = Swap32IfLE(code_sym);			\
    memcpy(pcap->vendorSignature, (vendor),		\
	   sz_rfbCapabilityInfoVendor);			\
    memcpy(pcap->nameSignature, sig_##code_sym,		\
	   sz_rfbCapabilityInfoName);			\
}

void rfbHandleSecTypeTight(rfbClientPtr cl);


/*-----------------------------------------------------------------------------
 * Server Interaction Capabilities Message (protocol version 3.7t)
 *
 * In the protocol version 3.7t, the server informs the client what message
 * types it supports in addition to ones defined in the protocol version 3.7.
 * Also, the server sends the list of all supported encodings (note that it's
 * not necessary to advertise the "raw" encoding since it MUST be supported in
 * RFB 3.x protocols).
 *
 * This data immediately follows the server initialisation message.
 */

typedef struct _rfbInteractionCapsMsg {
    uint16_t nServerMessageTypes;
    uint16_t nClientMessageTypes;
    uint16_t nEncodingTypes;
    uint16_t pad;			/* reserved, must be 0 */
    /* followed by nServerMessageTypes * rfbCapabilityInfo structures */
    /* followed by nClientMessageTypes * rfbCapabilityInfo structures */
} rfbInteractionCapsMsg;

#define sz_rfbInteractionCapsMsg 8

#define rfbFileListData 130
#define rfbFileDownloadData 131
#define rfbFileUploadCancel 132
#define rfbFileDownloadFailed 133

/* signatures for non-standard messages */
#define sig_rfbFileListData "FTS_LSDT"
#define sig_rfbFileDownloadData "FTS_DNDT"
#define sig_rfbFileUploadCancel "FTS_UPCN"
#define sig_rfbFileDownloadFailed "FTS_DNFL"



#define rfbFileListRequest 130
#define rfbFileDownloadRequest 131
#define rfbFileUploadRequest 132
#define rfbFileUploadData 133
#define rfbFileDownloadCancel 134
#define rfbFileUploadFailed 135
#define rfbFileCreateDirRequest 136

/* signatures for non-standard messages */
#define sig_rfbFileListRequest "FTC_LSRQ"
#define sig_rfbFileDownloadRequest "FTC_DNRQ"
#define sig_rfbFileUploadRequest "FTC_UPRQ"
#define sig_rfbFileUploadData "FTC_UPDT"
#define sig_rfbFileDownloadCancel "FTC_DNCN"
#define sig_rfbFileUploadFailed "FTC_UPFL"
#define sig_rfbFileCreateDirRequest "FTC_FCDR"


/* signatures for basic encoding types */
#define sig_rfbEncodingRaw       "RAW_____"
#define sig_rfbEncodingCopyRect  "COPYRECT"
#define sig_rfbEncodingRRE       "RRE_____"
#define sig_rfbEncodingCoRRE     "CORRE___"
#define sig_rfbEncodingHextile   "HEXTILE_"
#define sig_rfbEncodingZlib      "ZLIB____"
#define sig_rfbEncodingTight     "TIGHT___"
#define sig_rfbEncodingZlibHex   "ZLIBHEX_"


/* signatures for "fake" encoding types */
#define sig_rfbEncodingCompressLevel0  "COMPRLVL"
#define sig_rfbEncodingXCursor         "X11CURSR"
#define sig_rfbEncodingRichCursor      "RCHCURSR"
#define sig_rfbEncodingPointerPos      "POINTPOS"
#define sig_rfbEncodingLastRect        "LASTRECT"
#define sig_rfbEncodingNewFBSize       "NEWFBSIZ"
#define sig_rfbEncodingQualityLevel0   "JPEGQLVL"


/*-----------------------------------------------------------------------------
 * FileListRequest
 */

typedef struct _rfbFileListRequestMsg {
    uint8_t type;
    uint8_t flags;
    uint16_t dirNameSize;
    /* Followed by char Dirname[dirNameSize] */
} rfbFileListRequestMsg;

#define sz_rfbFileListRequestMsg 4

/*-----------------------------------------------------------------------------
 * FileDownloadRequest
 */

typedef struct _rfbFileDownloadRequestMsg {
    uint8_t type;
    uint8_t compressedLevel;
    uint16_t fNameSize;
    uint32_t position;
    /* Followed by char Filename[fNameSize] */
} rfbFileDownloadRequestMsg;

#define sz_rfbFileDownloadRequestMsg 8

/*-----------------------------------------------------------------------------
 * FileUploadRequest
 */

typedef struct _rfbFileUploadRequestMsg {
    uint8_t type;
    uint8_t compressedLevel;
    uint16_t fNameSize;
    uint32_t position;
    /* Followed by char Filename[fNameSize] */
} rfbFileUploadRequestMsg;

#define sz_rfbFileUploadRequestMsg 8


/*-----------------------------------------------------------------------------
 * FileUploadData
 */

typedef struct _rfbFileUploadDataMsg {
    uint8_t type;
    uint8_t compressedLevel;
    uint16_t realSize;
    uint16_t compressedSize;
    /* Followed by File[compressedSize], 
       but if (realSize = compressedSize = 0) followed by uint32_t modTime  */
} rfbFileUploadDataMsg;

#define sz_rfbFileUploadDataMsg 6

/*-----------------------------------------------------------------------------
 * FileDownloadCancel
 */

typedef struct _rfbFileDownloadCancelMsg {
    uint8_t type;
    uint8_t unused;
    uint16_t reasonLen;
    /* Followed by reason[reasonLen] */
} rfbFileDownloadCancelMsg;

#define sz_rfbFileDownloadCancelMsg 4

/*-----------------------------------------------------------------------------
 * FileUploadFailed
 */

typedef struct _rfbFileUploadFailedMsg {
    uint8_t type;
    uint8_t unused;
    uint16_t reasonLen;
    /* Followed by reason[reasonLen] */
} rfbFileUploadFailedMsg;

#define sz_rfbFileUploadFailedMsg 4

/*-----------------------------------------------------------------------------
 * FileCreateDirRequest
 */

typedef struct _rfbFileCreateDirRequestMsg {
    uint8_t type;
    uint8_t unused;
    uint16_t dNameLen;
    /* Followed by dName[dNameLen] */
} rfbFileCreateDirRequestMsg;

#define sz_rfbFileCreateDirRequestMsg 4


/*-----------------------------------------------------------------------------
 * Union of all client->server messages.
 */

typedef union _rfbClientToServerTightMsg {
    rfbFileListRequestMsg flr;
    rfbFileDownloadRequestMsg fdr;
    rfbFileUploadRequestMsg fupr;
    rfbFileUploadDataMsg fud;
    rfbFileDownloadCancelMsg fdc;
    rfbFileUploadFailedMsg fuf;
    rfbFileCreateDirRequestMsg fcdr;
} rfbClientToServerTightMsg;



/*-----------------------------------------------------------------------------
 * FileListData
 */

typedef struct _rfbFileListDataMsg {
    uint8_t type;
    uint8_t flags;
    uint16_t numFiles;
    uint16_t dataSize;
    uint16_t compressedSize;
    /* Followed by SizeData[numFiles] */
    /* Followed by Filenames[compressedSize] */
} rfbFileListDataMsg;

#define sz_rfbFileListDataMsg 8

/*-----------------------------------------------------------------------------
 * FileDownloadData
 */

typedef struct _rfbFileDownloadDataMsg {
    uint8_t type;
    uint8_t compressLevel;
    uint16_t realSize;
    uint16_t compressedSize;
    /* Followed by File[copressedSize], 
       but if (realSize = compressedSize = 0) followed by uint32_t modTime  */
} rfbFileDownloadDataMsg;

#define sz_rfbFileDownloadDataMsg 6


/*-----------------------------------------------------------------------------
 * FileUploadCancel
 */

typedef struct _rfbFileUploadCancelMsg {
    uint8_t type;
    uint8_t unused;
    uint16_t reasonLen;
    /* Followed by reason[reasonLen] */
} rfbFileUploadCancelMsg;

#define sz_rfbFileUploadCancelMsg 4

/*-----------------------------------------------------------------------------
 * FileDownloadFailed
 */

typedef struct _rfbFileDownloadFailedMsg {
    uint8_t type;
    uint8_t unused;
    uint16_t reasonLen;
    /* Followed by reason[reasonLen] */
} rfbFileDownloadFailedMsg;

#define sz_rfbFileDownloadFailedMsg 4




#endif


