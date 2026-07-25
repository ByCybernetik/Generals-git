#pragma once
#include "../gsi_types.h"
#include "gstats.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  pd_public_ro,
  pd_public_rw,
  pd_private_ro,
  pd_private_rw
} persisttype_t;
typedef void (*PersAuthCallbackFn)(int localid, int profileid, int authenticated, gsi_char *errmsg, void *instance);
typedef void (*PersDataCallbackFn)(int localid, int profileid, persisttype_t type, int index, int success, int modified, char *data, int len, void *instance);
typedef void (*PersDataSaveCallbackFn)(int localid, int profileid, persisttype_t type, int index, int success, int modified, void *instance);
int PersistThink(void);
void PreAuthenticatePlayerCD(int localid, const gsi_char *nick, const char *keyhash, const char *challengeresponse, PersAuthCallbackFn callback, void *instance);
void PreAuthenticatePlayerPM(int localid, int profileid, const char *challengeresponse, PersAuthCallbackFn callback, void *instance);
void GetPersistDataValues(int localid, int profileid, persisttype_t type, int index, gsi_char *keys, PersDataCallbackFn callback, void *instance);
void SetPersistDataValues(int localid, int profileid, persisttype_t type, int index, const gsi_char *keyvalues, PersDataSaveCallbackFn callback, void *instance);
#ifdef __cplusplus
}
#endif
