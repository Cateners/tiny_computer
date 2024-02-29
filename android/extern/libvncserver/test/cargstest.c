#include <rfb/rfb.h>

int main(int argc,char** argv)
{
	int fake_argc=6;
	char* fake_argv[6]={
		"dummy_program","-alwaysshared","-httpport","3002","-nothing","-dontdisconnect"
	};
	rfbScreenInfoPtr screen;
	rfbBool ret=0;

	screen = rfbGetScreen(&fake_argc,fake_argv,1024,768,8,3,1);
	if(!screen)
          return 0;

#define CHECK(a,b) if(screen->a!=b) { fprintf(stderr,#a " is %d (should be " #b ")\n",screen->a); ret=1; }
	CHECK(width,1024);
	CHECK(height,768);
	CHECK(alwaysShared,TRUE);
	CHECK(httpPort,3002);
	CHECK(dontDisconnect,TRUE);
	if(fake_argc!=2) {
		fprintf(stderr,"fake_argc is %d (should be 2)\n",fake_argc);
		ret=1;
	}
	if(strcmp(fake_argv[1],"-nothing")) {
		fprintf(stderr,"fake_argv[1] is %s (should be -nothing)\n",fake_argv[1]);
		ret=1;
	}
	return ret;
}

