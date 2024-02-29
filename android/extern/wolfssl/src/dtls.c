/* dtls.c
 *
 * Copyright (C) 2006-2022 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * WOLFSSL_DTLS_NO_HVR_ON_RESUME
 *     If defined, a DTLS server will not do a cookie exchange on successful
 *     client resumption: the resumption will be faster (one RTT less) and
 *     will consume less bandwidth (one ClientHello and one HelloVerifyRequest
 *     less). On the other hand, if a valid SessionID is collected, forged
 *     clientHello messages will consume resources on the server.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#ifndef WOLFCRYPT_ONLY

#include <wolfssl/error-ssl.h>
#include <wolfssl/internal.h>
#include <wolfssl/ssl.h>
#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif

#ifdef WOLFSSL_DTLS

void DtlsResetState(WOLFSSL* ssl)
{
    /* Reset the state so that we can statelessly await the
     * ClientHello that contains the cookie. Don't gate on IsAtLeastTLSv1_3
     * to handle the edge case when the peer wants a lower version. */

#ifdef WOLFSSL_SEND_HRR_COOKIE
    /* Remove cookie so that it will get computed again */
    TLSX_Remove(&ssl->extensions, TLSX_COOKIE, ssl->heap);
#endif

    /* Reset DTLS window */
#ifdef WOLFSSL_DTLS13
    w64Zero(&ssl->dtls13Epochs[0].nextSeqNumber);
    w64Zero(&ssl->dtls13Epochs[0].nextPeerSeqNumber);
    XMEMSET(ssl->dtls13Epochs[0].window, 0,
        sizeof(ssl->dtls13Epochs[0].window));
    Dtls13FreeFsmResources(ssl);
#endif
    ssl->keys.dtls_expected_peer_handshake_number = 0;
    ssl->keys.dtls_handshake_number = 0;

    /* Reset states */
    ssl->options.serverState = NULL_STATE;
    ssl->options.clientState = NULL_STATE;
    ssl->options.connectState = CONNECT_BEGIN;
    ssl->options.acceptState = ACCEPT_BEGIN;
    ssl->options.handShakeState = NULL_STATE;
    ssl->msgsReceived.got_client_hello = 0;
    ssl->keys.dtls_handshake_number = 0;
    ssl->keys.dtls_expected_peer_handshake_number = 0;
    ssl->options.clientState = 0;
    XMEMSET(ssl->keys.peerSeq->window, 0, sizeof(ssl->keys.peerSeq->window));
    XMEMSET(ssl->keys.peerSeq->prevWindow, 0,
        sizeof(ssl->keys.peerSeq->prevWindow));
}

#if !defined(NO_WOLFSSL_SERVER)

#if defined(NO_SHA) && defined(NO_SHA256)
#error "DTLS needs either SHA or SHA-256"
#endif /* NO_SHA && NO_SHA256 */

#if !defined(NO_SHA) && defined(NO_SHA256)
#define DTLS_COOKIE_TYPE WC_SHA
#define DTLS_COOKIE_SZ WC_SHA_DIGEST_SIZE
#endif /* !NO_SHA && NO_SHA256 */

#ifndef NO_SHA256
#define DTLS_COOKIE_TYPE WC_SHA256
#define DTLS_COOKIE_SZ WC_SHA256_DIGEST_SIZE
#endif /* !NO_SHA256 */

typedef struct WolfSSL_ConstVector {
    word32 size;
    const byte* elements;
} WolfSSL_ConstVector;

typedef struct WolfSSL_CH {
    ProtocolVersion* pv;
    const byte* random;
    WolfSSL_ConstVector sessionId;
    WolfSSL_ConstVector cookie;
    WolfSSL_ConstVector cipherSuite;
    WolfSSL_ConstVector compression;
    WolfSSL_ConstVector extension;
    word32 length;
} WolfSSL_CH;

