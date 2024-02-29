#include <ctype.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>

typedef struct {
  rfbScreenInfoPtr screen;
  rfbFontDataPtr font;
  char** list;
  int listSize;
  int selected;
  int displayStart;
  int x1,y1,x2,y2,textH,pageH;
  int xhot,yhot;
  int buttonWidth,okBX,cancelBX,okX,cancelX,okY;
  rfbBool okInverted,cancelInverted;
  int lastButtons;
  rfbPixel colour,backColour;
  SelectionChangedHookPtr selChangedHook;
  enum { SELECTING, OK, CANCEL } state;
} rfbSelectData;

static const char* okStr="OK";
static const char* cancelStr="Cancel";

static void selPaintButtons(rfbSelectData* m,rfbBool invertOk,rfbBool invertCancel)
{
  rfbScreenInfoPtr s = m->screen;
  rfbPixel bcolour = m->backColour;
  rfbPixel colour = m->colour;

  rfbFillRect(s,m->x1,m->okY-m->textH,m->x2,m->okY,bcolour);

  if(invertOk) {
    rfbFillRect(s,m->okBX,m->okY-m->textH,m->okBX+m->buttonWidth,m->okY,colour);
    rfbDrawStringWithClip(s,m->font,m->okX+m->xhot,m->okY-1+m->yhot,okStr,
			  m->x1,m->okY-m->textH,m->x2,m->okY,
			  bcolour,colour);
  } else
    rfbDrawString(s,m->font,m->okX+m->xhot,m->okY-1+m->yhot,okStr,colour);
    
  if(invertCancel) {
    rfbFillRect(s,m->cancelBX,m->okY-m->textH,
	     m->cancelBX+m->buttonWidth,m->okY,colour);
    rfbDrawStringWithClip(s,m->font,m->cancelX+m->xhot,m->okY-1+m->yhot,
			  cancelStr,m->x1,m->okY-m->textH,m->x2,m->okY,
			  bcolour,colour);
  } else
    rfbDrawString(s,m->font,m->cancelX+m->xhot,m->okY-1+m->yhot,cancelStr,colour);

  m->okInverted = invertOk;
  m->cancelInverted = invertCancel;
}

/* line is relative to displayStart */
static void selPaintLine(rfbSelectData* m,int line,rfbBool invert)
{
  int y1 = m->y1+line*m->textH, y2 = y1+m->textH;
  if(y2>m->y2)
    y2=m->y2;
  rfbFillRect(m->screen,m->x1,y1,m->x2,y2,invert?m->colour:m->backColour);
  if(m->displayStart+line<m->listSize)
    rfbDrawStringWithClip(m->screen,m->font,m->x1+m->xhot,y2-1+m->yhot,
			  m->list[m->displayStart+line],
			  m->x1,y1,m->x2,y2,
			  invert?m->backColour:m->colour,
			  invert?m->backColour:m->colour);
}

static void selSelect(rfbSelectData* m,int _index)
{
  int delta;

  if(_index==m->selected || _index<0 || _index>=m->listSize)
    return;

  if(m->selected>=0)
    selPaintLine(m,m->selected-m->displayStart,FALSE);

  if(_index<m->displayStart || _index>=m->displayStart+m->pageH) {
    /* targetLine is the screen line in which the selected line will
       be displayed.
       targetLine = m->pageH/2 doesn't look so nice */
    int targetLine = m->selected-m->displayStart;
    int lineStart,lineEnd;

    /* scroll */
    if(_index<targetLine)
      targetLine = _index;
    else if(_index+m->pageH-targetLine>=m->listSize)
      targetLine = _index+m->pageH-m->listSize;
    delta = _index-(m->displayStart+targetLine);

    if(delta>-m->pageH && delta<m->pageH) {
      if(delta>0) {
	lineStart = m->pageH-delta;
	lineEnd = m->pageH;
	rfbDoCopyRect(m->screen,m->x1,m->y1,m->x2,m->y1+lineStart*m->textH,
		      0,-delta*m->textH);
      } else {
	lineStart = 0;
	lineEnd = -delta;
	rfbDoCopyRect(m->screen,
		      m->x1,m->y1+lineEnd*m->textH,m->x2,m->y2,
		      0,-delta*m->textH);
      }
    } else {
      lineStart = 0;
      lineEnd = m->pageH;
    }
    m->displayStart += delta;
    for(delta=lineStart;delta<lineEnd;delta++)
      if(delta!=_index)
	selPaintLine(m,delta,FALSE);
  }

  m->selected = _index;
  selPaintLine(m,m->selected-m->displayStart,TRUE);

  if(m->selChangedHook)
    m->selChangedHook(_index);

  /* todo: scrollbars */
}

