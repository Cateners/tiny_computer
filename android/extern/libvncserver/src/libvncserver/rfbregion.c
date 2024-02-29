/* -=- sraRegion.c
 * Copyright (c) 2001 James "Wez" Weatherall, Johannes E. Schindelin
 *
 * A general purpose region clipping library
 * Only deals with rectangular regions, though.
 */

#include <rfb/rfb.h>
#include <rfb/rfbregion.h>

/* -=- Internal Span structure */

struct sraRegion;

typedef struct sraSpan {
  struct sraSpan *_next;
  struct sraSpan *_prev;
  int start;
  int end;
  struct sraRegion *subspan;
} sraSpan;

typedef struct sraRegion {
  sraSpan front;
  sraSpan back;
} sraSpanList;

/* -=- Span routines */

sraSpanList *sraSpanListDup(const sraSpanList *src);
void sraSpanListDestroy(sraSpanList *list);

static sraSpan *
sraSpanCreate(int start, int end, const sraSpanList *subspan) {
  sraSpan *item = (sraSpan*)malloc(sizeof(sraSpan));
  if (!item) return NULL;
  item->_next = item->_prev = NULL;
  item->start = start;
  item->end = end;
  item->subspan = sraSpanListDup(subspan);
  return item;
}

static sraSpan *
sraSpanDup(const sraSpan *src) {
  sraSpan *span;
  if (!src) return NULL;
  span = sraSpanCreate(src->start, src->end, src->subspan);
  return span;
}

static void
sraSpanInsertAfter(sraSpan *newspan, sraSpan *after) {
  if(newspan && after) {
    newspan->_next = after->_next;
    newspan->_prev = after;
    after->_next->_prev = newspan;
    after->_next = newspan;
  }
}

static void
sraSpanInsertBefore(sraSpan *newspan, sraSpan *before) {
  if(newspan && before) {
    newspan->_next = before;
    newspan->_prev = before->_prev;
    before->_prev->_next = newspan;
    before->_prev = newspan;
  }
}

static void
sraSpanRemove(sraSpan *span) {
  if(span) {
    span->_prev->_next = span->_next;
    span->_next->_prev = span->_prev;
  }
}

static void
sraSpanDestroy(sraSpan *span) {
  if (span->subspan) sraSpanListDestroy(span->subspan);
  free(span);
}

#ifdef DEBUG
static void
sraSpanCheck(const sraSpan *span, const char *text) {
  /* Check the span is valid! */
  if (span->start == span->end) {
    printf(text); 
    printf(":%d-%d\n", span->start, span->end);
  }
}
#endif

/* -=- SpanList routines */

static void sraSpanPrint(const sraSpan *s);

static void
sraSpanListPrint(const sraSpanList *l) {
  sraSpan *curr;
  if (!l) {
	  printf("NULL");
	  return;
  }
  curr = l->front._next;
  printf("[");
  while (curr != &(l->back)) {
    sraSpanPrint(curr);
    curr = curr->_next;
  }
  printf("]");
}

void
sraSpanPrint(const sraSpan *s) {
  printf("(%d-%d)", (s->start), (s->end));
  if (s->subspan)
    sraSpanListPrint(s->subspan);
}

static sraSpanList *
sraSpanListCreate(void) {
  sraSpanList *item = (sraSpanList*)malloc(sizeof(sraSpanList));
  if (!item) return NULL;
  item->front._next = &(item->back);
  item->front._prev = NULL;
  item->back._prev = &(item->front);
  item->back._next = NULL;
  return item;
}

sraSpanList *
sraSpanListDup(const sraSpanList *src) {
  sraSpanList *newlist;
  sraSpan *newspan, *curr;

  if (!src) return NULL;
  newlist = sraSpanListCreate();
  curr = src->front._next;
  while (curr != &(src->back)) {
    newspan = sraSpanDup(curr);
    sraSpanInsertBefore(newspan, &(newlist->back));
    curr = curr->_next;
  }

  return newlist;
}

