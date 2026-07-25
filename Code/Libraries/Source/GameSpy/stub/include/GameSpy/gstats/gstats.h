#pragma once
#include "../gsi_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define GE_NOERROR 0
#define GE_NOSOCKET 1
#define GE_NODNS 2
#define GE_NOCONNECT 3
#define GE_BUSY 4
#define GE_DATAERROR 5
#define GE_CONNECTING 6
#define GE_TIMEDOUT 7

#define SNAP_UPDATE 0
#define SNAP_FINAL 1

typedef struct gamespy_stats_game_s *statsgame_t;

extern char gcd_secret_key[256];
extern char gcd_gamename[256];

void msleep(unsigned int msec);

int InitStatsConnection(int gameport);
int IsStatsConnected(void);
void CloseStatsConnection(void);
char *GetChallenge(statsgame_t game);
char *GenerateAuth(const char *challenge, const gsi_char *password, char response[33]);
statsgame_t NewGame(int usebuckets);
void FreeGame(statsgame_t game);
int SendGameSnapShot(statsgame_t game, const gsi_char *snapshot, int final);

#ifdef __cplusplus
}
#endif
