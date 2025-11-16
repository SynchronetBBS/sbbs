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