void
sraSpanListDestroy(sraSpanList *list) {
  sraSpan *curr;
  while (list->front._next != &(list->back)) {
    curr = list->front._next;
    sraSpanRemove(curr);
    sraSpanDestroy(curr);
  }
  free(list);
}

static void
sraSpanListMakeEmpty(sraSpanList *list) {
  sraSpan *curr;
  while (list->front._next != &(list->back)) {
    curr = list->front._next;
    sraSpanRemove(curr);
    sraSpanDestroy(curr);
  }
  list->front._next = &(list->back);
  list->front._prev = NULL;
  list->back._prev = &(list->front);
  list->back._next = NULL;
}

static rfbBool
sraSpanListEqual(const sraSpanList *s1, const sraSpanList *s2) {
  sraSpan *sp1, *sp2;

  if (!s1) {
    if (!s2) {
      return 1;
    } else {
      rfbErr("sraSpanListEqual:incompatible spans (only one NULL!)\n");
      return FALSE;
    }
  }

  sp1 = s1->front._next;
  sp2 = s2->front._next;
  while ((sp1 != &(s1->back)) &&
	 (sp2 != &(s2->back))) {
    if ((sp1->start != sp2->start) ||
	(sp1->end != sp2->end) ||
	(!sraSpanListEqual(sp1->subspan, sp2->subspan))) {
      return 0;
    }
    sp1 = sp1->_next;
    sp2 = sp2->_next;
  }

  if ((sp1 == &(s1->back)) && (sp2 == &(s2->back))) {
    return 1;
  } else {
    return 0;
  }    
}

static rfbBool
sraSpanListEmpty(const sraSpanList *list) {
  return (list->front._next == &(list->back));
}

static unsigned long
sraSpanListCount(const sraSpanList *list) {
  sraSpan *curr = list->front._next;
  unsigned long count = 0;
  while (curr != &(list->back)) {
    if (curr->subspan) {
      count += sraSpanListCount(curr->subspan);
    } else {
      count += 1;
    }
    curr = curr->_next;
  }
  return count;
}

static void
sraSpanMergePrevious(sraSpan *dest) {
  sraSpan *prev = dest->_prev;
 
  while ((prev->_prev) &&
	 (prev->end == dest->start) &&
	 (sraSpanListEqual(prev->subspan, dest->subspan))) {
    /*
    printf("merge_prev:");
    sraSpanPrint(prev);
    printf(" & ");
    sraSpanPrint(dest);
    printf("\n");
    */
    dest->start = prev->start;
    sraSpanRemove(prev);
    sraSpanDestroy(prev);
    prev = dest->_prev;
  }
}    

static void
sraSpanMergeNext(sraSpan *dest) {
  sraSpan *next = dest->_next;
  while ((next->_next) &&
	 (next->start == dest->end) &&
	 (sraSpanListEqual(next->subspan, dest->subspan))) {
/*
	  printf("merge_next:");
    sraSpanPrint(dest);
    printf(" & ");
    sraSpanPrint(next);
    printf("\n");
	*/
    dest->end = next->end;
    sraSpanRemove(next);
    sraSpanDestroy(next);
    next = dest->_next;
  }
}

