/*
 * stats.c
 */

/*
 *  Copyright (C) 2002 RealVNC Ltd.
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <rfb/rfb.h>

char *messageNameServer2Client(uint32_t type, char *buf, int len);
char *messageNameClient2Server(uint32_t type, char *buf, int len);
char *encodingName(uint32_t enc, char *buf, int len);

rfbStatList *rfbStatLookupEncoding(rfbClientPtr cl, uint32_t type);
rfbStatList *rfbStatLookupMessage(rfbClientPtr cl, uint32_t type);

void  rfbStatRecordEncodingSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
void  rfbStatRecordEncodingRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
void  rfbStatRecordMessageSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
void  rfbStatRecordMessageRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw);
void rfbResetStats(rfbClientPtr cl);
void rfbPrintStats(rfbClientPtr cl);




char *messageNameServer2Client(uint32_t type, char *buf, int len) {
    if (buf==NULL) return "error";
    switch (type) {
    case rfbFramebufferUpdate:        snprintf(buf, len, "FramebufferUpdate"); break;
    case rfbSetColourMapEntries:      snprintf(buf, len, "SetColourMapEntries"); break;
    case rfbBell:                     snprintf(buf, len, "Bell"); break;
    case rfbServerCutText:            snprintf(buf, len, "ServerCutText"); break;
    case rfbResizeFrameBuffer:        snprintf(buf, len, "ResizeFrameBuffer"); break;
    case rfbFileTransfer:             snprintf(buf, len, "FileTransfer"); break;
    case rfbTextChat:                 snprintf(buf, len, "TextChat"); break;
    case rfbPalmVNCReSizeFrameBuffer: snprintf(buf, len, "PalmVNCReSize"); break;
    case rfbXvp:                      snprintf(buf, len, "XvpServerMessage"); break;
    default:
        snprintf(buf, len, "svr2cli-0x%08X", 0xFF);
    }
    return buf;
}

char *messageNameClient2Server(uint32_t type, char *buf, int len) {
    if (buf==NULL) return "error";
    switch (type) {
    case rfbSetPixelFormat:           snprintf(buf, len, "SetPixelFormat"); break;
    case rfbFixColourMapEntries:      snprintf(buf, len, "FixColourMapEntries"); break;
    case rfbSetEncodings:             snprintf(buf, len, "SetEncodings"); break;
    case rfbFramebufferUpdateRequest: snprintf(buf, len, "FramebufferUpdate"); break;
    case rfbKeyEvent:                 snprintf(buf, len, "KeyEvent"); break;
    case rfbPointerEvent:             snprintf(buf, len, "PointerEvent"); break;
    case rfbClientCutText:            snprintf(buf, len, "ClientCutText"); break;
    case rfbFileTransfer:             snprintf(buf, len, "FileTransfer"); break;
    case rfbSetScale:                 snprintf(buf, len, "SetScale"); break;
    case rfbSetServerInput:           snprintf(buf, len, "SetServerInput"); break;
    case rfbSetSW:                    snprintf(buf, len, "SetSingleWindow"); break;
    case rfbTextChat:                 snprintf(buf, len, "TextChat"); break;
    case rfbPalmVNCSetScaleFactor:    snprintf(buf, len, "PalmVNCSetScale"); break;
    case rfbXvp:                      snprintf(buf, len, "XvpClientMessage"); break;
    case rfbSetDesktopSize:           snprintf(buf, len, "SetDesktopSize"); break;
    default:
        snprintf(buf, len, "cli2svr-0x%08X", type);


    }
    return buf;
}

/* Encoding name must be <=16 characters to fit nicely on the status output in
 * an 80 column terminal window
 */
