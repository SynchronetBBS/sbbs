/*
 *%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 *%	  Copyright (C) 1989, by WATCOM Systems Inc. All rights     %
 *%	  reserved. No part of this software may be reproduced	    %
 *%	  in any form or by any means - graphic, electronic or	    %
 *%	  mechanical, including photocopying, recording, taping     %
 *%	  or information storage and retrieval systems - except     %
 *%	  with the written permission of WATCOM Systems Inc.	    %
 *%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  WILDARGV - split DOS command line into individual arguments expanding
	     those that contain ? or *.
  This module is a substitute for the "initargv" module contained in the
  library.

  Modified:	By:		Reason:
  ---------	---		-------
  23-aug-89	John Dahms	was ignoring files with Archive or
				read only attributes turned on. (Bug fix)
  15-sep-91	F.W.Crigger	Use _LpCmdLine, _LpPgmName, _argc, _argv,
  				___Argc, ___Argv
  13-jul-92	John Dahms	add (void near *) cast in _allocate
  02-nov-93	A.F.Scian	fixed so that it compiles as C++
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

extern	void	_Not_Enough_Memory();
extern	char	*_LpCmdLine;
extern	char	*_LpPgmName;
extern	int	_argc;			/* argument count  */
extern	char  **_argv;			/* argument vector */
extern	int	___Argc;		/* argument count */
extern	char  **___Argv;		/* argument vector */

#ifdef __cplusplus
};
#endif


static void *_allocate( unsigned amount )
    {
	void *p;

#if defined(__386__)
	p = malloc( amount );
#else
	p = _nmalloc( amount );
    #if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	if( (void near *) p == NULL )  p = malloc( amount );
    #endif
#endif
	if( p == NULL )  _Not_Enough_Memory();
	return( p );
    }


static int _make_argv( char *p, char ***argv )
    {
	int		argc;
	char		*start;
	char		*new_arg;
	char		wildcard;
	char		lastchar;
	DIR *		dir;
	struct dirent * dirent;
	char		drive[_MAX_DRIVE];
	char		directory[_MAX_DIR];
	char		name[_MAX_FNAME];
	char		extin[_MAX_EXT];
	char		pathin[_MAX_PATH];

	argc = 1;
	for(;;) {
	    while( *p == ' ' ) ++p;	/* skip over blanks */
	    if( *p == '\0' ) break;
	    /* we are at the start of a parm */
	    wildcard = 0;
	    if( *p == '\"' ) {
		p++;
		new_arg = start = p;
		for(;;) {
		    /* end of parm: NULLCHAR or quote */
		    if( *p == '\"' ) break;
		    if( *p == '\0' ) break;
		    if( *p == '\\' ) {
			if( p[1] == '\"'  ||  p[1] == '\\' )  ++p;
		    }
		    *new_arg++ = *p++;
		}
	    } else {
		new_arg = start = p;
		for(;;) {
		    /* end of parm: NULLCHAR or blank */
		    if( *p == '\0' ) break;
		    if( *p == ' ' ) break;
		    if(( *p == '\\' )&&( p[1] == '\"' )) {
			++p;
		    } else if( *p == '?'  ||  *p == '*' ) {
			wildcard = 1;
		    }
		    *new_arg++ = *p++;
		}
	    }
	    *argv = (char **) realloc( *argv, (argc+2) * sizeof( char * ) );
	    if( *argv == NULL )  _Not_Enough_Memory();
	    (*argv)[ argc ] = start;
	    ++argc;
	    lastchar = *p;
	    *new_arg = '\0';
	    ++p;
	    if( wildcard ) {
		/* expand file names */
		dir = opendir( start );
		if( dir != NULL ) {
		    --argc;
		    _splitpath( start, drive, directory, name, extin );
		    for(;;) {
			dirent = readdir( dir );
			if( dirent == NULL ) break;
			if( dirent->d_attr &
			  (_A_HIDDEN+_A_SYSTEM+_A_VOLID+_A_SUBDIR) ) continue;
			_splitpath( dirent->d_name, NULL, NULL, name, extin );
			_makepath( pathin, drive, directory, name, extin );
			*argv = (char **) realloc( *argv, (argc+2) * sizeof( char * ) );
			if( *argv == NULL )  _Not_Enough_Memory();
			new_arg = (char *) _allocate( strlen( pathin ) + 1 );
			strcpy( new_arg, pathin );
			(*argv)[argc++] = new_arg;
		    }
		    closedir( dir );
		}
	    }
	    if( lastchar == '\0' ) break;
	}
	return( argc );
    }


#ifdef __cplusplus
extern "C"
#endif
void __Init_Argv()
    {
	_argv = (char **) _allocate( 2 * sizeof( char * ) );
	_argv[0] = _LpPgmName;	/* fill in program name */
	_argc = _make_argv( _LpCmdLine, &_argv );
	_argv[_argc] = NULL;
	___Argc = _argc;
	___Argv = _argv;
    }
