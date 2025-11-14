#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydro/hydrogen.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"

int
tith_server(void *handle)
{
	if (cfg->inbound == NULL)
		tith_logError("No inbound");
	tith_handle = handle;

	tith_getCmd();
	tith_validateCmd(TITH_IAm, 2, TITH_REQUIRED, TITH_AcceptingAddress, TITH_OPTIONAL, TITH_ConnectingAddress);
	struct TITH_Node *dn = tith_getNode(cfg, tith_cmd->next);
	if (!dn)
		tith_logError("Can't find destination node");
	if (!dn->hasSecretKey)
		tith_logError("No secret key for destination node");
	struct TITH_Node *sn = NULL;
	if (tith_cmd->next->next != NULL && tith_cmd->next->next->type == TITH_ConnectingAddress) {
		sn = tith_getNode(cfg, tith_cmd->next->next);
		if (!sn)
			tith_logError("No key for source node");
	}
	tith_freeCmd();

	tith_getCmd();
	tith_validateCmd(TITH_LHP1, 1, TITH_REQUIRED, TITH_KK_Packet1);
	if (sn) {
		uint8_t packet2[hydro_kx_KK_PACKET2BYTES];
		if (hydro_kx_kk_2(&tith_sessionKeyPair, packet2, tith_cmd->next->value, sn->kp.pk, &dn->kp))
			tith_logError("Exchange failed");
		tith_freeCmd();

		tith_allocCmd(TITH_LHP2);
		tith_addData(TITH_KK_Packet2, sizeof(packet2), packet2);
		tith_sendCmd();
		tith_freeCmd();
		tith_encrypting = true;
	}
	else {
		if (hydro_kx_n_2(&tith_sessionKeyPair, tith_cmd->next->value, NULL, &dn->kp))
			tith_logError("Exchange failed");
		tith_freeCmd();
		tith_encrypting = true;
	}

	tith_allocCmd(TITH_Info);
	tith_addData(TITH_Message, 15, "HelloFromServer");
	tith_sendCmd();
	tith_freeCmd();

	tith_getCmd();
	tith_validateCmd(TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Server got info message: %.*s\n", (int)tith_cmd->next->length, tith_cmd->next->value);
	tith_freeCmd();

	return EXIT_SUCCESS;
}