static void selKbdAddEvent(rfbBool down,rfbKeySym keySym,rfbClientPtr cl)
{
  if(down) {
    if(keySym>' ' && keySym<0xff) {
      int i;
      rfbSelectData* m = (rfbSelectData*)cl->screen->screenData;
      char c = tolower(keySym);

      for(i=m->selected+1;m->list[i] && tolower(m->list[i][0])!=c;i++);
      if(!m->list[i])
	for(i=0;i<m->selected && tolower(m->list[i][0])!=c;i++);
      selSelect(m,i);
    } else if(keySym==XK_Escape) {
      rfbSelectData* m = (rfbSelectData*)cl->screen->screenData;
      m->state = CANCEL;
    } else if(keySym==XK_Return) {
      rfbSelectData* m = (rfbSelectData*)cl->screen->screenData;
      m->state = OK;
    } else {
      rfbSelectData* m = (rfbSelectData*)cl->screen->screenData;
      int curSel=m->selected;
      if(keySym==XK_Up) {
	if(curSel>0)
	  selSelect(m,curSel-1);
      } else if(keySym==XK_Down) {
	if(curSel+1<m->listSize)
	  selSelect(m,curSel+1);
      } else {
	if(keySym==XK_Page_Down) {
	  if(curSel+m->pageH<m->listSize)
	    selSelect(m,curSel+m->pageH);
	  else
	    selSelect(m,m->listSize-1);
	} else if(keySym==XK_Page_Up) {
	  if(curSel-m->pageH>=0)
	    selSelect(m,curSel-m->pageH);
	  else
	    selSelect(m,0);
	}
      }
    }
  }
}

static void selPtrAddEvent(int buttonMask,int x,int y,rfbClientPtr cl)
{
  rfbSelectData* m = (rfbSelectData*)cl->screen->screenData;
  if(y<m->okY && y>=m->okY-m->textH) {
    if(x>=m->okBX && x<m->okBX+m->buttonWidth) {
      if(!m->okInverted)
	selPaintButtons(m,TRUE,FALSE);
      if(buttonMask)
	m->state = OK;
    } else if(x>=m->cancelBX && x<m->cancelBX+m->buttonWidth) {
      if(!m->cancelInverted)
	selPaintButtons(m,FALSE,TRUE);
      if(buttonMask)
	m->state = CANCEL;
    } else if(m->okInverted || m->cancelInverted)
      selPaintButtons(m,FALSE,FALSE);
  } else {
    if(m->okInverted || m->cancelInverted)
      selPaintButtons(m,FALSE,FALSE);
    if(!m->lastButtons && buttonMask) {
      if(x>=m->x1 && x<m->x2 && y>=m->y1 && y<m->y2)
	selSelect(m,m->displayStart+(y-m->y1)/m->textH);
    }
  }
  m->lastButtons = buttonMask;

  /* todo: scrollbars */
}

static rfbCursorPtr selGetCursorPtr(rfbClientPtr cl)
{
  return NULL;
}

