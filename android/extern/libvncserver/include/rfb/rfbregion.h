#ifndef SRAREGION_H
#define SRAREGION_H

/* -=- SRA - Simple Region Algorithm
 * A simple rectangular region implementation.
 * Copyright (c) 2001 James "Wez" Weatherall, Johannes E. Schindelin
 */

/* -=- sraRect */

typedef struct _rect {
	int x1;
	int y1;
	int x2;
	int y2;
} sraRect;

typedef struct sraRegion sraRegion;

/* -=- Region manipulation functions */

extern sraRegion *sraRgnCreate(void);
extern sraRegion *sraRgnCreateRect(int x1, int y1, int x2, int y2);
extern sraRegion *sraRgnCreateRgn(const sraRegion *src);

extern void sraRgnDestroy(sraRegion *rgn);
extern void sraRgnMakeEmpty(sraRegion *rgn);
extern rfbBool sraRgnAnd(sraRegion *dst, const sraRegion *src);
extern void sraRgnOr(sraRegion *dst, const sraRegion *src);
extern rfbBool sraRgnSubtract(sraRegion *dst, const sraRegion *src);

extern void sraRgnOffset(sraRegion *dst, int dx, int dy);

extern rfbBool sraRgnPopRect(sraRegion *region, sraRect *rect,
			  unsigned long flags);

extern unsigned long sraRgnCountRects(const sraRegion *rgn);
extern rfbBool sraRgnEmpty(const sraRegion *rgn);

extern sraRegion *sraRgnBBox(const sraRegion *src);

/* -=- rectangle iterator */

typedef struct sraRectangleIterator {
  rfbBool reverseX,reverseY;
  int ptrSize,ptrPos;
  struct sraSpan** sPtrs;
} sraRectangleIterator;

extern sraRectangleIterator *sraRgnGetIterator(sraRegion *s);
extern sraRectangleIterator *sraRgnGetReverseIterator(sraRegion *s,rfbBool reverseX,rfbBool reverseY);
extern rfbBool sraRgnIteratorNext(sraRectangleIterator *i,sraRect *r);
extern void sraRgnReleaseIterator(sraRectangleIterator *i);

void sraRgnPrint(const sraRegion *s);

/* -=- Rectangle clipper (for speed) */

extern rfbBool sraClipRect(int *x, int *y, int *w, int *h,
			int cx, int cy, int cw, int ch);

extern rfbBool sraClipRect2(int *x, int *y, int *x2, int *y2,
			int cx, int cy, int cx2, int cy2);

#endif
