/*
 * Copyright (c) 2005 Novell, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail,
 * you may find current contact information at www.novell.com 
 *
 * Author		: Rohit Kumar
 * Email ID	: rokumar@novell.com
 * Date		: 14th July 2005
 */
 
#ifndef HANDLE_FILE_TRANSFER_REQUEST_H
#define HANDLE_FILE_TRANSFER_REQUEST_H


#include <rfb/rfb.h>


void InitFileTransfer();
int SetFtpRoot(char* path);
void EnableFileTransfer(rfbBool enable);
rfbBool IsFileTransferEnabled();
char* GetFtpRoot();

void HandleFileListRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileDownloadRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileDownloadCancelRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileUploadRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileUploadDataRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileUploadFailedRequest(rfbClientPtr cl, rfbTightClientRec* data);
void HandleFileCreateDirRequest(rfbClientPtr cl, rfbTightClientRec* data);

#endif

