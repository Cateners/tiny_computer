/**
   @example fontsel.c
   fontsel is a test for rfbSelectBox and rfbLoadConsoleFont. If you have Linux
   console fonts, you can browse them via VNC. Directory browsing not implemented
   yet :-(
*/

#include <rfb/rfb.h>

#define FONTDIR "/usr/lib/kbd/consolefonts/"
#define DEFAULTFONT FONTDIR "default8x16"

static char *fontlist[50]={
"8x16alt", "b.fnt", "c.fnt", "default8x16", "m.fnt", "ml.fnt", "mod_d.fnt",
"mod_s.fnt", "mr.fnt", "mu.fnt", "r.fnt", "rl.fnt", "ro.fnt", "s.fnt",
"sc.fnt", "scrawl_s.fnt", "scrawl_w.fnt", "sd.fnt", "t.fnt",
  NULL
};

static rfbScreenInfoPtr rfbScreen = NULL;
static rfbFontDataPtr curFont = NULL;
static void showFont(int index)
{
  char buffer[1024];

  if(!rfbScreen) return;

  if(curFont)
    rfbFreeFont(curFont);

  strcpy(buffer,FONTDIR);
  strcat(buffer,fontlist[index]);
  curFont = rfbLoadConsoleFont(buffer);

  rfbFillRect(rfbScreen,210,30-20,210+10*16,30-20+256*20/16,0xb77797);
  if(curFont) {
    int i,j;
    for(j=0;j<256;j+=16)
      for(i=0;i<16;i++)
	rfbDrawCharWithClip(rfbScreen,curFont,210+10*i,30+j*20/16,j+i,
			    0,0,640,480,0xffffff,0x000000);
  }
}

int main(int argc,char** argv)
{
  rfbFontDataPtr font;
  rfbScreenInfoPtr s=rfbGetScreen(&argc,argv,640,480,8,3,3);
  int i,j;

  if(!s)
    return 1;

  s->frameBuffer=(char*)malloc(640*480*3);
  if(!s->frameBuffer)
      return 1;

  rfbInitServer(s);

  for(j=0;j<480;j++)
    for(i=0;i<640;i++) {
      s->frameBuffer[(j*640+i)*3+0]=j*256/480;
      s->frameBuffer[(j*640+i)*3+1]=i*256/640;
      s->frameBuffer[(j*640+i)*3+2]=(i+j)*256/(480+640);
    }

  rfbScreen = s;
  font=rfbLoadConsoleFont(DEFAULTFONT);
  if(!font) {
    rfbErr("Couldn't find %s\n",DEFAULTFONT);
    exit(1);
  }
  
  for(j=0;j<10 && rfbIsActive(s);j++)
    rfbProcessEvents(s,900000);

  i = rfbSelectBox(s,font,fontlist,10,20,200,300,0xffdfdf,0x602040,2,showFont);
  rfbLog("Selection: %d: %s\n",i,(i>=0)?fontlist[i]:"cancelled");

  rfbFreeFont(font);
  free(s->frameBuffer);
  rfbScreenCleanup(s);

  return(0);
}

