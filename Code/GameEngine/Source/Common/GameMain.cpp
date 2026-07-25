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

// GameMain.cpp
// The main entry point for the game
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
#include "sdl3_host.h"
#endif

/**
 * This is the entry point for the game system.
 */
void GameMain( int argc, char *argv[] )
{
	// initialize the game engine using factory function
	TheGameEngine = CreateGameEngine();
	TheGameEngine->init(argc, argv);

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	Platform_Pump_Events();
#endif

	// run it
	TheGameEngine->execute();

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	// Hide/destroy SDL window before lengthy engine teardown (D3D/Vulkan).
	Platform_Pre_Shutdown();
#endif

	// since execute() returned, we are exiting the game
	delete TheGameEngine;
	TheGameEngine = NULL;

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
	Platform_Shutdown();
#endif

}

