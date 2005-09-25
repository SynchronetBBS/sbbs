/*
 * vt320 -- a DEC VT320 Terminal emulation
 * --
 * $Id$
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

package display;

import java.awt.Scrollbar;
import java.awt.Event;
import java.awt.Dimension;
import java.awt.BorderLayout;
import java.awt.FlowLayout;
import java.util.Vector;

import java.applet.Applet;

/**
 * A DEC VT320 Terminal Emulation (includes VT100/220 and ANSI).
 *
 * The terminal emulation accesses the applet parameters to configure itself.
 * The following parameters may be set. Default values are written in
 * <I>italics</I> and other possible values are <B>bold</B>.
 * <DL>
 *  <DT><TT>&lt;PARAM NAME="Fx" VALUE="<I>functionkeytext</I>"&gt</TT>
 *  <DD>Sets the string sent when the function key Fx (x between 1 und 20)
 *	is pressed.
 *  <DT><TT>&lt;PARAM NAME="VTcolumns" VALUE="<I>80</I>"&gt</TT>
 *  <DD>Sets the columns of the terminal initially. If the parameter
 *      VTresize is set to <B>screen</B> this may change, else it is fixed.
 *  <DT><TT>&lt;PARAM NAME="VTrows" VALUE="<I>24</I>"&gt</TT>
 *  <DD>Sets the rows of the terminal initially. If the parameter
 *      value of VTresize <B>screen</B> this may change!
 *  <DT><TT>&lt;PARAM NAME="VTfont" VALUE="<I>Courier</I>"&gt</TT>
 *  <DD>Sets the font to be used for the terminal. It is recommended to
 *      use <I>Courier</I> or at least a fixed width font.
 *  <DT><TT>&lt;PARAM NAME="VTfontsize" VALUE="<I>14</I>"&gt</TT>
 *  <DD>Sets the font size for the terminal. If the parameter
 *      value of VTresize is set to <B>font</B> this may change!
 *  <DT><TT>&lt;PARAM NAME="VTresize" VALUE="<I>font</I>"&gt</TT>
 *  <DD>This parameter determines what the terminal should do if the window
 *      is resized. The default setting <I><B>font</B></I> will result in
 *      resizing the font until is matches the window best. Other possible
 *      values are <B>none</B> or <B>screen</B>. <B>none</B> will let nothing
 *      happen and <B>screen</B> will let the display try to change the
 *      amount of rows and columns to match the window best.
 *  <DT><TT>&lt;PARAM NAME="VTscrollbar" VALUE="<I>false</I>"&gt</TT>
 *  <DD>Setting this parameter to <B>true</B> will add a scrollbar west to
 *      the terminal. Other possible values include <B>left</B> to put the
 *      scrollbar on the left side of the terminal and <B>right</B> to put it
 *      explicitely to the right side.
 *  <DT><TT>&lt;PARAM NAME="VTid" VALUE="<I>vt320</I>"&gt</TT>
 *  <DD>This parameter will override the terminal id <I>vt320</I>. It may
 *      be used to determine special terminal abilities of VT Terminals.
 *  <DT><TT>&lt;PARAM NAME="VTbuffer" VALUE="<I>xx</I>"&gt</TT>
 *  <DD>Initially this parameter is the same as the VTrows parameter. It
 *      cannot be less than the amount of rows on the display. It determines
 *      the available scrollback buffer.
 *  <DT><TT>&lt;PARAM NAME="VTcharset" VALUE="<I>none</I>"&gt</TT>
 *  <DD>Setting this parameter to <B>ibm</B> will enable mapping of ibm
 *      characters (as used in PC BBS systems) to UNICODE characters. Note
 *      that those special characters probably won't show on UNIX systems
 *      due to lack in X11 UNICODE support.
 *  <DT><TT>&lt;PARAM NAME="VTvms" VALUE="<I>false</I>"&gt</TT>
 *  <DD>Setting this parameter to <B>true</B> will change the Backspace key
 *      into a delete key, cause the numeric keypad keys to emit VT100
 *      codes when Ctrl is pressed, and make other VMS-important keyboard
 *      definitions.
 *  <DT><TT>&lt;PARAM NAME="F<I>nr</I>" VALUE="<I>string</I>"&gt</TT>
 *  <DD>Function keys from <I>F1</I> to <I>F20</I> are programmable. You can
 *      install any possible string including special characters, such as
 *      <TABLE BORDER>
 *      <TR><TD><TT>\e</TT></TD><TD>Escape</TD>
 *      <TR><TD><TT>\b</TT></TD><TD>Backspace</TD>
 *      <TR><TD><TT>\n</TT></TD><TD>Newline</TD>
 *      <TR><TD><TT>\r</TT></TD><TD>Return</TD>
 *      <TR><TD><TT>\xxxx</TT></TD><TD>Character xxxx (decimal)</TD>
 *      </TABLE>
 *  <DT><TT>&lt;PARAM NAME="CF<I>nr</I>" VALUE="<I>string</I>"&gt</TT>
 *  <DD>Function keys (with the Control-key pressed) from <I>CF1</I> to <I>CF20</I> are programmable too.
 *  <DT><TT>&lt;PARAM NAME="SF<I>nr</I>" VALUE="<I>string</I>"&gt</TT>
 *  <DD>Function keys (with the Shift-key pressed) from <I>SF1</I> to <I>SF20</I> are programmable too.
 *  <DT><TT>&lt;PARAM NAME="AF<I>nr</I>" VALUE="<I>string</I>"&gt</TT>
 *  <DD>Function keys (with the Alt-key pressed) from <I>AF1</I> to <I>AF20</I> are programmable too.
 * </DL>
 * @version $Id$
 * @author  Matthias L. Jugel, Marcus Mei?ner
 */
public class vt320 extends Terminal implements TerminalHost
{
  /**
   * Return the version of the terminal emulation and its display.
   */
  public String toString() { return "$Id$ "+display.version; }

  // the input handler takes the keyboard input from us.
  private TerminalHost host = this;

  // due to a bug with Windows we need a keypress cache
  private int pressedKey = ' ';
  private long pressedWhen = ' ';

  // The character display
  private CharDisplay display;
  private static int debug = 0;
  private String terminalID = "vt320";

  // X - COLUMNS, Y - ROWS
  int  R,C;
  int  Sc,Sr,Sa;
  int  attributes  = 0;
  int  insertmode  = 0;
  int  statusmode = 0;
  int  vt52mode  = 0;
  int  normalcursor  = 0;
  boolean moveoutsidemargins = true;
  boolean sendcrlf = true;

  private boolean  useibmcharset = false;

  private  static  int  lastwaslf = 0;
  private static  int i;
  private final static char ESC = 27;
  private final static char IND = 132;
  private final static char NEL = 133;
  private final static char RI  = 141;
  private final static char HTS = 136;
  private final static char DCS = 144;
  private final static char CSI = 155;
  private final static char OSC = 157;
  private final static int TSTATE_DATA  = 0;
  private final static int TSTATE_ESC  = 1; /* ESC */
  private final static int TSTATE_CSI  = 2; /* ESC [ */
  private final static int TSTATE_DCS  = 3; /* ESC P */
  private final static int TSTATE_DCEQ  = 4; /* ESC [? */
  private final static int TSTATE_ESCSQUARE= 5; /* ESC # */
  private final static int TSTATE_OSC= 6;       /* ESC ] */
  private final static int TSTATE_SETG0= 7;     /* ESC (? */
  private final static int TSTATE_SETG1= 8;     /* ESC )? */
  private final static int TSTATE_SETG2= 9;     /* ESC *? */
  private final static int TSTATE_SETG3= 10;    /* ESC +? */
  private final static int TSTATE_CSI_DOLLAR  = 11; /* ESC [ Pn $ */

  /* The graphics charsets
   * B - default ASCII
   * A - default UK
   * 0 - DEC SPECIAL
   * < - User defined
   * ....
   */
  private static char gx[] = {
  'B',      // g0
  '0',      // g1
  'A',      // g2
  '<',      // g3
  };
  private static char gr = 1;  // default GR to G1
  private static char gl = 0;  // default GL to G0

