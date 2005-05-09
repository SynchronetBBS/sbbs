// $Id$

var host;
if((host=resolve_host(argv[0]))==null)
	false;	// Work-around for bug in v3.12a (segfault when decoding "null")
else
	host;