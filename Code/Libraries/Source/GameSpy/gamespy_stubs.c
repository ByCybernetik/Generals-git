/*
 * Linux stub for Generals-era GameSpy SDK API.
 */
#include "Peer/Peer.h"
#include "GP/GP.h"
#include "ghttp/ghttp.h"
#include "gstats/gpersist.h"
#include "serverbrowsing/sb_serverbrowsing.h"
#include "qr2/qr2regkeys.h"

#include <string.h>
#include <unistd.h>

const char *qr2_registered_key_list[MAX_REGISTERED_KEYS] = {0};

char gcd_secret_key[256];
char gcd_gamename[256];

PEER peerInitialize(PEERCallbacks *callbacks) { (void)callbacks; return NULL; }
void peerShutdown(PEER peer) { (void)peer; }
void peerThink(PEER peer) { (void)peer; }
PEERBool peerIsConnected(PEER peer) { (void)peer; return PEERFalse; }
void peerConnect(PEER peer, const gsi_char *nick, int profileID, peerNickErrorCallback nickErrorCallback, peerConnectCallback connectCallback, void *param, PEERBool blocking)
{ (void)peer; (void)nick; (void)profileID; (void)nickErrorCallback; (void)connectCallback; (void)param; (void)blocking; }
void peerDisconnect(PEER peer) { (void)peer; }
void peerRetryWithNick(PEER peer, const gsi_char *nick) { (void)peer; (void)nick; }
void peerAuthenticateCDKey(PEER peer, const gsi_char *cdkey, peerAuthenticateCDKeyCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)cdkey; (void)callback; (void)param; (void)blocking; }
PEERBool peerSetTitle(PEER peer, const gsi_char *title, const gsi_char *qrSecretKey, const gsi_char *sbName, const gsi_char *sbSecretKey, int sbGameVersion, PEERBool pingRooms[NumRooms], PEERBool crossPingRooms[NumRooms])
{ (void)peer; (void)title; (void)qrSecretKey; (void)sbName; (void)sbSecretKey; (void)sbGameVersion; (void)pingRooms; (void)crossPingRooms; return PEERFalse; }
void peerJoinTitleRoom(PEER peer, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)password; (void)callback; (void)param; (void)blocking; }
void peerJoinGroupRoom(PEER peer, int groupID, peerJoinRoomCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)groupID; (void)callback; (void)param; (void)blocking; }
void peerJoinStagingRoom(PEER peer, SBServer server, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)server; (void)password; (void)callback; (void)param; (void)blocking; }
void peerLeaveRoom(PEER peer, RoomType roomType, const gsi_char *reason) { (void)peer; (void)roomType; (void)reason; }
void peerListGroupRooms(PEER peer, const gsi_char *fields, peerListGroupRoomsCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)fields; (void)callback; (void)param; (void)blocking; }
void peerCreateStagingRoom(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)name; (void)maxPlayers; (void)password; (void)callback; (void)param; (void)blocking; }
void peerCreateStagingRoomWithSocket(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char password[PEER_PASSWORD_LEN], SOCKET socket, unsigned short port, peerJoinRoomCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)name; (void)maxPlayers; (void)password; (void)socket; (void)port; (void)callback; (void)param; (void)blocking; }
void peerEnumPlayers(PEER peer, RoomType roomType, peerEnumPlayersCallback callback, void *param)
{ (void)peer; (void)roomType; (void)callback; (void)param; }
void peerMessagePlayer(PEER peer, const gsi_char *nick, const gsi_char *message, MessageType messageType)
{ (void)peer; (void)nick; (void)message; (void)messageType; }
void peerMessageRoom(PEER peer, RoomType roomType, const gsi_char *message, MessageType messageType)
{ (void)peer; (void)roomType; (void)message; (void)messageType; }
void peerUTMPlayer(PEER peer, const gsi_char *nick, const gsi_char *command, const gsi_char *parameters, PEERBool authenticate)
{ (void)peer; (void)nick; (void)command; (void)parameters; (void)authenticate; }
void peerUTMRoom(PEER peer, RoomType roomType, const gsi_char *command, const gsi_char *parameters, PEERBool authenticate)
{ (void)peer; (void)roomType; (void)command; (void)parameters; (void)authenticate; }
void peerSetReady(PEER peer, PEERBool ready) { (void)peer; (void)ready; }
void peerStartGame(PEER peer, const gsi_char *message, int reportingOptions) { (void)peer; (void)message; (void)reportingOptions; }
void peerStopGame(PEER peer) { (void)peer; }
void peerStartListingGames(PEER peer, const unsigned char *fields, int numFields, const gsi_char *filter, peerListingGamesCallback callback, void *param)
{ (void)peer; (void)fields; (void)numFields; (void)filter; (void)callback; (void)param; }
void peerStopListingGames(PEER peer) { (void)peer; }
void peerUpdateGame(PEER peer, SBServer server, PEERBool fullUpdate) { (void)peer; (void)server; (void)fullUpdate; }
PEERBool peerGetPlayerInfoNoWait(PEER peer, const gsi_char *nick, unsigned int *IP, int *profileID)
{ (void)peer; (void)nick; if (IP) *IP = 0; if (profileID) *profileID = 0; return PEERFalse; }
void peerGetPlayerProfileID(PEER peer, const gsi_char *nick, peerGetPlayerProfileIDCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)nick; (void)callback; (void)param; (void)blocking; }
PEERBool peerGetPlayerFlags(PEER peer, const gsi_char *nick, RoomType roomType, int *flags)
{ (void)peer; (void)nick; (void)roomType; if (flags) *flags = 0; return PEERFalse; }
unsigned int peerGetPublicIP(PEER peer) { (void)peer; return 0; }
const gsi_char *peerGetGlobalWatchKey(PEER peer, const gsi_char *nick, const gsi_char *key)
{ (void)peer; (void)nick; (void)key; return ""; }
void peerGetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const gsi_char **keys, peerGetRoomKeysCallback callback, void *param, PEERBool blocking)
{ (void)peer; (void)roomType; (void)nick; (void)num; (void)keys; (void)callback; (void)param; (void)blocking; }
void peerSetGlobalKeys(PEER peer, int num, const gsi_char **keys, const gsi_char **values)
{ (void)peer; (void)num; (void)keys; (void)values; }
void peerSetGlobalWatchKeys(PEER peer, RoomType roomType, int num, const gsi_char **keys, PEERBool addKeys)
{ (void)peer; (void)roomType; (void)num; (void)keys; (void)addKeys; }
void peerSetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const gsi_char **keys, const gsi_char **values)
{ (void)peer; (void)roomType; (void)nick; (void)num; (void)keys; (void)values; }
void peerSetRoomWatchKeys(PEER peer, RoomType roomType, int num, const gsi_char **keys, PEERBool addKeys)
{ (void)peer; (void)roomType; (void)num; (void)keys; (void)addKeys; }
void peerParseQuery(PEER peer, char *query, int len, struct sockaddr *sender)
{ (void)peer; (void)query; (void)len; (void)sender; }
void peerStateChanged(PEER peer) { (void)peer; }

