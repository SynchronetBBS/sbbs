/**
 * telnet -- implements a simple telnet
 * --
 * $Id$
 * $timestamp: Mon Aug  4 13:11:14 1997 by Matthias L. Jugel :$
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

import java.applet.Applet;
import java.awt.Frame;
import java.awt.Component;
import java.awt.Container;
import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Panel;
import java.awt.Event;
import java.util.Vector;
import java.util.Hashtable;
import java.util.Enumeration;
import java.io.IOException;

import socket.TelnetIO;
import socket.StatusPeer;

import display.Terminal;
import display.TerminalHost;

import modules.Module;

/**
 * A telnet implementation that supports different terminal emulations.
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meißner
 */
public class telnet extends Applet implements Runnable, TerminalHost, StatusPeer
{
  /**
   * The telnet io methods.
   * @see socket.TelnetIO
   */
  protected TelnetIO tio;

  /**
   * The terminal emulation (dynamically loaded).
   * @see emulation
   * @see display.Terminal
   * @see display.TerminalHost
   */
  protected Terminal term;

  /**
   * The host address to connect to. This is retrieved from the PARAM tag
   * "address".
   */
  protected String address;

  /**
   * The port number (default ist 23). This can be specified as the PARAM tag
   * "port".
   */
  protected int port = 23;

  /**
   * The proxy ip address. If this variable is set telnet will try to connect
   * to this address and then send a string to tell the relay where the
   * target host is.
   * @see address
   */
  protected String proxy = null;
  /**
   * The proxy port number. This is the port where the relay is expected to
   * listen for incoming connections.
   * @see proxy
   * @see port
   */
  protected int proxyport;
  
  /**
   * Emulation type (default is vt320). This can be specified as the PARAM
   * tag "emulation".
   * @see term
   * @see display.Terminal
   * @see display.TerminalHost
   */
  protected String emulation = "vt320";

  /**
   * Dynamically loaded modules are stored here.
   */
  protected Vector modules = null;

  // some state variables;
  private boolean localecho = true;
  private boolean connected = false;

  private Thread t;
  private Container parent;

  /**
   * This Hashtable contains information retrievable by getParameter() in case
   * the program is run as an application and the AppletStub is missing.
   */
  public Hashtable params;

  /** 
   * Retrieve the current version of the applet.
   * @return String a string with the version information.
   */
  public String getAppletInfo()
  {
    String info = "The Java(tm) Telnet Applet\n$Id$\n";
    info += "Terminal emulation: "+term.getTerminalType()+
      " ["+term.toString()+"]\n";
    info += "Terminal IO version: "+tio.toString()+"\n";
    if(modules != null && modules.size() > 0) {
      info += "Resident modules loaded: ("+modules.size()+")";
      for(int i = 0; i < modules.size(); i++)
        info += "   + "+(modules.elementAt(i)).toString()+"\n";
    }
    
    return info;
  }
    
  /**
   * Retrieve parameter tag information. This includes the tag information from
   * terminal and loaded modules.
   * @return String an array of array of string with tag information
   * @see java.applet.Applet#getParameterInfo
   */
  public String[][] getParameterInfo()
  {
    String pinfo[][];
    String info[][] = {
      {"address",  "String",   "IP address"},
      {"port",     "Integer",  "Port number"},
      {"proxy",    "String",   "IP address of relay"},
      {"proxyport","Integer",  "Port number of relay"},
      {"emulation","String",   "Emulation to be used (standard is vt320)"},
      {"localecho","String",   "Localecho Mode (on/off/auto)"},
    };
    String tinfo[][] = (term != null ? term.getParameterInfo() : null);
    if(tinfo != null) pinfo = new String[tinfo.length + 3][3];
    else pinfo = new String[3][3];
    System.arraycopy(info, 0, pinfo, 0, 3);
    System.arraycopy(tinfo, 0, pinfo, 3, tinfo.length);
    return pinfo;
  }