static int ReadVector8(const byte* input, WolfSSL_ConstVector* v)
{
    v->size = *input;
    v->elements = input + OPAQUE8_LEN;
    return v->size + OPAQUE8_LEN;
}

static int ReadVector16(const byte* input, WolfSSL_ConstVector* v)
{
    word16 size16;
    ato16(input, &size16);
    v->size = (word32)size16;
    v->elements = input + OPAQUE16_LEN;
    return v->size + OPAQUE16_LEN;
}

static int CreateDtlsCookie(WOLFSSL* ssl, const WolfSSL_CH* ch, byte* cookie)
{
    Hmac cookieHmac;
    int ret;

    ret = wc_HmacInit(&cookieHmac, ssl->heap, ssl->devId);
    if (ret != 0)
        return ret;
    ret = wc_HmacSetKey(&cookieHmac, DTLS_COOKIE_TYPE,
        ssl->buffers.dtlsCookieSecret.buffer,
        ssl->buffers.dtlsCookieSecret.length);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (const byte*)ssl->buffers.dtlsCtx.peer.sa,
        ssl->buffers.dtlsCtx.peer.sz);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (byte*)ch->pv, OPAQUE16_LEN);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (byte*)ch->random, RAN_LEN);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (byte*)ch->sessionId.elements,
        ch->sessionId.size);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (byte*)ch->cipherSuite.elements,
        ch->cipherSuite.size);
    if (ret != 0)
        goto out;
    ret = wc_HmacUpdate(&cookieHmac, (byte*)ch->compression.elements,
        ch->compression.size);
    if (ret != 0)
        goto out;
    ret = wc_HmacFinal(&cookieHmac, cookie);

out:
    wc_HmacFree(&cookieHmac);
    return ret;
}

static int ParseClientHello(const byte* input, word32 helloSz, WolfSSL_CH* ch)
{
    word32 idx = 0;

    /* protocol version, random and session id length check */
    if (OPAQUE16_LEN + RAN_LEN + OPAQUE8_LEN > helloSz)
        return BUFFER_ERROR;

    ch->pv = (ProtocolVersion*)(input + idx);
    idx += OPAQUE16_LEN;
    ch->random = (byte*)(input + idx);
    idx += RAN_LEN;
    idx += ReadVector8(input + idx, &ch->sessionId);
    if (idx > helloSz - OPAQUE8_LEN)
        return BUFFER_ERROR;
    idx += ReadVector8(input + idx, &ch->cookie);
    if (idx > helloSz - OPAQUE16_LEN)
        return BUFFER_ERROR;
    idx += ReadVector16(input + idx, &ch->cipherSuite);
    if (idx > helloSz - OPAQUE8_LEN)
        return BUFFER_ERROR;
    idx += ReadVector8(input + idx, &ch->compression);
    if (idx > helloSz - OPAQUE16_LEN)
        return BUFFER_ERROR;
    idx += ReadVector16(input + idx, &ch->extension);
    if (idx > helloSz)
        return BUFFER_ERROR;
    ch->length = idx;
    return 0;
}

#ifdef WOLFSSL_DTLS_NO_HVR_ON_RESUME
#ifdef HAVE_SESSION_TICKET
static int TlsxFindByType(WolfSSL_ConstVector* ret, word16 extType,
    WolfSSL_ConstVector exts)
{
    word32 len, idx = 0;
    word16 type;
    WolfSSL_ConstVector ext;

    XMEMSET(ret, 0, sizeof(*ret));
    len = exts.size;
    /* type + len */
    while (len >= OPAQUE16_LEN + OPAQUE16_LEN) {
        ato16(exts.elements + idx, &type);
        idx += OPAQUE16_LEN;
        idx += ReadVector16(exts.elements + idx, &ext);
        if (idx > exts.size)
            return BUFFER_ERROR;
        if (type == extType) {
            XMEMCPY(ret, &ext, sizeof(ext));
            return 0;
        }
        len = exts.size - idx;
    }
    return 0;
}

