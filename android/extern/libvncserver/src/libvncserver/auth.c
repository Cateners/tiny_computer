/*
 * auth.c - deal with authentication.
 *
 * This file implements the VNC authentication protocol when setting up an RFB
 * connection.
 */

/*
 *  Copyright (C) 2005 Rohit Kumar, Johannes E. Schindelin
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

#include <rfb/rfb.h>

/* RFB 3.8 clients are well informed */
void rfbClientSendString(rfbClientPtr cl, const char *reason);


/*
 * Handle security types
 */

static rfbSecurityHandler* securityHandlers = NULL;

/*
 * This method registers a list of new security types.  
 * It avoids same security type getting registered multiple times. 
 * The order is not preserved if multiple security types are
 * registered at one-go.
 */
void
rfbRegisterSecurityHandler(rfbSecurityHandler* handler)
{
	rfbSecurityHandler *head = securityHandlers, *next = NULL;

	if(handler == NULL)
		return;

	next = handler->next;

	while(head != NULL) {
		if(head == handler) {
			rfbRegisterSecurityHandler(next);
			return;
		}

		head = head->next;
	}

	handler->next = securityHandlers;
	securityHandlers = handler;

	rfbRegisterSecurityHandler(next);
}

/*
 * This method unregisters a list of security types. 
 * These security types won't be available for any new
 * client connection. 
 */
void
rfbUnregisterSecurityHandler(rfbSecurityHandler* handler)
{
	rfbSecurityHandler *cur = NULL, *pre = NULL;

	if(handler == NULL)
		return;

	if(securityHandlers == handler) {
		securityHandlers = securityHandlers->next;
		rfbUnregisterSecurityHandler(handler->next);
		return;
	}

	cur = pre = securityHandlers;

	while(cur) {
		if(cur == handler) {
			pre->next = cur->next;
			break;
		}
		pre = cur;
		cur = cur->next;
	}
	rfbUnregisterSecurityHandler(handler->next);
}

/*
 * Send the authentication challenge.
 */

static void
rfbVncAuthSendChallenge(rfbClientPtr cl)
{
	
    /* 4 byte header is alreay sent. Which is rfbSecTypeVncAuth 
       (same as rfbVncAuth). Just send the challenge. */
    rfbRandomBytes(cl->authChallenge);
    if (rfbWriteExact(cl, (char *)cl->authChallenge, CHALLENGESIZE) < 0) {
        rfbLogPerror("rfbAuthNewClient: write");
        rfbCloseClient(cl);
        return;
    }
    
    /* Dispatch client input to rfbVncAuthProcessResponse. */
    cl->state = RFB_AUTHENTICATION;
}

/*
 * Send the NO AUTHENTICATION. SCARR
 */

/*
 * The rfbVncAuthNone function is currently the only function that contains
 * special logic for the built-in Mac OS X VNC client which is activated by
 * a protocolMinorVersion == 889 coming from the Mac OS X VNC client.
 * The rfbProcessClientInitMessage function does understand how to handle the
 * RFB_INITIALISATION_SHARED state which was introduced to support the built-in
 * Mac OS X VNC client, but rfbProcessClientInitMessage does not examine the
 * protocolMinorVersion version field and so its support for the
 * RFB_INITIALISATION_SHARED state is not restricted to just the OS X client.
 */

