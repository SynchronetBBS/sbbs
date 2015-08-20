#include <gen_defs.h>
#include <dirwrap.h>

char *get_ctrl_dir(char *path, size_t pathsz)
{
#ifdef PREFIX
	char	ini_file[MAX_PATH];
#endif
	char *p;

	p=getenv("SBBSCTRL");
	if(p!=NULL) {
		strncpy(path, p, pathsz);
		if(pathsz > 0)
			path[pathsz-1]=0;
		return path;
	}

#ifdef PREFIX
	strncpy(path, PREFIX"/etc", pathsz);
	if(pathsz > 0)
		path[pathsz-1]=0;
	iniFileName(ini_file, sizeof(ini_file)-1, PREFIX"/etc", "sbbs.ini");
	if(fexistcase(ini_file)) {
		FILE*	fini;
		char*	str;

		fini=iniOpenFile(ini_file, FALSE);
		if(fini==NULL)
			return NULL;
		str = iniReadExistingString(fini, "Global", "CtrlDirectory", NULL, ini_file);
		iniCloseFile(fini);
		return str;
	}
#endif
	return NULL;
}
