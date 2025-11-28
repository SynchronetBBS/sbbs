/*
 * Implements FTS-5005 with some exceptions
 */

#include "tith-common.h"
#include "tith-config.h"
#include "tith-interface.h"

struct pendInfo {
	struct pendInfo *next;
	char *fileName;
	char flavour;
};

struct pendingAddress {
	char *destAddr;
	struct pendInfo *netmail;
	struct pendInfo *reference;
	char *request;
};

bool
bundleOutbound(const char *path, const char *domain, uint16_t zone)
{
	void *dir = openDirectory(path);
	for (const char *file = readdir(dir); file; file = readdir(dir)) {
		/*
		 * Hrm, we need routing info here to know what the
		 * source address should be.
		 */
	}
}

void
bundle(void)
{
	logString("Scaning outbounds");
	/*
	 * First, check the "main" outbound and any points, then check
	 * the outbound for any other domains with nodelists
	 * 
	 * Create bundles for all systems that support TITH
	 * TODO: Allow overriding this in a config somewhere.
	 */

	if (!isDir(cfg->outbound))
		tith_logError("Configured Outbound is not a directory");
	if (cfg->defaultDomain && cfg->defaultZone) {
		bundleOutbound(cfg->outbound, cfg->defaultDomain, cfg->defaultZone);
	}
}

int
TITH_main(int argc, char **argv, void *handle)
{
	if (setjmp(tith_exitJmpBuf)) {
		tith_cleanup();
		return EXIT_FAILURE;
	}
	char *cfname = NULL;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' || argv[i][0] == '/') {
			char *arg = &argv[i][1];
			while (*arg) {
				switch (*arg) {
					case 'c':
						arg++;
						if (cfname) {
							tith_popAlloc();
							free(cfname);
						}
						if (*arg)
							cfname = tith_strDup(arg);
						else {
							i++;
							cfname = tith_strDup(argv[i]);
						}
						tith_pushAlloc(cfname);
						break;
					default:
						tith_logError("Bad argument");
				}
			}
		}
		else {
			tith_logError("Bad argument");
		}
	}
	tith_readConfig(cfname);
	if (cfname) {
		tith_popAlloc();
		free(cfname);
	}

	bundle();

	tith_cleanup();
	return EXIT_SUCCESS;
}