static void
sraSpanListOr(sraSpanList *dest, const sraSpanList *src) {
  sraSpan *d_curr, *s_curr;
  int s_start, s_end;

  if (!dest) {
    if (!src) {
      return;
    } else {
      rfbErr("sraSpanListOr:incompatible spans (only one NULL!)\n");
      return;
    }
  }

  d_curr = dest->front._next;
  s_curr = src->front._next;
  s_start = s_curr->start;
  s_end = s_curr->end;
  while (s_curr != &(src->back)) {

    /* - If we are at end of destination list OR
       If the new span comes before the next destination one */
    if ((d_curr == &(dest->back)) ||
		(d_curr->start >= s_end)) {
      /* - Add the span */
      sraSpanInsertBefore(sraSpanCreate(s_start, s_end,
					s_curr->subspan),
			  d_curr);
      if (d_curr != &(dest->back))
	sraSpanMergePrevious(d_curr);
      s_curr = s_curr->_next;
      s_start = s_curr->start;
      s_end = s_curr->end;
    } else {

      /* - If the new span overlaps the existing one */
      if ((s_start < d_curr->end) &&
	  (s_end > d_curr->start)) {

	/* - Insert new span before the existing destination one? */
	if (s_start < d_curr->start) {
	  sraSpanInsertBefore(sraSpanCreate(s_start,
					    d_curr->start,
					    s_curr->subspan),
			      d_curr);
	  sraSpanMergePrevious(d_curr);
	}

	/* Split the existing span if necessary */
	if (s_end < d_curr->end) {
	  sraSpanInsertAfter(sraSpanCreate(s_end,
					   d_curr->end,
					   d_curr->subspan),
			     d_curr);
	  d_curr->end = s_end;
	}
	if (s_start > d_curr->start) {
	  sraSpanInsertBefore(sraSpanCreate(d_curr->start,
					    s_start,
					    d_curr->subspan),
			      d_curr);
	  d_curr->start = s_start;
	}

	/* Recursively OR subspans */
	sraSpanListOr(d_curr->subspan, s_curr->subspan);

	/* Merge this span with previous or next? */
	if (d_curr->_prev != &(dest->front))
	  sraSpanMergePrevious(d_curr);
	if (d_curr->_next != &(dest->back))
	  sraSpanMergeNext(d_curr);

	/* Move onto the next pair to compare */
	if (s_end > d_curr->end) {
	  s_start = d_curr->end;
	  d_curr = d_curr->_next;
	} else {
	  s_curr = s_curr->_next;
	  s_start = s_curr->start;
	  s_end = s_curr->end;
	}
      } else {
	/* - No overlap.  Move to the next destination span */
	d_curr = d_curr->_next;
      }
    }
  }
}

static rfbBool
sraSpanListAnd(sraSpanList *dest, const sraSpanList *src) {
  sraSpan *d_curr, *s_curr, *d_next;

  if (!dest) {
    if (!src) {
      return 1;
    } else {
      rfbErr("sraSpanListAnd:incompatible spans (only one NULL!)\n");
      return FALSE;
    }
  }

  d_curr = dest->front._next;
  s_curr = src->front._next;
  while ((s_curr != &(src->back)) && (d_curr != &(dest->back))) {

    /* - If we haven't reached a destination span yet then move on */
    if (d_curr->start >= s_curr->end) {
      s_curr = s_curr->_next;
      continue;
    }

    /* - If we are beyond the current destination span then remove it */
    if (d_curr->end <= s_curr->start) {
      sraSpan *next = d_curr->_next;
      sraSpanRemove(d_curr);
      sraSpanDestroy(d_curr);
      d_curr = next;
      continue;
    }

    /* - If we partially overlap a span then split it up or remove bits */
    if (s_curr->start > d_curr->start) {
      /* - The top bit of the span does not match */
      d_curr->start = s_curr->start;
    }
    if (s_curr->end < d_curr->end) {
      /* - The end of the span does not match */
      sraSpanInsertAfter(sraSpanCreate(s_curr->end,
				       d_curr->end,
				       d_curr->subspan),
			 d_curr);
      d_curr->end = s_curr->end;
    }

    /* - Now recursively process the affected span */
    if (!sraSpanListAnd(d_curr->subspan, s_curr->subspan)) {
      /* - The destination subspan is now empty, so we should remove it */
		sraSpan *next = d_curr->_next;
      sraSpanRemove(d_curr);
      sraSpanDestroy(d_curr);
      d_curr = next;
    } else {
      /* Merge this span with previous or next? */
      if (d_curr->_prev != &(dest->front))
	sraSpanMergePrevious(d_curr);

      /* - Move on to the next span */
      d_next = d_curr;
      if (s_curr->end >= d_curr->end) {
	d_next = d_curr->_next;
      }
      if (s_curr->end <= d_curr->end) {
	s_curr = s_curr->_next;
      }
      d_curr = d_next;
    }
  }

  while (d_curr != &(dest->back)) {
    sraSpan *next = d_curr->_next;
    sraSpanRemove(d_curr);
    sraSpanDestroy(d_curr);
    d_curr=next;
  }

  return !sraSpanListEmpty(dest);
}

