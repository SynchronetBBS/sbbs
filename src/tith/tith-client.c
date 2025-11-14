#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydro/hydrogen.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"

int
tith_client(int argc, char **argv, void *handle)
{
	char *cfname = NULL;
	char *source = NULL;
	char *dest = NULL;
	tith_handle = handle;
	for (int i = 0; i < argc; i++) {
		if (!dest) {
			dest = argv[i];
			tith_validateAddress(dest);
		}
		else if (!cfname) {
			source = argv[i];
			tith_validateAddress(source);
		}
	}
	if (!dest)
		tith_logError("No destination");
	if (!source)
		tith_logError("No source");
	struct TITH_Config *cfg = tith_readConfig(cfname);
	free(cfname);
	if (cfg->outbound == NULL)
		tith_logError("No outbound");

	struct TITH_TLV *cmd = tith_allocCmd(TITH_IAm);
	tith_addData(cmd, TITH_AcceptingAddress, strlen(dest), dest);
	tith_addData(cmd, TITH_ConnectingAddress, strlen(dest), source);
	struct TITH_Node *dn = tith_getNode(cfg, cmd->next);
	if (!dn)
		tith_logError("Can't find destination node");
	struct TITH_Node *sn = tith_getNode(cfg, cmd->next->next);
	if (!sn)
		tith_logError("No key for source node");
	if (!sn->hasSecretKey)
		tith_logError("No secret key for source node");
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);

	cmd = tith_allocCmd(TITH_LHP1);
	hydro_kx_state client_state;
	uint8_t packet1[hydro_kx_KK_PACKET1BYTES];
	if (hydro_kx_kk_1(&client_state, packet1, dn->kp.pk, &sn->kp))
		tith_logError("Error creating packet1");
	tith_addData(cmd, TITH_KK_Packet1, sizeof(packet1), packet1);
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);

	cmd = tith_getCmd();
	tith_validateCmd(cmd, TITH_LHP2, 1, TITH_REQUIRED, TITH_KK_Packet2);
	if (hydro_kx_kk_3(&client_state, &tith_sessionKeyPair, cmd->next->value, &sn->kp))
		tith_logError("Handshake failed");
	tith_freeTLV(cmd);
	tith_encrypting = true;

	cmd = tith_allocCmd(TITH_Info);
	tith_addData(cmd, TITH_Message, 15, "HelloFromClient");
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);

	cmd = tith_getCmd();
	tith_validateCmd(cmd, TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Client got info message: %.*s\n", (int)cmd->next->length, cmd->next->value);
	tith_freeTLV(cmd);

	return EXIT_SUCCESS;
}