  /**
   * We override the Applet method getParameter() to be able to handle 
   * parameters even as application.
   * @param name The name of the queried parameter.
   * @return the value of the parameter
   * @see java.applet.Applet#getParameter
   */ 
  public String getParameter(String name)
  {
    if(params == null) return super.getParameter(name);
    return (String)params.get(name);
  }

  /**
   * The main function is called on startup of the application.
   */
  public static void main(String args[]) 
  {
    // an application has to create a new instance of itself.
    telnet applet = new telnet();

    // create params from command line arguments
    applet.params = new Hashtable();
    switch(args.length) 
    {
    case 2: applet.params.put("port", args[1]);
    case 1: applet.params.put("address", args[0]); 
      break;
    default: 
      System.out.println("Usage: java telnet host [port]");
      System.exit(0);
    } 
    applet.params.put("VTscrollbar", "true");
    applet.params.put("module#1", "ButtonBar");
    applet.params.put("1#Button", "Exit|\\$exit()");
    applet.params.put("2#Button", "Connect|\\$connect(\\@address@,\\@port@)");
    applet.params.put("3#Input", "address#30|"
          +(args.length > 0 ? args[0] : "localhost"));
    applet.params.put("4#Input", "port#4|23");
    applet.params.put("5#Button", "Disconnect|\\$disconnect()");

    // we put the applet in its own frame
    Frame frame = new Frame("The Java Telnet Application ["+args[0]+"]");
    frame.setLayout(new BorderLayout());
    frame.add("Center", applet);
    frame.resize(380, 590);

    applet.init();

    frame.pack();
    frame.show();

    applet.start();
  }
  
  /**
   * Initialize applet. This method reads the PARAM tags "address",
   * "port" and "emulation". The emulation class is loaded dynamically.
   * It also loads modules given as parameter "module#<nr>".
   */
  public void init()
  {
    String tmp; 

    // save the current parent for future use
    parent = getParent();
    
    // get the address we want to connect to
    if((address = getParameter("address")) == null)
      address = getDocumentBase().getHost();

    if((tmp = getParameter("port")) == null) 
      port = 23;
    else
      port = Integer.parseInt(tmp);

    if((proxy = getParameter("proxy")) != null)
      if((tmp = getParameter("proxyport")) == null)
        proxyport = 31415;
      else
        proxyport = Integer.parseInt(tmp);
    
    if((emulation = getParameter("emulation")) == null)
      emulation = "vt320";

    // load the terminal emulation
    try {
      term = (Terminal)Class.forName("display."+emulation).newInstance();
      System.out.println("telnet: load terminal emulation: "+emulation);
    } catch(Exception e) {
      System.err.println("telnet: cannot load terminal emulation "+emulation);
      e.printStackTrace();
    }
    setLayout(new BorderLayout());
    
    // load modules, position is determined by the @<position> modifier
    modules = new Vector();
    int nr = 1;
    while((tmp = getParameter("module#"+nr++)) != null) try {
      Panel north = null, south = null, west = null, east = null;
      String position = "North", initFile = null;

      // try to get the initialization file name
      if(tmp.indexOf(',') != -1) {
        initFile = tmp.substring(tmp.indexOf(','+1));
        tmp = tmp.substring(0, tmp.indexOf(','));
        initFile = tmp.substring(tmp.indexOf(','+1));
      }
           
      // find the desired location
      if(tmp.indexOf('@') != -1) {
        position = tmp.substring(tmp.indexOf('@')+1);
        tmp = tmp.substring(0, tmp.indexOf('@'));
      } 
      Object obj = (Object)Class.forName("modules."+tmp).newInstance();

      // probe for module (implementing modules.Module)
      try {
        ((Module)obj).setLoader(this);
        modules.addElement((Module)obj);
        System.out.println("telnet: module "+tmp+" detected");
      } catch(ClassCastException e) {
        System.out.println("telnet: warning: "+tmp+" may not be a "+
                           "valid module");
      }

      // probe for visible component (java.awt.Component and descendants)
      try {
	Component component = (Component)obj;
	if(position.equals("North")) {
          if(north == null) { north = new Panel(); add("North", north); }
          north.add(component);
        } else if(position.equals("South")) {
          if(south == null) { south = new Panel(); add("South", south); }
          south.add(component);
        } else if(position.equals("East")) {
          if(east == null) { east = new Panel(); add("East", east); }
          east.add(component);
        } else if(position.equals("West")) {
          if(west == null) { west = new Panel(); add("West", west); } 
          west.add(component);
        }
        System.err.println("telnet: module "+tmp+" is a visible component");
      } catch(ClassCastException e) {}

    } catch(Exception e) {
      System.err.println("telnet: cannot load module "+tmp);
      e.printStackTrace();
    }
    if(modules.isEmpty()) modules = null;
    add("Center", term);
  }