void chatSetLocalIP(unsigned long preferredIP) { (void)preferredIP; }

GPResult gpInitialize(GPConnection *connection, int productID)
{ (void)productID; if (connection) *connection = NULL; return GP_NO_ERROR; }
void gpDestroy(GPConnection *connection) { if (connection) *connection = NULL; }
GPResult gpProcess(GPConnection *connection) { (void)connection; return GP_NO_ERROR; }
GPResult gpConnect(GPConnection *connection, const gsi_char nick[GP_NICK_LEN], const gsi_char email[GP_EMAIL_LEN], const gsi_char password[GP_PASSWORD_LEN], GPEnum firewall, GPEnum blocking, GPCallback callback, void *param)
{ (void)connection; (void)nick; (void)email; (void)password; (void)firewall; (void)blocking; (void)callback; (void)param; return GP_NO_ERROR; }
GPResult gpConnectNewUser(GPConnection *connection, const gsi_char nick[GP_NICK_LEN], const gsi_char email[GP_EMAIL_LEN], const gsi_char password[GP_PASSWORD_LEN], GPEnum firewall, GPEnum blocking, GPCallback callback, void *param)
{ (void)connection; (void)nick; (void)email; (void)password; (void)firewall; (void)blocking; (void)callback; (void)param; return GP_NO_ERROR; }
void gpDisconnect(GPConnection *connection) { (void)connection; }
GPResult gpIsConnected(GPConnection *connection, GPEnum *connected)
{ (void)connection; if (connected) *connected = GP_NOT_CONNECTED; return GP_NO_ERROR; }
GPResult gpSetCallback(GPConnection *connection, GPEnum func, GPCallback callback, void *param)
{ (void)connection; (void)func; (void)callback; (void)param; return GP_NO_ERROR; }
GPResult gpSetStatus(GPConnection *connection, GPEnum status, const gsi_char statusString[GP_STATUS_STRING_LEN], const gsi_char locationString[GP_LOCATION_STRING_LEN])
{ (void)connection; (void)status; (void)statusString; (void)locationString; return GP_NO_ERROR; }
GPResult gpSetInfoMask(GPConnection *connection, GPEnum mask)
{ (void)connection; (void)mask; return GP_NO_ERROR; }
GPResult gpGetInfo(GPConnection *connection, GPProfile profile, GPEnum checkCache, GPEnum blocking, GPCallback callback, void *param)
{ (void)connection; (void)profile; (void)checkCache; (void)blocking; (void)callback; (void)param; return GP_NO_ERROR; }
GPResult gpGetBuddyStatus(GPConnection *connection, int index, GPBuddyStatus *status)
{ (void)connection; (void)index; if (status) memset(status, 0, sizeof(*status)); return GP_NO_ERROR; }
GPResult gpGetBuddyIndex(GPConnection *connection, GPProfile profile, int *index)
{ (void)connection; (void)profile; if (index) *index = -1; return GP_NO_ERROR; }
GPResult gpSendBuddyRequest(GPConnection *connection, GPProfile profile, const gsi_char reason[GP_REASON_LEN])
{ (void)connection; (void)profile; (void)reason; return GP_NO_ERROR; }
GPResult gpAuthBuddyRequest(GPConnection *connection, GPProfile profile)
{ (void)connection; (void)profile; return GP_NO_ERROR; }
GPResult gpDenyBuddyRequest(GPConnection *connection, GPProfile profile)
{ (void)connection; (void)profile; return GP_NO_ERROR; }
GPResult gpDeleteBuddy(GPConnection *connection, GPProfile profile)
{ (void)connection; (void)profile; return GP_NO_ERROR; }
GPResult gpDeleteProfile(GPConnection *connection, GPCallback callback, void *param)
{ (void)connection; (void)callback; (void)param; return GP_NO_ERROR; }
GPResult gpSendBuddyMessage(GPConnection *connection, GPProfile profile, const gsi_char *message)
{ (void)connection; (void)profile; (void)message; return GP_NO_ERROR; }

