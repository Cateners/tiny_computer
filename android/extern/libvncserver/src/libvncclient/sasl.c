/*
 * The software in this file is derived from the vncconnection.c source file
 * from the GTK VNC Widget with modifications by S. Waterman <simon.waterman@zynstra.com>
 * for compatibility with libvncserver.  The copyright and license
 * statements below apply only to this source file and to no other parts of the
 * libvncserver library.
 *
 * GTK VNC Widget
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 * Copyright (C) 2009-2010 Daniel P. Berrange <dan@berrange.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/*
 * sasl.c - functions to deal with client side of the SASL protocol.
 */

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#define _POSIX_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <errno.h>
#include <rfb/rfbclient.h>

#include "sockets.h"

#include "sasl.h"

#include "tls.h"

#ifdef _MSC_VER
#  define snprintf _snprintf /* MSVC went straight to the underscored syntax */
#endif

/*
 * NB, keep in sync with similar method in qemud/remote.c
 */
static char *vnc_connection_addr_to_string(char *host, int port)
{
    char * buf = (char *)malloc(strlen(host) + 7);
    if (buf) {
        sprintf(buf, "%s;%hu", host, (unsigned short)port);
    }
    return buf;
}

static int log_func(void *context,
                    int level,
                    const char *message)
{
   rfbClientLog("SASL: %s\n", message);

   return SASL_OK;
}

static int user_callback_adapt(void *context,
                          int id,
                          const char **result,
                          unsigned *len)
{
   rfbClient* client = (rfbClient *)context;

   if (id != SASL_CB_AUTHNAME) {
       rfbClientLog("Unrecognized SASL callback ID %d\n", id);
       return SASL_FAIL;
   }

   if (!client->GetUser) {
       rfbClientLog("Client user callback not found\n");
       return SASL_FAIL;
   }

   *result = client->GetUser(client);

   if (! *result) return SASL_FAIL;
   /**len = strlen(*result);*/
   return SASL_OK;
}

static int password_callback_adapt(sasl_conn_t *conn,
                                   void * context,
                                   int id,
                                   sasl_secret_t **secret)
{
   rfbClient* client = (rfbClient *)context;
   char * password;

   if (id != SASL_CB_PASS) {
       rfbClientLog("Unrecognized SASL callback ID %d\n", id);
       return SASL_FAIL;
   }

   if (client->saslSecret) { /* If we've already got it just return it. */
       *secret = client->saslSecret;
       return SASL_OK;
   }

   if (!client->GetPassword) {
       rfbClientLog("Client password callback not found\n");
       return SASL_FAIL;
   }

   password = client->GetPassword(client);

   if (! password) return SASL_FAIL;

   sasl_secret_t *lsec = (sasl_secret_t *)malloc(sizeof(sasl_secret_t) + strlen(password));
   if (!lsec) {
       rfbClientLog("Could not allocate sasl_secret_t\n");
       return SASL_FAIL;
   }

   strcpy((char *)lsec->data, password);
   lsec->len = strlen(password);
   if (client->saslSecret)
       free(client->saslSecret);
   client->saslSecret = lsec;
   *secret = lsec;

   /* Clear client password */
   size_t i;
   for (i = 0; i < lsec->len; i++) {
        password[i] = '\0';
   }
   free(password);

   return SASL_OK;
}

#define SASL_MAX_MECHLIST_LEN 300
#define SASL_MAX_DATA_LEN (1024 * 1024)

/* Perform the SASL authentication process
 */
