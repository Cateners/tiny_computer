/*
 *  Copyright (C) 2012 Philip Van Hoof <philip@codeminded.be>
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
#include <rfb/rfbclient.h>
#include <errno.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

#include "tls.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

static rfbBool rfbTLSInitialized = FALSE;

// Locking callbacks are only initialized if we have mutex support.
#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD) || defined(LIBVNCSERVER_HAVE_WIN32THREADS)

static MUTEX(*mutex_buf) = NULL;

struct CRYPTO_dynlock_value {
	MUTEX(mutex);
};

static void locking_function(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		LOCK(mutex_buf[n]);
	else
		UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void)
{
	return ((unsigned long) CURRENT_THREAD_ID);
}

static struct CRYPTO_dynlock_value *dyn_create_function(const char *file, int line)
{
	struct CRYPTO_dynlock_value *value;

	value = (struct CRYPTO_dynlock_value *)
		malloc(sizeof(struct CRYPTO_dynlock_value));
	if (!value)
		goto err;
	INIT_MUTEX(value->mutex);

	return value;

err:
	return (NULL);
}

static void dyn_lock_function (int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		LOCK(l->mutex);
	else
		UNLOCK(l->mutex);
}


static void
dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	TINI_MUTEX(l->mutex);
	free(l);
}

static rfbBool InitLockingCb()
{
  mutex_buf = malloc(CRYPTO_num_locks() * MUTEX_SIZE);
  if (mutex_buf == NULL) {
    rfbClientLog("Failed to initialized OpenSSL: memory.\n");
    return FALSE;
  }

  int i;
  for (i = 0; i < CRYPTO_num_locks(); i++)
    INIT_MUTEX(mutex_buf[i]);

  CRYPTO_set_locking_callback(locking_function);
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

  return TRUE;
}

#else
//If mutex support is not available, locking initialization is a no-op.
static rfbBool InitLockingCb() { return TRUE; }
#endif

static int
ssl_error_to_errno (int ssl_error)
{
	switch (ssl_error) {
	case SSL_ERROR_NONE:
		return 0;
	case SSL_ERROR_ZERO_RETURN:
		/* this one does not map well at all */
		//d(printf ("ssl_errno: SSL_ERROR_ZERO_RETURN\n"));
		return EINVAL;
	case SSL_ERROR_WANT_READ:   /* non-fatal; retry */
	case SSL_ERROR_WANT_WRITE:  /* non-fatal; retry */
		//d(printf ("ssl_errno: SSL_ERROR_WANT_[READ,WRITE]\n"));
		return EAGAIN;
	case SSL_ERROR_SYSCALL:
		//d(printf ("ssl_errno: SSL_ERROR_SYSCALL\n"));
		return EINTR;
	case SSL_ERROR_SSL:
		//d(printf ("ssl_errno: SSL_ERROR_SSL  <-- very useful error...riiiiight\n"));
		return EINTR;
	default:
		//d(printf ("ssl_errno: default error\n"));
		return EINTR;
	}
}

static rfbBool
InitializeTLS(void)
{
  if (rfbTLSInitialized)
    return TRUE;

  if (!InitLockingCb())
    return FALSE;

  SSL_load_error_strings();
  SSLeay_add_ssl_algorithms();
  RAND_load_file("/dev/urandom", 1024);

  rfbClientLog("OpenSSL version %s initialized.\n", SSLeay_version(SSLEAY_VERSION));
  rfbTLSInitialized = TRUE;
  return TRUE;
}

static int sock_read_ready(SSL *ssl, uint32_t ms)
{
	int r = 0;
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);

	FD_SET(SSL_get_fd(ssl), &fds);

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	
	r = select (SSL_get_fd(ssl) + 1, &fds, NULL, NULL, &tv); 

	return r;
}

static int wait_for_data(SSL *ssl, int ret, int timeout)
{
  int err;
  int retval = 1;

  err = SSL_get_error(ssl, ret);
	
  switch(err)
  {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      ret = sock_read_ready(ssl, timeout*1000);
			
      if (ret == -1) {
        retval = 2;
      }
				
      break;
    default:
      retval = 3;
      long verify_res = SSL_get_verify_result(ssl);
      if (verify_res != X509_V_OK)
        rfbClientLog("Could not verify server certificate: %s.\n",
                     X509_verify_cert_error_string(verify_res));
      break;
   }
	
  ERR_clear_error();
				
  return retval;
}

