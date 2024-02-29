/*
 * websockets.c - deal with WebSockets clients.
 *
 * This code should be independent of any changes in the RFB protocol. It is
 * an additional handshake and framing of normal sockets:
 *   http://www.whatwg.org/specs/web-socket-protocol/
 *
 */

/*
 *  Copyright (C) 2010 Joel Martin
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

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#endif

#include <rfb/rfb.h>
/* errno */
#include <errno.h>

#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h>
#if LIBVNCSERVER_UNISTD_H
#include <unistd.h>
#endif
#include "rfb/rfbconfig.h"
#include "rfbssl.h"
#include "crypto.h"
#include "ws_decode.h"
#include "base64.h"

#if 0
#include <sys/syscall.h>
static int gettid() {
    return (int)syscall(SYS_gettid);
}
#endif

/*
 * draft-ietf-hybi-thewebsocketprotocol-10
 * 5.2.2. Sending the Server's Opening Handshake
 */
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define SERVER_HANDSHAKE_HYBI "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
Sec-WebSocket-Protocol: %s\r\n\
\r\n"

#define SERVER_HANDSHAKE_HYBI_NO_PROTOCOL "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n"

#define WEBSOCKETS_CLIENT_CONNECT_WAIT_MS 100
#define WEBSOCKETS_CLIENT_SEND_WAIT_MS 100
#define WEBSOCKETS_MAX_HANDSHAKE_LEN 4096

#if defined(__linux__) && defined(NEED_TIMEVAL)
struct timeval
{
   long int tv_sec,tv_usec;
}
;
#endif

static rfbBool webSocketsHandshake(rfbClientPtr cl, char *scheme);

static int webSocketsEncodeHybi(rfbClientPtr cl, const char *src, int len, char **dst);

static int ws_read(void *cl, char *buf, size_t len);


static int
min (int a, int b) {
    return a < b ? a : b;
}

static void webSocketsGenSha1Key(char *target, int size, char *key)
{
    unsigned char hash[SHA1_HASH_SIZE];
    char tmp[strlen(key) + sizeof(GUID) - 1];
    memcpy(tmp, key, strlen(key));
    memcpy(tmp + strlen(key), GUID, sizeof(GUID) - 1);
    hash_sha1(hash, tmp, sizeof(tmp));
    if (-1 == rfbBase64NtoP(hash, sizeof(hash), target, size))
	rfbErr("rfbBase64NtoP failed\n");
}

/*
 * rfbWebSocketsHandshake is called to handle new WebSockets connections
 */

rfbBool
webSocketsCheck (rfbClientPtr cl)
{
    char bbuf[4], *scheme;
    int ret;

    ret = rfbPeekExactTimeout(cl, bbuf, 4,
                                   WEBSOCKETS_CLIENT_CONNECT_WAIT_MS);
    if ((ret < 0) && (errno == ETIMEDOUT)) {
      rfbLog("Normal socket connection\n");
      return TRUE;
    } else if (ret <= 0) {
      rfbErr("webSocketsHandshake: unknown connection error\n");
      return FALSE;
    }

    if (strncmp(bbuf, "RFB ", 4) == 0) {
        rfbLog("Normal socket connection\n");
        return TRUE;
    } else if (strncmp(bbuf, "\x16", 1) == 0 || strncmp(bbuf, "\x80", 1) == 0) {
        rfbLog("Got TLS/SSL WebSockets connection\n");
        if (-1 == rfbssl_init(cl)) {
	  rfbErr("webSocketsHandshake: rfbssl_init failed\n");
	  return FALSE;
	}
	ret = rfbPeekExactTimeout(cl, bbuf, 4, WEBSOCKETS_CLIENT_CONNECT_WAIT_MS);
        scheme = "wss";
    } else {
        scheme = "ws";
    }

    if (strncmp(bbuf, "GET ", 4) != 0) {
      rfbErr("webSocketsHandshake: invalid client header\n");
      return FALSE;
    }

    rfbLog("Got '%s' WebSockets handshake\n", scheme);

    if (!webSocketsHandshake(cl, scheme)) {
        return FALSE;
    }
    /* Start WebSockets framing */
    return TRUE;
}

