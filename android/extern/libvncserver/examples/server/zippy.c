#include <stdio.h>
#include <stdlib.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <rfb/default8x16.h>

static int maxx=400, maxy=400, bpp=4;
/* odd maxx doesn't work (vncviewer bug) */

/* Here we create a structure so that every client has its own pointer */

/* turns the framebuffer black */
void blank_framebuffer(char* frame_buffer, int x1, int y1, int x2, int y2);
/* displays a red bar, a green bar, and a blue bar */
void draw_primary_colors (char* frame_buffer, int x1, int y1, int x2, int y2);
void draw_primary_colours_generic(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2);
void draw_primary_colours_generic_fast(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2);
void linecount (char* frame_buffer);
/* handles mouse events */
void on_mouse_event (int buttonMask,int x,int y,rfbClientPtr cl);
/* handles keyboard events */
void on_key_press (rfbBool down,rfbKeySym key,rfbClientPtr cl);

int main (int argc, char **argv)
{
	rfbScreenInfoPtr server;

	if(!rfbProcessSizeArguments(&maxx,&maxy,&bpp,&argc,argv))
	  return 1;
	  
        server = rfbGetScreen (&argc, argv, maxx, maxy, 8, 3, bpp);
        if(!server)
          return 1;
	server->desktopName = "Zippy das wundersquirrel\'s VNC server";
	server->frameBuffer = (char*)malloc(maxx*maxy*bpp);
	server->alwaysShared = TRUE;
        server->kbdAddEvent = on_key_press;
	server->ptrAddEvent = on_mouse_event;

	rfbInitServer (server);

	blank_framebuffer(server->frameBuffer, 0, 0, maxx, maxy);
	rfbRunEventLoop (server, -1, FALSE);
	free(server->frameBuffer);
	rfbScreenCleanup (server);
	return 0;
}

void blank_framebuffer(char* frame_buffer, int x1, int y1, int x2, int y2)
{
	int i;
        for (i=0; i < maxx * maxy * bpp; i++) frame_buffer[i]=(char) 0;
}

void draw_primary_colors (char* frame_buffer, int x1, int y1, int x2, int y2)
{
        int i, j, current_pixel;
        for (i=y1; i < y2; i++){
                for (j=x1; j < x2; j++) {
                        current_pixel = (i*x2 + j) * bpp;
                        if (i < y2 ) {
                                frame_buffer[current_pixel+0] = (char) 128;
                                frame_buffer[current_pixel+1] = (char) 0;
                                frame_buffer[current_pixel+2] = (char) 0;
                        }
                        if (i < y2/3*2) {
                                frame_buffer[current_pixel+0] = (char) 0;
                                frame_buffer[current_pixel+1] = (char) 128;
                                frame_buffer[current_pixel+2] = (char) 0;
                        }
                        if (i < y2/3) {
                                frame_buffer[current_pixel+0] = (char) 0;
                                frame_buffer[current_pixel+1] = (char) 0;
                                frame_buffer[current_pixel+2] = (char) 128;
                        }
                }
        }
 }

/* Dscho's versions (slower, but works for bpp != 3 or 4) */
void draw_primary_colours_generic(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2)
{
  rfbPixelFormat f=s->serverFormat;
  int i,j;
  for(j=y1;j<y2;j++)
    for(i=x1;i<x2;i++)
      if(j<y1*2/3+y2/3)
	rfbDrawPixel(s,i,j,f.redMax<<f.redShift);
      else if(j<y1/3+y2*2/3)
	rfbDrawPixel(s,i,j,f.greenMax<<f.greenShift);
      else
	rfbDrawPixel(s,i,j,f.blueMax<<f.blueShift);
}

void draw_primary_colours_generic_fast(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2)
{
  rfbPixelFormat f=s->serverFormat;
  int i,j,y3=(y1*2+y2)/3,y4=(y1+y2*2)/3;
  /* draw first pixel */
  rfbDrawPixel(s,x1,y1,f.redMax<<f.redShift);
  rfbDrawPixel(s,x1,y3,f.greenMax<<f.greenShift);
  rfbDrawPixel(s,x1,y4,f.blueMax<<f.blueShift);
  /* then copy stripes */
  for(j=0;j<y2-y4;j++)
    for(i=x1;i<x2;i++) {
#define ADDR(x,y) s->frameBuffer+(x)*bpp+(y)*s->paddedWidthInBytes
      memcpy(ADDR(i,j+y1),ADDR(x1,y1),bpp);
      memcpy(ADDR(i,j+y3),ADDR(x1,y3),bpp);
      memcpy(ADDR(i,j+y4),ADDR(x1,y4),bpp);
    }
}

static void draw_primary_colours_generic_ultrafast(rfbScreenInfoPtr s,int x1,int y1,int x2,int y2)
{
  rfbPixelFormat f=s->serverFormat;
  int y3=(y1*2+y2)/3,y4=(y1+y2*2)/3;
  /* fill rectangles */
  rfbFillRect(s,x1,y1,x2,y3,f.redMax<<f.redShift);
  rfbFillRect(s,x1,y3,x2,y4,f.greenMax<<f.greenShift);
  rfbFillRect(s,x1,y4,x2,y2,f.blueMax<<f.blueShift);
}

void linecount (char* frame_buffer)
{
        int i,j,k, current_pixel;
        for (i=maxy-4; i>maxy-20; i-=4)
                for (j=0; j<4; j++) for (k=0; k < maxx; k++) {
                        current_pixel = (i*j*maxx + k) * bpp;
                        if (i%2 == 0) {
                                frame_buffer[current_pixel+0] = (char) 0;
                                frame_buffer[current_pixel+1] = (char) 0;
                                frame_buffer[current_pixel+2] = (char) 128;
                        }

                        if (i%2 == 1) {
                                frame_buffer[current_pixel+0] = (char) 128;
                                frame_buffer[current_pixel+1] = (char) 0;
                                frame_buffer[current_pixel+2] = (char) 0;
                        }
                }

}


void on_key_press (rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
        if (down)		/* or else the action occurs on both the press and depress */
	switch (key) {

        case XK_b:
        case XK_B:
                blank_framebuffer(cl->screen->frameBuffer, 0, 0, maxx, maxy);
                rfbDrawString(cl->screen,&default8x16Font,20,maxy-20,"Hello, World!",0xffffff);
                rfbMarkRectAsModified(cl->screen,0, 0,maxx,maxy);
                rfbLog("Framebuffer blanked\n");
                break;
        case XK_p:
        case XK_P:
                /* draw_primary_colors (cl->screen->frameBuffer, 0, 0, maxx, maxy); */
		draw_primary_colours_generic_ultrafast (cl->screen, 0, 0, maxx, maxy);
                rfbMarkRectAsModified(cl->screen,0, 0,maxx,maxy);
                rfbLog("Primary colors displayed\n");
                break;
        case XK_Q:
        case XK_q:
                rfbLog("Exiting now\n");
                exit(0);
        case XK_C:
        case XK_c:
		rfbDrawString(cl->screen,&default8x16Font,20,100,"Hello, World!",0xffffff);
                rfbMarkRectAsModified(cl->screen,0, 0,maxx,maxy);
                break;
        default:
                rfbLog("The %c key was pressed\n", (char) key);
        }
}


void on_mouse_event (int buttonMask,int x,int y,rfbClientPtr cl)
{
	printf("buttonMask: %i\n"
		"x: %i\n" "y: %i\n", buttonMask, x, y);
}
