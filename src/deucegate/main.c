#include "deucegate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(FILE *out)
{
	fputs("DeuceGate - SSH-native GameSrv-compatible door server\n\n"
	    "Usage:\n"
	    "  deucegate [--root DIR]\n"
	    "  deucegate [--root DIR] --check-config\n"
	    "  deucegate [--root DIR] --authorize ALIAS PUBLIC_KEY [--replace]\n", out);
}

int
main(int argc, char **argv)
{
	const char *root = ".", *alias = NULL, *key = NULL;
	bool check = false, replace = false;
	dg_config_t cfg;
	char err[512];
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) root = argv[++i];
		else if (strcmp(argv[i], "--check-config") == 0) check = true;
		else if (strcmp(argv[i], "--authorize") == 0 && i + 2 < argc) {
			alias = argv[++i]; key = argv[++i];
		}
		else if (strcmp(argv[i], "--replace") == 0) replace = true;
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			usage(stdout); return 0;
		}
		else { usage(stderr); return 2; }
	}
	if (!dg_config_load(root, &cfg, err, sizeof(err))) {
		fprintf(stderr, "deucegate: %s\n", err);
		return 1;
	}
	if (check)
		return dg_config_check(&cfg, stdout) ? 0 : 1;
	if (alias != NULL) {
		if (!dg_user_authorize(&cfg, alias, key, replace, err, sizeof(err))) {
			fprintf(stderr, "deucegate: %s\n", err);
			return 1;
		}
		printf("Authorized SSH key for %s.\n", alias);
		return 0;
	}
	return dg_server_run(&cfg);
}