static void
rfbVncAuthNone(rfbClientPtr cl)
{
    /* The built-in Mac OS X VNC client behaves in a non-conforming fashion
     * when the server version is 3.7 or later AND the list of security types
     * sent to the OS X client contains the 'None' authentication type AND
     * the OS X client sends back the 'None' type as its choice.  In this case,
     * and this case ONLY, the built-in Mac OS X VNC client will NOT send the
     * ClientInit message and instead will behave as though an implicit
     * ClientInit message containing a shared-flag of true has been sent.
     * The special state RFB_INITIALISATION_SHARED represents this case.
     * The Mac OS X VNC client can be detected by checking protocolMinorVersion
     * for a value of 889.  No other VNC client is known to use this value
     * for protocolMinorVersion. */
    uint32_t authResult;

    /* The built-in Mac OS X VNC client expects to NOT receive a SecurityResult
     * message for authentication type 'None'.  Since its protocolMinorVersion
     * is greater than 7 (it is 889) this case must be tested for specially. */
    if (cl->protocolMajorVersion==3 && cl->protocolMinorVersion > 7 && cl->protocolMinorVersion != 889) {
        rfbLog("rfbProcessClientSecurityType: returning securityResult for client rfb version >= 3.8\n");
        authResult = Swap32IfLE(rfbVncAuthOK);
        if (rfbWriteExact(cl, (char *)&authResult, 4) < 0) {
            rfbLogPerror("rfbAuthProcessClientMessage: write");
            rfbCloseClient(cl);
            return;
        }
    }
    cl->state = cl->protocolMinorVersion == 889 ? RFB_INITIALISATION_SHARED : RFB_INITIALISATION;
    if (cl->state == RFB_INITIALISATION_SHARED)
        /* In this case we must call rfbProcessClientMessage now because
         * otherwise we would hang waiting for data to be received from the
         * client (the ClientInit message which will never come). */
        rfbProcessClientMessage(cl);
    return;
}


/*
 * Advertise the supported security types (protocol 3.7). Here before sending 
 * the list of security types to the client one more security type is added 
 * to the list if primaryType is not set to rfbSecTypeInvalid. This security
 * type is the standard vnc security type which does the vnc authentication
 * or it will be security type for no authentication.
 * Different security types will be added by applications using this library.
 */

static rfbSecurityHandler VncSecurityHandlerVncAuth = {
    rfbSecTypeVncAuth,
    rfbVncAuthSendChallenge,
    NULL
};

static rfbSecurityHandler VncSecurityHandlerNone = {
    rfbSecTypeNone,
    rfbVncAuthNone,
    NULL
};
                        

static void
rfbSendSecurityTypeList(rfbClientPtr cl, int primaryType)
{
    /* The size of the message is the count of security types +1,
     * since the first byte is the number of types. */
    int size = 1;
    rfbSecurityHandler* handler;
#define MAX_SECURITY_TYPES 255
    uint8_t buffer[MAX_SECURITY_TYPES+1];


    /* Fill in the list of security types in the client structure. (NOTE: Not really in the client structure) */
    switch (primaryType) {
    case rfbSecTypeNone:
	rfbUnregisterSecurityHandler(&VncSecurityHandlerVncAuth);
        rfbRegisterSecurityHandler(&VncSecurityHandlerNone);
        break;
    case rfbSecTypeVncAuth:
	rfbUnregisterSecurityHandler(&VncSecurityHandlerNone);
        rfbRegisterSecurityHandler(&VncSecurityHandlerVncAuth);
        break;
    }

    for (handler = securityHandlers;
	    handler && size<MAX_SECURITY_TYPES; handler = handler->next) {
	buffer[size] = handler->type;
	size++;
    }
    buffer[0] = (unsigned char)size-1;

    /* Send the list. */
    if (rfbWriteExact(cl, (char *)buffer, size) < 0) {
	rfbLogPerror("rfbSendSecurityTypeList: write");
	rfbCloseClient(cl);
	return;
    }

    /*
      * if count is 0, we need to send the reason and close the connection.
      */
    if(size <= 1) {
	/* This means total count is Zero and so reason msg should be sent */
	/* The execution should never reach here */
	char* reason = "No authentication mode is registered!";

	rfbClientSendString(cl, reason);
	return;
    }

    /* Dispatch client input to rfbProcessClientSecurityType. */
    cl->state = RFB_SECURITY_TYPE;
}




/*
 * Tell the client what security type will be used (protocol 3.3).
 */
static void
rfbSendSecurityType(rfbClientPtr cl, int32_t securityType)
{
    uint32_t value32;

    /* Send the value. */
    value32 = Swap32IfLE(securityType);
    if (rfbWriteExact(cl, (char *)&value32, 4) < 0) {
	rfbLogPerror("rfbSendSecurityType: write");
	rfbCloseClient(cl);
	return;
    }

    /* Decide what to do next. */
    switch (securityType) {
    case rfbSecTypeNone:
	/* Dispatch client input to rfbProcessClientInitMessage. */
	cl->state = RFB_INITIALISATION;
	break;
    case rfbSecTypeVncAuth:
	/* Begin the standard VNC authentication procedure. */
	rfbVncAuthSendChallenge(cl);
	break;
    default:
	/* Impossible case (hopefully). */
	rfbLogPerror("rfbSendSecurityType: assertion failed");
	rfbCloseClient(cl);
    }
}



