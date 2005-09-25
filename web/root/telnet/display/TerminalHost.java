/*
 * TerminalHost -- this interface defines the remote end of the connection
 *                 from our Terminal to the Host (virtual).
 * --
 * $Id$
 * $timestamp: Wed Mar  5 12:01:31 1997 by Matthias L. Jugel :$
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

package display;

/**
 * TerminalHost is an interface for the remote (virtual) end of our connection
 * to the host computer we are connected to.
 * @version $Id$
 * @author Matthias L Jugel, Marcus Meiﬂner
 */
public interface TerminalHost
{
	/**
	 * Send a string to the host and return if it was received successfully.
	 * @param s the string to send
	 * @return True for successful receivement.
	 */
	public boolean send(String s);
}

