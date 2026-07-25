/*
 * GameSpy SDK stubs for Generals Linux port.
 * No network I/O — offline / LAN-only build.
 */

#include "gp/gp.h"
#include "peer/peer.h"
#include "ghttp/ghttp.h"
#include "gstats/gstats.h"
#include "gstats/gpersist.h"
#include "qr2/qr2regkeys.h"
#include "gsplatformthread.h"

#include <pthread.h>
#include <string.h>
#include <time.h>

char gcd_secret_key[256];
char gcd_gamename[256];
char StatsServerHostname[64];

BucketFunc bucketfuncs[NUMOPS];
void *bopfuncs[NUMOPS][3];

static char s_challenge[] = "generals_stub_challenge";
static int s_stats_connected;
static unsigned int s_next_ghttp_request = 1;

struct PeerStub
{
	int alive;
};
static PeerStub s_peer;

struct GhttpPending
{
	int used;
	GHTTPRequest id;
	ghttpCompletedCallback callback;
	void *param;
};
static GhttpPending s_ghttp_pending[16];

static const char *s_qr2_keys[MAX_REGISTERED_KEYS];
const char *qr2_registered_key_list[MAX_REGISTERED_KEYS];

void gsiInitializeCriticalSection(GSICriticalSection *cs)
{
	pthread_mutex_init(cs, NULL);
}

void gsiDeleteCriticalSection(GSICriticalSection *cs)
{
	pthread_mutex_destroy(cs);
}

void gsiEnterCriticalSection(GSICriticalSection *cs)
{
	pthread_mutex_lock(cs);
}

void gsiLeaveCriticalSection(GSICriticalSection *cs)
{
	pthread_mutex_unlock(cs);
}

/* --- QR2 --- */

void qr2_register_keyA(int keyid, const char *key)
{
	if (keyid >= 0 && keyid < MAX_REGISTERED_KEYS)
	{
		s_qr2_keys[keyid] = key;
		qr2_registered_key_list[keyid] = key;
	}
}

void qr2_register_keyW(int keyid, const unsigned short *key)
{
	(void)key;
	qr2_register_keyA(keyid, "");
}

gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid)
{
	(void)keybuffer;
	(void)keyid;
	return gsi_false;
}

gsi_bool qr2_buffer_add(qr2_buffer_t outbuf, const gsi_char *value)
{
	(void)outbuf;
	(void)value;
	return gsi_false;
}

gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value)
{
	(void)outbuf;
	(void)value;
	return gsi_false;
}

/* --- gstats / gpersist --- */

int InitStatsConnection(int gameport)
{
	(void)gameport;
	s_stats_connected = 0;
	return GE_NOCONNECT;
}

int IsStatsConnected(void)
{
	return s_stats_connected;
}

void CloseStatsConnection(void)
{
	s_stats_connected = 0;
}

char *GetChallenge(statsgame_t game)
{
	(void)game;
	return s_challenge;
}

char *GenerateAuth(const char *challenge, const gsi_char *password, char response[33])
{
	(void)challenge;
	(void)password;
	if (response)
		memset(response, 0, 33);
	return response;
}

int PersistThink(void)
{
	return 0;
}

statsgame_t NewGame(int usebuckets)
{
	(void)usebuckets;
	return NULL;
}

void FreeGame(statsgame_t game)
{
	(void)game;
}

int SendGameSnapShotA(statsgame_t game, const char *snapshot, int final)
{
	(void)game;
	(void)snapshot;
	(void)final;
	return GE_NOCONNECT;
}

void PreAuthenticatePlayerPM(int localid, int profileid, const char *challengeresponse,
	PersAuthCallbackFn callback, void *instance)
{
	(void)localid;
	(void)profileid;
	(void)challengeresponse;
	if (callback)
		callback(localid, profileid, 0, (gsi_char *)"offline", instance);
}

void PreAuthenticatePlayerCD(int localid, const gsi_char *nick, const char *keyhash,
	const char *challengeresponse, PersAuthCallbackFn callback, void *instance)
{
	(void)localid;
	(void)nick;
	(void)keyhash;
	(void)challengeresponse;
	if (callback)
		callback(localid, 0, 0, (gsi_char *)"offline", instance);
}

void GetPersistDataValues(int localid, int profileid, persisttype_t type, int index,
	gsi_char *keys, PersDataCallbackFn callback, void *instance)
{
	(void)localid;
	(void)profileid;
	(void)type;
	(void)index;
	(void)keys;
	if (callback)
		callback(localid, profileid, type, index, 0, 0, (char *)"", 0, instance);
}