static rfbBool
sraSpanListSubtract(sraSpanList *dest, const sraSpanList *src) {
  sraSpan *d_curr, *s_curr;

  if (!dest) {
    if (!src) {
      return 1;
    } else {
      rfbErr("sraSpanListSubtract:incompatible spans (only one NULL!)\n");
      return FALSE;
    }
  }

  d_curr = dest->front._next;
  s_curr = src->front._next;
  while ((s_curr != &(src->back)) && (d_curr != &(dest->back))) {

    /* - If we haven't reached a destination span yet then move on */
    if (d_curr->start >= s_curr->end) {
      s_curr = s_curr->_next;
      continue;
    }

    /* - If we are beyond the current destination span then skip it */
    if (d_curr->end <= s_curr->start) {
      d_curr = d_curr->_next;
      continue;
    }

    /* - If we partially overlap the current span then split it up */
    if (s_curr->start > d_curr->start) {
      sraSpanInsertBefore(sraSpanCreate(d_curr->start,
					s_curr->start,
					d_curr->subspan),
			  d_curr);
      d_curr->start = s_curr->start;
    }
    if (s_curr->end < d_curr->end) {
      sraSpanInsertAfter(sraSpanCreate(s_curr->end,
				       d_curr->end,
				       d_curr->subspan),
			 d_curr);
      d_curr->end = s_curr->end;
    }

    /* - Now recursively process the affected span */
    if ((!d_curr->subspan) || !sraSpanListSubtract(d_curr->subspan, s_curr->subspan)) {
      /* - The destination subspan is now empty, so we should remove it */
      sraSpan *next = d_curr->_next;
      sraSpanRemove(d_curr);
      sraSpanDestroy(d_curr);
      d_curr = next;
    } else {
      /* Merge this span with previous or next? */
      if (d_curr->_prev != &(dest->front))
	sraSpanMergePrevious(d_curr);
      if (d_curr->_next != &(dest->back))
	sraSpanMergeNext(d_curr);

      /* - Move on to the next span */
      if (s_curr->end > d_curr->end) {
	d_curr = d_curr->_next;
      } else {
	s_curr = s_curr->_next;
      }
    }
  }

  return !sraSpanListEmpty(dest);
}

/* -=- Region routines */

sraRegion *
sraRgnCreate(void) {
  return (sraRegion*)sraSpanListCreate();
}

sraRegion *
sraRgnCreateRect(int x1, int y1, int x2, int y2) {
  sraSpanList *vlist, *hlist;
  sraSpan *vspan, *hspan;

  /* - Build the horizontal portion of the span */
  hlist = sraSpanListCreate();
  hspan = sraSpanCreate(x1, x2, NULL);
  sraSpanInsertAfter(hspan, &(hlist->front));

  /* - Build the vertical portion of the span */
  vlist = sraSpanListCreate();
  vspan = sraSpanCreate(y1, y2, hlist);
  sraSpanInsertAfter(vspan, &(vlist->front));

  sraSpanListDestroy(hlist);

  return (sraRegion*)vlist;
}

sraRegion *
sraRgnCreateRgn(const sraRegion *src) {
  return (sraRegion*)sraSpanListDup((sraSpanList*)src);
}

void
sraRgnDestroy(sraRegion *rgn) {
  sraSpanListDestroy((sraSpanList*)rgn);
}

void
sraRgnMakeEmpty(sraRegion *rgn) {
  sraSpanListMakeEmpty((sraSpanList*)rgn);
}