int rfbSelectBox(rfbScreenInfoPtr rfbScreen,rfbFontDataPtr font,
		 char** list,
		 int x1,int y1,int x2,int y2,
		 rfbPixel colour,rfbPixel backColour,
		 int border,SelectionChangedHookPtr selChangedHook)
{
   int bpp = rfbScreen->bitsPerPixel/8;
   char* frameBufferBackup;
   void* screenDataBackup = rfbScreen->screenData;
   rfbKbdAddEventProcPtr kbdAddEventBackup = rfbScreen->kbdAddEvent;
   rfbPtrAddEventProcPtr ptrAddEventBackup = rfbScreen->ptrAddEvent;
   rfbGetCursorProcPtr getCursorPtrBackup = rfbScreen->getCursorPtr;
   rfbDisplayHookPtr displayHookBackup = rfbScreen->displayHook;
   rfbSelectData selData;
   int i,j,k;
   int fx1,fy1,fx2,fy2; /* for font bbox */

   if(list==0 || *list==0)
     return(-1);
   
   rfbWholeFontBBox(font, &fx1, &fy1, &fx2, &fy2);
   selData.textH = fy2-fy1;
   /* I need at least one line for the choice and one for the buttons */
   if(y2-y1<selData.textH*2+3*border)
     return(-1);
   selData.xhot = -fx1;
   selData.yhot = -fy2;
   selData.x1 = x1+border;
   selData.y1 = y1+border;
   selData.y2 = y2-selData.textH-3*border;
   selData.x2 = x2-2*border;
   selData.pageH = (selData.y2-selData.y1)/selData.textH;

   i = rfbWidthOfString(font,okStr);
   j = rfbWidthOfString(font,cancelStr);
   selData.buttonWidth= k = 4*border+(i<j?j:i);
   selData.okBX = x1+(x2-x1-2*k)/3;
   if(selData.okBX<x1+border) /* too narrow! */
     return(-1);
   selData.cancelBX = x1+k+(x2-x1-2*k)*2/3;
   selData.okX = selData.okBX+(k-i)/2;
   selData.cancelX = selData.cancelBX+(k-j)/2;
   selData.okY = y2-border;

   frameBufferBackup = (char*)malloc((size_t)bpp*(x2-x1)*(y2-y1));
   if (!frameBufferBackup)
       return(-1);

   selData.state = SELECTING;
   selData.screen = rfbScreen;
   selData.font = font;
   selData.list = list;
   selData.colour = colour;
   selData.backColour = backColour;
   for(i=0;list[i];i++);
   selData.selected = i;
   selData.listSize = i;
   selData.displayStart = i;
   selData.lastButtons = 0;
   selData.selChangedHook = selChangedHook;
   
   rfbScreen->screenData = &selData;
   rfbScreen->kbdAddEvent = selKbdAddEvent;
   rfbScreen->ptrAddEvent = selPtrAddEvent;
   rfbScreen->getCursorPtr = selGetCursorPtr;
   rfbScreen->displayHook = NULL;
   
   /* backup screen */
   for(j=0;j<y2-y1;j++)
     memcpy(frameBufferBackup+j*(x2-x1)*bpp,
	    rfbScreen->frameBuffer+j*rfbScreen->paddedWidthInBytes+x1*bpp,
	    (size_t)(x2-x1)*bpp);

   /* paint list and buttons */
   rfbFillRect(rfbScreen,x1,y1,x2,y2,colour);
   selPaintButtons(&selData,FALSE,FALSE);
   selSelect(&selData,0);

   /* modal loop */
   while(selData.state == SELECTING)
     rfbProcessEvents(rfbScreen,20000);

   /* copy back screen data */
   for(j=0;j<y2-y1;j++)
     memcpy(rfbScreen->frameBuffer+j*rfbScreen->paddedWidthInBytes+x1*bpp,
	    frameBufferBackup+j*(x2-x1)*bpp,
	    (size_t)(x2-x1)*bpp);
   free(frameBufferBackup);
   rfbMarkRectAsModified(rfbScreen,x1,y1,x2,y2);
   rfbScreen->screenData = screenDataBackup;
   rfbScreen->kbdAddEvent = kbdAddEventBackup;
   rfbScreen->ptrAddEvent = ptrAddEventBackup;
   rfbScreen->getCursorPtr = getCursorPtrBackup;
   rfbScreen->displayHook = displayHookBackup;

   if(selData.state==CANCEL)
     selData.selected=-1;
   return(selData.selected);
}