  // array to store DEC Special -> Unicode mapping
  //  Unicode   DEC  Unicode name    (DEC name)
  private static char DECSPECIAL[] = {
    '\u0040', //5f blank
    '\u2666', //60 black diamond
    '\u2592', //61 grey square
    '\u2409', //62 Horizontal tab  (ht) pict. for control
    '\u240c', //63 Form Feed       (ff) pict. for control
    '\u240d', //64 Carriage Return (cr) pict. for control
    '\u240a', //65 Line Feed       (lf) pict. for control
    '\u00ba', //66 Masculine ordinal indicator
    '\u00b1', //67 Plus or minus sign
    '\u2424', //68 New Line        (nl) pict. for control
    '\u240b', //69 Vertical Tab    (vt) pict. for control
    '\u2518', //6a Forms light up   and left
    '\u2510', //6b Forms light down and left
    '\u250c', //6c Forms light down and right
    '\u2514', //6d Forms light up   and right
    '\u253c', //6e Forms light vertical and horizontal
    '\u2594', //6f Upper 1/8 block                        (Scan 1)
    '\u2580', //70 Upper 1/2 block                        (Scan 3)
    '\u2500', //71 Forms light horizontal or ?em dash?    (Scan 5)
    '\u25ac', //72 \u25ac black rect. or \u2582 lower 1/4 (Scan 7)
    '\u005f', //73 \u005f underscore  or \u2581 lower 1/8 (Scan 9)
    '\u251c', //74 Forms light vertical and right
    '\u2524', //75 Forms light vertical and left
    '\u2534', //76 Forms light up   and horizontal
    '\u252c', //77 Forms light down and horizontal
    '\u2502', //78 vertical bar
    '\u2264', //79 less than or equal
    '\u2265', //7a greater than or equal
    '\u00b6', //7b paragraph
    '\u2260', //7c not equal
    '\u00a3', //7d Pound Sign (british)
    '\u00b7'  //7e Middle Dot
  };

  private final static int KEYUP  = Event.UP % 1000;
  private final static int KEYDOWN  = Event.DOWN % 1000;
  private final static int KEYLEFT  = Event.LEFT % 1000;
  private final static int KEYRIGHT  = Event.RIGHT % 1000;
  private final static int KEYF1  = Event.F1 % 1000;
  private final static int KEYF2  = Event.F2 % 1000;
  private final static int KEYF3  = Event.F3 % 1000;
  private final static int KEYF4  = Event.F4 % 1000;
  private final static int KEYF5  = Event.F5 % 1000;
  private final static int KEYF6  = Event.F6 % 1000;
  private final static int KEYF7  = Event.F7 % 1000;
  private final static int KEYF8  = Event.F8 % 1000;
  private final static int KEYF9  = Event.F9 % 1000;
  private final static int KEYF10  = Event.F10 % 1000;
  private final static int KEYF11  = Event.F11 % 1000;
  private final static int KEYF12  = Event.F12 % 1000;
  private final static int KEYPGDN   = Event.PGDN % 1000;
  private final static int KEYPGUP   = Event.PGUP % 1000;

  private final static int KEYHOME  = Event.HOME % 1000;
  private final static int KEYEND  = Event.END % 1000;

  public static final int KEYPRINT_SCREEN  = 20;
  public static final int KEYSCROLL_LOCK    = 21;
  public static final int KEYCAPS_LOCK    = 22;
  public static final int KEYNUM_LOCK    = 23;
  public static final int KEYPAUSE    = 24;
  public static final int KEYINSERT    = 25;

    /**
     * The Insert key.
     */
    public static final int INSERT    = 1025;

  /**
   * Strings to send on function key presseic
   */
  private String FunctionKey[];
  private String FunctionKeyShift[];
  private String FunctionKeyCtrl[];
  private String FunctionKeyAlt[];
  private String KeyUp;
  private String KeyDown;
  private String KeyLeft;
  private String KeyRight;
  private String KeyBacktab;
  private String KeyTab;

  private String KP0;
  private String KP1;
  private String KP2;
  private String KP3;
  private String KP4;
  private String KP5;
  private String KP6;
  private String KP7;
  private String KP8;
  private String KP9;
  private String KPMinus;
  private String KPComma;
  private String KPPeriod;
  private String KPEnter;
  private String PF1;
  private String PF2;
  private String PF3;
  private String PF4;
  private String Help;
  private String Do;
  private String Find;
  private String Insert;
  private String Remove;
  private String Select;
  private String PrevScn;
  private String NextScn;


  private String osc,dcs;  /* to memorize OSC & DCS control sequence */

  private int term_state = TSTATE_DATA;
  private boolean vms = false;
  private byte[]  Tabs;
  private int[]  DCEvars = new int [10];
  private  int  DCEvar;

  /* operation system we run on, Scrollbar hack */
  private String osn = System.getProperty("os.name");

  public String[][] getParameterInfo() {
    String pinfo[][] = {
    {"VTcolumns",  "Integer",   "Columns of the terminal"},
    {"VTrows",     "Integer",   "Rows of the terminal"},
    {"VTfont",     "String",    "Terminal font (default is Courier)"},
    {"VTfontsize", "Integer",   "Font size"},
    {"VTbuffer",   "Integer",   "Scrollback buffer size"},
    {"VTscrollbar","Boolean",   "Enable or disable scrollbar"},
    {"VTresize",   "String",    "One of none, font, screen"},
    {"VTid",       "String",    "Terminal id, standard is VT320"},
    {"VTcharset",  "String",    "Charset used"},
    {"VTvms",      "Boolean",   "Enable or disable VMS key mappings"},
    {"F1 - F20",   "String",    "Programmable Function Keys"},
    {"SF1 - SF20",   "String",    "Programmable Shift-ed Function Keys"},
    {"CF1 - CF20",   "String",    "Programmable Control-ed Function Keys"},
    {"AF1 - AF20",   "String",    "Programmable Alt-ed Function Keys"},
    };
    return pinfo;
  }

  static String unEscape(String tmp) {
      int idx = 0, oldidx = 0;
      String cmd;

      cmd = "";
      while((idx = tmp.indexOf('\\', oldidx)) >= 0 &&
            ++idx <= tmp.length()) {
        cmd += tmp.substring(oldidx, idx-1);
        if(idx == tmp.length()) return cmd;
        switch(tmp.charAt(idx)) {
        case 'b': cmd += "\b"; break;
        case 'e': cmd += "\u001b"; break;
        case 'n': cmd += "\n"; break;
        case 'r': cmd += "\r"; break;
        case 't': cmd += "\t"; break;
        case 'v': cmd += "\u000b"; break;
        case 'a': cmd += "\u0012"; break;
        default : 
	  if ( (tmp.charAt(idx)>='0') && (tmp.charAt(idx)<='9')) {
	    for (i = idx;i<tmp.length();i++) {
	      if ( (tmp.charAt(i)<'0') || (tmp.charAt(i)>'9'))
	        break;
	    }
	    cmd += (char)(new java.lang.Integer(tmp.substring(idx, i)).intValue());
	    idx = i-1;
	  } else
	    cmd += tmp.substring(idx, ++idx);break;
	}
	oldidx = ++idx;
      }
      if(oldidx <= tmp.length()) cmd += tmp.substring(oldidx);
      return cmd;
  }

  /**
   * Initialize terminal.
   * @param parent the applet parent where to get parameters from
   * @see display.Terminal
   */
  public void addNotify() {
    if(display == null) {
      String width = "80", height = "24", resize ="font";
      String font = "Courier", fs = "14", bufs = "100";
      String scrb = "false";
      String vms = "false";
      String ibmcset = "false";

      Applet applet = (Applet)getParent();

      if(applet != null) {
        try {
          host = (TerminalHost)applet;
        } catch(ClassCastException e) {
          System.err.println("vt320: "+applet+" cannot receive terminal input");
          host = this;
        }

        width  = applet.getParameter("VTcolumns");
        height = applet.getParameter("VTrows");
        font   = applet.getParameter("VTfont");
        fs     = applet.getParameter("VTfontsize");
        bufs   = applet.getParameter("VTbuffer");
        scrb   = applet.getParameter("VTscrollbar");
        vms    = applet.getParameter("VTvms");
        resize = applet.getParameter("VTresize");
        resize = resize == null ? "font" : resize;
        ibmcset = applet.getParameter("VTcharset");
        if ((ibmcset!=null)&&(ibmcset.equals("ibm")))
          useibmcharset=true;

        if(applet.getParameter("VTid") != null)
          terminalID = applet.getParameter("VTid");
      }

      display = new CharDisplay(
          width==null?80:(new Integer(width)).intValue(),
          (height==null?24:(new Integer(height)).intValue()),
          font==null?"Courier":font,
          fs==null?14:(new Integer(fs)).intValue()
      );
      display.setBottomMargin((height==null?
			       24:
			       (new Integer(height)).intValue()) - 1);
      display.setBufferSize(bufs==null?100:(new Integer(bufs)).intValue());
      if(resize.equals("none"))
        display.setResizeStrategy(CharDisplay.RESIZE_NONE);
      if(resize.equals("font"))
        display.setResizeStrategy(CharDisplay.RESIZE_FONT);
      if(resize.equals("screen"))
        display.setResizeStrategy(CharDisplay.RESIZE_SCREEN);

      display.setBorder(2, false);

      setLayout(new BorderLayout());
      if(scrb != null && !scrb.equals("false")) {
        if(scrb.equals("left") || scrb.equals("true"))
          display.setScrollbar("West");
        else if(scrb.equals("right"))
          display.setScrollbar("East");
        else
          System.out.println("vt320: unknown scrollbar location: "+scrb);
      }
      if(vms != null && vms.equals("true"))
      {
        this.vms = true;
      }
      add("Center", display);
      InitTerminalVars();
      for (int i=1;i<20;i++)
      {
      	if (applet.getParameter("F"+i)!=null)
	  FunctionKey[i] = unEscape(applet.getParameter("F"+i));
        if (applet.getParameter("SF"+i)!=null)
          FunctionKeyShift[i] = unEscape(applet.getParameter("SF"+i));
        if (applet.getParameter("CF"+i)!=null)
          FunctionKeyCtrl[i] = unEscape(applet.getParameter("CF"+i));
        if (applet.getParameter("AF"+i)!=null)
          FunctionKeyAlt[i] = unEscape(applet.getParameter("AF"+i));
      }
    }
    super.addNotify();
  }

