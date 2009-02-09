//This file is used by some of the examples of this subdirectory.

int hextoi (int *rtn, char *s)
//converts a string (in hexadecimal notation) to integer
 {
	unsigned register a0; //return value
	unsigned register a1; //auxiliar variable
	a0=0;
	while (*s)
	 {
		a1=toupper (*s);
		if(a1>='0' && a1<='9')
			a1-='0';
		if(a1>='A' && a1<='F')
			a1-='7';
		if (a1>16u)
			return -1; //error in conversion
		a0*=16u;
		a0+=a1;
		s++;
	 }
	*rtn=a0;
	return 0;
 }

// rtn is the converted value.
// hextoi returns 0 if sucessful. On error it returns -1.