/* -=- Boolean Region ops */

rfbBool
sraRgnAnd(sraRegion *dst, const sraRegion *src) {
  return sraSpanListAnd((sraSpanList*)dst, (sraSpanList*)src);
}

void
sraRgnOr(sraRegion *dst, const sraRegion *src) {
  sraSpanListOr((sraSpanList*)dst, (sraSpanList*)src);
}

rfbBool
sraRgnSubtract(sraRegion *dst, const sraRegion *src) {
  return sraSpanListSubtract((sraSpanList*)dst, (sraSpanList*)src);
}

void
sraRgnOffset(sraRegion *dst, int dx, int dy) {
  sraSpan *vcurr, *hcurr;

  vcurr = ((sraSpanList*)dst)->front._next;
  while (vcurr != &(((sraSpanList*)dst)->back)) {
    vcurr->start += dy;
    vcurr->end += dy;
    
    hcurr = vcurr->subspan->front._next;
    while (hcurr != &(vcurr->subspan->back)) {
      hcurr->start += dx;
      hcurr->end += dx;
      hcurr = hcurr->_next;
    }

    vcurr = vcurr->_next;
  }
}

sraRegion *sraRgnBBox(const sraRegion *src) {
  int xmin=((unsigned int)(int)-1)>>1,ymin=xmin,xmax=1-xmin,ymax=xmax;
  sraSpan *vcurr, *hcurr;

  if(!src)
    return sraRgnCreate();

  vcurr = ((sraSpanList*)src)->front._next;
  while (vcurr != &(((sraSpanList*)src)->back)) {
    if(vcurr->start<ymin)
      ymin=vcurr->start;
    if(vcurr->end>ymax)
      ymax=vcurr->end;
    
    hcurr = vcurr->subspan->front._next;
    while (hcurr != &(vcurr->subspan->back)) {
      if(hcurr->start<xmin)
	xmin=hcurr->start;
      if(hcurr->end>xmax)
	xmax=hcurr->end;
      hcurr = hcurr->_next;
    }

    vcurr = vcurr->_next;
  }

  if(xmax<xmin || ymax<ymin)
    return sraRgnCreate();

  return sraRgnCreateRect(xmin,ymin,xmax,ymax);
}

rfbBool
sraRgnPopRect(sraRegion *rgn, sraRect *rect, unsigned long flags) {
  sraSpan *vcurr, *hcurr;
  sraSpan *vend, *hend;
  rfbBool right2left = (flags & 2) == 2;
  rfbBool bottom2top = (flags & 1) == 1;

  /* - Pick correct order */
  if (bottom2top) {
    vcurr = ((sraSpanList*)rgn)->back._prev;
    vend = &(((sraSpanList*)rgn)->front);
  } else {
    vcurr = ((sraSpanList*)rgn)->front._next;
    vend = &(((sraSpanList*)rgn)->back);
  }

  if (vcurr != vend) {
    rect->y1 = vcurr->start;
    rect->y2 = vcurr->end;

    /* - Pick correct order */
    if (right2left) {
      hcurr = vcurr->subspan->back._prev;
      hend = &(vcurr->subspan->front);
    } else {
      hcurr = vcurr->subspan->front._next;
      hend = &(vcurr->subspan->back);
    }

    if (hcurr != hend) {
      rect->x1 = hcurr->start;
      rect->x2 = hcurr->end;

      sraSpanRemove(hcurr);
      sraSpanDestroy(hcurr);
      
      if (sraSpanListEmpty(vcurr->subspan)) {
	sraSpanRemove(vcurr);
	sraSpanDestroy(vcurr);
      }

#if 0
      printf("poprect:(%dx%d)-(%dx%d)\n",
	     rect->x1, rect->y1, rect->x2, rect->y2);
#endif
      return 1;
    }
  }

  return 0;
}

