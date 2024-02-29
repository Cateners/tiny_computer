/*
 * 
 * This is an example of how to use libvncserver.
 * 
 * libvncserver example
 * Copyright (C) 2005 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>,
 * 		Karl Runge <runge@karlrunge.com>
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

#include <rfb/rfb.h>

static const int bpp=4;
static int maxx=800, maxy=600;

/* This initializes a nice (?) background */

static void initBuffer(unsigned char* buffer)
{
	int i,j;
	for(j=0;j<maxy;++j) {
		for(i=0;i<maxx;++i) {
			buffer[(j*maxx+i)*bpp+0]=(i+j)*128/(maxx+maxy); /* red */
			buffer[(j*maxx+i)*bpp+1]=i*128/maxx; /* green */
			buffer[(j*maxx+i)*bpp+2]=j*256/maxy; /* blue */
		}
	}
}

/* Example for an XCursor (foreground/background only) */

static void SetXCursor(rfbScreenInfoPtr rfbScreen)
{
	int width=13,height=11;
	char cursor[]=
		"             "
		" xx       xx "
		"  xx     xx  "
		"   xx   xx   "
		"    xx xx    "
		"     xxx     "
		"    xx xx    "
		"   xx   xx   "
		"  xx     xx  "
		" xx       xx "
		"             ",
	     mask[]=
		"xxxx     xxxx"
		"xxxx     xxxx"
		" xxxx   xxxx "
		"  xxxx xxxx  "
		"   xxxxxxx   "
		"    xxxxx    "
		"   xxxxxxx   "
		"  xxxx xxxx  "
		" xxxx   xxxx "
		"xxxx     xxxx"
		"xxxx     xxxx";
	rfbCursorPtr c;
	
	c=rfbMakeXCursor(width,height,cursor,mask);
	c->xhot=width/2;c->yhot=height/2;

	rfbSetCursor(rfbScreen, c);
}

static void SetXCursor2(rfbScreenInfoPtr rfbScreen)
{
	int width=13,height=22;
	char cursor[]=
		" xx          "
		" x x         "
		" x  x        "
		" x   x       "
		" x    x      "
		" x     x     "
		" x      x    "
		" x       x   "
		" x     xx x  "
		" x x   x xxx "
		" x xx  x   x "
		" xx x   x    "
		" xx  x  x    "
		" x    x  x   "
		" x    x  x   "
		"       x  x  "
		"        x  x "
		"        x  x "
		"         xx  "
		"             "
		"             ",
	     mask[]=
		"xxx          "
		"xxxx         "
		"xxxxx        "
		"xxxxxx       "
		"xxxxxxx      "
		"xxxxxxxx     "
		"xxxxxxxxx    "
		"xxxxxxxxxx   "
		"xxxxxxxxxxx  "
		"xxxxxxxxxxxx "
		"xxxxxxxxxxxxx"
		"xxxxxxxxxxxxx"
		"xxxxxxxxxx  x"
		"xxxxxxxxxx   "
		"xxx  xxxxxx  "
		"xxx  xxxxxx  "
		"xx    xxxxxx "
		"       xxxxx "
		"       xxxxxx"
		"        xxxxx"
		"         xxx "
		"             ";
	rfbCursorPtr c;
	
	c=rfbMakeXCursor(width,height,cursor,mask);
	c->xhot=0;c->yhot=0;

	rfbSetCursor(rfbScreen, c);
}

/* Example for a rich cursor (full-colour) */

static void SetRichCursor(rfbScreenInfoPtr rfbScreen)
{
	int i,j,w=32,h=32;
	/* runge */
	/*  rfbCursorPtr c = rfbScreen->cursor; */
	rfbCursorPtr c;
	char bitmap[]=
		"                                "
		"              xxxxxx            "
		"       xxxxxxxxxxxxxxxxx        "
		"      xxxxxxxxxxxxxxxxxxxxxx    "
		"    xxxxx  xxxxxxxx  xxxxxxxx   "
		"   xxxxxxxxxxxxxxxxxxxxxxxxxxx  "
		"  xxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
		"  xxxxx   xxxxxxxxxxx   xxxxxxx "
		"  xxxx     xxxxxxxxx     xxxxxx "
		"  xxxxx   xxxxxxxxxxx   xxxxxxx "
		" xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
		" xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx "
		" xxxxxxxxxxxx  xxxxxxxxxxxxxxx  "
		" xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
		" xxxxxxxxxxxxxxxxxxxxxxxxxxxx   "
		" xxxxxxxxxxx   xxxxxxxxxxxxxx   "
		" xxxxxxxxxx     xxxxxxxxxxxx    "
		"  xxxxxxxxx      xxxxxxxxx      "
		"   xxxxxxxxxx   xxxxxxxxx       "
		"      xxxxxxxxxxxxxxxxxxx       "
		"       xxxxxxxxxxxxxxxxxxx      "
		"         xxxxxxxxxxxxxxxxxxx    "
		"             xxxxxxxxxxxxxxxxx  "
		"                xxxxxxxxxxxxxxx "
		"   xxxx           xxxxxxxxxxxxx "
		"  xx   x            xxxxxxxxxxx "
		"  xxx               xxxxxxxxxxx "
		"  xxxx             xxxxxxxxxxx  "
		"   xxxxxx       xxxxxxxxxxxx    "
		"    xxxxxxxxxxxxxxxxxxxxxx      "
		"      xxxxxxxxxxxxxxxx          "
		"                                ";

	c=rfbMakeXCursor(w,h,bitmap,bitmap);
	c->xhot = 16; c->yhot = 24;

	c->richSource = (unsigned char*)malloc(w*h*bpp);
	if (!c->richSource) return;

	for(j=0;j<h;j++) {
		for(i=0;i<w;i++) {
			c->richSource[j*w*bpp+i*bpp+0]=i*0xff/w;
			c->richSource[j*w*bpp+i*bpp+1]=(i+j)*0xff/(w+h);
			c->richSource[j*w*bpp+i*bpp+2]=j*0xff/h;
			c->richSource[j*w*bpp+i*bpp+3]=0;
		}
	}
	rfbSetCursor(rfbScreen, c);
}

