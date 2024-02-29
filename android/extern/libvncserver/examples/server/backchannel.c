#include <rfb/rfb.h>

/**
 * @example backchannel.c
 * This is a simple example demonstrating a protocol extension.
 *
 * The "back channel" permits sending commands between client and server.
 * It works by sending plain text messages.
 *
 * As suggested in the RFB protocol, the back channel is enabled by asking
 * for a "pseudo encoding", and enabling the back channel on the client side
 * as soon as it gets a back channel message from the server.
 *
 * This implements the server part.
 *
 * Note: If you design your own extension and want it to be useful for others,
 * too, you should make sure that
 *
 * - your server as well as your client can speak to other clients and
 *   servers respectively (i.e. they are nice if they are talking to a
 *   program which does not know about your extension).
 *
 * - if the machine is little endian, all 16-bit and 32-bit integers are
 *   swapped before they are sent and after they are received.
 *
 */

#define rfbBackChannel 155

typedef struct backChannelMsg {
	uint8_t type;
	uint8_t pad1;
	uint16_t pad2;
	uint32_t size;
} backChannelMsg;

rfbBool enableBackChannel(rfbClientPtr cl, void** data, int encoding)
{
	if(encoding == rfbBackChannel) {
		backChannelMsg msg;
		const char* text="Server acknowledges back channel encoding\n";
		uint32_t length = strlen(text)+1;
		int n;

		rfbLog("Enabling the back channel\n");

		msg.type = rfbBackChannel;
		msg.size = Swap32IfLE(length);
		if((n = rfbWriteExact(cl, (char*)&msg, sizeof(msg))) <= 0 ||
				(n = rfbWriteExact(cl, text, length)) <= 0) {
			rfbLogPerror("enableBackChannel: write");
		}
		return TRUE;
	}
	return FALSE;
}

static rfbBool handleBackChannelMessage(rfbClientPtr cl, void* data,
		const rfbClientToServerMsg* message)
{
	if(message->type == rfbBackChannel) {
		backChannelMsg msg;
		char* text;
		int n;
		if((n = rfbReadExact(cl, ((char*)&msg)+1, sizeof(backChannelMsg)-1)) <= 0) {
			if(n != 0)
				rfbLogPerror("handleBackChannelMessage: read");
			rfbCloseClient(cl);
			return TRUE;
		}
		msg.size = Swap32IfLE(msg.size);
		if((text = malloc(msg.size)) == NULL) {
			rfbErr("Could not allocate %d bytes\n", msg.size);
			return TRUE;
		}
		if((n = rfbReadExact(cl, text, msg.size)) <= 0) {
			if(n != 0)
				rfbLogPerror("handleBackChannelMessage: read");
			rfbCloseClient(cl);
			return TRUE;
		}
		rfbLog("got message:\n%s\n", text);
		free(text);
		return TRUE;
	}
	return FALSE;
}

static int backChannelEncodings[] = {rfbBackChannel, 0};

static rfbProtocolExtension backChannelExtension = {
	NULL,				/* newClient */
	NULL,				/* init */
	backChannelEncodings,		/* pseudoEncodings */
	enableBackChannel,		/* enablePseudoEncoding */
	handleBackChannelMessage,	/* handleMessage */
	NULL,				/* close */
	NULL,				/* usage */
	NULL,				/* processArgument */
	NULL				/* next extension */
};

int main(int argc,char** argv)
{                                                                
	rfbScreenInfoPtr server;

	rfbRegisterProtocolExtension(&backChannelExtension);

	server=rfbGetScreen(&argc,argv,400,300,8,3,4);
	if(!server)
	  return 1;
	server->frameBuffer=(char*)malloc(400*300*4);
	rfbInitServer(server);           
	rfbRunEventLoop(server,-1,FALSE);
	return(0);
}