  private void InitTerminalVars() {
    int nw = display.getColumns();

    if (nw<132) nw=132; //catch possible later 132/80 resizes
    Tabs = new byte[nw];
    for (int i=0;i<nw;i+=8) {
      Tabs[i]=1;
    }

    PF1  = "\u001bOP";
    PF2  = "\u001bOQ";
    PF3  = "\u001bOR";
    PF4  = "\u001bOS";

    Find = "\u001b[1~";
    Insert = "\u001b[2~";
    Remove = "\u001b[3~";
    Select = "\u001b[4~";
    PrevScn = "\u001b[5~";
    NextScn = "\u001b[6~";

    Help = "\u001b[28~";
    Do = "\u001b[29~";

    FunctionKey = new String[21];
    FunctionKey[0]= "";
    FunctionKey[1]= PF1;
    FunctionKey[2]= PF2;
    FunctionKey[3]= PF3;
    FunctionKey[4]= PF4;
    /* following are defined differently for vt220 / vt132 ... */
    FunctionKey[5]= "\u001b[15~";
    FunctionKey[6]= "\u001b[17~";
    FunctionKey[7]= "\u001b[18~";
    FunctionKey[8]= "\u001b[19~";
    FunctionKey[9]= "\u001b[20~";
    FunctionKey[10]= "\u001b[21~";
    FunctionKey[11]= "\u001b[23~";
    FunctionKey[12]= "\u001b[24~";
    FunctionKey[13]= "\u001b[25~";
    FunctionKey[14]= "\u001b[26~";
    FunctionKey[15]= Help;
    FunctionKey[16]= Do;
    FunctionKey[17]= "\u001b[31~";
    FunctionKey[18]= "\u001b[32~";
    FunctionKey[19]= "\u001b[33~";
    FunctionKey[20]= "\u001b[34~";

    FunctionKeyShift = new String[21];
    FunctionKeyAlt = new String[21];
    FunctionKeyCtrl = new String[21];

    for (int i=0;i<20;i++)
    {
       FunctionKeyShift[i]="";
       FunctionKeyAlt[i]="";
       FunctionKeyCtrl[i]="";
    }
    FunctionKeyShift[15] = Find;
    FunctionKeyShift[16] = Select;

    KeyTab   = "\u0009";
    KeyBacktab = "\u001bOP\u0009";
    KeyUp    = "\u001b[A";
    KeyDown  = "\u001b[B";
    KeyRight  = "\u001b[C";
    KeyLeft  = "\u001b[D";
    KP0  = "\u001bOp";
    KP1  = "\u001bOq";
    KP2  = "\u001bOr";
    KP3  = "\u001bOs";
    KP4  = "\u001bOt";
    KP5  = "\u001bOu";
    KP6  = "\u001bOv";
    KP7  = "\u001bOw";
    KP8  = "\u001bOx";
    KP9  = "\u001bOy";
    KPMinus  = "\u001bOm";
    KPComma  = "\u001bOl";
    KPPeriod  = "\u001bOn";
    KPEnter  = "\u001bOM";

    /* ... */
  }

  public Dimension getSize() {
    return new Dimension(display.getColumns(), display.getRows());
  }

  public String getTerminalType() { return terminalID; }

  /**
   * Handle events for the terminal. Only accept events for the scroll
   * bar. Any other events have to be propagated to the parent.
   * @param evt the event
   */
  public boolean handleEvent(Event evt) {
    // request focus if mouse enters (necessary to avoid mistakes)
    if(evt.id == Event.MOUSE_ENTER) { display.requestFocus(); return true; }
    // give focus away if mouse leaves the window
    if(evt.id == Event.MOUSE_EXIT) { nextFocus(); return true; }

    // handle keyboard events

    /* Netscape for windows does not send keyDown when period
    * is pressed. This hack catches the keyUp event.
    */
    int id = evt.id;

    boolean control = (((evt.CTRL_MASK & evt.modifiers)==0) ? false : true);
    boolean shift = (((evt.SHIFT_MASK & evt.modifiers)==0) ? false : true );
    boolean alt = (((evt.ALT_MASK & evt.modifiers)==0) ? false :true);

    pressedKey:
    {
        if (pressedKey == 10
            && (evt.key == 10
            || evt.key == 13)
            && evt.when - pressedWhen < 50)    //  Ray: Elliminate stuttering enter/return
            break pressedKey;

        int priorKey = pressedKey;
        if(id == Event.KEY_PRESS)
            pressedKey = evt.key;               //  Keep track of last pressed key
        else if (evt.key == '.'
            && evt.id != Event.KEY_RELEASE
            && evt.key != pressedKey
        ) {
            pressedKey = 0;
        } else
            break pressedKey;
        pressedWhen = evt.when;
        if (evt.key == 10 && !control) {
            if (sendcrlf)
              host.send("\r\n"); /* YES, see RFC 854 */
            else
              host.send("\r"); /* YES, see RFC 854 */
            return true;
        } else
/* FIXME: on german PC keyboards you have to use Alt-Ctrl-q to get an @,
 * so we can't just use it here... will probably break some other VMS stuff
 * -Marcus
        if (((!vms && evt.key == '2') || evt.key == '@' || evt.key == ' ') && control)
 */
        if (((!vms && evt.key == '2') || evt.key == ' ') && control)
            return host.send("" + (char)0);
        else if (vms) {
	    if (evt.key == 8) {
		if (shift && !control)
		    return host.send("" + (char)10); //  VMS shift deletes word back
		if (control && !shift)
		    return host.send("" + (char)24); //  VMS control deletes line back
		return host.send("" + (char)127);   //  VMS other is delete
	    }
	    if (evt.key == 127 && !control) {
		if (shift)
		    return host.send(Insert); //  VMS shift delete = insert
		else
		    return host.send(Remove); //  VMS delete = remove
	    }
            if (control) {
                switch(evt.key) {
                    case '0':
                        return host.send(KP0);
                    case '1':
                        return host.send(KP1);
                    case '2':
                        return host.send(KP2);
                    case '3':
                        return host.send(KP3);
                    case '4':
                        return host.send(KP4);
                    case '5':
                        return host.send(KP5);
                    case '6':
                        return host.send(KP6);
                    case '7':
                        return host.send(KP7);
                    case '8':
                        return host.send(KP8);
                    case '9':
                        return host.send(KP9);
                    case '.':
                        return host.send(KPPeriod);
                    case '-':
                    case 31:
                        return host.send(KPMinus);
                    case '+':
                        return host.send(KPComma);
                    case 10:
                        return host.send(KPEnter);
                    case '/':
                        return host.send(PF2);
                    case '*':
                        return host.send(PF3);
                }
                if (shift && evt.key < 32)
                    return host.send(PF1+(char)(evt.key + 64));
            }
	}
	// Hmmm. Outside the VMS case?
        if (shift && (evt.key == '\t'))
	    return host.send(KeyBacktab);
	else
	    return host.send(""+(char)evt.key);
    }

    String input = "";

    if (evt.id == evt.KEY_ACTION)
    {
      String fmap[];
      /* bloodsucking little buggers at netscape
       * don't keep to the standard java values
       * of <ARROW> or <Fx>
       */
      fmap = FunctionKey;
      if (shift)
	fmap = FunctionKeyShift;
      if (control)
        fmap = FunctionKeyCtrl;
      if (alt)
        fmap = FunctionKeyAlt;
      switch (evt.key % 1000) {
      case KEYF1:
        input = fmap[1];
        break;
      case KEYF2:
        input = fmap[2];
        break;
      case KEYF3:
        input = fmap[3];
        break;
      case KEYF4:
        input = fmap[4];
        break;
      case KEYF5:
        input = fmap[5];
        break;
      case KEYF6:
        input = fmap[6];
        break;
      case KEYF7:
        input = fmap[7];
        break;
      case KEYF8:
        input = fmap[8];
        break;
      case KEYF9:
        input = fmap[9];
        break;
      case KEYF10:
        input = fmap[10];
        break;
      case KEYF11:
        input = fmap[11];
        break;
      case KEYF12:
        input = fmap[12];
        break;
      case KEYUP:
        input = KeyUp;
        break;
      case KEYDOWN:
        input = KeyDown;
        break;
      case KEYLEFT:
        input = KeyLeft;
        break;
      case KEYRIGHT:
        input = KeyRight;
        break;
      case KEYPGDN:
        input = NextScn;
        break;
      case KEYPGUP:
        input = PrevScn;
        break;
      case KEYINSERT:
        input = Insert;
        break;
//  The following cases fall through if not in VMS mode.
      case KEYHOME:
        if (vms)
        {
            input = "" + (char)8;
            break;
        }
	//FALLTHROUGH
      case KEYEND:
        if (vms)
        {
            input = "" + (char)5;
            break;
        }
	//FALLTHROUGH
      case KEYNUM_LOCK:
        if (vms && control) {
            if (pressedKey != evt.key) {
                pressedKey = evt.key;
                input = PF1;
            } else
                pressedKey = ' ';   //  Here, we eat a second numlock since that returns numlock state
        }
	break;
      default:
        /*if (debug>0)*/
          System.out.println("vt320: unknown event:"+(int)evt.key);
        break;
      }
    }

    if(input != null && input.length() > 0)
      return host.send(input);

    return false;
  }

