/*
 * rfbssl_openssl.c - Secure socket functions (openssl version)
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
#include <openssl/ssl.h>
#include <openssl/err.h>

struct rfbssl_ctx {
    SSL_CTX *ssl_ctx;
    SSL     *ssl;
};

static void rfbssl_error(void)
{
    char buf[1024];
    unsigned long e = ERR_get_error();
    rfbErr("%s (%ld)\n", ERR_error_string(e, buf), e);
}

int rfbssl_init(rfbClientPtr cl)
{
    char *keyfile;
    int r, ret = -1;
    struct rfbssl_ctx *ctx;

    SSL_library_init();
    SSL_load_error_strings();

    if (cl->screen->sslkeyfile && *cl->screen->sslkeyfile) {
      keyfile = cl->screen->sslkeyfile;
    } else {
      keyfile = cl->screen->sslcertfile;
    }

    if (NULL == (ctx = malloc(sizeof(struct rfbssl_ctx)))) {
	rfbErr("OOM\n");
    } else if (!cl->screen->sslcertfile || !cl->screen->sslcertfile[0]) {
	rfbErr("SSL connection but no cert specified\n");
    } else if (NULL == (ctx->ssl_ctx = SSL_CTX_new(SSLv23_server_method()))) {
	rfbssl_error();
    } else if (SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, keyfile, SSL_FILETYPE_PEM) <= 0) {
	rfbErr("Unable to load private key file %s\n", keyfile);
    } else if (SSL_CTX_use_certificate_file(ctx->ssl_ctx, cl->screen->sslcertfile, SSL_FILETYPE_PEM) <= 0) {
	rfbErr("Unable to load certificate file %s\n", cl->screen->sslcertfile);
    } else if (NULL == (ctx->ssl = SSL_new(ctx->ssl_ctx))) {
	rfbErr("SSL_new failed\n");
	rfbssl_error();
    } else if (!(SSL_set_fd(ctx->ssl, cl->sock))) {
	rfbErr("SSL_set_fd failed\n");
	rfbssl_error();
    } else {
	while ((r = SSL_accept(ctx->ssl)) < 0) {
	    if (SSL_get_error(ctx->ssl, r) != SSL_ERROR_WANT_READ)
		break;
	}
	if (r < 0) {
	    rfbErr("SSL_accept failed %d\n", SSL_get_error(ctx->ssl, r));
	} else {
	    cl->sslctx = (rfbSslCtx *)ctx;
	    ret = 0;
	}
    }
    return ret;
}

int rfbssl_write(rfbClientPtr cl, const char *buf, int bufsize)
{
    int ret;
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;

    while ((ret = SSL_write(ctx->ssl, buf, bufsize)) <= 0) {
	if (SSL_get_error(ctx->ssl, ret) != SSL_ERROR_WANT_WRITE)
	    break;
    }
    return ret;
}

int rfbssl_peek(rfbClientPtr cl, char *buf, int bufsize)
{
    int ret;
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;

    while ((ret = SSL_peek(ctx->ssl, buf, bufsize)) <= 0) {
	if (SSL_get_error(ctx->ssl, ret) != SSL_ERROR_WANT_READ)
	    break;
    }
    return ret;
}

int rfbssl_read(rfbClientPtr cl, char *buf, int bufsize)
{
    int ret;
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;

    while ((ret = SSL_read(ctx->ssl, buf, bufsize)) <= 0) {
	if (SSL_get_error(ctx->ssl, ret) != SSL_ERROR_WANT_READ)
	    break;
    }
    return ret;
}

int rfbssl_pending(rfbClientPtr cl)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    return SSL_pending(ctx->ssl);
}

void rfbssl_destroy(rfbClientPtr cl)
{
    struct rfbssl_ctx *ctx = (struct rfbssl_ctx *)cl->sslctx;
    if (ctx->ssl)
	SSL_free(ctx->ssl);
    if (ctx->ssl_ctx)
	SSL_CTX_free(ctx->ssl_ctx);
}