static int TlsTicketIsValid(WOLFSSL* ssl, WolfSSL_ConstVector exts,
    byte* isValid)
{
    WolfSSL_ConstVector tlsxSessionTicket;
    byte tempTicket[SESSION_TICKET_LEN];
    InternalTicket* it;
    int ret;

    *isValid = 0;
    ret = TlsxFindByType(&tlsxSessionTicket, TLSX_SESSION_TICKET, exts);
    if (ret != 0)
        return ret;
    if (tlsxSessionTicket.size == 0)
        return 0;
    if (tlsxSessionTicket.size > SESSION_TICKET_LEN)
        return 0;
    XMEMCPY(tempTicket, tlsxSessionTicket.elements, tlsxSessionTicket.size);
    ret = DoDecryptTicket(ssl, tempTicket, (word32)tlsxSessionTicket.size, &it);
    if (ret != WOLFSSL_TICKET_RET_OK && ret != WOLFSSL_TICKET_RET_CREATE)
        return 0;
    ForceZero(it, sizeof(InternalTicket));
    *isValid = 1;
    return 0;
}
#endif /* HAVE_SESSION_TICKET */

static int TlsSessionIdIsValid(WOLFSSL* ssl, WolfSSL_ConstVector sessionID,
    byte* isValid)
{
    WOLFSSL_SESSION* sess;
    word32 sessRow;
    int ret;

    *isValid = 0;
    if (ssl->options.sessionCacheOff)
        return 0;
    if (sessionID.size != ID_LEN)
        return 0;
#ifdef HAVE_EXT_CACHE
    {

        if (ssl->ctx->get_sess_cb != NULL) {
            int unused;
            sess =
                ssl->ctx->get_sess_cb(ssl, sessionID.elements, ID_LEN, &unused);
            if (sess != NULL) {
                *isValid = 1;
                wolfSSL_FreeSession(ssl->ctx, sess);
                return 0;
            }
        }
        if (ssl->ctx->internalCacheLookupOff)
            return 0;
    }
#endif
    ret = TlsSessionCacheGetAndLock(sessionID.elements, &sess, &sessRow);
    if (ret == 0 && sess != NULL) {
        *isValid = 1;
        TlsSessionCacheUnlockRow(sessRow);
    }

    return 0;
}

static int TlsResumptionIsValid(WOLFSSL* ssl, WolfSSL_CH* ch, byte* isValid)
{
    int ret;

    *isValid = 0;
#ifdef HAVE_SESSION_TICKET
    ret = TlsTicketIsValid(ssl, ch->extension, isValid);
    if (ret != 0)
        return ret;
    if (*isValid)
        return 0;
#endif /* HAVE_SESSION_TICKET */
    ret = TlsSessionIdIsValid(ssl, ch->sessionId, isValid);
    return ret;
}
#endif /* WOLFSSL_DTLS_NO_HVR_ON_RESUME */

int DoClientHelloStateless(WOLFSSL* ssl, const byte* input, word32* inOutIdx,
    word32 helloSz, byte* process)
{
    byte cookie[DTLS_COOKIE_SZ];
    int ret;
    WolfSSL_CH ch;

    *process = 1;
    ret = ParseClientHello(input + *inOutIdx, helloSz, &ch);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_DTLS_NO_HVR_ON_RESUME
    {
        byte isValid = 0;
        ret = TlsResumptionIsValid(ssl, &ch, &isValid);
        if (ret != 0)
            return ret;
        if (isValid)
            return 0;
    }
#endif /* WOLFSSL_DTLS_NO_HVR_ON_RESUME */

    ret = CreateDtlsCookie(ssl, &ch, cookie);
    if (ret != 0)
        return ret;
    if (ch.cookie.size != DTLS_COOKIE_SZ ||
        XMEMCMP(ch.cookie.elements, cookie, DTLS_COOKIE_SZ) != 0) {
        *process = 0;
        ret = SendHelloVerifyRequest(ssl, cookie, DTLS_COOKIE_SZ);
    }

    return ret;
}
#endif /* !defined(NO_WOLFSSL_SERVER) */