void SetPersistDataValues(int localid, int profileid, persisttype_t type, int index,
	const gsi_char *data, PersDataSaveCallbackFn callback, void *instance)
{
	(void)localid;
	(void)profileid;
	(void)type;
	(void)index;
	(void)data;
	if (callback)
		callback(localid, profileid, type, index, 0, 0, instance);
}

/* --- ghttp --- */

static void ghttp_fire_pending(void)
{
	for (int i = 0; i < (int)GSI_DIM(s_ghttp_pending); ++i)
	{
		if (!s_ghttp_pending[i].used)
			continue;
		if (s_ghttp_pending[i].callback)
			s_ghttp_pending[i].callback(s_ghttp_pending[i].id, GHTTPConnectFailed, NULL, 0,
				s_ghttp_pending[i].param);
		s_ghttp_pending[i].used = 0;
	}
}

void ghttpStartup(void) {}
void ghttpCleanup(void) { ghttp_fire_pending(); }

static GHTTPRequest ghttp_queue(ghttpCompletedCallback callback, void *param)
{
	for (int i = 0; i < (int)GSI_DIM(s_ghttp_pending); ++i)
	{
		if (!s_ghttp_pending[i].used)
		{
			s_ghttp_pending[i].used = 1;
			s_ghttp_pending[i].id = (GHTTPRequest)s_next_ghttp_request++;
			s_ghttp_pending[i].callback = callback;
			s_ghttp_pending[i].param = param;
			return s_ghttp_pending[i].id;
		}
	}
	return 0;
}

GHTTPRequest ghttpGet(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback callback, void *param)
{
	(void)URL;
	(void)blocking;
	return ghttp_queue(callback, param);
}

GHTTPRequest ghttpHead(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback callback, void *param)
{
	(void)URL;
	(void)blocking;
	return ghttp_queue(callback, param);
}

void ghttpThink(void)
{
	ghttp_fire_pending();
}

const char *ghttpGetHeaders(GHTTPRequest request)
{
	(void)request;
	return "";
}

GHTTPBool ghttpSetProxy(const char *server)
{
	(void)server;
	return GHTTPTrue;
}

/* --- GP --- */

GPResult gpInitialize(GPConnection *connection, int productID, int namespaceID, int partnerID)
{
	(void)productID;
	(void)namespaceID;
	(void)partnerID;
	if (connection)
		*connection = (GPConnection)&s_peer;
	return GP_NO_ERROR;
}

void gpDestroy(GPConnection *connection)
{
	if (connection)
		*connection = NULL;
}

GPResult gpSetCallback(GPConnection *connection, GPEnum func, GPCallback callback, void *param)
{
	(void)connection;
	(void)func;
	(void)callback;
	(void)param;
	return GP_NO_ERROR;
}

GPResult gpProcess(GPConnection *connection)
{
	(void)connection;
	return GP_NO_ERROR;
}

GPResult gpConnect(GPConnection *connection, const gsi_char *nick, const gsi_char *email,
	const gsi_char *password, GPEnum firewall, GPEnum blocking, GPCallback callback, void *param)
{
	(void)connection;
	(void)nick;
	(void)email;
	(void)password;
	(void)firewall;
	(void)blocking;
	(void)callback;
	(void)param;
	return GP_NETWORK_ERROR;
}

GPResult gpConnectNewUser(GPConnection *connection, const gsi_char nick[GP_NICK_LEN],
	const gsi_char uniquenick[GP_UNIQUENICK_LEN], const gsi_char email[GP_EMAIL_LEN],
	const gsi_char password[GP_PASSWORD_LEN], const gsi_char cdkey[GP_CDKEY_LEN], GPEnum firewall,
	GPEnum blocking, GPCallback callback, void *param)
{
	(void)nick;
	(void)uniquenick;
	(void)email;
	(void)password;
	(void)cdkey;
	return gpConnect(connection, nick, email, password, firewall, blocking, callback, param);
}

void gpDisconnect(GPConnection *connection)
{
	(void)connection;
}

GPResult gpIsConnected(GPConnection *connection, GPEnum *connected)
{
	(void)connection;
	if (connected)
		*connected = GP_NOT_CONNECTED;
	return GP_NO_ERROR;
}

GPResult gpDeleteProfile(GPConnection *connection, GPCallback callback, void *param)
{
	(void)connection;
	(void)callback;
	(void)param;
	return GP_NO_ERROR;
}

