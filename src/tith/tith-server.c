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

	tith_allocTLV(TITH_Info);
	tith_addData(tith_TLV, TITH_Message, 15, "HelloFromServer", true);
	tith_sendTLV();
	tith_freeTLV();

	tith_getTLV();
	tith_parseTLV(tith_TLV);
	tith_validateTLV(TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	fprintf(stderr, "Server got info message: %.*s\n", (int)tith_TLV->child->length, tith_TLV->child->value);
	tith_freeTLV();

	return EXIT_SUCCESS;
}
