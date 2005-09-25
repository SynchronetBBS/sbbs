package socket;

import java.io.IOException;
import java.util.Date;

/** Wrapper for a Java Telnet call. 
 * To use, make a new TelnetWrapper() with the name or IP address of a host.
 * Then, for most uses, the easiest way is to call setPrompt() with the
 * expected prompt, then call login(), and a sequence of sendLine()'s
 * until you get what you want done.
 * <P>
 * If you don't know the prompt ahead of time, you have to do a sequence of
 * send() and wait() or receiveUntil() calls.  send() sends a string across
 * the telnet connection. Add a '\r' to the end if you want to
 * complete a command. wait() waits for an exact string from the other side
 * of the telnet connection, and returns nothing,
 * receiveUntil() also waits for a string, but returns all the data
 * that it received while waiting, including the string itself. 
 * Use this if you want the output from a command. Please note that
 * the telnet connection will usually echo the sent command. 
 * <P>
 * sendLine() is generally better, since it adds the '\r'
 * automatically, waits for the prompt before returning, and returns all
 * data received before the prompt, with the prompt itself cut off the
 * end, and the sent command cut off the beginning. login() and
 * sendLine() are implemented using send(), wait() and receiveUntil().
 * They can be freely mixed and matched.
 * <P>
 * Here is a simple example of the use of TelnetWrapper:
 * <PRE>
 * // creates a new file in /tmp, lists the directory to prove it done
 * {
 *   TelnetWrapper telnet = new TelnetWrapper("123.45.78.90");
 *
 *   // setting the correct prompt ahead of time is very important 
 *   // if you want to use login and sendLine
 *   telnet.setPrompt("$ ");
 *   telnet.login("loginname", "password");
 *
 *   // this is how you have to do it otherwise
 *   telnet.send("touch /tmp/TELNET_WRAPPER" + "\r");
 *   telnet.wait("$ ");
 *
 *   // sendLine 1: adds the \r automatically, 2: waits for the prompt
 *   // before returning 3: returns what was printed from the command
 *   String ls = telnet.sendLine("ls /tmp");
 *   System.out.println(ls);
 *
 *   // clean up
 *   telnet.disconnect();
 * } 
 * </PRE>
 * @author George Ruban 3/4/97
 * @version 0.2 5/15/97 - added comments, replaced String += with
 *    StringBuffer.append() in receiveUntil(), added port constructor
 * @version 0.3 7/30/97 - added optional timeout to receiveUntil() and wait()
 * @see TelnetIO
 */
public class TelnetWrapper
{
  /** The telnet connection. That which is wrapped. */
  TelnetIO tio;
  /** Set to true for System.out.println debugging. */
  public boolean debug = false;
  /** The current prompt on the remote system. */
  private String prompt;

  /** The default prompt used by all TelnetWrappers unless specifically
   * overridden.
   * @see #setPrompt
   */
  private static String defaultPrompt = "$ ";

  /** The default login name used by TelnetWrappers.
   * If defaultLogin and defaultPassword are both non-null
   * when a TelnetWrapper is created, the TelnetWrapper will attempt
   * to login.
   */
  private static String defaultLogin = null;

  /** The default password used by TelnetWrappers.
   * If defaultLogin and defaultPassword are both non-null
   * when a TelnetWrapper is created, the TelnetWrapper will attempt
   * to login.
   */
  private static String defaultPassword = null;
  
  /** Skip any received data until the token appears. 
   * More efficient than receiveUntil, but liable to fail on large
   * tokens that can be spread over several "send"s. In that case,
   * consider using receiveUntil and ignoring the return value.
   * @param token String to wait for
   * @exception IOException on problems with the socket connection
   * @see #receiveUntil
   */
  public void wait(String token) throws IOException
  {
    wait(token, -1);
  }