char *encodingName(uint32_t type, char *buf, int len) {
    if (buf==NULL) return "error";
    
    switch (type) {
    case rfbEncodingRaw:                snprintf(buf, len, "raw");         break;
    case rfbEncodingCopyRect:           snprintf(buf, len, "copyRect");    break;
    case rfbEncodingRRE:                snprintf(buf, len, "RRE");         break;
    case rfbEncodingCoRRE:              snprintf(buf, len, "CoRRE");       break;
    case rfbEncodingHextile:            snprintf(buf, len, "hextile");     break;
    case rfbEncodingZlib:               snprintf(buf, len, "zlib");        break;
    case rfbEncodingTight:              snprintf(buf, len, "tight");       break;
    case rfbEncodingTightPng:           snprintf(buf, len, "tightPng");    break;
    case rfbEncodingZlibHex:            snprintf(buf, len, "zlibhex");     break;
    case rfbEncodingUltra:              snprintf(buf, len, "ultra");       break;
    case rfbEncodingZRLE:               snprintf(buf, len, "ZRLE");        break;
    case rfbEncodingZYWRLE:             snprintf(buf, len, "ZYWRLE");      break;
    case rfbEncodingCache:              snprintf(buf, len, "cache");       break;
    case rfbEncodingCacheEnable:        snprintf(buf, len, "cacheEnable"); break;
    case rfbEncodingXOR_Zlib:           snprintf(buf, len, "xorZlib");     break;
    case rfbEncodingXORMonoColor_Zlib:  snprintf(buf, len, "xorMonoZlib");  break;
    case rfbEncodingXORMultiColor_Zlib: snprintf(buf, len, "xorColorZlib"); break;
    case rfbEncodingSolidColor:         snprintf(buf, len, "solidColor");  break;
    case rfbEncodingXOREnable:          snprintf(buf, len, "xorEnable");   break;
    case rfbEncodingCacheZip:           snprintf(buf, len, "cacheZip");    break;
    case rfbEncodingSolMonoZip:         snprintf(buf, len, "monoZip");     break;
    case rfbEncodingUltraZip:           snprintf(buf, len, "ultraZip");    break;

    case rfbEncodingXCursor:            snprintf(buf, len, "Xcursor");     break;
    case rfbEncodingRichCursor:         snprintf(buf, len, "RichCursor");  break;
    case rfbEncodingPointerPos:         snprintf(buf, len, "PointerPos");  break;

    case rfbEncodingLastRect:           snprintf(buf, len, "LastRect");    break;
    case rfbEncodingNewFBSize:          snprintf(buf, len, "NewFBSize");   break;
    case rfbEncodingExtDesktopSize:     snprintf(buf, len, "ExtendedDesktopSize"); break;
    case rfbEncodingKeyboardLedState:   snprintf(buf, len, "LedState");    break;
    case rfbEncodingSupportedMessages:  snprintf(buf, len, "SupportedMessage");  break;
    case rfbEncodingSupportedEncodings: snprintf(buf, len, "SupportedEncoding"); break;
    case rfbEncodingServerIdentity:     snprintf(buf, len, "ServerIdentify");    break;

    /* The following lookups do not report in stats */
    case rfbEncodingCompressLevel0: snprintf(buf, len, "CompressLevel0");  break;
    case rfbEncodingCompressLevel1: snprintf(buf, len, "CompressLevel1");  break;
    case rfbEncodingCompressLevel2: snprintf(buf, len, "CompressLevel2");  break;
    case rfbEncodingCompressLevel3: snprintf(buf, len, "CompressLevel3");  break;
    case rfbEncodingCompressLevel4: snprintf(buf, len, "CompressLevel4");  break;
    case rfbEncodingCompressLevel5: snprintf(buf, len, "CompressLevel5");  break;
    case rfbEncodingCompressLevel6: snprintf(buf, len, "CompressLevel6");  break;
    case rfbEncodingCompressLevel7: snprintf(buf, len, "CompressLevel7");  break;
    case rfbEncodingCompressLevel8: snprintf(buf, len, "CompressLevel8");  break;
    case rfbEncodingCompressLevel9: snprintf(buf, len, "CompressLevel9");  break;
    
    case rfbEncodingQualityLevel0:  snprintf(buf, len, "QualityLevel0");   break;
    case rfbEncodingQualityLevel1:  snprintf(buf, len, "QualityLevel1");   break;
    case rfbEncodingQualityLevel2:  snprintf(buf, len, "QualityLevel2");   break;
    case rfbEncodingQualityLevel3:  snprintf(buf, len, "QualityLevel3");   break;
    case rfbEncodingQualityLevel4:  snprintf(buf, len, "QualityLevel4");   break;
    case rfbEncodingQualityLevel5:  snprintf(buf, len, "QualityLevel5");   break;
    case rfbEncodingQualityLevel6:  snprintf(buf, len, "QualityLevel6");   break;
    case rfbEncodingQualityLevel7:  snprintf(buf, len, "QualityLevel7");   break;
    case rfbEncodingQualityLevel8:  snprintf(buf, len, "QualityLevel8");   break;
    case rfbEncodingQualityLevel9:  snprintf(buf, len, "QualityLevel9");   break;


    default:
        snprintf(buf, len, "Enc(0x%08X)", type);
    }

    return buf;
}





