#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydro/hydrogen.h"
#include "base64.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"

static void
removePadding(char *str)
{
	size_t len = strlen(str);
	while (str[len - 1] == '=') {
		len--;
		str[len] = 0;
	}
}

static void
genKeyPair(void)
{
	hydro_kx_keypair kp;
	hydro_kx_keygen(&kp);
	char *pk = b64_encode(kp.pk, sizeof(kp.pk));
	removePadding(pk);
	char *sk = b64_encode(kp.sk, sizeof(kp.sk));
	removePadding(sk);
	if (fprintf(stdout, "Keypair: %s,%s\n", pk, sk) < 0)
		tith_logError("Failed to write key pair");
	if (fflush(stdout) == EOF)
		tith_logError("Failed to flush stdout");
	free(sk);
	free(pk);
}

int
main(int argc, char **argv)
{
	char *cfname = NULL;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' || argv[i][0] == '/') {
			char *arg = &argv[i][1];
			while (*arg) {
				switch (*arg) {
					case 'k':
						genKeyPair();
						exit(EXIT_SUCCESS);
						break;
					case 'c':
						arg++;
						free(cfname);
						if (*arg)
							cfname = StrDup(arg);
						else {
							i++;
							cfname = StrDup(argv[i]);
						}
						break;
					default:
						tith_logError("Bad argument");
				}
			}
		}
	}
	struct TITH_Config *cfg = tith_readConfig(cfname);
	free(cfname);
	if (cfg->inbound == NULL)
		tith_logError("No inbound");

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
	if (hydro_kx_kk_2(&sessionKeyPair, packet2, cmd->next->value, sn->kp.pk, &dn->kp))
		tith_logError("Exchange failed");
	tith_freeTLV(cmd);

	cmd = tith_allocCmd(TITH_LHP2);
	tith_addData(cmd, TITH_KK_Packet2, sizeof(packet2), packet2);
	tith_sendCmd(cmd);
	tith_freeTLV(cmd);
	encrypting = true;

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