  /** Wait for a String or a timeout. 
   * If time runs out, throws a TimedOutException.
   * Sleeps in intervals of 100 milliseconds until either receiving the
   * token or timeout.
   * <P>
   * More efficient than receiveUntil, but liable to fail on large
   * tokens that can be spread over several "send"s. In that case,
   * consider using receiveUntil and ignoring the return value.
   * @param token String to wait for
   * @param timeout time in milliseconds to wait (negative means wait forever)
   * @exception IOException on problems with the socket connection
   * @exception TimedOutException if time runs out before token received
   * @see #receiveUntil(String, long)
   */
  public void wait(String token, long timeout) 
    throws IOException, TimedOutException
  {
    if(debug) System.out.println("wait(" + token + ", " + timeout + ")...");
    String tmp = "";
    long deadline = 0;
    if(timeout >= 0) 
      deadline = new Date().getTime() + timeout;
    
    do {
      if(timeout >= 0)
      {
	while(available() <= 0)
	{
	  if(new Date().getTime() > deadline) 
	    throw new TimedOutException();
	  try{
	    Thread.currentThread().sleep(100);
	  }
	  catch(InterruptedException ignored)
	  {}
	}
      }
      tmp = receive();
    } while(tmp.indexOf(token) == -1);
    if(debug) System.out.println("wait(" + token  + ", " + timeout + 
				 ") successful.");
  }

  /** Returns bytes available to be read.  Since they haven't been
   * negotiated over, this could be misleading...
   */
  public int available() throws IOException
  {
    return tio.available();
  }
	
  /** Returns a String from the telnet connection. Blocks
   * until one is available. No guarantees that the string is in
   * any way complete.
   * NOTE: uses Java 1.0.2 style String-bytes conversion.*/
  public String receive() throws IOException
  {
    String s = new String(receiveBytes(), 0);
    if(debug) System.out.println(s);
    return s;
  }

  /** Returns a byte array. Blocks until data is available. */
  public byte[] receiveBytes() throws IOException
  {
    return tio.receive();
  }

  /** Returns all data received up until a certain token. 
   * @param token String to wait for
   * @exception IOException on problems with the socket connection
   * @see #wait
   */
  public String receiveUntil(String token) throws IOException
  {
    return receiveUntil(token, -1);
  }
  

  /** Returns all data received up until a certain token. 
   * @param token String to wait for
   * @param timeout time in milliseconds to wait (negative means wait forever)
   * @exception IOException on problems with the socket connection
   * @exception TimedOutException if time runs out before token received
   * @see #wait(String, long)
   */
  public String receiveUntil(String token, long timeout) 
    throws IOException, TimedOutException
  {
    StringBuffer buf = new StringBuffer();
    long deadline = 0;
    if(timeout >= 0) 
      deadline = new Date().getTime() + timeout;
    do
    {
      if(timeout >= 0)
      {
	while(available() <= 0)
	{
	  if(new Date().getTime() > deadline) 
	    throw new TimedOutException();
	  try{
	    Thread.currentThread().sleep(100);
	  }
	  catch(InterruptedException ignored)
	  {}
	}
      }
      buf.append(receive());
    } while(buf.toString().indexOf(token) == -1);
    return buf.toString();
  }
  
  /** Sends a String to the remote host.
   * NOTE: uses Java 1.0.2 style String-bytes conversion.
   * @exception IOException on problems with the socket connection
   */
  public void send(String s) throws IOException
  {
    if(debug) System.out.println(s);
    byte[] buf = new byte[s.length()];
    s.getBytes(0, buf.length, buf, 0);
    tio.send(buf);
  }

  /** Sends a line to the remote host, returns all data before the prompt.
   * Since telnet seems to rely on carriage returns ('\r'), 
   * one will be appended to the sent string, if necessary.
   * @param command command line to send
   * @return whatever data the command produced before the prompt.
   * @see #setPrompt
   */
  public String sendLine(String command) throws IOException
  {
    if(command.charAt(command.length() -1) != '\r') 
      command += "\r";
    send(command);
    String s = receiveUntil(prompt);

    // telnet typically echoes the command with a \r\n ending...
    return s.substring(command.length() + 1, s.indexOf(prompt));
  }
  
  /** Sends bytes over the telnet connection. */
  public void send(byte[] buf) throws IOException
  {
    tio.send(buf);
  }
  
  /** Logs in as a particular user and password. 
    * Returns after receiving prompt. */
  public void login(String loginName, String password) throws IOException
  {
    wait("login:");
    send(loginName + "\r");
    wait("Password:");
    sendLine(password + "\r");
  }
    
