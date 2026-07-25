#include "PreRTS.h"
#include "EditorApp.h"

#include "Common/Debug.h"
#include "Common/FileSystem.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/LocalFileSystem.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/NameKeyGenerator.h"
#include "Common/SubsystemInterface.h"
#include "Common/TerrainTypes.h"
#include "Common/ThingFactory.h"
#include "Common/ModuleFactory.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "Common/MultiplayerSettings.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Anim2D.h"
#include "GameClient/GameText.h"
#include "GameClient/Water.h"
#include "Common/INI.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Armor.h"
#include "GameLogic/CaveSystem.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/Damage.h"
#include "Common/DamageFX.h"
#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/GameClient/W3DFileSystem.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "Win32Device/Common/Win32LocalFileSystem.h"
#include "Win32Device/Common/Win32BIGFileSystem.h"
#include "sdlaudio_manager.h"

#include "assetmgr.h"
#include "ww3d.h"

#include <unistd.h>
#include <stdio.h>

/* Required by various GameEngine units when linking the editor. */
HWND ApplicationHWnd = NULL;
HINSTANCE ApplicationHInstance = NULL;
char *gAppPrefix = (char *)"wb_linux_";
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";

static SubsystemInterfaceList s_subsystemList;
static W3DAssetManager *s_assetManager = NULL;

template <class SUBSYSTEM>
static void initSubsystem(SUBSYSTEM *&sysref, SUBSYSTEM *sys, const char *path1 = NULL,
	const char *path2 = NULL, const char *dirpath = NULL)
{
	sysref = sys;
	s_subsystemList.initSubsystem(sys, path1, path2, dirpath, NULL);
}

bool EditorApp::init(const char *gameDataDir)
{
	DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
	initMemoryManager();

	if (gameDataDir && gameDataDir[0])
	{
		if (chdir(gameDataDir) != 0)
		{
			fprintf(stderr, "EditorApp: chdir(%s) failed\n", gameDataDir);
			return false;
		}
	}

	TheNameKeyGenerator = new NameKeyGenerator;
	TheNameKeyGenerator->init();

	TheFileSystem = new FileSystem;

	initSubsystem(TheLocalFileSystem, (LocalFileSystem *)new Win32LocalFileSystem);
	initSubsystem(TheArchiveFileSystem, (ArchiveFileSystem *)new Win32BIGFileSystem);
	initSubsystem(TheWritableGlobalData, new GlobalData(), "Data\\INI\\Default\\GameData.ini",
		"Data\\INI\\GameData.ini");
	initSubsystem(TheGameText, CreateGameTextInterface());
	initSubsystem(TheAudio, (AudioManager *)(new SDLAudioManager()));
	initSubsystem(TheScienceStore, new ScienceStore(), "Data\\INI\\Default\\Science.ini", "Data\\INI\\Science.ini");
	initSubsystem(TheMultiplayerSettings, new MultiplayerSettings(), "Data\\INI\\Default\\Multiplayer.ini",
		"Data\\INI\\Multiplayer.ini");
	initSubsystem(TheTerrainTypes, new TerrainTypeCollection(), "Data\\INI\\Default\\Terrain.ini",
		"Data\\INI\\Terrain.ini");
	initSubsystem(TheTerrainRoads, new TerrainRoadCollection(), "Data\\INI\\Default\\Roads.ini",
		"Data\\INI\\Roads.ini");

	/* Must load Water.ini before water draw (same as WorldBuilder.cpp). */
	{
		INI ini;
		ini.load(AsciiString("Data\\INI\\Default\\Water.ini"), INI_LOAD_OVERWRITE, NULL);
		ini.load(AsciiString("Data\\INI\\Water.ini"), INI_LOAD_OVERWRITE, NULL);
	}

	initSubsystem(TheScriptEngine, (ScriptEngine *)(new ScriptEngine()));
	initSubsystem(TheModuleFactory, (ModuleFactory *)(new W3DModuleFactory()));
	initSubsystem(TheSidesList, new SidesList());
	initSubsystem(TheCaveSystem, new CaveSystem());
	initSubsystem(TheRankInfoStore, new RankInfoStore(), NULL, "Data\\INI\\Rank.ini");
	initSubsystem(ThePlayerTemplateStore, new PlayerTemplateStore(), "Data\\INI\\Default\\PlayerTemplate.ini",
		"Data\\INI\\PlayerTemplate.ini");
	initSubsystem(TheSpecialPowerStore, new SpecialPowerStore(), "Data\\INI\\Default\\SpecialPower.ini",
		"Data\\INI\\SpecialPower.ini");
	initSubsystem(TheParticleSystemManager, (ParticleSystemManager *)(new W3DParticleSystemManager()));
	initSubsystem(TheFXListStore, new FXListStore(), "Data\\INI\\Default\\FXList.ini", "Data\\INI\\FXList.ini");
	initSubsystem(TheWeaponStore, new WeaponStore(), NULL, "Data\\INI\\Weapon.ini");
	initSubsystem(TheObjectCreationListStore, new ObjectCreationListStore(),
		"Data\\INI\\Default\\ObjectCreationList.ini", "Data\\INI\\ObjectCreationList.ini");
	initSubsystem(TheLocomotorStore, new LocomotorStore(), NULL, "Data\\INI\\Locomotor.ini");
	initSubsystem(TheDamageFXStore, new DamageFXStore(), NULL, "Data\\INI\\DamageFX.ini");
	initSubsystem(TheArmorStore, new ArmorStore(), NULL, "Data\\INI\\Armor.ini");
	initSubsystem(TheThingFactory, new ThingFactory(), "Data\\INI\\Default\\Object.ini", NULL, "Data\\INI\\Object");
	initSubsystem(TheCrateSystem, new CrateSystem(), "Data\\INI\\Default\\Crate.ini", "Data\\INI\\Crate.ini");
	initSubsystem(TheUpgradeCenter, new UpgradeCenter, "Data\\INI\\Default\\Upgrade.ini", "Data\\INI\\Upgrade.ini");
	initSubsystem(TheAnim2DCollection, new Anim2DCollection);

	s_subsystemList.postProcessLoadAll();

	TheSubsystemList = &s_subsystemList;

	/* W3D assets for Create_Render_Obj (Art/W3D + Art/Textures via GameFileClass). */
	TheW3DFileSystem = new W3DFileSystem;
	s_assetManager = new W3DAssetManager;
	s_assetManager->Set_WW3D_Load_On_Demand(true);
	/* No DX device in MapViewport path — keep texture names, skip GPU upload. */
	WW3D::Enable_Texturing(false);

	if (TheWritableGlobalData)
		TheWritableGlobalData->m_useHalfHeightMap = false;

	m_ready = true;
	DEBUG_LOG(("WorldBuilderLinux EditorApp ready (cwd=%s)\n", gameDataDir ? gameDataDir : "."));
	return true;
}

void EditorApp::shutdown()
{
	if (!m_ready)
		return;

	if (s_assetManager)
	{
		s_assetManager->Free_Assets();
		delete s_assetManager;
		s_assetManager = NULL;
	}
	if (TheW3DFileSystem)
	{
		delete TheW3DFileSystem;
		TheW3DFileSystem = NULL;
	}

	s_subsystemList.shutdownAll();
	TheSubsystemList = NULL;
	if (TheFileSystem)
	{
		delete TheFileSystem;
		TheFileSystem = NULL;
	}
	if (TheNameKeyGenerator)
	{
		delete TheNameKeyGenerator;
		TheNameKeyGenerator = NULL;
	}
	m_ready = false;
}
