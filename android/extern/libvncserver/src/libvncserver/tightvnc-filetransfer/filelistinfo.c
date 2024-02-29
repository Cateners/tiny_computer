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
#include "rfb/rfb.h"
#include "filelistinfo.h"


/* This method is used for debugging purpose */
void
DisplayFileList(FileListInfo fli)
{
    int i = 0;
    if((fli.pEntries == NULL) || (fli.numEntries == 0)) return;
    
    rfbLog("DISPLAYING FILE NAMES IN THE LIST ...START\n\n");
    rfbLog("Numer of entries:: %d\n", fli.numEntries);
    for(i = 0; i < fli.numEntries; i++)
		rfbLog("file[%d]\t<%s>\n", i, fli.pEntries[i].name);
    rfbLog("DISPLAYING FILE NAMES IN THE LIST ...END\n\n");
}

#ifndef __GNUC__
#define __FUNCTION__ "unknown"
#endif

int 
AddFileListItemInfo(FileListInfoPtr fileListInfoPtr, char* name, 
					unsigned int size, unsigned int data)
{
	FileListItemInfoPtr fileListItemInfoPtr = (FileListItemInfoPtr) 
												calloc((fileListInfoPtr->numEntries + 1), 
														sizeof(FileListItemInfo));
	if(fileListItemInfoPtr == NULL) {
	    rfbLog("File [%s]: Method [%s]: fileListItemInfoPtr is NULL\n",
	    		__FILE__, __FUNCTION__);
		return FAILURE;
	}    

	if(fileListInfoPtr->numEntries != 0) {
	    memcpy(fileListItemInfoPtr, fileListInfoPtr->pEntries, 
	    		fileListInfoPtr->numEntries * sizeof(FileListItemInfo));
	}

	strcpy(fileListItemInfoPtr[fileListInfoPtr->numEntries].name, name);
	fileListItemInfoPtr[fileListInfoPtr->numEntries].size = size;
	fileListItemInfoPtr[fileListInfoPtr->numEntries].data = data;

	if(fileListInfoPtr->pEntries != NULL) {
	    free(fileListInfoPtr->pEntries);
	    fileListInfoPtr->pEntries = NULL;	
	}

	fileListInfoPtr->pEntries = fileListItemInfoPtr;
	fileListItemInfoPtr = NULL;
	fileListInfoPtr->numEntries++;

	return SUCCESS;
}


char* 
GetFileNameAt(FileListInfo fileListInfo, int number)
{
	char* name = NULL;
	if(number >= 0 && number < fileListInfo.numEntries)
		name = fileListInfo.pEntries[number].name;
	return name;
}


unsigned int 
GetFileSizeAt(FileListInfo fileListInfo, int number)
{
	unsigned int size = 0;
	if(number >= 0 && number < fileListInfo.numEntries)
		size = fileListInfo.pEntries[number].size;
	return size;
}


unsigned int 
GetFileDataAt(FileListInfo fileListInfo, int number)
{
	unsigned int data = 0;
	if(number >= 0 && number < fileListInfo.numEntries)
		data = fileListInfo.pEntries[number].data;
	return data;
}


unsigned int 
GetSumOfFileNamesLength(FileListInfo fileListInfo)
{
	int i = 0, sumLen = 0;
	for(i = 0; i < fileListInfo.numEntries; i++)
		sumLen += strlen(fileListInfo.pEntries[i].name);
	return sumLen;
}


void
FreeFileListInfo(FileListInfo fileListInfo)
{
	if(fileListInfo.pEntries != NULL) {
		free(fileListInfo.pEntries);
		fileListInfo.pEntries = NULL;
	}
	fileListInfo.numEntries = 0;
}

