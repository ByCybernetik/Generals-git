#include "../../stub/include/gamespy/gp/gp.h"

#undef gpInitialize
#define gpInitialize(connection, productID) gpInitializeGenerals((connection), (productID))

#undef gpConnectNewUser
#define gpConnectNewUser(connection, nick, email, password, firewall, blocking, callback, param) \
	gpConnectNewUserGenerals((connection), (nick), (email), (password), (firewall), (blocking), (callback), (param))

GPResult gpInitializeGenerals(GPConnection *connection, int productID);
GPResult gpConnectNewUserGenerals(GPConnection *connection, const gsi_char *nick, const gsi_char *email,
	const gsi_char *password, GPEnum firewall, GPEnum blocking, GPCallback callback, void *param);
