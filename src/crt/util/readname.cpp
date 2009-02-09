//Function used by programs to append extensions in a filename if filename
//has no extension.

//strtarget is the target string
//strsource is the source string
//ext is the filename extension

//By M rcio Afonso Arimura Fialho

void readfname (char *strtarget, char *strsource, char *ext) //reads filename
 {
	int c0;
	char *ps,*pt;

	c0=0;
	ps=strsource;
	pt=strtarget;
	while (*ps) //reads Source file filename.
	 {
		*pt=*ps;
		c0++;
		if (c0==123) //file name mustn't exceed the lenght of 127 characters
			break;
		ps++;
		pt++;
	 }
	*pt=0;
	for (;(char huge *)pt!=(char huge *)strtarget;)
	 {
		pt--;
		if (*pt=='.')
			return;
		if (*pt=='\\')
			break;
	 }
	 strcat(strtarget,ext); //if no extension was given to filename, appends ext
 }
