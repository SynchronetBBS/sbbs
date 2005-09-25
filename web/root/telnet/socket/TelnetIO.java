/**
 * socket.TelnetIO - a telnet implementation
 * --
 * $Id$
 * $timestamp: Tue May 27 13:27:05 1997 by Matthias L. Jugel :$
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

package socket;

import java.net.Socket;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.awt.Dimension;
import java.util.Vector;

/**
 * Implements simple telnet io
 *
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Meiﬂner
 * @version 1.2 3/7/97 George Ruban added available() because it was needed.
 */
public class TelnetIO implements StatusPeer
{
  /**
   * Return the version of TelnetIO.
   */
  public String toString() { return "$Id$"; }
  
	/**
	 * Debug level. This results in additional diagnostic messages on the
	 * java console.
	 */
	private static int debug = 0;

	/**
	 * State variable for telnetnegotiation reader
	 */
	private byte neg_state = 0;

	/**
	 * constants for the negotiation state
	 */
	private final static byte STATE_DATA	= 0;
	private final static byte STATE_IAC	= 1;
	private final static byte STATE_IACSB	= 2;
	private final static byte STATE_IACWILL	= 3;
	private final static byte STATE_IACDO	= 4;
	private final static byte STATE_IACWONT	= 5;
	private final static byte STATE_IACDONT	= 6;
	private final static byte STATE_IACSBIAC	= 7;
	private final static byte STATE_IACSBDATA	= 8;
	private final static byte STATE_IACSBDATAIAC	= 9;

	/**
	 * What IAC SB <xx> we are handling right now
	 */
	private byte current_sb;

	/**
	 * IAC - init sequence for telnet negotiation.
	 */
	private final static byte IAC  = (byte)255;
	/**
	 * [IAC] End Of Record
	 */
	private final static byte EOR  = (byte)239;
	/**
	 * [IAC] WILL
	 */
	private final static byte WILL  = (byte)251;
	/**
	 * [IAC] WONT
	 */
	private final static byte WONT  = (byte)252;
	/**
	 * [IAC] DO
	 */
	private final static byte DO    = (byte)253;
	/**
	 * [IAC] DONT
	 */
	private final static byte DONT  = (byte)254;
	/**
	 * [IAC] Sub Begin 
	 */
	private final static byte SB  = (byte)250;
	/**
	 * [IAC] Sub End
	 */
	private final static byte SE  = (byte)240;
	/**
	 * Telnet option: echo text
	 */
	private final static byte TELOPT_ECHO  = (byte)1;  /* echo on/off */
	/**
	 * Telnet option: End Of Record
	 */
	private final static byte TELOPT_EOR   = (byte)25;  /* end of record */
	/**
	 * Telnet option: Negotiate About Window Size
	 */
	private final static byte TELOPT_NAWS  = (byte)31;  /* NA-WindowSize*/
	/**
	 * Telnet option: Terminal Type
	 */
	private final static byte TELOPT_TTYPE  = (byte)24;  /* terminal type */

	private final static byte[] IACWILL  = { IAC, WILL };
	private final static byte[] IACWONT  = { IAC, WONT };
	private final static byte[] IACDO    = { IAC, DO	};
	private final static byte[] IACDONT  = { IAC, DONT };
	private final static byte[] IACSB  = { IAC, SB };
	private final static byte[] IACSE  = { IAC, SE };

	/** 
	 * Telnet option qualifier 'IS' 
	 */
	private final static byte TELQUAL_IS = (byte)0;

	/** 
	 * Telnet option qualifier 'SEND' 
	 */
	private final static byte TELQUAL_SEND = (byte)1;

	/**
	 * What IAC DO(NT) request do we have received already ?
	 */
        private byte[] receivedDX;
  
	/**
	 * What IAC WILL/WONT request do we have received already ?
	 */
	private byte[] receivedWX;
	/**
	 * What IAC DO/DONT request do we have sent already ?
	 */
	private byte[] sentDX;
	/**
	 * What IAC WILL/WONT request do we have sent already ?
	 */
	private byte[] sentWX;

	private Socket socket;
	private BufferedInputStream is;
	private BufferedOutputStream os;

	private StatusPeer peer = this;		/* peer, notified on status */