rfbBool
HandleSASLAuth(rfbClient *client)
{
    sasl_conn_t *saslconn = NULL;
    sasl_security_properties_t secprops;
    const char *clientout;
    char *serverin = NULL;
    unsigned int clientoutlen, serverinlen;
    int err, complete = 0;
    char *localAddr = NULL, *remoteAddr = NULL;
    const void *val;
    sasl_ssf_t ssf;
    sasl_callback_t saslcb[] = {
        {SASL_CB_LOG, (void *)log_func, NULL},
        {SASL_CB_AUTHNAME, client->GetUser ? (void *)user_callback_adapt : NULL, client},
        {SASL_CB_PASS, client->GetPassword ? (void *)password_callback_adapt : NULL, client},
        { .id = 0 },
    };
    sasl_interact_t *interact = NULL;
    uint32_t mechlistlen;
    char *mechlist;
    char *wantmech;
    const char *mechname;

    if (client->saslconn)
        sasl_dispose(&client->saslconn);

    /* Sets up the SASL library as a whole */
    err = sasl_client_init(NULL);
    rfbClientLog("Client initialize SASL authentication %d\n", err);
    if (err != SASL_OK) {
        rfbClientLog("failed to initialize SASL library: %d (%s)\n",
                  err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    /* Get local address in form  IPADDR:PORT */
    struct sockaddr_storage localAddress;
    socklen_t addressLength = sizeof(localAddress);
    char buf[INET6_ADDRSTRLEN];
    int  port;

    if (getsockname(client->sock, (struct sockaddr*)&localAddress, &addressLength)) {
        rfbClientLog("failed to get local address\n");
        goto error;
    }

    if (localAddress.ss_family == AF_INET) {
        struct sockaddr_in *sa_in = (struct sockaddr_in*)&localAddress;
        inet_ntop(AF_INET, &(sa_in->sin_addr), buf, INET_ADDRSTRLEN);
        port = ntohs(sa_in->sin_port);
        localAddr = vnc_connection_addr_to_string(buf, port);
    } else if (localAddress.ss_family == AF_INET6) {
        struct sockaddr_in6 *sa_in = (struct sockaddr_in6*)&localAddress;
        inet_ntop(AF_INET6, &(sa_in->sin6_addr), buf, INET6_ADDRSTRLEN);
        port = ntohs(sa_in->sin6_port);
        localAddr = vnc_connection_addr_to_string(buf, port);
    } else {
        rfbClientLog("failed to get local address\n");
        goto error;
    }

    /* Get remote address in form  IPADDR:PORT */
    remoteAddr = vnc_connection_addr_to_string(client->serverHost, client->serverPort);

    rfbClientLog("Client SASL new host:'%s' local:'%s' remote:'%s'\n", client->serverHost, localAddr, remoteAddr);

    /* Setup a handle for being a client */
    err = sasl_client_new("vnc",
                          client->serverHost,
                          localAddr,
                          remoteAddr,
                          saslcb,
                          SASL_SUCCESS_DATA,
                          &saslconn);
    free(localAddr);
    free(remoteAddr);

    if (err != SASL_OK) {
        rfbClientLog("Failed to create SASL client context: %d (%s)\n",
                  err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    /* Initialize some connection props we care about */
    if (client->tlsSession) {
        if (!(ssf = (sasl_ssf_t)GetTLSCipherBits(client))) {
            rfbClientLog("%s", "invalid cipher size for TLS session\n");
            goto error;
        }

        rfbClientLog("Setting external SSF %d\n", ssf);
        err = sasl_setprop(saslconn, SASL_SSF_EXTERNAL, &ssf);
        if (err != SASL_OK) {
            rfbClientLog("cannot set external SSF %d (%s)\n",
                      err, sasl_errstring(err, NULL, NULL));
            goto error;
        }
    }

    memset (&secprops, 0, sizeof secprops);
    /* If we've got TLS, we don't care about SSF */
    secprops.min_ssf = client->tlsSession ? 0 : 56; /* Equiv to DES supported by all Kerberos */
    secprops.max_ssf = client->tlsSession ? 0 : 100000; /* Very strong ! AES == 256 */
    secprops.maxbufsize = 100000;
    /* If we're not TLS, then forbid any anonymous or trivially crackable auth */
    secprops.security_flags = client->tlsSession ? 0 :
        SASL_SEC_NOANONYMOUS | SASL_SEC_NOPLAINTEXT;

    err = sasl_setprop(saslconn, SASL_SEC_PROPS, &secprops);
    if (err != SASL_OK) {
        rfbClientLog("cannot set security props %d (%s)\n",
                  err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    /* Get the supported mechanisms from the server */
    if (!ReadFromRFBServer(client, (char *)&mechlistlen, 4)) {
        rfbClientLog("failed to read mechlistlen\n");
        goto error;
    }
    mechlistlen = rfbClientSwap32IfLE(mechlistlen);
    rfbClientLog("mechlistlen is %d\n", mechlistlen);
    if (mechlistlen > SASL_MAX_MECHLIST_LEN) {
        rfbClientLog("mechlistlen %d too long\n", mechlistlen);
        goto error;
    }

    mechlist = malloc(mechlistlen+1);
    if (!mechlist || !ReadFromRFBServer(client, mechlist, mechlistlen)) {
        free(mechlist);
        goto error;
    }
    mechlist[mechlistlen] = '\0';

    /* Allow the client to influence the mechanism selected. */
    if (client->GetSASLMechanism) {
        wantmech = client->GetSASLMechanism(client, mechlist);
        
        if (wantmech && *wantmech != 0) {
            if (strstr(mechlist, wantmech) == NULL) {
                rfbClientLog("Client requested SASL mechanism %s not supported by server\n",
                             wantmech);
                free(mechlist);
                free(wantmech);
                goto error;
            } else {
                free(mechlist);
                mechlist = wantmech;
            }
        }
    }

    rfbClientLog("Client start negotiation mechlist '%s'\n", mechlist);

    /* Start the auth negotiation on the client end first */
    err = sasl_client_start(saslconn,
                            mechlist,
                            &interact,
                            &clientout,
                            &clientoutlen,
                            &mechname);
    if (err != SASL_OK && err != SASL_CONTINUE && err != SASL_INTERACT) {
        rfbClientLog("Failed to start SASL negotiation: %d (%s)\n",
                  err, sasl_errdetail(saslconn));
        free(mechlist);
        mechlist = NULL;
        goto error;
    }

    /* Need to gather some credentials from the client */
    if (err == SASL_INTERACT) {
        rfbClientLog("User interaction required but not currently supported\n");
        goto error;
    }

    rfbClientLog("Server start negotiation with mech %s. Data %d bytes %p '%s'\n",
              mechname, clientoutlen, clientout, clientout);

    if (clientoutlen > SASL_MAX_DATA_LEN) {
        rfbClientLog("SASL negotiation data too long: %d bytes\n",
                  clientoutlen);
        goto error;
    }

    /* Send back the chosen mechname */
    uint32_t mechnamelen = rfbClientSwap32IfLE(strlen(mechname));
    if (!WriteToRFBServer(client, (char *)&mechnamelen, 4)) goto error;
    if (!WriteToRFBServer(client, (char *)mechname, strlen(mechname))) goto error;

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (clientout) {
        uint32_t colsw = rfbClientSwap32IfLE(clientoutlen + 1);
        if (!WriteToRFBServer(client, (char *)&colsw, 4)) goto error;
        if (!WriteToRFBServer(client, (char *)clientout, clientoutlen + 1)) goto error;
    } else {
        uint32_t temp = 0;
        if (!WriteToRFBServer(client, (char *)&temp, 4)) goto error;
    }

    rfbClientLog("%s", "Getting sever start negotiation reply\n");
    /* Read the 'START' message reply from server */
    if (!ReadFromRFBServer(client, (char *)&serverinlen, 4)) goto error;
    serverinlen = rfbClientSwap32IfLE(serverinlen);

    if (serverinlen > SASL_MAX_DATA_LEN) {
        rfbClientLog("SASL negotiation data too long: %d bytes\n",
                  serverinlen);
        goto error;
    }

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (serverinlen) {
        serverin = malloc(serverinlen);
        if (!serverin || !ReadFromRFBServer(client, serverin, serverinlen)) goto error;
        serverin[serverinlen-1] = '\0';
        serverinlen--;
    } else {
        serverin = NULL;
    }
    if (!ReadFromRFBServer(client, (char *)&complete, 1)) goto error;

    rfbClientLog("Client start result complete: %d. Data %d bytes %p '%s'\n",
              complete, serverinlen, serverin, serverin);

    /* Loop-the-loop...
     * Even if the server has completed, the client must *always* do at least one step
     * in this loop to verify the server isn't lying about something. Mutual auth */
    for (;;) {
        err = sasl_client_step(saslconn,
                               serverin,
                               serverinlen,
                               &interact,
                               &clientout,
                               &clientoutlen);
        if (err != SASL_OK && err != SASL_CONTINUE && err != SASL_INTERACT) {
            rfbClientLog("Failed SASL step: %d (%s)\n",
                      err, sasl_errdetail(saslconn));
            goto error;
        }

        /* Need to gather some credentials from the client */
        if (err == SASL_INTERACT) {
            rfbClientLog("User interaction required but not currently supported\n");
            goto error;
        }

        if (serverin) {
            free(serverin);
            serverin = NULL;
        }

        rfbClientLog("Client step result %d. Data %d bytes %p '%s'\n", err, clientoutlen, clientout, clientout);

        /* Previous server call showed completion & we're now locally complete too */
        if (complete && err == SASL_OK)
            break;

        /* Not done, prepare to talk with the server for another iteration */

        /* NB, distinction of NULL vs "" is *critical* in SASL */
        if (clientout) {
            uint32_t colsw = rfbClientSwap32IfLE(clientoutlen + 1);
            if (!WriteToRFBServer(client, (char *)&colsw, 4)) goto error;
            if (!WriteToRFBServer(client, (char *)clientout, clientoutlen + 1)) goto error;
        } else {
            uint32_t temp = 0;
            if (!WriteToRFBServer(client, (char *)&temp, 4)) goto error;
        }

        rfbClientLog("Server step with %d bytes %p\n", clientoutlen, clientout);

        if (!ReadFromRFBServer(client, (char *)&serverinlen, 4)) goto error;
        serverinlen = rfbClientSwap32IfLE(serverinlen);

        if (serverinlen > SASL_MAX_DATA_LEN) {
            rfbClientLog("SASL negotiation data too long: %d bytes\n",
                      serverinlen);
            goto error;
        }

    /* NB, distinction of NULL vs "" is *critical* in SASL */
        if (serverinlen) {
            serverin = malloc(serverinlen);
            if (!serverin || !ReadFromRFBServer(client, serverin, serverinlen)) goto error;
            serverin[serverinlen-1] = '\0';
            serverinlen--;
        } else {
            serverin = NULL;
        }
        if (!ReadFromRFBServer(client, (char *)&complete, 1)) goto error;

        rfbClientLog("Client step result complete: %d. Data %d bytes %p '%s'\n",
                  complete, serverinlen, serverin, serverin);

        /* This server call shows complete, and earlier client step was OK */
        if (complete && err == SASL_OK) {
            free(serverin);
            serverin = NULL;
            break;
        }
    }

    /* Check for suitable SSF if non-TLS */
    if (!client->tlsSession) {
        err = sasl_getprop(saslconn, SASL_SSF, &val);
        if (err != SASL_OK) {
            rfbClientLog("cannot query SASL ssf on connection %d (%s)\n",
                      err, sasl_errstring(err, NULL, NULL));
            goto error;
        }
        ssf = *(const int *)val;
        rfbClientLog("SASL SSF value %d\n", ssf);
        if (ssf < 56) { /* 56 == DES level, good for Kerberos */
            rfbClientLog("negotiation SSF %d was not strong enough\n", ssf);
            goto error;
        }
    }

    rfbClientLog("%s", "SASL authentication complete\n");

    uint32_t result;
    if (!ReadFromRFBServer(client, (char *)&result, 4)) {
        rfbClientLog("Failed to read authentication result\n");
        goto error;
    }
    result = rfbClientSwap32IfLE(result);

    if (result != 0) {
        rfbClientLog("Authentication failure\n");
        goto error;
    }
    rfbClientLog("Authentication successful - switching to SSF\n");

    /* This must come *after* check-auth-result, because the former
     * is defined to be sent unencrypted, and setting saslconn turns
     * on the SSF layer encryption processing */
    client->saslconn = saslconn;

    /* Clear SASL secret from memory if set - it'll be free'd on dispose */
    if (client->saslSecret) {
        size_t i;
        for (i = 0; i < client->saslSecret->len; i++)
            client->saslSecret->data[i] = '\0';
        client->saslSecret->len = 0;
    }
        
    return TRUE;

 error:
    if (client->saslSecret) {
        size_t i;
        for (i = 0; i < client->saslSecret->len; i++)
            client->saslSecret->data[i] = '\0';
        client->saslSecret->len = 0;
    }

    if (saslconn)
        sasl_dispose(&saslconn);
    return FALSE;
}

int
ReadFromSASL(rfbClient* client, char *out, unsigned int n)
{
    size_t want;

    if (client->saslDecoded == NULL) {
        char *encoded;
        int encodedLen;
        int err, ret;

        encodedLen = 8192;
        encoded = (char *)malloc(encodedLen);
        if (!encoded) {
            errno = EIO;
            return -EIO;
        }
        ret = read(client->sock, encoded, encodedLen);
        if (ret < 0) {
            free(encoded);
            return ret;
        }
        if (ret == 0) {
            free(encoded);
            errno = EIO;
            return -EIO;
        }

        err = sasl_decode(client->saslconn, encoded, ret,
                          &client->saslDecoded, &client->saslDecodedLength);
        free(encoded);
        if (err != SASL_OK) {
	    rfbClientLog("Failed to decode SASL data %s\n",
                      sasl_errstring(err, NULL, NULL));
            return -EINVAL;
        }
        client->saslDecodedOffset = 0;
    }

    want = client->saslDecodedLength - client->saslDecodedOffset;
    if (want > n)
        want = n;

    memcpy(out,
           client->saslDecoded + client->saslDecodedOffset,
           want);
    client->saslDecodedOffset += want;
    if (client->saslDecodedOffset == client->saslDecodedLength) {
        client->saslDecodedLength = client->saslDecodedOffset = 0;
        client->saslDecoded = NULL;
    }

    if (!want) {
        errno = EAGAIN;
        return -EAGAIN;
    }

    return want;
}