unsigned long
sraRgnCountRects(const sraRegion *rgn) {
  unsigned long count = sraSpanListCount((sraSpanList*)rgn);
  return count;
}

rfbBool
sraRgnEmpty(const sraRegion *rgn) {
  return sraSpanListEmpty((sraSpanList*)rgn);
}

/* iterator stuff */
sraRectangleIterator *sraRgnGetIterator(sraRegion *s)
{
  /* these values have to be multiples of 4 */
#define DEFSIZE 4
#define DEFSTEP 8
  sraRectangleIterator *i =
    (sraRectangleIterator*)malloc(sizeof(sraRectangleIterator));
  if(!i)
    return NULL;

  /* we have to recurse eventually. So, the first sPtr is the pointer to
     the sraSpan in the first level. the second sPtr is the pointer to
     the sraRegion.back. The third and fourth sPtr are for the second
     recursion level and so on. */
  i->sPtrs = (sraSpan**)malloc(sizeof(sraSpan*)*DEFSIZE);
  if(!i->sPtrs) {
    free(i);
    return NULL;
  }
  i->ptrSize = DEFSIZE;
  i->sPtrs[0] = &(s->front);
  i->sPtrs[1] = &(s->back);
  i->ptrPos = 0;
  i->reverseX = 0;
  i->reverseY = 0;
  return i;
}

sraRectangleIterator *sraRgnGetReverseIterator(sraRegion *s,rfbBool reverseX,rfbBool reverseY)
{
  sraRectangleIterator *i = sraRgnGetIterator(s);
  if(reverseY) {
    i->sPtrs[1] = &(s->front);
    i->sPtrs[0] = &(s->back);
  }
  i->reverseX = reverseX;
  i->reverseY = reverseY;
  return(i);
}

static rfbBool sraReverse(sraRectangleIterator *i)
{
  return( ((i->ptrPos&2) && i->reverseX) ||
     (!(i->ptrPos&2) && i->reverseY));
}

static sraSpan* sraNextSpan(sraRectangleIterator *i)
{
  if(sraReverse(i))
    return(i->sPtrs[i->ptrPos]->_prev);
  else
    return(i->sPtrs[i->ptrPos]->_next);
}

rfbBool sraRgnIteratorNext(sraRectangleIterator* i,sraRect* r)
{
  /* is the subspan finished? */
  while(sraNextSpan(i) == i->sPtrs[i->ptrPos+1]) {
    i->ptrPos -= 2;
    if(i->ptrPos < 0) /* the end */
      return(0);
  }

  i->sPtrs[i->ptrPos] = sraNextSpan(i);

  /* is this a new subspan? */
  while(i->sPtrs[i->ptrPos]->subspan) {
    if(i->ptrPos+2 > i->ptrSize) { /* array is too small */
      i->ptrSize += DEFSTEP;
      i->sPtrs = (sraSpan**)realloc(i->sPtrs, sizeof(sraSpan*)*i->ptrSize);
    }
    i->ptrPos += 2;
    if(sraReverse(i)) {
      i->sPtrs[i->ptrPos]   =   i->sPtrs[i->ptrPos-2]->subspan->back._prev;
      i->sPtrs[i->ptrPos+1] = &(i->sPtrs[i->ptrPos-2]->subspan->front);
    } else {
      i->sPtrs[i->ptrPos]   =   i->sPtrs[i->ptrPos-2]->subspan->front._next;
      i->sPtrs[i->ptrPos+1] = &(i->sPtrs[i->ptrPos-2]->subspan->back);
    }
  }

  if((i->ptrPos%4)!=2) {
    rfbErr("sraRgnIteratorNext: offset is wrong (%d%%4!=2)\n",i->ptrPos);
    return FALSE;
  }

  r->y1 = i->sPtrs[i->ptrPos-2]->start;
  r->y2 = i->sPtrs[i->ptrPos-2]->end;
  r->x1 = i->sPtrs[i->ptrPos]->start;
  r->x2 = i->sPtrs[i->ptrPos]->end;

  return(-1);
}