  /**
   * Upon start of the applet try to create a new connection.
   */
  public void start()
  {
    if(!connect(address, port) && params == null)
      showStatus("telnet: connection to "+address+" "+port+" failed");
  }

  /**
   * Disconnect when the applet is stopped.
   */
  public final void stop()
  {
    disconnect();
  }

  /**
   * Try to read data from the sockets and put it on the terminal.
   * This is done until the thread dies or an error occurs.
   */
  public void run()
  {
    while(t != null)
      try {
        String tmp = new String(tio.receive(), 0);

        // cycle through the list of modules
        if(modules != null) {
          Enumeration modlist = modules.elements();
          while(modlist.hasMoreElements()) {
            Module m = (Module)modlist.nextElement();
            String modified = m.receive(tmp);
            // call the receive() method and if it returns null
            // remove the module from the list
            if(modified == null) modules.removeElement(m);
            else tmp = modified;
          }
        }
        // put the modified string to the terminal
        term.putString(tmp);
    } catch(IOException e) {
      disconnect();
    }
  }

  /**
   * Connect to the specified host and port but don't break existing 
   * connections. Connects to the host and port specified in the tags. 
   * @return false if connection was unsuccessful
   */
  public boolean connect()
  {
    return connect(address, port);
  }

  /**
   * Connect to the specified host and port but don't break existing 
   * connections. Uses the port specified in the tags or 23.
   * @param host destination host address
   */
  public boolean connect(String host)
  {
    return connect(host, port);
  }
  
  /**
   * Connect to the specified host and port but don't break existing 
   * connections.
   * @param host destination host address
   * @param prt destination hosts port
   */
  public boolean connect(String host, int prt)
  {
    address = host; port = prt;

    if(address == null || address.length() == 0) return false;
    
    // There should be no thread when we try to connect
    if(t != null && connected) {
      System.err.println("telnet: connect: existing connection preserved");
      return false;
    } else t = null;
    
    try {
      // In any case try to disconnect if tio is still active
      // if there was no tio create a new one.
      if(tio != null) try { tio.disconnect(); } catch(IOException e) {}
      else (tio = new TelnetIO()).setPeer(this);

      term.putString("Trying "+address+(port==23?"":" "+port)+" ...\n\r");
      try {
        // connect to to our destination at the given port
        if(proxy != null) {
          tio.connect(proxy, proxyport);
          String str = "relay "+address+" "+port+"\n";
          byte[] bytes = new byte[str.length()];
          str.getBytes(0, str.length(), bytes, 0);
          tio.send(bytes);
        } else 
          tio.connect(address, port);
        term.putString("Connected to "+address+".\n\r");
        // initial conditions are connected and localecho
        connected = true;
        localecho = true;
	if ( (getParameter("localecho")!=null) && 
	     getParameter("localecho").equals("no")
        )
	  localecho = false;

        // cycle through the list of modules and notify connection
        if(modules != null) {
          Enumeration modlist = modules.elements();
          while(modlist.hasMoreElements())
            // call the connect() method
            ((Module)modlist.nextElement()).connect(address, port);
        }
      } catch(IOException e) {
        term.putString("Failed to connect.\n\r");
        // to be sure, we remove the TelnetIO instance
        tio = null;
        System.err.println("telnet: failed to connect to "+address+" "+port);
        e.printStackTrace();
        return false;
      }
      // if our connection was successful, create a new thread and start it
      t = new Thread(this);
      t.setPriority(Thread.MIN_PRIORITY);
      t.start();
    } catch(Exception e) {
      // hmm, what happened?
      System.err.println("telnet: an error occured:");
      e.printStackTrace();
      return false;
    }
    return true;
  }

