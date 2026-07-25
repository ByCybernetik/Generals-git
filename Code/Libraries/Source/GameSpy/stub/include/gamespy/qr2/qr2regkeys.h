#ifndef QR2REGKEYS_H
#define QR2REGKEYS_H

#include "../gscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GSI_UNICODE
#define qr2_register_key qr2_register_keyA
#else
#define qr2_register_key qr2_register_keyW
#endif

#define MAX_REGISTERED_KEYS 256

#define NUM_RESERVED_KEYS 100
#define HOSTNAME_KEY 1
#define GAMENAME_KEY 2
#define GAMEVER_KEY 3
#define HOSTPORT_KEY 4
#define MAPNAME_KEY 5
#define GAMETYPE_KEY 6
#define NUMPLAYERS_KEY 7
#define MAXPLAYERS_KEY 8

extern const char *qr2_registered_key_list[];

void qr2_register_keyA(int keyid, const char *key);
void qr2_register_keyW(int keyid, const unsigned short *key);

#ifdef __cplusplus
}
#endif

#endif
