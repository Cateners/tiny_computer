/*
 * rfbssl_gnutls.c - Secure socket functions (gnutls version)
 */

/*
 *  Copyright (C) 2011 Gernot Tenchio
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

#include "rfbssl.h"
#include <gnutls/gnutls.h>
#include <errno.h>

struct rfbssl_ctx {
    char peekbuf[2048];
    int peeklen;
    int peekstart;
    gnutls_session_t session;
    gnutls_certificate_credentials_t x509_cred;
    gnutls_dh_params_t dh_params;
#ifdef I_LIKE_RSA_PARAMS_THAT_MUCH
    gnutls_rsa_params_t rsa_params;
#endif
};

void rfbssl_log_func(int level, const char *msg)
{
    rfbErr("SSL: %s", msg);
}

static void rfbssl_error(const char *msg, int e)
{
    rfbErr("%s: %s (%ld)\n", msg, gnutls_strerror(e), e);
}

static int rfbssl_init_session(struct rfbssl_ctx *ctx, int fd)
{
    gnutls_session_t session;
    int ret;

    if (GNUTLS_E_SUCCESS != (ret = gnutls_init(&session, GNUTLS_SERVER))) {
      /* */
    } else if (GNUTLS_E_SUCCESS != (ret = gnutls_set_default_priority(session))) {
      /* */
    } else if (GNUTLS_E_SUCCESS != (ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, ctx->x509_cred))) {
      /* */
    } else {
      gnutls_session_enable_compatibility_mode(session);
      gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t)(uintptr_t)fd);
      ctx->session = session;
    }
    return ret;
}

static int generate_dh_params(struct rfbssl_ctx *ctx)
{
    int ret;
    if (GNUTLS_E_SUCCESS == (ret = gnutls_dh_params_init(&ctx->dh_params)))
	ret = gnutls_dh_params_generate2(ctx->dh_params, 1024);
    return ret;
}

#ifdef I_LIKE_RSA_PARAMS_THAT_MUCH
static int generate_rsa_params(struct rfbssl_ctx *ctx)
{
    int ret;
    if (GNUTLS_E_SUCCESS == (ret = gnutls_rsa_params_init(&ctx->rsa_params)))
	ret = gnutls_rsa_params_generate2(ctx->rsa_params, 512);
    return ret;
}
#endif

struct rfbssl_ctx *rfbssl_init_global(char *key, char *cert)
{
    int ret = GNUTLS_E_SUCCESS;
    struct rfbssl_ctx *ctx = NULL;

    if (NULL == (ctx = malloc(sizeof(struct rfbssl_ctx)))) {
	return NULL;
    } else if (GNUTLS_E_SUCCESS != (ret = gnutls_global_init())) {
	/* */
    } else if (GNUTLS_E_SUCCESS != (ret = gnutls_certificate_allocate_credentials(&ctx->x509_cred))) {
	/* */
    } else if ((ret = gnutls_certificate_set_x509_trust_file(ctx->x509_cred, cert, GNUTLS_X509_FMT_PEM)) < 0) {
	/* */
    } else if (GNUTLS_E_SUCCESS != (ret = gnutls_certificate_set_x509_key_file(ctx->x509_cred, cert, key, GNUTLS_X509_FMT_PEM))) {
	/* */
    } else if (GNUTLS_E_SUCCESS != (ret = generate_dh_params(ctx))) {
	/* */
#ifdef I_LIKE_RSA_PARAMS_THAT_MUCH
    } else if (GNUTLS_E_SUCCESS != (ret = generate_rsa_params(ctx))) {
	/* */
#endif
    } else {
	gnutls_global_set_log_function(rfbssl_log_func);
	gnutls_global_set_log_level(1);
	gnutls_certificate_set_dh_params(ctx->x509_cred, ctx->dh_params);
	/* newly allocated memory should be initialized, at least where it is important */
	ctx->peekstart = ctx->peeklen = 0;
	return ctx;
    }

    free(ctx);
    return NULL;
}

