/**
 * @example SDLvncviewer.c
 * Once built, you can run it via `SDLvncviewer <remote-host>`.
 */

#include <SDL.h>
#include <signal.h>
#include <rfb/rfbclient.h>

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{4, rfbButton4Mask},
	{5, rfbButton5Mask},
	{0,0}
};

struct { char mask; int bits_stored; } utf8Mapping[]= {
	{0b00111111, 6},
	{0b01111111, 7},
	{0b00011111, 5},
	{0b00001111, 4},
	{0b00000111, 3},
	{0,0}
};

static int enableResizable = 1, viewOnly, listenLoop, buttonMask;
int sdlFlags;
SDL_Texture *sdlTexture;
SDL_Renderer *sdlRenderer;
SDL_Window *sdlWindow;
/* client's pointer position */
int x,y;

static int rightAltKeyDown, leftAltKeyDown;

static rfbBool resize(rfbClient* client) {
	int width=client->width,height=client->height,
		depth=client->format.bitsPerPixel;

	if (enableResizable)
		sdlFlags |= SDL_WINDOW_RESIZABLE;

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width; client->updateRect.h = height;

	/* (re)create the surface used as the client's framebuffer */
	SDL_FreeSurface(rfbClientGetClientData(client, SDL_Init));
	SDL_Surface* sdl=SDL_CreateRGBSurface(0,
					      width,
					      height,
					      depth,
					      0,0,0,0);
	if(!sdl)
	    rfbClientErr("resize: error creating surface: %s\n", SDL_GetError());

	rfbClientSetClientData(client, SDL_Init, sdl);
	client->width = sdl->pitch / (depth / 8);
	client->frameBuffer=sdl->pixels;

	client->format.bitsPerPixel=depth;
	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;
	client->format.redMax=sdl->format->Rmask>>client->format.redShift;
	client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
	client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
	SetFormatAndEncodings(client);

	/* create or resize the window */
	if(!sdlWindow) {
	    sdlWindow = SDL_CreateWindow(client->desktopName,
					 SDL_WINDOWPOS_UNDEFINED,
					 SDL_WINDOWPOS_UNDEFINED,
					 width,
					 height,
					 sdlFlags);
	    if(!sdlWindow)
		rfbClientErr("resize: error creating window: %s\n", SDL_GetError());
	} else {
	    SDL_SetWindowSize(sdlWindow, width, height);
	}

	/* create the renderer if it does not already exist */
	if(!sdlRenderer) {
	    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
	    if(!sdlRenderer)
		rfbClientErr("resize: error creating renderer: %s\n", SDL_GetError());
	    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  /* make the scaled rendering look smoother. */
	}
	SDL_RenderSetLogicalSize(sdlRenderer, width, height);  /* this is a departure from the SDL1.2-based version, but more in the sense of a VNC viewer in keeeping aspect ratio */

	/* (re)create the texture that sits in between the surface->pixels and the renderer */
	if(sdlTexture)
	    SDL_DestroyTexture(sdlTexture);
	sdlTexture = SDL_CreateTexture(sdlRenderer,
				       SDL_PIXELFORMAT_ARGB8888,
				       SDL_TEXTUREACCESS_STREAMING,
				       width, height);
	if(!sdlTexture)
	    rfbClientErr("resize: error creating texture: %s\n", SDL_GetError());
	return TRUE;
}