#if defined(WOLFSSL_DTLS_CID)

typedef struct ConnectionID {
    byte length;
/* Ignore "nonstandard extension used : zero-sized array in struct/union"
 * MSVC warning */
#ifdef _MSC_VER
#pragma warning(disable: 4200)
#endif
    byte id[];
} ConnectionID;

typedef struct CIDInfo {
    ConnectionID* tx;
    ConnectionID* rx;
    byte negotiated : 1;
} CIDInfo;

static ConnectionID* DtlsCidNew(const byte* cid, byte size, void* heap)
{
    ConnectionID* ret;

    ret = (ConnectionID*)XMALLOC(sizeof(ConnectionID) + size, heap,
        DYNAMIC_TYPE_TLSX);
    if (ret == NULL)
        return NULL;

    ret->length = size;
    XMEMCPY(ret->id, cid, size);

    return ret;
}

static WC_INLINE CIDInfo* DtlsCidGetInfo(WOLFSSL* ssl)
{
    return ssl->dtlsCidInfo;
}

static int DtlsCidGetSize(WOLFSSL* ssl, unsigned int* size, int rx)
{
    ConnectionID* id;
    CIDInfo* info;

    if (ssl == NULL || size == NULL)
        return BAD_FUNC_ARG;

    info = DtlsCidGetInfo(ssl);
    if (info == NULL)
        return WOLFSSL_FAILURE;

    id = rx ? info->rx : info->tx;
    if (id == NULL) {
        *size = 0;
        return WOLFSSL_SUCCESS;
    }

    *size = id->length;
    return WOLFSSL_SUCCESS;
}

static int DtlsCidGet(WOLFSSL* ssl, unsigned char* buf, int bufferSz, int rx)
{
    ConnectionID* id;
    CIDInfo* info;

    if (ssl == NULL || buf == NULL)
        return BAD_FUNC_ARG;

    info = DtlsCidGetInfo(ssl);
    if (info == NULL)
        return WOLFSSL_FAILURE;

    id = rx ? info->rx : info->tx;
    if (id == NULL || id->length == 0)
        return WOLFSSL_SUCCESS;

    if (id->length > bufferSz)
        return LENGTH_ERROR;

    XMEMCPY(buf, id->id, id->length);
    return WOLFSSL_SUCCESS;
}

static CIDInfo* DtlsCidGetInfoFromExt(byte* ext)
{
    WOLFSSL** sslPtr;
    WOLFSSL* ssl;

    if (ext == NULL)
        return NULL;
    sslPtr = (WOLFSSL**)ext;
    ssl = *sslPtr;
    if (ssl == NULL)
        return NULL;
    return ssl->dtlsCidInfo;
}

static void DtlsCidUnsetInfoFromExt(byte* ext)
{
    WOLFSSL** sslPtr;
    WOLFSSL* ssl;

    if (ext == NULL)
        return;
    sslPtr = (WOLFSSL**)ext;
    ssl = *sslPtr;
    if (ssl == NULL)
        return;
    ssl->dtlsCidInfo = NULL;
}

void TLSX_ConnectionID_Free(byte* ext, void* heap)
{
    CIDInfo* info;
    (void)heap;

    info = DtlsCidGetInfoFromExt(ext);
    if (info == NULL)
        return;
    if (info->rx != NULL)
        XFREE(info->rx, heap, DYNAMIC_TYPE_TLSX);
    if (info->tx != NULL)
        XFREE(info->tx, heap, DYNAMIC_TYPE_TLSX);
    XFREE(info, heap, DYNAMIC_TYPE_TLSX);
    DtlsCidUnsetInfoFromExt(ext);
    XFREE(ext, heap, DYNAMIC_TYPE_TLSX);
}

