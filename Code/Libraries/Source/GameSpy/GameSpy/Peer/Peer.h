#include "../../stub/include/gamespy/peer/peer.h"
#include "../../stub/include/gamespy/serverbrowsing/sb_serverbrowsing.h"

typedef SBServer GServer;

#undef peerSetTitle
#define peerSetTitle(peer, title, qrSecretKey, sbName, sbSecretKey, sbGameVersion, pingRooms, crossPingRooms) \
	peerSetTitleGenerals((peer), (title), (qrSecretKey), (sbName), (sbSecretKey), (sbGameVersion), (pingRooms), \
		(crossPingRooms))

PEERBool peerSetTitleGenerals(PEER peer, const gsi_char *title, const gsi_char *qrSecretKey,
	const gsi_char *sbName, const gsi_char *sbSecretKey, int sbGameVersion, PEERBool pingRooms[NumRooms],
	PEERBool crossPingRooms[NumRooms]);

void chatSetLocalIP(unsigned int ip);
