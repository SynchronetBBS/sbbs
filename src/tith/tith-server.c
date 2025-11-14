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

	struct TITH_TLV *cmd = tith_getCmd();
	tith_validateCmd(cmd, TITH_IAm, 2, TITH_REQUIRED, TITH_AcceptingAddress, TITH_REQUIRED, TITH_ConnectingAddress);
	struct TITH_Node *dn = tith_getNode(cfg, cmd->next);
	if (!dn)
		tith_logError("Can't find destination node");
	if (!dn->hasSecretKey)
		tith_logError("No secret key for destination node");
	struct TITH_Node *sn = tith_getNode(cfg, cmd->next->next);
	if (!sn)
		tith_logError("No key for source node");
	tith_freeTLV(cmd);

	cmd = tith_getCmd();
	tith_validateCmd(cmd, TITH_LHP1, 1, TITH_REQUIRED, TITH_KK_Packet1);
	uint8_t packet2[hydro_kx_KK_PACKET2BYTES];
	if (hydro_kx_kk_2(&tith_sessionKeyPair, packet2, cmd->next->value, sn->kp.pk, &dn->kp))
		tith_logError("Exchange failed");
	tith_freeTLV(cmd);

	cmd = tith_allocCmd(TITH_LHP2);
	tith_addData(cmd, TITH_KK_Packet2, sizeof(packet2), packet2);
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);
	tith_encrypting = true;

	cmd = tith_allocCmd(TITH_Info);
	tith_addData(cmd, TITH_Message, 15, "HelloFromServer");
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);

	cmd = tith_getCmd();
	tith_validateCmd(cmd, TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Server got info message: %.*s\n", (int)cmd->next->length, cmd->next->value);
	tith_freeTLV(cmd);

	return EXIT_SUCCESS;
}
