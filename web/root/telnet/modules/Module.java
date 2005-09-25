/**
 * Module -- Module interface
 * --
 * $Id$
 * $timestamp: Mon Mar 24 15:35:13 1997 by Matthias L. Jugel :$
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

package modules;

/**
 * Modules must implement this interface to be detected as valid modules
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 */
public interface Module 
{
	/**
	 * Set the loader of the module. This is necessary to know if you want to
	 * contact the modules parent.
	 * @param loader The object that has loaded this module.
	 */
	public void setLoader(Object loader);

	/**
	 * Connected to the remote host. This method notifies upon new connection.
	 * @param host remote hostname
	 * @param port remote port
	 */
	public void connect(String host, int port);
	
	/**
	 * Disconnect from the host. This method notifies of lost connection.
	 */
	public void disconnect();
	
	/**
	 * Receive data from somewhere. If a modules does not want to receive data
	 * it should return null to remove itself from the list of receiver modules.
	 * @param s The string we receive.
   * @return the modified string or null (to remove from receiver list)
	 */
	public String receive(String s);
}

