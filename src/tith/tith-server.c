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
	// We require the Origin here because we have no context.
	tith_validateTLV(tith_TLV, TITH_SignedTLV, 3, TITH_REQUIRED, TITH_Origin, TITH_REQUIRED, TITH_SignedData, TITH_REQUIRED, TITH_Signature);
	tith_parseTLV(tith_TLV->child->next);
	tith_parseTLV(tith_TLV->child->next->child);
	tith_validateTLV(tith_TLV->child->next->child, TITH_Info, 1, TITH_REQUIRED, TITH_Message);
	tith_parseTLV(tith_TLV->child->next->child->child);
	fprintf(stderr, "Server got info message: %.*s\n", (int)tith_TLV->child->next->child->child->length, tith_TLV->child->next->child->child->value);
	tith_freeTLV();

	return EXIT_SUCCESS;
}
