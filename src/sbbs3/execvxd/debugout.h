/* DEBUGOUT.H */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _DEBUGOUT_H_
#define _DEBUGOUT_H_

#ifndef DEBUG_VAR		// Pre-define to use different/dynamic debug enable var
#define DEBUG_VAR 0x1
#endif

#ifndef DEVICENAME		// Pre-define to display devicename on output
#define DEVICENAME "DBTRACE"
#endif

#ifndef DBTRACEDPF		// Pre-define if using other printf type function
#define DBTRACEDPF dprintf
#endif

#ifndef DBTFILEOFF		// Pre-define if __FILE__ offset required != 2
#define DBTFILEOFF 2
#endif

#ifndef TEXT			// Windows Unicode support
#define TEXT(x)		x
#endif

#ifndef DBTRACENL		// Pre-define if \n required
#define DBTRACENL TEXT("")
#endif

#if !defined(DBG) && !defined(DEBUG)

#define DBTRACE(b,s);			{ /* DBTRACE */ }
#define DBTRACEd(b,s,d);		{ /* DBTRACE */ }
#define DBTRACEs(b,s,t);		{ /* DBTRACE */ }
#define DBTRACEx(b,s,x);		{ /* DBTRACE */ }
#define DBTRACEdd(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACEds(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACEsd(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACEdx(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACExd(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACExx(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACEsx(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACExs(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACEss(b,s,x,y); 	{ /* DBTRACE */ }
#define DBTRACExxd(b,s,x,y,z);	{ /* DBTRACE */ }
#define DBTRACEddx(b,s,x,y,z);	{ /* DBTRACE */ }
#define DBTRACEdds(b,s,x,y,z);	{ /* DBTRACE */ }
#define DBTRACExdd(b,s,x,y,z);	{ /* DBTRACE */ }
#define DBTRACExxx(b,s,x,y,z);	{ /* DBTRACE */ }
#define DBTRACExsx(b,s,x,y,z);	{ /* DBTRACE */ }

#else /* DBG or DEBUG */

#define DBTRACE(b,s);			{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s%s") \
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s)					\
										,DBTRACENL); } }
#define DBTRACEd(b,s,d);		{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ") \
                                    	TEXT("%d%s")						\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),d						\
										,DBTRACENL); } }
#define DBTRACEs(b,s,t);		{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%s%s") 						\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),t						\
										,DBTRACENL); } }
#define DBTRACEx(b,s,x);		{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%02X%s")                            \
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x						\
										,DBTRACENL); } }
#define DBTRACEdd(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
										TEXT("%d %d%s")                     \
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACEds(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%d %s%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACEsd(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%s %d%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACEdx(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%d %X%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACExd(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %d%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACExx(b,s,x,y); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %X%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,y 					\
										,DBTRACENL); } }
#define DBTRACEsx(b,s,t,x); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%s %X%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),t,x 					\
										,DBTRACENL); } }
#define DBTRACExs(b,s,x,t); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %s%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,t 					\
										,DBTRACENL); } }
#define DBTRACEss(b,s,x,t); 	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%s %s%s")							\
										,DEVICENAME,TEXT(__FILE__+DBTFILEOFF) 	\
										,__LINE__,TEXT(s),x,t 					\
										,DBTRACENL); } }
#define DBTRACEddx(b,s,x,y,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%d %d %X%s")                  \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,y,z							\
										,DBTRACENL); } }
#define DBTRACEdds(b,s,x,y,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%d %d %s%s")                  \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,y,z							\
										,DBTRACENL); } }
#define DBTRACExdd(b,s,x,y,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %d %d%s")                        \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,y,z							\
                                        ,DBTRACENL); } }
#define DBTRACExxd(b,s,x,y,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %X %d%s")                        \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,y,z							\
                                        ,DBTRACENL); } }
#define DBTRACExxx(b,s,x,y,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %X %X%s")                        \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,y,z							\
										,DBTRACENL); } }
#define DBTRACExsx(b,s,x,t,z);	{ if(DEBUG_VAR&(1<<b)) {					\
									DBTRACEDPF(TEXT("%-8s (%-12.12s %4d): %s: ")\
                                    	TEXT("%X %s %X%s")                        \
										,DEVICENAME 						\
										,TEXT(__FILE__+DBTFILEOFF),__LINE__		\
										,TEXT(s),x,t,z							\
										,DBTRACENL); } }

#endif

#endif	// Don't add anything after this line
