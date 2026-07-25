/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	INI name tables for DamageType / DeathType. Include after DEFINE_DAMAGE_NAMES
**	and/or DEFINE_DEATH_NAMES (see Weapon.cpp, INI.cpp).
*/

#ifndef __DAMAGE_NAME_TABLES_H_
#define __DAMAGE_NAME_TABLES_H_

#ifdef DEFINE_DAMAGE_NAMES
static const char *TheDamageNames[] =
{
	"EXPLOSION",
	"CRUSH",
	"ARMOR_PIERCING",
	"SMALL_ARMS",
	"GATTLING",
	"RADIATION",
	"FLAME",
	"LASER",
	"SNIPER",
	"POISON",
	"HEALING",
	"UNRESISTABLE",
	"WATER",
	"DEPLOY",
	"SURRENDER",
	"HACK",
	"KILL_PILOT",
	"PENALTY",
	"FALLING",
	"MELEE",
	"DISARM",
	"HAZARD_CLEANUP",
	"PARTICLE_BEAM",
	"TOPPLING",
	"INFANTRY_MISSILE",
	"AURORA_BOMB",
	"LAND_MINE",
	"JET_MISSILES",
	"STEALTHJET_MISSILES",
	"MOLOTOV_COCKTAIL",
	"COMANCHE_VULCAN",
	"FLESHY_SNIPER",

	NULL
};
#endif

#ifdef DEFINE_DEATH_NAMES
static const char *TheDeathNames[] =
{
	"NORMAL",
	"NONE",
	"CRUSHED",
	"BURNED",
	"EXPLODED",
	"POISONED",
	"TOPPLED",
	"FLOODED",
	"SUICIDED",
	"LASERED",
	"DETONATED",
	"SPLATTED",
	"POISONED_BETA",

	"EXTRA_2",
	"EXTRA_3",
	"EXTRA_4",
	"EXTRA_5",
	"EXTRA_6",
	"EXTRA_7",
	"EXTRA_8",

	NULL
};
#endif

#endif