rfbStatList *rfbStatLookupEncoding(rfbClientPtr cl, uint32_t type)
{
    rfbStatList *ptr;
    if (cl==NULL) return NULL;
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
    {
        if (ptr->type==type) return ptr;
    }
    /* Well, we are here... need to *CREATE* an entry */
    ptr = (rfbStatList *)malloc(sizeof(rfbStatList));
    if (ptr!=NULL)
    {
        memset((char *)ptr, 0, sizeof(rfbStatList));
        ptr->type = type;
        /* add to the top of the list */
        ptr->Next = cl->statEncList;
        cl->statEncList = ptr;
    }
    return ptr;
}


rfbStatList *rfbStatLookupMessage(rfbClientPtr cl, uint32_t type)
{
    rfbStatList *ptr;
    if (cl==NULL) return NULL;
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
    {
        if (ptr->type==type) return ptr;
    }
    /* Well, we are here... need to *CREATE* an entry */
    ptr = (rfbStatList *)malloc(sizeof(rfbStatList));
    if (ptr!=NULL)
    {
        memset((char *)ptr, 0, sizeof(rfbStatList));
        ptr->type = type;
        /* add to the top of the list */
        ptr->Next = cl->statMsgList;
        cl->statMsgList = ptr;
    }
    return ptr;
}

void rfbStatRecordEncodingSentAdd(rfbClientPtr cl, uint32_t type, int byteCount) /* Specifically for tight encoding */
{
    rfbStatList *ptr;

    ptr = rfbStatLookupEncoding(cl, type);
    if (ptr!=NULL)
        ptr->bytesSent      += byteCount;
}


void  rfbStatRecordEncodingSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw)
{
    rfbStatList *ptr;

    ptr = rfbStatLookupEncoding(cl, type);
    if (ptr!=NULL)
    {
        ptr->sentCount++;
        ptr->bytesSent      += byteCount;
        ptr->bytesSentIfRaw += byteIfRaw;
    }
}

void  rfbStatRecordEncodingRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw)
{
    rfbStatList *ptr;

    ptr = rfbStatLookupEncoding(cl, type);
    if (ptr!=NULL)
    {
        ptr->rcvdCount++;
        ptr->bytesRcvd      += byteCount;
        ptr->bytesRcvdIfRaw += byteIfRaw;
    }
}

void  rfbStatRecordMessageSent(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw)
{
    rfbStatList *ptr;

    ptr = rfbStatLookupMessage(cl, type);
    if (ptr!=NULL)
    {
        ptr->sentCount++;
        ptr->bytesSent      += byteCount;
        ptr->bytesSentIfRaw += byteIfRaw;
    }
}