  /**
   * Dummy method to handle input events (String).
   * This is only needed if our parent is not TerminalHost
   * @param s String to handle
   * @return always true
   * @see display.TerminalHost
   */
  public boolean send(String s) {
    putString(s);
    return true;
  }

  private void handle_dcs(String dcs) {
    System.out.println("DCS: "+dcs);
  }
  private void handle_osc(String osc) {
    System.out.println("OSC: "+osc);
  }

  /**
  * Put String at current cursor position. Moves cursor
  * according to the String. Does NOT wrap.
  * @param s the string
  */
  public void putString(String s) {
    int  i,len=s.length();

    display.markLine(R,1);
    for (i=0;i<len;i++)
      putChar(s.charAt(i),false);
    display.setCursorPos(C, R);
    display.redraw();
  }

  public void putChar(char c) {
    putChar(c,true);
    display.redraw();
  }

  public final static char unimap[] = {
//#
//#    Name:     cp437_DOSLatinUS to Unicode table
//#    Unicode version: 1.1
//#    Table version: 1.1
//#    Table format:  Format A
//#    Date:          03/31/95
//#    Authors:       Michel Suignard <michelsu@microsoft.com>
//#                   Lori Hoerth <lorih@microsoft.com>
//#    General notes: none
//#
//#    Format: Three tab-separated columns
//#        Column #1 is the cp1255_WinHebrew code (in hex)
//#        Column #2 is the Unicode (in hex as 0xXXXX)
//#        Column #3 is the Unicode name (follows a comment sign, '#')
//#
//#    The entries are in cp437_DOSLatinUS order
//#

  0x0000,// #NULL
  0x0001,// #START OF HEADING
  0x0002,// #START OF TEXT
  0x0003,// #END OF TEXT
  0x0004,// #END OF TRANSMISSION
  0x0005,// #ENQUIRY
  0x0006,// #ACKNOWLEDGE
  0x0007,// #BELL
  0x0008,// #BACKSPACE
  0x0009,// #HORIZONTAL TABULATION
  0x000a,// #LINE FEED
  0x000b,// #VERTICAL TABULATION
  0x000c,// #FORM FEED
  0x000d,// #CARRIAGE RETURN
  0x000e,// #SHIFT OUT
  0x000f,// #SHIFT IN
  0x0010,// #DATA LINK ESCAPE
  0x0011,// #DEVICE CONTROL ONE
  0x0012,// #DEVICE CONTROL TWO
  0x0013,// #DEVICE CONTROL THREE
  0x0014,// #DEVICE CONTROL FOUR
  0x0015,// #NEGATIVE ACKNOWLEDGE
  0x0016,// #SYNCHRONOUS IDLE
  0x0017,// #END OF TRANSMISSION BLOCK
  0x0018,// #CANCEL
  0x0019,// #END OF MEDIUM
  0x001a,// #SUBSTITUTE
  0x001b,// #ESCAPE
  0x001c,// #FILE SEPARATOR
  0x001d,// #GROUP SEPARATOR
  0x001e,// #RECORD SEPARATOR
  0x001f,// #UNIT SEPARATOR
  0x0020,// #SPACE
  0x0021,// #EXCLAMATION MARK
  0x0022,// #QUOTATION MARK
  0x0023,// #NUMBER SIGN
  0x0024,// #DOLLAR SIGN
  0x0025,// #PERCENT SIGN
  0x0026,// #AMPERSAND
  0x0027,// #APOSTROPHE
  0x0028,// #LEFT PARENTHESIS
  0x0029,// #RIGHT PARENTHESIS
  0x002a,// #ASTERISK
  0x002b,// #PLUS SIGN
  0x002c,// #COMMA
  0x002d,// #HYPHEN-MINUS
  0x002e,// #FULL STOP
  0x002f,// #SOLIDUS
  0x0030,// #DIGIT ZERO
  0x0031,// #DIGIT ONE
  0x0032,// #DIGIT TWO
  0x0033,// #DIGIT THREE
  0x0034,// #DIGIT FOUR
  0x0035,// #DIGIT FIVE
  0x0036,// #DIGIT SIX
  0x0037,// #DIGIT SEVEN
  0x0038,// #DIGIT EIGHT
  0x0039,// #DIGIT NINE
  0x003a,// #COLON
  0x003b,// #SEMICOLON
  0x003c,// #LESS-THAN SIGN
  0x003d,// #EQUALS SIGN
  0x003e,// #GREATER-THAN SIGN
  0x003f,// #QUESTION MARK
  0x0040,// #COMMERCIAL AT
  0x0041,// #LATIN CAPITAL LETTER A
  0x0042,// #LATIN CAPITAL LETTER B
  0x0043,// #LATIN CAPITAL LETTER C
  0x0044,// #LATIN CAPITAL LETTER D
  0x0045,// #LATIN CAPITAL LETTER E
  0x0046,// #LATIN CAPITAL LETTER F
  0x0047,// #LATIN CAPITAL LETTER G
  0x0048,// #LATIN CAPITAL LETTER H
  0x0049,// #LATIN CAPITAL LETTER I
  0x004a,// #LATIN CAPITAL LETTER J
  0x004b,// #LATIN CAPITAL LETTER K
  0x004c,// #LATIN CAPITAL LETTER L
  0x004d,// #LATIN CAPITAL LETTER M
  0x004e,// #LATIN CAPITAL LETTER N
  0x004f,// #LATIN CAPITAL LETTER O
  0x0050,// #LATIN CAPITAL LETTER P
  0x0051,// #LATIN CAPITAL LETTER Q
  0x0052,// #LATIN CAPITAL LETTER R
  0x0053,// #LATIN CAPITAL LETTER S
  0x0054,// #LATIN CAPITAL LETTER T
  0x0055,// #LATIN CAPITAL LETTER U
  0x0056,// #LATIN CAPITAL LETTER V
  0x0057,// #LATIN CAPITAL LETTER W
  0x0058,// #LATIN CAPITAL LETTER X
  0x0059,// #LATIN CAPITAL LETTER Y
  0x005a,// #LATIN CAPITAL LETTER Z
  0x005b,// #LEFT SQUARE BRACKET
  0x005c,// #REVERSE SOLIDUS
  0x005d,// #RIGHT SQUARE BRACKET
  0x005e,// #CIRCUMFLEX ACCENT
  0x005f,// #LOW LINE
  0x0060,// #GRAVE ACCENT
  0x0061,// #LATIN SMALL LETTER A
  0x0062,// #LATIN SMALL LETTER B
  0x0063,// #LATIN SMALL LETTER C
  0x0064,// #LATIN SMALL LETTER D
  0x0065,// #LATIN SMALL LETTER E
  0x0066,// #LATIN SMALL LETTER F
  0x0067,// #LATIN SMALL LETTER G
  0x0068,// #LATIN SMALL LETTER H
  0x0069,// #LATIN SMALL LETTER I
  0x006a,// #LATIN SMALL LETTER J
  0x006b,// #LATIN SMALL LETTER K
  0x006c,// #LATIN SMALL LETTER L
  0x006d,// #LATIN SMALL LETTER M
  0x006e,// #LATIN SMALL LETTER N
  0x006f,// #LATIN SMALL LETTER O
  0x0070,// #LATIN SMALL LETTER P
  0x0071,// #LATIN SMALL LETTER Q
  0x0072,// #LATIN SMALL LETTER R
  0x0073,// #LATIN SMALL LETTER S
  0x0074,// #LATIN SMALL LETTER T
  0x0075,// #LATIN SMALL LETTER U
  0x0076,// #LATIN SMALL LETTER V
  0x0077,// #LATIN SMALL LETTER W
  0x0078,// #LATIN SMALL LETTER X
  0x0079,// #LATIN SMALL LETTER Y
  0x007a,// #LATIN SMALL LETTER Z
  0x007b,// #LEFT CURLY BRACKET
  0x007c,// #VERTICAL LINE
  0x007d,// #RIGHT CURLY BRACKET
  0x007e,// #TILDE
  0x007f,// #DELETE
  0x00c7,// #LATIN CAPITAL LETTER C WITH CEDILLA
  0x00fc,// #LATIN SMALL LETTER U WITH DIAERESIS
  0x00e9,// #LATIN SMALL LETTER E WITH ACUTE
  0x00e2,// #LATIN SMALL LETTER A WITH CIRCUMFLEX
  0x00e4,// #LATIN SMALL LETTER A WITH DIAERESIS
  0x00e0,// #LATIN SMALL LETTER A WITH GRAVE
  0x00e5,// #LATIN SMALL LETTER A WITH RING ABOVE
  0x00e7,// #LATIN SMALL LETTER C WITH CEDILLA
  0x00ea,// #LATIN SMALL LETTER E WITH CIRCUMFLEX
  0x00eb,// #LATIN SMALL LETTER E WITH DIAERESIS
  0x00e8,// #LATIN SMALL LETTER E WITH GRAVE
  0x00ef,// #LATIN SMALL LETTER I WITH DIAERESIS
  0x00ee,// #LATIN SMALL LETTER I WITH CIRCUMFLEX
  0x00ec,// #LATIN SMALL LETTER I WITH GRAVE
  0x00c4,// #LATIN CAPITAL LETTER A WITH DIAERESIS
  0x00c5,// #LATIN CAPITAL LETTER A WITH RING ABOVE
  0x00c9,// #LATIN CAPITAL LETTER E WITH ACUTE
  0x00e6,// #LATIN SMALL LIGATURE AE
  0x00c6,// #LATIN CAPITAL LIGATURE AE
  0x00f4,// #LATIN SMALL LETTER O WITH CIRCUMFLEX
  0x00f6,// #LATIN SMALL LETTER O WITH DIAERESIS
  0x00f2,// #LATIN SMALL LETTER O WITH GRAVE
  0x00fb,// #LATIN SMALL LETTER U WITH CIRCUMFLEX
  0x00f9,// #LATIN SMALL LETTER U WITH GRAVE
  0x00ff,// #LATIN SMALL LETTER Y WITH DIAERESIS
  0x00d6,// #LATIN CAPITAL LETTER O WITH DIAERESIS
  0x00dc,// #LATIN CAPITAL LETTER U WITH DIAERESIS
  0x00a2,// #CENT SIGN
  0x00a3,// #POUND SIGN
  0x00a5,// #YEN SIGN
  0x20a7,// #PESETA SIGN
  0x0192,// #LATIN SMALL LETTER F WITH HOOK
  0x00e1,// #LATIN SMALL LETTER A WITH ACUTE
  0x00ed,// #LATIN SMALL LETTER I WITH ACUTE
  0x00f3,// #LATIN SMALL LETTER O WITH ACUTE
  0x00fa,// #LATIN SMALL LETTER U WITH ACUTE
  0x00f1,// #LATIN SMALL LETTER N WITH TILDE
  0x00d1,// #LATIN CAPITAL LETTER N WITH TILDE
  0x00aa,// #FEMININE ORDINAL INDICATOR
  0x00ba,// #MASCULINE ORDINAL INDICATOR
  0x00bf,// #INVERTED QUESTION MARK
  0x2310,// #REVERSED NOT SIGN
  0x00ac,// #NOT SIGN
  0x00bd,// #VULGAR FRACTION ONE HALF
  0x00bc,// #VULGAR FRACTION ONE QUARTER
  0x00a1,// #INVERTED EXCLAMATION MARK
  0x00ab,// #LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
  0x00bb,// #RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
  0x2591,// #LIGHT SHADE
  0x2592,// #MEDIUM SHADE
  0x2593,// #DARK SHADE
  0x2502,// #BOX DRAWINGS LIGHT VERTICAL
  0x2524,// #BOX DRAWINGS LIGHT VERTICAL AND LEFT
  0x2561,// #BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
  0x2562,// #BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
  0x2556,// #BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
  0x2555,// #BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
  0x2563,// #BOX DRAWINGS DOUBLE VERTICAL AND LEFT
  0x2551,// #BOX DRAWINGS DOUBLE VERTICAL
  0x2557,// #BOX DRAWINGS DOUBLE DOWN AND LEFT
  0x255d,// #BOX DRAWINGS DOUBLE UP AND LEFT
  0x255c,// #BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
  0x255b,// #BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
  0x2510,// #BOX DRAWINGS LIGHT DOWN AND LEFT
  0x2514,// #BOX DRAWINGS LIGHT UP AND RIGHT
  0x2534,// #BOX DRAWINGS LIGHT UP AND HORIZONTAL
  0x252c,// #BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
  0x251c,// #BOX DRAWINGS LIGHT VERTICAL AND RIGHT
  0x2500,// #BOX DRAWINGS LIGHT HORIZONTAL
  0x253c,// #BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
  0x255e,// #BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
  0x255f,// #BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
  0x255a,// #BOX DRAWINGS DOUBLE UP AND RIGHT
  0x2554,// #BOX DRAWINGS DOUBLE DOWN AND RIGHT
  0x2569,// #BOX DRAWINGS DOUBLE UP AND HORIZONTAL
  0x2566,// #BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
  0x2560,// #BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
  0x2550,// #BOX DRAWINGS DOUBLE HORIZONTAL
  0x256c,// #BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
  0x2567,// #BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
  0x2568,// #BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
  0x2564,// #BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
  0x2565,// #BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
  0x2559,// #BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
  0x2558,// #BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
  0x2552,// #BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
  0x2553,// #BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
  0x256b,// #BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
  0x256a,// #BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
  0x2518,// #BOX DRAWINGS LIGHT UP AND LEFT
  0x250c,// #BOX DRAWINGS LIGHT DOWN AND RIGHT
  0x2588,// #FULL BLOCK
  0x2584,// #LOWER HALF BLOCK
  0x258c,// #LEFT HALF BLOCK
  0x2590,// #RIGHT HALF BLOCK
  0x2580,// #UPPER HALF BLOCK
  0x03b1,// #GREEK SMALL LETTER ALPHA
  0x00df,// #LATIN SMALL LETTER SHARP S
  0x0393,// #GREEK CAPITAL LETTER GAMMA
  0x03c0,// #GREEK SMALL LETTER PI
  0x03a3,// #GREEK CAPITAL LETTER SIGMA
  0x03c3,// #GREEK SMALL LETTER SIGMA
  0x00b5,// #MICRO SIGN
  0x03c4,// #GREEK SMALL LETTER TAU
  0x03a6,// #GREEK CAPITAL LETTER PHI
  0x0398,// #GREEK CAPITAL LETTER THETA
  0x03a9,// #GREEK CAPITAL LETTER OMEGA
  0x03b4,// #GREEK SMALL LETTER DELTA
  0x221e,// #INFINITY
  0x03c6,// #GREEK SMALL LETTER PHI
  0x03b5,// #GREEK SMALL LETTER EPSILON
  0x2229,// #INTERSECTION
  0x2261,// #IDENTICAL TO
  0x00b1,// #PLUS-MINUS SIGN
  0x2265,// #GREATER-THAN OR EQUAL TO
  0x2264,// #LESS-THAN OR EQUAL TO
  0x2320,// #TOP HALF INTEGRAL
  0x2321,// #BOTTOM HALF INTEGRAL
  0x00f7,// #DIVISION SIGN
  0x2248,// #ALMOST EQUAL TO
  0x00b0,// #DEGREE SIGN
  0x2219,// #BULLET OPERATOR
  0x00b7,// #MIDDLE DOT
  0x221a,// #SQUARE ROOT
  0x207f,// #SUPERSCRIPT LATIN SMALL LETTER N
  0x00b2,// #SUPERSCRIPT TWO
  0x25a0,// #BLACK SQUARE
  0x00a0,// #NO-BREAK SPACE
  };

