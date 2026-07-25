#pragma once
#include "../gsi_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { SBFalse = 0, SBTrue = 1 } SBBool;
typedef struct _SBServer *SBServer;
typedef void (*SBServerKeyEnumFn)(gsi_char *key, gsi_char *value, void *instance);
SBBool SBServerHasFullKeys(SBServer server);
SBBool SBServerHasBasicKeys(SBServer server);
const gsi_char *SBServerGetStringValue(SBServer server, const gsi_char *keyname, const gsi_char *def);
int SBServerGetIntValue(SBServer server, const gsi_char *key, int idefault);
unsigned int SBServerGetPrivateInetAddress(SBServer server);
unsigned short SBServerGetPrivateQueryPort(SBServer server);
unsigned int SBServerGetPublicInetAddress(SBServer server);
const gsi_char *SBServerGetPlayerStringValue(SBServer server, int playernum, const gsi_char *key, const gsi_char *sdefault);
int SBServerGetPlayerIntValue(SBServer server, int playernum, const gsi_char *key, int idefault);
void SBServerEnumKeys(SBServer server, SBServerKeyEnumFn KeyFn, void *instance);
#ifdef __cplusplus
}
#endif
