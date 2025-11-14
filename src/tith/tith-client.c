#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "hydro/hydrogen.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"
#include "tith-interface.h"

int
tith_client(int argc, char **argv, void *handle)
{
	char *source = NULL;
	char *dest = NULL;
	tith_handle = handle;
	for (int i = 0; i < argc; i++) {
		if (!dest) {
			dest = argv[i];
			tith_validateAddress(dest);
		}
		else if (!source) {
			source = argv[i];
			tith_validateAddress(source);
		}
	}
	if (!dest)
		tith_logError("No destination");
	if (!source)
		logString("No source address, assuming #-1:-1/-1.-1");
	if (cfg->outbound == NULL)
		tith_logError("No outbound");

	tith_allocCmd(TITH_IAm);
	tith_addData(TITH_AcceptingAddress, strlen(dest), dest);
	if (source)
		tith_addData(TITH_ConnectingAddress, strlen(dest), source);
	struct TITH_Node *dn = tith_getNode(cfg, tith_cmd->next);
	if (!dn)
		tith_logError("Can't find destination node");
	struct TITH_Node *sn = NULL;
	if (source) {
		sn = tith_getNode(cfg, tith_cmd->next->next);
		if (!sn)
			tith_logError("No key for source node");
		if (!sn->hasSecretKey)
			tith_logError("No secret key for source node");
	}
	tith_sendCmd();
	tith_freeCmd();

	if (source) {
		tith_allocCmd(TITH_LHP1);
		hydro_kx_state client_state;
		uint8_t packet1[hydro_kx_KK_PACKET1BYTES];
		if (hydro_kx_kk_1(&client_state, packet1, dn->kp.pk, &sn->kp))
			tith_logError("Error creating packet1");
		tith_addData(TITH_KK_Packet1, sizeof(packet1), packet1);
		tith_sendCmd();
		tith_freeCmd();

		tith_getCmd();
		tith_validateCmd(TITH_LHP2, 1, TITH_REQUIRED, TITH_KK_Packet2);
		if (hydro_kx_kk_3(&client_state, &tith_sessionKeyPair, tith_cmd->next->value, &sn->kp))
			tith_logError("Handshake failed");
		tith_freeCmd();
		tith_encrypting = true;
	}
	else {
		tith_allocCmd(TITH_LHP1);
		uint8_t packet1[hydro_kx_N_PACKET1BYTES];
		if (hydro_kx_n_1(&tith_sessionKeyPair, packet1, NULL, dn->kp.pk))
			tith_logError("Error creating packet1");
		tith_addData(TITH_KK_Packet1, sizeof(packet1), packet1);
		tith_sendCmd();
		tith_freeCmd();
		tith_encrypting = true;
	}

	tith_allocCmd(TITH_Info);
	tith_addData(TITH_Message, 15, "HelloFromClient");
	tith_sendCmd();
	tith_freeCmd();

	tith_getCmd();
	tith_validateCmd(TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Client got info message: %.*s\n", (int)tith_cmd->next->length, tith_cmd->next->value);
	tith_freeCmd();

	return EXIT_SUCCESS;
}