  /**
   * Disconnect from the remote host.
   * @return false if there was a problem disconnecting.
   */
  public boolean disconnect()
  {
    if(tio == null) {
      System.err.println("telnet: no connection");
      return false;
    }
    try {
      connected = false; t = null;
      // cycle through the list of modules and notify connection
      if(modules != null) {
        Enumeration modlist = modules.elements();
        while(modlist.hasMoreElements())
          // call the disconnect() method
          ((Module)modlist.nextElement()).disconnect();
      }
      term.putString("\n\rConnection closed.\n\r");
      tio.disconnect();
    } catch(Exception e) {
      System.err.println("telnet: disconnection problem");
      e.printStackTrace();
      tio = null; t = null;
      return false;
    }
    return true;
  }
  
  /**
   * Send a String to the remote host. Implements display.TerminalHost
   * @param s String to be sent
   * @return true if we are connected
   * @see display.TerminalHost
   */
  public boolean send(String str)
  {
    if(connected) try {
      byte[] bytes = new byte[str.length()];
      str.getBytes(0, str.length(), bytes, 0);
      tio.send(bytes);
      if(localecho) {
        if ((str.length()==2) && (str.charAt(0)=='\r') && (str.charAt(1)==0))
          term.putString("\r\n");
        else
          term.putString(str);
      }
    } catch(Exception e) {
      System.err.println("telnet.send(): disconnected");
      disconnect();
      return false;
    }
    else return false;
    return true;
  }

  /**
   * Send a String to the remote Host.
   * @param str String to be sent
   * @return true if we are connected
   * @see modules.BSXModule
   */
  public boolean writeToSocket(String str)
    {
      if (connected) try {
	byte[] bytes = new byte[str.length()];
	str.getBytes(0, str.length(), bytes, 0);
	tio.send(bytes);
      } catch(Exception e) {
	System.err.println("telnet.send(): disconnected");
	disconnect();
	return false;
      }
      else return false;
      return true;
    }
  /**
   * Send a String to the users terminal
   * @param str String to be displayed
   * @return void
   * @see modules.BSXModule
   */
  public void writeToUser(String str)
  {
    if (term!=null)
      term.putString(str);
  }

  /**
   * This method is called when telnet needs to be notified of status changes.
   * @param status Vector of status information.
   * @return an object of the information requested.
   * @see socket.StatusPeer
   */
  public Object notifyStatus(Vector status)
  {
    String what = (String)status.elementAt(0);

    if(what.equals("NAWS"))
      return term.getSize();
    if(what.equals("TTYPE"))
      if(term.getTerminalType() == null)
        return emulation;
      else return term.getTerminalType();

    if(what.equals("LOCALECHO") || what.equals("NOLOCALECHO")) {
      boolean nlocalecho = localecho ;
    if(what.equals("LOCALECHO"))
	nlocalecho = true;
    if(what.equals("NOLOCALECHO")) 
	nlocalecho = false;
      if ( (getParameter("localecho")==null) ||
	   getParameter("localecho").equals("auto")
      )
	localecho = nlocalecho;
      /* else ignore ... */
    }
    return null;
  }
}
