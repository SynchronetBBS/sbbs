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
