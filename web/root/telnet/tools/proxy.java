/**
 * proxy -- a proxy server for telnet
 * --
 * $Id$
 * $timestamp: Tue May 27 15:08:19 1997 by Matthias L. Jugel :$
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

import java.net.*;
import java.io.*;
import java.util.*;

/**
 * proxy class -- implements a proxy server to redirect network access
 * @author Matthias L. Jugel, Marcus Meiﬂner
 * @version $Id$
 */

public class proxy implements Runnable
{
  String remoteHost;
  int localPort, remotePort;

  Thread listener, connection;

  ServerSocket server;

  /**
   * Create a server socket and start listening on the local port.
   * @param lport local port
   * @param raddr address of the destination
   * @param rport port on the destination host
   */
  public proxy(int lport, String raddr, int rport)
  {
    localPort = lport;
    remoteHost = raddr; remotePort = rport;
    
    log("destination host is "+remoteHost+" at port "+remotePort);
    try {
      server = new ServerSocket(localPort);
    } catch(Exception e) {
      System.err.println("proxy: error: cannot create server socket");
    }
    log("listening on port "+localPort);

    listener = new Thread(this);
    listener.setPriority(Thread.MIN_PRIORITY);
    listener.start();
  }
  
  /**
   * This method is called when the application is run on the commandline.
   * It takes two or three arguments:
   * usage: java proxy local-port destination-host destination-port
   * @param args The command line arguments
   */
  public static void main(String args[]) 
  {
    String remoteHost = "";
    int localPort = 0, remotePort = 0;
    
    if(args.length < 2)
    {
      System.err.println("proxy: usage: proxy <port> "+
                         "<destination host> [<destination port>]");
      System.exit(1);
    }
    try {
      localPort = Integer.parseInt(args[0]);
    } catch(Exception e) {
      System.err.println("proxy: parameter <port>: number expected");
      System.exit(1);
    }
    remoteHost = args[1];
    if(args.length > 2)
    {
      try {
        remotePort = Integer.parseInt(args[2]);
      } catch(Exception e) {
        System.err.println("proxy: parameter <destination port>: "+
                           "number expected");
        System.exit(1);
      }
    }

    proxy me = new proxy(localPort, 
                         remoteHost, (remotePort == 0 ? 23 : remotePort));
  }

  /**
   * Cycle around until an error occurs and wait for incoming connections.
   * An incoming connection will create two redirectors. One for
   * local-host - destination-host and one for destination-host - local-host.
   */
  public void run()
  {
    while(true)
    {
      Socket localSocket = null;
      try {
        localSocket = server.accept();
      } catch(Exception e) {
        System.err.println("proxy: error: accept connection failed");
        continue;
      }
      log("accepted connection from "+
          localSocket.getInetAddress().getHostName());
      try {
        Socket destinationSocket = new Socket(remoteHost, remotePort);
        log("connecting "+localSocket.getInetAddress().getHostName()+" <-> "+
            destinationSocket.getInetAddress().getHostName());
        redirector r1 = new redirector(localSocket, destinationSocket);
        redirector r2 = new redirector(destinationSocket, localSocket);
        r1.couple(r2);
        r2.couple(r1);
      } catch(Exception e) {
        System.err.println("proxy: error: cannot create sockets");
        try {
          DataOutputStream os = new DataOutputStream(localSocket.getOutputStream());
          os.writeChars("Remote host refused connection.\n");
          localSocket.close();
        } catch(IOException ioe) { ioe.printStackTrace(); }
        continue;
      }
    }
  }

  private void log(String msg)
  {
    System.out.println("proxy: ["+new Date()+"] "+msg);
  }
}

/**
 * A class useful for the proxy server.
 * This class takes over control of newly created connections and redirects
 * the data streams.
 */
class redirector implements Runnable
{
  private redirector companion = null;
  private Socket localSocket, remoteSocket;
  private InputStream from;
  private OutputStream to;
  private byte[] buffer = new byte[4096];

  /**
   * redirector gets the streams from sockets
   */
  public redirector(Socket local, Socket remote)
  {
    try {
      localSocket = local; 
      remoteSocket = remote;
      from = localSocket.getInputStream();
      to = remoteSocket.getOutputStream();
    } catch(Exception e) {
      System.err.println("redirector: cannot get streams");
    }
  }

  /**
   * couple this redirector instance with another one (usually the other
   * direction of the connection)
   */
  public void couple(redirector c) { 
    companion = c; 
    Thread listen = new Thread(this);
    listen.start();
  }

  /**
   * decouple us from our companion. This will let this redirector die after
   * exiting from run()
   */
  public void decouple() { companion = null; }

  /**
   * read data from the input and write it to the destination stream until
   * an error occurs or our companion is decoupled from us
   */
  public void run()
  {
    int count;
    try {
      while(companion != null) {
        if((count = from.read(buffer)) < 0)
          break;
        to.write(buffer, 0, count);
      }
    } catch(Exception e) {
      System.err.println("redirector: connection lost");
    }
    try {
      from.close();
      to.close();
      localSocket.close();
      remoteSocket.close();
      // is our companion dead? no, then decouple, because we die
      if(companion != null) companion.decouple();
    } catch(Exception io) {
      System.err.println("redirector: error closing streams and sockets");
      io.printStackTrace();
    }
  }
}

      
