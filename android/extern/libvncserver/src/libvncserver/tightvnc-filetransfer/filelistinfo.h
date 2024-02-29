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
 

#ifndef FILE_LIST_INFO_H
#define FILE_LIST_INFO_H

#include <limits.h>

#if !defined(NAME_MAX)
#define NAME_MAX 255
#endif


#define SUCCESS 1
#define FAILURE 0

typedef struct _FileListItemInfo {
    char name[NAME_MAX];
    unsigned int size;
    unsigned int data;
} FileListItemInfo, *FileListItemInfoPtr;

typedef struct _FileListItemSize {
    unsigned int size;
    unsigned int data;
} FileListItemSize, *FileListItemSizePtr;

typedef struct _FileListInfo {
    FileListItemInfoPtr pEntries;
    int numEntries;
} FileListInfo, *FileListInfoPtr;

int AddFileListItemInfo(FileListInfoPtr fileListInfoPtr, char* name, unsigned int size, unsigned int data);
char* GetFileNameAt(FileListInfo fileListInfo, int number);
unsigned int GetFileSizeAt(FileListInfo fileListInfo, int number);
unsigned int GetFileDataAt(FileListInfo fileListInfo, int number);
unsigned int GetSumOfFileNamesLength(FileListInfo fileListInfo);
void FreeFileListInfo(FileListInfo fileListInfo);

void DisplayFileList(FileListInfo fli);

#endif

