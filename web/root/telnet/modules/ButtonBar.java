/**
 * ButtonBar -- a programmable button bar
 * --
 * $Id$
 * $timestamp: Mon Aug  4 14:12:21 1997 by Matthias L. Jugel :$
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
import frame;

import java.applet.Applet;
import java.util.Hashtable;
import java.util.Vector;

import java.awt.Panel;
import java.awt.Button;
import java.awt.TextField;
import java.awt.Event;
import java.awt.BorderLayout;
import java.awt.GridLayout;
import java.awt.Dimension;
import java.awt.Container;
import java.awt.Frame;

/**
 * This class implements a programmable button bar.
 * You can add <A HREF="#buttons">Buttons</A> and <A HREF="#fields">Input 
 * fields</A> to trigger actions in the <A HREF="telnet.html">telnet 
 * applet</A>. On how to load a module, please refer to the 
 * <A HREF="telnet.html">telnet</A> documentation.
 * 
 * <DL>
 *  <A NAME="buttons"></A>
 *  <DT><B>Buttons:</B>
 *  <DD><TT>&lt;PARAM NAME=<B><I>number</I></B>#Button VALUE=&quot;<B><I>buttontext</I></B>|<B><I>buttonaction</I></B>&quot;&gt;</TT>
 *  <DD><B><I>number</I></B> is the sequence number and determines the place
 *      of the button on the row.
 *      <P>
 *  <DD><B><I>buttontext</I></B> is a string displayed on the button.
 *      <P>
 *  <DD><A NAME="buttonaction"><B><I>buttonaction</I></B></A> may be one
 *      of the following functions or strings<BR>
 *      <FONT SIZE=-1>(<I>Note:</I> the backslash character
 *      in front of the dollar sign is mandatory!)</FONT>
 *      <UL>
 *       <LI><TT><I>simple text</I></TT>
 *           to be sent to the remote host. Newline and/or carriage return
 *           characters may be added in C syntax <B>\n</B> and <B>\r</B>.
 *           To support unimplemented function keys the <B>\e</B> escape
 *           character may be useful. The <B>\b</B> backspace character is
 *           also supported.
 *           The text may contain <A HREF="#fieldreference"><B><I>field 
 *           reference(s)</I></B></A>.
 *           <P>
 *       <LI><TT>\$connect(<B><I>host</I></B>[,<B><I>port</I></B>])</TT> 
 *           tries to initiate a connection to the <B><I>host</I></B>
 *           at the <B><I>port</I></B>, if given. The standard port is
 *           23. <B><I>host</I></B> and <B><I>port</I></B> may be hostname
 *           and number or <A HREF="#fieldreference"><B><I>field
 *           reference(s)</I></B></A>. If a connection already exists
 *           nothing will happen.<BR>
 *           <FONT SIZE=-1>(<I>Note:</I> It is not allowed to have
 *           spaces anywhere inside the parenthesis!)</FONT>
 *           <P>
 *       <LI><TT>\$disconnect()</TT>
 *           terminates the current connection, but if there was no
 *           connection nothing will happen.
 *           <P>
 *       <LI><TT>\$detach()</TT>
 *           detaches the applet from the web browser window and
 *           creates a new frame externally. This may be used to allow
 *           users to use the applet while browsing the web with the
 *           same browser window.<BR>
 *           <FONT SIZE=-1>(<I>Note:</I> You need to load the applet via the
 *           <A HREF="appWrapper.html">appWrapper class</A> or
 *           it will not work properly!)</FONT>
 *      </UL>
 *  <DD><B>Examples:</B><BR>
 *      <FONT SIZE=-1>(<I>Note:</I> It makes sense if you look at the
 *      examples for input fields below.)</FONT>
 *      <PRE>
 *        &lt;PARAM NAME=1#Button VALUE="HELP!|help\r\n"&gt;
 *        &lt;PARAM NAME=2#Button VALUE="HELP:|help \@help@\r\n"&gt;
 *        &lt;PARAM NAME=4#Button VALUE="simple|\$connect(localhost)"&gt;
 *        &lt;PARAM NAME=5#Button VALUE="complete|\$connect(www,4711)"&gt;
 *        &lt;PARAM NAME=6#Button VALUE="connect|\$connect(\@address@)"&gt;
 *        &lt;PARAM NAME=8#Button VALUE="connect to port|\$connect(\@address@,\@port@)"&gt;
 *        &lt;PARAM NAME=10#Button VALUE="window|\$detach()"&gt;
 *     </PRE>
 *     <P>
 *  <A NAME="fields"></A>
 *  <DT><B>Input fields</B>
 *  <DD><TT>&lt;PARAM NAME=<B><I>number</I></B>#Input VALUE=&quot;<B><I>fieldname</I></B>[#<I><B>length</B></I>]|<B><I>initial text</I></B>[|<B><I>action</I></B>]&quot;&gt;</TT>
 *  <DD><B><I>number</I></B> is the sequence number and determines the place
 *      of the field on the row.
 *      <P>
 *  <DD><A NAME="fieldreference"><B><I>fieldname</I></B></A> is a
 *      symbolic name to reference the input field. A reference may be used in 
 *      <A HREF="#buttonaction"><B><I>button actions</I></B></A> and
 *      is constructed as follows:
 *      <TT>\@<B><I>fieldname</I></B>@</TT>
 *      The <B>\@fieldname@</B> macro will be replaced by the string entered in
 *      the text field.
 *      <P>
 *  <DD><B><I>length</I></B> is the length of the input field in numbers of
 *      characters.
 *      <P>
 *  <DD><B><I>initial text</I></B> is the text to be placed into the input
 *      field on startup
 *  <DD><B><I>action</I></B> may be used similar to a 
 *      <A HREF="#buttonaction"><B><I>button action</I></B></A>. This action 
 *      will be used if the users presses Return in the inputfield. Leave
 *      empty if you only want to use a button to send the text!
 *  <DD><B>Examples:</B><BR>
 *      <FONT SIZE=-1>(<I>Note:</I> It makes sense if you look at the
 *      examples for buttons before.)</FONT>
 *      <PRE>
 *        &lt;PARAM NAME=3#Input VALUE="help#10|"&gt;
 *        &lt;PARAM NAME=7#Input VALUE="address|www.first.gmd.de"&gt;
 *        &lt;PARAM NAME=8#Input VALUE="send#5|who|\@send@\r\n"&gt;
 *        &lt;PARAM NAME=9#Input VALUE="port#5|4711"&gt;
 *      </PRE>
 *      <P>
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 * @see modules.Module
 */