void  rfbStatRecordMessageRcvd(rfbClientPtr cl, uint32_t type, int byteCount, int byteIfRaw)
{
    rfbStatList *ptr;

    ptr = rfbStatLookupMessage(cl, type);
    if (ptr!=NULL)
    {
        ptr->rcvdCount++;
        ptr->bytesRcvd      += byteCount;
        ptr->bytesRcvdIfRaw += byteIfRaw;
    }
}


int rfbStatGetSentBytes(rfbClientPtr cl)
{
    rfbStatList *ptr=NULL;
    int bytes=0;
    if (cl==NULL) return 0;
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesSent;
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesSent;
    return bytes;
}

int rfbStatGetSentBytesIfRaw(rfbClientPtr cl)
{
    rfbStatList *ptr=NULL;
    int bytes=0;
    if (cl==NULL) return 0;
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesSentIfRaw;
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesSentIfRaw;
    return bytes;
}

int rfbStatGetRcvdBytes(rfbClientPtr cl)
{
    rfbStatList *ptr=NULL;
    int bytes=0;
    if (cl==NULL) return 0;
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesRcvd;
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesRcvd;
    return bytes;
}

int rfbStatGetRcvdBytesIfRaw(rfbClientPtr cl)
{
    rfbStatList *ptr=NULL;
    int bytes=0;
    if (cl==NULL) return 0;
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesRcvdIfRaw;
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
        bytes += ptr->bytesRcvdIfRaw;
    return bytes;
}

int rfbStatGetMessageCountSent(rfbClientPtr cl, uint32_t type)
{
  rfbStatList *ptr=NULL;
    if (cl==NULL) return 0;
  for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
      if (ptr->type==type) return ptr->sentCount;
  return 0;
}
int rfbStatGetMessageCountRcvd(rfbClientPtr cl, uint32_t type)
{
  rfbStatList *ptr=NULL;
    if (cl==NULL) return 0;
  for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
      if (ptr->type==type) return ptr->rcvdCount;
  return 0;
}

int rfbStatGetEncodingCountSent(rfbClientPtr cl, uint32_t type)
{
  rfbStatList *ptr=NULL;
    if (cl==NULL) return 0;
  for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
      if (ptr->type==type) return ptr->sentCount;
  return 0;
}
int rfbStatGetEncodingCountRcvd(rfbClientPtr cl, uint32_t type)
{
  rfbStatList *ptr=NULL;
    if (cl==NULL) return 0;
  for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
      if (ptr->type==type) return ptr->rcvdCount;
  return 0;
}




void rfbResetStats(rfbClientPtr cl)
{
    rfbStatList *ptr;
    if (cl==NULL) return;
    while (cl->statEncList!=NULL)
    {
        ptr = cl->statEncList;
        cl->statEncList = ptr->Next;
        free(ptr);
    }
    while (cl->statMsgList!=NULL)
    {
        ptr = cl->statMsgList;
        cl->statMsgList = ptr->Next;
        free(ptr);
    }
}


