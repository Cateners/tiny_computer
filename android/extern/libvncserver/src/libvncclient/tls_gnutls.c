/*
 *  Copyright (C) 2009 Vic Lee.
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

#include <stdio.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <rfb/rfbclient.h>
#include <errno.h>
#include "tls.h"


static const char *rfbTLSPriority = "NORMAL:+DHE-DSS:+RSA:+DHE-RSA:+SRP";
static const char *rfbAnonTLSPriority = "NORMAL:+ANON-ECDH:+ANON-DH";

#define DH_BITS 1024
static gnutls_dh_params_t rfbDHParams;

static rfbBool rfbTLSInitialized = FALSE;

static int
verify_certificate_callback (gnutls_session_t session)
{
  unsigned int status;
  const gnutls_datum_t *cert_list;
  unsigned int cert_list_size;
  int ret;
  gnutls_x509_crt_t cert;
  rfbClient *sptr;
  char *hostname;

  sptr = (rfbClient *)gnutls_session_get_ptr(session);
  if (!sptr) {
    rfbClientLog("Failed to validate certificate - missing client data\n");
    return GNUTLS_E_CERTIFICATE_ERROR;
  }

  hostname = sptr->serverHost;
  if (!hostname) {
      rfbClientLog("No server hostname found for client\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
  }

  /* This verification function uses the trusted CAs in the credentials
   * structure. So you must have installed one or more CA certificates.
   */
  ret = gnutls_certificate_verify_peers2 (session, &status);
  if (ret < 0)
    {
      rfbClientLog ("Certificate validation call failed\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  if (status & GNUTLS_CERT_INVALID)
    rfbClientLog("The certificate is not trusted.\n");

  if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
    rfbClientLog("The certificate hasn't got a known issuer.\n");

  if (status & GNUTLS_CERT_REVOKED)
    rfbClientLog("The certificate has been revoked.\n");

  if (status & GNUTLS_CERT_EXPIRED)
    rfbClientLog("The certificate has expired\n");

  if (status & GNUTLS_CERT_NOT_ACTIVATED)
    rfbClientLog("The certificate is not yet activated\n");

  if (status)
    return GNUTLS_E_CERTIFICATE_ERROR;

  /* Up to here the process is the same for X.509 certificates and
   * OpenPGP keys. From now on X.509 certificates are assumed. This can
   * be easily extended to work with openpgp keys as well.
   */
  if (gnutls_certificate_type_get (session) != GNUTLS_CRT_X509) {
    rfbClientLog("The certificate was not X509\n");
    return GNUTLS_E_CERTIFICATE_ERROR;
  }

  if (gnutls_x509_crt_init (&cert) < 0)
    {
      rfbClientLog("Error initialising certificate structure\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  cert_list = gnutls_certificate_get_peers (session, &cert_list_size);
  if (cert_list == NULL)
    {
      rfbClientLog("No certificate was found!\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  if (gnutls_x509_crt_import (cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
    {
      rfbClientLog("Error parsing certificate\n");
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  if (!gnutls_x509_crt_check_hostname (cert, hostname))
    {
      rfbClientLog("The certificate's owner does not match hostname '%s'\n",
              hostname);
      return GNUTLS_E_CERTIFICATE_ERROR;
    }

  gnutls_x509_crt_deinit (cert);

  /* notify gnutls to continue handshake normally */
  return 0;
}

static rfbBool
InitializeTLS(void)
{
  int ret;

  if (rfbTLSInitialized) return TRUE;
  if ((ret = gnutls_global_init()) < 0 ||
      (ret = gnutls_dh_params_init(&rfbDHParams)) < 0 ||
      (ret = gnutls_dh_params_generate2(rfbDHParams, DH_BITS)) < 0)
  {
    rfbClientLog("Failed to initialized GnuTLS: %s.\n", gnutls_strerror(ret));
    return FALSE;
  }
  rfbClientLog("GnuTLS version %s initialized.\n", gnutls_check_version(NULL));
  rfbTLSInitialized = TRUE;
  return TRUE;
}

/*
 * On Windows, translate WSAGetLastError() to errno values as GNU TLS does it
 * internally too. This is necessary because send() and recv() on Windows
 * don't set errno when they fail but GNUTLS expects a proper errno value.
 *
 * Use gnutls_transport_set_global_errno() like the GNU TLS documentation
 * suggests to avoid problems with different errno variables when GNU TLS and
 * libvncclient are linked to different versions of msvcrt.dll.
 */
#ifdef WIN32
static void WSAtoTLSErrno(gnutls_session_t* session)
{
  switch(WSAGetLastError()) {
#if (GNUTLS_VERSION_NUMBER >= 0x029901)
  case WSAEWOULDBLOCK:
    gnutls_transport_set_errno(session, EAGAIN);
    break;
  case WSAEINTR:
    gnutls_transport_set_errno(session, EINTR);
    break;
  default:
    gnutls_transport_set_errno(session, EIO);
    break;
#else
  case WSAEWOULDBLOCK:
    gnutls_transport_set_global_errno(EAGAIN);
    break;
  case WSAEINTR:
    gnutls_transport_set_global_errno(EINTR);
    break;
  default:
    gnutls_transport_set_global_errno(EIO);
    break;
#endif
  }
}
#endif

static ssize_t
PushTLS(gnutls_transport_ptr_t transport, const void *data, size_t len)
{
  rfbClient *client = (rfbClient*)transport;
  int ret;

  while (1)
  {
    ret = write(client->sock, data, len);
    if (ret < 0)
    {
#ifdef WIN32
      WSAtoTLSErrno((gnutls_session_t*)&client->tlsSession);
#endif
      if (errno == EINTR) continue;
      return -1;
    }
    return ret;
  }
}


static ssize_t
PullTLS(gnutls_transport_ptr_t transport, void *data, size_t len)
{
  rfbClient *client = (rfbClient*)transport;
  int ret;

  while (1)
  {
    ret = read(client->sock, data, len);
    if (ret < 0)
    {
#ifdef WIN32
      WSAtoTLSErrno((gnutls_session_t*)&client->tlsSession);
#endif
      if (errno == EINTR) continue;
      return -1;
    }
    return ret;
  }
}

static int
PullTimeout(gnutls_transport_ptr_t transport, unsigned int timeout)
{
  rfbClient *client = (rfbClient*)transport;
  int ret;

  while (1)
  {
    ret = gnutls_system_recv_timeout((gnutls_transport_ptr_t)(long)client->sock, timeout);

    if (ret < 0)
    {
#ifdef WIN32
      WSAtoTLSErrno((gnutls_session_t*)&client->tlsSession);
#endif
      if (errno == EINTR) continue;
    }
    return ret;
  }
}

static rfbBool
InitializeTLSSession(rfbClient* client, rfbBool anonTLS)
{
  int ret;
  const char *p;

  if (client->tlsSession) return TRUE;

  if ((ret = gnutls_init((gnutls_session_t*)&client->tlsSession, GNUTLS_CLIENT)) < 0)
  {
    rfbClientLog("Failed to initialized TLS session: %s.\n", gnutls_strerror(ret));
    return FALSE;
  }

  if ((ret = gnutls_priority_set_direct((gnutls_session_t)client->tlsSession,
    anonTLS ? rfbAnonTLSPriority : rfbTLSPriority, &p)) < 0)
  {
    rfbClientLog("Warning: Failed to set TLS priority: %s (%s).\n", gnutls_strerror(ret), p);
  }

  gnutls_transport_set_ptr((gnutls_session_t)client->tlsSession, (gnutls_transport_ptr_t)client);
  gnutls_transport_set_push_function((gnutls_session_t)client->tlsSession, PushTLS);
  gnutls_transport_set_pull_function((gnutls_session_t)client->tlsSession, PullTLS);

  gnutls_transport_set_pull_timeout_function((gnutls_session_t)client->tlsSession, PullTimeout);
  gnutls_handshake_set_timeout((gnutls_session_t)client->tlsSession, 15000);

  INIT_MUTEX(client->tlsRwMutex);

  rfbClientLog("TLS session initialized.\n");

  return TRUE;
}

static rfbBool
SetTLSAnonCredential(rfbClient* client)
{
  gnutls_anon_client_credentials_t anonCred;
  int ret;

  if ((ret = gnutls_anon_allocate_client_credentials(&anonCred)) < 0 ||
      (ret = gnutls_credentials_set((gnutls_session_t)client->tlsSession, GNUTLS_CRD_ANON, anonCred)) < 0)
  {
    FreeTLS(client);
    rfbClientLog("Failed to create anonymous credentials: %s", gnutls_strerror(ret));
    return FALSE;
  }
  rfbClientLog("TLS anonymous credential created.\n");
  return TRUE;
}

static rfbBool
HandshakeTLS(rfbClient* client)
{
  int ret;

  while ((ret = gnutls_handshake((gnutls_session_t)client->tlsSession)) < 0)
  {
    if (!gnutls_error_is_fatal(ret))
    {
      rfbClientLog("TLS handshake got a temporary error: %s.\n", gnutls_strerror(ret));
      continue;
    }
    rfbClientLog("TLS handshake failed: %s\n", gnutls_strerror(ret));
    FreeTLS(client);
    return FALSE;
  }

  rfbClientLog("TLS handshake done.\n");
  return TRUE;
}

/* VeNCrypt sub auth. 1 byte auth count, followed by count * 4 byte integers */
static rfbBool
ReadVeNCryptSecurityType(rfbClient* client, uint32_t *result)
{
    uint8_t count=0;
    uint8_t loop=0;
    uint32_t tAuth[256], t;
    char buf1[500],buf2[10];
    uint32_t origAuthScheme, authScheme;

    if (!ReadFromRFBServer(client, (char *)&count, 1)) return FALSE;

    if (count==0)
    {
        rfbClientLog("List of security types is ZERO. Giving up.\n");
        return FALSE;
    }

    rfbClientLog("We have %d security types to read\n", count);
    authScheme=0;
    /* now, we have a list of available security types to read ( uint8_t[] ) */
    for (loop=0;loop<count;loop++)
    {
        if (!ReadFromRFBServer(client, (char *)&tAuth[loop], 4)) return FALSE;
        t=rfbClientSwap32IfLE(tAuth[loop]);
        rfbClientLog("%d) Received security type %d\n", loop, t);
        if (t==rfbNoAuth ||
            t==rfbVncAuth ||
            t==rfbVeNCryptPlain ||
            t==rfbVeNCryptTLSNone ||
            t==rfbVeNCryptTLSVNC ||
            t==rfbVeNCryptTLSPlain ||
#ifdef LIBVNCSERVER_HAVE_SASL
            t==rfbVeNCryptTLSSASL ||
            t==rfbVeNCryptX509SASL ||
#endif /*LIBVNCSERVER_HAVE_SASL */
            t==rfbVeNCryptX509None ||
            t==rfbVeNCryptX509VNC ||
            t==rfbVeNCryptX509Plain)
        {
            if (
                authScheme==0 ||
                authScheme==rfbNoAuth ||
                authScheme==rfbVncAuth ||
                authScheme==rfbVeNCryptPlain)
            {
                /* for security reasons, the encrypted type has a higher priority */
                origAuthScheme=tAuth[loop];
                authScheme=t;
            }
        }
        tAuth[loop]=t;
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
        rfbClientLog("Unknown VeNCrypt authentication scheme from VNC server: %s\n",
               buf1);
        return FALSE;
    }
    else
    {
        rfbClientLog("Selecting security type %d\n", authScheme);
        /* send back 4 bytes (in original byte order!) indicating which security type to use */
        if (!WriteToRFBServer(client, (char *)&origAuthScheme, 4)) return FALSE;
    }
    *result = authScheme;
    return TRUE;
}

static void
FreeX509Credential(rfbCredential *cred)
{
  if (cred->x509Credential.x509CACertFile) free(cred->x509Credential.x509CACertFile);
  if (cred->x509Credential.x509CACrlFile) free(cred->x509Credential.x509CACrlFile);
  if (cred->x509Credential.x509ClientCertFile) free(cred->x509Credential.x509ClientCertFile);
  if (cred->x509Credential.x509ClientKeyFile) free(cred->x509Credential.x509ClientKeyFile);
  free(cred);
}

static gnutls_certificate_credentials_t
CreateX509CertCredential(rfbCredential *cred)
{
  gnutls_certificate_credentials_t x509_cred;
  int ret;

  if (!cred->x509Credential.x509CACertFile)
  {
    rfbClientLog("No CA certificate provided.\n");
    return NULL;
  }

  if ((ret = gnutls_certificate_allocate_credentials(&x509_cred)) < 0)
  {
    rfbClientLog("Cannot allocate credentials: %s.\n", gnutls_strerror(ret));
    return NULL;
  }
  if ((ret = gnutls_certificate_set_x509_trust_file(x509_cred,
    cred->x509Credential.x509CACertFile, GNUTLS_X509_FMT_PEM)) < 0)
  {
    rfbClientLog("Cannot load CA credentials: %s.\n", gnutls_strerror(ret));
    gnutls_certificate_free_credentials (x509_cred);
    return NULL;
  }
  if (cred->x509Credential.x509ClientCertFile && cred->x509Credential.x509ClientKeyFile)
  {
    if ((ret = gnutls_certificate_set_x509_key_file(x509_cred,
      cred->x509Credential.x509ClientCertFile, cred->x509Credential.x509ClientKeyFile,
      GNUTLS_X509_FMT_PEM)) < 0)
    {
      rfbClientLog("Cannot load client certificate or key: %s.\n", gnutls_strerror(ret));
      gnutls_certificate_free_credentials (x509_cred);
      return NULL;
    }
  } else
  {
    rfbClientLog("No client certificate or key provided.\n");
  }
  if (cred->x509Credential.x509CACrlFile)
  {
    if ((ret = gnutls_certificate_set_x509_crl_file(x509_cred,
      cred->x509Credential.x509CACrlFile, GNUTLS_X509_FMT_PEM)) < 0)
    {
      rfbClientLog("Cannot load CRL: %s.\n", gnutls_strerror(ret));
      gnutls_certificate_free_credentials (x509_cred);
      return NULL;
    }
  } else
  {
    rfbClientLog("No CRL provided.\n");
  }
  gnutls_certificate_set_dh_params (x509_cred, rfbDHParams);
  return x509_cred;
}


rfbBool
HandleAnonTLSAuth(rfbClient* client)
{
  if (!InitializeTLS() || !InitializeTLSSession(client, TRUE)) return FALSE;

  if (!SetTLSAnonCredential(client)) return FALSE;

  if (!HandshakeTLS(client)) return FALSE;

  return TRUE;
}

rfbBool
HandleVeNCryptAuth(rfbClient* client)
{
  uint8_t major, minor, status;
  uint32_t authScheme;
  rfbBool anonTLS;
  gnutls_certificate_credentials_t x509_cred = NULL;
  int ret;

  /* Read VeNCrypt version */
  if (!ReadFromRFBServer(client, (char *)&major, 1) ||
      !ReadFromRFBServer(client, (char *)&minor, 1))
  {
    return FALSE;
  }
  rfbClientLog("Got VeNCrypt version %d.%d from server.\n", (int)major, (int)minor);

  if (major != 0 && minor != 2)
  {
    rfbClientLog("Unsupported VeNCrypt version.\n");
    return FALSE;
  }

  if (!WriteToRFBServer(client, (char *)&major, 1) ||
      !WriteToRFBServer(client, (char *)&minor, 1) ||
      !ReadFromRFBServer(client, (char *)&status, 1))
  {
    return FALSE;
  }

  if (status != 0)
  {
    rfbClientLog("Server refused VeNCrypt version %d.%d.\n", (int)major, (int)minor);
    return FALSE;
  }

  if (!ReadVeNCryptSecurityType(client, &authScheme)) return FALSE;
  client->subAuthScheme = authScheme;

  switch (authScheme)
  {
    /* Unencrypted types do not require additional actions */
    case rfbNoAuth:
    case rfbVncAuth:
    case rfbVeNCryptPlain:
      return TRUE;
      break;

    /* Some VeNCrypt security types are anonymous TLS, others are X509 */
    case rfbVeNCryptTLSNone:
    case rfbVeNCryptTLSVNC:
    case rfbVeNCryptTLSPlain:
#ifdef LIBVNCSERVER_HAVE_SASL
    case rfbVeNCryptTLSSASL:
#endif /* LIBVNCSERVER_HAVE_SASL */
      anonTLS = TRUE;
      break;

    default:
      anonTLS = FALSE;
      break;
  }

  /* Ack is only requred for the encrypted connection */
  if (!ReadFromRFBServer(client, (char *)&status, 1) || status != 1)
  {
    rfbClientLog("Server refused VeNCrypt authentication %d (%d).\n", authScheme, (int)status);
    return FALSE;
  }

  if (!InitializeTLS()) return FALSE;

  /* Get X509 Credentials if it's not anonymous */
  if (!anonTLS)
  {
    rfbCredential *cred;

    if (!client->GetCredential)
    {
      rfbClientLog("GetCredential callback is not set.\n");
      return FALSE;
    }
    cred = client->GetCredential(client, rfbCredentialTypeX509);
    if (!cred)
    {
      rfbClientLog("Reading credential failed\n");
      return FALSE;
    }

    x509_cred = CreateX509CertCredential(cred);
    FreeX509Credential(cred);
    if (!x509_cred) return FALSE;
  }

  /* Start up the TLS session */
  if (!InitializeTLSSession(client, anonTLS)) return FALSE;

  if (anonTLS)
  {
    if (!SetTLSAnonCredential(client)) return FALSE;
  }
  else
  {
    /* Set the certificate verification callback. */
    gnutls_certificate_set_verify_function (x509_cred, verify_certificate_callback);
    gnutls_session_set_ptr ((gnutls_session_t)client->tlsSession, (void *)client);

    if ((ret = gnutls_credentials_set((gnutls_session_t)client->tlsSession, GNUTLS_CRD_CERTIFICATE, x509_cred)) < 0)
    {
      rfbClientLog("Cannot set x509 credential: %s.\n", gnutls_strerror(ret));
      FreeTLS(client);
      return FALSE;
    }
  }

  if (!HandshakeTLS(client)) return FALSE;

  /* We are done here. The caller should continue with client->subAuthScheme
   * to do actual sub authentication.
   */
  return TRUE;
}

int
ReadFromTLS(rfbClient* client, char *out, unsigned int n)
{
  ssize_t ret;

  LOCK(client->tlsRwMutex);
  ret = gnutls_record_recv((gnutls_session_t)client->tlsSession, out, n);
  UNLOCK(client->tlsRwMutex);

  if (ret >= 0) return ret;
  if (ret == GNUTLS_E_REHANDSHAKE || ret == GNUTLS_E_AGAIN)
  {
    errno = EAGAIN;
  } else
  {
    rfbClientLog("Error reading from TLS: %s.\n", gnutls_strerror(ret));
    errno = EINTR;
  }
  return -1;
}

int
WriteToTLS(rfbClient* client, const char *buf, unsigned int n)
{
  unsigned int offset = 0;
  ssize_t ret;

  while (offset < n)
  {
    LOCK(client->tlsRwMutex);
    ret = gnutls_record_send((gnutls_session_t)client->tlsSession, buf+offset, (size_t)(n-offset));
    UNLOCK(client->tlsRwMutex);

    if (ret == 0) continue;
    if (ret < 0)
    {
      if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED) continue;
      rfbClientLog("Error writing to TLS: %s.\n", gnutls_strerror(ret));
      return -1;
    }
    offset += (unsigned int)ret;
  }

  return offset;
}

void FreeTLS(rfbClient* client)
{
  if (client->tlsSession)
  {
    gnutls_deinit((gnutls_session_t)client->tlsSession);
    client->tlsSession = NULL;
    TINI_MUTEX(client->tlsRwMutex);
  }
}

#ifdef LIBVNCSERVER_HAVE_SASL
int
GetTLSCipherBits(rfbClient* client)
{
    gnutls_cipher_algorithm_t cipher = gnutls_cipher_get((gnutls_session_t)client->tlsSession);

    return gnutls_cipher_get_key_size(cipher) * 8;
}
#endif /* LIBVNCSERVER_HAVE_SASL */

