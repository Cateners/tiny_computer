/**
 * @example ppmtest.c
 * A simple example of an RFB client
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <rfb/rfbclient.h>

static void PrintRect(rfbClient* client, int x, int y, int w, int h) {
	rfbClientLog("Received an update for %d,%d,%d,%d.\n",x,y,w,h);
}

static void SaveFramebufferAsPPM(rfbClient* client, int x, int y, int w, int h) {
	static time_t t=0,t1;
	FILE* f;
	int i,j;
	rfbPixelFormat* pf=&client->format;
	int bpp=pf->bitsPerPixel/8;
	int row_stride=client->width*bpp;

	/* save one picture only if the last is older than 2 seconds */
	t1=time(NULL);
	if(t1-t>2)
		t=t1;
	else
		return;

	/* assert bpp=4 */
	if(bpp!=4 && bpp!=2) {
		rfbClientLog("bpp = %d (!=4)\n",bpp);
		return;
	}

	f=fopen("framebuffer.ppm","wb");
	if(!f) {
		rfbClientErr("Could not open framebuffer.ppm\n");
		return;
	}

	fprintf(f,"P6\n# %s\n%d %d\n255\n",client->desktopName,client->width,client->height);
	for(j=0;j<client->height*row_stride;j+=row_stride)
		for(i=0;i<client->width*bpp;i+=bpp) {
			unsigned char* p=client->frameBuffer+j+i;
			unsigned int v;
			if(bpp==4)
				v=*(unsigned int*)p;
			else if(bpp==2)
				v=*(unsigned short*)p;
			else
				v=*(unsigned char*)p;
			fputc((v>>pf->redShift)*256/(pf->redMax+1),f);
			fputc((v>>pf->greenShift)*256/(pf->greenMax+1),f);
			fputc((v>>pf->blueShift)*256/(pf->blueMax+1),f);
		}
	fclose(f);
}

char * getuser(rfbClient *client)
{
return strdup("testuser@test");
}

char * getpassword(rfbClient *client)
{
return strdup("Password");
}

int
main(int argc, char **argv)
{
	rfbClient* client = rfbGetClient(8,3,4);
	time_t t=time(NULL);

#ifdef LIBVNCSERVER_HAVE_SASL
        client->GetUser = getuser;
        client->GetPassword = getpassword;
#endif

	if(argc>1 && !strcmp("-print",argv[1])) {
		client->GotFrameBufferUpdate = PrintRect;
		argv[1]=argv[0]; argv++; argc--;
	} else
		client->GotFrameBufferUpdate = SaveFramebufferAsPPM;

	/* The -listen option is used to make us a daemon process which listens for
	   incoming connections from servers, rather than actively connecting to a
	   given server. The -tunnel and -via options are useful to create
	   connections tunneled via SSH port forwarding. We must test for the
	   -listen option before invoking any Xt functions - this is because we use
	   forking, and Xt doesn't seem to cope with forking very well. For -listen
	   option, when a successful incoming connection has been accepted,
	   listenForIncomingConnections() returns, setting the listenSpecified
	   flag. */

	if (!rfbInitClient(client,&argc,argv))
		return 1;

	/* TODO: better wait for update completion */
	while (time(NULL)-t<5) {
		static int i=0;
		fprintf(stderr,"\r%d",i++);
		int n = WaitForMessage(client,50);
		if(n < 0)
			break;
		if(n)
		    if(!HandleRFBServerMessage(client))
			break;
	}

	rfbClientCleanup(client);

	return 0;
}

