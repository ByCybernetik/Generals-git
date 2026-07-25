#pragma once

#include "MapOpen.h"
#include <vector>

struct SDL_Window;

class MapDocument;
class MapViewport;

namespace MainUi
{
	struct State
	{
		char pathBuf[1024];
		bool openClicked = false;
		bool browseClicked = false;
		bool newClicked = false;
		bool requestQuit = false;
		bool mapsListDirty = true;
		int mapViewW = 1280;
		int mapViewH = 720;
		int mapsTab = 1; /* 0 = user, 1 = system/official (OpenMap DEBUG default) */
		std::vector<MapOpen::MapEntry> userMaps;
		std::vector<MapOpen::MapEntry> officialMaps;
	};

	void draw(State &state, MapDocument &doc, MapViewport &viewport, SDL_Window *window);
}
