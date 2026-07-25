/*
**	INI name tables for WeaponSlotType. Include after DEFINE_WEAPONSLOTTYPE_NAMES.
*/

#ifndef __WEAPON_SLOT_TYPE_NAME_TABLES_H_
#define __WEAPON_SLOT_TYPE_NAME_TABLES_H_

#include "GameLogic/WeaponSetType.h"
#include "Common/INI.h"

#ifdef DEFINE_WEAPONSLOTTYPE_NAMES
static char *TheWeaponSlotTypeNames[] =
{
	"PRIMARY",
	"SECONDARY",
	"TERTIARY",

	NULL
};

static const LookupListRec TheWeaponSlotTypeNamesLookupList[] =
{
	{ "PRIMARY",		PRIMARY_WEAPON },
	{ "SECONDARY",	SECONDARY_WEAPON },
	{ "TERTIARY",		TERTIARY_WEAPON },

	{ NULL, 0	}// keep this last!
};
#endif

#endif
