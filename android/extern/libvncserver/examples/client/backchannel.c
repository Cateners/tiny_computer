/**
 * @example backchannel-client.c
 * A simple example of an RFB client
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <rfb/rfbclient.h>

static void HandleRect(rfbClient* client, int x, int y, int w, int h) {
}

/*
 * The client part of the back channel extension example.
 *
 */

#define rfbBackChannel 155

typedef struct backChannelMsg {
        uint8_t type;
        uint8_t pad1;
        uint16_t pad2;
        uint32_t size;
} backChannelMsg;

static void sendMessage(rfbClient* client, char* text)
{
	backChannelMsg msg;
	uint32_t length = strlen(text)+1;

	msg.type = rfbBackChannel;
	msg.size = rfbClientSwap32IfLE(length);
	if(!WriteToRFBServer(client, (char*)&msg, sizeof(msg)) ||
			!WriteToRFBServer(client, text, length)) {
		rfbClientLog("enableBackChannel: write error (%d: %s)", errno, strerror(errno));
	}
}

static rfbBool handleBackChannelMessage(rfbClient* client,
	rfbServerToClientMsg* message)
{
	backChannelMsg msg;
	char* text;

	if(message->type != rfbBackChannel)
		return FALSE;

	rfbClientSetClientData(client, sendMessage, sendMessage);

	if(!ReadFromRFBServer(client, ((char*)&msg)+1, sizeof(msg)-1))
		return TRUE;
	msg.size = rfbClientSwap32IfLE(msg.size);
	text = malloc(msg.size);
	if(!ReadFromRFBServer(client, text, msg.size)) {
		free(text);
		return TRUE;
	}

	rfbClientLog("got back channel message: %s\n", text);
	free(text);

	return TRUE;
}

static int backChannelEncodings[] = { rfbBackChannel, 0 };

static rfbClientProtocolExtension backChannel = {
	backChannelEncodings,		/* encodings */
	NULL,				/* handleEncoding */
	handleBackChannelMessage,	/* handleMessage */
	NULL,				/* next extension */
	NULL,				/* securityTypes */
	NULL				/* handleAuthentication */
};

int
main(int argc, char **argv)
{
	rfbClient* client = rfbGetClient(8,3,4);

	client->GotFrameBufferUpdate = HandleRect;
	rfbClientRegisterExtension(&backChannel);

	if (!rfbInitClient(client,&argc,argv))
		return 1;

	while (1) {
		/* After each idle second, send a message */
		if(WaitForMessage(client,1000000)>0)
			HandleRFBServerMessage(client);
		else if(rfbClientGetClientData(client, sendMessage))
			sendMessage(client, "Dear Server,\n"
					"thank you for understanding "
					"back channel messages!");
	}

	rfbClientCleanup(client);

	return 0;
}

