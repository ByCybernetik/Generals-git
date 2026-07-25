#include "MainUi.h"
#include "MapDocument.h"
#include "MapViewport.h"
#include "MapOpen.h"
#include "imgui.h"

#include <SDL3/SDL.h>

namespace {

void applyFixedLayout()
{
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const float menuBarH = ImGui::GetFrameHeight();
	const ImVec2 workPos(vp->WorkPos.x, vp->WorkPos.y + menuBarH);
	const ImVec2 workSize(vp->WorkSize.x, vp->WorkSize.y - menuBarH);
	const float leftW = workSize.x * 0.26f;
	const float infoH = workSize.y * 0.32f;
	const float openMapH = workSize.y - infoH;

	ImGui::SetNextWindowPos(workPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(leftW, openMapH), ImGuiCond_Always);
}

void applyMapInfoLayout()
{
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const float menuBarH = ImGui::GetFrameHeight();
	const ImVec2 workPos(vp->WorkPos.x, vp->WorkPos.y + menuBarH);
	const ImVec2 workSize(vp->WorkSize.x, vp->WorkSize.y - menuBarH);
	const float leftW = workSize.x * 0.26f;
	const float infoH = workSize.y * 0.32f;
	const float openMapH = workSize.y - infoH;

	ImGui::SetNextWindowPos(ImVec2(workPos.x, workPos.y + openMapH), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(leftW, infoH), ImGuiCond_Always);
}

void applyMapViewLayout()
{
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const float menuBarH = ImGui::GetFrameHeight();
	const ImVec2 workPos(vp->WorkPos.x, vp->WorkPos.y + menuBarH);
	const ImVec2 workSize(vp->WorkSize.x, vp->WorkSize.y - menuBarH);
	const float leftW = workSize.x * 0.26f;

	ImGui::SetNextWindowPos(ImVec2(workPos.x + leftW, workPos.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(workSize.x - leftW, workSize.y), ImGuiCond_Always);
}

} // namespace

void MainUi::draw(State &state, MapDocument &doc, MapViewport &viewport, SDL_Window *window)
{
	ImGuiIO &io = ImGui::GetIO();

	if (state.mapsListDirty)
	{
		MapOpen::refreshUserList(state.userMaps);
		MapOpen::refreshOfficialList(state.officialMaps);
		state.mapsListDirty = false;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Map", "Ctrl+N"))
				state.newClicked = true;
			if (ImGui::MenuItem("Open Map...", "Ctrl+O"))
				state.browseClicked = true;
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Alt+F4"))
				state.requestQuit = true;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
		state.browseClicked = true;
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
		state.newClicked = true;

	/* Layout mirrors original OpenMap dialog: System/User radios + list + open. */
	applyFixedLayout();
	ImGui::Begin("Open Map", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	if (ImGui::Button("Browse..."))
		state.browseClicked = true;
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
		state.mapsListDirty = true;
	ImGui::SameLine();
	if (ImGui::Button("New Template"))
		state.newClicked = true;

	ImGui::Separator();
	if (ImGui::RadioButton("System Maps", state.mapsTab == 1))
		state.mapsTab = 1;
	ImGui::SameLine();
	if (ImGui::RadioButton("User Maps", state.mapsTab == 0))
		state.mapsTab = 0;

	const std::vector<MapOpen::MapEntry> &maps =
		(state.mapsTab == 1) ? state.officialMaps : state.userMaps;
	if (state.mapsTab == 1)
		ImGui::Text("Official (%d) — maps.big", (int)maps.size());
	else
		ImGui::TextWrapped("User — %s", MapOpen::defaultMapsDirectory().c_str());

	/* Leave room for path + Open + status (~100px). */
	const float bottomReserve = 110.f;
	const ImVec2 listSize(0.f, std::max(80.f, ImGui::GetContentRegionAvail().y - bottomReserve));
	if (ImGui::BeginChild("open_map_list", listSize, ImGuiChildFlags_Borders))
	{
		if (maps.empty())
		{
			ImGui::TextDisabled(state.mapsTab == 1 ? "(no system maps found)" : "(no user maps found)");
		}
		for (size_t i = 0; i < maps.size(); ++i)
		{
			const MapOpen::MapEntry &e = maps[i];
			ImGui::PushID((int)i);
			const bool selected = (state.pathBuf[0] && e.path == state.pathBuf);
			if (ImGui::Selectable(e.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
			{
				snprintf(state.pathBuf, sizeof(state.pathBuf), "%s", e.path.c_str());
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					state.openClicked = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", e.path.c_str());
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	ImGui::InputText("##map_path", state.pathBuf, sizeof(state.pathBuf));
	if (ImGui::Button("Open", ImVec2(-1, 0)))
		state.openClicked = true;

	ImGui::TextWrapped("%s", doc.isLoaded() ? doc.path().str() : "(no map loaded)");
	if (!doc.lastError().isEmpty())
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", doc.lastError().str());
	ImGui::End();

	applyMapInfoLayout();
	ImGui::Begin("Map Info", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	if (doc.isLoaded())
	{
		ImGui::Text("Size: %d x %d", doc.width(), doc.height());
		ImGui::Text("Border: %d", doc.borderSize());
		ImGui::Text("Playable: %d x %d", doc.width() - 2 * doc.borderSize(),
			doc.height() - 2 * doc.borderSize());
	}
	else
	{
		ImGui::TextUnformatted("No map.");
	}
	ImGui::Text("FPS: %.1f", io.Framerate);
	ImGui::TextUnformatted("LMB/RMB pan  MMB rotate  Wheel zoom");
	ImGui::TextUnformatted("MMB click = reset camera");
	{
		bool showModels = viewport.showModels();
		if (ImGui::Checkbox("Show Models", &showModels))
			viewport.setShowModels(showModels);
		bool showWater = viewport.showWater();
		if (ImGui::Checkbox("Show Water", &showWater))
			viewport.setShowWater(showWater);
	}
	ImGui::Text("Objects: %zu", doc.objects().size());
	ImGui::End();

	applyMapViewLayout();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Map View", nullptr,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	{
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x >= 64.f && avail.y >= 64.f)
		{
			state.mapViewW = (int)avail.x;
			state.mapViewH = (int)avail.y;
		}

		const ImVec2 imageSize((float)state.mapViewW, (float)state.mapViewH);
		if (viewport.hasTexture())
		{
			ImGui::Image(ImTextureRef(viewport.textureId()), imageSize);
			viewport.handleMapViewInput(io, ImGui::IsItemHovered());
		}
		else
		{
			ImGui::Dummy(imageSize);
			ImGui::SetCursorPos(ImVec2(12, 12));
			ImGui::TextUnformatted(viewport.meshReady() ? "Rendering map..." : "3D map view not ready.");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	if (state.browseClicked)
	{
		state.browseClicked = false;
		const std::string mapsDir = MapOpen::defaultMapsDirectory();
		MapOpen::showOpenDialog(window, mapsDir.c_str());
	}
}
