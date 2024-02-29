/*
 *  This parses the command line arguments. It was separated from main.c by
 *  Justin Dearing <jdeari01@longisland.poly.edu>.
 */

/*
 *  LibVNCServer (C) 2001 Johannes E. Schindelin <Johannes.Schindelin@gmx.de>
 *  Original OSXvnc (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  see GPL (latest version) for full details
 */

#include <rfb/rfb.h>

extern int rfbStringToAddr(char *str, in_addr_t *iface);

void
rfbUsage(void)
{
    rfbProtocolExtension* extension;

    fprintf(stderr, "-rfbport port          TCP port for RFB protocol\n");
#ifdef LIBVNCSERVER_IPv6
    fprintf(stderr, "-rfbportv6 port        TCP6 port for RFB protocol\n");
#endif
    fprintf(stderr, "-rfbwait time          max time in ms to wait for RFB client\n");
    fprintf(stderr, "-rfbauth passwd-file   use authentication on RFB protocol\n"
                    "                       (use 'storepasswd' to create a password file)\n");
    fprintf(stderr, "-rfbversion 3.x        Set the version of the RFB we choose to advertise\n");
    fprintf(stderr, "-permitfiletransfer    permit file transfer support\n");
    fprintf(stderr, "-passwd plain-password use authentication \n"
                    "                       (use plain-password as password, USE AT YOUR RISK)\n");
    fprintf(stderr, "-deferupdate time      time in ms to defer updates "
                                                             "(default 40)\n");
    fprintf(stderr, "-deferptrupdate time   time in ms to defer pointer updates"
                                                           " (default none)\n");
    fprintf(stderr, "-desktop name          VNC desktop name (default \"LibVNCServer\")\n");
    fprintf(stderr, "-alwaysshared          always treat new clients as shared\n");
    fprintf(stderr, "-nevershared           never treat new clients as shared\n");
    fprintf(stderr, "-dontdisconnect        don't disconnect existing clients when a "
                                                             "new non-shared\n"
                    "                       connection comes in (refuse new connection "
                                                                "instead)\n");
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
    fprintf(stderr, "-sslkeyfile path       set path to private key file for encrypted WebSockets connections\n");
    fprintf(stderr, "-sslcertfile path      set path to certificate file for encrypted WebSockets connections\n");
#endif
    fprintf(stderr, "-httpdir dir-path      enable http server using dir-path home\n");
    fprintf(stderr, "-httpport portnum      use portnum for http connection\n");
#ifdef LIBVNCSERVER_IPv6
    fprintf(stderr, "-httpportv6 portnum    use portnum for IPv6 http connection\n");
#endif
    fprintf(stderr, "-enablehttpproxy       enable http proxy support\n");
    fprintf(stderr, "-progressive height    enable progressive updating for slow links\n");
    fprintf(stderr, "-listen ipaddr         listen for connections only on network interface with\n");
    fprintf(stderr, "                       addr ipaddr. '-listen localhost' and hostname work too.\n");
#ifdef LIBVNCSERVER_IPv6
    fprintf(stderr, "-listenv6 ipv6addr     listen for IPv6 connections only on network interface with\n");
    fprintf(stderr, "                       addr ipv6addr. '-listen localhost' and hostname work too.\n");
#endif

    for(extension=rfbGetExtensionIterator();extension;extension=extension->next)
	if(extension->usage)
		extension->usage();
    rfbReleaseExtensionIterator();
}

/* purges COUNT arguments from ARGV at POSITION and decrements ARGC.
   POSITION points to the first non purged argument afterwards. */
void rfbPurgeArguments(int* argc,int* position,int count,char *argv[])
{
  int amount=(*argc)-(*position)-count;
  if(amount)
    memmove(argv+(*position),argv+(*position)+count,sizeof(char*)*amount);
  (*argc)-=count;
}

