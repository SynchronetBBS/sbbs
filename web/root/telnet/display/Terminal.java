/*
 * Terminal -- Terminal emulation (abstract class)
 * --
 * $Id$
 * $timestamp: Wed Mar  5 11:27:13 1997 by Matthias L. Jugel :$
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

import java.awt.Panel;
import java.awt.Dimension;

/**
 * Terminal is an abstract emulation class.
 * It contains a character display.
 *
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 */
public abstract class Terminal extends Panel
{
	/**
	 * Get the specific parameter info for the emulation.
	 * @see java.applet.Applet
	 */
	public abstract String[][] getParameterInfo();
  
	/**
	 * Put a character on the screen. The method has to see if it is
	 * a special character that needs to be handles special.
	 * @param c the character
	 * @see #putString
	 */
	public abstract void putChar(char c);
	
	/**
	 * Put a character on the screen. The method has to parse the string
	 * may handle special characters.
	 * @param s the string
	 * @see #putString
	 */
	public abstract void putString(String s);

	/**
	 * Return the current size of the terminal in characters.
	 */
	public abstract Dimension getSize();
  
	/**
	 * Return actual terminal type identifier.
	 */
	public abstract String getTerminalType();
}