static rfbBool
load_crls_from_file(char *file, SSL_CTX *ssl_ctx)
{
  X509_STORE *st;
  int i;
  int count = 0;
  BIO *bio;
  STACK_OF(X509_INFO) *xis = NULL;
  X509_INFO *xi;

  st = SSL_CTX_get_cert_store(ssl_ctx);

  bio = BIO_new_file(file, "r");
  if (bio == NULL)
    return FALSE;

  xis = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL);
  BIO_free(bio);

  for (i = 0; i < sk_X509_INFO_num(xis); i++)
  {
    xi = sk_X509_INFO_value(xis, i);
    if (xi->crl)
    {
      X509_STORE_add_crl(st, xi->crl);
      xi->crl = NULL;
      count++;
    }
  }

  sk_X509_INFO_pop_free(xis, X509_INFO_free);

  if (count > 0)
    return TRUE;
  else
    return FALSE;
}

static SSL *
open_ssl_connection (rfbClient *client, int sockfd, rfbBool anonTLS, rfbCredential *cred)
{
  SSL_CTX *ssl_ctx = NULL;
  SSL *ssl = NULL;
  int n, finished = 0;
  X509_VERIFY_PARAM *param;
  uint8_t verify_crls;

  if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
  {
    rfbClientLog("Could not create new SSL context.\n");
    return NULL;
  }

  param = X509_VERIFY_PARAM_new();

  /* Setup verification if not anonymous */
  if (!anonTLS)
  {
    verify_crls = cred->x509Credential.x509CrlVerifyMode;
    if (cred->x509Credential.x509CACertFile)
    {
      if (!SSL_CTX_load_verify_locations(ssl_ctx, cred->x509Credential.x509CACertFile, NULL))
      {
        rfbClientLog("Failed to load CA certificate from %s.\n",
                     cred->x509Credential.x509CACertFile);
        goto error_free_ctx;
      }
    } else {
      rfbClientLog("Using default paths for certificate verification.\n");
      SSL_CTX_set_default_verify_paths (ssl_ctx);
    }

    if (cred->x509Credential.x509CACrlFile)
    {
      if (!load_crls_from_file(cred->x509Credential.x509CACrlFile, ssl_ctx))
      {
        rfbClientLog("CRLs could not be loaded.\n");
        goto error_free_ctx;
      }
      if (verify_crls == rfbX509CrlVerifyNone) verify_crls = rfbX509CrlVerifyAll;
    }

    if (cred->x509Credential.x509ClientCertFile && cred->x509Credential.x509ClientKeyFile)
    {
      if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cred->x509Credential.x509ClientCertFile) != 1)
      {
        rfbClientLog("Client certificate could not be loaded.\n");
        goto error_free_ctx;
      }

      if (SSL_CTX_use_PrivateKey_file(ssl_ctx, cred->x509Credential.x509ClientKeyFile,
                                      SSL_FILETYPE_PEM) != 1)
      {
        rfbClientLog("Client private key could not be loaded.\n");
        goto error_free_ctx;
      }

      if (SSL_CTX_check_private_key(ssl_ctx) == 0) {
        rfbClientLog("Client certificate and private key do not match.\n");
        goto error_free_ctx;
      }
    }

    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);

    if (verify_crls == rfbX509CrlVerifyClient) 
      X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK);
    else if (verify_crls == rfbX509CrlVerifyAll)
      X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);

    if(!X509_VERIFY_PARAM_set1_host(param, client->serverHost, strlen(client->serverHost)))
    {
      rfbClientLog("Could not set server name for verification.\n");
      goto error_free_ctx;
    }
    SSL_CTX_set1_param(ssl_ctx, param);
    SSL_CTX_set_cipher_list(ssl_ctx, "ALL");
  } else { /* anonTLS here */
      /* Need anonymous ciphers for anonTLS, see https://github.com/LibVNC/libvncserver/issues/347#issuecomment-597477103 */
#ifdef LIBWOLFSSL_VERSION_STRING
      SSL_CTX_set_cipher_list(ssl_ctx, "ADH-AES256-GCM-SHA384:ADH-AES128-SHA"); // wolfSSL requires full cipher names
#else
      SSL_CTX_set_cipher_list(ssl_ctx, "aNULL");
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined LIBRESSL_VERSION_NUMBER) ||\
    defined(LIBWOLFSSL_VERSION_STRING)
      /*
	See https://www.openssl.org/docs/man1.1.0/man3/SSL_set_security_level.html
	Not specifying 0 here makes LibVNCClient fail connecting to some servers.
      */
      SSL_CTX_set_security_level(ssl_ctx, 0);
      /*
	Specifying a maximum protocol version of 1.2 gets us ADH cipher on OpenSSL 1.1.x,
	see https://github.com/LibVNC/libvncserver/issues/347#issuecomment-597974313
       */
      SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_2_VERSION);
