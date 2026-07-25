#pragma once
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