static rfbKeySym SDL_key2rfbKeySym(SDL_KeyboardEvent* e) {
	rfbKeySym k = 0;
	SDL_Keycode sym = e->keysym.sym;

	switch (sym) {
	case SDLK_BACKSPACE: k = XK_BackSpace; break;
	case SDLK_TAB: k = XK_Tab; break;
	case SDLK_CLEAR: k = XK_Clear; break;
	case SDLK_RETURN: k = XK_Return; break;
	case SDLK_PAUSE: k = XK_Pause; break;
	case SDLK_ESCAPE: k = XK_Escape; break;
	case SDLK_DELETE: k = XK_Delete; break;
	case SDLK_KP_0: k = XK_KP_0; break;
	case SDLK_KP_1: k = XK_KP_1; break;
	case SDLK_KP_2: k = XK_KP_2; break;
	case SDLK_KP_3: k = XK_KP_3; break;
	case SDLK_KP_4: k = XK_KP_4; break;
	case SDLK_KP_5: k = XK_KP_5; break;
	case SDLK_KP_6: k = XK_KP_6; break;
	case SDLK_KP_7: k = XK_KP_7; break;
	case SDLK_KP_8: k = XK_KP_8; break;
	case SDLK_KP_9: k = XK_KP_9; break;
	case SDLK_KP_PERIOD: k = XK_KP_Decimal; break;
	case SDLK_KP_DIVIDE: k = XK_KP_Divide; break;
	case SDLK_KP_MULTIPLY: k = XK_KP_Multiply; break;
	case SDLK_KP_MINUS: k = XK_KP_Subtract; break;
	case SDLK_KP_PLUS: k = XK_KP_Add; break;
	case SDLK_KP_ENTER: k = XK_KP_Enter; break;
	case SDLK_KP_EQUALS: k = XK_KP_Equal; break;
	case SDLK_UP: k = XK_Up; break;
	case SDLK_DOWN: k = XK_Down; break;
	case SDLK_RIGHT: k = XK_Right; break;
	case SDLK_LEFT: k = XK_Left; break;
	case SDLK_INSERT: k = XK_Insert; break;
	case SDLK_HOME: k = XK_Home; break;
	case SDLK_END: k = XK_End; break;
	case SDLK_PAGEUP: k = XK_Page_Up; break;
	case SDLK_PAGEDOWN: k = XK_Page_Down; break;
	case SDLK_F1: k = XK_F1; break;
	case SDLK_F2: k = XK_F2; break;
	case SDLK_F3: k = XK_F3; break;
	case SDLK_F4: k = XK_F4; break;
	case SDLK_F5: k = XK_F5; break;
	case SDLK_F6: k = XK_F6; break;
	case SDLK_F7: k = XK_F7; break;
	case SDLK_F8: k = XK_F8; break;
	case SDLK_F9: k = XK_F9; break;
	case SDLK_F10: k = XK_F10; break;
	case SDLK_F11: k = XK_F11; break;
	case SDLK_F12: k = XK_F12; break;
	case SDLK_F13: k = XK_F13; break;
	case SDLK_F14: k = XK_F14; break;
	case SDLK_F15: k = XK_F15; break;
	case SDLK_NUMLOCKCLEAR: k = XK_Num_Lock; break;
	case SDLK_CAPSLOCK: k = XK_Caps_Lock; break;
	case SDLK_SCROLLLOCK: k = XK_Scroll_Lock; break;
	case SDLK_RSHIFT: k = XK_Shift_R; break;
	case SDLK_LSHIFT: k = XK_Shift_L; break;
	case SDLK_RCTRL: k = XK_Control_R; break;
	case SDLK_LCTRL: k = XK_Control_L; break;
	case SDLK_RALT: k = XK_Alt_R; break;
	case SDLK_LALT: k = XK_Alt_L; break;
	case SDLK_LGUI: k = XK_Super_L; break;
	case SDLK_RGUI: k = XK_Super_R; break;
#if 0
	case SDLK_COMPOSE: k = XK_Compose; break;
#endif
	case SDLK_MODE: k = XK_Mode_switch; break;
	case SDLK_HELP: k = XK_Help; break;
	case SDLK_PRINTSCREEN: k = XK_Print; break;
	case SDLK_SYSREQ: k = XK_Sys_Req; break;
	default: break;
	}
	/* SDL_TEXTINPUT does not generate characters if ctrl is down, so handle those here */
        if (k == 0 && sym > 0x0 && sym < 0x100 && e->keysym.mod & KMOD_CTRL)
               k = sym;

	return k;
}

