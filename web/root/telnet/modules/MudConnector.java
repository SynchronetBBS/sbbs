/**
 * MudConnector -- This module is especially designed for the 
 *                 MUD Connector (http://www.mudconnect.com/)
 *                 It loads a tabulator separated list via http and
 *                 displays it in a List-Box.
 * --
 * $Id$
 * $timestamp: Sun Apr 13 22:29:16 1997 by Matthias L. Jugel :$
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

import java.applet.*;
import java.awt.*;
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * A specially designed module for the 
 * <A HREF="http://www.mudconnect.com/">MUD Connector</A>.
 * It loads a list of MUDs via http and displays them in a list box to 
 * be selected. A selected MUD can then be entered.
 *
 * <DL>
 *  <DT><B>MudConnector parameterfile:</B>
 *   <DD><PRE>&lt;PARAM NAME=MUDlist VALUE=&quot;<B><I>url</I></B>&quot;&gt;</PRE>
 *   <DD>The url of the Mudlist. This url MUST be located on the same web
 *       server as the applet!
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 * @see modules.Module
 */
public class MudConnector extends Panel implements Module, Runnable
{
  // parent applet
  private telnet app;

  // server url for the mudlist
  private String url;
  private URL server;

  // vector containing the mud data (host#port)
  private Vector index = new Vector();
  // the actually displayed list of muds
  private java.awt.List display = new java.awt.List();

  private CardLayout layouter;
  private Panel progress, address;
  private Label indicator;
  private TextField info;

  // buttons
  private Button connectButton, disconnectButton, refreshButton, showButton;

  /**
   * setLoader() is called upon start of the parent applet. This method
   * initializes the GUI of the module, e.g the list and buttons.
   * @param loader the parent applet
   */
  public void setLoader(Object loader) {
    // do nothing if we already know our loader
    if(app != null) return;
    app = (telnet)loader;

    // cardlayout to display either the progress while loading the mudlist
    // or the mud listbox plus its buttons.
    setLayout(layouter = new CardLayout());

    add("PROGRESS", progress = new Panel());
    progress.add(indicator = new Label("Loading mudlist, please wait..."));
    
    // the interactive module GUI is arranged in a grid
    GridBagLayout grid = new GridBagLayout();

    address = new Panel();
    address.setLayout(grid);
    
    // the constraints for each of the components
    GridBagConstraints constraints = new GridBagConstraints();

    // the listbox is arranged on the left side getting most of the space
    constraints.gridheight = 2;
    constraints.weightx = 2.0;
    constraints.fill = GridBagConstraints.BOTH;
    grid.setConstraints(display, constraints);
    address.add(display);

    // generic panel that places its components centered.
    Panel panel = new Panel();

    // the button panel, buttons are placed east of the list and on the top
    panel.add(showButton = new Button("Info!"));
    panel.add(connectButton = new Button("connect"));
    panel.add(disconnectButton = new Button("disconnect"));
    panel.add(refreshButton = new Button("refresh list"));
    constraints.weightx = 0.0;
    constraints.weightx = 0.0;
    constraints.gridheight = 1;
    constraints.gridwidth = GridBagConstraints.REMAINDER;
    constraints.fill = GridBagConstraints.NONE;
    grid.setConstraints(panel, constraints);
    address.add(panel);

    // the mud information text is below the buttons
    (panel = new Panel()).add(info = new TextField(30));
    info.setEditable(false);
    address.add(panel);
    grid.setConstraints(panel, constraints);
    add("ADDRESS", address);

    layouter.show(this, "PROGRESS");
  }
  
  /**
   * when newly added try to load the mudlist using the parameter "mudlist"
   */
  public void addNotify()
  {
    super.addNotify();
    if(url == null) {
      if(app.getParameter("mudlist") != null) {
        url = app.getParameter("mudlist");
        loadData();
      }
      else {
        indicator.setText("The \"mudlist\" is not set, "+
                          "cannot load data!");
        System.out.println("MudConnector: cannot load data, missing parameter");
      }
    }
  }

  /**
   * initiate the loading process, display progress meter
   */
  private void loadData() 
  {
    layouter.show(this, "PROGRESS");
    
    Thread t = new Thread(this);
    t.setPriority(Thread.MIN_PRIORITY);
    t.start();
  }