#endif
  }

  if (!(ssl = SSL_new (ssl_ctx)))
  {
    rfbClientLog("Could not create a new SSL session.\n");
    goto error_free_ctx;
  }

  SSL_set_fd (ssl, sockfd);
  SSL_CTX_set_app_data (ssl_ctx, client);

  do
  {
    n = SSL_connect(ssl);
		
    if (n != 1) 
    {
      if (wait_for_data(ssl, n, 1) != 1) 
      {
        finished = 1;
        SSL_shutdown(ssl);

        goto error_free_ssl;
      }
    }
  } while( n != 1 && finished != 1 );

  X509_VERIFY_PARAM_free(param);
  return ssl;

error_free_ssl:
  SSL_free(ssl);

error_free_ctx:
  X509_VERIFY_PARAM_free(param);
  SSL_CTX_free(ssl_ctx);

  return NULL;
}


static rfbBool
InitializeTLSSession(rfbClient* client, rfbBool anonTLS, rfbCredential *cred)
{
  if (client->tlsSession) return TRUE;

  client->tlsSession = open_ssl_connection (client, client->sock, anonTLS, cred);

  if (!client->tlsSession)
    return FALSE;

  INIT_MUTEX(client->tlsRwMutex);

  rfbClientLog("TLS session initialized.\n");

  return TRUE;
}

static rfbBool
HandshakeTLS(rfbClient* client)
{
  int timeout = 15;
  int ret;

return TRUE;

  while (timeout > 0 && (ret = SSL_do_handshake(client->tlsSession)) < 0)
  {
    if (ret != -1)
    {
      rfbClientLog("TLS handshake blocking.\n");
#ifdef WIN32
      Sleep(1000);
#else
	  sleep(1);
#endif
      timeout--;
      continue;
    }
    rfbClientLog("TLS handshake failed.\n");

    FreeTLS(client);
    return FALSE;
  }

  if (timeout <= 0)
  {
    rfbClientLog("TLS handshake timeout.\n");
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

rfbBool
HandleAnonTLSAuth(rfbClient* client)
{
  if (!InitializeTLS() || !InitializeTLSSession(client, TRUE, NULL)) return FALSE;

  if (!HandshakeTLS(client)) return FALSE;

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

rfbBool
HandleVeNCryptAuth(rfbClient* client)
{
  uint8_t major, minor, status;
  uint32_t authScheme;
  rfbBool anonTLS;
  rfbCredential *cred = NULL;
  rfbBool result = TRUE;

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
  }

  /* Start up the TLS session */
  if (!InitializeTLSSession(client, anonTLS, cred)) result = FALSE;

  if (!HandshakeTLS(client)) result = FALSE;

  /* We are done here. The caller should continue with client->subAuthScheme
   * to do actual sub authentication.
   */
  if (cred) FreeX509Credential(cred);
  return result;
}

int
ReadFromTLS(rfbClient* client, char *out, unsigned int n)
{
  int ret = 0;
  int ssl_error = SSL_ERROR_NONE;

  LOCK(client->tlsRwMutex);
  ret = SSL_read (client->tlsSession, out, n);

  if (ret < 0)
      ssl_error = SSL_get_error(client->tlsSession, ret);
  UNLOCK(client->tlsRwMutex);

  if (ret >= 0)
    return ret;
  else {
    errno = ssl_error_to_errno(ssl_error);
    if (errno != EAGAIN) {
      rfbClientLog("Error reading from TLS: -.\n");
    }
  }

  return -1;
}

int
WriteToTLS(rfbClient* client, const char *buf, unsigned int n)
{
  unsigned int offset = 0;
  int ret = 0;
  int ssl_error = SSL_ERROR_NONE;

  while (offset < n)
  {
    LOCK(client->tlsRwMutex);
    ret = SSL_write (client->tlsSession, buf + offset, (size_t)(n-offset));

    if (ret < 0)
      ssl_error = SSL_get_error (client->tlsSession, ret);
    UNLOCK(client->tlsRwMutex);

    if (ret == 0) continue;
    if (ret < 0)
    {
      errno = ssl_error_to_errno(ssl_error);
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
      rfbClientLog("Error writing to TLS: -\n");
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
    SSL_free(client->tlsSession);
    client->tlsSession = NULL;
    TINI_MUTEX(client->tlsRwMutex);
  }
}

#ifdef LIBVNCSERVER_HAVE_SASL
int GetTLSCipherBits(rfbClient* client)
{
    SSL *ssl = (SSL *)(client->tlsSession);

    const SSL_CIPHER *cipher = SSL_get_current_cipher(ssl);

    return SSL_CIPHER_get_bits(cipher, NULL);
}
#endif /* LIBVNCSERVER_HAVE_SASL */

