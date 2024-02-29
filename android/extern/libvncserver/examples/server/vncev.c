/**
 * @example vncev.c
 * This program is a simple server to show events coming from the client
*/
#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#endif
#include <rfb/rfbconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#if LIBVNCSERVER_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <rfb/rfb.h>
#include <rfb/default8x16.h>

#define width 100
#define height 100
static char f[width*height];
static char* keys[0x400];

static int hex2number(unsigned char c)
{
   if(c>'f') return(-1);
   else if(c>'F')
     return(10+c-'a');
   else if(c>'9')
     return(10+c-'A');
   else
     return(c-'0');
}

static void read_keys(void)
{
   int i,j,k;
   char buffer[1024];
   FILE* keysyms=fopen("keysym.h","r");

   memset(keys,0,0x400*sizeof(char*));
   
   if(!keysyms)
     return;
   
   while(!feof(keysyms)) {
      fgets(buffer,1024,keysyms);
      if(!strncmp(buffer,"#define XK_",strlen("#define XK_"))) {
	 for(i=strlen("#define XK_");buffer[i] && buffer[i]!=' '
	     && buffer[i]!='\t';i++);
	 if(buffer[i]==0) /* don't support wrapped lines */
	   continue;
	 buffer[i]=0;
	 for(i++;buffer[i] && buffer[i]!='0';i++);
	 if(buffer[i]==0 || buffer[i+1]!='x') continue;
	 for(j=0,i+=2;(k=hex2number(buffer[i]))>=0;i++)
	   j=j*16+k;
	 if(keys[j&0x3ff]) {
	    char* x=(char*)malloc(1+strlen(keys[j&0x3ff])+1+strlen(buffer+strlen("#define ")));
	    if(!x) {
	      memset(keys,0,0x400*sizeof(char*));
	      fclose(keysyms);
	      return;
	    }
	    strcpy(x,keys[j&0x3ff]);
	    strcat(x,",");
	    strcat(x,buffer+strlen("#define "));
	    free(keys[j&0x3ff]);
	    keys[j&0x3ff]=x;
	 } else
	   keys[j&0x3ff] = strdup(buffer+strlen("#define "));
      }
      
   }
   fclose(keysyms);
}

static int lineHeight=16,lineY=height-16;
static void output(rfbScreenInfoPtr s,char* line)
{
   rfbDoCopyRect(s,0,0,width,height-lineHeight,0,-lineHeight);
   rfbDrawString(s,&default8x16Font,10,lineY,line,0x01);
   rfbLog("%s\n",line);
}

static void dokey(rfbBool down,rfbKeySym k,rfbClientPtr cl)
{
   char buffer[1024+32];
   
   sprintf(buffer,"%s: %s (0x%x)",
	   down?"down":"up",keys[k&0x3ff]?keys[k&0x3ff]:"",(unsigned int)k);
   output(cl->screen,buffer);
}

static void doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
   char buffer[1024];
   if(buttonMask) {
      sprintf(buffer,"Ptr: mouse button mask 0x%x at %d,%d",buttonMask,x,y);
      output(cl->screen,buffer);
   }
   
}

static enum rfbNewClientAction newclient(rfbClientPtr cl)
{
   char buffer[1024];
   struct sockaddr_in addr;
   socklen_t len=sizeof(addr);
   unsigned int ip;
   
   getpeername(cl->sock,(struct sockaddr*)&addr,&len);
   ip=ntohl(addr.sin_addr.s_addr);
   sprintf(buffer,"Client connected from ip %d.%d.%d.%d",
	   (ip>>24)&0xff,(ip>>16)&0xff,(ip>>8)&0xff,ip&0xff);
   output(cl->screen,buffer);
   return RFB_CLIENT_ACCEPT;
}

int main(int argc,char** argv)
{
   rfbScreenInfoPtr s=rfbGetScreen(&argc,argv,width,height,8,1,1);
   if(!s)
     return 1;
   s->colourMap.is16=FALSE;
   s->colourMap.count=2;
   s->colourMap.data.bytes=(unsigned char*)"\xd0\xd0\xd0\x30\x01\xe0";
   s->serverFormat.trueColour=FALSE;
   s->frameBuffer=f;
   s->kbdAddEvent=dokey;
   s->ptrAddEvent=doptr;
   s->newClientHook=newclient;
   
   memset(f,0,width*height);
   read_keys();
   rfbInitServer(s);
   
   while(1) {
      rfbProcessEvents(s,999999);
   }
}