	/**
	 * Connect to the remote host at the specified port.
	 * @param address the symbolic host address
	 * @param port the numeric port
	 * @see #disconnect
	 */
	public void connect(String address, int port) throws IOException {
		if(debug > 0) System.out.println("Telnet.connect("+address+","+port+")");
		socket = new Socket(address, port);
		is = new BufferedInputStream(socket.getInputStream());
		os = new BufferedOutputStream(socket.getOutputStream());
		neg_state = 0;
		receivedDX = new byte[256]; 
		sentDX = new byte[256];
		receivedWX = new byte[256]; 
		sentWX = new byte[256];
	}

	/**
	 * Disconnect from remote host.
	 * @see #connect
	 */
	public void disconnect() throws IOException {
	  if(debug > 0) System.out.println("TelnetIO.disconnect()");
	  if(socket !=null) socket.close();
	}
  
	/**
	 * Connect to the remote host at the default telnet port (23).
	 * @param address the symbolic host address
	 */
	public void connect(String address) throws IOException {
		connect(address, 23);
	}

	/**
	 * Set the object to be notified about current status.
	 * @param obj object to be notified.
	 */
	public void setPeer(StatusPeer obj) { peer = obj; }

	/** Returns bytes available to be read.  Since they haven't been
	 * negotiated over, this could be misleading.
	 * Most useful as a boolean value - "are any bytes available" -
	 * rather than as an exact count of "how many ara available."
	 *
	 * @exception IOException on problems with the socket connection
	 */
	public int available() throws IOException
	{
	  return is.available();
	}
	

	/**
	 * Read data from the remote host. Blocks until data is available. 
	 * Returns an array of bytes.
	 * @see #send
	 */
	public byte[] receive() throws IOException {
		int count = is.available();
		byte buf[] = new byte[count];
		count = is.read(buf);
		if(count < 0) throw new IOException("Connection closed.");
		if(debug > 1) System.out.println("TelnetIO.receive(): read bytes: "+count);
		buf = negotiate(buf, count);
		return buf;
	}

	/**
	 * Send data to the remote host.
	 * @param buf array of bytes to send
	 * @see #receive
	 */
	public void send(byte[] buf) throws IOException {
		if(debug > 1) System.out.println("TelnetIO.send("+buf+")");
		os.write(buf);
		os.flush();
	}

	public void send(byte b) throws IOException {
		if(debug > 1) System.out.println("TelnetIO.send("+b+")");
		os.write(b);
		os.flush();
	}

	/**
	 * Handle an incoming IAC SB <type> <bytes> IAC SE
	 * @param type type of SB
	 * @param sbata byte array as <bytes>
	 * @param sbcount nr of bytes. may be 0 too.
	 */
	private void handle_sb(byte type, byte[] sbdata, int sbcount) 
		throws IOException 
	{
		if(debug > 1) 
			System.out.println("TelnetIO.handle_sb("+type+")");
		switch (type) {
		case TELOPT_TTYPE:
			if (sbcount>0 && sbdata[0]==TELQUAL_SEND) {
				String ttype;
				send(IACSB);send(TELOPT_TTYPE);send(TELQUAL_IS);
				/* FIXME: need more logic here if we use 
				 * more than one terminal type
				 */
				Vector vec = new Vector(2);
				vec.addElement("TTYPE");
				ttype = (String)peer.notifyStatus(vec);
				if(ttype == null) ttype = "dumb";
				byte[] bttype = new byte[ttype.length()];

				ttype.getBytes(0,ttype.length(), bttype, 0);
				send(bttype);
				send(IACSE);
			}

		}
	}

	/**
	 * Notify about current telnet status. This method is called top-down.
	 * @param status contains status information
	 */
	public Object notifyStatus(Vector status) {
		if(debug > 0) 
		  System.out.println("TelnetIO.notifyStatus("+status+")");
		return null;
	}

