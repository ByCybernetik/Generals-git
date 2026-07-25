/*
**	INI name table for CommandSourceMask. Include after DEFINE_COMMANDSOURCEMASK_NAMES.
*/

#ifndef __COMMAND_SOURCE_MASK_NAME_TABLES_H_
#define __COMMAND_SOURCE_MASK_NAME_TABLES_H_

#ifdef DEFINE_COMMANDSOURCEMASK_NAMES
static const char *TheCommandSourceMaskNames[] =
{
	"FROM_PLAYER",
	"FROM_SCRIPT",
	"FROM_AI",

	NULL
};
#endif

#endif