void rfbPrintStats(rfbClientPtr cl)
{
    rfbStatList *ptr=NULL;
    char encBuf[64];
    double savings=0.0;
    int    totalRects=0;
    double totalBytes=0.0;
    double totalBytesIfRaw=0.0;

    char *name=NULL;
    int bytes=0;
    int bytesIfRaw=0;
    int count=0;

    if (cl==NULL) return;
    
    rfbLog("%-21.21s  %-6.6s   %9.9s/%9.9s (%6.6s)\n", "Statistics", "events", "Transmit","RawEquiv","saved");
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
    {
        name       = messageNameServer2Client(ptr->type, encBuf, sizeof(encBuf));
        count      = ptr->sentCount;
        bytes      = ptr->bytesSent;
        bytesIfRaw = ptr->bytesSentIfRaw;
        
        savings = 0.0;
        if (bytesIfRaw>0.0)
            savings = 100.0 - (((double)bytes / (double)bytesIfRaw) * 100.0);
        if ((bytes>0) || (count>0) || (bytesIfRaw>0))
            rfbLog(" %-20.20s: %6d | %9d/%9d (%5.1f%%)\n",
	        name, count, bytes, bytesIfRaw, savings);
        totalRects += count;
        totalBytes += bytes;
        totalBytesIfRaw += bytesIfRaw;
    }

    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
    {
        name       = encodingName(ptr->type, encBuf, sizeof(encBuf));
        count      = ptr->sentCount;
        bytes      = ptr->bytesSent;
        bytesIfRaw = ptr->bytesSentIfRaw;
        savings    = 0.0;

        if (bytesIfRaw>0.0)
            savings = 100.0 - (((double)bytes / (double)bytesIfRaw) * 100.0);
        if ((bytes>0) || (count>0) || (bytesIfRaw>0))
            rfbLog(" %-20.20s: %6d | %9d/%9d (%5.1f%%)\n",
	        name, count, bytes, bytesIfRaw, savings);
        totalRects += count;
        totalBytes += bytes;
        totalBytesIfRaw += bytesIfRaw;
    }
    savings=0.0;
    if (totalBytesIfRaw>0.0)
        savings = 100.0 - ((totalBytes/totalBytesIfRaw)*100.0);
    rfbLog(" %-20.20s: %6d | %9.0f/%9.0f (%5.1f%%)\n",
            "TOTALS", totalRects, totalBytes,totalBytesIfRaw, savings);

    totalRects=0.0;
    totalBytes=0.0;
    totalBytesIfRaw=0.0;

    rfbLog("%-21.21s  %-6.6s   %9.9s/%9.9s (%6.6s)\n", "Statistics", "events", "Received","RawEquiv","saved");
    for (ptr = cl->statMsgList; ptr!=NULL; ptr=ptr->Next)
    {
        name       = messageNameClient2Server(ptr->type, encBuf, sizeof(encBuf));
        count      = ptr->rcvdCount;
        bytes      = ptr->bytesRcvd;
        bytesIfRaw = ptr->bytesRcvdIfRaw;
        savings    = 0.0;

        if (bytesIfRaw>0.0)
            savings = 100.0 - (((double)bytes / (double)bytesIfRaw) * 100.0);
        if ((bytes>0) || (count>0) || (bytesIfRaw>0))
            rfbLog(" %-20.20s: %6d | %9d/%9d (%5.1f%%)\n",
	        name, count, bytes, bytesIfRaw, savings);
        totalRects += count;
        totalBytes += bytes;
        totalBytesIfRaw += bytesIfRaw;
    }
    for (ptr = cl->statEncList; ptr!=NULL; ptr=ptr->Next)
    {
        name       = encodingName(ptr->type, encBuf, sizeof(encBuf));
        count      = ptr->rcvdCount;
        bytes      = ptr->bytesRcvd;
        bytesIfRaw = ptr->bytesRcvdIfRaw;
        savings    = 0.0;

        if (bytesIfRaw>0.0)
            savings = 100.0 - (((double)bytes / (double)bytesIfRaw) * 100.0);
        if ((bytes>0) || (count>0) || (bytesIfRaw>0))
            rfbLog(" %-20.20s: %6d | %9d/%9d (%5.1f%%)\n",
	        name, count, bytes, bytesIfRaw, savings);
        totalRects += count;
        totalBytes += bytes;
        totalBytesIfRaw += bytesIfRaw;
    }
    savings=0.0;
    if (totalBytesIfRaw>0.0)
        savings = 100.0 - ((totalBytes/totalBytesIfRaw)*100.0);
    rfbLog(" %-20.20s: %6d | %9.0f/%9.0f (%5.1f%%)\n",
            "TOTALS", totalRects, totalBytes,totalBytesIfRaw, savings);
      
} 

