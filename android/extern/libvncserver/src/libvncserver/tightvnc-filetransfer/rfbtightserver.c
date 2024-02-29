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


#include <rfb/rfb.h>
#include "rfbtightproto.h"
#include "handlefiletransferrequest.h"
#include "filetransfermsg.h"

/*
 * Get my data!
 *
 * This gets the extension specific data from the client structure. If
 * the data is not found, the client connection is closed, a complaint
 * is logged, and NULL is returned.
 */

extern rfbProtocolExtension tightVncFileTransferExtension;

rfbTightClientPtr 
rfbGetTightClientData(rfbClientPtr cl)
{
    rfbTightClientPtr rtcp = (rfbTightClientPtr) 
				rfbGetExtensionClientData(cl, 
				&tightVncFileTransferExtension);
    if(rtcp == NULL) {
        rfbLog("Extension client data is null, closing the connection !\n");
	rfbCloseClient(cl);
    }

    return rtcp;
}

/*
 * Send the authentication challenge.
 */

static void
rfbVncAuthSendChallenge(rfbClientPtr cl)
{
	
    rfbLog("tightvnc-filetransfer/rfbVncAuthSendChallenge\n");
    /* 4 byte header is alreay sent. Which is rfbSecTypeVncAuth (same as rfbVncAuth). Just send the challenge. */
    rfbRandomBytes(cl->authChallenge);
    if (rfbWriteExact(cl, (char *)cl->authChallenge, CHALLENGESIZE) < 0) {
        rfbLogPerror("rfbAuthNewClient: write");
        rfbCloseClient(cl);
        return;
    }
    
    /* Dispatch client input to rfbVncAuthProcessResponse. */
   /* This methos is defined in auth.c file */
    rfbAuthProcessClientMessage(cl);

}

/*
 * LibVNCServer has a bug WRT Tight SecurityType and RFB 3.8
 * It should send auth result even for rfbAuthNone.
 * See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=517422
 * For testing set USE_SECTYPE_TIGHT_FOR_RFB_3_8 when compiling
 * or set it here.
 */
#define SECTYPE_TIGHT_FOR_RFB_3_8 \
	if (cl->protocolMajorVersion==3 && cl->protocolMinorVersion > 7) { \
		uint32_t authResult; \
		rfbLog("rfbProcessClientSecurityType: returning securityResult for client rfb version >= 3.8\n"); \
		authResult = Swap32IfLE(rfbVncAuthOK); \
		if (rfbWriteExact(cl, (char *)&authResult, 4) < 0) { \
			rfbLogPerror("rfbAuthProcessClientMessage: write"); \
			rfbCloseClient(cl); \
			return; \
		} \
	}

/*
 Enabled by runge on 2010/01/02
 */
#define USE_SECTYPE_TIGHT_FOR_RFB_3_8

/*
 * Read client's preferred authentication type (protocol 3.7t).
 */

void
rfbProcessClientAuthType(rfbClientPtr cl)
{
    uint32_t auth_type;
    int n, i;
    rfbTightClientPtr rtcp = rfbGetTightClientData(cl);

    rfbLog("tightvnc-filetransfer/rfbProcessClientAuthType\n");

    if(rtcp == NULL)
	return;

    /* Read authentication type selected by the client. */
    n = rfbReadExact(cl, (char *)&auth_type, sizeof(auth_type));
    if (n <= 0) {
	if (n == 0)
	    rfbLog("rfbProcessClientAuthType: client gone\n");
	else
	    rfbLogPerror("rfbProcessClientAuthType: read");
	rfbCloseClient(cl);
	return;
    }
    auth_type = Swap32IfLE(auth_type);

    /* Make sure it was present in the list sent by the server. */
    for (i = 0; i < rtcp->nAuthCaps; i++) {
	if (auth_type == rtcp->authCaps[i])
	    break;
    }
    if (i >= rtcp->nAuthCaps) {
	rfbLog("rfbProcessClientAuthType: "
	       "wrong authentication type requested\n");
	rfbCloseClient(cl);
	return;
    }

    switch (auth_type) {
    case rfbAuthNone:
	/* Dispatch client input to rfbProcessClientInitMessage. */
#ifdef USE_SECTYPE_TIGHT_FOR_RFB_3_8
	SECTYPE_TIGHT_FOR_RFB_3_8
#endif
	cl->state = RFB_INITIALISATION;
	break;
    case rfbAuthVNC:
	rfbVncAuthSendChallenge(cl);
	break;
    default:
	rfbLog("rfbProcessClientAuthType: unknown authentication scheme\n");
	rfbCloseClient(cl);
    }
}