rfbBool 
rfbProcessArguments(rfbScreenInfoPtr rfbScreen,int* argc, char *argv[])
{
    int i,i1;

    if(!argc) return TRUE;
    
    for (i = i1 = 1; i < *argc;) {
        if (strcmp(argv[i], "-help") == 0) {
	    rfbUsage();
	    return FALSE;
	} else if (strcmp(argv[i], "-rfbport") == 0) { /* -rfbport port */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
	    rfbScreen->port = atoi(argv[++i]);
#ifdef LIBVNCSERVER_IPv6
	} else if (strcmp(argv[i], "-rfbportv6") == 0) { /* -rfbportv6 port */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
	    rfbScreen->ipv6port = atoi(argv[++i]);
#endif
        } else if (strcmp(argv[i], "-rfbwait") == 0) {  /* -rfbwait ms */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
	    rfbScreen->maxClientWait = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbauth") == 0) {  /* -rfbauth passwd-file */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->authPasswdData = argv[++i];

        } else if (strcmp(argv[i], "-permitfiletransfer") == 0) {  /* -permitfiletransfer  */
            rfbScreen->permitFileTransfer = TRUE;
        } else if (strcmp(argv[i], "-rfbversion") == 0) {  /* -rfbversion 3.6  */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
	    sscanf(argv[++i],"%d.%d", &rfbScreen->protocolMajorVersion, &rfbScreen->protocolMinorVersion);
	} else if (strcmp(argv[i], "-passwd") == 0) {  /* -passwd password */
	    char **passwds = malloc(sizeof(char**)*2);
	    if (!passwds || i + 1 >= *argc) {
		rfbUsage();
		free(passwds);
		return FALSE;
	    }
	    passwds[0] = argv[++i];
	    passwds[1] = NULL;
	    rfbScreen->authPasswdData = (void*)passwds;
	    rfbScreen->passwordCheck = rfbCheckPasswordByList;
        } else if (strcmp(argv[i], "-deferupdate") == 0) {  /* -deferupdate milliseconds */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->deferUpdateTime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-deferptrupdate") == 0) {  /* -deferptrupdate milliseconds */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->deferPtrUpdateTime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-desktop") == 0) {  /* -desktop desktop-name */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->desktopName = argv[++i];
        } else if (strcmp(argv[i], "-alwaysshared") == 0) {
	    rfbScreen->alwaysShared = TRUE;
        } else if (strcmp(argv[i], "-nevershared") == 0) {
            rfbScreen->neverShared = TRUE;
        } else if (strcmp(argv[i], "-dontdisconnect") == 0) {
            rfbScreen->dontDisconnect = TRUE;
        } else if (strcmp(argv[i], "-httpdir") == 0) {  /* -httpdir directory-path */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->httpDir = argv[++i];
        } else if (strcmp(argv[i], "-httpport") == 0) {  /* -httpport portnum */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->httpPort = atoi(argv[++i]);
#ifdef LIBVNCSERVER_IPv6
	} else if (strcmp(argv[i], "-httpportv6") == 0) {  /* -httpportv6 portnum */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->http6Port = atoi(argv[++i]);
#endif
        } else if (strcmp(argv[i], "-enablehttpproxy") == 0) {
            rfbScreen->httpEnableProxyConnect = TRUE;
        } else if (strcmp(argv[i], "-progressive") == 0) {  /* -httpport portnum */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->progressiveSliceHeight = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-listen") == 0) {  /* -listen ipaddr */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            if (! rfbStringToAddr(argv[++i], &(rfbScreen->listenInterface))) {
                return FALSE;
            }
#ifdef LIBVNCSERVER_IPv6
	} else if (strcmp(argv[i], "-listenv6") == 0) {  /* -listenv6 ipv6addr */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
	    rfbScreen->listen6Interface = argv[++i];
#endif
#ifdef LIBVNCSERVER_WITH_WEBSOCKETS
        } else if (strcmp(argv[i], "-sslkeyfile") == 0) {  /* -sslkeyfile sslkeyfile */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->sslkeyfile = argv[++i];
        } else if (strcmp(argv[i], "-sslcertfile") == 0) {  /* -sslcertfile sslcertfile */
            if (i + 1 >= *argc) {
		rfbUsage();
		return FALSE;
	    }
            rfbScreen->sslcertfile = argv[++i];
#endif
        } else {
	    rfbProtocolExtension* extension;
	    int handled=0;

	    for(extension=rfbGetExtensionIterator();handled==0 && extension;
			extension=extension->next)
		if(extension->processArgument)
			handled = extension->processArgument(*argc - i, argv + i);
	    rfbReleaseExtensionIterator();

	    if(handled==0) {
		i++;
		i1=i;
		continue;
	    }
	    i+=handled-1;
	}
	/* we just remove the processed arguments from the list */
	rfbPurgeArguments(argc,&i1,i-i1+1,argv);
	i=i1;
    }
    return TRUE;
}

rfbBool 
rfbProcessSizeArguments(int* width,int* height,int* bpp,int* argc, char *argv[])
{
    int i,i1;

    if(!argc) return TRUE;
    for (i = i1 = 1; i < *argc-1;) {
        if (strcmp(argv[i], "-bpp") == 0) {
               *bpp = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-width") == 0) {
               *width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-height") == 0) {
               *height = atoi(argv[++i]);
        } else {
	    i++;
	    i1=i;
	    continue;
	}
	rfbPurgeArguments(argc,&i1,i-i1,argv);
	i=i1;
    }
    return TRUE;
}

