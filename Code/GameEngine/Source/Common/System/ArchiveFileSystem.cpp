/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
//                                                                          
//                       Westwood Studios Pacific.                          
//                                                                          
//                       Confidential Information                           
//                Copyright (C) 2001 - All Rights Reserved                  
//                                                                          
//----------------------------------------------------------------------------
//
// Project:   Generals
//
// Module:    Game Engine Common
//
// File name: ArchiveFileSystem.cpp
//
// Created:   11/26/01 TR
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes                                                      
//----------------------------------------------------------------------------

#include "PreRTS.h"
#include "Common/ArchiveFile.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/AsciiString.h"
#include "Common/PerfTimer.h"
#include <string.h>

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

//----------------------------------------------------------------------------
//         Externals                                                     
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines                                                         
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Types                                                     
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
//         Private Data                                                     
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data                                                      
//----------------------------------------------------------------------------

ArchiveFileSystem *TheArchiveFileSystem = NULL;


//----------------------------------------------------------------------------
//         Private Prototypes                                               
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions                                               
//----------------------------------------------------------------------------

static void AddArchiveLookupCandidate(AsciiString *candidates, int *count, int maxCandidates, const AsciiString& candidate)
{
	if (candidate.isEmpty() || *count >= maxCandidates) {
		return;
	}

	for (int i = 0; i < *count; ++i) {
		if (candidates[i] == candidate) {
			return;
		}
	}

	candidates[(*count)++] = candidate;
}