/* UTF-8 decoding is from https://rosettacode.org/wiki/UTF-8_encode_and_decode which is under GFDL 1.2 */
static rfbKeySym utf8char2rfbKeySym(const char chr[4]) {
	int bytes = strlen(chr);
	int shift = utf8Mapping[0].bits_stored * (bytes - 1);
	rfbKeySym codep = (*chr++ & utf8Mapping[bytes].mask) << shift;
	int i;
	for(i = 1; i < bytes; ++i, ++chr) {
		shift -= utf8Mapping[0].bits_stored;
		codep |= ((char)*chr & utf8Mapping[0].mask) << shift;
	}
	return codep;
}

static void update(rfbClient* cl,int x,int y,int w,int h) {
	SDL_Surface *sdl = rfbClientGetClientData(cl, SDL_Init);
	/* update texture from surface->pixels */
	SDL_Rect r = {x,y,w,h};
 	if(SDL_UpdateTexture(sdlTexture, &r, sdl->pixels + y*sdl->pitch + x*4, sdl->pitch) < 0)
	    rfbClientErr("update: failed to update texture: %s\n", SDL_GetError());
	/* copy texture to renderer and show */
	if(SDL_RenderClear(sdlRenderer) < 0)
	    rfbClientErr("update: failed to clear renderer: %s\n", SDL_GetError());
	if(SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL) < 0)
	    rfbClientErr("update: failed to copy texture to renderer: %s\n", SDL_GetError());
	SDL_RenderPresent(sdlRenderer);
}

static void kbd_leds(rfbClient* cl, int value, int pad) {
	/* note: pad is for future expansion 0=unused */
	fprintf(stderr,"Led State= 0x%02X\n", value);
	fflush(stderr);
}

/* trivial support for textchat */
static void text_chat(rfbClient* cl, int value, char *text) {
	switch(value) {
	case rfbTextChatOpen:
		fprintf(stderr,"TextChat: We should open a textchat window!\n");
		TextChatOpen(cl);
		break;
	case rfbTextChatClose:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	case rfbTextChatFinished:
		fprintf(stderr,"TextChat: We should close our window!\n");
		break;
	default:
		fprintf(stderr,"TextChat: Received \"%s\"\n", text);
		break;
	}
	fflush(stderr);
}

#ifdef __MINGW32__
#define LOG_TO_FILE
#endif

#ifdef LOG_TO_FILE
#include <stdarg.h>
static void
log_to_file(const char *format, ...)
{
    FILE* logfile;
    static char* logfile_str=0;
    va_list args;
    char buf[256];
    time_t log_clock;

    if(!rfbEnableClientLogging)
      return;

    if(logfile_str==0) {
	logfile_str=getenv("VNCLOG");
	if(logfile_str==0)
	    logfile_str="vnc.log";
    }

    logfile=fopen(logfile_str,"a");

    va_start(args, format);

    time(&log_clock);
    strftime(buf, 255, "%d/%m/%Y %X ", localtime(&log_clock));
    fprintf(logfile,buf);

    vfprintf(logfile, format, args);
    fflush(logfile);

    va_end(args);
    fclose(logfile);
}
#endif


static void cleanup(rfbClient* cl)
{
  /*
    just in case we're running in listenLoop:
    close viewer window by restarting SDL video subsystem
  */
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  SDL_InitSubSystem(SDL_INIT_VIDEO);
  if(cl)
    rfbClientCleanup(cl);
}