  /**
   * The body of the thread opens a URLConnection with the address given as
   * parameter "mudlist" and downloads it. It expects a tabulator separated
   * list <mudname> <mudhost> <mudport> and the amount of muds in the file
   * at the beginning of the file.
   */
  public void run() 
  {
    try {
      System.out.print("MudConnector: loading data...");
      if(display.countItems() > 0) display.clear();
      index = new Vector();
      
      // open the url and the data stream
      server = new URL(url);
      StreamTokenizer ts = new StreamTokenizer(server.openStream());
      // reset the syntax and make all but the chars from 0-31 to
      // plain text
      ts.resetSyntax();
      ts.whitespaceChars(0, 31);
      ts.wordChars(32, 255);

      // paint a rectangle for the progress indicator with the size of the
      // text above
      Graphics pg = progress.getGraphics();

      // initialize the current number of muds loaded and load the maximum
      // number of muds in the file (first line in the file)
      int p = 1, max = 1;
      int token = ts.nextToken();
      if(token != ts.TT_EOF) try {
        ts.sval = ts.sval.substring(1);
        max = Integer.parseInt(ts.sval); 
        System.out.print("["+max+" muds expected] ");
        token = ts.nextToken();
      } catch(NumberFormatException e) {
        System.out.print("'"+ts.sval+"'");
        System.out.print("[# of muds incorrect, expecting 1000] ");
        max = 1000;
      }
      
      // read line by line and display the progress bar
      while(token != ts.TT_EOF) {
        pg.setColor(getBackground());
        pg.draw3DRect(indicator.location().x - 1, 
                      indicator.location().y + indicator.size().height + 4, 
                      indicator.size().width + 1, 21, false);
        pg.fill3DRect(indicator.location().x, 
                    indicator.location().y + indicator.size().height + 5, 
                    p++ * indicator.size().width / max, 20, true);
        
        String name = ts.sval; token = ts.nextToken();
        if(token != ts.TT_EOL && token != ts.TT_EOF) {
          String host = ts.sval; token = ts.nextToken();
          int port;
          try {
            port = Integer.parseInt(ts.sval);
          } catch(NumberFormatException e) {
            port = 23;
          }
          display.addItem(name);
          index.addElement(host+"#"+port);
        } else 
          System.out.println("unexpected ("+name+") "+
                             (token == ts.TT_EOF ? "EOF" : "EOL"));
        while((token = ts.nextToken()) != ts.TT_WORD && 
              token != ts.TT_EOF);
      }
    } catch(Exception e) {
      indicator.setText("The \"mudlist\" parameter is incorrect, "+
                        "cannot load data!");
      e.printStackTrace();
    }
    System.out.println("("+index.size()+" muds)...done");
    layouter.show(this, "ADDRESS");
  }
  
  /**
   * handle list selection, connect, disconnect and refresh button
   * @param evt the event to process
   */
  public boolean handleEvent(Event evt) {
    if(evt.target == connectButton && evt.id == Event.ACTION_EVENT) {
      if(display.getSelectedIndex() < 0 ||
         display.getSelectedIndex() > index.size()) {
        info.setText("You did not select a MUD!");
        return false;
      }
      String address = (String)index.elementAt(display.getSelectedIndex());
      int port = Integer.parseInt(address.substring(address.indexOf('#')+1));
      app.connect(address.substring(0, address.indexOf('#')), port);
      return true;
    }
    if(evt.target == disconnectButton && evt.id == Event.ACTION_EVENT)
      app.disconnect();
    if(evt.target == refreshButton && evt.id == Event.ACTION_EVENT)
      loadData();
    if(evt.target == showButton && evt.id == Event.ACTION_EVENT) {
      if(display.getSelectedIndex() < 0 ||
         display.getSelectedIndex() > index.size()) {
        info.setText("You did not select a MUD!");
        return false;
      }
      URL page = null;
      try {
        page = new URL("http://www.mudconnect.com/mud-bin/adv_search.cgi"+
                       "?Mode=MUD&mud="+
                       display.getSelectedItem().replace(' ', '+'));
      } catch(MalformedURLException e) {
        info.setText("There was an URL error!");
        e.printStackTrace();
        return false;
      }
      app.getAppletContext().showDocument(page, "_TOP");
      return true;
    }
    
    if(evt.target == display && evt.id == Event.LIST_SELECT) {
      String tmp = (String)index.elementAt(display.getSelectedIndex());
      info.setText(tmp.substring(0, tmp.indexOf('#'))+" "+
                   tmp.substring(tmp.indexOf('#')+1));
    }
    return false;
  }

  /**
   * dummy methods
   */
  public void connect(String host, int port) {}
  public void disconnect() {}
  public String receive(String str)
  {
    return null;
  }
  
}