static void AddTgaToDdsCandidate(AsciiString *candidates, int *count, int maxCandidates, const AsciiString& path)
{
	if (!path.endsWith(".tga")) {
		return;
	}

	char buf[_MAX_PATH];
	strncpy(buf, path.str(), sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	Int len = (Int)strlen(buf);
	if (len >= 4) {
		buf[len - 3] = 'd';
		buf[len - 2] = 'd';
		buf[len - 1] = 's';
		AddArchiveLookupCandidate(candidates, count, maxCandidates, AsciiString(buf));
	}
}

static void AddStrippedPrefixCandidates(AsciiString *candidates, int *count, int maxCandidates, const AsciiString& path)
{
	static const char *prefixes[] = {
		"art/textures/",
		"art/terrain/",
	};

	for (int i = 0; i < 2; ++i) {
		if (path.startsWith(prefixes[i])) {
			AsciiString shortPath(path.str() + strlen(prefixes[i]));
			AddArchiveLookupCandidate(candidates, count, maxCandidates, shortPath);
			AddTgaToDdsCandidate(candidates, count, maxCandidates, shortPath);
		}
	}
}

int ArchiveBuildLookupPathCandidates(const AsciiString& filename, AsciiString *candidates, int maxCandidates)
{
	int count = 0;
	AsciiString base = filename;
	base.toLower();

	AddArchiveLookupCandidate(candidates, &count, maxCandidates, base);
	AddTgaToDdsCandidate(candidates, &count, maxCandidates, base);
	AddStrippedPrefixCandidates(candidates, &count, maxCandidates, base);

	return count;
}

Bool ArchiveLookupInGlobalTree(const ArchivedDirectoryInfo *dirInfo, const AsciiString& path, AsciiString *archiveFilenameOut)
{
	AsciiString work = path;
	work.toLower();
	AsciiString token;
	const ArchivedDirectoryInfo *dir = dirInfo;

	work.nextToken(&token, "\\/");

	while (!token.find('.') || work.find('.')) {
		ArchivedDirectoryInfoMap::const_iterator it = dir->m_directories.find(token);
		if (it == dir->m_directories.end()) {
			return FALSE;
		}
		dir = &it->second;
		work.nextToken(&token, "\\/");
	}

	ArchivedFileLocationMap::const_iterator it = dir->m_files.find(token);
	if (it == dir->m_files.end()) {
		return FALSE;
	}

	if (archiveFilenameOut != NULL) {
		*archiveFilenameOut = it->second;
	}
	return TRUE;
}

const ArchivedFileInfo *ArchiveLookupInArchiveTree(const DetailedArchivedDirectoryInfo *dirInfo, const AsciiString& path)
{
	AsciiString work = path;
	work.toLower();
	AsciiString token;
	const DetailedArchivedDirectoryInfo *dir = dirInfo;

	work.nextToken(&token, "\\/");

	while ((token.find('.') == NULL) || (work.find('.') != NULL)) {
		DetailedArchivedDirectoryInfoMap::const_iterator it = dir->m_directories.find(token);
		if (it == dir->m_directories.end()) {
			return NULL;
		}
		dir = &it->second;
		work.nextToken(&token, "\\/");
	}

	ArchivedFileInfoMap::const_iterator it = dir->m_files.find(token);
	if (it != dir->m_files.end()) {
		return &it->second;
	}
	return NULL;
}


//----------------------------------------------------------------------------
//         Public Functions                                                
//----------------------------------------------------------------------------

//------------------------------------------------------
// ArchivedFileInfo
//------------------------------------------------------
ArchiveFileSystem::ArchiveFileSystem() 
{
}

ArchiveFileSystem::~ArchiveFileSystem() 
{
	ArchiveFileMap::iterator iter = m_archiveFileMap.begin();
	while (iter != m_archiveFileMap.end()) {
		ArchiveFile *file = iter->second;
		if (file != NULL) {
			delete file;
			file = NULL;
		}
		iter++;
	}
}

void ArchiveFileSystem::loadIntoDirectoryTree(const ArchiveFile *archiveFile, const AsciiString& archiveFilename, Bool overwrite)
{

	FilenameList filenameList;

	archiveFile->getFileListInDirectory(AsciiString(""), AsciiString(""), AsciiString("*"), filenameList, TRUE);

	FilenameListIter it = filenameList.begin();

	AsciiString archiveKey = archiveFilename;
	archiveKey.toLower();

	while (it != filenameList.end()) {
		// add this filename to the directory tree.
		AsciiString path = *it;
		path.toLower();
		AsciiString token;
		AsciiString debugpath;

		ArchivedDirectoryInfo *dirInfo = &m_rootDirectory;

		Bool infoInPath;
		infoInPath = path.nextToken(&token, "\\/");

		while (infoInPath && (!token.find('.') || path.find('.'))) {
			ArchivedDirectoryInfoMap::iterator tempiter = dirInfo->m_directories.find(token);
			if (tempiter == dirInfo->m_directories.end()) 
			{
				dirInfo->m_directories[token].clear();
				dirInfo->m_directories[token].m_directoryName = token;
			}

			dirInfo = &(dirInfo->m_directories[token]);
			debugpath.concat(token);
			debugpath.concat('\\');
			infoInPath = path.nextToken(&token, "\\/");
		}

		// token is the filename, and dirInfo is the directory that this file is in.
		if (dirInfo->m_files.find(token) == dirInfo->m_files.end() || overwrite) {
			AsciiString path2;
			path2 = debugpath;
			path2.concat(token);
//			DEBUG_LOG(("ArchiveFileSystem::loadIntoDirectoryTree - adding file %s, archived in %s\n", path2.str(), archiveFilename.str()));
			dirInfo->m_files[token] = archiveKey;
		}

		it++;
	}
}

void ArchiveFileSystem::loadMods() {
	if (TheGlobalData->m_modBIG.isNotEmpty())
	{
		ArchiveFile *archiveFile = openArchiveFile(TheGlobalData->m_modBIG.str());

		if (archiveFile != NULL) {
			DEBUG_LOG(("ArchiveFileSystem::loadMods - loading %s into the directory tree.\n", TheGlobalData->m_modBIG.str()));
			loadIntoDirectoryTree(archiveFile, TheGlobalData->m_modBIG, TRUE);
			m_archiveFileMap[TheGlobalData->m_modBIG] = archiveFile;
			DEBUG_LOG(("ArchiveFileSystem::loadMods - %s inserted into the archive file map.\n", TheGlobalData->m_modBIG.str()));
		}
		else
		{
			DEBUG_LOG(("ArchiveFileSystem::loadMods - could not openArchiveFile(%s)\n", TheGlobalData->m_modBIG.str()));
		}
	}

	if (TheGlobalData->m_modDir.isNotEmpty())
	{
#ifdef DEBUG_LOGGING
		Bool ret =
#endif
		loadBigFilesFromDirectory(TheGlobalData->m_modDir, "*.big", TRUE);
		DEBUG_ASSERTLOG(ret, ("loadBigFilesFromDirectory(%s) returned FALSE!\n", TheGlobalData->m_modDir.str()));
	}
}

Bool ArchiveFileSystem::doesFileExist(const Char *filename) const
{
	AsciiString candidates[ARCHIVE_LOOKUP_MAX_CANDIDATES];
	Int count = ArchiveBuildLookupPathCandidates(AsciiString(filename), candidates, ARCHIVE_LOOKUP_MAX_CANDIDATES);

	for (Int i = 0; i < count; ++i) {
		if (ArchiveLookupInGlobalTree(&m_rootDirectory, candidates[i], NULL)) {
			return TRUE;
		}
	}

	return FALSE;
}

ArchiveFile *ArchiveFileSystem::findArchiveFile(const AsciiString& archiveFilename) const
{
	if (archiveFilename.isEmpty()) {
		return NULL;
	}

	AsciiString key = archiveFilename;
	key.toLower();

	ArchiveFileMap::const_iterator it = m_archiveFileMap.find(key);
	if (it != m_archiveFileMap.end()) {
		return it->second;
	}

	for (it = m_archiveFileMap.begin(); it != m_archiveFileMap.end(); ++it) {
		if (it->first.compareNoCase(archiveFilename) == 0) {
			return it->second;
		}
	}

	return NULL;
}

File * ArchiveFileSystem::openFile(const Char *filename, Int access /* = 0 */) 
{
	AsciiString archiveFilename;
	archiveFilename = getArchiveFilenameForFile(AsciiString(filename));

	if (archiveFilename.getLength() == 0) {
		return NULL;
	}

	ArchiveFile *archiveFile = findArchiveFile(archiveFilename);
	if (archiveFile == NULL) {
		return NULL;
	}

	File *opened = archiveFile->openFile(filename, access);
	return opened;
}

Bool ArchiveFileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo) const
{
	if (fileInfo == NULL) {
		return FALSE;
	}

	if (filename.getLength() <= 0) {
		return FALSE;
	}

	AsciiString archiveFilename = getArchiveFilenameForFile(filename);
	ArchiveFile *archiveFile = findArchiveFile(archiveFilename);
	if (archiveFile != NULL)
	{
		return archiveFile->getFileInfo(filename, fileInfo);
	}
	else
	{
		return FALSE;
	}
}

AsciiString ArchiveFileSystem::getArchiveFilenameForFile(const AsciiString& filename) const
{
	AsciiString candidates[ARCHIVE_LOOKUP_MAX_CANDIDATES];
	Int count = ArchiveBuildLookupPathCandidates(filename, candidates, ARCHIVE_LOOKUP_MAX_CANDIDATES);
	AsciiString archiveFilename;

	for (Int i = 0; i < count; ++i) {
		if (ArchiveLookupInGlobalTree(&m_rootDirectory, candidates[i], &archiveFilename)) {
			return archiveFilename;
		}
	}

	return AsciiString::TheEmptyString;
}

void ArchiveFileSystem::getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList &filenameList, Bool searchSubdirectories) const
{
	ArchiveFileMap::const_iterator it = m_archiveFileMap.begin();
	while (it != m_archiveFileMap.end()) {
		it->second->getFileListInDirectory(currentDirectory, originalDirectory, searchName, filenameList, searchSubdirectories);
		it++;
	}
}