/*
 * Read tunneling type requested by the client (protocol 3.7t).
 * NOTE: Currently, we don't support tunneling, and this function
 *       can never be called.
 */

void
rfbProcessClientTunnelingType(rfbClientPtr cl)
{
    /* If we were called, then something's really wrong. */
    rfbLog("rfbProcessClientTunnelingType: not implemented\n");
    rfbCloseClient(cl);
    return;
}


/*
 * Send the list of our authentication capabilities to the client
 * (protocol 3.7t).
 */

static void
rfbSendAuthCaps(rfbClientPtr cl)
{
    rfbAuthenticationCapsMsg caps;
    rfbCapabilityInfo caplist[MAX_AUTH_CAPS];
    int count = 0;
    rfbTightClientPtr rtcp = rfbGetTightClientData(cl);

    rfbLog("tightvnc-filetransfer/rfbSendAuthCaps\n");

    if(rtcp == NULL)
	return;

    if (cl->screen->authPasswdData && !cl->reverseConnection) {
	/* chk if this condition is valid or not. */
	    SetCapInfo(&caplist[count], rfbAuthVNC, rfbStandardVendor);
	    rtcp->authCaps[count++] = rfbAuthVNC;
    }

    rtcp->nAuthCaps = count;
    caps.nAuthTypes = Swap32IfLE((uint32_t)count);
    if (rfbWriteExact(cl, (char *)&caps, sz_rfbAuthenticationCapsMsg) < 0) {
	rfbLogPerror("rfbSendAuthCaps: write");
	rfbCloseClient(cl);
	return;
    }

    if (count) {
	if (rfbWriteExact(cl, (char *)&caplist[0],
		       count * sz_rfbCapabilityInfo) < 0) {
	    rfbLogPerror("rfbSendAuthCaps: write");
	    rfbCloseClient(cl);
	    return;
	}
	/* Dispatch client input to rfbProcessClientAuthType. */
	/* Call the function for authentication from here */
	rfbProcessClientAuthType(cl);
    } else {
#ifdef USE_SECTYPE_TIGHT_FOR_RFB_3_8
	SECTYPE_TIGHT_FOR_RFB_3_8
#endif
	/* Dispatch client input to rfbProcessClientInitMessage. */
	cl->state = RFB_INITIALISATION;
    }
}


/*
 * Send the list of our tunneling capabilities (protocol 3.7t).
 */

static void
rfbSendTunnelingCaps(rfbClientPtr cl)
{
    rfbTunnelingCapsMsg caps;
    uint32_t nTypes = 0;		/* we don't support tunneling yet */

    rfbLog("tightvnc-filetransfer/rfbSendTunnelingCaps\n");

    caps.nTunnelTypes = Swap32IfLE(nTypes);
    if (rfbWriteExact(cl, (char *)&caps, sz_rfbTunnelingCapsMsg) < 0) {
	rfbLogPerror("rfbSendTunnelingCaps: write");
	rfbCloseClient(cl);
	return;
    }

    if (nTypes) {
	/* Dispatch client input to rfbProcessClientTunnelingType(). */
	/* The flow should not reach here as tunneling is not implemented. */
	rfbProcessClientTunnelingType(cl);
    } else {
	rfbSendAuthCaps(cl);
    }
}



/*
 * rfbSendInteractionCaps is called after sending the server
 * initialisation message, only if TightVNC protocol extensions were
 * enabled (protocol 3.7t). In this function, we send the lists of
 * supported protocol messages and encodings.
 */

/* Update these constants on changing capability lists below! */
/* Values updated for FTP */ 
#define N_SMSG_CAPS  4
#define N_CMSG_CAPS  6
#define N_ENC_CAPS  12

