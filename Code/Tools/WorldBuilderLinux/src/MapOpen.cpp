#include "PreRTS.h"
#include "MapOpen.h"

#include "Common/FileSystem.h"
#include "Common/AsciiString.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <algorithm>
#include <set>

namespace MapOpen
{
namespace
{
char g_pendingPath[PATH_MAX];
bool g_hasPending = false;

void SDLCALL openDialogCallback(void * /*userdata*/, const char *const *filelist, int /*filter*/)
{
	if (!filelist || !filelist[0])
		return;
	strncpy(g_pendingPath, filelist[0], sizeof(g_pendingPath) - 1);
	g_pendingPath[sizeof(g_pendingPath) - 1] = '\0';
	g_hasPending = true;
}

bool isDir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool isFile(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Like OpenMap.cpp: folder name is the map name. */
std::string folderNameFromMapPath(const std::string &path)
{
	std::string p = path;
	for (size_t i = 0; i < p.size(); ++i)
		if (p[i] == '\\')
			p[i] = '/';
	while (!p.empty() && p.back() == '/')
		p.pop_back();
	const size_t slash = p.find_last_of('/');
	if (slash == std::string::npos)
		return p;
	const size_t slash2 = p.find_last_of('/', slash ? slash - 1 : 0);
	if (slash2 == std::string::npos)
		return p.substr(0, slash);
	return p.substr(slash2 + 1, slash - slash2 - 1);
}

bool endsWithMap(const AsciiString &path)
{
	const char *s = path.str();
	const size_t n = strlen(s);
	return n >= 4 && strcasecmp(s + n - 4, ".map") == 0;
}

/** Build Maps\Name\Name.map like original OpenMap::OnOK for system maps. */
std::string officialOpenPath(const std::string &mapName)
{
	return std::string("Maps\\") + mapName + "\\" + mapName + ".map";
}
} // namespace

std::string defaultMapsDirectory()
{
	const char *home = getenv("HOME");
	if (!home || !home[0])
		return std::string("Maps");
	std::string p = std::string(home) + "/.config/Command and Conquer Generals Data/Maps";
	if (isDir(p.c_str()))
		return p;
	if (isDir("Maps"))
		return std::string("Maps");
	return p;
}

void refreshUserList(std::vector<MapEntry> &out, const char *mapsDir)
{
	out.clear();
	const std::string root = mapsDir && mapsDir[0] ? mapsDir : defaultMapsDirectory();
	DIR *d = opendir(root.c_str());
	if (!d)
		return;

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL)
	{
		if (ent->d_name[0] == '.')
			continue;
		char sub[PATH_MAX];
		snprintf(sub, sizeof(sub), "%s/%s", root.c_str(), ent->d_name);
		if (!isDir(sub))
			continue;

		/* OpenMap.cpp: Maps\Name\Name.map */
		char mapPath[PATH_MAX];
		snprintf(mapPath, sizeof(mapPath), "%s/%s.map", sub, ent->d_name);
		if (!isFile(mapPath))
			continue;

		MapEntry e;
		e.name = ent->d_name;
		e.path = mapPath;
		e.official = false;
		out.push_back(e);
	}
	closedir(d);
	std::sort(out.begin(), out.end(),
		[](const MapEntry &a, const MapEntry &b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
}

void refreshOfficialList(std::vector<MapEntry> &out)
{
	out.clear();
	if (!TheFileSystem)
	{
		fprintf(stderr, "MapOpen: TheFileSystem is NULL\n");
		return;
	}

	/*
	 * Original OpenMap (DEBUG System Maps) enumerates Maps\<dir>\ directories and keeps
	 * those that have Maps\<name>\<name>.map. Official content lives in maps.big;
	 * list via FileSystem (archive) then keep unique folder names.
	 */
	FilenameList files;
	TheFileSystem->getFileListInDirectory(AsciiString("Maps\\"), AsciiString("*.map"), files, TRUE);
	if (files.empty())
		TheFileSystem->getFileListInDirectory(AsciiString("Maps"), AsciiString("*.map"), files, TRUE);
	if (files.empty())
		TheFileSystem->getFileListInDirectory(AsciiString("maps\\"), AsciiString("*.map"), files, TRUE);

	std::set<std::string> seen;
	for (FilenameListIter it = files.begin(); it != files.end(); ++it)
	{
		if (!endsWithMap(*it))
			continue;
		const std::string folder = folderNameFromMapPath(it->str());
		if (folder.empty() || strcasecmp(folder.c_str(), "maps") == 0)
			continue;

		std::string key = folder;
		for (size_t i = 0; i < key.size(); ++i)
			key[i] = (char)tolower((unsigned char)key[i]);
		if (seen.count(key))
			continue;
		seen.insert(key);

		/* Prefer canonical Name\Name.map; fall back to the path we found. */
		const std::string canonical = officialOpenPath(folder);
		MapEntry e;
		e.name = folder;
		e.official = true;
		if (TheFileSystem->doesFileExist(canonical.c_str()))
			e.path = canonical;
		else
			e.path = it->str();
		out.push_back(e);
	}

	std::sort(out.begin(), out.end(),
		[](const MapEntry &a, const MapEntry &b) { return strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });

	fprintf(stderr, "MapOpen: official maps listed: %d (raw files=%d)\n", (int)out.size(),
		(int)files.size());
}

void showOpenDialog(SDL_Window *window, const char *defaultLocation)
{
	static const SDL_DialogFileFilter filters[] = {
		{"Generals map (*.map)", "map"},
		{"All files", "*"},
	};
	const char *loc = (defaultLocation && defaultLocation[0]) ? defaultLocation : NULL;
	SDL_ShowOpenFileDialog(openDialogCallback, NULL, window, filters, 2, loc, false);
}

bool takePendingPath(char *out, size_t outSize)
{
	if (!g_hasPending || !out || outSize == 0)
		return false;
	strncpy(out, g_pendingPath, outSize - 1);
	out[outSize - 1] = '\0';
	g_hasPending = false;
	return true;
}
} // namespace MapOpen
