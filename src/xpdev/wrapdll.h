/* Defines DLLEXPORT for cross-platform development wrappers */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#if defined(DLLEXPORT)
	#undef DLLEXPORT
#endif

#if defined(_WIN32) && (defined(WRAPPER_IMPORTS) || defined(WRAPPER_EXPORTS))
	#if defined(WRAPPER_IMPORTS)
		#define DLLEXPORT   __declspec(dllimport)
	#else
		#define DLLEXPORT   __declspec(dllexport)
	#endif
#else   /* !_WIN32 || !_DLL*/
	#define DLLEXPORT
#endif

