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
 

#ifndef FILE_TRANSFER_MSG_H
#define FILE_TRANSFER_MSG_H

#ifdef WIN32
#pragma push_macro("CreateDirectory")
#undef CreateDirectory /* Prevent macro clashes under Windows */
#endif /* _MSC_VER */

typedef struct _FileTransferMsg {
	char* data;
	unsigned int length;
} FileTransferMsg;

FileTransferMsg GetFileListResponseMsg(char* path, char flag);

FileTransferMsg GetFileDownloadResponseMsg(char* path);
FileTransferMsg GetFileDownloadLengthErrResponseMsg();
FileTransferMsg  GetFileDownLoadErrMsg();
FileTransferMsg GetFileDownloadResponseMsgInBlocks(rfbClientPtr cl, rfbTightClientPtr data);
FileTransferMsg ChkFileDownloadErr(rfbClientPtr cl, rfbTightClientPtr data);

FileTransferMsg GetFileUploadLengthErrResponseMsg();
FileTransferMsg GetFileUploadCompressedLevelErrMsg();
FileTransferMsg ChkFileUploadErr(rfbClientPtr cl, rfbTightClientPtr data);
FileTransferMsg ChkFileUploadWriteErr(rfbClientPtr cl, rfbTightClientPtr data, char* pBuf);

void CreateDirectory(char* dirName);
void FileUpdateComplete(rfbClientPtr cl, rfbTightClientPtr data);
void CloseUndoneFileUpload(rfbClientPtr cl, rfbTightClientPtr data);
void CloseUndoneFileDownload(rfbClientPtr cl, rfbTightClientPtr data);

void FreeFileTransferMsg(FileTransferMsg ftm);

#ifdef _MSC_VER
#  pragma pop_macro("CreateDirectory") /* Restore original macro */
#endif /* _MSC_VER */

#endif