/* runge */
static void SetRichCursor2(rfbScreenInfoPtr rfbScreen)
{
	int i,j,w=17,h=16;
	/*  rfbCursorPtr c = rfbScreen->cursor; */
	rfbCursorPtr c;
	char bitmap[]=
		"                 "
		"xxxx             "
		"xxxxxxxx         "
		"xxxxxxxxxxxx    x"
		"xxx  xxxxxxxx   x"
		"xxxxxxxxxxxxxx  x"
		"xxxxxxxxxxxxxxx x"
		"xxxxx   xxxxxxx x"
		"xxxx     xxxxxx x"
		"xxxxx   xxxxxxx x"
		"xxxxxxxxxxxxxxx x"
		"xxxxxxxxxxxxxxx x"
		"xxxxxxxxxxxxxx  x"
		"xxxxxxxxxxxxx   x"
		"xxxxxxxxxxxxx   x"
		"xxxxxxxxxxxxx   x";
	/*  c=rfbScreen->cursor = rfbMakeXCursor(w,h,bitmap,bitmap); */
	c=rfbMakeXCursor(w,h,bitmap,bitmap);
	c->xhot = 5; c->yhot = 7;

	c->richSource = (unsigned char*)malloc(w*h*bpp);
	if(!c->richSource) return;
	for(j=0;j<h;j++) {
		for(i=0;i<w;i++) {
			c->richSource[j*w*bpp+i*bpp+0]=0xff;
			c->richSource[j*w*bpp+i*bpp+1]=0x00;
			c->richSource[j*w*bpp+i*bpp+2]=0x7f;
			c->richSource[j*w*bpp+i*bpp+3]=0;
		}
	}
	rfbSetCursor(rfbScreen, c);
}

/* alpha channel */

static void SetAlphaCursor(rfbScreenInfoPtr screen,int mode)
{
	int i,j;
	rfbCursorPtr c = screen->cursor;
	int maskStride;

	if(!c)
		return;

	maskStride = (c->width+7)/8;

	if(c->alphaSource) {
		free(c->alphaSource);
		c->alphaSource=NULL;
	}

	if(mode==0)
		return;

	c->alphaSource = (unsigned char*)malloc(c->width*c->height);
	if (!c->alphaSource) return;

	for(j=0;j<c->height;j++)
		for(i=0;i<c->width;i++) {
			unsigned char value=0x100*i/c->width;
			rfbBool masked=(c->mask[(i/8)+maskStride*j]<<(i&7))&0x80;
			c->alphaSource[i+c->width*j]=(masked?(mode==1?value:0xff-value):0);
		}
	if(c->cleanupMask)
		free(c->mask);
	c->mask=(unsigned char*)rfbMakeMaskFromAlphaSource(c->width,c->height,c->alphaSource);
	c->cleanupMask=TRUE;
}

/* Here the pointer events are handled */

static void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
	static int oldButtonMask=0;
	static int counter=0;

	if((oldButtonMask&1)==0 && (buttonMask&1)==1) {
		switch(++counter) {
		case 7:
			SetRichCursor(cl->screen);
			SetAlphaCursor(cl->screen,2);
			break;
		case 6:
			SetRichCursor(cl->screen);
			SetAlphaCursor(cl->screen,1);
			break;
		case 5:
			SetRichCursor2(cl->screen);
			SetAlphaCursor(cl->screen,0);
			break;
		case 4:
			SetXCursor(cl->screen);
			break;
		case 3:
			SetRichCursor2(cl->screen);
			SetAlphaCursor(cl->screen,2);
			break;
		case 2:
			SetXCursor(cl->screen);
			SetAlphaCursor(cl->screen,2);
			break;
		case 1:
			SetXCursor2(cl->screen);
			SetAlphaCursor(cl->screen,0);
			break;
		default:
			SetRichCursor(cl->screen);
			counter=0;
		}
	}
	if(buttonMask&2) {
		rfbScreenCleanup(cl->screen);
		exit(0);
	}

	if(buttonMask&4)
		rfbCloseClient(cl);


	oldButtonMask=buttonMask;

	rfbDefaultPtrAddEvent(buttonMask,x,y,cl);
}

/* Initialization */

int main(int argc,char** argv)
{
	rfbScreenInfoPtr rfbScreen = rfbGetScreen(&argc,argv,maxx,maxy,8,3,bpp);
        if(!rfbScreen)
          return 1;

	rfbScreen->desktopName = "Cursor Test";
	rfbScreen->frameBuffer = (char*)malloc(maxx*maxy*bpp);
	rfbScreen->ptrAddEvent = doptr;

	initBuffer((unsigned char*)rfbScreen->frameBuffer);


	SetRichCursor(rfbScreen);

	/* initialize the server */
	rfbInitServer(rfbScreen);

	rfbLog("Change cursor shape with left mouse button,\n\t"
			"quit with right one (middle button quits server).\n");

	/* this is the blocking event loop, i.e. it never returns */
	/* 40000 are the microseconds to wait on select(), i.e. 0.04 seconds */
	rfbRunEventLoop(rfbScreen,40000,FALSE);

	free(rfbScreen->frameBuffer);
	rfbScreenCleanup(rfbScreen);

	return(0);
}