static rfbBool
webSocketsHandshake(rfbClientPtr cl, char *scheme)
{
    char *buf, *response, *line;
    int n, linestart = 0, len = 0, llen, base64 = FALSE;
    char *path = NULL, *host = NULL, *origin = NULL, *protocol = NULL;
    char *key1 = NULL, *key2 = NULL;
    char *sec_ws_origin = NULL;
    char *sec_ws_key = NULL;
    char sec_ws_version = 0;
    ws_ctx_t *wsctx = NULL;

    buf = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!buf) {
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }
    response = (char *) malloc(WEBSOCKETS_MAX_HANDSHAKE_LEN);
    if (!response) {
        free(buf);
        rfbLogPerror("webSocketsHandshake: malloc");
        return FALSE;
    }

    while (len < WEBSOCKETS_MAX_HANDSHAKE_LEN-1) {
        if ((n = rfbReadExactTimeout(cl, buf+len, 1,
                                     WEBSOCKETS_CLIENT_SEND_WAIT_MS)) <= 0) {
            if ((n < 0) && (errno == ETIMEDOUT)) {
                break;
            }
            if (n == 0) {
                rfbLog("webSocketsHandshake: client gone\n");
            }
            else {
                rfbLogPerror("webSocketsHandshake: read");
            }

            free(response);
            free(buf);
            return FALSE;
        }

        len += 1;
        llen = len - linestart;
        if (((llen >= 2)) && (buf[len-1] == '\n')) {
            line = buf+linestart;
            if ((llen == 2) && (strncmp("\r\n", line, 2) == 0)) {
                if (key1 && key2 && len+8 < WEBSOCKETS_MAX_HANDSHAKE_LEN) {
                    if ((n = rfbReadExact(cl, buf+len, 8)) <= 0) {
                        if ((n < 0) && (errno == ETIMEDOUT)) {
                            break;
                        }
                        if (n == 0)
                            rfbLog("webSocketsHandshake: client gone\n");
                        else
                            rfbLogPerror("webSocketsHandshake: read");
                        free(response);
                        free(buf);
                        return FALSE;
                    }
                    len += 8;
                } else {
                    buf[len] = '\0';
                }
                break;
            } else if ((llen >= 16) && ((strncmp("GET ", line, min(llen,4))) == 0)) {
                /* 16 = 4 ("GET ") + 1 ("/.*") + 11 (" HTTP/1.1\r\n") */
                path = line+4;
                buf[len-11] = '\0'; /* Trim trailing " HTTP/1.1\r\n" */
                cl->wspath = strdup(path);
                /* rfbLog("Got path: %s\n", path); */
            } else if ((strncasecmp("host: ", line, min(llen,6))) == 0) {
                host = line+6;
                buf[len-2] = '\0';
                /* rfbLog("Got host: %s\n", host); */
            } else if ((strncasecmp("origin: ", line, min(llen,8))) == 0) {
                origin = line+8;
                buf[len-2] = '\0';
                /* rfbLog("Got origin: %s\n", origin); */
            } else if ((strncasecmp("sec-websocket-key1: ", line, min(llen,20))) == 0) {
                key1 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key1: %s\n", key1); */
            } else if ((strncasecmp("sec-websocket-key2: ", line, min(llen,20))) == 0) {
                key2 = line+20;
                buf[len-2] = '\0';
                /* rfbLog("Got key2: %s\n", key2); */
            /* HyBI */

            } else if ((strncasecmp("sec-websocket-protocol: ", line, min(llen,24))) == 0) {
                protocol = line+24;
                buf[len-2] = '\0';
                rfbLog("Got protocol: %s\n", protocol);
            } else if ((strncasecmp("sec-websocket-origin: ", line, min(llen,22))) == 0) {
                sec_ws_origin = line+22;
                buf[len-2] = '\0';
            } else if ((strncasecmp("sec-websocket-key: ", line, min(llen,19))) == 0) {
                sec_ws_key = line+19;
                buf[len-2] = '\0';
            } else if ((strncasecmp("sec-websocket-version: ", line, min(llen,23))) == 0) {
                sec_ws_version = strtol(line+23, NULL, 10);
                buf[len-2] = '\0';
            }

            linestart = len;
        }
    }
    
    /* older hixie handshake, this could be removed if
     * a final standard is established -- removed now */
    if (!sec_ws_version) {
        rfbErr("Hixie no longer supported\n");
        free(response);
        free(buf);
        return FALSE;
    } 

    if (!sec_ws_key) {
        rfbErr("webSocketsHandshake: sec-websocket-key is missing\n");
        free(response);
        free(buf);
        return FALSE;
    }

    if (!(path && host && (origin || sec_ws_origin))) {
        rfbErr("webSocketsHandshake: incomplete client handshake\n");
        free(response);
        free(buf);
        return FALSE;
    }

    if ((protocol) && (strstr(protocol, "base64"))) {
        rfbLog("  - webSocketsHandshake: using base64 encoding\n");
        base64 = TRUE;
        protocol = "base64";
    } else {
        rfbLog("  - webSocketsHandshake: using binary/raw encoding\n");
        if ((protocol) && (strstr(protocol, "binary"))) {
            protocol = "binary";
        } else {
            protocol = "";
        }
    }

    /*
     * Generate the WebSockets server response based on the the headers sent
     * by the client.
     */
    char accept[B64LEN(SHA1_HASH_SIZE) + 1];
    rfbLog("  - WebSockets client version hybi-%02d\n", sec_ws_version);
    webSocketsGenSha1Key(accept, sizeof(accept), sec_ws_key);

    if(strlen(protocol) > 0) {
        len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
                 SERVER_HANDSHAKE_HYBI, accept, protocol);
    } else {
        len = snprintf(response, WEBSOCKETS_MAX_HANDSHAKE_LEN,
                       SERVER_HANDSHAKE_HYBI_NO_PROTOCOL, accept);
    }

    if (rfbWriteExact(cl, response, len) < 0) {
        rfbErr("webSocketsHandshake: failed sending WebSockets response\n");
        free(response);
        free(buf);
        return FALSE;
    }
    /* rfbLog("webSocketsHandshake: %s\n", response); */
    free(response);
    free(buf);

    wsctx = calloc(1, sizeof(ws_ctx_t));
    if (!wsctx) {
        rfbErr("webSocketsHandshake: could not allocate memory for context\n");
        return FALSE;
    }
    wsctx->encode = webSocketsEncodeHybi;
    wsctx->decode = webSocketsDecodeHybi;
    wsctx->ctxInfo.readFunc = ws_read;
    wsctx->base64 = base64;
    hybiDecodeCleanupComplete(wsctx);
    cl->wsctx = (wsCtx *)wsctx;
    return TRUE;
}