  public char map_cp850_unicode(char x) {
    if (x>=0x100)
      return x;
    return unimap[x];
  }

  private void _SetCursor(int row,int col) {
    int maxr = display.getRows();
    int tm = display.getTopMargin();

    R = (row<0)?0:row;
    C = (col<0)?0:col;

    if (!moveoutsidemargins) {
	R	+= display.getTopMargin();
	maxr	 = display.getBottomMargin();
    }
    if (R>maxr) R = maxr;
  }

  public void putChar(char c,boolean doshowcursor) {
    Dimension size;
    int  rows = display.getRows() ; //statusline
    int  columns = display.getColumns();
    int  tm = display.getTopMargin();
    int  bm = display.getBottomMargin();
    String  tosend;
    Vector  vec;
    byte  msg[];

    if (debug>4) System.out.println("putChar("+c+" ["+((int)c)+"]) at R="+R+" , C="+C+", columns="+columns+", rows="+rows);
    display.markLine(R,1);
    if (c>255) {
      if (debug>0)
        System.out.println("char > 255:"+((int)c));
      return;
    }
    switch (term_state) {
    case TSTATE_DATA:
      /* FIXME: we shouldn't use chars with bit 8 set if ibmcharset.
       * probably... but some BBS do anyway...
       */
      if (!useibmcharset) {
        boolean doneflag = true;
        switch (c) {
        case OSC:
          osc="";
          term_state = TSTATE_OSC;
          break;
        case RI:
          if (R>tm)
            R--;
          else
            display.insertLine(R,1,display.SCROLL_DOWN);
          if (debug>1)
            System.out.println("RI");
          break;
        case IND:
          if (R == tm - 1 || R == bm || R == rows - 1) //  Ray: not bottom margin - 1
            display.insertLine(R,1,display.SCROLL_UP);
          else
            R++;
          if (debug>1)
            System.out.println("IND (at "+R+" )");
          break;
        case NEL:
          if (R == tm - 1 || R == bm || R == rows - 1) //  Ray: not bottom margin - 1
            display.insertLine(R,1,display.SCROLL_UP);
          else
            R++;
          C=0;
          if (debug>1)
            System.out.println("NEL (at "+R+" )");
          break;
        case HTS:
          Tabs[C] = 1;
          if (debug>1)
            System.out.println("HTS");
          break;
        case DCS:
          dcs="";
          term_state = TSTATE_DCS;
          break;
        default:
          doneflag = false;
          break;
        }
        if (doneflag) break;
      }
      switch (c) {
      case CSI: // should be in the 8bit section, but some BBS use this
        term_state = TSTATE_DCEQ;
        break;
      case ESC:
        term_state = TSTATE_ESC;
        lastwaslf=0;
        break;
      case '\b':
        C--;
        if (C<0)
          C=0;
        lastwaslf = 0;
        break;
      case '\t':
        if (insertmode == 1) {
          int  nr,newc;

          newc = C;
          do {
            display.insertChar(C,R,' ',attributes);
            newc++;
          } while (newc<columns && Tabs[newc]==0);
        } else {
          do {
            display.putChar(C++,R,' ',attributes);
          } while (C<columns && (Tabs[C]==0));
        }
        lastwaslf = 0;
        break;
      case '\r':
        C=0;
        break;
      case '\n':
	if (debug>3)
		System.out.println("R= "+R+", bm "+bm+", tm="+tm+", rows="+rows);
        if (!vms)
        {
            if (lastwaslf!=0 && lastwaslf!=c)   //  Ray: I do not understand this logic.
              break;
            lastwaslf=c;
            /*C = 0;*/
        }
	// note: we do not scroll at the topmargin! only at the bottommargin
	// of the scrollregion and at the display bottom.
	if ( R == bm || R >= rows - 1)
	    display.insertLine(R,1);
	else
	    R++;
        break;
      case '\016':
        /* ^N, Shift out - Put G1 into GL */
        gl = 1;
        break;
      case '\017':
        /* ^O, Shift in - Put G0 into GL */
        gl = 0;
        break;
      default:
        lastwaslf=0;
        if (c<32) {
          if (c!=0)
            if (debug>0)
              System.out.println("TSTATE_DATA char: "+((int)c));
          break;
        }
        if(C >= columns) {
          if(R < rows - 1)
            R++;
          else
            display.insertLine(R,display.SCROLL_UP);
          C = 0;
        }

        // Mapping if DEC Special is chosen charset
        if ( gx[gl] == '0' ) {
          if ( c >= '\u005f' && c <= '\u007e' ) {
          if (debug>3)
            System.out.print("Mapping "+c+" (index "+((short)c-0x5f)+" to ");
          c = DECSPECIAL[(short)c - 0x5f];
          if (debug>3)
            System.out.println(c+" ("+(int)c+")");
          }
        }
/*
        if ( gx[gr] == '0' ) {
          if ( c >= '\u00bf' && c <= '\u00fe' ) {
          if (debug>2)
            System.out.print("Mapping "+c);
            c = DECSPECIAL[(short)c - 0xbf];
            if (debug>2)
              System.out.println("to "+c);
          }
        }
*/
        if (useibmcharset)
          c = map_cp850_unicode(c);

	/*if(true || (statusmode == 0)) { */
	if (debug>4) System.out.println("output "+c+" at "+C+","+R);
	  if (insertmode==1) {
	    display.insertChar(C, R, c, attributes);
	  } else {
	    display.putChar(C, R, c, attributes);
	  }
	/*
	} else {
	  if (insertmode==1) {
	    display.insertChar(C, rows, c, attributes);
	  } else {
	    display.putChar(C, rows, c, attributes);
	  }
	}
	*/
        C++;
        break;
      } /* switch(c) */
      break;
    case TSTATE_OSC:
      if ((c<0x20) && (c!=ESC)) {// NP - No printing character
        handle_osc(osc);
        term_state = TSTATE_DATA;
        break;
      }
      //but check for vt102 ESC \
      if (c=='\\' && osc.charAt(osc.length()-1)==ESC) {
        handle_osc(osc);
        term_state = TSTATE_DATA;
        break;
      }
      osc = osc + c;
      break;
    case TSTATE_ESC:
      switch (c) {
      case '#':
        term_state = TSTATE_ESCSQUARE;
        break;
      case 'c':
        /* Hard terminal reset */
        /*FIXME:*/
        term_state = TSTATE_DATA;
        break;
      case '[':
        term_state = TSTATE_CSI;
        DCEvar    = 0;
        DCEvars[0]  = 0;
        DCEvars[1]  = 0;
        DCEvars[2]  = 0;
        DCEvars[3]  = 0;
        break;
        case ']':
        osc="";
        term_state = TSTATE_OSC;
        break;
      case 'P':
        dcs="";
        term_state = TSTATE_DCS;
        break;
      case 'E':
        if (R == tm - 1 || R == bm || R == rows - 1) //  Ray: not bottom margin - 1
          display.insertLine(R,1,display.SCROLL_UP);
        else
          R++;
        C=0;
        if (debug>1)
          System.out.println("ESC E (at "+R+")");
        term_state = TSTATE_DATA;
        break;
      case 'D':
        if (R == tm - 1 || R == bm || R == rows - 1)
          display.insertLine(R,1,display.SCROLL_UP);
        else
          R++;
        if (debug>1)
          System.out.println("ESC D (at "+R+" )");
        term_state = TSTATE_DATA;
        break;
      case 'M': // IL
        if ((R>=tm) && (R<=bm)) // in scrolregion
          display.insertLine(R,1,display.SCROLL_DOWN);
	/* else do nothing ; */
        if (debug>1)
          System.out.println("ESC M ");
        term_state = TSTATE_DATA;
        break;
      case 'H':
        if (debug>1)
          System.out.println("ESC H at "+C);
        /* right border probably ...*/
        if (C>=columns)
          C=columns-1;
        Tabs[C] = 1;
        term_state = TSTATE_DATA;
        break;
      case '=':
        /*application keypad*/
        if (debug>0)
          System.out.println("ESC =");
        term_state = TSTATE_DATA;
        break;
      case '>':
        /*normal keypad*/
        if (debug>0)
          System.out.println("ESC >");
        term_state = TSTATE_DATA;
        break;
      case '7':
        /*save cursor */
        Sc = C;
        Sr = R;
        Sa = attributes;
        if (debug>1)
          System.out.println("ESC 7");
        term_state = TSTATE_DATA;
        break;
      case '8':
        /*restore cursor */
        C = Sc;
        R = Sr;
        attributes = Sa;
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC 7");
        break;
      case '(':
        /* Designate G0 Character set (ISO 2022) */
        term_state = TSTATE_SETG0;
        break;
      case ')':
        /* Designate G0 character set (ISO 2022) */
        term_state = TSTATE_SETG1;
        break;
      case '*':
        /* Designate G1 Character set (ISO 2022) */
        term_state = TSTATE_SETG2;
        break;
      case '+':
        /* Designate G1 Character set (ISO 2022) */
        term_state = TSTATE_SETG3;
        break;
      case '~':
        /* Locking Shift 1, right */
        term_state = TSTATE_DATA;
        gr = 1;
        break;
      case 'n':
        /* Locking Shift 2 */
        term_state = TSTATE_DATA;
        gl = 2;
        break;
      case '}':
        /* Locking Shift 2, right */
        term_state = TSTATE_DATA;
        gr = 2;
        break;
      case 'o':
        /* Locking Shift 3 */
        term_state = TSTATE_DATA;
        gl = 3;
        break;
      case '|':
        /* Locking Shift 3, right */
        term_state = TSTATE_DATA;
        gr = 3;
        break;
      default:
        System.out.println("ESC unknown letter: ("+((int)c)+")");
        term_state = TSTATE_DATA;
        break;
      }
      break;
    case TSTATE_SETG0:
      if(c!='0' && c!='A' && c!='B')
        System.out.println("ESC ( : G0 char set?  ("+((int)c)+")");
      else {
        if (debug>2) System.out.println("ESC ( : G0 char set  ("+c+" "+((int)c)+")");
        gx[0] = c;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_SETG1:
      if(c!='0' && c!='A' && c!='B')
        System.out.println("ESC ) :G1 char set?  ("+((int)c)+")");
      else {
        if (debug>2) System.out.println("ESC ) :G1 char set  ("+c+" "+((int)c)+")");
        gx[1] = c;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_SETG2:
      if(c!='0' && c!='A' && c!='B')
        System.out.println("ESC*:G2 char set?  ("+((int)c)+")");
      else {
        if (debug>2) System.out.println("ESC*:G2 char set  ("+c+" "+((int)c)+")");
	gx[2] = c;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_SETG3:
      if(c!='0' && c!='A' && c!='B')
        System.out.println("ESC+:G3 char set?  ("+((int)c)+")");
      else {
        if (debug>2) System.out.println("ESC+:G3 char set  ("+c+" "+((int)c)+")");
        gx[3] = c;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_ESCSQUARE:
      switch (c) {
      case '8':
        for (int i=0;i<columns;i++)
          for (int j=0;j<rows;j++)
            display.putChar(i,j,'E',0);
        break;
      default:
        System.out.println("ESC # "+c+" not supported.");
        break;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_DCS:
      if (c=='\\' && dcs.charAt(dcs.length()-1)==ESC) {
        handle_dcs(dcs);
        term_state = TSTATE_DATA;
        break;
      }
      dcs = dcs + c;
      break;
    case TSTATE_DCEQ:
      switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        DCEvars[DCEvar]=DCEvars[DCEvar]*10+((int)c)-48;
        break;
      case 'r': // XTERM_RESTORE
        if (true || debug>1)
          System.out.println("ESC [ ? "+DCEvars[0]+" r");
        /* DEC Mode reset */
        switch (DCEvars[0]){
        case 3: /* 80 columns*/
          size = display.size();
          display.setWindowSize(80,rows);
          layout();
          break;
        case 4: /* scrolling mode, smooth */
          break;
        case 5: /* light background */
          break;
        case 6: /* move inside margins ? */
          moveoutsidemargins = true;
          break;
        case 12:/* local echo off */
          break;
        }
        term_state = TSTATE_DATA;
        break;
      case 'h': // DECSET
        if (debug>0)
          System.out.println("ESC [ ? "+DCEvars[0]+" h");
        /* DEC Mode set */
        switch (DCEvars[0]){
        case 1:  /* Application cursor keys */
          KeyUp  = "\u001bOA";
          KeyDown  = "\u001bOB";
          KeyRight= "\u001bOC";
          KeyLeft  = "\u001bOD";
          break;
        case 3: /* 132 columns*/
          size = display.size();
          display.setWindowSize(132,rows);
          layout();
          break;
        case 4: /* scrolling mode, smooth */
          break;
        case 5: /* light background */
          break;
        case 6: /* move inside margins ? */
          moveoutsidemargins = false;
          break;
        case 12:/* local echo off */
          break;
        }
        term_state = TSTATE_DATA;
        break;
      case 'l':	//DECRST
        /* DEC Mode reset */
        if (debug>0)
          System.out.println("ESC [ ? "+DCEvars[0]+" l");
        switch (DCEvars[0]){
        case 1:  /* Application cursor keys */
          KeyUp  = "\u001b[A";
          KeyDown  = "\u001b[B";
          KeyRight= "\u001b[C";
          KeyLeft  = "\u001b[D";
          break;
        case 3: /* 80 columns*/
          size = display.size();
          display.setWindowSize(80,rows);
          layout();
          break;
        case 4: /* scrolling mode, jump */
          break;
        case 5: /* dark background */
          break;
        case 6: /* move outside margins ? */
          moveoutsidemargins = true;
          break;
        case 12:/* local echo on */
          break;
        }
        term_state = TSTATE_DATA;
        break;
      case ';':
        DCEvar++;
        DCEvars[DCEvar] = 0;
        break;
      case 'n':
        if (debug>0)
          System.out.println("ESC [ ? "+DCEvars[0]+" n");
        switch (DCEvars[0]) {
        case 15:
          /* printer? no printer. */
          host.send(((char)ESC)+"[?13n");
          System.out.println("ESC[5n");
          break;
        default:break;
        }
        term_state = TSTATE_DATA;
        break;
      default:
        if (debug>0)
          System.out.println("ESC [ ? "+DCEvars[0]+" "+c);
        term_state = TSTATE_DATA;
        break;
      }
      break;
    case TSTATE_CSI_DOLLAR:
      switch (c) {
      case '}':
	System.out.println("Active Status Display now "+DCEvars[0]);
	statusmode = DCEvars[0];
	break;
/* bad documentation?
      case '-':
	System.out.println("Set Status Display now "+DCEvars[0]);
	break;
 */
      case '~':
	System.out.println("Status Line mode now "+DCEvars[0]);
	break;
      default:
	System.out.println("UNKNOWN Status Display code "+c+", with Pn="+DCEvars[0]);
	break;
      }
      term_state = TSTATE_DATA;
      break;
    case TSTATE_CSI:
      switch (c) {
      case '$':
	term_state=TSTATE_CSI_DOLLAR;
	break;
      case '?':
        DCEvar=0;
        DCEvars[0]=0;
        term_state=TSTATE_DCEQ;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        DCEvars[DCEvar]=DCEvars[DCEvar]*10+((int)c)-48;
        break;
      case ';':
        DCEvar++;
        DCEvars[DCEvar] = 0;
        break;
      case 'c':/* send primary device attributes */
        /* send (ESC[?61c) */
        host.send(((char)ESC)+"[?1;2c");
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" c");
        break;
      case 'q':
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" q");
        term_state = TSTATE_DATA;
        break;
      case 'g':
        /* used for tabsets */
        switch (DCEvars[0]){
        case 3:/* clear them */
          int nw = display.getColumns();
          Tabs = new byte[nw];
          break;
        case 0:
          Tabs[C] = 0;
          break;
        }
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" g");
        term_state = TSTATE_DATA;
        break;
      case 'h':
        switch (DCEvars[0]) {
        case 4:
          insertmode = 1;
          break;
        case 20:
          sendcrlf = true;
          break;
        default:
          System.out.println("unsupported: ESC [ "+DCEvars[0]+" h");
          break;
        }
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" h");
        break;
      case 'l':
        switch (DCEvars[0]) {
        case 4:
          insertmode = 0;
          break;
        case 20:
          sendcrlf = false;
          break;
        }
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" l");
        break;
      case 'A': // CUU
      {
        int limit;
	/* FIXME: xterm only cares about 0 and topmargin */
        if (R > bm)
            limit = bm+1;
        else if (R >= tm) {
            limit = tm;
        } else
            limit = 0;
        if (DCEvars[0]==0)
          R--;
        else
          R-=DCEvars[0];
        if (R < limit)
            R = limit;
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" A");
        break;
      }
      case 'B':	// CUD
        /* cursor down n (1) times */
      {
        int limit;
        if (R < tm)
            limit = tm-1;
        else if (R <= bm) {
            limit = bm;
        } else
            limit = rows - 1;
        if (DCEvars[0]==0)
          R++;
        else
          R+=DCEvars[0];
        if (R > limit)
            R = limit;
        else {
            if (debug>2) System.out.println("Not limited.");
        }
        if (debug>2) System.out.println("to: " + R);
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" B (at C="+C+")");
        break;
      }
      case 'C':
        if (DCEvars[0]==0)
          C++;
        else
          C+=DCEvars[0];
        if (C>columns-1)
          C=columns-1;
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" C");
        break;
      case 'd': // CVA
	R = DCEvars[0];
        System.out.println("ESC [ "+DCEvars[0]+" d");
        term_state = TSTATE_DATA;
	break;
      case 'D':
        if (DCEvars[0]==0)
          C--;
        else
          C-=DCEvars[0];
        if (C<0) C=0;
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" D");
        break;
      case 'r': // DECSTBM
        if (DCEvar>0)   //  Ray:  Any argument is optional
        {
          R = DCEvars[1]-1;
          if (R < 0)
            R = rows-1;
          else if (R >= rows) {
            R = rows - 1;
 	  }
        } else
          R = rows - 1;
        display.setBottomMargin(DCEvars[1]-1);
        if (R >= DCEvars[0])
        {
          R = DCEvars[0]-1;
          if (R < 0)
            R = 0;
        }
        display.setTopMargin(DCEvars[0]-1);
	_SetCursor(0,0);
        if (debug>1)
          System.out.println("ESC ["+DCEvars[0]+" ; "+DCEvars[1]+" r");
        term_state = TSTATE_DATA;
        break;
      case 'G':  /* CUP  / cursor absolute column */
	C = DCEvars[0];
	System.out.println("ESC [ "+DCEvars[0]+" G");
        term_state = TSTATE_DATA;
	break;
      case 'H':  /* CUP  / cursor position */
        /* gets 2 arguments */
	_SetCursor(DCEvars[0]-1,DCEvars[1]-1);
        term_state = TSTATE_DATA;
        if (debug>2) {
          System.out.println("ESC [ "+DCEvars[0]+";"+DCEvars[1]+" H, moveoutsidemargins "+moveoutsidemargins);
          System.out.println("	-> R now "+R+", C now "+C);
	}
        break;
      case 'f':  /* move cursor 2 */
        /* gets 2 arguments */
        R = DCEvars[0]-1;
        C = DCEvars[1]-1;
        if (C<0) C=0;
        if (R<0) R=0;
        term_state = TSTATE_DATA;
        if (debug>2)
          System.out.println("ESC [ "+DCEvars[0]+";"+DCEvars[1]+" f");
        break;
      case 'L':
        /* insert n lines */
        if (DCEvars[0]==0)
          display.insertLine(R,display.SCROLL_DOWN);
        else
          display.insertLine(R,DCEvars[0],display.SCROLL_DOWN);
        term_state = TSTATE_DATA;
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" L (at R "+R+")");
        break;
      case 'M':
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+"M at R="+R);
        if (DCEvars[0]==0)
          display.deleteLine(R);
        else
          for (int i=0;i<DCEvars[0];i++)
            display.deleteLine(R);
        term_state = TSTATE_DATA;
        break;
      case 'K':
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" K");
        /* clear in line */
        switch (DCEvars[0]) {
        case 0:/*clear to right*/
          if (C<columns-1)
            display.deleteArea(C,R,columns-C,1);
          break;
        case 1:/*clear to the left*/
          if (C>0)
            display.deleteArea(0,R,C,1);    // Ray: Should at least include character before this one, not C-1
          break;
        case 2:/*clear whole line */
          display.deleteArea(0,R,columns,1);
          break;
        }
        term_state = TSTATE_DATA;
        break;
      case 'J':
        /* clear display.below current line */
        switch (DCEvars[0]) {
        case 0:
          if (R<rows-1)
            display.deleteArea(0,R + 1,columns,rows-R-1);
          if (C<columns-1)
            display.deleteArea(C,R,columns-C,1);
          break;
        case 1:
          if (R>0)
            display.deleteArea(0,0,columns,R-1);
          if (C>0)
            display.deleteArea(0,R,C,1);    // Ray: Should at least include character before this one, not C-1
          break;
        case 2:
          display.deleteArea(0,0,columns,rows);
          break;
        }
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" J");
        term_state = TSTATE_DATA;
        break;
      case '@':
	if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" @");
	for (int i=0;i<DCEvars[0];i++)
	  display.insertChar(C,R,' ',attributes);
	term_state = TSTATE_DATA;
	break;
      case 'P':
        if (debug>1)
          System.out.println("ESC [ "+DCEvars[0]+" P, C="+C+",R="+R);
	if (DCEvars[0]==0) DCEvars[0]=1;
	for (int i=0;i<DCEvars[0];i++)
          display.deleteChar(C,R);
        term_state = TSTATE_DATA;
        break;
      case 'n':
        switch (DCEvars[0]){
        case 5: /* malfunction? No malfunction. */
          host.send(((char)ESC)+"[0n");
          if(debug > 1)
            System.out.println("ESC[5n");
          break;
        case 6:
          host.send(((char)ESC)+"["+R+";"+C+"R");
          if(debug > 1)
            System.out.println("ESC[6n");
          break;
        default:
          if (debug>0)
            System.out.println("ESC [ "+DCEvars[0]+" n??");
          break;
        }
        term_state = TSTATE_DATA;
        break;
      case 'm':  /* attributes as color, bold , blink,*/
        if (debug>3)
          System.out.print("ESC [ ");
        if (DCEvar == 0 && DCEvars[0] == 0)
          attributes = 0;
        for (i=0;i<=DCEvar;i++) {
          switch (DCEvars[i]) {
          case 0:
            if (DCEvar>0)
              attributes =0;
            break;
          case 4:
            attributes |= CharDisplay.UNDERLINE;
            break;
          case 1:
            attributes |= CharDisplay.BOLD;
            break;
          case 7:
            attributes |= CharDisplay.INVERT;
            break;
          case 5: /* blink on */
            break;
          case 25: /* blinking off */
            break;
          case 27:
            attributes &= ~CharDisplay.INVERT;
            break;
          case 24:
            attributes &= ~CharDisplay.UNDERLINE;
            break;
          case 22:
            attributes &= ~CharDisplay.BOLD;
            break;
          case 30:
          case 31:
          case 32:
          case 33:
          case 34:
          case 35:
          case 36:
          case 37:
            attributes &= ~(0xf<<3);
            attributes |= ((DCEvars[i]-30)+1)<<3;
            break;
          case 39:
            attributes &= ~(0xf<<3);
            break;
          case 40:
          case 41:
          case 42:
          case 43:
          case 44:
          case 45:
          case 46:
          case 47:
            attributes &= ~(0xf<<7);
            attributes |= ((DCEvars[i]-40)+1)<<7;
            break;
          case 49:
            attributes &= ~(0xf<<7);
            break;

          default:
            System.out.println("ESC [ "+DCEvars[i]+" m unknown...");
            break;
          }
          if (debug>3)
            System.out.print(""+DCEvars[i]+";");
        }
        if (debug>3)
          System.out.print(" (attributes = "+attributes+")m \n");
        term_state = TSTATE_DATA;
        break;
      default:
        if (debug>0)
          System.out.println("ESC [ unknown letter:"+c+" ("+((int)c)+")");
        term_state = TSTATE_DATA;
        break;
      }
      break;
    default:
      term_state = TSTATE_DATA;
      break;
    }
    if (C > columns) C = columns;
    if (R > rows)  R = rows;
    if (C < 0)  C = 0;
    if (R < 0)  R = 0;
    if (doshowcursor)
      display.setCursorPos(C, R);
    display.markLine(R,1);
  }
}