/*
 * rfbAuthNewClient is called right after negotiating the protocol
 * version. Depending on the protocol version, we send either a code
 * for authentication scheme to be used (protocol 3.3), or a list of
 * possible "security types" (protocol 3.7).
 */

void
rfbAuthNewClient(rfbClientPtr cl)
{
    int32_t securityType = rfbSecTypeInvalid;

    if (!cl->screen->authPasswdData || cl->reverseConnection) {
	/* chk if this condition is valid or not. */
	securityType = rfbSecTypeNone;
    } else if (cl->screen->authPasswdData) {
 	    securityType = rfbSecTypeVncAuth;
    }

    if (cl->protocolMajorVersion==3 && cl->protocolMinorVersion < 7)
    {
	/* Make sure we use only RFB 3.3 compatible security types. */
	if (securityType == rfbSecTypeInvalid) {
	    rfbLog("VNC authentication disabled - RFB 3.3 client rejected\n");
	    rfbClientConnFailed(cl, "Your viewer cannot handle required "
				"authentication methods");
	    return;
	}
	rfbSendSecurityType(cl, securityType);
    } else {
	/* Here it's ok when securityType is set to rfbSecTypeInvalid. */
	rfbSendSecurityTypeList(cl, securityType);
    }
}

/*
 * Read the security type chosen by the client (protocol 3.7).
 */

void
rfbProcessClientSecurityType(rfbClientPtr cl)
{
    int n;
    uint8_t chosenType;
    rfbSecurityHandler* handler;
    
    /* Read the security type. */
    n = rfbReadExact(cl, (char *)&chosenType, 1);
    if (n <= 0) {
	if (n == 0)
	    rfbLog("rfbProcessClientSecurityType: client gone\n");
	else
	    rfbLogPerror("rfbProcessClientSecurityType: read");
	rfbCloseClient(cl);
	return;
    }

    /* Make sure it was present in the list sent by the server. */
    for (handler = securityHandlers; handler; handler = handler->next) {
	if (chosenType == handler->type) {
	      rfbLog("rfbProcessClientSecurityType: executing handler for type %d\n", chosenType);
	      handler->handler(cl);
	      return;
	}
    }

    rfbLog("rfbProcessClientSecurityType: wrong security type (%d) requested\n", chosenType);
    rfbCloseClient(cl);
}



/*
 * rfbAuthProcessClientMessage is called when the client sends its
 * authentication response.
 */

void
rfbAuthProcessClientMessage(rfbClientPtr cl)
{
    int n;
    uint8_t response[CHALLENGESIZE];
    uint32_t authResult;

    if ((n = rfbReadExact(cl, (char *)response, CHALLENGESIZE)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbAuthProcessClientMessage: read");
        rfbCloseClient(cl);
        return;
    }

    if(!cl->screen->passwordCheck(cl,(const char*)response,CHALLENGESIZE)) {
        rfbErr("rfbAuthProcessClientMessage: password check failed\n");
        authResult = Swap32IfLE(rfbVncAuthFailed);
        if (rfbWriteExact(cl, (char *)&authResult, 4) < 0) {
            rfbLogPerror("rfbAuthProcessClientMessage: write");
        }
	/* support RFB 3.8 clients, they expect a reason *why* it was disconnected */
        if (cl->protocolMinorVersion > 7) {
            rfbClientSendString(cl, "password check failed!");
	}
	else
            rfbCloseClient(cl);
        return;
    }

    authResult = Swap32IfLE(rfbVncAuthOK);

    if (rfbWriteExact(cl, (char *)&authResult, 4) < 0) {
        rfbLogPerror("rfbAuthProcessClientMessage: write");
        rfbCloseClient(cl);
        return;
    }

    cl->state = RFB_INITIALISATION;
}