word16 TLSX_ConnectionID_Write(byte* ext, byte* output)
{
    CIDInfo* info;

    info = DtlsCidGetInfoFromExt(ext);
    if (info == NULL)
        return 0;

    /* empty CID */
    if (info->rx == NULL) {
        *output = 0;
        return OPAQUE8_LEN;
    }

    *output = info->rx->length;
    XMEMCPY(output + OPAQUE8_LEN, info->rx->id, info->rx->length);
    return OPAQUE8_LEN + info->rx->length;
}

word16 TLSX_ConnectionID_GetSize(byte* ext)
{
    CIDInfo* info = DtlsCidGetInfoFromExt(ext);
    if (info == NULL)
        return 0;
    return info->rx == NULL ? OPAQUE8_LEN : OPAQUE8_LEN + info->rx->length;
}

int TLSX_ConnectionID_Use(WOLFSSL* ssl)
{
    CIDInfo* info;
    WOLFSSL** ext;
    int ret;

    ext = (WOLFSSL**)TLSX_Find(ssl->extensions, TLSX_CONNECTION_ID);
    if (ext != NULL)
        return 0;

    info = (CIDInfo*)XMALLOC(sizeof(CIDInfo), ssl->heap, DYNAMIC_TYPE_TLSX);
    if (info == NULL)
        return MEMORY_ERROR;
    ext = (WOLFSSL**)XMALLOC(sizeof(WOLFSSL**), ssl->heap, DYNAMIC_TYPE_TLSX);
    if (ext == NULL) {
        XFREE(info, ssl->heap, DYNAMIC_TYPE_TLSX);
        return MEMORY_ERROR;
    }
    XMEMSET(info, 0, sizeof(CIDInfo));
    /* CIDInfo needs to be accessed every time we send or receive a record. To
     * avoid the cost of the extension lookup save a pointer to the structure
     * inside the SSL object itself, and save a pointer to the SSL object in the
     * extension. The extension freeing routine uses te pointer to the SSL
     * object to find the structure and to set ssl->dtlsCidInfo pointer to NULL
     * after freeing the structure. */
    ssl->dtlsCidInfo = info;
    *ext = ssl;
    ret =
        TLSX_Push(&ssl->extensions, TLSX_CONNECTION_ID, (void*)ext, ssl->heap);
    if (ret != 0) {
        XFREE(info, ssl->heap, DYNAMIC_TYPE_TLSX);
        XFREE(ext, ssl->heap, DYNAMIC_TYPE_TLSX);
        ssl->dtlsCidInfo = NULL;
        return ret;
    }

    return 0;
}

int TLSX_ConnectionID_Parse(WOLFSSL* ssl, const byte* input, word16 length,
    byte isRequest)
{
    ConnectionID* id;
    CIDInfo* info;
    byte cidSize;
    TLSX* ext;

    ext = TLSX_Find(ssl->extensions, TLSX_CONNECTION_ID);
    if (ext == NULL) {
        /* CID not enabled */
        if (isRequest) {
            WOLFSSL_MSG("Received CID ext but it's not enabled, ignoring");
            return 0;
        }
        else {
            WOLFSSL_MSG("CID ext not requested by the Client, aborting");
            return UNSUPPORTED_EXTENSION;
        }
    }

    info = DtlsCidGetInfo(ssl);
    if (info == NULL)
        return BAD_STATE_E;

    /* it may happen if we process two ClientHello because the server sent an
     * HRR request */
    if (info->tx != NULL) {
        if (ssl->options.side != WOLFSSL_SERVER_END &&
            ssl->options.serverState != SERVER_HELLO_RETRY_REQUEST_COMPLETE)
            return BAD_STATE_E;

        XFREE(info->tx, ssl->heap, DYNAMIC_TYPE_TLSX);
        info->tx = NULL;
    }

    if (length < OPAQUE8_LEN)
        return BUFFER_ERROR;

    cidSize = *input;
    if (cidSize + OPAQUE8_LEN > length)
        return BUFFER_ERROR;

    if (cidSize > 0) {
        id = (ConnectionID*)XMALLOC(sizeof(*id) + cidSize, ssl->heap,
            DYNAMIC_TYPE_TLSX);
        if (id == NULL)
            return MEMORY_ERROR;
        XMEMCPY(id->id, input + OPAQUE8_LEN, cidSize);
        id->length = cidSize;
        info->tx = id;
    }

    info->negotiated = 1;
    if (isRequest)
        ext->resp = 1;

    return 0;
}

