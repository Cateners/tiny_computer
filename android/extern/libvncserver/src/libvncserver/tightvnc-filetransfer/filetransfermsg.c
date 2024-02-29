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
 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include <sys/utime.h>
#define mkdir(path, perms) _mkdir(path) /* Match POSIX signature */
#ifdef _MSC_VER
#define S_ISREG(m)	(((m) & _S_IFMT) == _S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFDIR) == S_IFDIR)
#define S_IWUSR		S_IWRITE
#define S_IRUSR		S_IREAD
#define S_IWOTH		0x0000002
#define S_IROTH		0x0000004
#define S_IWGRP		0x0000010
#define S_IRGRP		0x0000020
/* Prevent POSIX deprecation warnings on MSVC */
#define creat _creat
#define open _open
#define read _read
#define write _write
#define close _close
#define unlink _unlink
#endif /* _MSC_VER */
#else
#include <dirent.h>
#include <utime.h>
#endif

#include <errno.h>
#if LIBVNCSERVER_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include <rfb/rfb.h>
#include "rfbtightproto.h"
#include "filelistinfo.h"
#include "filetransfermsg.h"
#include "handlefiletransferrequest.h"

#define SZ_RFBBLOCKSIZE 8192


void
FreeFileTransferMsg(FileTransferMsg ftm)
{

	if(ftm.data != NULL) {
		free(ftm.data);
		ftm.data = NULL;
	}

	ftm.length = 0;

}


/******************************************************************************
 * Methods to handle file list request.
 ******************************************************************************/

int CreateFileListInfo(FileListInfoPtr pFileListInfo, char* path, int flag);
FileTransferMsg CreateFileListErrMsg(char flags);
FileTransferMsg CreateFileListMsg(FileListInfo fileListInfo, char flags);


/*
 * This is the method called by HandleFileListRequest to get the file list
 */

FileTransferMsg 
GetFileListResponseMsg(char* path, char flags)
{
	FileTransferMsg fileListMsg;
	FileListInfo fileListInfo;
	int status = -1;
	
	memset(&fileListMsg, 0, sizeof(FileTransferMsg));
	memset(&fileListInfo, 0, sizeof(FileListInfo));

	
	 /* fileListInfo can have null data if the folder is Empty 
	or if some error condition has occurred.
	The return value is 'failure' only if some error condition has occurred.
	 */
	status = CreateFileListInfo(&fileListInfo, path, !(flags  & 0x10));

	if(status == FAILURE) {
		fileListMsg = CreateFileListErrMsg(flags);
	}
	else {
		/* DisplayFileList(fileListInfo); For Debugging  */
		
		fileListMsg = CreateFileListMsg(fileListInfo, flags);
		FreeFileListInfo(fileListInfo);
	}
	
	return fileListMsg;
}

#if !defined(__GNUC__) && !defined(_MSC_VER)
#define __FUNCTION__ "unknown"
#endif

#ifdef WIN32

/* Most of the Windows version here is based on https://github.com/danielgindi/FileDir */

#define FILETIME_TO_TIME_T(FILETIME) (((((__int64)FILETIME.dwLowDateTime) | (((__int64)FILETIME.dwHighDateTime) << 32)) - 116444736000000000L) / 10000000L)

#ifdef FILE_ATTRIBUTE_INTEGRITY_STREAM
#define IS_REGULAR_FILE_HAS_ATTRIBUTE_INTEGRITY_STREAM(dwFileAttributes) (!!(dwFileAttributes & FILE_ATTRIBUTE_INTEGRITY_STREAM))
#else
#define IS_REGULAR_FILE_HAS_ATTRIBUTE_INTEGRITY_STREAM(dwFileAttributes) 0
#endif

#ifdef FILE_ATTRIBUTE_NO_SCRUB_DATA
#define IS_REGULAR_FILE_HAS_ATTRIBUTE_NO_SCRUB_DATA(dwFileAttributes) (!!(dwFileAttributes & FILE_ATTRIBUTE_NO_SCRUB_DATA))
#else
#define IS_REGULAR_FILE_HAS_ATTRIBUTE_NO_SCRUB_DATA(dwFileAttributes) 0
#endif