public class ButtonBar extends Panel implements Module
{
  // our parent is the telnet app
  private telnet parent;
  
  // these tables contain our buttons and fields.
  private Hashtable buttons = null;
  private Hashtable fields = null;

  // the top level (for detaching)
  private Container toplevel;

  /**
   * This method is called by our loader to notify us of it. 
   * @param o The object that has loaded this object.
   * @see display.Module
   */
  public void setLoader(Object o) { parent = (telnet)o; }

  /**
   * If the applet connects this method is called.
   * @param host remote hostaddress - not used
   * @param port remote port - not used
   */
  public void connect(String host, int port) {
    // do nothing yet.
  }

  /**
   * Get notified of disconnection. Do nothing.
   */
  public void disconnect() {
    // do nothing yet
  }

  /**
   * This module does not take any input. It works passive.
   * @return null to remove from the list of receiver modules.
   * @see display.Module
   */
  public String receive(String s) { return null; }
	
  /**
   * create the buttonbar from the parameter list. We will know our parent,
   * when we have been added.
   */
  public void addNotify() {
    if(buttons == null && fields == null) {
      String tmp; 

      int nr = 1;
      String button = null, input = null;
      while((button = parent.getParameter(nr+"#Button")) != null ||
	    (input = parent.getParameter(nr+"#Input")) != null) {
	nr++;
	if(button != null) {
	  if(buttons == null) buttons = new Hashtable();
	  int idx = button.indexOf('|');
	  if(button.length() == 0)
	    System.out.println("ButtonBar: Button: no definition");
	  if(idx < 0 || idx == 0) {
	    System.out.println("ButtonBar: Button: empty name \""+button+"\"");
	    continue;
	  }
	  if(idx == button.length() - 1) {
	    System.out.println("ButtonBar: Button: empty command \""+button+"\"");
	    continue;
	  }
	  Button b = new Button(button.substring(0, idx));
	  buttons.put(b, button.substring(idx+1, button.length()));
	  add(b);
	} else
	  if(input != null) {
	    if(fields == null) fields = new Hashtable();
	    if(buttons == null) buttons = new Hashtable();
	    int idx = input.indexOf('|');
	    if(input.length() == 0)
	      System.out.println("ButtonBar: Input field: no definition");
	    if(idx < 0 || idx == 0) {
	      System.out.println("ButtonBar: Input field: empty name \""+input+"\"");
	      continue;
	    }
	    int si, size;
	    if((si = input.indexOf('#', 0)) == 0) {
	      System.out.println("ButtonBar: Input field: empty name");
	      continue;
	    }
	    if(si < 0 || si == idx-1) size = 10;
	    else size = Integer.parseInt(input.substring(si+1, idx));
	    TextField t = 
	      new TextField(input.substring(idx + 1, 
					    input.lastIndexOf('|') == idx ?
					    input.length() : 
					    (idx = input.lastIndexOf('|'))),
			    size);
	    buttons.put(t, input.substring(idx + 1, input.length()));
	    fields.put(input.substring(0, (si < 0 ? idx : si)), t);
	    add(t);
	  }
	button = input = null;
      }
    }
    super.addNotify();
  }