GPResult gpGetInfo(GPConnection *connection, GPProfile profile, GPEnum checkCache, GPEnum blocking,
	GPCallback callback, void *param)
{
	(void)connection;
	(void)profile;
	(void)checkCache;
	(void)blocking;
	(void)callback;
	(void)param;
	return GP_SERVER_ERROR;
}

GPResult gpSetInfoMask(GPConnection *connection, GPEnum mask)
{
	(void)connection;
	(void)mask;
	return GP_NO_ERROR;
}

GPResult gpSendBuddyRequest(GPConnection *connection, GPProfile profile, const gsi_char reason[GP_REASON_LEN])
{
	(void)connection;
	(void)profile;
	(void)reason;
	return GP_NO_ERROR;
}

GPResult gpAuthBuddyRequest(GPConnection *connection, GPProfile profile)
{
	(void)connection;
	(void)profile;
	return GP_NO_ERROR;
}

GPResult gpDenyBuddyRequest(GPConnection *connection, GPProfile profile)
{
	(void)connection;
	(void)profile;
	return GP_NO_ERROR;
}

GPResult gpDeleteBuddy(GPConnection *connection, GPProfile profile)
{
	(void)connection;
	(void)profile;
	return GP_NO_ERROR;
}

GPResult gpGetBuddyStatus(GPConnection *connection, int index, GPBuddyStatus *status)
{
	(void)connection;
	(void)index;
	if (status)
		memset(status, 0, sizeof(*status));
	return GP_NO_ERROR;
}

GPResult gpGetBuddyIndex(GPConnection *connection, GPProfile profile, int *index)
{
	(void)connection;
	(void)profile;
	if (index)
		*index = -1;
	return GP_NO_ERROR;
}

GPResult gpSetStatus(GPConnection *connection, GPEnum status, const gsi_char *statusString,
	const gsi_char *locationString)
{
	(void)connection;
	(void)status;
	(void)statusString;
	(void)locationString;
	return GP_NO_ERROR;
}

GPResult gpSendBuddyMessage(GPConnection *connection, GPProfile profile, const gsi_char *message)
{
	(void)connection;
	(void)profile;
	(void)message;
	return GP_NO_ERROR;
}

/* --- Peer --- */

PEER peerInitialize(PEERCallbacks *callbacks)
{
	(void)callbacks;
	s_peer.alive = 1;
	return (PEER)&s_peer;
}

void peerShutdown(PEER peer)
{
	(void)peer;
	s_peer.alive = 0;
}

void peerThink(PEER peer) { (void)peer; }

void chatSetLocalIP(unsigned int ip) { (void)ip; }

PEERBool peerIsConnected(PEER peer)
{
	(void)peer;
	return PEERFalse;
}

PEERBool peerSetTitle(PEER peer, const gsi_char *title, const gsi_char *qrSecretKey,
	const gsi_char *sbName, const gsi_char *sbSecretKey, int sbGameVersion, int sbMaxUpdates,
	PEERBool natNegotiate, PEERBool pingRooms[NumRooms], PEERBool crossPingRooms[NumRooms])
{
	(void)peer;
	(void)title;
	(void)qrSecretKey;
	(void)sbName;
	(void)sbSecretKey;
	(void)sbGameVersion;
	(void)sbMaxUpdates;
	(void)natNegotiate;
	(void)pingRooms;
	(void)crossPingRooms;
	return PEERTrue;
}

void peerConnect(PEER peer, const gsi_char *nick, int profileID, peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback, void *param, PEERBool authenticate)
{
	(void)peer;
	(void)nick;
	(void)profileID;
	(void)nickErrorCallback;
	(void)authenticate;
	if (connectCallback)
		connectCallback(peer, PEERFalse, 0, param);
}

void peerDisconnect(PEER peer) { (void)peer; }
void peerRetryWithNick(PEER peer, const gsi_char *nick) { (void)peer; (void)nick; }

void peerJoinTitleRoom(PEER peer, const gsi_char *password, peerJoinRoomCallback callback, void *param,
	PEERBool authenticate)
{
	(void)peer;
	(void)password;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, PEERJoinFailed, TitleRoom, param);
}

void peerJoinGroupRoom(PEER peer, int groupID, peerJoinRoomCallback callback, void *param, PEERBool authenticate)
{
	(void)peer;
	(void)groupID;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, PEERJoinFailed, GroupRoom, param);
}

void peerJoinStagingRoom(PEER peer, SBServer server, const gsi_char *password, peerJoinRoomCallback callback,
	void *param, PEERBool authenticate)
{
	(void)peer;
	(void)server;
	(void)password;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, PEERJoinFailed, StagingRoom, param);
}