#define IS_REGULAR_FILE(dwFileAttributes) \
	( \
	!!(dwFileAttributes & FILE_ATTRIBUTE_NORMAL) || \
	( \
	!(dwFileAttributes & FILE_ATTRIBUTE_DEVICE) && \
	!(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && \
	!(dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) && \
	!IS_REGULAR_FILE_HAS_ATTRIBUTE_INTEGRITY_STREAM(dwFileAttributes) && \
	!IS_REGULAR_FILE_HAS_ATTRIBUTE_NO_SCRUB_DATA(dwFileAttributes) && \
	!(dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) && \
	!(dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) \
	) \
	)

#define IS_FOLDER(dwFileAttributes) (!!(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))

int
CreateFileListInfo(FileListInfoPtr pFileListInfo, char* path, int flag)
{
	int pathLen, basePathLength;
	char *basePath, *pChar;
	WIN32_FIND_DATAA winFindData;
	HANDLE findHandle;

	if(path == NULL) {
		return FAILURE;
	}

	if(strlen(path) == 0) {
		/* In this case we will send the list of entries in ftp root*/
		sprintf(path, "%s%s", GetFtpRoot(), "/");
	}

	/* Create a search string, like C:\folder\* */

	pathLen = strlen(path);
	basePath = malloc(pathLen + 3);
	memcpy(basePath, path, pathLen);
	basePathLength = pathLen;
	basePath[basePathLength] = '\\';
	basePath[basePathLength + 1] = '*';
	basePath[basePathLength + 2] = '\0';

	/* Start a search */
	memset(&winFindData, 0, sizeof(winFindData));
	findHandle = FindFirstFileA(path, &winFindData);

	basePath[basePathLength] = '\0'; /* Restore to a basePath + \ */
	/* Convert \ to / */
	for(pChar = basePath; *pChar; pChar++) {
		if (*pChar == '\\') {
			*pChar = '/';
		}
	}

	/* While we can find a next file do...
	   But ignore \. and '.. entries, which are current folder and parent folder respectively */
	while(findHandle != INVALID_HANDLE_VALUE && winFindData.cFileName[0] == '.' && 
		(winFindData.cFileName[1] == '\0' || 
		(winFindData.cFileName[1] == '.' && winFindData.cFileName[2] == '\0'))) {
		char fullpath[PATH_MAX];
		fullpath[0] = 0;

		strncpy_s(fullpath, PATH_MAX, basePath, basePathLength);
		strncpy_s(fullpath + basePathLength, PATH_MAX - basePathLength, winFindData.cFileName, (int)strlen(winFindData.cFileName));

		if(IS_FOLDER(winFindData.dwFileAttributes)) {
			if (AddFileListItemInfo(pFileListInfo, winFindData.cFileName, -1, 0) == 0) {
				rfbLog("File [%s]: Method [%s]: Add directory %s in the"
					" list failed\n", __FILE__, __FUNCTION__, fullpath);
				continue;
			}
		} 
		else if(IS_REGULAR_FILE(winFindData.dwFileAttributes)) {
			if(flag) {
				unsigned int fileSize = (winFindData.nFileSizeHigh * (MAXDWORD+1)) + winFindData.nFileSizeLow;
				if(AddFileListItemInfo(pFileListInfo, winFindData.cFileName, fileSize, FILETIME_TO_TIME_T(winFindData.ftLastWriteTime)) == 0) {
					rfbLog("File [%s]: Method [%s]: Add file %s in the "
						"list failed\n", __FILE__, __FUNCTION__, fullpath);
					continue;
				}			
			}
		}

		if(FindNextFileA(findHandle, &winFindData) == 0) {
			FindClose(findHandle);
			findHandle = INVALID_HANDLE_VALUE;
		}
	}

	if(findHandle != INVALID_HANDLE_VALUE) {
		FindClose(findHandle);
	}

	free(basePath);
	
	return SUCCESS;
}

#else /* WIN32 */

int
CreateFileListInfo(FileListInfoPtr pFileListInfo, char* path, int flag)
{
	DIR* pDir = NULL;
	struct dirent* pDirent = NULL;

	if(path == NULL) {
		return FAILURE;
	}

	if(strlen(path) == 0) {
		/* In this case we will send the list of entries in ftp root*/
		sprintf(path, "%s%s", GetFtpRoot(), "/");
	}

	if((pDir = opendir(path)) == NULL) {
		rfbLog("File [%s]: Method [%s]: not able to open the dir\n",
				__FILE__, __FUNCTION__);
		return FAILURE; 		
	}

	while((pDirent = readdir(pDir))) {
		if(strcmp(pDirent->d_name, ".") && strcmp(pDirent->d_name, "..")) {
			struct stat stat_buf;
			/*
			int fpLen = sizeof(char)*(strlen(pDirent->d_name)+strlen(path)+2);
			*/
			char fullpath[PATH_MAX];

			memset(fullpath, 0, PATH_MAX);

			strcpy(fullpath, path);
			if(path[strlen(path)-1] != '/')
				strcat(fullpath, "/");
			strcat(fullpath, pDirent->d_name);

			if(stat(fullpath, &stat_buf) < 0) {
				rfbLog("File [%s]: Method [%s]: Reading stat for file %s failed\n", 
						__FILE__, __FUNCTION__, fullpath);
				continue;
			}

			if(S_ISDIR(stat_buf.st_mode)) {
				if(AddFileListItemInfo(pFileListInfo, pDirent->d_name, -1, 0) == 0) {
					rfbLog("File [%s]: Method [%s]: Add directory %s in the"
							" list failed\n", __FILE__, __FUNCTION__, fullpath);
					continue;
				}
			}
			else {
				if(flag) {
					if(AddFileListItemInfo(pFileListInfo, pDirent->d_name, 
												stat_buf.st_size, 
												stat_buf.st_mtime) == 0) {
						rfbLog("File [%s]: Method [%s]: Add file %s in the "
								"list failed\n", __FILE__, __FUNCTION__, fullpath);
						continue;
					}			
				}
			}
		}
	}
	if(closedir(pDir) < 0) {
	    rfbLog("File [%s]: Method [%s]: ERROR Couldn't close dir\n",
	    	__FILE__, __FUNCTION__);
	}
	
	return SUCCESS;
}

#endif


FileTransferMsg
CreateFileListErrMsg(char flags)
{
	FileTransferMsg fileListMsg;
	rfbFileListDataMsg* pFLD = NULL;
	char* data = NULL;
	unsigned int length = 0;

	memset(&fileListMsg, 0, sizeof(FileTransferMsg));

	data = (char*) calloc(sizeof(rfbFileListDataMsg), sizeof(char));
	if(data == NULL) {
		return fileListMsg;
	}
	length = sizeof(rfbFileListDataMsg) * sizeof(char);
	pFLD = (rfbFileListDataMsg*) data;
	
	pFLD->type = rfbFileListData;
	pFLD->numFiles = Swap16IfLE(0);
	pFLD->dataSize = Swap16IfLE(0);
	pFLD->compressedSize = Swap16IfLE(0);
	pFLD->flags = flags | 0x80;

	fileListMsg.data = data;
	fileListMsg.length = length;

	return fileListMsg;
}


FileTransferMsg
CreateFileListMsg(FileListInfo fileListInfo, char flags)
{
	FileTransferMsg fileListMsg;
	rfbFileListDataMsg* pFLD = NULL;
	char *data = NULL, *pFileNames = NULL;
	unsigned int length = 0, dsSize = 0, i = 0;
	FileListItemSizePtr pFileListItemSize = NULL;

	memset(&fileListMsg, 0, sizeof(FileTransferMsg));
	dsSize = fileListInfo.numEntries * 8;
	length = sz_rfbFileListDataMsg + dsSize + 
			GetSumOfFileNamesLength(fileListInfo) + 
			fileListInfo.numEntries;

	data = (char*) calloc(length, sizeof(char));
	if(data == NULL) {
		return fileListMsg;
	}
	pFLD = (rfbFileListDataMsg*) data;
	pFileListItemSize = (FileListItemSizePtr) &data[sz_rfbFileListDataMsg];
	pFileNames = &data[sz_rfbFileListDataMsg + dsSize];

	pFLD->type            = rfbFileListData;
    pFLD->flags 		  = flags & 0xF0;
    pFLD->numFiles 		  = Swap16IfLE(fileListInfo.numEntries);
    pFLD->dataSize 		  = Swap16IfLE(GetSumOfFileNamesLength(fileListInfo) + 
    									fileListInfo.numEntries);
    pFLD->compressedSize  = pFLD->dataSize;

	for(i =0; i <fileListInfo.numEntries; i++) {
		pFileListItemSize[i].size = Swap32IfLE(GetFileSizeAt(fileListInfo, i));
		pFileListItemSize[i].data = Swap32IfLE(GetFileDataAt(fileListInfo, i));
		strcpy(pFileNames, GetFileNameAt(fileListInfo, i));
		
		if(i+1 < fileListInfo.numEntries)
			pFileNames += strlen(pFileNames) + 1;
	}

	fileListMsg.data 	= data;
	fileListMsg.length 	= length;

	return fileListMsg;
}


/******************************************************************************
 * Methods to handle File Download Request.
 ******************************************************************************/

FileTransferMsg CreateFileDownloadErrMsg(char* reason, unsigned int reasonLen);
FileTransferMsg CreateFileDownloadZeroSizeDataMsg(unsigned long mTime);
FileTransferMsg CreateFileDownloadBlockSizeDataMsg(unsigned short sizeFile, char *pFile);

FileTransferMsg 
GetFileDownLoadErrMsg()
{
	FileTransferMsg fileDownloadErrMsg;

	char reason[] = "An internal error on the server caused download failure";
	int reasonLen = strlen(reason);

	memset(&fileDownloadErrMsg, 0, sizeof(FileTransferMsg));
	
	fileDownloadErrMsg = CreateFileDownloadErrMsg(reason, reasonLen);

	return fileDownloadErrMsg;
}


FileTransferMsg
GetFileDownloadReadDataErrMsg()
{
	char reason[] = "Cannot open file, perhaps it is absent or is a directory";
	int reasonLen = strlen(reason);

	return CreateFileDownloadErrMsg(reason, reasonLen);

}


FileTransferMsg
GetFileDownloadLengthErrResponseMsg()
{
	char reason [] = "Path length exceeds PATH_MAX (4096) bytes";
	int reasonLen = strlen(reason);

	return CreateFileDownloadErrMsg(reason, reasonLen);
}


FileTransferMsg
GetFileDownloadResponseMsgInBlocks(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	/* const unsigned int sz_rfbBlockSize = SZ_RFBBLOCKSIZE; */
    int numOfBytesRead = 0;
	char pBuf[SZ_RFBBLOCKSIZE];
	char* path = rtcp->rcft.rcfd.fName;

	memset(pBuf, 0, SZ_RFBBLOCKSIZE);

	if((rtcp->rcft.rcfd.downloadInProgress == FALSE) && (rtcp->rcft.rcfd.downloadFD == -1)) {
		if((rtcp->rcft.rcfd.downloadFD = open(path, O_RDONLY)) == -1) {
			rfbLog("File [%s]: Method [%s]: Error: Couldn't open file\n", 
					__FILE__, __FUNCTION__);
			return GetFileDownloadReadDataErrMsg();
		}
		rtcp->rcft.rcfd.downloadInProgress = TRUE;
	}
	if((rtcp->rcft.rcfd.downloadInProgress == TRUE) && (rtcp->rcft.rcfd.downloadFD != -1)) {
		if( (numOfBytesRead = read(rtcp->rcft.rcfd.downloadFD, pBuf, SZ_RFBBLOCKSIZE)) <= 0) {
			close(rtcp->rcft.rcfd.downloadFD);
			rtcp->rcft.rcfd.downloadFD = -1;
			rtcp->rcft.rcfd.downloadInProgress = FALSE;
			if(numOfBytesRead == 0) {
				return CreateFileDownloadZeroSizeDataMsg(rtcp->rcft.rcfd.mTime);
			}			
			return GetFileDownloadReadDataErrMsg();
		}
	return CreateFileDownloadBlockSizeDataMsg(numOfBytesRead, pBuf);
	}
	return GetFileDownLoadErrMsg();
}


FileTransferMsg
ChkFileDownloadErr(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
    FileTransferMsg fileDownloadMsg;
	struct stat stat_buf;
	int sz_rfbFileSize = 0;
	char* path = rtcp->rcft.rcfd.fName;

	memset(&fileDownloadMsg, 0, sizeof(FileTransferMsg));

	if( (path == NULL) || (strlen(path) == 0) ||
		(stat(path, &stat_buf) < 0) || (!(S_ISREG(stat_buf.st_mode))) ) {

			char reason[] = "Cannot open file, perhaps it is absent or is not a regular file";
			int reasonLen = strlen(reason);

			rfbLog("File [%s]: Method [%s]: Reading stat for path %s failed\n", 
					__FILE__, __FUNCTION__, path);	
			
			fileDownloadMsg = CreateFileDownloadErrMsg(reason, reasonLen);
	}
	else {
		rtcp->rcft.rcfd.mTime = stat_buf.st_mtime;
		sz_rfbFileSize = stat_buf.st_size;
		if(sz_rfbFileSize <= 0) {
			fileDownloadMsg = CreateFileDownloadZeroSizeDataMsg(stat_buf.st_mtime);
		}

	}
	return fileDownloadMsg;
}


FileTransferMsg
CreateFileDownloadErrMsg(char* reason, unsigned int reasonLen)
{
	FileTransferMsg fileDownloadErrMsg;
	int length = sz_rfbFileDownloadFailedMsg + reasonLen + 1;
	rfbFileDownloadFailedMsg *pFDF = NULL;
	char *pFollow = NULL;
	
	char *pData = (char*) calloc(length, sizeof(char));
	memset(&fileDownloadErrMsg, 0, sizeof(FileTransferMsg));
	if(pData == NULL) {
		rfbLog("File [%s]: Method [%s]: pData is NULL\n",
				__FILE__, __FUNCTION__);	
		return fileDownloadErrMsg;
	}

	pFDF = (rfbFileDownloadFailedMsg *) pData;
	pFollow = &pData[sz_rfbFileDownloadFailedMsg];
	
	pFDF->type = rfbFileDownloadFailed;
	pFDF->reasonLen = Swap16IfLE(reasonLen);
	memcpy(pFollow, reason, reasonLen);

	fileDownloadErrMsg.data	= pData;
	fileDownloadErrMsg.length	= length;

	return fileDownloadErrMsg;
}


FileTransferMsg
CreateFileDownloadZeroSizeDataMsg(unsigned long mTime)
{
	FileTransferMsg fileDownloadZeroSizeDataMsg;
	int length = sz_rfbFileDownloadDataMsg + sizeof(uint32_t);
	rfbFileDownloadDataMsg *pFDD = NULL;
	char *pFollow = NULL;
	
	char *pData = (char*) calloc(length, sizeof(char));
	memset(&fileDownloadZeroSizeDataMsg, 0, sizeof(FileTransferMsg));
	if(pData == NULL) {
		rfbLog("File [%s]: Method [%s]: pData is NULL\n",
				__FILE__, __FUNCTION__);	
		return fileDownloadZeroSizeDataMsg;
	}

	pFDD = (rfbFileDownloadDataMsg *) pData;
	pFollow = &pData[sz_rfbFileDownloadDataMsg];
	
	pFDD->type = rfbFileDownloadData;
	pFDD->compressLevel = 0;
	pFDD->compressedSize = Swap16IfLE(0);
	pFDD->realSize = Swap16IfLE(0);
	
	memcpy(pFollow, &mTime, sizeof(uint32_t));

	fileDownloadZeroSizeDataMsg.data	= pData;
	fileDownloadZeroSizeDataMsg.length	= length;

	return fileDownloadZeroSizeDataMsg;

}


FileTransferMsg
CreateFileDownloadBlockSizeDataMsg(unsigned short sizeFile, char *pFile)
{
	FileTransferMsg fileDownloadBlockSizeDataMsg;
	int length = sz_rfbFileDownloadDataMsg + sizeFile;
	rfbFileDownloadDataMsg *pFDD = NULL;
	char *pFollow = NULL;
	
	char *pData = (char*) calloc(length, sizeof(char));
	memset(&fileDownloadBlockSizeDataMsg, 0, sizeof(FileTransferMsg));
	if(NULL == pData) {
		rfbLog("File [%s]: Method [%s]: pData is NULL\n",
				__FILE__, __FUNCTION__);	
		return fileDownloadBlockSizeDataMsg;
	}

	pFDD = (rfbFileDownloadDataMsg *) pData;
	pFollow = &pData[sz_rfbFileDownloadDataMsg];
	
	pFDD->type = rfbFileDownloadData;
	pFDD->compressLevel = 0;
	pFDD->compressedSize = Swap16IfLE(sizeFile);
	pFDD->realSize = Swap16IfLE(sizeFile);
	
	memcpy(pFollow, pFile, sizeFile);

	fileDownloadBlockSizeDataMsg.data	= pData;
	fileDownloadBlockSizeDataMsg.length	= length;

	return fileDownloadBlockSizeDataMsg;

}


/******************************************************************************
 * Methods to handle file upload request
 ******************************************************************************/

FileTransferMsg CreateFileUploadErrMsg(char* reason, unsigned int reasonLen);

FileTransferMsg 
GetFileUploadLengthErrResponseMsg()
{
	char reason [] = "Path length exceeds PATH_MAX (4096) bytes";
	int reasonLen = strlen(reason);

	return CreateFileUploadErrMsg(reason, reasonLen);
}


FileTransferMsg
ChkFileUploadErr(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
    FileTransferMsg fileUploadErrMsg;

	memset(&fileUploadErrMsg, 0, sizeof(FileTransferMsg));
	if((strlen(rtcp->rcft.rcfu.fName) == 0) ||
		((rtcp->rcft.rcfu.uploadFD = creat(rtcp->rcft.rcfu.fName, 
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)) {

			char reason[] = "Could not create file";
			int reasonLen = strlen(reason);
			fileUploadErrMsg = CreateFileUploadErrMsg(reason, reasonLen);
	}
	else
		rtcp->rcft.rcfu.uploadInProgress = TRUE;
	
	return fileUploadErrMsg;
}


FileTransferMsg
GetFileUploadCompressedLevelErrMsg()
{
	char reason[] = "Server does not support data compression on upload";
	int reasonLen = strlen(reason);

	return CreateFileUploadErrMsg(reason, reasonLen);
}


FileTransferMsg
ChkFileUploadWriteErr(rfbClientPtr cl, rfbTightClientPtr rtcp, char* pBuf)
{
	FileTransferMsg ftm;
	unsigned long numOfBytesWritten = 0;

	memset(&ftm, 0, sizeof(FileTransferMsg));

	numOfBytesWritten = write(rtcp->rcft.rcfu.uploadFD, pBuf, rtcp->rcft.rcfu.fSize);

	if(numOfBytesWritten != rtcp->rcft.rcfu.fSize) {		
		char reason[] = "Error writing file data";
		int reasonLen = strlen(reason);
		ftm = CreateFileUploadErrMsg(reason, reasonLen);
		CloseUndoneFileUpload(cl, rtcp);
	}		
	return ftm;
}


void
FileUpdateComplete(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	/* Here we are settimg the modification and access time of the file */
	/* Windows code stes mod/access/creation time of the file */
	struct utimbuf utb;

	utb.actime = utb.modtime = rtcp->rcft.rcfu.mTime;
	if(utime(rtcp->rcft.rcfu.fName, &utb) == -1) {
		rfbLog("File [%s]: Method [%s]: Setting the modification/access"
				" time for the file <%s> failed\n", __FILE__, 
				__FUNCTION__, rtcp->rcft.rcfu.fName);
	}

	if(rtcp->rcft.rcfu.uploadFD != -1) {
		close(rtcp->rcft.rcfu.uploadFD);
		rtcp->rcft.rcfu.uploadFD = -1;
		rtcp->rcft.rcfu.uploadInProgress = FALSE;
	}
}


FileTransferMsg
CreateFileUploadErrMsg(char* reason, unsigned int reasonLen)
{
	FileTransferMsg fileUploadErrMsg;
	int length = sz_rfbFileUploadCancelMsg + reasonLen;
	rfbFileUploadCancelMsg *pFDF = NULL;
	char *pFollow = NULL;
	
	char *pData = (char*) calloc(length, sizeof(char));
	memset(&fileUploadErrMsg, 0, sizeof(FileTransferMsg));
	if(pData == NULL) {
		rfbLog("File [%s]: Method [%s]: pData is NULL\n",
				__FILE__, __FUNCTION__);	
		return fileUploadErrMsg;
	}

	pFDF = (rfbFileUploadCancelMsg *) pData;
	pFollow = &pData[sz_rfbFileUploadCancelMsg];
	
	pFDF->type = rfbFileUploadCancel;
	pFDF->reasonLen = Swap16IfLE(reasonLen);
	memcpy(pFollow, reason, reasonLen);

	fileUploadErrMsg.data		= pData;
	fileUploadErrMsg.length		= length;

	return fileUploadErrMsg;
}


/******************************************************************************
 * Method to cancel File Transfer operation.
 ******************************************************************************/

void
CloseUndoneFileUpload(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	/* TODO :: File Upload case is not handled currently */
	/* TODO :: In case of concurrency we need to use Critical Section */

	if(cl == NULL)
		return;

	
	if(rtcp->rcft.rcfu.uploadInProgress == TRUE) {
		rtcp->rcft.rcfu.uploadInProgress = FALSE;

		if(rtcp->rcft.rcfu.uploadFD != -1) {
			close(rtcp->rcft.rcfu.uploadFD);
			rtcp->rcft.rcfu.uploadFD = -1;
		}

		if(unlink(rtcp->rcft.rcfu.fName) == -1) {
			rfbLog("File [%s]: Method [%s]: Delete operation on file <%s> failed\n", 
					__FILE__, __FUNCTION__, rtcp->rcft.rcfu.fName);
		}

		memset(rtcp->rcft.rcfu.fName, 0 , PATH_MAX);
	}
}


void
CloseUndoneFileDownload(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	if(cl == NULL)
		return;
	
	if(rtcp->rcft.rcfd.downloadInProgress == TRUE) {
		rtcp->rcft.rcfd.downloadInProgress = FALSE;
		/* the thread will return if downloadInProgress is FALSE */
		pthread_join(rtcp->rcft.rcfd.downloadThread, NULL);

		if(rtcp->rcft.rcfd.downloadFD != -1) {			
			close(rtcp->rcft.rcfd.downloadFD);
			rtcp->rcft.rcfd.downloadFD = -1;
		}
		memset(rtcp->rcft.rcfd.fName, 0 , PATH_MAX);
	}
}


/******************************************************************************
 * Method to handle create directory request.
 ******************************************************************************/

#ifdef _MSC_VER
#undef CreateDirectory /* Prevent macro clashes under Windows */
#endif /* _MSC_VER */

void
CreateDirectory(char* dirName)
{
	if(dirName == NULL) return;

	if(mkdir(dirName, 0700) == -1) {
		rfbLog("File [%s]: Method [%s]: Create operation for directory <%s> failed\n", 
				__FILE__, __FUNCTION__, dirName);
	}
}