  public boolean handleEvent(Event evt) {
    String tmp;
    if(evt.id == Event.ACTION_EVENT &&
       (tmp = (String)buttons.get(evt.target)) != null) {
      System.out.println("ButtonBar: "+tmp);
      String cmd = "", function = null;
      int idx = 0, oldidx = 0;
      while((idx = tmp.indexOf('\\', oldidx)) >= 0 && 
	    ++idx <= tmp.length()) {
	cmd += tmp.substring(oldidx, idx-1);
	switch(tmp.charAt(idx)) {
	case 'b': cmd += "\b"; break;
	case 'e': cmd += ""; break;
	case 'n': cmd += "\n"; break;
	case 'r': cmd += "\r"; break;
	case '$': {
	  int ni = tmp.indexOf('(', idx+1);
	  if(ni < idx) {
	    System.out.println("ERROR: Function: missing '('");
	    break;
	  }
	  if(ni == ++idx) {
	    System.out.println("ERROR: Function: missing name");
	    break;
	  }
	  function = tmp.substring(idx, ni);
	  idx = ni+1;
	  ni = tmp.indexOf(')', idx);
	  if(ni < idx) {
	    System.out.println("ERROR: Function: missing ')'");
	    break;
	  }
	  tmp = tmp.substring(idx, ni);
	  idx = oldidx = 0;
	  continue;
	}
	case '@': {
	  int ni = tmp.indexOf('@', idx+1);
	  if(ni < idx) {
	    System.out.println("ERROR: Input Field: '@'-End Marker not found");
	    break;
	  }
	  if(ni == ++idx) {
	    System.out.println("ERROR: Input Field: no name specified");
	    break;
	  }
	  String name = tmp.substring(idx, ni);
	  idx = ni;
	  TextField t;
	  if(fields == null || (t = (TextField)fields.get(name)) == null) {
	    System.out.println("ERROR: Input Field: requested input \""+
			       name+"\" does not exist");
	    break;
	  }
	  cmd += t.getText();
	  t.setText("");
	  break;
	}
	default : cmd += tmp.substring(idx, ++idx);
	}
	oldidx = ++idx;
      }

      if(oldidx <= tmp.length()) cmd += tmp.substring(oldidx, tmp.length());
      
      if(function != null) {
	if(function.equals("exit")) { 
	  try {
	    System.exit(0);
	  } catch(Exception e) { e.printStackTrace(); }
	}
	if(function.equals("connect")) {
	  String address = null;
	  int port = -1;
	  try {
	    if((idx = cmd.indexOf(",")) >= 0) {
	      try {
		port = Integer.parseInt(cmd.substring(idx+1, cmd.length()));
	      } catch(Exception e) {
		port = -1;
	      }
	      cmd = cmd.substring(0, idx);
	    }
	    if(cmd.length() > 0) address = cmd;
	    if(address != null) 
	      if(port != -1) parent.connect(address, port);
	      else parent.connect(address);
	    else parent.connect();
	  } catch(Exception e) {
	    System.err.println("ButtonBar: connect(): failed");
	    e.printStackTrace();
	  }
	} else
	  if(function.equals("disconnect") && parent.disconnect())
	    parent.send("\r\nClosed connection.\r\n");
	  else
	    if(function.equals("detach")) {
	      if(parent.getParent() instanceof Frame) {
		Frame top = (Frame)parent.getParent();
		if(toplevel != null) {
		  System.out.println("ButtonBar: reattaching applet...");
		  toplevel.setLayout(new BorderLayout());
		  toplevel.add("Center", parent);
		  toplevel.validate();
		  toplevel.layout();
		  toplevel = null;
		} else {
		  System.out.println("ButtonBar: destroying window...");
		  parent.disconnect();
		}
		top.dispose();
	      } else {
		System.out.println("ButtonBar: detaching applet...");
		toplevel = parent.getParent();
		frame top = new frame("The Java Telnet Applet");
		Dimension s = parent.size();
		top.reshape(0, 0, s.width, s.height);
		top.setLayout(new BorderLayout());
		top.add("Center", parent);
		top.pack();
		top.show();
	      }
	    }
	    else
	      System.out.println("ERROR: function not implemented: \""+
				 function+"\"");
	return true;
      }
      // cmd += tmp.substring(oldidx, tmp.length());
      if(cmd.length() > 0) parent.send(cmd);
      return true;
    }
    return false;
  }
}