void
rfbSendInteractionCaps(rfbClientPtr cl)
{
    rfbInteractionCapsMsg intr_caps;
    rfbCapabilityInfo smsg_list[N_SMSG_CAPS];
    rfbCapabilityInfo cmsg_list[N_CMSG_CAPS];
    rfbCapabilityInfo enc_list[N_ENC_CAPS];
    int i, n_enc_caps = N_ENC_CAPS;

    /* Fill in the header structure sent prior to capability lists. */
    intr_caps.nServerMessageTypes = Swap16IfLE(N_SMSG_CAPS);
    intr_caps.nClientMessageTypes = Swap16IfLE(N_CMSG_CAPS);
    intr_caps.nEncodingTypes = Swap16IfLE(N_ENC_CAPS);
    intr_caps.pad = 0;

    rfbLog("tightvnc-filetransfer/rfbSendInteractionCaps\n");

    /* Supported server->client message types. */
    /* For file transfer support: */
    i = 0;
	if((IsFileTransferEnabled() == TRUE) && ( cl->viewOnly == FALSE)) {
	    SetCapInfo(&smsg_list[i++], rfbFileListData,           rfbTightVncVendor);
	    SetCapInfo(&smsg_list[i++], rfbFileDownloadData,       rfbTightVncVendor);
	    SetCapInfo(&smsg_list[i++], rfbFileUploadCancel,       rfbTightVncVendor);
	    SetCapInfo(&smsg_list[i++], rfbFileDownloadFailed,     rfbTightVncVendor);
	    if (i != N_SMSG_CAPS) {
			rfbLog("rfbSendInteractionCaps: assertion failed, i != N_SMSG_CAPS\n");
			rfbCloseClient(cl);
			return;
	    }
	}

    /* Supported client->server message types. */
    /* For file transfer support: */
    i = 0;
	if((IsFileTransferEnabled() == TRUE) && ( cl->viewOnly == FALSE)) {
	    SetCapInfo(&cmsg_list[i++], rfbFileListRequest,        rfbTightVncVendor);
	    SetCapInfo(&cmsg_list[i++], rfbFileDownloadRequest,    rfbTightVncVendor);
	    SetCapInfo(&cmsg_list[i++], rfbFileUploadRequest,      rfbTightVncVendor);
	    SetCapInfo(&cmsg_list[i++], rfbFileUploadData,         rfbTightVncVendor);
	    SetCapInfo(&cmsg_list[i++], rfbFileDownloadCancel,     rfbTightVncVendor);
	    SetCapInfo(&cmsg_list[i++], rfbFileUploadFailed,       rfbTightVncVendor);
	    if (i != N_CMSG_CAPS) {
			rfbLog("rfbSendInteractionCaps: assertion failed, i != N_CMSG_CAPS\n");
			rfbCloseClient(cl);
			return;
	    }		
	}
	
    /* Encoding types. */
    i = 0;
    SetCapInfo(&enc_list[i++],  rfbEncodingCopyRect,       rfbStandardVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingRRE,            rfbStandardVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingCoRRE,          rfbStandardVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingHextile,        rfbStandardVendor);
#ifdef LIBVNCSERVER_HAVE_LIBZ
    SetCapInfo(&enc_list[i++],  rfbEncodingZlib,           rfbTridiaVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingTight,          rfbTightVncVendor);
#else
    n_enc_caps -= 2;
#endif
    SetCapInfo(&enc_list[i++],  rfbEncodingCompressLevel0, rfbTightVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingQualityLevel0,  rfbTightVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingXCursor,        rfbTightVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingRichCursor,     rfbTightVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingPointerPos,     rfbTightVncVendor);
    SetCapInfo(&enc_list[i++],  rfbEncodingLastRect,       rfbTightVncVendor);
    if (i != n_enc_caps) {
	rfbLog("rfbSendInteractionCaps: assertion failed, i != N_ENC_CAPS\n");
	rfbCloseClient(cl);
	return;
    }

    /* Send header and capability lists */
    if (rfbWriteExact(cl, (char *)&intr_caps,
		   sz_rfbInteractionCapsMsg) < 0 ||
	rfbWriteExact(cl, (char *)&smsg_list[0],
		   sz_rfbCapabilityInfo * N_SMSG_CAPS) < 0 || 
	rfbWriteExact(cl, (char *)&cmsg_list[0],
		   sz_rfbCapabilityInfo * N_CMSG_CAPS) < 0  ||
	rfbWriteExact(cl, (char *)&enc_list[0],
		   sz_rfbCapabilityInfo * N_ENC_CAPS) < 0) {
	rfbLogPerror("rfbSendInteractionCaps: write");
	rfbCloseClient(cl);	
	return;
    }

    /* Dispatch client input to rfbProcessClientNormalMessage(). */
    cl->state = RFB_NORMAL;
}



rfbBool
rfbTightExtensionInit(rfbClientPtr cl, void* data)
{

   rfbSendInteractionCaps(cl);

    return TRUE;
}

static rfbBool
handleMessage(rfbClientPtr cl,
	const char* messageName,
	void (*handler)(rfbClientPtr cl, rfbTightClientPtr data))
{
	rfbTightClientPtr data;

	rfbLog("tightvnc-filetransfer: %s message received\n", messageName);

	if((IsFileTransferEnabled() == FALSE) || ( cl->viewOnly == TRUE)) {
		rfbCloseClient(cl);
		return FALSE;	
	}

	data = rfbGetTightClientData(cl);
	if(data == NULL)
		return FALSE;

	handler(cl, data);
	return TRUE;
}