static rfbBool handleSDLEvent(rfbClient *cl, SDL_Event *e)
{
	switch(e->type) {
	case SDL_WINDOWEVENT:
	    switch (e->window.event) {
	    case SDL_WINDOWEVENT_EXPOSED:
		SendFramebufferUpdateRequest(cl, 0, 0,
					cl->width, cl->height, FALSE);
		break;
	    case SDL_WINDOWEVENT_RESIZED:
	        SendExtDesktopSize(cl, e->window.data1, e->window.data2);
	        break;
	    case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (SDL_HasClipboardText()) {
		        char *text = SDL_GetClipboardText();
			if(text) {
			    rfbClientLog("sending clipboard text '%s'\n", text);
			    if(!SendClientCutTextUTF8(cl, text, strlen(text)))
			       SendClientCutText(cl, text, strlen(text));
			}
		}

		break;
	    case SDL_WINDOWEVENT_FOCUS_LOST:
		if (rightAltKeyDown) {
			SendKeyEvent(cl, XK_Alt_R, FALSE);
			rightAltKeyDown = FALSE;
			rfbClientLog("released right Alt key\n");
		}
		if (leftAltKeyDown) {
			SendKeyEvent(cl, XK_Alt_L, FALSE);
			leftAltKeyDown = FALSE;
			rfbClientLog("released left Alt key\n");
		}
		break;
	    }
	    break;
	case SDL_MOUSEWHEEL:
	{
	        int steps;
	        if (viewOnly)
			break;

		if(e->wheel.y > 0)
		    for(steps = 0; steps < e->wheel.y; ++steps) {
			SendPointerEvent(cl, x, y, rfbButton4Mask);
			SendPointerEvent(cl, x, y, 0);
		    }
		if(e->wheel.y < 0)
		    for(steps = 0; steps > e->wheel.y; --steps) {
			SendPointerEvent(cl, x, y, rfbButton5Mask);
			SendPointerEvent(cl, x, y, 0);
		    }
		if(e->wheel.x > 0)
		    for(steps = 0; steps < e->wheel.x; ++steps) {
			SendPointerEvent(cl, x, y, 0b01000000);
			SendPointerEvent(cl, x, y, 0);
		    }
		if(e->wheel.x < 0)
		    for(steps = 0; steps > e->wheel.x; --steps) {
			SendPointerEvent(cl, x, y, 0b00100000);
			SendPointerEvent(cl, x, y, 0);
		    }
		break;
	}
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		int state, i;
		if (viewOnly)
			break;

		if (e->type == SDL_MOUSEMOTION) {
			x = e->motion.x;
			y = e->motion.y;
			state = e->motion.state;
		}
		else {
			x = e->button.x;
			y = e->button.y;
			state = e->button.button;
			for (i = 0; buttonMapping[i].sdl; i++)
				if (state == buttonMapping[i].sdl) {
					state = buttonMapping[i].rfb;
					if (e->type == SDL_MOUSEBUTTONDOWN)
						buttonMask |= state;
					else
						buttonMask &= ~state;
					break;
				}
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask);
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		if (viewOnly)
			break;
		SendKeyEvent(cl, SDL_key2rfbKeySym(&e->key),
			e->type == SDL_KEYDOWN ? TRUE : FALSE);
		if (e->key.keysym.sym == SDLK_RALT)
			rightAltKeyDown = e->type == SDL_KEYDOWN;
		if (e->key.keysym.sym == SDLK_LALT)
			leftAltKeyDown = e->type == SDL_KEYDOWN;
		break;
	case SDL_TEXTINPUT:
                if (viewOnly)
			break;
		rfbKeySym sym = utf8char2rfbKeySym(e->text.text);
		SendKeyEvent(cl, sym, TRUE);
		SendKeyEvent(cl, sym, FALSE);
                break;
	case SDL_QUIT:
                if(listenLoop)
		  {
		    cleanup(cl);
		    return FALSE;
		  }
		else
		  {
		    rfbClientCleanup(cl);
		    exit(0);
		  }
	default:
		rfbClientLog("ignore SDL event: 0x%x\n", e->type);
	}
	return TRUE;
}

static void got_selection_latin1(rfbClient *cl, const char *text, int len)
{
        rfbClientLog("received latin1 clipboard text '%s'\n", text);
        if(SDL_SetClipboardText(text) != 0)
	    rfbClientErr("could not set received latin1 clipboard text: %s\n", SDL_GetError());
}