void peerCreateStagingRoom(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char *password,
	peerJoinRoomCallback callback, void *param, PEERBool authenticate)
{
	(void)peer;
	(void)name;
	(void)maxPlayers;
	(void)password;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, PEERJoinFailed, StagingRoom, param);
}

void peerCreateStagingRoomWithSocket(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char *password,
	int socket, unsigned short port, peerJoinRoomCallback callback, void *param, PEERBool authenticate)
{
	(void)socket;
	(void)port;
	peerCreateStagingRoom(peer, name, maxPlayers, password, callback, param, authenticate);
}

void peerLeaveRoom(PEER peer, RoomType roomType, const gsi_char *reason)
{
	(void)peer;
	(void)roomType;
	(void)reason;
}

void peerListGroupRooms(PEER peer, const gsi_char *fields, peerListGroupRoomsCallback callback, void *param,
	PEERBool authenticate)
{
	(void)peer;
	(void)fields;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, 0, NULL, NULL, 0, 0, 0, 0, param);
}

void peerStartListingGames(PEER peer, const unsigned char *fields, int numFields, const gsi_char *filter,
	peerListingGamesCallback callback, void *param)
{
	(void)peer;
	(void)fields;
	(void)numFields;
	(void)filter;
	if (callback)
		callback(peer, PEERFalse, NULL, NULL, PEERFalse, PEER_CLEAR, 100, param);
}

void peerStopListingGames(PEER peer) { (void)peer; }
void peerUpdateGame(PEER peer, SBServer server, PEERBool fullUpdate)
{
	(void)peer;
	(void)server;
	(void)fullUpdate;
}

void peerStartGame(PEER peer, const gsi_char *message, int reportingOptions)
{
	(void)peer;
	(void)message;
	(void)reportingOptions;
}

void peerStopGame(PEER peer) { (void)peer; }
void peerStateChanged(PEER peer) { (void)peer; }
void peerSetReady(PEER peer, PEERBool ready) { (void)peer; (void)ready; }

void peerEnumPlayers(PEER peer, RoomType roomType, peerEnumPlayersCallback callback, void *userData)
{
	(void)peer;
	(void)roomType;
	if (callback)
		callback(peer, PEERFalse, roomType, -1, NULL, 0, userData);
}

void peerMessageRoom(PEER peer, RoomType roomType, const gsi_char *message, MessageType messageType)
{
	(void)peer;
	(void)roomType;
	(void)message;
	(void)messageType;
}

void peerMessagePlayer(PEER peer, const gsi_char *nick, const gsi_char *message, MessageType messageType)
{
	(void)peer;
	(void)nick;
	(void)message;
	(void)messageType;
}

void peerUTMRoom(PEER peer, RoomType roomType, const gsi_char *key, const gsi_char *val, PEERBool authenticate)
{
	(void)peer;
	(void)roomType;
	(void)key;
	(void)val;
	(void)authenticate;
}

void peerUTMPlayer(PEER peer, const gsi_char *nick, const gsi_char *key, const gsi_char *val, PEERBool authenticate)
{
	(void)peer;
	(void)nick;
	(void)key;
	(void)val;
	(void)authenticate;
}

void peerSetGlobalKeys(PEER peer, int num, const char **keys, const char **values)
{
	(void)peer;
	(void)num;
	(void)keys;
	(void)values;
}

void peerSetGlobalWatchKeys(PEER peer, RoomType roomType, int num, const char **keys, PEERBool add)
{
	(void)peer;
	(void)roomType;
	(void)num;
	(void)keys;
	(void)add;
}

void peerSetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const char **keys,
	const char **values)
{
	(void)peer;
	(void)roomType;
	(void)nick;
	(void)num;
	(void)keys;
	(void)values;
}

void peerGetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const char **keys,
	peerGetRoomKeysCallback callback, void *param, PEERBool authenticate)
{
	(void)peer;
	(void)roomType;
	(void)nick;
	(void)num;
	(void)keys;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, roomType, nick, num, NULL, NULL, param);
}

void peerSetRoomWatchKeys(PEER peer, RoomType roomType, int num, const char **keys, PEERBool add)
{
	(void)peer;
	(void)roomType;
	(void)num;
	(void)keys;
	(void)add;
}

const gsi_char *peerGetGlobalWatchKey(PEER peer, const gsi_char *nick, const gsi_char *key)
{
	(void)peer;
	(void)nick;
	(void)key;
	return "";
}