void sraRgnReleaseIterator(sraRectangleIterator* i)
{
  free(i->sPtrs);
  free(i);
}

void
sraRgnPrint(const sraRegion *rgn) {
	sraSpanListPrint((sraSpanList*)rgn);
}

rfbBool
sraClipRect(int *x, int *y, int *w, int *h,
	    int cx, int cy, int cw, int ch) {
  if (*x < cx) {
    *w -= (cx-*x);
    *x = cx;
  }
  if (*y < cy) {
    *h -= (cy-*y);
    *y = cy;
  }
  if (*x+*w > cx+cw) {
    *w = (cx+cw)-*x;
  }
  if (*y+*h > cy+ch) {
    *h = (cy+ch)-*y;
  }
  return (*w>0) && (*h>0);
}

rfbBool
sraClipRect2(int *x, int *y, int *x2, int *y2,
	    int cx, int cy, int cx2, int cy2) {
  if (*x < cx)
    *x = cx;
  if (*y < cy)
    *y = cy;
  if (*x >= cx2)
    *x = cx2-1;
  if (*y >= cy2)
    *y = cy2-1;
  if (*x2 <= cx)
    *x2 = cx+1;
  if (*y2 <= cy)
    *y2 = cy+1;
  if (*x2 > cx2)
    *x2 = cx2;
  if (*y2 > cy2)
    *y2 = cy2;
  return (*x2>*x) && (*y2>*y);
}

/* test */

#ifdef SRA_TEST
/* pipe the output to sort|uniq -u and you'll get the errors. */
int main(int argc, char** argv)
{
  sraRegionPtr region, region1, region2;
  sraRectangleIterator* i;
  sraRect rect;
  rfbBool b;

  region = sraRgnCreateRect(10, 10, 600, 300);
  region1 = sraRgnCreateRect(40, 50, 350, 200);
  region2 = sraRgnCreateRect(0, 0, 20, 40);

  sraRgnPrint(region);
  printf("\n[(10-300)[(10-600)]]\n\n");

  b = sraRgnSubtract(region, region1);
  printf("%s ",b?"true":"false");
  sraRgnPrint(region);
  printf("\ntrue [(10-50)[(10-600)](50-200)[(10-40)(350-600)](200-300)[(10-600)]]\n\n");

  sraRgnOr(region, region2);
  printf("%ld\n6\n\n", sraRgnCountRects(region));

  i = sraRgnGetIterator(region);
  while(sraRgnIteratorNext(i, &rect))
    printf("%dx%d+%d+%d ",
	   rect.x2-rect.x1,rect.y2-rect.y1,
	   rect.x1,rect.y1);
  sraRgnReleaseIterator(i);
  printf("\n20x10+0+0 600x30+0+10 590x10+10+40 30x150+10+50 250x150+350+50 590x100+10+200 \n\n");

  i = sraRgnGetReverseIterator(region,1,0);
  while(sraRgnIteratorNext(i, &rect))
    printf("%dx%d+%d+%d ",
	   rect.x2-rect.x1,rect.y2-rect.y1,
	   rect.x1,rect.y1);
  sraRgnReleaseIterator(i);
  printf("\n20x10+0+0 600x30+0+10 590x10+10+40 250x150+350+50 30x150+10+50 590x100+10+200 \n\n");

  i = sraRgnGetReverseIterator(region,1,1);
  while(sraRgnIteratorNext(i, &rect))
    printf("%dx%d+%d+%d ",
	   rect.x2-rect.x1,rect.y2-rect.y1,
	   rect.x1,rect.y1);
  sraRgnReleaseIterator(i);
  printf("\n590x100+10+200 250x150+350+50 30x150+10+50 590x10+10+40 600x30+0+10 20x10+0+0 \n\n");

  sraRgnDestroy(region);
  sraRgnDestroy(region1);
  sraRgnDestroy(region2);

  return(0);
}
#endif