int rfbssl_init(rfbClientPtr cl)
{
    int ret = -1;
    struct rfbssl_ctx *ctx;
    char *keyfile;
    if (!(keyfile = cl->screen->sslkeyfile))
	keyfile = cl->screen->sslcertfile;

    if (!cl->screen->sslcertfile || !cl->screen->sslcertfile[0]) {
	rfbErr("SSL connection but no cert specified\n");
    } else if (NULL == (ctx = rfbssl_init_global(keyfile,  cl->screen->sslcertfile))) {
	/* */
    } else if (GNUTLS_E_SUCCESS != (ret = rfbssl_init_session(ctx, cl->sock))) {
	/* */
    } else {
	while (GNUTLS_E_SUCCESS != (ret = gnutls_handshake(ctx->session))) {
	    if (ret == GNUTLS_E_AGAIN)
		continue;
	    break;
	}
    }

    if (ret != GNUTLS_E_SUCCESS) {
	rfbssl_error(__func__, ret);
    } else {
	cl->sslctx = (rfbSslCtx *)ctx;
	rfbLog("%s protocol initialized\n", gnutls_protocol_get_name(gnutls_protocol_get_version(ctx->session)));
    }
    return ret;
}

static int rfbssl_do_read(rfbClientPtr cl, char *buf, int bufsize)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    int ret;

    while ((ret = gnutls_record_recv(ctx->session, buf, bufsize)) < 0) {
	if (ret == GNUTLS_E_AGAIN) {
	    /* continue */
	} else if (ret == GNUTLS_E_INTERRUPTED) {
	    /* continue */
	} else {
	    break;
	}
    }

    if (ret < 0) {
	rfbssl_error(__func__, ret);
	errno = EIO;
	ret = -1;
    }

    return ret < 0 ? -1 : ret;
}

int rfbssl_write(rfbClientPtr cl, const char *buf, int bufsize)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    int ret;

    while ((ret = gnutls_record_send(ctx->session, buf, bufsize)) < 0) {
	if (ret == GNUTLS_E_AGAIN) {
	    /* continue */
	} else if (ret == GNUTLS_E_INTERRUPTED) {
	    /* continue */
	} else {
	    break;
	}
    }

    if (ret < 0)
	rfbssl_error(__func__, ret);

    return ret;
}

static void rfbssl_gc_peekbuf(struct rfbssl_ctx *ctx, int bufsize)
{
    if (ctx->peekstart) {
	int spaceleft = sizeof(ctx->peekbuf) - ctx->peeklen - ctx->peekstart;
	if (spaceleft < bufsize) {
	    memmove(ctx->peekbuf, ctx->peekbuf + ctx->peekstart, ctx->peeklen);
	    ctx->peekstart = 0;
	}
    }
}

static int __rfbssl_read(rfbClientPtr cl, char *buf, int bufsize, int peek)
{
    int ret = 0;
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;

    rfbssl_gc_peekbuf(ctx, bufsize);

    if (ctx->peeklen) {
	/* If we have any peek data, simply return that. */
	ret = bufsize < ctx->peeklen ? bufsize : ctx->peeklen;
	memcpy (buf, ctx->peekbuf + ctx->peekstart, ret);
	if (!peek) {
	    ctx->peeklen -= ret;
	    if (ctx->peeklen != 0)
		ctx->peekstart += ret;
	    else
		ctx->peekstart = 0;
	}
    }

    if (ret < bufsize) {
	int n;
	/* read the remaining data */
	if ((n = rfbssl_do_read(cl, buf + ret, bufsize - ret)) <= 0) {
	    rfbErr("rfbssl_%s: %s error\n", __func__, peek ? "peek" : "read");
	    return n;
	}
	if (peek) {
	    memcpy(ctx->peekbuf + ctx->peekstart + ctx->peeklen, buf + ret, n);
	    ctx->peeklen += n;
	}
	ret += n;
    }

    return ret;
}

int rfbssl_read(rfbClientPtr cl, char *buf, int bufsize)
{
    return __rfbssl_read(cl, buf, bufsize, 0);
}

int rfbssl_peek(rfbClientPtr cl, char *buf, int bufsize)
{
    return __rfbssl_read(cl, buf, bufsize, 1);
}

int rfbssl_pending(rfbClientPtr cl)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    int ret = ctx->peeklen;

    if (ret <= 0)
	ret = gnutls_record_check_pending(ctx->session);

    return ret;
}

void rfbssl_destroy(rfbClientPtr cl)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    gnutls_bye(ctx->session, GNUTLS_SHUT_WR);
    gnutls_deinit(ctx->session);
    gnutls_certificate_free_credentials(ctx->x509_cred);
}
