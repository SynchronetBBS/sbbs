#ifdef _WIN32
__declspec(dllimport)
#endif
int dssh_abi_consumer_check(void);

int
main(void)
{
	return dssh_abi_consumer_check();
}
