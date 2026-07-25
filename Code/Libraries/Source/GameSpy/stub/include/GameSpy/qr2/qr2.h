#pragma once
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
