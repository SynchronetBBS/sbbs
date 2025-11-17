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

	tith_allocTLV(TITH_Info);
	tith_addData(tith_TLV, TITH_Message, 15, "HelloFromClient", true);
	tith_sendTLV();
	tith_freeTLV();

	tith_getTLV();
	tith_parseTLV(tith_TLV);
	tith_validateTLV(TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Client got info message: %.*s\n", (int)tith_TLV->child->length, tith_TLV->child->value);
	tith_freeTLV();

	return EXIT_SUCCESS;
}
