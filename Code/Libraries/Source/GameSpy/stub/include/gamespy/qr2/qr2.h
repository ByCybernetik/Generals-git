#ifndef QR2_H
#define QR2_H

#include "gscommon.h"
#include "qr2regkeys.h"

#ifndef GSI_UNICODE
#define qr2_buffer_add qr2_buffer_addA
#else
#define qr2_buffer_add qr2_buffer_addW
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	e_qrnoerror,
	e_qrwsockerror,
	e_qrbinderror,
	e_qrdnserror,
	e_qrconnerror,
	e_qrnochallengeerror,
	qr2_error_t_count
} qr2_error_t;

typedef enum
{
	key_server,
	key_player,
	key_team,
	key_type_count
} qr2_key_type;

typedef struct qr2_buffer_s *qr2_buffer_t;
typedef struct qr2_keybuffer_s *qr2_keybuffer_t;

gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid);
gsi_bool qr2_buffer_add(qr2_buffer_t outbuf, const gsi_char *value);
gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value);
gsi_bool qr2_buffer_addA(qr2_buffer_t outbuf, const char *value);

#ifdef __cplusplus
}
#endif

#endif
