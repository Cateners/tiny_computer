/*
 *  Copyright (C) 2000-2002 Constantin Kaplinsky.  All Rights Reserved.
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

/*
 * rfbclient.c - functions to deal with client side of RFB protocol.
 */

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#define _POSIX_SOURCE
#define _XOPEN_SOURCE 600
#endif
#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#endif
#include <errno.h>
#include <rfb/rfbclient.h>
#ifdef WIN32
#undef socklen_t
#endif
#ifdef LIBVNCSERVER_HAVE_LIBZ
#include <zlib.h>
#ifdef __CHECKER__
#undef Z_NULL
#define Z_NULL NULL
#endif
#endif

#ifndef _MSC_VER
/* Strings.h is not available in MSVC */
#include <strings.h>
#endif

#include <stdarg.h>
#include <time.h>

#include "crypto.h"

#include "sasl.h"
#ifdef LIBVNCSERVER_HAVE_LZO
#include <lzo/lzo1x.h>
#else
#include "minilzo.h"
#endif
#include "tls.h"

#define MAX_TEXTCHAT_SIZE 10485760 /* 10MB */

/*
 * rfbClientLog prints a time-stamped message to the log file (stderr).
 */

rfbBool rfbEnableClientLogging=TRUE;

static void
rfbDefaultClientLog(const char *format, ...)
{
    va_list args;
    char buf[256];
    time_t log_clock;

    if(!rfbEnableClientLogging)
      return;

    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %X ", localtime(&log_clock));
    fprintf(stderr, "%s", buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
}

rfbClientLogProc rfbClientLog=rfbDefaultClientLog;
rfbClientLogProc rfbClientErr=rfbDefaultClientLog;

/* extensions */

rfbClientProtocolExtension* rfbClientExtensions = NULL;

void rfbClientRegisterExtension(rfbClientProtocolExtension* e)
{
	e->next = rfbClientExtensions;
	rfbClientExtensions = e;
}

/* client data */

void rfbClientSetClientData(rfbClient* client, void* tag, void* data)
{
	rfbClientData* clientData = client->clientData;

	while(clientData && clientData->tag != tag)
		clientData = clientData->next;
	if(clientData == NULL) {
		clientData = calloc(sizeof(rfbClientData), 1);
		if(clientData == NULL) return;
		clientData->next = client->clientData;
		client->clientData = clientData;
		clientData->tag = tag;
	}

	clientData->data = data;
}

void* rfbClientGetClientData(rfbClient* client, void* tag)
{
	rfbClientData* clientData = client->clientData;

	while(clientData) {
		if(clientData->tag == tag)
			return clientData->data;
		clientData = clientData->next;
	}

	return NULL;
}

static rfbBool HandleRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleCoRRE32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleHextile32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltra8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltra16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltra32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltraZip8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltraZip16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleUltraZip32(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE15(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE24(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE24Up(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE24Down(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTRLE32(rfbClient* client, int rx, int ry, int rw, int rh);
#ifdef LIBVNCSERVER_HAVE_LIBZ
static rfbBool HandleZlib8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZlib16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZlib32(rfbClient* client, int rx, int ry, int rw, int rh);
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
static rfbBool HandleTight8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTight16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleTight32(rfbClient* client, int rx, int ry, int rw, int rh);

static long ReadCompactLen (rfbClient* client);
#endif
static rfbBool HandleZRLE8(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE15(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE16(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE24(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE24Up(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE24Down(rfbClient* client, int rx, int ry, int rw, int rh);
static rfbBool HandleZRLE32(rfbClient* client, int rx, int ry, int rw, int rh);
#endif

/*
 * Server Capability Functions
 */
rfbBool
SupportsClient2Server(rfbClient* client, int messageType)
{
    return (client->supportedMessages.client2server[((messageType & 0xFF)/8)] & (1<<(messageType % 8)) ? TRUE : FALSE);
}

rfbBool
SupportsServer2Client(rfbClient* client, int messageType)
{
    return (client->supportedMessages.server2client[((messageType & 0xFF)/8)] & (1<<(messageType % 8)) ? TRUE : FALSE);
}

void
SetClient2Server(rfbClient* client, int messageType)
{
  client->supportedMessages.client2server[((messageType & 0xFF)/8)] |= (1<<(messageType % 8));
}

void
SetServer2Client(rfbClient* client, int messageType)
{
  client->supportedMessages.server2client[((messageType & 0xFF)/8)] |= (1<<(messageType % 8));
}

void
ClearClient2Server(rfbClient* client, int messageType)
{
  client->supportedMessages.client2server[((messageType & 0xFF)/8)] &= ~(1<<(messageType % 8));
}

void
ClearServer2Client(rfbClient* client, int messageType)
{
  client->supportedMessages.server2client[((messageType & 0xFF)/8)] &= ~(1<<(messageType % 8));
}


void
DefaultSupportedMessages(rfbClient* client)
{
    memset((char *)&client->supportedMessages,0,sizeof(client->supportedMessages));

    /* Default client supported messages (universal RFB 3.3 protocol) */
    SetClient2Server(client, rfbSetPixelFormat);
    /* SetClient2Server(client, rfbFixColourMapEntries); Not currently supported */
    SetClient2Server(client, rfbSetEncodings);
    SetClient2Server(client, rfbFramebufferUpdateRequest);
    SetClient2Server(client, rfbKeyEvent);
    SetClient2Server(client, rfbPointerEvent);
    SetClient2Server(client, rfbClientCutText);
    /* technically, we only care what we can *send* to the server
     * but, we set Server2Client Just in case it ever becomes useful
     */
    SetServer2Client(client, rfbFramebufferUpdate);
    SetServer2Client(client, rfbSetColourMapEntries);
    SetServer2Client(client, rfbBell);
    SetServer2Client(client, rfbServerCutText);
}

void
DefaultSupportedMessagesUltraVNC(rfbClient* client)
{
    DefaultSupportedMessages(client);
    SetClient2Server(client, rfbFileTransfer);
    SetClient2Server(client, rfbSetScale);
    SetClient2Server(client, rfbSetServerInput);
    SetClient2Server(client, rfbSetSW);
    SetClient2Server(client, rfbTextChat);
    SetClient2Server(client, rfbPalmVNCSetScaleFactor);
    /* technically, we only care what we can *send* to the server */
    SetServer2Client(client, rfbResizeFrameBuffer);
    SetServer2Client(client, rfbPalmVNCReSizeFrameBuffer);
    SetServer2Client(client, rfbFileTransfer);
    SetServer2Client(client, rfbTextChat);
}


void
DefaultSupportedMessagesTightVNC(rfbClient* client)
{
    DefaultSupportedMessages(client);
    SetClient2Server(client, rfbFileTransfer);
    SetClient2Server(client, rfbSetServerInput);
    SetClient2Server(client, rfbSetSW);
    /* SetClient2Server(client, rfbTextChat); */
    /* technically, we only care what we can *send* to the server */
    SetServer2Client(client, rfbFileTransfer);
    SetServer2Client(client, rfbTextChat);
}

#ifndef WIN32
static rfbBool
IsUnixSocket(const char *name)
{
  struct stat sb;
  if(stat(name, &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFSOCK)
    return TRUE;
  return FALSE;
}
#endif

/*
 * ConnectToRFBServer.
 */

rfbBool
ConnectToRFBServer(rfbClient* client,const char *hostname, int port)
{
  if (client->serverPort==-1) {
    /* serverHost is a file recorded by vncrec. */
    const char* magic="vncLog0.0";
    char buffer[10];
    rfbVNCRec* rec = (rfbVNCRec*)malloc(sizeof(rfbVNCRec));
    if(!rec) {
        rfbClientLog("Could not allocate rfbVNCRec memory\n");
        return FALSE;
    }
    client->vncRec = rec;

    rec->file = fopen(client->serverHost,"rb");
    rec->tv.tv_sec = 0;
    rec->readTimestamp = FALSE;
    rec->doNotSleep = FALSE;
    
    if (!rec->file) {
      rfbClientLog("Could not open %s.\n",client->serverHost);
      return FALSE;
    }
    setbuf(rec->file,NULL);

    if (fread(buffer,1,strlen(magic),rec->file) != strlen(magic) || strncmp(buffer,magic,strlen(magic))) {
      rfbClientLog("File %s was not recorded by vncrec.\n",client->serverHost);
      fclose(rec->file);
      return FALSE;
    }
    client->sock = RFB_INVALID_SOCKET;
    return TRUE;
  }

#ifndef WIN32
  if(IsUnixSocket(hostname))
    /* serverHost is a UNIX socket. */
    client->sock = ConnectClientToUnixSockWithTimeout(hostname, client->connectTimeout);
  else
#endif
  {
#ifdef LIBVNCSERVER_IPv6
    client->sock = ConnectClientToTcpAddr6WithTimeout(hostname, port, client->connectTimeout);
#else
    unsigned int host;

    /* serverHost is a hostname */
    if (!StringToIPAddr(hostname, &host)) {
      rfbClientLog("Couldn't convert '%s' to host address\n", hostname);
      return FALSE;
    }
    client->sock = ConnectClientToTcpAddrWithTimeout(host, port, client->connectTimeout);
#endif
  }

  if (client->sock == RFB_INVALID_SOCKET) {
    rfbClientLog("Unable to connect to VNC server\n");
    return FALSE;
  }

  if(client->QoS_DSCP && !SetDSCP(client->sock, client->QoS_DSCP))
     return FALSE;

  return TRUE;
}

/*
 * ConnectToRFBRepeater.
 */

rfbBool ConnectToRFBRepeater(rfbClient* client,const char *repeaterHost, int repeaterPort, const char *destHost, int destPort)
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  char tmphost[250];

#ifdef LIBVNCSERVER_IPv6
  client->sock = ConnectClientToTcpAddr6WithTimeout(repeaterHost, repeaterPort, client->connectTimeout);
#else
  unsigned int host;
  if (!StringToIPAddr(repeaterHost, &host)) {
    rfbClientLog("Couldn't convert '%s' to host address\n", repeaterHost);
    return FALSE;
  }

  client->sock = ConnectClientToTcpAddrWithTimeout(host, repeaterPort, client->connectTimeout);
#endif

  if (client->sock == RFB_INVALID_SOCKET) {
    rfbClientLog("Unable to connect to VNC repeater\n");
    return FALSE;
  }

  if (!ReadFromRFBServer(client, pv, sz_rfbProtocolVersionMsg))
    return FALSE;
  pv[sz_rfbProtocolVersionMsg] = 0;

  /* UltraVNC repeater always report version 000.000 to identify itself */
  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2 || major != 0 || minor != 0) {
    rfbClientLog("Not a valid VNC repeater (%s)\n",pv);
    return FALSE;
  }

  rfbClientLog("Connected to VNC repeater, using protocol version %d.%d\n", major, minor);

  memset(tmphost, 0, sizeof(tmphost));
  if(snprintf(tmphost, sizeof(tmphost), "%s:%d", destHost, destPort) >= (int)sizeof(tmphost))
    return FALSE; /* output truncated */
  if (!WriteToRFBServer(client, tmphost, sizeof(tmphost)))
    return FALSE;

  return TRUE;
}

extern void rfbClientEncryptBytes(unsigned char* bytes, char* passwd);
extern void rfbClientEncryptBytes2(unsigned char *where, const int length, unsigned char *key);

static void
ReadReason(rfbClient* client)
{
    uint32_t reasonLen;
    char *reason;

    if (!ReadFromRFBServer(client, (char *)&reasonLen, 4)) return;
    reasonLen = rfbClientSwap32IfLE(reasonLen);
    if(reasonLen > 1<<20) {
      rfbClientLog("VNC connection failed, but sent reason length of %u exceeds limit of 1MB",(unsigned int)reasonLen);
      return;
    }
    reason = malloc(reasonLen+1);
    if (!reason || !ReadFromRFBServer(client, reason, reasonLen)) { free(reason); return; }
    reason[reasonLen]=0;
    rfbClientLog("VNC connection failed: %s\n",reason);
    free(reason);
}

rfbBool
rfbHandleAuthResult(rfbClient* client)
{
    uint32_t authResult=0;

    if (!ReadFromRFBServer(client, (char *)&authResult, 4)) return FALSE;

    authResult = rfbClientSwap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      rfbClientLog("VNC authentication succeeded\n");
      return TRUE;
      break;
    case rfbVncAuthFailed:
      errno = EACCES;
      if (client->major==3 && client->minor>7)
      {
        /* we have an error following */
        ReadReason(client);
        return FALSE;
      }
      rfbClientLog("VNC authentication failed\n");
      return FALSE;
    case rfbVncAuthTooMany:
      errno = EACCES;
      rfbClientLog("VNC authentication failed - too many tries\n");
      return FALSE;
    }

    errno = EACCES;
    rfbClientLog("Unknown VNC authentication result: %d\n",
                 (int)authResult);
    return FALSE;
}


static rfbBool
ReadSupportedSecurityType(rfbClient* client, uint32_t *result, rfbBool subAuth)
{
    uint8_t count=0;
    uint8_t loop=0;
    uint8_t flag=0;
    rfbBool extAuthHandler;
    uint8_t tAuth[256];
    char buf1[500],buf2[10];
    uint32_t authScheme;
    rfbClientProtocolExtension* e;

    if (!ReadFromRFBServer(client, (char *)&count, 1)) return FALSE;

    if (count==0)
    {
        rfbClientLog("List of security types is ZERO, expecting an error to follow\n");
        ReadReason(client);
        return FALSE;
    }

    rfbClientLog("We have %d security types to read\n", count);
    authScheme=0;
    /* now, we have a list of available security types to read ( uint8_t[] ) */
    for (loop=0;loop<count;loop++)
    {
        if (!ReadFromRFBServer(client, (char *)&tAuth[loop], 1)) return FALSE;
        rfbClientLog("%d) Received security type %d\n", loop, tAuth[loop]);

		switch (tAuth[loop]) {
		case rfbUltra:
			rfbClientLog("UltraVNC server detected, enabling UltraVNC specific messages\n");
			DefaultSupportedMessagesUltraVNC(client);
			break;
		case rfbTight:
			rfbClientLog("TightVNC server detected, enabling TightVNC specific messages\n");
			DefaultSupportedMessagesTightVNC(client);
			break;
		}

        if (flag) continue;
        extAuthHandler=FALSE;
        for (e = rfbClientExtensions; e; e = e->next) {
            if (!e->handleAuthentication) continue;
            uint32_t const* secType;
            for (secType = e->securityTypes; secType && *secType; secType++) {
                if (tAuth[loop]==*secType) {
                    extAuthHandler=TRUE;
                }
            }
        }
        if (tAuth[loop]==rfbVncAuth || tAuth[loop]==rfbNoAuth ||
			extAuthHandler ||
#if defined(LIBVNCSERVER_HAVE_GNUTLS) || defined(LIBVNCSERVER_HAVE_LIBSSL)
	    (!subAuth && (tAuth[loop]==rfbTLS || tAuth[loop]==rfbVeNCrypt)) ||
#endif
#ifdef LIBVNCSERVER_HAVE_SASL
            tAuth[loop]==rfbSASL ||
#endif /* LIBVNCSERVER_HAVE_SASL */
            ((tAuth[loop]==rfbARD || tAuth[loop]==rfbUltraMSLogonII) && client->GetCredential))
        {
            if (!subAuth && client->clientAuthSchemes)
            {
                int i;
                for (i=0;client->clientAuthSchemes[i];i++)
                {
                    if (client->clientAuthSchemes[i]==(uint32_t)tAuth[loop])
                    {
                        flag++;
                        authScheme=tAuth[loop];
                        break;
                    }
                }
            }
            else
            {
                flag++;
                authScheme=tAuth[loop];
            }
            if (flag)
            {
                rfbClientLog("Selecting security type %d (%d/%d in the list)\n", authScheme, loop, count);
                /* send back a single byte indicating which security type to use */
                if (!WriteToRFBServer(client, (char *)&tAuth[loop], 1)) return FALSE;
            }
        }
    }
    if (authScheme==0)
    {
        memset(buf1, 0, sizeof(buf1));
        for (loop=0;loop<count;loop++)
        {
            if (strlen(buf1)>=sizeof(buf1)-1) break;
            snprintf(buf2, sizeof(buf2), (loop>0 ? ", %d" : "%d"), (int)tAuth[loop]);
            strncat(buf1, buf2, sizeof(buf1)-strlen(buf1)-1);
        }
        errno = EACCES;
        rfbClientLog("Unknown authentication scheme from VNC server: %s\n",
               buf1);
        return FALSE;
    }
    *result = authScheme;
    return TRUE;
}

static rfbBool
HandleVncAuth(rfbClient *client)
{
    uint8_t challenge[CHALLENGESIZE];
    char *passwd=NULL;
    int i;

    if (!ReadFromRFBServer(client, (char *)challenge, CHALLENGESIZE)) return FALSE;

    if (client->serverPort!=-1) { /* if not playing a vncrec file */
      if (client->GetPassword)
        passwd = client->GetPassword(client);

      if ((!passwd) || (strlen(passwd) == 0)) {
        rfbClientLog("Reading password failed\n");
        return FALSE;
      }
      if (strlen(passwd) > 8) {
        passwd[8] = '\0';
      }

      rfbClientEncryptBytes(challenge, passwd);

      /* Lose the password from memory */
      for (i = strlen(passwd); i >= 0; i--) {
        passwd[i] = '\0';
      }
      free(passwd);

      if (!WriteToRFBServer(client, (char *)challenge, CHALLENGESIZE)) return FALSE;
    }

    /* Handle the SecurityResult message */
    if (!rfbHandleAuthResult(client)) return FALSE;

    return TRUE;
}

static void
FreeUserCredential(rfbCredential *cred)
{
  if (cred->userCredential.username) free(cred->userCredential.username);
  if (cred->userCredential.password) free(cred->userCredential.password);
  free(cred);
}

static rfbBool
HandlePlainAuth(rfbClient *client)
{
  uint32_t ulen, ulensw;
  uint32_t plen, plensw;
  rfbCredential *cred;

  if (!client->GetCredential)
  {
    rfbClientLog("GetCredential callback is not set.\n");
    return FALSE;
  }
  cred = client->GetCredential(client, rfbCredentialTypeUser);
  if (!cred)
  {
    rfbClientLog("Reading credential failed\n");
    return FALSE;
  }

  ulen = (cred->userCredential.username ? strlen(cred->userCredential.username) : 0);
  ulensw = rfbClientSwap32IfLE(ulen);
  plen = (cred->userCredential.password ? strlen(cred->userCredential.password) : 0);
  plensw = rfbClientSwap32IfLE(plen);
  if (!WriteToRFBServer(client, (char *)&ulensw, 4) ||
      !WriteToRFBServer(client, (char *)&plensw, 4))
  {
    FreeUserCredential(cred);
    return FALSE;
  }
  if (ulen > 0)
  {
    if (!WriteToRFBServer(client, cred->userCredential.username, ulen))
    {
      FreeUserCredential(cred);
      return FALSE;
    }
  }
  if (plen > 0)
  {
    if (!WriteToRFBServer(client, cred->userCredential.password, plen))
    {
      FreeUserCredential(cred);
      return FALSE;
    }
  }

  FreeUserCredential(cred);

  /* Handle the SecurityResult message */
  if (!rfbHandleAuthResult(client)) return FALSE;

  return TRUE;
}

/* Simple 64bit big integer arithmetic implementation */
/* (x + y) % m, works even if (x + y) > 64bit */
#define rfbAddM64(x,y,m) ((x+y)%m+(x+y<x?(((uint64_t)-1)%m+1)%m:0))
/* (x * y) % m */
static uint64_t
rfbMulM64(uint64_t x, uint64_t y, uint64_t m)
{
  uint64_t r;
  for(r=0;x>0;x>>=1)
  {
    if (x&1) r=rfbAddM64(r,y,m);
    y=rfbAddM64(y,y,m);
  }
  return r;
}
/* (x ^ y) % m */
static uint64_t
rfbPowM64(uint64_t b, uint64_t e, uint64_t m)
{
  uint64_t r;
  for(r=1;e>0;e>>=1)
  {
    if(e&1) r=rfbMulM64(r,b,m);
    b=rfbMulM64(b,b,m);
  }
  return r;
}

static rfbBool
HandleUltraMSLogonIIAuth(rfbClient *client)
{
  uint8_t gen[8], mod[8], resp[8], pub[8], priv[8];
  uint8_t username[256], password[64], key[8];
  rfbCredential *cred;

  if (!ReadFromRFBServer(client, (char *)gen, sizeof(gen))) return FALSE;
  if (!ReadFromRFBServer(client, (char *)mod, sizeof(mod))) return FALSE;
  if (!ReadFromRFBServer(client, (char *)resp, sizeof(resp))) return FALSE;

  if(!dh_generate_keypair(priv, pub, gen, sizeof(gen), mod, sizeof(priv))) {
      rfbClientErr("HandleUltraMSLogonIIAuth: generating keypair failed\n");
      return FALSE;
  }

  if(!dh_compute_shared_key(key, priv, resp, mod, sizeof(key))) {
      rfbClientErr("HandleUltraMSLogonIIAuth: creating shared key failed\n");
      return FALSE;
  }

  if (!client->GetCredential)
  {
    rfbClientLog("GetCredential callback is not set.\n");
    return FALSE;
  }
  rfbClientLog("WARNING! MSLogon security type has very low password encryption! "\
    "Use it only with SSH tunnel or trusted network.\n");
  cred = client->GetCredential(client, rfbCredentialTypeUser);
  if (!cred)
  {
    rfbClientLog("Reading credential failed\n");
    return FALSE;
  }

  memset(username, 0, sizeof(username));
  strncpy((char *)username, cred->userCredential.username, sizeof(username)-1);
  memset(password, 0, sizeof(password));
  strncpy((char *)password, cred->userCredential.password, sizeof(password)-1);
  FreeUserCredential(cred);

  rfbClientEncryptBytes2(username, sizeof(username), (unsigned char *)key);
  rfbClientEncryptBytes2(password, sizeof(password), (unsigned char *)key);

  if (!WriteToRFBServer(client, (char *)pub, sizeof(pub))) return FALSE;
  if (!WriteToRFBServer(client, (char *)username, sizeof(username))) return FALSE;
  if (!WriteToRFBServer(client, (char *)password, sizeof(password))) return FALSE;

  /* Handle the SecurityResult message */
  if (!rfbHandleAuthResult(client)) return FALSE;

  return TRUE;
}

static rfbBool
HandleMSLogonAuth(rfbClient *client)
{
  uint64_t gen, mod, resp, priv, pub, key;
  uint8_t username[256], password[64];
  rfbCredential *cred;

  if (!ReadFromRFBServer(client, (char *)&gen, 8)) return FALSE;
  if (!ReadFromRFBServer(client, (char *)&mod, 8)) return FALSE;
  if (!ReadFromRFBServer(client, (char *)&resp, 8)) return FALSE;
  gen = rfbClientSwap64IfLE(gen);
  mod = rfbClientSwap64IfLE(mod);
  resp = rfbClientSwap64IfLE(resp);

  if (!client->GetCredential)
  {
    rfbClientLog("GetCredential callback is not set.\n");
    return FALSE;
  }
  rfbClientLog("WARNING! MSLogon security type has very low password encryption! "\
    "Use it only with SSH tunnel or trusted network.\n");
  cred = client->GetCredential(client, rfbCredentialTypeUser);
  if (!cred)
  {
    rfbClientLog("Reading credential failed\n");
    return FALSE;
  }

  memset(username, 0, sizeof(username));
  strncpy((char *)username, cred->userCredential.username, sizeof(username)-1);
  memset(password, 0, sizeof(password));
  strncpy((char *)password, cred->userCredential.password, sizeof(password)-1);
  FreeUserCredential(cred);

  srand(time(NULL));
  priv = ((uint64_t)rand())<<32;
  priv |= (uint64_t)rand();

  pub = rfbPowM64(gen, priv, mod);
  key = rfbPowM64(resp, priv, mod);
  pub = rfbClientSwap64IfLE(pub);
  key = rfbClientSwap64IfLE(key);

  rfbClientEncryptBytes2(username, sizeof(username), (unsigned char *)&key);
  rfbClientEncryptBytes2(password, sizeof(password), (unsigned char *)&key);

  if (!WriteToRFBServer(client, (char *)&pub, 8)) return FALSE;
  if (!WriteToRFBServer(client, (char *)username, sizeof(username))) return FALSE;
  if (!WriteToRFBServer(client, (char *)password, sizeof(password))) return FALSE;

  /* Handle the SecurityResult message */
  if (!rfbHandleAuthResult(client)) return FALSE;

  return TRUE;
}


static rfbBool
HandleARDAuth(rfbClient *client)
{
  uint8_t gen[2], len[2];
  size_t keylen;
  uint8_t *mod = NULL, *resp = NULL, *priv = NULL, *pub = NULL, *key = NULL, *shared = NULL;
  uint8_t userpass[128], ciphertext[128];
  int ciphertext_len;
  int passwordLen, usernameLen;
  rfbCredential *cred = NULL;
  rfbBool result = FALSE;

  /* Step 1: Read the authentication material from the socket.
     A two-byte generator value, a two-byte key length value. */
  if (!ReadFromRFBServer(client, (char *)gen, 2)) {
      rfbClientErr("HandleARDAuth: reading generator value failed\n");
      goto out;
  }
  if (!ReadFromRFBServer(client, (char *)len, 2)) {
      rfbClientErr("HandleARDAuth: reading key length failed\n");
      goto out;
  }
  keylen = 256*len[0]+len[1]; /* convert from char[] to int */

  mod = (uint8_t*)malloc(keylen*5); /* the block actually contains mod, resp, pub, priv and key */
  if (!mod)
      goto out;

  resp = mod+keylen;
  pub = resp+keylen;
  priv = pub+keylen;
  key = priv+keylen;

  /* Step 1: Read the authentication material from the socket.
     The prime modulus (keylen bytes) and the peer's generated public key (keylen bytes). */
  if (!ReadFromRFBServer(client, (char *)mod, keylen)) {
      rfbClientErr("HandleARDAuth: reading prime modulus failed\n");
      goto out;
  }
  if (!ReadFromRFBServer(client, (char *)resp, keylen)) {
      rfbClientErr("HandleARDAuth: reading peer's generated public key failed\n");
      goto out;
  }

  /* Step 2: Generate own Diffie-Hellman public-private key pair. */
  if(!dh_generate_keypair(priv, pub, gen, 2, mod, keylen)) {
      rfbClientErr("HandleARDAuth: generating keypair failed\n");
      goto out;
  }

  /* Step 3: Perform Diffie-Hellman key agreement, using the generator (gen),
     prime (mod), and the peer's public key. The output will be a shared
     secret known to both us and the peer. */
  if(!dh_compute_shared_key(key, priv, resp, mod, keylen)) {
      rfbClientErr("HandleARDAuth: creating shared key failed\n");
      goto out;
  }

  /* Step 4: Perform an MD5 hash of the shared secret.
     This 128-bit (16-byte) value will be used as the AES key. */
  shared = malloc(MD5_HASH_SIZE);
  if(!hash_md5(shared, key, keylen)) {
      rfbClientErr("HandleARDAuth: hashing shared key failed\n");
      goto out;
  }

  /* Step 5: Pack the username and password into a 128-byte
     plaintext "userpass" structure: { username[64], password[64] }.
     Null-terminate each. Fill the unused bytes with random characters
     so that the encryption output is less predictable. */
  if(!client->GetCredential) {
      rfbClientErr("HandleARDAuth: GetCredential callback is not set\n");
      goto out;
  }
  cred = client->GetCredential(client, rfbCredentialTypeUser);
  if(!cred) {
      rfbClientErr("HandleARDAuth: reading credential failed\n");
      goto out;
  }
  passwordLen = strlen(cred->userCredential.password)+1;
  usernameLen = strlen(cred->userCredential.username)+1;
  if (passwordLen > sizeof(userpass)/2)
      passwordLen = sizeof(userpass)/2;
  if (usernameLen > sizeof(userpass)/2)
      usernameLen = sizeof(userpass)/2;
  random_bytes(userpass, sizeof(userpass));
  memcpy(userpass, cred->userCredential.username, usernameLen);
  memcpy(userpass+sizeof(userpass)/2, cred->userCredential.password, passwordLen);

  /* Step 6: Encrypt the plaintext credentials with the 128-bit MD5 hash
     from step 4, using the AES 128-bit symmetric cipher in electronic
     codebook (ECB) mode. Use no further padding for this block cipher. */
  if(!encrypt_aes128ecb(ciphertext, &ciphertext_len, shared, userpass, sizeof(userpass))) {
      rfbClientErr("HandleARDAuth: encrypting credentials failed\n");
      goto out;
  }

  /* Step 7: Write the ciphertext from step 6 to the stream.
     Write the generated DH public key to the stream. */
  if (!WriteToRFBServer(client, (char *)ciphertext, sizeof(ciphertext)))
      goto out;
  if (!WriteToRFBServer(client, (char *)pub, keylen))
      goto out;

  /* Handle the SecurityResult message */
  if (!rfbHandleAuthResult(client))
      goto out;

  result = TRUE;

 out:
  if (cred)
    FreeUserCredential(cred);

  free(mod);
  free(shared);

  return result;
}



/*
 * SetClientAuthSchemes.
 */

void
SetClientAuthSchemes(rfbClient* client,const uint32_t *authSchemes, int size)
{
  int i;

  if (client->clientAuthSchemes)
  {
    free(client->clientAuthSchemes);
    client->clientAuthSchemes = NULL;
  }
  if (authSchemes)
  {
    if (size<0)
    {
      /* If size<0 we assume the passed-in list is also 0-terminate, so we
       * calculate the size here */
      for (size=0;authSchemes[size];size++) ;
    }
    client->clientAuthSchemes = (uint32_t*)malloc(sizeof(uint32_t)*(size+1));
    if (client->clientAuthSchemes) {
      for (i=0;i<size;i++)
        client->clientAuthSchemes[i] = authSchemes[i];
      client->clientAuthSchemes[size] = 0;
    }
  }
}

/*
 * InitialiseRFBConnection.
 */

rfbBool
InitialiseRFBConnection(rfbClient* client)
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  uint32_t authScheme;
  uint32_t subAuthScheme;
  rfbClientInitMsg ci;

  /* if the connection is immediately closed, don't report anything, so
       that pmw's monitor can make test connections */

  if (client->listenSpecified)
    errorMessageOnReadFailure = FALSE;

  if (!ReadFromRFBServer(client, pv, sz_rfbProtocolVersionMsg)) return FALSE;

    errorMessageOnReadFailure = TRUE;

    pv[sz_rfbProtocolVersionMsg] = 0;

    if (sscanf(pv, rfbProtocolVersionFormat, &major, &minor) != 2) {
        rfbClientLog("Not a valid VNC server (%s)\n", pv);
        errno = EPROTO;
        return FALSE;
    }

    client->serverMajor = major;
    client->serverMinor = minor;
    rfbClientLog("Received protocol version %d.%d\n", major, minor);

    DefaultSupportedMessages(client);
    client->major = major;
    client->minor = minor;

    /* fall back to viewer supported version */
    if ((major == rfbProtocolMajorVersion) && (minor > rfbProtocolMinorVersion))
        client->minor = rfbProtocolMinorVersion;

    /* Legacy version of UltraVNC uses minor codes 4 and 6 for the server */
  /* left in for backwards compatibility */
  if (major==3 && (minor==4 || minor==6)) {
      rfbClientLog("UltraVNC server detected, enabling UltraVNC specific messages\n",pv);
      DefaultSupportedMessagesUltraVNC(client);
  }

  /* Legacy version of UltraVNC Single Click uses minor codes 14 and 16 for the server */
  /* left in for backwards compatibility */
  if (major==3 && (minor==14 || minor==16)) {
     minor = minor - 10;
     client->minor = minor;
     rfbClientLog("UltraVNC Single Click server detected, enabling UltraVNC specific messages\n",pv);
     DefaultSupportedMessagesUltraVNC(client);
  }

  /* Legacy version of TightVNC uses minor codes 5 for the server */
  /* left in for backwards compatibility */
  if (major==3 && minor==5) {
      rfbClientLog("TightVNC server detected, enabling TightVNC specific messages\n",pv);
      DefaultSupportedMessagesTightVNC(client);
  }

  /* we do not support > RFB3.8 */
  if ((major==3 && minor>8) || major>3)
  {
    client->major=3;
    client->minor=8;
  }

  rfbClientLog("VNC server supports protocol version %d.%d (viewer %d.%d)\n",
	  major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  sprintf(pv,rfbProtocolVersionFormat,client->major,client->minor);

  if (!WriteToRFBServer(client, pv, sz_rfbProtocolVersionMsg)) return FALSE;


  /* 3.7 and onwards sends a # of security types first */
  if (client->major==3 && client->minor > 6)
  {
    if (!ReadSupportedSecurityType(client, &authScheme, FALSE)) return FALSE;
  }
  else
  {
    if (!ReadFromRFBServer(client, (char *)&authScheme, 4)) return FALSE;
    authScheme = rfbClientSwap32IfLE(authScheme);
  }
  
  rfbClientLog("Selected Security Scheme %d\n", authScheme);
  client->authScheme = authScheme;
  
  switch (authScheme) {

  case rfbConnFailed:
    ReadReason(client);
    return FALSE;

  case rfbNoAuth:
    rfbClientLog("No authentication needed\n");

    /* 3.8 and upwards sends a Security Result for rfbNoAuth */
    if ((client->major==3 && client->minor > 7) || client->major>3)
        if (!rfbHandleAuthResult(client)) return FALSE;        

    break;

  case rfbVncAuth:
    if (!HandleVncAuth(client)) return FALSE;
    break;

#ifdef LIBVNCSERVER_HAVE_SASL
  case rfbSASL:
    if (!HandleSASLAuth(client)) return FALSE;
    break;
#endif /* LIBVNCSERVER_HAVE_SASL */

  case rfbUltraMSLogonII:
    if (!HandleUltraMSLogonIIAuth(client)) return FALSE;
    break;

  case rfbMSLogon:
    if (!HandleMSLogonAuth(client)) return FALSE;
    break;

  case rfbARD:
    if (!HandleARDAuth(client)) return FALSE;
    break;

  case rfbTLS:
    if (!HandleAnonTLSAuth(client)) return FALSE;
    /* After the TLS session is established, sub auth types are expected.
     * Note that all following reading/writing are through the TLS session from here.
     */
    if (!ReadSupportedSecurityType(client, &subAuthScheme, TRUE)) return FALSE;
    client->subAuthScheme = subAuthScheme;

    switch (subAuthScheme) {

      case rfbConnFailed:
        ReadReason(client);
        return FALSE;

      case rfbNoAuth:
        rfbClientLog("No sub authentication needed\n");
        /* 3.8 and upwards sends a Security Result for rfbNoAuth */
        if ((client->major==3 && client->minor > 7) || client->major>3)
            if (!rfbHandleAuthResult(client)) return FALSE;
        break;

      case rfbVncAuth:
        if (!HandleVncAuth(client)) return FALSE;
        break;

#ifdef LIBVNCSERVER_HAVE_SASL
      case rfbSASL:
        if (!HandleSASLAuth(client)) return FALSE;
        break;
#endif /* LIBVNCSERVER_HAVE_SASL */

      default:
        rfbClientLog("Unknown sub authentication scheme from VNC server: %d\n",
            (int)subAuthScheme);
        return FALSE;
    }

    break;

  case rfbVeNCrypt:
    if (!HandleVeNCryptAuth(client)) return FALSE;

    switch (client->subAuthScheme) {
      /*
       * rfbNoAuth and rfbVncAuth are not actually part of VeNCrypt, however
       * it is important to support them to ensure better compatibility.
       * When establishing a connection, the client does not know whether
       * the server supports encryption, and always prefers VeNCrypt if enabled.
       * Next, if encryption is not available on the server, the connection
       * will fail. Since the RFB doesn't have any downgrade methods in case
       * of failure, a client that does not support unencrypted VeNCrypt methods
       * will never be able to connect.
       *
       * The RFB specification also considers any ordinary subauths are valid,
       * which legitimizes this solution.
       *
       * rfbVeNCryptPlain is also supported for better compatibility.
       */

      case rfbNoAuth:
      case rfbVeNCryptTLSNone:
      case rfbVeNCryptX509None:
        rfbClientLog("No sub authentication needed\n");
        if (!rfbHandleAuthResult(client)) return FALSE;
        break;

      case rfbVncAuth:
      case rfbVeNCryptTLSVNC:
      case rfbVeNCryptX509VNC:
        if (!HandleVncAuth(client)) return FALSE;
        break;

      case rfbVeNCryptPlain:
      case rfbVeNCryptTLSPlain:
      case rfbVeNCryptX509Plain:
        if (!HandlePlainAuth(client)) return FALSE;
        break;

#ifdef LIBVNCSERVER_HAVE_SASL
      case rfbVeNCryptX509SASL:
      case rfbVeNCryptTLSSASL:
        if (!HandleSASLAuth(client)) return FALSE;
        break;
#endif /* LIBVNCSERVER_HAVE_SASL */

      default:
        rfbClientLog("Unknown sub authentication scheme from VNC server: %d\n",
            client->subAuthScheme);
        return FALSE;
    }

    break;

  default:
    {
      rfbBool authHandled=FALSE;
      rfbClientProtocolExtension* e;
      for (e = rfbClientExtensions; e; e = e->next) {
        uint32_t const* secType;
        if (!e->handleAuthentication) continue;
        for (secType = e->securityTypes; secType && *secType; secType++) {
          if (authScheme==*secType) {
            if (!e->handleAuthentication(client, authScheme)) return FALSE;
            if (!rfbHandleAuthResult(client)) return FALSE;
            authHandled=TRUE;
          }
        }
      }
      if (authHandled) break;
    }
    rfbClientLog("Unknown authentication scheme from VNC server: %d\n",
	    (int)authScheme);
    return FALSE;
  }

  ci.shared = (client->appData.shareDesktop ? 1 : 0);

  if (!WriteToRFBServer(client,  (char *)&ci, sz_rfbClientInitMsg)) return FALSE;

  if (!ReadFromRFBServer(client, (char *)&client->si, sz_rfbServerInitMsg)) return FALSE;

  client->si.framebufferWidth = rfbClientSwap16IfLE(client->si.framebufferWidth);
  client->si.framebufferHeight = rfbClientSwap16IfLE(client->si.framebufferHeight);
  client->si.format.redMax = rfbClientSwap16IfLE(client->si.format.redMax);
  client->si.format.greenMax = rfbClientSwap16IfLE(client->si.format.greenMax);
  client->si.format.blueMax = rfbClientSwap16IfLE(client->si.format.blueMax);
  client->si.nameLength = rfbClientSwap32IfLE(client->si.nameLength);

  if (client->si.nameLength > 1<<20) {
      rfbClientErr("Too big desktop name length sent by server: %u B > 1 MB\n", (unsigned int)client->si.nameLength);
      return FALSE;
  }

  client->desktopName = malloc(client->si.nameLength + 1);
  if (!client->desktopName) {
    rfbClientLog("Error allocating memory for desktop name, %lu bytes\n",
            (unsigned long)client->si.nameLength);
    return FALSE;
  }

  if (!ReadFromRFBServer(client, client->desktopName, client->si.nameLength)) return FALSE;

  client->desktopName[client->si.nameLength] = 0;

  rfbClientLog("Desktop name \"%s\"\n",client->desktopName);

  rfbClientLog("Connected to VNC server, using protocol version %d.%d\n",
	  client->major, client->minor);

  rfbClientLog("VNC server default format:\n");
  PrintPixelFormat(&client->si.format);

  return TRUE;
}


/*
 * SetFormatAndEncodings.
 */

rfbBool
SetFormatAndEncodings(rfbClient* client)
{
  rfbSetPixelFormatMsg spf;
  union {
    char bytes[sz_rfbSetEncodingsMsg + MAX_ENCODINGS*4];
    rfbSetEncodingsMsg msg;
  } buf;

  rfbSetEncodingsMsg *se = &buf.msg;
  uint32_t *encs = (uint32_t *)(&buf.bytes[sz_rfbSetEncodingsMsg]);
  int len = 0;
  rfbBool requestCompressLevel = FALSE;
  rfbBool requestQualityLevel = FALSE;
  rfbBool requestLastRectEncoding = FALSE;
  rfbClientProtocolExtension* e;

  if (!SupportsClient2Server(client, rfbSetPixelFormat)) return TRUE;

  spf.type = rfbSetPixelFormat;
  spf.pad1 = 0;
  spf.pad2 = 0;
  spf.format = client->format;
  spf.format.redMax = rfbClientSwap16IfLE(spf.format.redMax);
  spf.format.greenMax = rfbClientSwap16IfLE(spf.format.greenMax);
  spf.format.blueMax = rfbClientSwap16IfLE(spf.format.blueMax);

  if (!WriteToRFBServer(client, (char *)&spf, sz_rfbSetPixelFormatMsg))
    return FALSE;


  if (!SupportsClient2Server(client, rfbSetEncodings)) return TRUE;

  se->type = rfbSetEncodings;
  se->pad = 0;
  se->nEncodings = 0;

  if (client->appData.encodingsString) {
    const char *encStr = client->appData.encodingsString;
    int encStrLen;
    do {
      const char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
	encStrLen = nextEncStr - encStr;
	nextEncStr++;
      } else {
	encStrLen = strlen(encStr);
      }

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCopyRect);
#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = TRUE;
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
	if (client->appData.enableJPEG)
	  requestQualityLevel = TRUE;
#endif
#endif
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingHextile);
#ifdef LIBVNCSERVER_HAVE_LIBZ
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlib);
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
      } else if (strncasecmp(encStr,"zlibhex",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlibHex);
	if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9)
	  requestCompressLevel = TRUE;
      } else if (strncasecmp(encStr,"trle",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingTRLE);
      } else if (strncasecmp(encStr,"zrle",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZRLE);
      } else if (strncasecmp(encStr,"zywrle",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZYWRLE);
	requestQualityLevel = TRUE;
#endif
      } else if ((strncasecmp(encStr,"ultra",encStrLen) == 0) || (strncasecmp(encStr,"ultrazip",encStrLen) == 0)) {
        /* There are 2 encodings used in 'ultra' */
        encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingUltra);
        encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingUltraZip);
      } else if (strncasecmp(encStr,"corre",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCoRRE);
      } else if (strncasecmp(encStr,"rre",encStrLen) == 0) {
	encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRRE);
      } else {
	rfbClientLog("Unknown encoding '%.*s'\n",encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
        client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }
  }
  else {
    if (SameMachine(client->sock)) {
      /* TODO:
      if (!tunnelSpecified) {
      */
      rfbClientLog("Same machine: preferring raw encoding\n");
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRaw);
      /*
      } else {
	rfbClientLog("Tunneling active: preferring tight encoding\n");
      }
      */
    }

    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCopyRect);
#ifdef LIBVNCSERVER_HAVE_LIBZ
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingTight);
    requestLastRectEncoding = TRUE;
#endif
#endif
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingHextile);
#ifdef LIBVNCSERVER_HAVE_LIBZ
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZlib);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZRLE);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingZYWRLE);
#endif
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingUltra);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingUltraZip);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCoRRE);
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRRE);

    if (client->appData.compressLevel >= 0 && client->appData.compressLevel <= 9) {
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.compressLevel +
					  rfbEncodingCompressLevel0);
    } else /* if (!tunnelSpecified) */ {
      /* If -tunnel option was provided, we assume that server machine is
	 not in the local network so we use default compression level for
	 tight encoding instead of fast compression. Thus we are
	 requesting level 1 compression only if tunneling is not used. */
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingCompressLevel1);
    }

    if (client->appData.enableJPEG) {
      if (client->appData.qualityLevel < 0 || client->appData.qualityLevel > 9)
	client->appData.qualityLevel = 5;
      encs[se->nEncodings++] = rfbClientSwap32IfLE(client->appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }
  }



  /* Remote Cursor Support (local to viewer) */
  if (client->appData.useRemoteCursor) {
    if (se->nEncodings < MAX_ENCODINGS)
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingXCursor);
    if (se->nEncodings < MAX_ENCODINGS)
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingRichCursor);
    if (se->nEncodings < MAX_ENCODINGS)
      encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingPointerPos);
  }

  /* Keyboard State Encodings */
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingKeyboardLedState);

  /* New Frame Buffer Size */
  if (se->nEncodings < MAX_ENCODINGS && client->canHandleNewFBSize)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingNewFBSize);
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingExtDesktopSize);

  /* Last Rect */
  if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingLastRect);

  /* Server Capabilities */
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingSupportedMessages);
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingSupportedEncodings);
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingServerIdentity);

  /* xvp */
  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingXvp);

  if (se->nEncodings < MAX_ENCODINGS)
    encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingQemuExtendedKeyEvent);

