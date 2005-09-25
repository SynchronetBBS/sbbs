/**
 * TextLabel -- A module to display a Label on the applet.
 * --
 * $Id$
 * $timestamp: Wed Jul  9 17:37:28 1997 by Matthias L. Jugel :$
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

import telnet;
import java.awt.Panel;
import java.awt.Label;
import java.awt.GridLayout;
import java.awt.Font;

/**
 * This small module lets you display text somewhere in the applets area.
 *
 * <DL>
 *  <DT><B>Label:</B>
 *   <DD><PRE>&lt;PARAM NAME=labelRows    VALUE=&quot;<B><I>rows</B></I>&quot;&gt;</PRE>
 *   <DD>Defines the how many rows the label will have.
 *   <DD><PRE>&lt;PARAM NAME=labelCols    VALUE=&quot;<B><I>cols</B></I>&quot;&gt;</PRE>
 *   <DD>Defines the how many columns the label will have.
 *   <DD><PRE>&lt;PARAM NAME=labelFont    VALUE=&quot;<B><I>font[,size]</B></I>&quot;&gt;</PRE>
 *   <DD>The font for displaying the label text. If the <I>size</I> is left out
 *       a standard size of 14 points is assumed.
 *   <DD><PRE>&lt;PARAM NAME=label#<I>number</I> VALUE=&quot;<B><I>text</I></B>&quot;&gt;</PRE>
 *   <DT>The labels are enumerated and displayed in rows and columns.
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 * @see modules.Module
 */
public class TextLabel extends Panel implements Module
{
  telnet applet;
  
	/**
	 * Set the applet as module loader and configure.
	 * @param o the object that is the applet (must be an Applet)
	 * @see module.Module
	 * @see java.applet.Applet
	 */
	public void setLoader(Object o) { 
    applet = (telnet)o;

    int rows = 1, cols = 1;
    
    String tmp = applet.getParameter("labelRows");
    if(tmp != null) rows = Integer.parseInt(tmp);
    if((tmp = applet.getParameter("labelCols")) != null)
      cols = Integer.parseInt(tmp);

    setLayout(new GridLayout(rows, cols));

    Font labelFont = null;
    if((tmp = applet.getParameter("labelFont")) != null) {
      int idx = tmp.indexOf(",");
      int size = 14;
      if(idx != -1) size = Integer.parseInt(tmp.substring(idx+1));
      labelFont = new Font(tmp, Font.PLAIN, size);
    }
    
    int no = 1;
    while((tmp = applet.getParameter("label#"+no++)) != null) {
      Label text = new Label(tmp);
      if(labelFont != null) text.setFont(labelFont);
      add(text);
    }
  }
	
	/**
	 * Do nothing upon connect.
	 * @param host remote hostaddress - not used
	 * @param port remote port - not used
	 */
	public void connect(String host, int port) {}
  
	/**
	 * Do nothing upon disconnecton.
	 */
	public void disconnect() {}
	
	/**
   * Do nothing when receiving text. Be removed upon first call.
	 * @param s The string received.
	 * @see peer.InputPeer
	 */
	public String receive(String s) { return null; }
}

