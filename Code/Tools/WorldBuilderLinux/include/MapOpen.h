#pragma once

#include <vector>
#include <string>

struct SDL_Window;

namespace MapOpen
{
	struct MapEntry
	{
		std::string name;  /* display name */
		std::string path;  /* disk path or VFS path (Maps\\Name\\Name.map) */
		bool official = false;
	};

	/** $HOME/.config/Command and Conquer Generals Data/Maps */
	std::string defaultMapsDirectory();

	/** User maps on disk under Maps/<name>/<name>.map */
	void refreshUserList(std::vector<MapEntry> &out, const char *mapsDir = nullptr);

	/** Official maps from maps.big via TheFileSystem (Maps\\*.map). */
	void refreshOfficialList(std::vector<MapEntry> &out);

	void showOpenDialog(SDL_Window *window, const char *defaultLocation);
	bool takePendingPath(char *out, size_t outSize);
}
