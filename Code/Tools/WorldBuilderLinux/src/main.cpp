#include "VulkanHost.h"
#include "EditorApp.h"
#include "MapDocument.h"
#include "MapViewport.h"
#include "MapOpen.h"
#include "MainUi.h"
#include "RoadGeometry.h"

#include "Common/AsciiString.h"
#include "imgui.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <string>

static const Int kDefaultPlayableX = 100;
static const Int kDefaultPlayableY = 100;
static const UnsignedByte kDefaultHeight = 16;
static const Int kDefaultBorder = 30;

static void resolveGameDir(char *out, size_t outSize, int argc, char **argv)
{
	out[0] = '\0';
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc)
		{
			strncpy(out, argv[i + 1], outSize - 1);
			out[outSize - 1] = '\0';
			return;
		}
	}
	if (access("Data", R_OK) == 0)
	{
		strncpy(out, ".", outSize - 1);
		return;
	}
	if (access("game/Data", R_OK) == 0)
	{
		strncpy(out, "game", outSize - 1);
		return;
	}
	strncpy(out, ".", outSize - 1);
}

static bool refreshViewport(VulkanHost &host, MapDocument &doc, MapViewport &viewport)
{
	if (!viewport.rebuildMesh(host, doc))
	{
		fprintf(stderr, "MapViewport::rebuildMesh failed\n");
		return false;
	}
	viewport.render(host, 1280, 720);
	return viewport.hasTexture();
}

int main(int argc, char **argv)
{
	char gameDir[PATH_MAX];
	resolveGameDir(gameDir, sizeof(gameDir), argc, argv);

	EditorApp editor;
	if (!editor.init(gameDir))
	{
		fprintf(stderr, "EditorApp::init failed (game-dir=%s)\n", gameDir);
		return 1;
	}

	/* Headless diagnostic: worldbuilder --dump-roads <path.to.map> */
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--dump-roads") == 0 && i + 1 < argc)
		{
			const char *mapPath = argv[i + 1];
			MapDocument doc;
			if (!doc.load(AsciiString(mapPath)))
			{
				fprintf(stderr, "dump-roads: load failed: %s\n", doc.lastError().str());
				editor.shutdown();
				return 1;
			}
			const std::vector<MapRoadSegment> &segs = doc.roads();
			printf("=== %zu map road segments ===\n", segs.size());
			for (size_t s = 0; s < segs.size(); ++s)
			{
				const MapRoadSegment &r = segs[s];
				printf("seg[%3zu] %-16s (%8.2f,%8.2f) -> (%8.2f,%8.2f) flags %x/%x\n", s, r.name.str(), r.x0,
					r.y0, r.x1, r.y1, r.flags0, r.flags1);
			}
			std::vector<RoadDrawPiece> pieces;
			buildRoadDrawPieces(segs, pieces);
			printf("=== %zu draw pieces ===\n", pieces.size());
			for (size_t p = 0; p < pieces.size(); ++p)
			{
				const RoadDrawPiece &r = pieces[p];
				printf(
					"piece[%3zu] kind=%d %-16s scale=%.1f wit=%.2f loc (%8.2f,%8.2f)->(%8.2f,%8.2f) "
					"t0(%8.2f,%8.2f) b0(%8.2f,%8.2f) t1(%8.2f,%8.2f) b1(%8.2f,%8.2f)\n",
					p, (int)r.kind, r.typeName.c_str(), r.scale, r.widthInTex, r.x0, r.y0, r.x1, r.y1, r.t0x, r.t0y,
					r.b0x, r.b0y, r.t1x, r.t1y, r.b1x, r.b1y);
			}
			editor.shutdown();
			return 0;
		}
	}

	VulkanHost host;
	if (!host.init("WorldBuilder Linux (ImGui)", 1280, 800))
	{
		fprintf(stderr, "VulkanHost::init failed\n");
		editor.shutdown();
		return 1;
	}

	MapViewport viewport;
	if (!viewport.init(host))
	{
		fprintf(stderr, "MapViewport::init failed (shaders in %s?)\n", WB_SHADER_DIR);
		host.shutdown();
		editor.shutdown();
		return 1;
	}

	MapDocument doc;
	MainUi::State ui = {};
	{
		const std::string mapsDir = MapOpen::defaultMapsDirectory();
		snprintf(ui.pathBuf, sizeof(ui.pathBuf), "%s", mapsDir.c_str());
	}

	const char *cliMap = NULL;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--map") == 0 && i + 1 < argc)
		{
			cliMap = argv[++i];
			break;
		}
		if (argv[i][0] != '-')
		{
			const char *dot = strrchr(argv[i], '.');
			if (dot && strcasecmp(dot, ".map") == 0)
			{
				cliMap = argv[i];
				break;
			}
		}
	}

	if (cliMap)
	{
		snprintf(ui.pathBuf, sizeof(ui.pathBuf), "%s", cliMap);
		if (doc.load(AsciiString(ui.pathBuf)))
		{
			refreshViewport(host, doc, viewport);
			fprintf(stderr, "Opened map: %s (%dx%d)\n", ui.pathBuf, doc.width(), doc.height());
		}
		else
		{
			fprintf(stderr, "Failed to open %s: %s\n", ui.pathBuf, doc.lastError().str());
		}
	}
	else if (doc.createBlank(kDefaultPlayableX, kDefaultPlayableY, kDefaultHeight, kDefaultBorder))
	{
		if (refreshViewport(host, doc, viewport))
			fprintf(stderr, "Blank template 3D view: %dx%d (playable %dx%d)\n", doc.width(), doc.height(),
				kDefaultPlayableX, kDefaultPlayableY);
	}
	else
	{
		fprintf(stderr, "createBlank failed: %s\n", doc.lastError().str());
	}

	const ImVec4 clearColor = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);

	while (!host.shouldQuit())
	{
		host.pollEvents();

		if (MapOpen::takePendingPath(ui.pathBuf, sizeof(ui.pathBuf)))
			ui.openClicked = true;

		if (!host.beginFrame())
			continue;

		if (viewport.meshReady())
			viewport.render(host, ui.mapViewW, ui.mapViewH);

		MainUi::draw(ui, doc, viewport, host.window());

		if (ui.requestQuit)
			host.requestQuit();

		if (ui.newClicked)
		{
			ui.newClicked = false;
			if (doc.createBlank(kDefaultPlayableX, kDefaultPlayableY, kDefaultHeight, kDefaultBorder))
				refreshViewport(host, doc, viewport);
		}

		if (ui.openClicked)
		{
			ui.openClicked = false;
			if (doc.load(AsciiString(ui.pathBuf)))
			{
				refreshViewport(host, doc, viewport);
				fprintf(stderr, "Opened map: %s (%dx%d)\n", ui.pathBuf, doc.width(), doc.height());
				ui.mapsListDirty = true;
			}
			else
			{
				fprintf(stderr, "Open failed: %s\n", doc.lastError().str());
			}
		}

		host.endFrame(clearColor);
	}

	viewport.destroy(host);
	host.shutdown();
	editor.shutdown();
	return 0;
}