static void got_selection_utf8(rfbClient *cl, const char *buf, int len)
{
        rfbClientLog("received utf8 clipboard text '%s'\n", buf);
        if(SDL_SetClipboardText(buf) != 0)
	    rfbClientErr("could not set received utf8 clipboard text: %s\n", SDL_GetError());
}


static rfbCredential* get_credential(rfbClient* cl, int credentialType){
        rfbCredential *c = malloc(sizeof(rfbCredential));
	c->userCredential.username = malloc(RFB_BUF_SIZE);
	c->userCredential.password = malloc(RFB_BUF_SIZE);

	if(credentialType != rfbCredentialTypeUser) {
	    rfbClientErr("something else than username and password required for authentication\n");
	    return NULL;
	}

	rfbClientLog("username and password required for authentication!\n");
	printf("user: ");
	fgets(c->userCredential.username, RFB_BUF_SIZE, stdin);
	printf("pass: ");
	fgets(c->userCredential.password, RFB_BUF_SIZE, stdin);

	/* remove trailing newlines */
	c->userCredential.username[strcspn(c->userCredential.username, "\n")] = 0;
	c->userCredential.password[strcspn(c->userCredential.password, "\n")] = 0;

	return c;
}


#ifdef mac
#define main SDLmain
#endif

int main(int argc,char** argv) {
	rfbClient* cl;
	int i, j;
	SDL_Event e;

#ifdef LOG_TO_FILE
	rfbClientLog=rfbClientErr=log_to_file;
#endif

	for (i = 1, j = 1; i < argc; i++)
		if (!strcmp(argv[i], "-viewonly"))
			viewOnly = 1;
		else if (!strcmp(argv[i], "-resizable"))
			enableResizable = 1;
		else if (!strcmp(argv[i], "-no-resizable"))
			enableResizable = 0;
		else if (!strcmp(argv[i], "-listen")) {
		        listenLoop = 1;
			argv[i] = "-listennofork";
                        ++j;
		}
		else {
			if (i != j)
				argv[j] = argv[i];
			j++;
		}
	argc = j;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	atexit(SDL_Quit);
	signal(SIGINT, exit);

	do {
	  /* 16-bit: cl=rfbGetClient(5,3,2); */
	  cl=rfbGetClient(8,3,4);
	  cl->MallocFrameBuffer=resize;
	  cl->canHandleNewFBSize = TRUE;
	  cl->GotFrameBufferUpdate=update;
	  cl->HandleKeyboardLedState=kbd_leds;
	  cl->HandleTextChat=text_chat;
	  /* two different cut text handlers here for demo purposes, you
	     might as well use the same callback for both if it doesn't
	     matter for your application */
	  cl->GotXCutText = got_selection_latin1;
	  cl->GotXCutTextUTF8 = got_selection_utf8;
	  cl->GetCredential = get_credential;
	  cl->listenPort = LISTEN_PORT_OFFSET;
	  cl->listen6Port = LISTEN_PORT_OFFSET;
	  if(!rfbInitClient(cl,&argc,argv))
	    {
	      cl = NULL; /* rfbInitClient has already freed the client struct */
	      cleanup(cl);
	      break;
	    }

	  while(1) {
	    if(SDL_PollEvent(&e)) {
	      /*
		handleSDLEvent() return 0 if user requested window close.
		In this case, handleSDLEvent() will have called cleanup().
	      */
	      if(!handleSDLEvent(cl, &e))
		break;
	    }
	    else {
	      i=WaitForMessage(cl,500);
	      if(i<0)
		{
		  cleanup(cl);
		  break;
		}
	      if(i)
		if(!HandleRFBServerMessage(cl))
		  {
		    cleanup(cl);
		    break;
		  }
	    }
	  }
	}
	while(listenLoop);

	return 0;
}

