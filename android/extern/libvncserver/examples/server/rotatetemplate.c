#define OUT_T CONCAT3E(uint,OUTBITS,_t)
#define FUNCTION CONCAT2E(FUNCNAME,OUTBITS)

static void FUNCTION(rfbScreenInfoPtr screen)
{
	OUT_T* buffer = (OUT_T*)screen->frameBuffer;
	int i, j, w = screen->width, h = screen->height;
	OUT_T* newBuffer = (OUT_T*)malloc(w * h * sizeof(OUT_T));
	if (!newBuffer) return;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
			newBuffer[FUNC(i, j)] = buffer[i + j * w];

	memcpy(buffer, newBuffer, w * h * sizeof(OUT_T));
	free(newBuffer);

#ifdef SWAPDIMENSIONS
	screen->width = h;
	screen->paddedWidthInBytes = h * OUTBITS / 8;
	screen->height = w;

	{
		rfbClientIteratorPtr iterator;
		rfbClientPtr cl;
		iterator = rfbGetClientIterator(screen);
		while ((cl = rfbClientIteratorNext(iterator)) != NULL)
			cl->newFBSizePending = 1;
	}
#endif

	rfbMarkRectAsModified(screen, 0, 0, screen->width, screen->height);
}

#if OUTBITS == 32
void FUNCNAME(rfbScreenInfoPtr screen) {
	if (screen->serverFormat.bitsPerPixel == 32)
		CONCAT2E(FUNCNAME,32)(screen);
	else if (screen->serverFormat.bitsPerPixel == 16)
		CONCAT2E(FUNCNAME,16)(screen);
	else if (screen->serverFormat.bitsPerPixel == 8)
		CONCAT2E(FUNCNAME,8)(screen);
	else {
		rfbErr("Unsupported pixel depth: %d\n",
			screen->serverFormat.bitsPerPixel);
		return;
	}
}
#endif

#undef FUNCTION
#undef OUTBITS