void DtlsCIDOnExtensionsParsed(WOLFSSL* ssl)
{
    CIDInfo* info;

    info = DtlsCidGetInfo(ssl);
    if (info == NULL)
        return;

    if (!info->negotiated) {
        TLSX_Remove(&ssl->extensions, TLSX_CONNECTION_ID, ssl->heap);
        return;
    }
}

byte DtlsCIDCheck(WOLFSSL* ssl, const byte* input, word16 inputSize)
{
    CIDInfo* info;
    info = DtlsCidGetInfo(ssl);
    if (info == NULL || info->rx == NULL || info->rx->length == 0)
        return 0;
    if (inputSize < info->rx->length)
        return 0;
    return XMEMCMP(input, info->rx->id, info->rx->length) == 0;
}

int wolfSSL_dtls_cid_use(WOLFSSL* ssl)
{
    int ret;

    /* CID is supported on DTLSv1.3 only */
    if (!IsAtLeastTLSv1_3(ssl->version))
        return WOLFSSL_FAILURE;

    ssl->options.useDtlsCID = 1;
    ret = TLSX_ConnectionID_Use(ssl);
    if (ret != 0)
        return ret;
    return WOLFSSL_SUCCESS;
}

int wolfSSL_dtls_cid_is_enabled(WOLFSSL* ssl)
{
    return DtlsCidGetInfo(ssl) != NULL;
}

int wolfSSL_dtls_cid_set(WOLFSSL* ssl, unsigned char* cid, unsigned int size)
{
    ConnectionID* newCid;
    CIDInfo* cidInfo;

    if (!ssl->options.useDtlsCID)
        return WOLFSSL_FAILURE;

    cidInfo = DtlsCidGetInfo(ssl);
    if (cidInfo == NULL)
        return WOLFSSL_FAILURE;

    if (cidInfo->rx != NULL) {
        XFREE(cidInfo->rx, ssl->heap, DYNAMIC_TYPE_TLSX);
        cidInfo->rx = NULL;
    }

    /* empty CID */
    if (size == 0)
        return WOLFSSL_SUCCESS;

    if (size > DTLS_CID_MAX_SIZE)
        return LENGTH_ERROR;

    newCid = DtlsCidNew(cid, (byte)size, ssl->heap);
    if (newCid == NULL)
        return MEMORY_ERROR;
    cidInfo->rx = newCid;
    return WOLFSSL_SUCCESS;
}

int wolfSSL_dtls_cid_get_rx_size(WOLFSSL* ssl, unsigned int* size)
{
    return DtlsCidGetSize(ssl, size, 1);
}

int wolfSSL_dtls_cid_get_rx(WOLFSSL* ssl, unsigned char* buf,
    unsigned int bufferSz)
{
    return DtlsCidGet(ssl, buf, bufferSz, 1);
}

int wolfSSL_dtls_cid_get_tx_size(WOLFSSL* ssl, unsigned int* size)
{
    return DtlsCidGetSize(ssl, size, 0);
}

int wolfSSL_dtls_cid_get_tx(WOLFSSL* ssl, unsigned char* buf,
    unsigned int bufferSz)
{
    return DtlsCidGet(ssl, buf, bufferSz, 0);
}

#endif /* WOLFSSL_DTLS_CID */
#endif /* WOLFSSL_DTLS */

#endif /* WOLFCRYPT_ONLY */