rfbBool
rfbTightExtensionMsgHandler(struct _rfbClientRec* cl, void* data,
				const rfbClientToServerMsg* msg)
{
    switch (msg->type) {
		
	case rfbFileListRequest:

	return handleMessage(cl, "rfbFileListRequest", HandleFileListRequest);

	case rfbFileDownloadRequest:

	return handleMessage(cl, "rfbFileDownloadRequest", HandleFileDownloadRequest);

	case rfbFileUploadRequest:	
	
	return handleMessage(cl, "rfbFileUploadRequest", HandleFileUploadRequest);

	case rfbFileUploadData:

	return handleMessage(cl, "rfbFileUploadDataRequest", HandleFileUploadDataRequest);

	case rfbFileDownloadCancel:

	return handleMessage(cl, "rfbFileDownloadCancelRequest", HandleFileDownloadCancelRequest);

	case rfbFileUploadFailed:

	return handleMessage(cl, "rfbFileUploadFailedRequest", HandleFileUploadFailedRequest);

	case rfbFileCreateDirRequest:

	return handleMessage(cl, "rfbFileCreateDirRequest", HandleFileCreateDirRequest);
	
    default:

	rfbLog("rfbProcessClientNormalMessage: unknown message type %d\n",
		msg->type);

	/*

	We shouldn't close the connection here for unhandled msg, 
	it should be left to libvncserver.
	rfbLog(" ... closing connection\n");
	rfbCloseClient(cl);

	*/
	
	return FALSE;

    }
}


void
rfbTightExtensionClientClose(rfbClientPtr cl, void* data) {

	if(data != NULL) {
		CloseUndoneFileUpload(cl, data);
		CloseUndoneFileDownload(cl, data);
		free(data);
	}
}

void
rfbTightUsage(void) {
    fprintf(stderr, "\nlibvncserver-tight-extension options:\n");
    fprintf(stderr, "-disablefiletransfer   disable file transfer\n");
    fprintf(stderr, "-ftproot string        set ftp root\n");
    fprintf(stderr,"\n");
}

int
rfbTightProcessArg(int argc, char *argv[]) {

    rfbLog("tightvnc-filetransfer/rfbTightProcessArg\n");

    InitFileTransfer();

    if(argc<1)
	return 0;

    if (strcmp(argv[0], "-ftproot") == 0) { /* -ftproot string */
	if (2 > argc) {
	    return 0;
	}
	rfbLog("ftproot is set to <%s>\n", argv[1]);
	if(SetFtpRoot(argv[1]) == FALSE) {
	    rfbLog("ERROR:: Path specified for ftproot in invalid\n");
	    return 0;
	}
	return 2;
    } else if (strcmp(argv[0], "-disablefiletransfer") == 0) {
	EnableFileTransfer(FALSE);
	return 1;
    }
    return 0;
}

/*
  * This method should be registered to libvncserver to handle rfbSecTypeTight  security type.
  */
void
rfbHandleSecTypeTight(rfbClientPtr cl) {

    rfbTightClientPtr rtcp = (rfbTightClientPtr) malloc(sizeof(rfbTightClientRec));

    rfbLog("tightvnc-filetransfer/rfbHandleSecTypeTight\n");

    if(rtcp == NULL) {
        /* Error condition close socket */
        rfbLog("Memory error has occurred while handling "
		"Tight security type... closing connection.\n");
	 rfbCloseClient(cl);
	 return;
    }

    memset(rtcp, 0, sizeof(rfbTightClientRec));
    rtcp->rcft.rcfd.downloadFD = -1;
    rtcp->rcft.rcfu.uploadFD = -1;
    rfbEnableExtension(cl, &tightVncFileTransferExtension, rtcp);

    rfbSendTunnelingCaps(cl);

}

rfbProtocolExtension tightVncFileTransferExtension = {
	NULL,
	rfbTightExtensionInit,
	NULL,
	NULL,
	rfbTightExtensionMsgHandler,
	rfbTightExtensionClientClose,
	rfbTightUsage,
	rfbTightProcessArg,
	NULL
};

static rfbSecurityHandler tightVncSecurityHandler = {
	rfbSecTypeTight,
	rfbHandleSecTypeTight,
	NULL
};

void rfbRegisterTightVNCFileTransferExtension(void) {
	rfbRegisterProtocolExtension(&tightVncFileTransferExtension);
	rfbRegisterSecurityHandler(&tightVncSecurityHandler);
        /*
	  Called as well from rfbTightProcessArg(), but only if there are non
	  handled cmdline args. Thus, init file transfer here as well.
	*/
        InitFileTransfer();
}

void 
rfbUnregisterTightVNCFileTransferExtension(void) {
	rfbUnregisterProtocolExtension(&tightVncFileTransferExtension);
	rfbUnregisterSecurityHandler(&tightVncSecurityHandler);
}

