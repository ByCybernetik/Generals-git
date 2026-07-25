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

// FILE: CommandXlat.h ///////////////////////////////////////////////////////////
// Author: Steven Johnson, Dec 2001

#pragma once

#ifndef _H_CommandXlat
#define _H_CommandXlat

#include "GameClient/InGameUI.h"
#include "GameClient/ViewFilterTypes.h"
#include "Common/SpecialPowerType.h"

enum GUICommandType;

//-----------------------------------------------------------------------------
class CommandTranslator : public GameMessageTranslator
{
public:

	CommandTranslator();
	~CommandTranslator();

	enum CommandEvaluateType { DO_COMMAND, DO_HINT, EVALUATE_ONLY };


	GameMessage::Type evaluateForceAttack( Drawable *draw, const Coord3D *pos, CommandEvaluateType type );
	GameMessage::Type evaluateContextCommand( Drawable *draw, const Coord3D *pos, CommandEvaluateType type );

private:

	Int m_objective;
	Bool m_teamExists;				///< is there a currently selected "team"?
  
 	// these are for determining if a drag occurred or it wasjust a sloppy click
 	ICoord2D m_mouseRightDragAnchor;		// the location of a possible mouse drag start
 	ICoord2D m_mouseRightDragLift;			// the location of a possible mouse drag end
 	UnsignedInt m_mouseRightDown;	// when the mouse down happened
 	UnsignedInt m_mouseRightUp;		// when the mouse up happened

  	GameMessage::Type createMoveToLocationMessage( Drawable *draw, const Coord3D *dest, CommandEvaluateType commandType );
	GameMessage::Type createAttackMessage( Drawable *draw, Drawable *other, CommandEvaluateType commandType );
	GameMessage::Type createEnterMessage( Drawable *enter, CommandEvaluateType commandType );
	GameMessage::Type issueMoveToLocationCommand( const Coord3D *pos, Drawable *drawableInWay, CommandEvaluateType commandType );
	GameMessage::Type issueAttackCommand( Drawable *target, CommandEvaluateType commandType, GUICommandType command = (GUICommandType)0 );
	GameMessage::Type issueSpecialPowerCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos, Object* ignoreSelObj );
	GameMessage::Type issueFireWeaponCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos );
	GameMessage::Type issueCombatDropCommand( const CommandButton *command, CommandEvaluateType commandType, Drawable *target, const Coord3D *pos );

	virtual GameMessageDisposition translateGameMessage(const GameMessage *msg);
};

class PickAndPlayInfo
{
public:
	PickAndPlayInfo::PickAndPlayInfo(); //INITIALIZE THE CONSTRUCTOR IN CPP

	Bool						m_air;					//Are we attacking an airborned target?
	Drawable				*m_drawTarget;	//Do we have an override draw target?
	WeaponSlotType	*m_weaponSlot;	//Are we forcing a specific weapon slot? NULL if unspecified.
	SpecialPowerType m_specialPowerType; //Which special power are use using? SPECIAL_INVALID if unspecified.
};

extern void pickAndPlayUnitVoiceResponse( const DrawableList *list, GameMessage::Type msgType, PickAndPlayInfo *info = NULL );

#endif
