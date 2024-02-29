#ifndef _VNCSSL_H
#define _VNCSSL_H 1

#include "rfb/rfb.h"
#include "rfb/rfbconfig.h"

int rfbssl_init(rfbClientPtr cl);
int rfbssl_pending(rfbClientPtr cl);
int rfbssl_peek(rfbClientPtr cl, char *buf, int bufsize);
int rfbssl_read(rfbClientPtr cl, char *buf, int bufsize);
int rfbssl_write(rfbClientPtr cl, const char *buf, int bufsize);
void rfbssl_destroy(rfbClientPtr cl);


#endif /* _VNCSSL_H */
