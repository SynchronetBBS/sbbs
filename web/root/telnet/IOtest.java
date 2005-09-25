/* IOtest.java -- An example how to use the TelnetIO class
 * --
 * Author: Matthias L. Jugel
 *
 * Usage: compile with javac IOtest.java
 *        run program with java IOtest
 *
 * This is not an applet, but the idea might be used in one. 
 */

import java.util.Vector;
import java.io.*;
import socket.*;

/**
 * IOtest -- a test class for telnet i/o
 * --
 * @version	$Id$
 * @author	Matthias L. Jugel
 */
class IOtest {

  // create a new telnet io instance
  static TelnetIO tio = new TelnetIO();

  // skip any received data until the prompt appears
  private static void wait(String prompt)
  {
    String tmp = "";
    do {
      try { tmp = new String(tio.receive(), 0); }
      catch(IOException e) { e.printStackTrace(); }
      System.out.println(tmp);
    } while(tmp.indexOf(prompt) == -1);
  }

  // send a string to the remote host, since TelnetIO needs a byte buffer
  // we have to convert the string first
  private static void send(String str)
  {
    byte[] buf = new byte[str.length()];
    str.getBytes(0, str.length(), buf, 0);
    try { tio.send(buf); } catch(IOException e) {}
  }

  // this function is called when running the class with java IOtest
  // looks very much like a modem login script ;-)
  public static void main(String args[])
  {
    try {
      tio.connect("localhost");
      wait("login:");
      send("<YOUR LOGIN NAME>\r");
      wait("Password:");
      send("<YOUR PASSWORD>\r");
      wait("<YOUR SHELL PROMPT>");
      send("touch /tmp/THIS_WAS_AN_APPLET\r");
      tio.disconnect();
    } catch(IOException e) { e.printStackTrace(); }
  }
}