#ifdef LIBVNCSERVER_HAVE_LIBZ
  /* extendedclipboard. tell server we support it if client has the callback set */
  if(client->GotXCutTextUTF8)
    if (se->nEncodings < MAX_ENCODINGS)
       encs[se->nEncodings++] = rfbClientSwap32IfLE(rfbEncodingExtendedClipboard);
#endif

  /* client extensions */
  for(e = rfbClientExtensions; e; e = e->next)
    if(e->encodings) {
      int* enc;
      for(enc = e->encodings; *enc; enc++)
        if(se->nEncodings < MAX_ENCODINGS)
          encs[se->nEncodings++] = rfbClientSwap32IfLE(*enc);
    }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

  se->nEncodings = rfbClientSwap16IfLE(se->nEncodings);

  if (!WriteToRFBServer(client, buf.bytes, len)) return FALSE;

  return TRUE;
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 */

rfbBool
SendIncrementalFramebufferUpdateRequest(rfbClient* client)
{
	return SendFramebufferUpdateRequest(client,
			client->updateRect.x, client->updateRect.y,
			client->updateRect.w, client->updateRect.h, TRUE);
}


/*
 * SendFramebufferUpdateRequest.
 */

rfbBool
SendFramebufferUpdateRequest(rfbClient* client, int x, int y, int w, int h, rfbBool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  if (!SupportsClient2Server(client, rfbFramebufferUpdateRequest)) return TRUE;

  if (client->requestedResize) {
    rfbClientLog("Skipping Update - resize in progress\n");
    return TRUE;
  }

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = rfbClientSwap16IfLE(x);
  fur.y = rfbClientSwap16IfLE(y);
  fur.w = rfbClientSwap16IfLE(w);
  fur.h = rfbClientSwap16IfLE(h);

  if (!WriteToRFBServer(client, (char *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return FALSE;

  return TRUE;
}


/*
 * SendScaleSetting.
 */
rfbBool
SendScaleSetting(rfbClient* client,int scaleSetting)
{
  rfbSetScaleMsg ssm;

  ssm.scale = scaleSetting;
  ssm.pad = 0;
  
  /* favor UltraVNC SetScale if both are supported */
  if (SupportsClient2Server(client, rfbSetScale)) {
      ssm.type = rfbSetScale;
      if (!WriteToRFBServer(client, (char *)&ssm, sz_rfbSetScaleMsg))
          return FALSE;
  }
  
  if (SupportsClient2Server(client, rfbPalmVNCSetScaleFactor)) {
      ssm.type = rfbPalmVNCSetScaleFactor;
      if (!WriteToRFBServer(client, (char *)&ssm, sz_rfbSetScaleMsg))
          return FALSE;
  }

  return TRUE;
}

/*
 * TextChatFunctions (UltraVNC)
 * Extremely bandwidth friendly method of communicating with a user
 * (Think HelpDesk type applications)
 */

rfbBool TextChatSend(rfbClient* client, char *text)
{
    rfbTextChatMsg chat;
    int count = strlen(text);

    if (!SupportsClient2Server(client, rfbTextChat)) return TRUE;
    chat.type = rfbTextChat;
    chat.pad1 = 0;
    chat.pad2 = 0;
    chat.length = (uint32_t)count;
    chat.length = rfbClientSwap32IfLE(chat.length);

    if (!WriteToRFBServer(client, (char *)&chat, sz_rfbTextChatMsg))
        return FALSE;

    if (count>0) {
        if (!WriteToRFBServer(client, text, count))
            return FALSE;
    }
    return TRUE;
}

rfbBool TextChatOpen(rfbClient* client)
{
    rfbTextChatMsg chat;

    if (!SupportsClient2Server(client, rfbTextChat)) return TRUE;
    chat.type = rfbTextChat;
    chat.pad1 = 0;
    chat.pad2 = 0;
    chat.length = rfbClientSwap32IfLE(rfbTextChatOpen);
    return  (WriteToRFBServer(client, (char *)&chat, sz_rfbTextChatMsg) ? TRUE : FALSE);
}

rfbBool TextChatClose(rfbClient* client)
{
    rfbTextChatMsg chat;
    if (!SupportsClient2Server(client, rfbTextChat)) return TRUE;
    chat.type = rfbTextChat;
    chat.pad1 = 0;
    chat.pad2 = 0;
    chat.length = rfbClientSwap32IfLE(rfbTextChatClose);
    return  (WriteToRFBServer(client, (char *)&chat, sz_rfbTextChatMsg) ? TRUE : FALSE);
}

rfbBool TextChatFinish(rfbClient* client)
{
    rfbTextChatMsg chat;
    if (!SupportsClient2Server(client, rfbTextChat)) return TRUE;
    chat.type = rfbTextChat;
    chat.pad1 = 0;
    chat.pad2 = 0;
    chat.length = rfbClientSwap32IfLE(rfbTextChatFinished);
    return  (WriteToRFBServer(client, (char *)&chat, sz_rfbTextChatMsg) ? TRUE : FALSE);
}

/*
 * UltraVNC Server Input Disable
 * Apparently, the remote client can *prevent* the local user from interacting with the display
 * I would think this is extremely helpful when used in a HelpDesk situation
 */
rfbBool PermitServerInput(rfbClient* client, int enabled)
{
    rfbSetServerInputMsg msg;

    if (!SupportsClient2Server(client, rfbSetServerInput)) return TRUE;
    /* enabled==1, then server input from local keyboard is disabled */
    msg.type = rfbSetServerInput;
    msg.status = (enabled ? 1 : 0);
    msg.pad = 0;
    return  (WriteToRFBServer(client, (char *)&msg, sz_rfbSetServerInputMsg) ? TRUE : FALSE);
}


/*
 * send xvp client message
 * A client supporting the xvp extension sends this to request that the server initiate
 * a clean shutdown, clean reboot or abrupt reset of the system whose framebuffer the
 * client is displaying.
 *
 * only version 1 is defined in the protocol specs
 *
 * possible values for code are:
 *   rfbXvp_Shutdown
 *   rfbXvp_Reboot
 *   rfbXvp_Reset
 */

rfbBool SendXvpMsg(rfbClient* client, uint8_t version, uint8_t code)
{
    rfbXvpMsg xvp;

    if (!SupportsClient2Server(client, rfbXvp)) return TRUE;
    xvp.type = rfbXvp;
    xvp.pad = 0;
    xvp.version = version;
    xvp.code = code;

    if (!WriteToRFBServer(client, (char *)&xvp, sz_rfbXvpMsg))
        return FALSE;

    return TRUE;
}


/*
 * SendPointerEvent.
 */

rfbBool
SendPointerEvent(rfbClient* client,int x, int y, int buttonMask)
{
  rfbPointerEventMsg pe;

  if (!SupportsClient2Server(client, rfbPointerEvent)) return TRUE;

  pe.type = rfbPointerEvent;
  pe.buttonMask = buttonMask;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  pe.x = rfbClientSwap16IfLE(x);
  pe.y = rfbClientSwap16IfLE(y);
  return WriteToRFBServer(client, (char *)&pe, sz_rfbPointerEventMsg);
}


/*
 * Resize client
 */

static rfbBool
ResizeClientBuffer(rfbClient* client, int width, int height)
{
  client->width = width;
  client->height = height;
  client->updateRect.x = client->updateRect.y = 0;
  client->updateRect.w = client->width;
  client->updateRect.h = client->height;
  return client->MallocFrameBuffer(client);
}


/*
 * SendExtDesktopSize
 */

rfbBool
SendExtDesktopSize(rfbClient* client, uint16_t width, uint16_t height)
{
  rfbSetDesktopSizeMsg sdm;
  rfbExtDesktopScreen screen;

  if (client->screen.width == 0 && client->screen.height == 0 ) {
    rfbClientLog("Screen not yet received from server - not sending dimensions %dx%d\n", width, height);
    return TRUE;
  }

  if (client->screen.width != rfbClientSwap16IfLE(width) || client->screen.height != rfbClientSwap16IfLE(height)) {
    rfbClientLog("Sending dimensions %dx%d\n", width, height);
    sdm.type = rfbSetDesktopSize;
    sdm.width = rfbClientSwap16IfLE(width);
    sdm.height = rfbClientSwap16IfLE(height);
    sdm.numberOfScreens = 1;
    screen = client->screen;
    screen.x = 0;
    screen.y = 0;
    screen.width = rfbClientSwap16IfLE(width);
    screen.height = rfbClientSwap16IfLE(height);

    if (!WriteToRFBServer(client, (char *)&sdm, sz_rfbSetDesktopSizeMsg)) return FALSE;
    if (!WriteToRFBServer(client, (char *)&screen, sz_rfbExtDesktopScreen)) return FALSE;

    client->screen.width = screen.width;
    client->screen.height = screen.height;

    client->requestedResize = FALSE;

    SendFramebufferUpdateRequest(client, 0, 0, width, height, FALSE);

    client->requestedResize = TRUE;
  }

  return TRUE;
}


/*
 * SendKeyEvent.
 */

rfbBool
SendKeyEvent(rfbClient* client, uint32_t key, rfbBool down)
{
  rfbKeyEventMsg ke;

  if (!SupportsClient2Server(client, rfbKeyEvent)) return TRUE;

  memset(&ke, 0, sizeof(ke));
  ke.type = rfbKeyEvent;
  ke.down = down ? 1 : 0;
  ke.key = rfbClientSwap32IfLE(key);
  return WriteToRFBServer(client, (char *)&ke, sz_rfbKeyEventMsg);
}


/*
 * SendExtendedKeyEvent.
 */

rfbBool
SendExtendedKeyEvent(rfbClient* client, uint32_t keysym, uint32_t keycode, rfbBool down)
{
  rfbQemuExtendedKeyEventMsg ke;

  /* FIXME: rfbQemuEvent also covers audio events, but this model for checking
   * for supported messages is somewhat limited, so I'll leave this as is for
   * now.
   */
  if (!SupportsClient2Server(client, rfbQemuEvent)) return FALSE;

  memset(&ke, 0, sizeof(ke));
  ke.type = rfbQemuEvent;
  ke.subtype = 0; /* key event subtype */
  ke.down = rfbClientSwap16IfLE(!!down);
  ke.keysym = rfbClientSwap32IfLE(keysym);
  ke.keycode = rfbClientSwap32IfLE(keycode);
  return WriteToRFBServer(client, (char *)&ke, sz_rfbQemuExtendedKeyEventMsg);
}


#ifdef LIBVNCSERVER_HAVE_LIBZ
/*
 * sendExtClientCutTextNotify
 * it is needed when client send utf8 clipboard data
 * please refer to
 * https://github.com/rfbproto/rfbproto/blob/master/rfbproto.rst#extended-clipboard-pseudo-encoding
 * to supprot utf-8 clipboard we need at least support notify and text
 */

static rfbBool
sendExtClientCutTextNotify(rfbClient *client)
{
  rfbClientCutTextMsg cct = {0, };
  const uint32_t be_flags = rfbClientSwap32IfLE(rfbExtendedClipboard_Notify
                                                | rfbExtendedClipboard_Text); /*text and notify*/
  cct.type = rfbClientCutText;
  cct.length = rfbClientSwap32IfLE(-((uint32_t)sizeof(be_flags)));/*flag*/
  rfbBool ret = WriteToRFBServer(client, (char *)&cct, sz_rfbClientCutTextMsg)
                && WriteToRFBServer(client, (char *)&be_flags, sizeof(be_flags));
  return ret;
}

static rfbBool
sendExtClientCutTextRequest(rfbClient *client)
{
  rfbClientCutTextMsg cct = {0, };
  const uint32_t be_flags = rfbClientSwap32IfLE(rfbExtendedClipboard_Text|rfbExtendedClipboard_Request);
  cct.type = rfbClientCutText;
  cct.length = rfbClientSwap32IfLE(-((uint32_t)sizeof(be_flags)));/*flag*/
  rfbBool ret = WriteToRFBServer(client, (char *)&cct, sz_rfbClientCutTextMsg)
                && WriteToRFBServer(client, (char *)&be_flags, sizeof(be_flags));
  return ret;
}

/**
 * Due to bugs, many servers (including most versions of LibVNCServer) can't
 * properly handle zlib streams created by compress() function of zlib library.
 *
 * Primary bug is that these servers don't expect inflate() to return Z_STREAM_END
 * which is the case for (correct) streams created by compress().
 *
 * So this function creates a compatible stream, for which inflate() returns Z_OK.
 * This is how some clients create zlib streams, unintentionally avoiding the bug.
 */
static int
CompressClipData(Bytef *dest, uLongf *destLen, Bytef *source, uLong sourceLen)
{
  int ret;
  z_stream *zs = (z_stream*)malloc(sizeof(z_stream));
  memset(zs, 0, sizeof(z_stream));

  zs->zfree = Z_NULL;
  zs->zalloc = Z_NULL;
  zs->opaque = Z_NULL;
  ret = deflateInit(zs, Z_DEFAULT_COMPRESSION);
  if (ret == Z_OK) {
    zs->avail_in = sourceLen;
    zs->next_in = source;
    zs->avail_out = *destLen;
    zs->next_out = dest;

    do {
      // Using Z_SYNC_FLUSH instead of Z_FINISH is the key here.
      ret = deflate(zs, Z_SYNC_FLUSH);
    } while (ret >= 0 && zs->avail_in > 0);

    *destLen = zs->total_out;
    deflateEnd(zs);
  }
  return ret;
}

/*
 * sendExtClientCutTextProvide
 * it need send notify first to grab clipboard (server will check that)
 */

static rfbBool
sendExtClientCutTextProvide(rfbClient *client, char* data, int len)
{
  rfbClientCutTextMsg cct = {0, };
  int sentLen = len + 1;  /* Sent data is null terminated*/
  const uint32_t be_flags = rfbClientSwap32IfLE(rfbExtendedClipboard_Provide
                                                | rfbExtendedClipboard_Text); /*text and provide*/
  const uint32_t be_size = rfbClientSwap32IfLE(sentLen);
  const size_t sz_to_compressed = sizeof(be_size) + sentLen; /*size, data*/
  uLong csz = compressBound(sz_to_compressed);

  unsigned char *buf = malloc(sz_to_compressed);
  if (!buf) {
    rfbClientLog("sendExtClientCutTextProvide. alloc buf failed\n");
    return FALSE;
  }
  memcpy(buf, &be_size, sizeof(be_size));
  memcpy(buf + sizeof(be_size), data, len);
  buf[sz_to_compressed - 1] = 0;  /* Null terminate sent data */

  unsigned char *cbuf = malloc(sizeof(be_flags) + csz); /*flag, compressed*/
  if (!cbuf) {
    rfbClientLog("sendExtClientCutTextProvide. alloc cbuf failed\n");
    free(buf);
    return FALSE;
  }
  memcpy(cbuf, &be_flags, sizeof(be_flags));
  if (CompressClipData(cbuf + sizeof(be_flags), &csz, buf, sz_to_compressed) != Z_OK) {
    rfbClientLog("sendExtClientCutTextProvide: compress cbuf failed\n");
    free(buf);
    free(cbuf);
    return FALSE;
  }

  cct.type = rfbClientCutText;
  cct.length = rfbClientSwap32IfLE(-(sizeof(be_flags) + csz));/*flag, compressed*/
  rfbBool ret = sendExtClientCutTextNotify(client)
                && WriteToRFBServer(client, (char *)&cct, sz_rfbClientCutTextMsg)
                && WriteToRFBServer(client, (char *)cbuf, sizeof(be_flags) + csz);
  free(buf);
  free(cbuf);
  return ret;
}
#endif

/*
 * SendClientCutText.
 */

rfbBool
SendClientCutText(rfbClient* client, char *str, int len)
{
  rfbClientCutTextMsg cct;

  if (!SupportsClient2Server(client, rfbClientCutText)) return TRUE;

  memset(&cct, 0, sizeof(cct));
  cct.type = rfbClientCutText;
  cct.length = rfbClientSwap32IfLE(len);
  return  (WriteToRFBServer(client, (char *)&cct, sz_rfbClientCutTextMsg) &&
	   WriteToRFBServer(client, str, len));
}

rfbBool
SendClientCutTextUTF8(rfbClient* client, char *str, int len)
{
#ifdef LIBVNCSERVER_HAVE_LIBZ
    return client->extendedClipboardServerCapabilities && sendExtClientCutTextProvide(client, str, len);
#else
    return FALSE;
#endif
}


#ifdef LIBVNCSERVER_HAVE_LIBZ
/*
 * process server clipboard extend text
 */

static rfbBool
rfbClientProcessExtServerCutText(rfbClient* client, char *data, int len)
{
  uint32_t flags;
  if (len < sizeof(flags)) {
    rfbClientLog("rfbClientProcessExtServerCutText. len < 4\n");
    return FALSE;
  }
  memcpy(&flags, data, sizeof(flags));
  data += sizeof(flags);
  len -= sizeof(flags);
  flags = rfbClientSwap32IfLE(flags);

  /*
   * only process (text | provide). Ignore all others
   * modify here if need more types(rtf,html,dib,files)
   */
  if (flags & rfbExtendedClipboard_Caps) {
    rfbClientLog("rfbClientProcessExtServerCutText. default cap.\n");
    client->extendedClipboardServerCapabilities |= rfbExtendedClipboard_Text; /* for now, only text */
    return TRUE;
  }
  if (!(flags & rfbExtendedClipboard_Text)) {
    rfbClientLog("rfbClientProcessExtServerCutText. not text type. ignore\n");
    return TRUE;
  }
  if (flags & rfbExtendedClipboard_Notify) {
    return sendExtClientCutTextRequest(client);
  }
  if (!(flags & rfbExtendedClipboard_Provide)) {
    rfbClientLog("rfbClientProcessExtServerCutText. not provide type. ignore\n");
    return TRUE;
  }

  z_stream stream;
  stream.zalloc = NULL;
  stream.zfree = NULL;
  stream.opaque = NULL;
  stream.avail_in = 0;
  stream.next_in = NULL;
  if (inflateInit(&stream) != Z_OK) {
    rfbClientLog("rfbClientProcessExtServerCutText. inflateInit failed\n");
    return FALSE;
  }
  stream.avail_in = len;
  stream.next_in = (unsigned char *)data;

  uint32_t size;
  stream.avail_out = sizeof(size);
  stream.next_out = (unsigned char *)&size;
  if (inflate(&stream, Z_SYNC_FLUSH) != Z_OK) {
    rfbClientLog("rfbClientProcessExtServerCutText. inflate size failed\n");
    inflateEnd(&stream);
    return FALSE;
  }
  size = rfbClientSwap32IfLE(size);
  if (size > (1 << 20)) {
    rfbClientLog("rfbClientProcessExtServerCutText. size too large\n");
    inflateEnd(&stream);
    return FALSE;
  }

  unsigned char *buf = malloc(size);
  if (!buf) {
    rfbClientLog("rfbClientProcessExtServerCutText. alloc buf failed\n");
    inflateEnd(&stream);
    return FALSE;
  }
  stream.avail_out = size;
  stream.next_out = buf;
  uLong out_before = stream.total_out;
  int err = inflate(&stream, Z_SYNC_FLUSH);
  if (err != Z_OK && err != Z_STREAM_END) {
    rfbClientLog("rfbClientProcessExtServerCutText. inflate buf failed\n");
    free(buf);
    inflateEnd(&stream);
    return FALSE;
  }
  if ((stream.total_out - out_before) != size) {
    rfbClientLog("rfbClientProcessExtServerCutText. inflate size error\n");
    free(buf);
    inflateEnd(&stream);
    return FALSE;
  }
  if (client->GotXCutTextUTF8)
    client->GotXCutTextUTF8(client, (char *)buf, size);
  free(buf);

  inflateEnd(&stream);
  return TRUE;
}
#endif


/*
 * HandleRFBServerMessage.
 */

rfbBool
HandleRFBServerMessage(rfbClient* client)
{
  rfbServerToClientMsg msg;

  if (client->serverPort==-1)
    client->vncRec->readTimestamp = TRUE;
  if (!ReadFromRFBServer(client, (char *)&msg, 1))
    return FALSE;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    /* TODO:
    int i;
    uint16_t rgb[3];
    XColor xc;

    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbSetColourMapEntriesMsg - 1))
      return FALSE;

    msg.scme.firstColour = rfbClientSwap16IfLE(msg.scme.firstColour);
    msg.scme.nColours = rfbClientSwap16IfLE(msg.scme.nColours);

    for (i = 0; i < msg.scme.nColours; i++) {
      if (!ReadFromRFBServer(client, (char *)rgb, 6))
	return FALSE;
      xc.pixel = msg.scme.firstColour + i;
      xc.red = rfbClientSwap16IfLE(rgb[0]);
      xc.green = rfbClientSwap16IfLE(rgb[1]);
      xc.blue = rfbClientSwap16IfLE(rgb[2]);
      xc.flags = DoRed|DoGreen|DoBlue;
      XStoreColor(dpy, cmap, &xc);
    }
    */

    break;
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;
    int linesToRead;
    int bytesPerLine;
    int i;

    if (!ReadFromRFBServer(client, ((char *)&msg.fu) + 1,
			   sz_rfbFramebufferUpdateMsg - 1))
      return FALSE;

    msg.fu.nRects = rfbClientSwap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer(client, (char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return FALSE;

      rect.encoding = rfbClientSwap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
	break;

      rect.r.x = rfbClientSwap16IfLE(rect.r.x);
      rect.r.y = rfbClientSwap16IfLE(rect.r.y);
      rect.r.w = rfbClientSwap16IfLE(rect.r.w);
      rect.r.h = rfbClientSwap16IfLE(rect.r.h);


      if (rect.encoding == rfbEncodingXCursor ||
	  rect.encoding == rfbEncodingRichCursor) {

	if (!HandleCursorShape(client,
			       rect.r.x, rect.r.y, rect.r.w, rect.r.h,
			       rect.encoding)) {
	  return FALSE;
	}
	continue;
      }

      if (rect.encoding == rfbEncodingPointerPos) {
	if (!client->HandleCursorPos(client,rect.r.x, rect.r.y)) {
	  return FALSE;
	}
	continue;
      }
      
      if (rect.encoding == rfbEncodingKeyboardLedState) {
          /* OK! We have received a keyboard state message!!! */
          client->KeyboardLedStateEnabled = 1;
          if (client->HandleKeyboardLedState!=NULL)
              client->HandleKeyboardLedState(client, rect.r.x, 0);
          /* stash it for the future */
          client->CurrentKeyboardLedState = rect.r.x;
          continue;
      }

      if (rect.encoding == rfbEncodingNewFBSize) {
	if(!ResizeClientBuffer(client, rect.r.w, rect.r.h))
	  return FALSE;
	SendFramebufferUpdateRequest(client, 0, 0, rect.r.w, rect.r.h, FALSE);
	rfbClientLog("Got new framebuffer size: %dx%d\n", rect.r.w, rect.r.h);
	continue;
      }

      if (rect.encoding == rfbEncodingExtDesktopSize) {
        /* read encoding data */
        int screens;
        int loop;
        rfbBool invalidScreen = FALSE;
        rfbExtDesktopScreen screen;
        rfbExtDesktopSizeMsg eds;
        if (!ReadFromRFBServer(client, ((char *)&eds), sz_rfbExtDesktopSizeMsg)) {
          return FALSE;
        }

        screens = eds.numberOfScreens;
        for (loop=0; loop < screens; loop++)
        {
          if (!ReadFromRFBServer(client, ((char *)&screen), sz_rfbExtDesktopScreen)) {
            return FALSE;
          }
          if (screen.id != 0 && screen.width && screen.height) {
            client->screen = screen;
          } else {
            invalidScreen = TRUE;
          }
        }

        if (!invalidScreen && (client->width != rect.r.w || client->height != rect.r.h)) {
          if(!ResizeClientBuffer(client, rect.r.w, rect.r.h)) {
            return FALSE;
          }
          rfbClientLog("Updated desktop size: %dx%d\n", rect.r.w, rect.r.h);
        }
        client->requestedResize = FALSE;

        continue;
      }

      /* rect.r.w=byte count */
      if (rect.encoding == rfbEncodingSupportedMessages) {
          int loop;
          if (!ReadFromRFBServer(client, (char *)&client->supportedMessages, sz_rfbSupportedMessages))
              return FALSE;

          /* msgs is two sets of bit flags of supported messages client2server[] and server2client[] */
          /* currently ignored by this library */

          rfbClientLog("client2server supported messages (bit flags)\n");
          for (loop=0;loop<32;loop+=8)
            rfbClientLog("%02X: %04x %04x %04x %04x - %04x %04x %04x %04x\n", loop,
                client->supportedMessages.client2server[loop],   client->supportedMessages.client2server[loop+1],
                client->supportedMessages.client2server[loop+2], client->supportedMessages.client2server[loop+3],
                client->supportedMessages.client2server[loop+4], client->supportedMessages.client2server[loop+5],
                client->supportedMessages.client2server[loop+6], client->supportedMessages.client2server[loop+7]);

          rfbClientLog("server2client supported messages (bit flags)\n");
          for (loop=0;loop<32;loop+=8)
            rfbClientLog("%02X: %04x %04x %04x %04x - %04x %04x %04x %04x\n", loop,
                client->supportedMessages.server2client[loop],   client->supportedMessages.server2client[loop+1],
                client->supportedMessages.server2client[loop+2], client->supportedMessages.server2client[loop+3],
                client->supportedMessages.server2client[loop+4], client->supportedMessages.server2client[loop+5],
                client->supportedMessages.server2client[loop+6], client->supportedMessages.server2client[loop+7]);
          continue;
      }

      /* rect.r.w=byte count, rect.r.h=# of encodings */
      if (rect.encoding == rfbEncodingSupportedEncodings) {
          char *buffer;
          buffer = malloc(rect.r.w);
          if (!ReadFromRFBServer(client, buffer, rect.r.w))
          {
              free(buffer);
              return FALSE;
          }

          /* buffer now contains rect.r.h # of uint32_t encodings that the server supports */
          /* currently ignored by this library */
          free(buffer);
          continue;
      }

      /* rect.r.w=byte count */
      if (rect.encoding == rfbEncodingServerIdentity) {
          char *buffer;
          buffer = malloc(rect.r.w+1);
          if (!buffer || !ReadFromRFBServer(client, buffer, rect.r.w))
          {
              free(buffer);
              return FALSE;
          }
          buffer[rect.r.w]=0; /* null terminate, just in case */
          rfbClientLog("Connected to Server \"%s\"\n", buffer);
          free(buffer);
          continue;
      }

      /* rfbEncodingUltraZip is a collection of subrects.   x = # of subrects, and h is always 0 */
      if (rect.encoding != rfbEncodingUltraZip)
      {
        if ((rect.r.x + rect.r.w > client->width) ||
	    (rect.r.y + rect.r.h > client->height))
	    {
	      rfbClientLog("Rect too large: %dx%d at (%d, %d)\n",
	  	  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	      return FALSE;
            }

        /* UltraVNC with scaling, will send rectangles with a zero W or H
         *
        if ((rect.encoding != rfbEncodingTight) && 
            (rect.r.h * rect.r.w == 0))
        {
	  rfbClientLog("Zero size rect - ignoring (encoding=%d (0x%08x) %dx, %dy, %dw, %dh)\n", rect.encoding, rect.encoding, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
	  continue;
        }
        */
        
        /* If RichCursor encoding is used, we should prevent collisions
	   between framebuffer updates and cursor drawing operations. */
        client->SoftCursorLockArea(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
      }

      switch (rect.encoding) {

      case rfbEncodingRaw: {
	int y=rect.r.y, h=rect.r.h;

	bytesPerLine = rect.r.w * client->format.bitsPerPixel / 8;
	/* RealVNC 4.x-5.x on OSX can induce bytesPerLine==0, 
	   usually during GPU accel. */
	/* Regardless of cause, do not divide by zero. */
	linesToRead = bytesPerLine ? (RFB_BUFFER_SIZE / bytesPerLine) : 0;

	while (linesToRead && h > 0) {
	  if (linesToRead > h)
	    linesToRead = h;

	  if (!ReadFromRFBServer(client, client->buffer,bytesPerLine * linesToRead))
	    return FALSE;

	  client->GotBitmap(client, (uint8_t *)client->buffer,
			   rect.r.x, y, rect.r.w,linesToRead);

	  h -= linesToRead;
	  y += linesToRead;

	}
	break;
      } 

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;

	if (!ReadFromRFBServer(client, (char *)&cr, sz_rfbCopyRect))
	  return FALSE;

	cr.srcX = rfbClientSwap16IfLE(cr.srcX);
	cr.srcY = rfbClientSwap16IfLE(cr.srcY);

	/* If RichCursor encoding is used, we should extend our
	   "cursor lock area" (previously set to destination
	   rectangle) to the source rectangle as well. */
	client->SoftCursorLockArea(client,
				   cr.srcX, cr.srcY, rect.r.w, rect.r.h);

        client->GotCopyRect(client, cr.srcX, cr.srcY, rect.r.w, rect.r.h,
                            rect.r.x, rect.r.y);

	break;
      }

      case rfbEncodingRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingCoRRE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleCoRRE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleCoRRE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleCoRRE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingHextile:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleHextile8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleHextile16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleHextile32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }

      case rfbEncodingUltra:
      {
        switch (client->format.bitsPerPixel) {
        case 8:
          if (!HandleUltra8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 16:
          if (!HandleUltra16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 32:
          if (!HandleUltra32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        }
        break;
      }
      case rfbEncodingUltraZip:
      {
        switch (client->format.bitsPerPixel) {
        case 8:
          if (!HandleUltraZip8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 16:
          if (!HandleUltraZip16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        case 32:
          if (!HandleUltraZip32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
            return FALSE;
          break;
        }
        break;
      }

      case rfbEncodingTRLE:
	  {
        switch (client->format.bitsPerPixel) {
        case 8:
          if (!HandleTRLE8(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
            return FALSE;
          break;
        case 16:
          if (client->si.format.greenMax > 0x1F) {
            if (!HandleTRLE16(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
              return FALSE;
          } else {
            if (!HandleTRLE15(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
              return FALSE;
          }
          break;
        case 32: {
          uint32_t maxColor =
              (client->format.redMax << client->format.redShift) |
              (client->format.greenMax << client->format.greenShift) |
              (client->format.blueMax << client->format.blueShift);
          if ((client->format.bigEndian && (maxColor & 0xff) == 0) ||
              (!client->format.bigEndian && (maxColor & 0xff000000) == 0)) {
            if (!HandleTRLE24(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
              return FALSE;
          } else if (!client->format.bigEndian && (maxColor & 0xff) == 0) {
            if (!HandleTRLE24Up(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h))
              return FALSE;
          } else if (client->format.bigEndian && (maxColor & 0xff000000) == 0) {
            if (!HandleTRLE24Down(client, rect.r.x, rect.r.y, rect.r.w,
                                  rect.r.h))
              return FALSE;
          } else if (!HandleTRLE32(client, rect.r.x, rect.r.y, rect.r.w,
                                   rect.r.h))
            return FALSE;
          break;
        }
        }
        break;
      }

#ifdef LIBVNCSERVER_HAVE_LIBZ
      case rfbEncodingZlib:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleZlib8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleZlib16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleZlib32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
     }

#ifdef LIBVNCSERVER_HAVE_LIBJPEG
      case rfbEncodingTight:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleTight8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (!HandleTight16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 32:
	  if (!HandleTight32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	break;
      }
#endif
      case rfbEncodingZRLE:
	/* Fail safe for ZYWRLE unsupport VNC server. */
	client->appData.qualityLevel = 9;
	/* fall through */
      case rfbEncodingZYWRLE:
      {
	switch (client->format.bitsPerPixel) {
	case 8:
	  if (!HandleZRLE8(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	case 16:
	  if (client->si.format.greenMax > 0x1F) {
	    if (!HandleZRLE16(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else {
	    if (!HandleZRLE15(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  }
	  break;
	case 32:
	{
	  uint32_t maxColor=(client->format.redMax<<client->format.redShift)|
		(client->format.greenMax<<client->format.greenShift)|
		(client->format.blueMax<<client->format.blueShift);
	  if ((client->format.bigEndian && (maxColor&0xff)==0) ||
	      (!client->format.bigEndian && (maxColor&0xff000000)==0)) {
	    if (!HandleZRLE24(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (!client->format.bigEndian && (maxColor&0xff)==0) {
	    if (!HandleZRLE24Up(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (client->format.bigEndian && (maxColor&0xff000000)==0) {
	    if (!HandleZRLE24Down(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	      return FALSE;
	  } else if (!HandleZRLE32(client, rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return FALSE;
	  break;
	}
	}
	break;
     }

#endif

      case rfbEncodingQemuExtendedKeyEvent:
        SetClient2Server(client, rfbQemuEvent);
        break;

      default:
	 {
	   rfbBool handled = FALSE;
	   rfbClientProtocolExtension* e;

	   for(e = rfbClientExtensions; !handled && e; e = e->next)
	     if(e->handleEncoding && e->handleEncoding(client, &rect))
	       handled = TRUE;

	   if(!handled) {
	     rfbClientLog("Unknown rect encoding %d\n",
		 (int)rect.encoding);
	     return FALSE;
	   }
	 }
      }

      /* Now we may discard "soft cursor locks". */
      client->SoftCursorUnlockScreen(client);

      client->GotFrameBufferUpdate(client, rect.r.x, rect.r.y, rect.r.w, rect.r.h);
    }

    if (client->automaticUpdateRequests && !SendIncrementalFramebufferUpdateRequest(client))
      return FALSE;

    if (client->FinishedFrameBufferUpdate)
      client->FinishedFrameBufferUpdate(client);

    break;
  }

  case rfbBell:
  {
    client->Bell(client);

    break;
  }

  case rfbServerCutText:
  {
    char *buffer;
#ifdef LIBVNCSERVER_HAVE_LIBZ
    int32_t ilen; /* also as a flag, if ilen < 0, it is ext clipboard text */
#endif

    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
			   sz_rfbServerCutTextMsg - 1))
      return FALSE;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    ilen = rfbClientSwap32IfLE(msg.sct.length);
    msg.sct.length = ilen < 0 ? -ilen : ilen;
#else
    msg.sct.length = rfbClientSwap32IfLE(msg.sct.length);
#endif

    if (msg.sct.length > 1<<20) {
	    rfbClientErr("Ignoring too big cut text length sent by server: %u B > 1 MB\n", (unsigned int)msg.sct.length);
	    return FALSE;
    }  

    buffer = malloc(msg.sct.length+1);

    if (!buffer || !ReadFromRFBServer(client, buffer, msg.sct.length)) {
      free(buffer);
      return FALSE;
    }

    buffer[msg.sct.length] = 0;

#ifdef LIBVNCSERVER_HAVE_LIBZ
    if (ilen < 0 && client->GotXCutTextUTF8) {
      if (!rfbClientProcessExtServerCutText(client, buffer, -ilen)) {
        free(buffer);
        return FALSE;
      }
    } else if (client->GotXCutText)
      client->GotXCutText(client, buffer, msg.sct.length);
#else
    if (client->GotXCutText)
      client->GotXCutText(client, buffer, msg.sct.length);
#endif

    free(buffer);

    break;
  }

  case rfbTextChat:
  {
      char *buffer=NULL;
      if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                             sz_rfbTextChatMsg- 1))
        return FALSE;
      msg.tc.length = rfbClientSwap32IfLE(msg.sct.length);
      switch(msg.tc.length) {
      case rfbTextChatOpen:
          rfbClientLog("Received TextChat Open\n");
          if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatOpen, NULL);
          break;
      case rfbTextChatClose:
          rfbClientLog("Received TextChat Close\n");
         if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatClose, NULL);
          break;
      case rfbTextChatFinished:
          rfbClientLog("Received TextChat Finished\n");
         if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)rfbTextChatFinished, NULL);
          break;
      default:
	  if(msg.tc.length > MAX_TEXTCHAT_SIZE)
	      return FALSE;
          buffer=malloc(msg.tc.length+1);
          if (!buffer || !ReadFromRFBServer(client, buffer, msg.tc.length))
          {
              free(buffer);
              return FALSE;
          }
          /* Null Terminate <just in case> */
          buffer[msg.tc.length]=0;
          rfbClientLog("Received TextChat \"%s\"\n", buffer);
          if (client->HandleTextChat!=NULL)
              client->HandleTextChat(client, (int)msg.tc.length, buffer);
          free(buffer);
          break;
      }
      break;
  }

  case rfbXvp:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbXvpMsg -1))
      return FALSE;

    SetClient2Server(client, rfbXvp);
    /* technically, we only care what we can *send* to the server
     * but, we set Server2Client Just in case it ever becomes useful
     */
    SetServer2Client(client, rfbXvp);

    if(client->HandleXvpMsg)
      client->HandleXvpMsg(client, msg.xvp.version, msg.xvp.code);

    break;
  }

  case rfbResizeFrameBuffer:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbResizeFrameBufferMsg -1))
      return FALSE;
    client->width = rfbClientSwap16IfLE(msg.rsfb.framebufferWidth);
    client->height = rfbClientSwap16IfLE(msg.rsfb.framebufferHeigth);
    client->updateRect.x = client->updateRect.y = 0;
    client->updateRect.w = client->width;
    client->updateRect.h = client->height;
    if (!client->MallocFrameBuffer(client))
      return FALSE;

    SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
    rfbClientLog("Got new framebuffer size: %dx%d\n", client->width, client->height);
    break;
  }

  case rfbPalmVNCReSizeFrameBuffer:
  {
    if (!ReadFromRFBServer(client, ((char *)&msg) + 1,
                           sz_rfbPalmVNCReSizeFrameBufferMsg -1))
      return FALSE;
    client->width = rfbClientSwap16IfLE(msg.prsfb.buffer_w);
    client->height = rfbClientSwap16IfLE(msg.prsfb.buffer_h);
    client->updateRect.x = client->updateRect.y = 0;
    client->updateRect.w = client->width;
    client->updateRect.h = client->height;
    if (!client->MallocFrameBuffer(client))
      return FALSE;
    SendFramebufferUpdateRequest(client, 0, 0, client->width, client->height, FALSE);
    rfbClientLog("Got new framebuffer size: %dx%d\n", client->width, client->height);
    break;
  }

  default:
    {
      rfbBool handled = FALSE;
      rfbClientProtocolExtension* e;

      for(e = rfbClientExtensions; !handled && e; e = e->next)
	if(e->handleMessage && e->handleMessage(client, &msg))
	  handled = TRUE;

      if(!handled) {
	char buffer[256];
	rfbClientLog("Unknown message type %d from VNC server\n",msg.type);
	ReadFromRFBServer(client, buffer, 256);
	return FALSE;
      }
    }
  }

  return TRUE;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
			       ((uint8_t*)&(pix))[1] = *(ptr)++)

#define GET_PIXEL32(pix, ptr) (((uint8_t*)&(pix))[0] = *(ptr)++, \
			       ((uint8_t*)&(pix))[1] = *(ptr)++, \
			       ((uint8_t*)&(pix))[2] = *(ptr)++, \
			       ((uint8_t*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)
#define CONCAT3(a,b,c) a##b##c
#define CONCAT3E(a,b,c) CONCAT3(a,b,c)

#define BPP 8
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "ultra.c"
#include "zlib.c"
#include "tight.c"
#include "trle.c"
#include "zrle.c"
#undef BPP
#define BPP 16
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "ultra.c"
#include "zlib.c"
#include "tight.c"
#include "trle.c"
#include "zrle.c"
#define REALBPP 15
#include "trle.c"
#define REALBPP 15
#include "zrle.c"
#undef BPP
#define BPP 32
#include "rre.c"
#include "corre.c"
#include "hextile.c"
#include "ultra.c"
#include "zlib.c"
#include "tight.c"
#include "trle.c"
#include "zrle.c"
#define REALBPP 24
#include "trle.c"
#define REALBPP 24
#include "zrle.c"
#define REALBPP 24
#define UNCOMP 8
#include "trle.c"
#define REALBPP 24
#define UNCOMP 8
#include "zrle.c"
#define REALBPP 24
#define UNCOMP -8
#include "trle.c"
#define REALBPP 24
#define UNCOMP -8
#include "zrle.c"
#undef BPP


/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(rfbPixelFormat *format)
{
  if (format->bitsPerPixel == 1) {
    rfbClientLog("  Single bit per pixel.\n");
    rfbClientLog(
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    rfbClientLog("  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      rfbClientLog("  %s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      rfbClientLog("  TRUE colour: max red %d green %d blue %d"
		   ", shift red %d green %d blue %d\n",
		   format->redMax, format->greenMax, format->blueMax,
		   format->redShift, format->greenShift, format->blueShift);
    } else {
      rfbClientLog("  Colour map (not true colour).\n");
    }
  }
}

/* avoid name clashes with LibVNCServer */

#define rfbEncryptBytes rfbClientEncryptBytes
#define rfbEncryptBytes2 rfbClientEncryptBytes2
#define rfbDes rfbClientDes
#define rfbDesKey rfbClientDesKey
#define rfbUseKey rfbClientUseKey

#include "vncauth.c"
