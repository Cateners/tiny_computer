#include <stdio.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)
#define CONCAT3(a,b,c) a##b##c
#define CONCAT3E(a,b,c) CONCAT3(a,b,c)

#define FUNCNAME rfbRotate
#define FUNC(i, j) (h - 1 - j + i * h)
#define SWAPDIMENSIONS
#define OUTBITS 8
#include "rotatetemplate.c"
#define OUTBITS 16
#include "rotatetemplate.c"
#define OUTBITS 32
#include "rotatetemplate.c"
#undef FUNCNAME
#undef FUNC

#define FUNCNAME rfbRotateCounterClockwise
#define FUNC(i, j) (j + (w - 1 - i) * h)
#define OUTBITS 8
#include "rotatetemplate.c"
#define OUTBITS 16
#include "rotatetemplate.c"
#define OUTBITS 32
#include "rotatetemplate.c"
#undef FUNCNAME
#undef FUNC
#undef SWAPDIMENSIONS

#define FUNCNAME rfbFlipHorizontally
#define FUNC(i, j) ((w - 1 - i) + j * w)
#define OUTBITS 8
#include "rotatetemplate.c"
#define OUTBITS 16
#include "rotatetemplate.c"
#define OUTBITS 32
#include "rotatetemplate.c"
#undef FUNCNAME
#undef FUNC

#define FUNCNAME rfbFlipVertically
#define FUNC(i, j) (i + (h - 1 - j) * w)
#define OUTBITS 8
#include "rotatetemplate.c"
#define OUTBITS 16
#include "rotatetemplate.c"
#define OUTBITS 32
#include "rotatetemplate.c"
#undef FUNCNAME
#undef FUNC

#define FUNCNAME rfbRotateHundredAndEighty
#define FUNC(i, j) ((w - 1 - i) + (h - 1 - j) * w)
#define OUTBITS 8
#include "rotatetemplate.c"
#define OUTBITS 16
#include "rotatetemplate.c"
#define OUTBITS 32
#include "rotatetemplate.c"
#undef FUNCNAME
#undef FUNC

static void HandleKey(rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
	if(down) {
		if (key==XK_Escape || key=='q' || key=='Q')
			rfbCloseClient(cl);
		else if (key == 'r')
			rfbRotate(cl->screen);
		else if (key == 'R')
			rfbRotateCounterClockwise(cl->screen);
		else if (key == 'f')
			rfbFlipHorizontally(cl->screen);
		else if (key == 'F')
			rfbFlipVertically(cl->screen);
	}
}

#define HAVE_HANDLEKEY
#include "pnmshow.c"

