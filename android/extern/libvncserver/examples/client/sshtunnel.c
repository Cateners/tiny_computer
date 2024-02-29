/**
 * @example sshtunnel.c
 * An example of an RFB client tunneled through SSH by using libssh2.
 * This is based on https://www.libssh2.org/examples/direct_tcpip.html
 * with the following changes:
 *  - the listening is split out into a separate thread function
 *  - the listener gets closed immediately once a connection was accepted
 *  - the listening port is chosen by the OS, SO_REUSEADDR removed
 *  - global variables moved into SshData helper structure
 *  - added name resolution for the ssh host
 */

#include <rfb/rfbclient.h>
#include <libssh2.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#ifdef LIBVNCSERVER_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef LIBVNCSERVER_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif

/* The one global bool that's global so we can set it via
   a signal handler... */
int maintain_connection = 1;

typedef struct
{
    rfbClient *client;
    LIBSSH2_SESSION *session;
#ifdef LIBVNCSERVER_HAVE_LIBPTHREAD
    pthread_t thread;
#elif defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    uintptr_t thread;
#endif
    int ssh_sock;
    int local_listensock;
    int local_listenport;
    const char *remote_desthost;
    int remote_destport;
} SshData;


THREAD_ROUTINE_RETURN_TYPE ssh_proxy_loop(void *arg)
{
    SshData *data = arg;
    int rc, i;
    struct sockaddr_in sin;
    socklen_t sinlen;
    LIBSSH2_CHANNEL *channel = NULL;
    const char *shost;
    int sport;
    fd_set fds;
    struct timeval tv;
    ssize_t len, wr;
    char buf[16384];
    int proxy_sock = RFB_INVALID_SOCKET;

    proxy_sock = accept(data->local_listensock, (struct sockaddr *)&sin, &sinlen);
    if(proxy_sock == RFB_INVALID_SOCKET) {
        fprintf(stderr, "ssh_proxy_loop: accept: %s\n", strerror(errno));
        goto shutdown;
    }

    /* Close listener once a connection got accepted */
    rfbCloseSocket(data->local_listensock);

    shost = inet_ntoa(sin.sin_addr);
    sport = ntohs(sin.sin_port);

    printf("ssh_proxy_loop: forwarding connection from %s:%d here to remote %s:%d\n",
        shost, sport, data->remote_desthost, data->remote_destport);

    channel = libssh2_channel_direct_tcpip_ex(data->session, data->remote_desthost,
        data->remote_destport, shost, sport);
    if(!channel) {
        fprintf(stderr, "ssh_proxy_loop: Could not open the direct-tcpip channel!\n"
                "(Note that this can be a problem at the server!"
                " Please review the server logs.)\n");
        goto shutdown;
    }

    /* Must use non-blocking IO hereafter due to the current libssh2 API */
    libssh2_session_set_blocking(data->session, 0);

    while(1) {
        FD_ZERO(&fds);
        FD_SET(proxy_sock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        rc = select(proxy_sock + 1, &fds, NULL, NULL, &tv);
        if(-1 == rc) {
            fprintf(stderr, "ssh_proxy_loop: select: %s\n", strerror(errno));
            goto shutdown;
        }
        if(rc && FD_ISSET(proxy_sock, &fds)) {
            len = recv(proxy_sock, buf, sizeof(buf), 0);
            if(len < 0) {
                fprintf(stderr, "read: %s\n", strerror(errno));
                goto shutdown;
            }
            else if(0 == len) {
                fprintf(stderr, "ssh_proxy_loop: the client at %s:%d disconnected!\n", shost,
                    sport);
                goto shutdown;
            }
            wr = 0;
            while(wr < len) {
                i = libssh2_channel_write(channel, buf + wr, len - wr);
                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    fprintf(stderr, "ssh_proxy_loop: libssh2_channel_write: %d\n", i);
                    goto shutdown;
                }
                wr += i;
            }
        }
        while(1) {
            len = libssh2_channel_read(channel, buf, sizeof(buf));
            if(LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if(len < 0) {
                fprintf(stderr, "ssh_proxy_loop: libssh2_channel_read: %d\n", (int)len);
                goto shutdown;
            }
            wr = 0;
            while(wr < len) {
                i = send(proxy_sock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    fprintf(stderr, "ssh_proxy_loop: write: %s\n", strerror(errno));
                    goto shutdown;
                }
                wr += i;
            }
            if(libssh2_channel_eof(channel)) {
                fprintf(stderr, "ssh_proxy_loop: the server at %s:%d disconnected!\n",
                    data->remote_desthost, data->remote_destport);
                goto shutdown;
            }
        }
    }

 shutdown:

    printf("ssh_proxy_loop: shutting down\n");

    rfbCloseSocket(proxy_sock);

    if(channel)
        libssh2_channel_free(channel);

    libssh2_session_disconnect(data->session, "Client disconnecting normally");
    libssh2_session_free(data->session);

    rfbCloseSocket(data->ssh_sock);

    return THREAD_ROUTINE_RETURN_VALUE;
}

/**
   Decide whether or not the SSH tunnel setup should continue
   based on the current host and its fingerprint.
   Business logic is up to the implementer in a real app, i.e.
   compare keys, ask user etc...
   @return -1 if tunnel setup should be aborted
            0 if tunnel setup should continue
 */
int ssh_fingerprint_check(const char *fingerprint, size_t fingerprint_len,
                          const char *host, rfbClient *client)
{
    size_t i;
    fprintf(stderr, "ssh_fingerprint_check: host %s has ", host);
    for(i = 0; i < fingerprint_len; i++)
        printf("%02X ", (unsigned char)fingerprint[i]);
    printf("\n");

    return 0;
}


/**
   Creates an SSH tunnel and a local proxy and returns the port the proxy is listening on.
   @return A pointer to an SshData structure or NULL on error.
 */
SshData* ssh_tunnel_open(const char *ssh_host,
			 const char *ssh_user,
			 const char *ssh_password,
			 const char *ssh_pub_key_path,
			 const char *ssh_priv_key_path,
			 const char *ssh_priv_key_password,
			 const char *rfb_host,
			 int rfb_port,
			 rfbClient *client)
{
    int rc, i;
    struct sockaddr_in sin;
    socklen_t sinlen;
    const char *fingerprint;
    char *userauthlist;
    struct addrinfo hints, *res;
    SshData *data;

    /* Sanity checks */
    if(!ssh_host || !ssh_user || !rfb_host) /* these must be set */
	return NULL;

    data = calloc(1, sizeof(SshData));

    data->client = client;
    data->remote_desthost = rfb_host; /* resolved by the server */
    data->remote_destport = rfb_port;

    /* Connect to SSH server */
    data->ssh_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(data->ssh_sock == RFB_INVALID_SOCKET) {
        fprintf(stderr, "ssh_tunnel_open: socket: %s\n", strerror(errno));
        goto error;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rc = getaddrinfo(ssh_host, NULL, &hints, &res)) == 0) {
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = (((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
	freeaddrinfo(res);
    } else {
        fprintf(stderr, "ssh_tunnel_open: getaddrinfo: %s\n", gai_strerror(rc));
	goto error;
    }

    sin.sin_port = htons(22);
    if(connect(data->ssh_sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        fprintf(stderr, "ssh_tunnel_open: failed to connect to SSH server!\n");
	goto error;
    }

    /* Create a session instance */
    data->session = libssh2_session_init();
    if(!data->session) {
        fprintf(stderr, "ssh_tunnel_open: could not initialize SSH session!\n");
	goto error;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(data->session, data->ssh_sock);
    if(rc) {
        fprintf(stderr, "ssh_tunnel_open: error when starting up SSH session: %d\n", rc);
        goto error;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(data->session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if(ssh_fingerprint_check(fingerprint, 32, ssh_host, data->client) == -1) {
        fprintf(stderr, "ssh_tunnel_open: fingerprint check indicated tunnel setup stop\n");
        goto error;
    }

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(data->session, ssh_user, strlen(ssh_user));
    printf("ssh_tunnel_open: authentication methods: %s\n", userauthlist);

    if(ssh_password && strstr(userauthlist, "password")) {
        if(libssh2_userauth_password(data->session, ssh_user, ssh_password)) {
            fprintf(stderr, "ssh_tunnel_open: authentication by password failed.\n");
            goto error;
        }
    }
    else if(ssh_priv_key_path && ssh_priv_key_password && strstr(userauthlist, "publickey")) {
        if(libssh2_userauth_publickey_fromfile(data->session, ssh_user, ssh_pub_key_path,
                                               ssh_priv_key_path, ssh_priv_key_password)) {
            fprintf(stderr, "ssh_tunnel_open: authentication by public key failed!\n");
            goto error;
        }
    }
    else {
        fprintf(stderr, "ssh_tunnel_open: no supported authentication methods found!\n");
        goto error;
    }

    /* Create and bind the local listening socket */
    data->local_listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(data->local_listensock == RFB_INVALID_SOCKET) {
        fprintf(stderr, "ssh_tunnel_open: socket: %s\n", strerror(errno));
        return NULL;
    }
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0); /* let the OS choose the port */
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(INADDR_NONE == sin.sin_addr.s_addr) {
        fprintf(stderr, "ssh_tunnel_open: inet_addr: %s\n", strerror(errno));
        goto error;
    }
    sinlen = sizeof(sin);
    if(-1 == bind(data->local_listensock, (struct sockaddr *)&sin, sinlen)) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto error;
    }
    if(-1 == listen(data->local_listensock, 1)) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        goto error;
    }

    /* get info back from OS */
    if (getsockname(data->local_listensock, (struct sockaddr *)&sin, &sinlen ) == -1){
	fprintf(stderr, "ssh_tunnel_open: getsockname: %s\n", strerror(errno));
	goto error;
    }

    data->local_listenport = ntohs(sin.sin_port);

    printf("ssh_tunnel_open: waiting for TCP connection on %s:%d...\n",
        inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));


    /* Create the proxy thread */
#if defined(LIBVNCSERVER_HAVE_LIBPTHREAD)
    if (pthread_create(&data->thread, NULL, ssh_proxy_loop, data) != 0) {
#elif defined(LIBVNCSERVER_HAVE_WIN32THREADS)
    if(data->thread = _beginthread(proxy_loop, 0, data) == 0);
#endif
	fprintf(stderr, "ssh_tunnel_open: proxy thread creation failed\n");
	goto error;
    }

    return data;

 error:
    if (data->session) {
	libssh2_session_disconnect(data->session, "Error in SSH tunnel setup");
	libssh2_session_free(data->session);
    }

    rfbCloseSocket(data->local_listensock);
    rfbCloseSocket(data->ssh_sock);

    free(data);

    return NULL;
}


void ssh_tunnel_close(SshData *data) {
    if(!data)
	return;

    /* the proxy thread does the internal cleanup as it can be
       ended due to external reasons */
    THREAD_JOIN(data->thread);

    free(data);

    printf("ssh_tunnel_close: done\n");
}


void intHandler(int dummy) {
    maintain_connection = 0;
}


int main(int argc, char *argv[])
{
    rfbClient *client = rfbGetClient(8,3,4);

    /*
      Get args and create SSH tunnel
     */
    int rc = libssh2_init(0);
    if(rc) {
        fprintf(stderr, "libssh2 initialization failed (%d)\n", rc);
        return EXIT_FAILURE;
    }

    SshData *data;
    if (argc == 6) {
	/* SSH tunnel w/ password */
	data = ssh_tunnel_open(argv[1], argv[2], argv[3], NULL, NULL, NULL, argv[4], atoi(argv[5]), client);
    } else if (argc == 8) {
	/* SSH tunnel w/ privkey */
	data = ssh_tunnel_open(argv[1], argv[2], NULL, argv[3], argv[4], argv[5], argv[6], atoi(argv[7]), client);
    } else {
	fprintf(stderr,
		"Usage (w/ password): %s <ssh-server-IP> <ssh-server-username> <ssh-server-password> <rfb-host> <rfb-port>\n"
		"Usage (w/ privkey):  %s <ssh-server-IP> <ssh-server-username> <pubkey_filename> <privkey_filename> <privkey_password> <rfb-host> <rfb-port>\n",
		argv[0], argv[0]);
	return(EXIT_FAILURE);
    }


    /*
      The actual VNC connection setup.
     */
    client->serverHost = strdup("127.0.0.1");
    if(data) // might be NULL if ssh setup failed
	client->serverPort = data->local_listenport;
    rfbClientSetClientData(client, (void*)42, data);

    if (!data || !rfbInitClient(client,NULL,NULL))
	return EXIT_FAILURE;

    printf("Successfully connected to %s:%d - hit Ctrl-C to disconnect\n", client->serverHost, client->serverPort);

    signal(SIGINT, intHandler);

    while (maintain_connection) {
	int n = WaitForMessage(client,50);
	if(n < 0)
	    break;
	if(n)
	    if(!HandleRFBServerMessage(client))
		break;
    }

    /* Disconnect client inside tunnel */
    if(client && client->sock != RFB_INVALID_SOCKET)
	    rfbCloseSocket(client->sock);

    /* Close the tunnel and clean up */
    ssh_tunnel_close(rfbClientGetClientData(client, (void*)42));

    /* free client */
    rfbClientCleanup(client);

    /* Teardown libssh2 */
    libssh2_exit();

    return EXIT_SUCCESS;
}

