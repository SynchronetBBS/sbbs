#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydro/hydrogen.h"

#include "base64.h"
#include "tith-client.h"
#include "tith-common.h"
#include "tith-config.h"
#include "tith-server.h"

struct TITH_Config *cfg;

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
TITH_main(int argc, char **argv, void *handle)
{
	char *cfname = NULL;
	bool isServer = false;
	int firstNonOpt = 0;
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
							cfname = tith_strDup(arg);
						else {
							i++;
							cfname = tith_strDup(argv[i]);
						}
						break;
					case 's':
						arg++;
						isServer = true;
						break;
					default:
						tith_logError("Bad argument");
				}
			}
		}
		else {
			if (!firstNonOpt)
				firstNonOpt = i;
		}
	}
	if (!firstNonOpt && !isServer)
		tith_logError("No nodes specified");
	cfg = tith_readConfig(cfname);
	free(cfname);
	if (setjmp(tith_exitJmpBuf))
		return EXIT_FAILURE;
	if (isServer)
		return tith_server(handle);
	return(tith_client(argc - firstNonOpt, &argv[firstNonOpt], handle));
}
