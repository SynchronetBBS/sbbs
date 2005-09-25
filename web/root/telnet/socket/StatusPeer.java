/**
 * Status peer interface.
 * --
 * $Id$
 * $timestamp: Wed Mar  5 13:40:54 1997 by Matthias L. Jugel :$
 *
 * This file is part of "The Java Telnet Applet".
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * "The Java Telnet Applet" is distributed in the hope that it will be 
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

package socket;

import java.util.Vector;

/**
 * StatusPeer -- interface for status messages
 * --
 * @version	$Id$
 * @author	Matthias L. Jugel, Marcus Meiﬂner
 */

public interface StatusPeer
{
	/**
	 * This method is called for the peer of the TelnetIO class if there is
	 * a statuschange.
	 * @param status A Vector containing the key as element 0 and any arguments
	 *               from element 1 on.
	 * @return an object that matches the requested information or null
	 * @see socket.TelnetIO
	 */
  public Object notifyStatus(Vector status);
}