PEERBool peerGetPlayerFlags(PEER peer, const gsi_char *nick, RoomType roomType, int *flags)
{
	(void)peer;
	(void)nick;
	(void)roomType;
	if (flags)
		*flags = 0;
	return PEERFalse;
}

PEERBool peerGetPlayerInfoNoWait(PEER peer, const gsi_char *nick, unsigned int *IP, int *profileID)
{
	(void)peer;
	(void)nick;
	if (IP)
		*IP = 0;
	if (profileID)
		*profileID = 0;
	return PEERFalse;
}

void peerGetPlayerProfileID(PEER peer, const gsi_char *nick, peerGetPlayerProfileIDCallback callback, void *param,
	PEERBool authenticate)
{
	(void)peer;
	(void)nick;
	(void)authenticate;
	if (callback)
		callback(peer, PEERFalse, nick, 0, param);
}

void peerAuthenticateCDKey(PEER peer, const gsi_char *cdkey, peerAuthenticateCDKeyCallback callback, void *param,
	PEERBool blocking)
{
	(void)peer;
	(void)cdkey;
	(void)blocking;
	if (callback)
		callback(peer, 0, "", param);
}

void peerParseQuery(PEER peer, char *query, int len, struct sockaddr *sender)
{
	(void)peer;
	(void)query;
	(void)len;
	(void)sender;
}

void peerSetUpdatesRoomChannel(PEER peer, const gsi_char *channel)
{
	(void)peer;
	(void)channel;
}

/* Generals-era API (fewer parameters than modern GameSpy headers). */
GPResult gpInitializeGenerals(GPConnection *connection, int productID)
{
	return gpInitialize(connection, productID, 0, 0);
}

GPResult gpConnectNewUserGenerals(GPConnection *connection, const gsi_char *nick, const gsi_char *email,
	const gsi_char *password, GPEnum firewall, GPEnum blocking, GPCallback callback, void *param)
{
	gsi_char emptyCdkey[GP_CDKEY_LEN];
	gsi_char emptyUnique[GP_UNIQUENICK_LEN];
	memset(emptyCdkey, 0, sizeof(emptyCdkey));
	memset(emptyUnique, 0, sizeof(emptyUnique));
	return gpConnectNewUser(connection, nick, emptyUnique, email, password, emptyCdkey, firewall, blocking,
		callback, param);
}

PEERBool peerSetTitleGenerals(PEER peer, const gsi_char *title, const gsi_char *qrSecretKey,
	const gsi_char *sbName, const gsi_char *sbSecretKey, int sbGameVersion, PEERBool pingRooms[NumRooms],
	PEERBool crossPingRooms[NumRooms])
{
	return peerSetTitle(peer, title, qrSecretKey, sbName, sbSecretKey, sbGameVersion, 30, PEERTrue, pingRooms,
		crossPingRooms);
}

unsigned int peerGetPublicIP(PEER peer)
{
	(void)peer;
	return 0;
}

/* --- Server browsing --- */

const gsi_char *SBServerGetStringValue(SBServer server, const gsi_char *keyname, const gsi_char *def)
{
	(void)server;
	(void)keyname;
	return def ? def : "";
}

int SBServerGetIntValue(SBServer server, const gsi_char *key, int idefault)
{
	(void)server;
	(void)key;
	return idefault;
}

const gsi_char *SBServerGetPlayerStringValue(SBServer server, int playernum, const gsi_char *key,
	const gsi_char *sdefault)
{
	(void)server;
	(void)playernum;
	(void)key;
	return sdefault ? sdefault : "";
}

int SBServerGetPlayerIntValue(SBServer server, int playernum, const gsi_char *key, int idefault)
{
	(void)server;
	(void)playernum;
	(void)key;
	return idefault;
}

unsigned int SBServerGetPrivateInetAddress(SBServer server)
{
	(void)server;
	return 0;
}

unsigned int SBServerGetPublicInetAddress(SBServer server)
{
	(void)server;
	return 0;
}

unsigned short SBServerGetPrivateQueryPort(SBServer server)
{
	(void)server;
	return 0;
}

SBBool SBServerHasBasicKeys(SBServer server)
{
	(void)server;
	return SBFalse;
}

SBBool SBServerHasFullKeys(SBServer server)
{
	(void)server;
	return SBFalse;
}

void SBServerEnumKeys(SBServer server, SBServerKeyEnumFn KeyFn, void *instance)
{
	(void)server;
	(void)KeyFn;
	(void)instance;
}
