/*
** Linux stub for legacy GameSpy.cpp / GameSpyGameInfo.cpp (obsolete single-thread path).
** Online play uses PeerThread + StagingRoomGameInfo instead.
*/

#include "PreRTS.h"

#include "GameNetwork/GameSpy.h"

GameSpyChatInterface *TheGameSpyChat = NULL;

GameSpyChatInterface *createGameSpyChat(void)
{
	return NULL;
}