  /** Connects to the default telnet port on the given host. 
   * If the defaultLogin and defaultPassword are non-null, attempts login. */
  public TelnetWrapper(String host) throws IOException
  {
    tio = new TelnetIO();
    setPrompt(defaultPrompt);
    tio.connect(host);
    if(defaultLogin != null && defaultPassword != null)
    {
      login(defaultLogin, defaultPassword);
    }
  }

  /** Connects to a specific telnet port on the given host. 
   * If the defaultLogin and defaultPassword are non-null, attempts login. */
  public TelnetWrapper(String host, int port) throws IOException
  {
    tio = new TelnetIO();
    setPrompt(defaultPrompt);
    tio.connect(host, port);
    if(defaultLogin != null && defaultPassword != null)
    {
      login(defaultLogin, defaultPassword);
    }
  }
  
  /** Sets the expected prompt. 
   * If this function is not explicitly called, the default prompt is used.
   * @see #setDefaultPrompt
   */
  public void setPrompt(String prompt)
  {
    if(prompt == null) throw new IllegalArgumentException("null prompt.");
    this.prompt = prompt;
  }

  /** Sets the default prompt used by all TelnetWrappers.
   * This can be specifically overridden for a specific instance.
   * The default prompt starts out as "$ " until this function is called.
   * @see #setPrompt
   */
  public static void setDefaultPrompt(String prompt)
  {
    if(prompt == null) throw new IllegalArgumentException("null prompt.");
    defaultPrompt = prompt;
  }

  /** Sets the default login used by TelnetWrappers.
   * If this method is called with non-null login and password,
   * all TelnetWrappers will attempt to login when first created.
   * @param login login name to use
   * @param password password to use
   * @see #login
   * @see #unsetLogin
   */
  public static void setLogin(String login, String password)
  {
    if(login == null || password == null)
      throw new IllegalArgumentException("null login or password.");
    defaultLogin = login;
    defaultPassword = password;
  }


  /** Turns off the default login of TelnetWrappers.
   * After this method is called, TelnetWrappers will not
   * login until that method is explicitly called.
   * @see #setLogin
   * @see #login
   */
  public static void unsetLogin()
  {
    defaultLogin = defaultPassword = null;
  }
  
  /** Ends the telnet connection. */
  public void disconnect() throws IOException
  {
    if(tio != null) tio.disconnect();
    tio = null;
  }
  
  /** Ends the telnet connection. */
  public void finalize()
  {
    try
    {
      disconnect();
    }
    catch(IOException e)
    {} // after all, what can be done at this point?
  }  

  /** Telnet test driver.
   * Modeled after the IOtest.java example in the Telnet Applet.
   * Logs in to "host", creates a timestamped file in /tmp, lists the
   * /tmp directory to System.out, disconnects.  Shows off several
   * TelnetWrapper methods.
   * @param args host login password prompt
   */
  public static void main(String args[]) throws IOException
  {
    if(args.length != 4) throw new 
      IllegalArgumentException("Usage: TelnetWrapper host login password prompt");
    
    String host = args[0];
    String login = args[1];
    String password = args[2];
    String prompt = args[3];

    Date now = new Date();
    String timestamp = now.getYear() + "-" +
		(now.getMonth()+1) + "-" + now.getDate() + "-" +
		  now.getHours() + ":" + now.getMinutes() + ":" +
		    now.getSeconds();
    TelnetWrapper telnet = new TelnetWrapper(host);
    telnet.debug = true;

    // setting the correct prompt ahead of time is very important 
    // if you want to use login and sendLine
    telnet.setPrompt(prompt);
    telnet.login(login, password);

    // this is how you have to do it otherwise
    telnet.send("touch /tmp/TELNET_WRAPPER-" + timestamp + "\r");
    telnet.wait(prompt);

    // sendLine 1: adds the \r automatically, 2: waits for the prompt
    // before returning 3: returns what was printed from the command
    String ls = telnet.sendLine("ls /tmp");
    System.out.println(ls);

    // clean up
    telnet.disconnect();
  }
}

