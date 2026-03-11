/*
 * mocks_readcfg.h
 *
 * Shared mocks for readcfg.h functionality.  Provides Config struct and
 * configuration stubs used by 6+ test files.
 */

#ifndef MOCKS_READCFG_H
#define MOCKS_READCFG_H

struct config Config;

void AddInboundDir(const char *d)
{
	(void)d;
}

bool Config_Init(uint16_t n, struct NodeData *(*f)(int))
{
	(void)n; (void)f;
	return false;
}

void Config_Close(void)
{
}

#endif /* MOCKS_READCFG_H */
