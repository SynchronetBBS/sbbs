/**
 * Script -- A module for scripting (very simple).
 * --
 * $Id$
 * $timestamp: Mon Mar 24 15:52:12 1997 by Matthias L. Jugel :$
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
import java.util.Hashtable;
import java.util.Enumeration;
import java.awt.Dialog;
import java.awt.Frame;
import java.awt.TextField;
import java.awt.Label;
import java.awt.Event;

/**
 * A very simple scripting module. It takes pairs of pattern and text and
 * sends the corresponding text when the pattern matches. Each pattern is
 * only matched once per connected session.
 *
 * <DL>
 *  <DT><B>Scripts:</B>
 *   <DD><PRE>&lt;PARAM NAME=script VALUE=&quot;<B><I>pattern</I></B>|<B><I>text</I></B>|<B><I>...</I></B>&quot;&gt;</PRE>
 *   <DD>A script contains of pairs of <I>pattern</I> and <I>text</I> strings.
 *       If the pattern is matched against the output from the remote host,
 *       the corresponding text will be sent. Each pattern will match only
 *       <B>once</B> per session. A session is defined by connect and 
 *       disconnect.<P>
 *       Thus it is possible to program an autologin as follows:<BR>
 *       <PRE><B>"login:|leo|Password:|mypassword|leo@www|ls"</B></PRE>
 *       Newlines will be added automatically to the string sent! At the
 *       moment the order of the pattern and text pairs is <I>not</I> relevant.
 *       <P>
 *       It is possible to prompt the user for input if a match occurs. If the
 *       corresponding <I>text</I> is a string enclosed in braces ([] or {}) a
 *       dialog window is opened with <I>text</I> as prompt. A special case 
 *       is an empty prompt in which case the <I>pattern</I> will be shown as 
 *       prompt. &quot;[Your name:]&quot; would open a dialog window with the
 *       text &quot;Your name&quot; as prompt. Curly braces have a special
 *       meaning; any user input will be shown as &quot;*&quot; which makes
 *       it possible to program password prompts. Example: 
 *       &quot;{Your password:}&quot;.<P>
 *       A special match like: &quot;login:|[]&quot; can be used to open a
 *       dialog and display &quot;login:&quot; as prompt. This works for
 *       &quot;{}&quot; as well.
 *       
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meißner
 * @see modules.Module
 */
public class Script extends Hashtable implements Module
{
  // This is the target for any text we want to send
  private telnet applet = null;

  /**
   * Set the applet as module loader
   * @param o the object that is the applet (must be an Applet)
   * @see module.Module
   * @see java.applet.Applet
   */
  public void setLoader(Object o) { applet = (telnet)o; }
	
  /**
   * Configure the script module by reading the script PARAMeter.
   * @param host remote hostaddress - not used
   * @param port remote port - not used
   */
  public void connect(String host, int port) {
    String tmp = applet.getParameter("script");
    
    // delete all entries
    clear();
    
    if(tmp != null) {
      int idx = tmp.indexOf('|');
      int oldidx = 0;
      while(idx >= 0) {
	String match = tmp.substring(oldidx, idx);
	oldidx = idx;
	idx = tmp.indexOf('|', idx+1);
	idx = idx < 0 ? idx = tmp.length() : idx;
	String send = tmp.substring(oldidx+1, idx);
	put(match, send);
	oldidx = idx+1;
	idx = tmp.indexOf('|', idx+1);
      }
    }
  }

  /**
   * Get notified of disconnection. Do nothing.
   */
  public void disconnect() {}
	
  /**
   * This method is called when data is received. It tries to match the
   * input to the list of patterns and sends corresponding text on success.
   * If the response is [] or {} the user will be prompted with the matching
   * text. You can modify the prompt string by entering it inside of the
   * brackets or curly braces (e.g. [Enter your id:]). In case of curly
   * braces the input area will not show the typed in text (for passwords)!
   *
   * @param s The string to test.
   * @see peer.InputPeer
   */
  public String receive(String s) {
    if(isEmpty()) return s;
    Enumeration match = keys();
    while(match.hasMoreElements()) {
      String key = (String)match.nextElement();
      if(s.indexOf(key) != -1) {
	String value = (String)get(key);
	if(value.indexOf("[") == 0 || value.indexOf("{") == 0) {
	  TextField input = new TextField(20);
	  if(value.startsWith("{")) input.setEchoCharacter('*');
	  if("[]".equals(value) || "{}".equals(value)) value = key;
	  else value = value.substring(1, value.length() - 1);
	  Thread current = Thread.currentThread();
	  new UserDialog(new Frame(), value, false, current, input);
	  current.suspend();
	  value = input.getText();
	}
	applet.send(value + "\r");
	remove(key);
      }
    }
    return s;
  }
}

class UserDialog extends Dialog {
  TextField input;
  Thread thread;
  String value;

  public UserDialog(Frame parent, String value, boolean modal, 
		    Thread t, TextField reply) {
    super(parent, value, modal);
    thread = t; input = reply;
    add("West", new Label(value));
    add("Center", input);
    pack();
    show();
  }

  public boolean handleEvent(Event evt) {
    if(evt.target == input && evt.key == 10) {
      thread.resume();
      hide(); dispose();
      return true;
    }
    return false;
  }
}
