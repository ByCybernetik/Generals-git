#!/usr/bin/env python3
"""Generate minimal Generals-era GameSpy stub headers under Code/Libraries/Source/GameSpy/stub/include."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1] / "Code/Libraries/Source/GameSpy/stub/include/GameSpy"


def w(rel: str, content: str) -> None:
    path = ROOT / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    print("wrote", path)


if ROOT.exists():
    shutil.rmtree(ROOT)
ROOT.mkdir(parents=True)

w(
    "gsi_types.h",
    r'''#pragma once
typedef char gsi_char;
typedef int gsi_bool;
typedef int gsi_i32;
typedef long long gsi_i64;
#ifndef gsi_true
#define gsi_true 1
#define gsi_false 0
#endif
''',
)

w(
    "ghttp/ghttp.h",
    r'''#pragma once
#include "../gsi_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { GHTTPFalse, GHTTPTrue } GHTTPBool;
typedef int GHTTPRequest;
typedef int GHTTPByteCount;
typedef enum {
  GHTTPSuccess = 0,
  GHTTPOutOfMemory,
  GHTTPBufferOverflow,
  GHTTPParseURLFailed,
  GHTTPHostLookupFailed,
  GHTTPSocketFailed,
  GHTTPConnectFailed,
  GHTTPBadResponse,
  GHTTPRequestRejected,
  GHTTPUnauthorized,
  GHTTPForbidden,
  GHTTPFileNotFound,
  GHTTPServerError,
  GHTTPFileWriteFailed,
  GHTTPFileReadFailed,
  GHTTPFileIncomplete,
  GHTTPFileToBig,
  GHTTPEncryptionError,
  GHTTPRequestCancelled
} GHTTPResult;
typedef GHTTPBool (*ghttpCompletedCallback)(GHTTPRequest request, GHTTPResult result, char *buffer, GHTTPByteCount bufferLen, void *param);
void ghttpStartup(void);
void ghttpCleanup(void);
void ghttpThink(void);
GHTTPRequest ghttpGet(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback cb, void *param);
GHTTPRequest ghttpHead(const gsi_char *URL, GHTTPBool blocking, ghttpCompletedCallback cb, void *param);
const char *ghttpGetHeaders(GHTTPRequest request);
GHTTPBool ghttpSetProxy(const char *server);
#ifdef __cplusplus
}
#endif
''',
)

w(
    "qr2/qr2.h",
    r'''#pragma once
#include "../gsi_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { key_server = 0, key_player, key_team } qr2_key_type;
typedef enum {
  e_qrnoerror = 0,
  e_qrwsockerror,
  e_qrbinderror,
  e_qrdnserror,
  e_qrconnerror,
  e_qrnochallengeerror,
  e_qrbufferout
} qr2_error_t;
typedef struct qr2_buffer_s *qr2_buffer_t;
typedef struct qr2_keybuffer_s *qr2_keybuffer_t;
gsi_bool qr2_buffer_add(qr2_buffer_t outbuf, const gsi_char *value);
gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value);
gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid);
void qr2_register_key(int keyid, const gsi_char *key);
#ifdef __cplusplus
}
#endif
''',
)

# Reserved keys only. Generals defines custom keys (EXECRC_KEY, NUMPLAYER_KEY, ...)
# as NUM_RESERVED_KEYS+N in PeerThread.cpp — do not redefine those names here.
w(
    "qr2/qr2regkeys.h",
    r'''#pragma once
#include "qr2.h"
#define MAX_REGISTERED_KEYS 254
#define NUM_RESERVED_KEYS 50

#define HOSTNAME_KEY 1
#define GAMENAME_KEY 2
#define GAMEVER_KEY 3
#define HOSTPORT_KEY 4
#define MAPNAME_KEY 5
#define GAMETYPE_KEY 6
#define GAMEVARIANT_KEY 7
#define NUMPLAYERS_KEY 8
#define NUMTEAMS_KEY 9
#define MAXPLAYERS_KEY 10
#define GAMEMODE_KEY 11
#define TEAMPLAY_KEY 12
#define FRAGLIMIT_KEY 13
#define TEAMFRAGLIMIT_KEY 14
#define TIMEELAPSED_KEY 15
#define TIMELIMIT_KEY 16
#define ROUNDTIME_KEY 17
#define ROUNDELAPSED_KEY 18
#define PASSWORD_KEY 19
#define GROUPID_KEY 20
#define PLAYER__KEY 21
#define SCORE__KEY 22
#define SKILL__KEY 23
#define PING__KEY 24
#define TEAM__KEY 25
#define DEATHS__KEY 26
#define PID__KEY_RESERVED 27
#define TEAM_T_KEY 28
#define SCORE_T_KEY 29
#define NN_GROUP_ID_KEY 30
#define COUNTRY_KEY 31
#define REGION_KEY 32

extern const char *qr2_registered_key_list[];
''',
)

w(
    "serverbrowsing/sb_serverbrowsing.h",
    r'''#pragma once
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
''',
)

w(
    "gstats/gstats.h",
    r'''#pragma once
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
''',
)

w(
    "gstats/gpersist.h",
    r'''#pragma once
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
''',
)

w(
    "GP/GP.h",
    r'''#pragma once
#include "../gsi_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define GP_NICK_LEN 31
#define GP_EMAIL_LEN 51
#define GP_PASSWORD_LEN 31
#define GP_REASON_LEN 1025
#define GP_STATUS_STRING_LEN 256
#define GP_LOCATION_STRING_LEN 256
#define GP_COUNTRYCODE_LEN 3

typedef void *GPConnection;
typedef int GPProfile;
typedef void (*GPCallback)(GPConnection *connection, void *arg, void *param);

typedef enum {
  GP_NO_ERROR = 0,
  GP_MEMORY_ERROR,
  GP_PARAMETER_ERROR,
  GP_NETWORK_ERROR,
  GP_SERVER_ERROR
} GPResult;

typedef enum {
  GP_ERROR = 0,
  GP_RECV_BUDDY_REQUEST,
  GP_RECV_BUDDY_STATUS,
  GP_RECV_BUDDY_MESSAGE,
  GP_RECV_GAME_INVITE,
  GP_FIREWALL,
  GP_NO_FIREWALL,
  GP_BLOCKING,
  GP_NON_BLOCKING,
  GP_CHECK_CACHE,
  GP_DONT_CHECK_CACHE,
  GP_CONNECTED,
  GP_NOT_CONNECTED,
  GP_MASK_NONE = 0,
  GP_MASK_HOMEPAGE = 1,
  GP_MASK_ZIPCODE = 2,
  GP_MASK_COUNTRYCODE = 4,
  GP_MASK_BIRTHDAY = 8,
  GP_MASK_SEX = 16,
  GP_MASK_ALL = 0xFFFFFFFF,
  GP_OFFLINE = 0,
  GP_ONLINE = 1,
  GP_PLAYING = 2,
  GP_STAGING = 3,
  GP_CHATTING = 4,
  GP_AWAY = 5,
  GP_FATAL = 1,
  GP_NON_FATAL = 0
} GPEnum;

typedef enum {
  GP_GENERAL = 0x0000,
  GP_PARSE,
  GP_NOT_LOGGED_IN,
  GP_BAD_SESSKEY,
  GP_DATABASE,
  GP_NETWORK,
  GP_FORCED_DISCONNECT,
  GP_CONNECTION_CLOSED,
  GP_LOGIN = 0x0100,
  GP_LOGIN_TIMEOUT,
  GP_LOGIN_BAD_NICK,
  GP_LOGIN_BAD_EMAIL,
  GP_LOGIN_BAD_PASSWORD,
  GP_LOGIN_BAD_PROFILE,
  GP_LOGIN_PROFILE_DELETED,
  GP_LOGIN_CONNECTION_FAILED,
  GP_LOGIN_SERVER_AUTH_FAILED,
  GP_NEWUSER = 0x0200,
  GP_NEWUSER_BAD_NICK,
  GP_NEWUSER_BAD_PASSWORD,
  GP_UPDATEUI = 0x0300,
  GP_UPDATEUI_BAD_EMAIL,
  GP_NEWPROFILE = 0x0400,
  GP_NEWPROFILE_BAD_NICK,
  GP_NEWPROFILE_BAD_OLD_NICK,
  GP_UPDATEPRO = 0x0500,
  GP_UPDATEPRO_BAD_NICK,
  GP_ADDBUDDY = 0x0600,
  GP_ADDBUDDY_BAD_FROM,
  GP_ADDBUDDY_BAD_NEW,
  GP_ADDBUDDY_ALREADY_BUDDY,
  GP_AUTHADD = 0x0700,
  GP_AUTHADD_BAD_FROM,
  GP_AUTHADD_BAD_SIG,
  GP_STATUS = 0x0800,
  GP_BM = 0x0900,
  GP_BM_NOT_BUDDY,
  GP_GETPROFILE = 0x0A00,
  GP_GETPROFILE_BAD_PROFILE,
  GP_DELBUDDY = 0x0B00,
  GP_DELBUDDY_NOT_BUDDY,
  GP_DELPROFILE = 0x0C00,
  GP_DELPROFILE_LAST_PROFILE,
  GP_SEARCH = 0x0D00,
  GP_SEARCH_CONNECTION_FAILED
} GPErrorCode;

typedef struct {
  GPResult result;
  GPErrorCode errorCode;
  gsi_char *errorString;
  GPEnum fatal;
} GPErrorArg;

typedef struct {
  GPResult result;
  GPProfile profile;
} GPConnectResponseArg;

typedef struct {
  GPProfile profile;
  gsi_char nick[GP_NICK_LEN];
  gsi_char email[GP_EMAIL_LEN];
  gsi_char countrycode[GP_COUNTRYCODE_LEN];
  gsi_char reason[GP_REASON_LEN];
} GPRecvBuddyRequestArg;

typedef struct {
  GPProfile profile;
  unsigned int date;
  gsi_char nick[GP_NICK_LEN];
  gsi_char message[2048];
} GPRecvBuddyMessageArg;

typedef struct {
  GPProfile profile;
  GPEnum status;
  gsi_char statusString[GP_STATUS_STRING_LEN];
  gsi_char locationString[GP_LOCATION_STRING_LEN];
} GPBuddyStatus;

typedef struct {
  GPProfile profile;
  int index;
  gsi_char nick[GP_NICK_LEN];
  gsi_char email[GP_EMAIL_LEN];
  gsi_char countrycode[GP_COUNTRYCODE_LEN];
} GPRecvBuddyStatusArg;

typedef struct {
  GPResult result;
  GPProfile profile;
  gsi_char nick[GP_NICK_LEN];
  gsi_char uniquenick[GP_NICK_LEN];
  gsi_char email[GP_EMAIL_LEN];
  gsi_char firstname[31];
  gsi_char lastname[31];
  gsi_char homepage[128];
  int icquin;
  gsi_char zipcode[11];
  gsi_char countrycode[GP_COUNTRYCODE_LEN];
  float longitude;
  float latitude;
  gsi_char place[128];
  int birthday;
  int birthmonth;
  int birthyear;
  GPEnum sex;
  GPEnum publicmask;
  gsi_char aimname[31];
  int pic;
  int occupationid;
  int industryid;
  int incomeid;
  int educationid;
  int hoursperweek;
  int onetime;
} GPGetInfoResponseArg;

/* Generals-era signatures (pre-namespace/partnerID SDK). */
GPResult gpInitialize(GPConnection *connection, int productID);
void gpDestroy(GPConnection *connection);
GPResult gpProcess(GPConnection *connection);
GPResult gpConnect(GPConnection *connection, const gsi_char nick[GP_NICK_LEN], const gsi_char email[GP_EMAIL_LEN], const gsi_char password[GP_PASSWORD_LEN], GPEnum firewall, GPEnum blocking, GPCallback callback, void *param);
GPResult gpConnectNewUser(GPConnection *connection, const gsi_char nick[GP_NICK_LEN], const gsi_char email[GP_EMAIL_LEN], const gsi_char password[GP_PASSWORD_LEN], GPEnum firewall, GPEnum blocking, GPCallback callback, void *param);
void gpDisconnect(GPConnection *connection);
GPResult gpIsConnected(GPConnection *connection, GPEnum *connected);
GPResult gpSetCallback(GPConnection *connection, GPEnum func, GPCallback callback, void *param);
GPResult gpSetStatus(GPConnection *connection, GPEnum status, const gsi_char statusString[GP_STATUS_STRING_LEN], const gsi_char locationString[GP_LOCATION_STRING_LEN]);
GPResult gpSetInfoMask(GPConnection *connection, GPEnum mask);
GPResult gpGetInfo(GPConnection *connection, GPProfile profile, GPEnum checkCache, GPEnum blocking, GPCallback callback, void *param);
GPResult gpGetBuddyStatus(GPConnection *connection, int index, GPBuddyStatus *status);
GPResult gpGetBuddyIndex(GPConnection *connection, GPProfile profile, int *index);
GPResult gpSendBuddyRequest(GPConnection *connection, GPProfile profile, const gsi_char reason[GP_REASON_LEN]);
GPResult gpAuthBuddyRequest(GPConnection *connection, GPProfile profile);
GPResult gpDenyBuddyRequest(GPConnection *connection, GPProfile profile);
GPResult gpDeleteBuddy(GPConnection *connection, GPProfile profile);
GPResult gpDeleteProfile(GPConnection *connection, GPCallback callback, void *param);
GPResult gpSendBuddyMessage(GPConnection *connection, GPProfile profile, const gsi_char *message);

#ifdef __cplusplus
}
#endif
''',
)

w("gp/gp.h", '#pragma once\n#include "../GP/GP.h"\n')
w("gp/GP.h", '#pragma once\n#include "../GP/GP.h"\n')
w("GP/gp.h", '#pragma once\n#include "GP.h"\n')

w(
    "Peer/Peer.h",
    r'''#pragma once
#include "../gsi_types.h"
#include "../serverbrowsing/sb_serverbrowsing.h"
#include "../qr2/qr2.h"
#include <sys/socket.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void *PEER;
typedef int SOCKET;
typedef SBServer GServer;

typedef enum { PEERFalse = 0, PEERTrue = 1 } PEERBool;
typedef enum { TitleRoom = 0, GroupRoom, StagingRoom, NumRooms } RoomType;
typedef enum { NormalMessage = 0, ActionMessage, NoticeMessage } MessageType;
typedef enum {
  PEERJoinSuccess = 0,
  PEERFullRoom,
  PEERInviteOnlyRoom,
  PEERBannedFromRoom,
  PEERBadPassword,
  PEERAlreadyInRoom,
  PEERNoConnection,
  PEERJoinFailed
} PEERJoinResult;

#define PEER_PASSWORD_LEN 24
#define PEER_ADD 0
#define PEER_UPDATE 1
#define PEER_REMOVE 2
#define PEER_CLEAR 3
#define PEER_COMPLETE 4
#define PEER_IN_USE 1
#define PEER_FLAG_STAGING 0x01
#define PEER_FLAG_READY 0x02
#define PEER_FLAG_PLAYING 0x04
#define PEER_FLAG_AWAY 0x08
#define PEER_FLAG_HOST 0x10
#define PEER_FLAG_OP 0x20
#define PEER_FLAG_VOICE 0x40
#define PEER_KEEP_REPORTING 0
#define PEER_STOP_REPORTING 1

typedef void (*peerDisconnectedCallback)(PEER peer, const char *reason, void *param);
typedef void (*peerRoomMessageCallback)(PEER peer, RoomType roomType, const char *nick, const char *message, MessageType messageType, void *param);
typedef void (*peerRoomUTMCallback)(PEER peer, RoomType roomType, const char *nick, const char *command, const char *parameters, PEERBool authenticated, void *param);
typedef void (*peerRoomNameChangedCallback)(PEER peer, RoomType roomType, void *param);
typedef void (*peerRoomModeChangedCallback)(PEER peer, RoomType roomType, void *param);
typedef void (*peerPlayerMessageCallback)(PEER peer, const char *nick, const char *message, MessageType messageType, void *param);
typedef void (*peerPlayerUTMCallback)(PEER peer, const char *nick, const char *command, const char *parameters, PEERBool authenticated, void *param);
typedef void (*peerReadyChangedCallback)(PEER peer, const char *nick, PEERBool ready, void *param);
typedef void (*peerGameStartedCallback)(PEER peer, SBServer server, const char *message, void *param);
typedef void (*peerPlayerJoinedCallback)(PEER peer, RoomType roomType, const char *nick, void *param);
typedef void (*peerPlayerLeftCallback)(PEER peer, RoomType roomType, const char *nick, const char *reason, void *param);
typedef void (*peerKickedCallback)(PEER peer, RoomType roomType, const char *nick, const char *reason, void *param);
typedef void (*peerNewPlayerListCallback)(PEER peer, RoomType roomType, void *param);
typedef void (*peerPlayerChangedNickCallback)(PEER peer, RoomType roomType, const char *oldNick, const char *newNick, void *param);
typedef void (*peerPlayerInfoCallback)(PEER peer, RoomType roomType, const char *nick, unsigned int IP, int profileID, void *param);
typedef void (*peerPlayerFlagsChangedCallback)(PEER peer, RoomType roomType, const char *nick, int oldFlags, int newFlags, void *param);
typedef void (*peerPingCallback)(PEER peer, const char *nick, int ping, void *param);
typedef void (*peerCrossPingCallback)(PEER peer, const char *nick1, const char *nick2, int crossPing, void *param);
typedef void (*peerGlobalKeyChangedCallback)(PEER peer, const char *nick, const char *key, const char *value, void *param);
typedef void (*peerRoomKeyChangedCallback)(PEER peer, RoomType roomType, const char *nick, const char *key, const char *value, void *param);
typedef void (*peerQRServerKeyCallback)(PEER peer, int key, qr2_buffer_t buffer, void *param);
typedef void (*peerQRPlayerKeyCallback)(PEER peer, int key, int index, qr2_buffer_t buffer, void *param);
typedef void (*peerQRTeamKeyCallback)(PEER peer, int key, int index, qr2_buffer_t buffer, void *param);
typedef void (*peerQRKeyListCallback)(PEER peer, qr2_key_type type, qr2_keybuffer_t keyBuffer, void *param);
typedef int (*peerQRCountCallback)(PEER peer, qr2_key_type type, void *param);
typedef void (*peerQRAddErrorCallback)(PEER peer, qr2_error_t error, char *errMsg, void *param);
typedef void (*peerQRNatNegotiateCallback)(PEER peer, int cookie, void *param);
typedef void (*peerQRPublicAddressCallback)(PEER peer, unsigned int ip, unsigned short port, void *param);

typedef struct {
  peerDisconnectedCallback disconnected;
  peerRoomMessageCallback roomMessage;
  peerRoomUTMCallback roomUTM;
  peerRoomNameChangedCallback roomNameChanged;
  peerRoomModeChangedCallback roomModeChanged;
  peerPlayerMessageCallback playerMessage;
  peerPlayerUTMCallback playerUTM;
  peerReadyChangedCallback readyChanged;
  peerGameStartedCallback gameStarted;
  peerPlayerJoinedCallback playerJoined;
  peerPlayerLeftCallback playerLeft;
  peerKickedCallback kicked;
  peerNewPlayerListCallback newPlayerList;
  peerPlayerChangedNickCallback playerChangedNick;
  peerPlayerInfoCallback playerInfo;
  peerPlayerFlagsChangedCallback playerFlagsChanged;
  peerPingCallback ping;
  peerCrossPingCallback crossPing;
  peerGlobalKeyChangedCallback globalKeyChanged;
  peerRoomKeyChangedCallback roomKeyChanged;
  peerQRServerKeyCallback qrServerKey;
  peerQRPlayerKeyCallback qrPlayerKey;
  peerQRTeamKeyCallback qrTeamKey;
  peerQRKeyListCallback qrKeyList;
  peerQRCountCallback qrCount;
  peerQRAddErrorCallback qrAddError;
  peerQRNatNegotiateCallback qrNatNegotiateCallback;
  peerQRPublicAddressCallback qrPublicAddressCallback;
  void *param;
} PEERCallbacks;

typedef void (*peerConnectCallback)(PEER peer, PEERBool success, void *param);
typedef void (*peerNickErrorCallback)(PEER peer, int type, const char *nick, void *param);
typedef void (*peerJoinRoomCallback)(PEER peer, PEERBool success, PEERJoinResult result, RoomType roomType, void *param);
typedef void (*peerListGroupRoomsCallback)(PEER peer, PEERBool success, int groupID, SBServer server, const char *name, int numWaiting, int maxWaiting, int numGames, int numPlaying, void *param);
typedef void (*peerEnumPlayersCallback)(PEER peer, PEERBool success, RoomType roomType, int index, const char *nick, int flags, void *param);
typedef void (*peerListingGamesCallback)(PEER peer, PEERBool success, const char *name, SBServer server, PEERBool staging, int msg, int progress, void *param);
typedef void (*peerAuthenticateCDKeyCallback)(PEER peer, int result, char *message, void *param);
typedef void (*peerGetPlayerProfileIDCallback)(PEER peer, PEERBool success, const char *nick, int profileID, void *param);
typedef void (*peerGetRoomKeysCallback)(PEER peer, PEERBool success, RoomType roomType, const char *nick, int num, char **keys, char **values, void *param);

PEER peerInitialize(PEERCallbacks *callbacks);
void peerShutdown(PEER peer);
void peerThink(PEER peer);
PEERBool peerIsConnected(PEER peer);
void peerConnect(PEER peer, const gsi_char *nick, int profileID, peerNickErrorCallback nickErrorCallback, peerConnectCallback connectCallback, void *param, PEERBool blocking);
void peerDisconnect(PEER peer);
void peerRetryWithNick(PEER peer, const gsi_char *nick);
void peerAuthenticateCDKey(PEER peer, const gsi_char *cdkey, peerAuthenticateCDKeyCallback callback, void *param, PEERBool blocking);
/* Generals-era peerSetTitle (no sbMaxUpdates / natNegotiate args). */
PEERBool peerSetTitle(PEER peer, const gsi_char *title, const gsi_char *qrSecretKey, const gsi_char *sbName, const gsi_char *sbSecretKey, int sbGameVersion, PEERBool pingRooms[NumRooms], PEERBool crossPingRooms[NumRooms]);
void peerJoinTitleRoom(PEER peer, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking);
void peerJoinGroupRoom(PEER peer, int groupID, peerJoinRoomCallback callback, void *param, PEERBool blocking);
void peerJoinStagingRoom(PEER peer, SBServer server, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking);
void peerLeaveRoom(PEER peer, RoomType roomType, const gsi_char *reason);
void peerListGroupRooms(PEER peer, const gsi_char *fields, peerListGroupRoomsCallback callback, void *param, PEERBool blocking);
void peerCreateStagingRoom(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char password[PEER_PASSWORD_LEN], peerJoinRoomCallback callback, void *param, PEERBool blocking);
void peerCreateStagingRoomWithSocket(PEER peer, const gsi_char *name, int maxPlayers, const gsi_char password[PEER_PASSWORD_LEN], SOCKET socket, unsigned short port, peerJoinRoomCallback callback, void *param, PEERBool blocking);
void peerEnumPlayers(PEER peer, RoomType roomType, peerEnumPlayersCallback callback, void *param);
void peerMessagePlayer(PEER peer, const gsi_char *nick, const gsi_char *message, MessageType messageType);
void peerMessageRoom(PEER peer, RoomType roomType, const gsi_char *message, MessageType messageType);
void peerUTMPlayer(PEER peer, const gsi_char *nick, const gsi_char *command, const gsi_char *parameters, PEERBool authenticate);
void peerUTMRoom(PEER peer, RoomType roomType, const gsi_char *command, const gsi_char *parameters, PEERBool authenticate);
void peerSetReady(PEER peer, PEERBool ready);
void peerStartGame(PEER peer, const gsi_char *message, int reportingOptions);
void peerStopGame(PEER peer);
void peerStartListingGames(PEER peer, const unsigned char *fields, int numFields, const gsi_char *filter, peerListingGamesCallback callback, void *param);
void peerStopListingGames(PEER peer);
void peerUpdateGame(PEER peer, SBServer server, PEERBool fullUpdate);
PEERBool peerGetPlayerInfoNoWait(PEER peer, const gsi_char *nick, unsigned int *IP, int *profileID);
void peerGetPlayerProfileID(PEER peer, const gsi_char *nick, peerGetPlayerProfileIDCallback callback, void *param, PEERBool blocking);
PEERBool peerGetPlayerFlags(PEER peer, const gsi_char *nick, RoomType roomType, int *flags);
unsigned int peerGetPublicIP(PEER peer);
#define peerGetLocalIP peerGetPublicIP
const gsi_char *peerGetGlobalWatchKey(PEER peer, const gsi_char *nick, const gsi_char *key);
void peerGetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const gsi_char **keys, peerGetRoomKeysCallback callback, void *param, PEERBool blocking);
void peerSetGlobalKeys(PEER peer, int num, const gsi_char **keys, const gsi_char **values);
void peerSetGlobalWatchKeys(PEER peer, RoomType roomType, int num, const gsi_char **keys, PEERBool addKeys);
void peerSetRoomKeys(PEER peer, RoomType roomType, const gsi_char *nick, int num, const gsi_char **keys, const gsi_char **values);
void peerSetRoomWatchKeys(PEER peer, RoomType roomType, int num, const gsi_char **keys, PEERBool addKeys);
void peerParseQuery(PEER peer, char *query, int len, struct sockaddr *sender);
void peerStateChanged(PEER peer);

/* Exposed from chat SDK; Generals calls it from PeerThread. */
void chatSetLocalIP(unsigned long preferredIP);

#ifdef __cplusplus
}
#endif
''',
)

w("peer/peer.h", '#pragma once\n#include "../Peer/Peer.h"\n')
w("peer/Peer.h", '#pragma once\n#include "../Peer/Peer.h"\n')
w("Peer/peer.h", '#pragma once\n#include "Peer.h"\n')

w(
    "chat/chat.h",
    r'''#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void chatSetLocalIP(unsigned long preferredIP);
#ifdef __cplusplus
}
#endif
''',
)

print("done")