void ghttpStartup(void) {}
void ghttpCleanup(void) {}
void ghttpThink(void) {}
GHTTPRequest ghttpGet(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback cb, void *param)
{ (void)URL; (void)blocking; (void)cb; (void)param; return -1; }
GHTTPRequest ghttpHead(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback cb, void *param)
{ (void)URL; (void)blocking; (void)cb; (void)param; return -1; }
const char *ghttpGetHeaders(GHTTPRequest request) { (void)request; return ""; }
GHTTPBool ghttpSetProxy(const char *server) { (void)server; return GHTTPFalse; }

int PersistThink(void) { return 0; }
void PreAuthenticatePlayerCD(int localid, const gsi_char *nick, const char *keyhash, const char *challengeresponse, PersAuthCallbackFn callback, void *instance)
{ (void)localid; (void)nick; (void)keyhash; (void)challengeresponse; (void)callback; (void)instance; }
void PreAuthenticatePlayerPM(int localid, int profileid, const char *challengeresponse, PersAuthCallbackFn callback, void *instance)
{ (void)localid; (void)profileid; (void)challengeresponse; (void)callback; (void)instance; }
void GetPersistDataValues(int localid, int profileid, persisttype_t type, int index, gsi_char *keys, PersDataCallbackFn callback, void *instance)
{ (void)localid; (void)profileid; (void)type; (void)index; (void)keys; (void)callback; (void)instance; }
void SetPersistDataValues(int localid, int profileid, persisttype_t type, int index, const gsi_char *keyvalues, PersDataSaveCallbackFn callback, void *instance)
{ (void)localid; (void)profileid; (void)type; (void)index; (void)keyvalues; (void)callback; (void)instance; }

void msleep(unsigned int msec) { usleep(msec * 1000u); }

int InitStatsConnection(int gameport) { (void)gameport; return GE_NOCONNECT; }
int IsStatsConnected(void) { return 0; }
void CloseStatsConnection(void) {}
char *GetChallenge(statsgame_t game) { (void)game; return (char *)"NULLGAME"; }
char *GenerateAuth(const char *challenge, const gsi_char *password, char response[33])
{
	(void)challenge; (void)password;
	if (response) { memset(response, '0', 32); response[32] = 0; }
	return response;
}
statsgame_t NewGame(int usebuckets) { (void)usebuckets; return NULL; }
void FreeGame(statsgame_t game) { (void)game; }
int SendGameSnapShot(statsgame_t game, const gsi_char *snapshot, int final)
{ (void)game; (void)snapshot; (void)final; return GE_NOCONNECT; }

SBBool SBServerHasFullKeys(SBServer server) { (void)server; return SBFalse; }
SBBool SBServerHasBasicKeys(SBServer server) { (void)server; return SBFalse; }
const gsi_char *SBServerGetStringValue(SBServer server, const gsi_char *keyname, const gsi_char *def)
{ (void)server; (void)keyname; return def; }
int SBServerGetIntValue(SBServer server, const gsi_char *key, int idefault)
{ (void)server; (void)key; return idefault; }
unsigned int SBServerGetPrivateInetAddress(SBServer server) { (void)server; return 0; }
unsigned short SBServerGetPrivateQueryPort(SBServer server) { (void)server; return 0; }
unsigned int SBServerGetPublicInetAddress(SBServer server) { (void)server; return 0; }
const gsi_char *SBServerGetPlayerStringValue(SBServer server, int playernum, const gsi_char *key, const gsi_char *sdefault)
{ (void)server; (void)playernum; (void)key; return sdefault; }
int SBServerGetPlayerIntValue(SBServer server, int playernum, const gsi_char *key, int idefault)
{ (void)server; (void)playernum; (void)key; return idefault; }
void SBServerEnumKeys(SBServer server, SBServerKeyEnumFn KeyFn, void *instance)
{ (void)server; (void)KeyFn; (void)instance; }

gsi_bool qr2_buffer_add(qr2_buffer_t outbuf, const gsi_char *value) { (void)outbuf; (void)value; return gsi_false; }
gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value) { (void)outbuf; (void)value; return gsi_false; }
gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid) { (void)keybuffer; (void)keyid; return gsi_false; }
void qr2_register_key(int keyid, const gsi_char *key) { (void)keyid; (void)key; }
