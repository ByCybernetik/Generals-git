#pragma once
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