	/* wo faengt buf an bei buf[0] oder bei buf[1] */
	private byte[] negotiate(byte buf[], int count) throws IOException {
		if(debug > 1) 
			System.out.println("TelnetIO.negotiate("+buf+","+count+")");
		byte nbuf[] = new byte[count];
		byte sbbuf[] = new byte[count];
		byte sendbuf[] = new byte[3];
		byte b,reply;
		int  sbcount = 0;
		int boffset = 0, noffset = 0;
		Vector	vec = new Vector(2);

		while(boffset < count) {
			b=buf[boffset++];
			/* of course, byte is a signed entity (-128 -> 127)
			 * but apparently the SGI Netscape 3.0 doesn't seem
			 * to care and provides happily values up to 255
			 */
			if (b>=128)
				b=(byte)((int)b-256);
			switch (neg_state) {
			case STATE_DATA:
				if (b==IAC) {
					neg_state = STATE_IAC;
				} else {
					nbuf[noffset++]=b;
				}
				break;
			case STATE_IAC:
				switch (b) {
				case IAC:
					if(debug > 2) 
						System.out.print("IAC ");
					neg_state = STATE_DATA;
					nbuf[noffset++]=IAC;
					break;
				case WILL:
					if(debug > 2)
						System.out.print("WILL ");
					neg_state = STATE_IACWILL;
					break;
				case WONT:
					if(debug > 2)
						System.out.print("WONT ");
					neg_state = STATE_IACWONT;
					break;
				case DONT:
					if(debug > 2)
						System.out.print("DONT ");
					neg_state = STATE_IACDONT;
					break;
				case DO:
					if(debug > 2)
						System.out.print("DO ");
					neg_state = STATE_IACDO;
					break;
				case EOR:
					if(debug > 2)
						System.out.print("EOR ");
					neg_state = STATE_DATA;
					break;
				case SB:
					if(debug > 2)
						System.out.print("SB ");
					neg_state = STATE_IACSB;
					sbcount = 0;
					break;
				default:
					if(debug > 2)
						System.out.print(
							"<UNKNOWN "+b+" > "
						);
					neg_state = STATE_DATA;
					break;
				}
				break;
			case STATE_IACWILL:
				switch(b) {
				case TELOPT_ECHO:
					if(debug > 2) 
						System.out.println("ECHO");
					reply = DO;
					vec = new Vector(2);
					vec.addElement("NOLOCALECHO");
					peer.notifyStatus(vec);
					break;
				case TELOPT_EOR:
					if(debug > 2) 
						System.out.println("EOR");
					reply = DO;
					break;
				default:
					if(debug > 2)
						System.out.println(
							"<UNKNOWN,"+b+">"
						);
					reply = DONT;
					break;
				}
				if(debug > 1)
				  System.out.println("<"+b+", WILL ="+WILL+">");
				if (	reply != sentDX[b+128] ||
					WILL != receivedWX[b+128]
				) {
					sendbuf[0]=IAC;
					sendbuf[1]=reply;
					sendbuf[2]=b;
					send(sendbuf);
					sentDX[b+128] = reply;
					receivedWX[b+128] = WILL;
				}
				neg_state = STATE_DATA;
				break;
			case STATE_IACWONT:
				switch(b) {
				case TELOPT_ECHO:
					if(debug > 2) 
						System.out.println("ECHO");

					vec = new Vector(2);
					vec.addElement("LOCALECHO");
					peer.notifyStatus(vec);
					reply = DONT;
					break;
				case TELOPT_EOR:
					if(debug > 2) 
						System.out.println("EOR");
					reply = DONT;
					break;
				default:
					if(debug > 2) 
						System.out.println(
							"<UNKNOWN,"+b+">"
						);
					reply = DONT;
					break;
				}
				if (	reply != sentDX[b+128] ||
					WONT != receivedWX[b+128]
				) {
					sendbuf[0]=IAC;
					sendbuf[1]=reply;
					sendbuf[2]=b;
					send(sendbuf);
					sentDX[b+128] = reply;
					receivedWX[b+128] = WILL;
				}
				neg_state = STATE_DATA;
				break;
			case STATE_IACDO:
				switch (b) {
				case TELOPT_ECHO:
					if(debug > 2) 
						System.out.println("ECHO");
					reply = WILL;
					vec = new Vector(2);
					vec.addElement("LOCALECHO");
					peer.notifyStatus(vec);
					break;
				case TELOPT_TTYPE:
					if(debug > 2) 
						System.out.println("TTYPE");
					reply = WILL;
					break;
				case TELOPT_NAWS:
					if(debug > 2) 
						System.out.println("NAWS");
					vec = new Vector(2);
					vec.addElement("NAWS");
					Dimension size = (Dimension)
						peer.notifyStatus(vec);
					receivedDX[b] = DO;
					if(size == null)
					{
						/* this shouldn't happen */
						send(IAC);
						send(WONT);
						send(TELOPT_NAWS);
						reply = WONT;
						sentWX[b] = WONT;
						break;
					}
					reply = WILL;
					sentWX[b] = WILL;
					sendbuf[0]=IAC;
					sendbuf[1]=WILL;
					sendbuf[2]=TELOPT_NAWS;
					send(sendbuf);
					send(IAC);send(SB);send(TELOPT_NAWS);
					send((byte) (size.width >> 8));
					send((byte) (size.width & 0xff));
					send((byte) (size.height >> 8));
					send((byte) (size.height & 0xff));
					send(IAC);send(SE);
					break;
				default:
					if(debug > 2) 
						System.out.println(
							"<UNKNOWN,"+b+">"
						);
					reply = WONT;
					break;
				}
				if (	reply != sentWX[128+b] ||
					DO != receivedDX[128+b]
				) {
					sendbuf[0]=IAC;
					sendbuf[1]=reply;
					sendbuf[2]=b;
					send(sendbuf);
					sentWX[b+128] = reply;
					receivedDX[b+128] = DO;
				}
				neg_state = STATE_DATA;
				break;
			case STATE_IACDONT:
				switch (b) {
				case TELOPT_ECHO:
					if(debug > 2) 
						System.out.println("ECHO");
					reply	= WONT;
					vec = new Vector(2);
					vec.addElement("NOLOCALECHO");
					peer.notifyStatus(vec);
					break;
				case TELOPT_NAWS:
					if(debug > 2) 
						System.out.println("NAWS");
					reply	= WONT;
					break;
				default:
					if(debug > 2) 
						System.out.println(
							"<UNKNOWN,"+b+">"
						);
					reply	= WONT;
					break;
				}
				if (	reply	!= sentWX[b+128] ||
					DONT	!= receivedDX[b+128]
				) {
					send(IAC);send(reply);send(b);
					sentWX[b+128]		= reply;
					receivedDX[b+128]	= DONT;
				}
				neg_state = STATE_DATA;
				break;
			case STATE_IACSBIAC:
				if(debug > 2) System.out.println(""+b+" ");
				if (b == IAC) {
					sbcount		= 0;
					current_sb	= b;
					neg_state	= STATE_IACSBDATA;
				} else {
					System.out.println("(bad) "+b+" ");
					neg_state	= STATE_DATA;
				}
				break;
			case STATE_IACSB:
				if(debug > 2) System.out.println(""+b+" ");
				switch (b) {
				case IAC:
					neg_state = STATE_IACSBIAC;
					break;
				default:
					current_sb	= b;
					sbcount		= 0;
					neg_state	= STATE_IACSBDATA;
					break;
				}
				break;
			case STATE_IACSBDATA:
				if (debug > 2) System.out.println(""+b+" ");
				switch (b) {
				case IAC:
					neg_state = STATE_IACSBDATAIAC;
					break;
				default:
					sbbuf[sbcount++] = b;
					break;
				}
				break;
			case STATE_IACSBDATAIAC:
				if (debug > 2) System.out.println(""+b+" ");
				switch (b) {
				case IAC:
					neg_state = STATE_IACSBDATA;
					sbbuf[sbcount++] = IAC;
					break;
				case SE:
					handle_sb(current_sb,sbbuf,sbcount);
					current_sb	= 0;
					neg_state	= STATE_DATA;
					break;
				case SB:
					handle_sb(current_sb,sbbuf,sbcount);
					neg_state	= STATE_IACSB;
					break;
				default:
					neg_state	= STATE_DATA;
					break;
				}
				break;
			default:
				if (debug > 2) 
					System.out.println(
						"This should not happen: "+
						neg_state+" "
					);
				neg_state = STATE_DATA;
				break;
			}
		}
		buf	= new byte[noffset];
		System.arraycopy(nbuf, 0, buf, 0, noffset);
		return buf;
	}
}