static int
ws_read(void *ctxPtr, char *buf, size_t len)
{
    int n;
    rfbClientPtr cl = ctxPtr;
    if (cl->sslctx) {
        n = rfbssl_read(cl, buf, len);
    } else {
        n = read(cl->sock, buf, len);
    }
    return n;
}

static int
webSocketsEncodeHybi(rfbClientPtr cl, const char *src, int len, char **dst)
{
    int blen, ret = -1, sz = 0;
    unsigned char opcode = '\0'; /* TODO: option! */
    ws_header_t *header;
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;


    /* Optional opcode:
     *   0x0 - continuation
     *   0x1 - text frame (base64 encode buf)
     *   0x2 - binary frame (use raw buf)
     *   0x8 - connection close
     *   0x9 - ping
     *   0xA - pong
    **/
    if (!len) {
	  /* nothing to encode */
	  return 0;
    }
    if (len > UPDATE_BUF_SIZE) {
      rfbErr("%s: Data length %d larger than maximum of %d bytes\n", __func__, len, UPDATE_BUF_SIZE);
      return -1;
    }

    header = (ws_header_t *)wsctx->codeBufEncode;

    if (wsctx->base64) {
        opcode = WS_OPCODE_TEXT_FRAME;
        /* calculate the resulting size */
        blen = B64LEN(len);
    } else {
        opcode = WS_OPCODE_BINARY_FRAME;
        blen = len;
    }

    header->b0 = 0x80 | (opcode & 0x0f);
    if (blen <= 125) {
      header->b1 = (uint8_t)blen;
      sz = 2;
    } else if (blen <= 65536) {
      header->b1 = 0x7e;
      header->u.s16.l16 = WS_HTON16((uint16_t)blen);
      sz = 4;
    } else {
      header->b1 = 0x7f;
      header->u.s64.l64 = WS_HTON64(blen);
      sz = 10;
    }

    if (wsctx->base64) {
        if (-1 == (ret = rfbBase64NtoP((unsigned char *)src, len, wsctx->codeBufEncode + sz, sizeof(wsctx->codeBufEncode) - sz))) {
            rfbErr("%s: Base 64 encode failed\n", __func__);
        } else {
          if (ret != blen)
            rfbErr("%s: Base 64 encode; something weird happened\n", __func__);
          ret += sz;
        }
    } else {
        memcpy(wsctx->codeBufEncode + sz, src, len);
        ret =  sz + len;
    }

    *dst = wsctx->codeBufEncode;

    return ret;
}

int
webSocketsEncode(rfbClientPtr cl, const char *src, int len, char **dst)
{
    return webSocketsEncodeHybi(cl, src, len, dst);
}

int
webSocketsDecode(rfbClientPtr cl, char *dst, int len)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx; 
    wsctx->ctxInfo.ctxPtr = cl;
    return webSocketsDecodeHybi(wsctx, dst, len);
}

/**
 * This is a stub function that was once used for Hixie-encoding.
 * We keep it for API compatibility.
 */
rfbBool
webSocketCheckDisconnect(rfbClientPtr cl)
{
    return FALSE;
}


/* returns TRUE if there is data waiting to be read in our internal buffer
 * or if is there any pending data in the buffer of the SSL implementation
 */
rfbBool
webSocketsHasDataInBuffer(rfbClientPtr cl)
{
    ws_ctx_t *wsctx = (ws_ctx_t *)cl->wsctx;

    if (wsctx && wsctx->readlen)
        return TRUE;

    return (cl->sslctx && rfbssl_pending(cl) > 0);
}
