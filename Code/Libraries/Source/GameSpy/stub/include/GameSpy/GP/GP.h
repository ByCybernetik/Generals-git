#pragma once
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
