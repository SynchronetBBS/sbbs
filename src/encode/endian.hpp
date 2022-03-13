/* Library to deal with big/little endian-specific data types */

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
 * https://www.gnu.org/licenses/old-licenses/lgpl-2.0.html					*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef ENDIAN_HPP_
#define ENDIAN_HPP_
 
#include "xpendian.h"

template <typename T>
class BigEndInt {
	T value{};
public:
	void operator = (T nval) {
		value = BE_INT(nval);
	}
	T operator = (const BigEndInt&) {
		return BE_INT(value);
	}
};

template <typename T>
class LittleEndInt {
	T value{};
public:
	void operator = (T nval) {
		value = LE_INT(nval);
	}
	T operator = (const LittleEndInt&) {
		return LE_INT(value);
	}
};

#endif // Don't add anything after this line
