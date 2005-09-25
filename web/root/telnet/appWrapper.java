/**
 * appWrapper -- applet/application wrapper
 * --
 * $Id$
 * $timestamp: Thu Jul 24 13:08:23 1997 by Matthias L. Jugel :$
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
import java.applet.AppletStub;

import java.awt.Frame;
import java.awt.Event;
import java.awt.Panel;
import java.awt.Button;
import java.awt.BorderLayout;
import java.awt.Graphics;
import java.awt.Color;
import java.awt.FontMetrics;

/**
 * The appWrapper is thought to make the applet itself independent from
 * the original context. This is necessary to be able to detach the applet
 * from the web browsers window without disconnecting it from events.
 * Note: This applet should work with any applet without changes.
 *
 * <DL>
 * <DT><B><PRE>&lt;PARAM NAME=&quot;applet&quot; VALUE=&quot;<I>applet</I>&quot;&gt;</PRE></B>
 * <DD>Defines the applet to be loaded by the appWrapper. State the applet 
 *     class name without &quot;.class&quot;!<P>
 * <DT><B><PRE>&lt;PARAM NAME=&quot;startButton&quot; VALUE=&quot;<I>text</I>&quot;&gt;</PRE></B>
 * <DD>If this parameter is set the applet is not loaded until the user presses
 *     the button. This decreases first time download delay. The <I>text</I>
 *     given as value to the parameter is shown on the button. While loading
 *     the applet the message "Loading ..." is shown on the button.<P>
 * <DT><B><PRE>&lt;PARAM NAME=&quot;stopButton&quot; VALUE=&quot;<I>text</I>&quot;&gt;</PRE></B>
 * <DD>This parameter defines the button text when the applet is loaded. When 
 *     pressing the button while the applet is running this causes the applet
 *     window to be destroyed and the applet is stopped.<P>
 * <DT><B><PRE>&lt;PARAM NAME=&quot;frameTitle&quot; VALUE=&quot;<I>text</I>&quot;&gt;</PRE></B>
 * <DD>The <I>frameTitle</I> is the text that is shown in the title bar of the
 *     applet window.<P>
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel
 */
public class appWrapper extends Applet implements AppletStub, Runnable
{
  Thread loader = null;
  
  String appletName = null;
  Applet applet = null;

  Button startButton = null;
  String startLabel, stopLabel, frameTitle;

  frame f;
  
  /**
   * Applet initialization. We load the class giving in parameter "applet"
   * and set the stub corresponding to ours. Thus we are able to give
   * it access to the parameters and any applet specific context.
   */
  public void init() {

    // get the applet parameter
    if((appletName = getParameter("applet")) == null) {
      showStatus("appWrapper: missing applet parameter, nothing loaded");
      System.err.println("appWrapper: missing applet parameter");
      return;
    }

    setLayout(new BorderLayout());

    // get the button and title parameters
    if((startLabel = getParameter("startButton")) == null)
      run();
    else {
      startButton = new Button(getParameter("startButton"));
      add("Center", startButton);
      if((stopLabel = getParameter("stopButton")) == null)
        stopLabel = "STOP!";
      if((frameTitle = getParameter("frameTitle")) == null)
        frameTitle = "The Java Telnet Applet";
    }
    
  }
  
  /**
   * Load the applet finally. When using a button this creates a new frame
   * to put the applet in.
   */
  public void run() {
    if(applet == null) try {
      applet = (Applet)Class.forName(getParameter("applet")).newInstance();
      applet.setStub(this);
    } catch(Exception e) {
      System.err.println("appWrapper: could not load "+appletName);
      e.printStackTrace();
      return;
    } else {
      System.err.println("appWrapper: applet already loaded");
      return;
    }

    if(startButton == null) {
      add("Center", applet);
      applet.init();
    } else {
      f = new frame(frameTitle);
      f.setLayout(new BorderLayout());
      f.add("Center", applet);
      applet.init();
      f.resize(applet.minimumSize());
      f.pack();
      f.show();
    }
    applet.start();

    if(startButton != null)
      startButton.setLabel(stopLabel);
    
    // stop loader thread
    while(loader != null) {
      if(f == null || !f.isVisible()) {
        startButton.setLabel(startLabel);
        loader.stop();
        loader = null;
      }
      try { loader.sleep(5000); }
      catch(InterruptedException e) {
        e.printStackTrace();
      }
    }
  }

  /**
   * This method is called when the applet want's to be resized.
   * @param width the width of the applet
   * @param height the height of the applet
   */
  public void appletResize(int width, int height) {
    System.err.println("appWrapper: appletResize()");
    if(applet != null) applet.resize(width, height);
  }

  /**
   * Give information about the applet.
   */
  public String getAppletInfo()
  {
    String info = "appWrapper: $Id$\n";
    if(applet != null)
      info += applet.getAppletInfo();
    return info;
  }
  
  /**
   * Give information about the appWrapper and the applet loaded.
   */
  public String[][] getParameterInfo()
  {
    String info[][];
    String wrapper[][] = {
      {"applet",   "String",   "appWrapper: Applet to load"},
    };
    if(applet != null) {
      String tmp[][] = applet.getParameterInfo();
      info = new String[tmp.length + 1][3];
      System.arraycopy(tmp, 0, info, 1, tmp.length);
    }
    else info = new String[1][3];
    System.arraycopy(wrapper, 0, info, 0, 1);
      
    return info;
  }

  /**
   * Write a message to the applet area.
   */
  public void paint(Graphics g) 
  {
    String message;
    if(applet != null) 
      message = "Click to reattach the Applet!";
    else message = "The was no applet load (maybe an error)!";

    
    int width = size().width / 2 - 
      (getFontMetrics(getFont())).stringWidth(message) / 2;
    int height = size().height / 2;
    
    g.setColor(Color.red);
    g.drawString(message, width, height);
  }
  
  /**
   * reshape the applet and ourself
   */
  public void reshape(int x, int y, int w, int h)
  {
    if(applet != null) applet.reshape(x, y, w, h);
    super.reshape(x, y, w, h);
  }

  /**
   * Handle button events. When pressed it either creates the new applet
   * window or destoys it.
   */
  public boolean handleEvent(Event evt) 
  {
    if(evt.target == startButton && evt.id == Event.ACTION_EVENT) {
      if(applet == null) {
        startButton.setLabel("Loading ...");
        (loader = new Thread(this)).start();
      } else {
        if(applet.getParent() instanceof Frame) {
          Frame frame = (Frame)applet.getParent();
          frame.hide();
          frame.dispose();
        }
        applet.stop();
        applet.destroy();
        applet = null;
        startButton.setLabel(startLabel);
      }
      return true;
    }
    if(evt.id == Event.MOUSE_UP && applet.getParent() instanceof Frame) {
      Frame frame = (Frame)applet.getParent();
      frame.hide();
      frame.dispose();
      add("Center", applet);
      validate();
      layout();
      return true;
    }
    return false;
  }
}
