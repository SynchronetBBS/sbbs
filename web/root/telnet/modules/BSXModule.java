/**
 * BSXModule -- implements BSX controll sequence handling
 * --
 * $Id$
 * $timestamp: Tue Oct 14 18:00:00 1997 by Thomas Kriegelstein :$
 *
 * This file and the related package may be part of "The Java Telnet
 * Applet" by Matthias L. Jugel.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * "The BSXModule" is distributed in the hope that it will be 
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

import modules.bsx.*;
import java.applet.Applet;
import java.awt.*;
import java.io.*;
import java.util.*;
/**
 * The ultimate BSX module implements most of the common used
 * BSX controll sequences.
 * <P>
 * Features:<UL>
 * <LI>a Statemachine to parse the Strings passed by the telnet
 * <LI>a own package named bsx, for Window and Polygon handling
 * terrible english because my native language is german
 * </UL>
 * TODO:<UL>
 * <LI>more BSX Sequences
 * <LI>Documentaion
 * </UL>
 * 
 * @author  Thomas Kriegelstein
 */

public class BSXModule extends Panel implements Module 
{
  /** Client Version */
  protected String clientVersion = "Foob 0.5";
  /** The Frame, where the graphic is drawn */
  private BSXDisplay bsxWindow;
  /** Our parent, the telnet app. */
  private telnet parent;
  /** the Container, wich is around us */
  private Container toplevel;
  /** the Buttons to switch on and off the logging and BSX handling */
  private Button bsxButton,logButton;
  /** the states of the Buttons */
  private boolean bsxCheckIt,logging;
  /** register o as our parent */ 
  public void setLoader(Object o) { parent = (telnet)o; }
  /** do nothing */
  public void connect(String host, int port) {}
  /** do nothing */
  public void disconnect() {}
  /**
   * This method is called by the telnet, so that we can parse the String.
   * Prints out the filtered String if logging s enabled.
   * @param String s the String to be parsed
   * @return a filtered String with no BSX sequences in it
   */
  public String receive(String s) 
    {
      String res;
      if (!bsxCheckIt) res=s;
      else res=parseString(s);
      if (logging)
	System.out.println(res);
      return res;
    }
  /**
   * Adds two Buttons to the Panel and instanciates a BSXWindow.
   */   
  public void addNotify() 
    {
      this.setLayout(new GridLayout(1,0));
      // Enable BSX Parsing
      if (bsxWindow==null)
	{
	  this.add(bsxButton=new Button("BSX aus!"));
	  bsxCheckIt=true;
	  this.add(logButton=new Button("Logging an!"));
	  logging=false;
	  bsxWindow = new BSXDisplay();
	}
      //
      super.addNotify();
    }
  /**
   * Java 1.0 eventhandling routines.
   * @param Event e
   * @return true if Event has been recognized and fullfilled.
   */
  public boolean handleEvent(Event e)
    {
      if ((e.id==Event.ACTION_EVENT)&&(e.target==bsxButton)) 
	{
	  bsxCheckIt=!bsxCheckIt;
	  if (bsxCheckIt) 
	    {
	      parent.writeToSocket("#VER "+clientVersion+"\r\n");
	      bsxButton.setLabel("BSX aus!");
	      bsxWindow.show();
	    }
	  else 
	    {
	      parent.writeToSocket("#VER\r\n");
	      bsxButton.setLabel("BSX an!");
	      bsxWindow.hide();
	      if (debug>1)
		{
		  parent.writeToUser(lastCommand+strIdentifier+BSXData);
		  System.out.println("BSX off: last Input -");
		  System.out.println(lastCommand+strIdentifier+BSXData);
		}
	    }
	  resetMachine();  
	  return true;
	}
      if ((e.id==Event.ACTION_EVENT)&&(e.target==logButton))
	{
	  logging=!logging;
	  if (logging)
	    {
	      logButton.setLabel("Logging aus!");
	    }
	  else
	    {
	      logButton.setLabel("Logging an!");
	    }
	  return true;
	}
      return super.handleEvent(e);
    }

  /** 
   * And here we go again * latest implementation of my StateMachine
   * all methods that return a String, return the String passed to them
   * but without the chars they have recognized and solved
   * the real results are contained in the (global) variables
   */

  /**
   * my Debuglevel, because this software is still beta.
   */
  private static int debug = 0;
  /**
   * a main for test-purposes
   */
  public static void main (String args[])
  {
    BSXModule bsxm=new BSXModule();
    debug=1;
    String res;
    res=bsxm.parseString("Testing@Hawk@RQV@SCEintro.@VIO/magier:foobar.@RFS@RMO/magier:foo");
    res+=bsxm.parseString("bar.@RFSThis is a test.@TMS@PRO@RFS@BOMTest me Carefully.");
    System.out.println("Gefiltert: "+res);
  }
  /**
   * lastCommand is a Statedescriptor.<BR>
   * if lastCommand.length()=4 then the entire Command has been read
   * else go on reading the command completly<BR>
   * Commands are: PRO SCE VIO DFS DFO TMS RMO RFS RQV PUR
   * additional commands are: BOM EOM LON LOF TXT which will not be supported 
   * by Regenbogen BSX java Client.<BR>
   * Commands are preceeded by a @
   */
  private String lastCommand="";
  /**
   * lastState identifies what we have done last time we received a string
   * <UL>
   * <LI>-1 undefined -> used to mark succesful work
   * <LI> 0 nothing -> go on searching for a new command
   * <LI> 1 identifier -> we have to complete the identifier
   * <LI> 2 download -> we have to get more BSXData
   * </UL>
   */
  private int lastState;
  /** holds the identifier of the current object */
  private String strIdentifier="";
  /** holds the X position of the current object as a String*/
  private String strXPos="";
  /** holds the X position of the current object as an int */
  private int intXPos=0;
  /** holds the Y position of the current object as a String*/
  private String strYPos="";
  /** holds the Y position of the current object as an int */
  private int intYPos=0;
  /** holds the number of polygons in the current description as a String */
  private String strPolys="";
  /** holds the number of polygons in the current description as an int */
  private int intPolys=0;
  /** holds the number of edges in the current polygon as a String */
  private String strEdges="";
  /** holds the number of edges in the current polygon as an int */
  private int intEdges=0;
  /** holds the BSX description, including the numbers of polygons and edges */
  private String BSXData="";
  /** 
   * reset machine to an initial status
   */ 
  private void resetMachine()
  {
    if (debug>0)
    {
      System.err.println("BSXStateMachine statustrace:");
      System.err.println("Command:    "+lastCommand);
      System.err.println("State:      "+lastState);
      System.err.println("Identifier: "+strIdentifier);
    }
    lastCommand="";
    lastState=0;
    strIdentifier="";
    strXPos="";
    intXPos=0;
    strYPos="";
    intYPos=0;
    strPolys="";
    intPolys=0;
    strEdges="";
    intEdges=0;
    BSXData="";
  }
  /**
   * convert a Hexnumber contained in a 2byteString into int
   * @param String s
   * @return the converted int or 0 if not succesful 
   */
  private int asciiHexToInt(String s)
  {
    if (s.length()==2)
    {
      try
      {
        int h=(int)s.charAt(0);
        if (h>='A') h-='A'-10; else h-='0';
        int l=(int)s.charAt(1);
        if (l>='A') l-='A'-10; else l-='0';
	if (debug>9) warn("Converting Hex: "+s+" to int: "+(h*16+l));
        return h*16+l;
      }
      catch (Exception e)
      {
        return 0;
      }
    }
    return 0;
  }
  /**
   * The method called to start the parsing
   * @param String s
   * @return filtered String
   */
  private String parseString(String s)
  {
    if ((s.indexOf("@")==-1)&&(lastState==0))
    {
      if (debug>0) warn("parseString done");
      return s;
    }
    if (lastState!=0)
    {
      if (debug>0) warn("parseString pending Input");
      s=decodeCommand(s);
    }
    int i=0;
    int pos=s.indexOf("@",i);
    String res="";
    while ((pos!=-1)&&(pos<s.length()))
    {
      if (debug>0) warn("parseString Command at "+pos);
      res+=s.substring(0,pos);
      if (debug>0) warn("still to parse: "+s.substring(pos));
      String tmp=decodeCommand(s.substring(pos));
      if (tmp.equals(s.substring(pos))) i++;
      else i=0;
      s=tmp;
      pos=s.indexOf("@",i); // search for next Command
    }
    res+=s;
    return res;
  }
  /**
   * simple messaging for debug purposes
   * @param String s to write to stderr
   */
  private void warn(String s) { System.err.println("Warning: "+s+"."); }
  /**
   * read the identifier into strIdentifier
   * @param String s to be parsed
   * @returns String without the first identifier
   */
  private String readIdentifier(String s)
  {
    if (strIdentifier.equals("")) lastState=1;
    if (lastState==1)
    {
      int i=s.indexOf(".");
      if (i==-1)
      {
        strIdentifier+=s;
        return "";
      }
      else
      {
        lastState=-1;
        strIdentifier+=s.substring(0,i);
        return s.substring(i+1);
      }
    }
    return s;
  }
  /**
   * read the BSX-Descriptions into BSXData
   * @param String s to be parsed
   * @return String without the first BSX Data sequence
   */
  private String readBSXData(String s)
  {
    if (strPolys.equals("")) lastState=2;
    if (lastState==2)
    {
      while((strPolys.length()<2)&&(s.length()>0))
      {
        BSXData+=s.charAt(0);
        strPolys+=s.charAt(0); s=s.substring(1);
      }
      if (strPolys.length()<2) return "";
      else if (debug>1) warn("PolyCount read:"+strPolys);
      if (intPolys == 0) 
	intPolys=asciiHexToInt(strPolys);
      if (debug>9) warn("equals "+intPolys);
      while ((s.length()>0)&&(intPolys>0))
      {
        while((strEdges.length()<2)&&(s.length()>0))
        {
          BSXData+=s.charAt(0);
          strEdges+=s.charAt(0); s=s.substring(1);
        }
        if (strEdges.length()<2) return "";
	else if (debug>1) warn("EdgeCount read:"+strEdges);
	if (intEdges == 0)
	  intEdges=(asciiHexToInt(strEdges)*2+1)*2;
        // color plus every edge x,y times 2 is the number of chars to read.
        while((s.length()>0)&&(intEdges>0))
        {
          BSXData+=s.charAt(0); s=s.substring(1);
          intEdges--;
        }
        if (intEdges>0) return "";
        strEdges=""; // reset the number of Edges!
        intPolys--;
      }
      if (intPolys>0) return "";
      if (intPolys==0) strPolys="";
      lastState=-1;
    }
    return s;
  }
  /**
   * read the Command, catch all necessary data, and execute it
   * @param String s to be parsed
   * @return String without first command
   */
  private String decodeCommand(String s)
  {
    while ((lastCommand.length()!=4)&&(s.length()>0))
    {
      lastCommand+=s.charAt(0); s=s.substring(1);
    }
    if (lastCommand.length()!=4)
    {
      lastState=-1;
      return "";
    }
    if (false) {} // hehe, just to write everytime "else if"
    // ReFresh Scene
    else if (lastCommand.equals("@RFS"))
      {
        resetMachine();
        if (bsxWindow!=null) bsxWindow.repaint();
        else warn("No BSX Output Available");
      }
    // VIsualize Object
    else if (lastCommand.equals("@VIO"))
      {
        s=readIdentifier(s);
        if (lastState==1) return "";
        while ((strXPos.length()<2)&&(s.length()>0))
        {
          strXPos+=s.charAt(0); s=s.substring(1);
        }
        if (strXPos.length()!=2)
        {
          return "";
        }
        while ((strYPos.length()<2)&&(s.length()>0))
        {
          strYPos+=s.charAt(0); s=s.substring(1);
        }
        if (strYPos.length()!=2)
        {
          return "";
        }
	intXPos=asciiHexToInt(strXPos);
	intYPos=asciiHexToInt(strYPos);
        if (bsxWindow!=null)
          if (!bsxWindow.showObject(strIdentifier,intXPos,intYPos))
            if (parent!=null)
              parent.writeToSocket("#RQO "+strIdentifier+"\r\n");
            else warn("no Socket to send to");
          else
            if (debug>0)
              System.out.println("Query BSX-Description for "+strIdentifier);
            else ;
        else warn("no BSX Output available");
        resetMachine();
      }
    // ReMove Object
    else if (lastCommand.equals("@RMO"))
      {
        s=readIdentifier(s);
        if (lastState==1) return "";
        if (bsxWindow!=null) bsxWindow.removeObject(strIdentifier);
        else warn("no BSX Output available");
        resetMachine();
      }
    // ReQuest Version
    else if (lastCommand.equals("@RQV"))
      {
        if (parent!=null) parent.writeToSocket("#VER "+clientVersion+"\r\n");
        else warn("no Socket to send to");
        resetMachine();
      }
    // PROmotion
    else if (lastCommand.equals("@PRO"))
      {
        if (parent!=null)
          parent.writeToUser(
"****************************************************************************\r\n"+
"*                                                                          *\r\n"+
"*  Regenbogen BSX - Ein MultiUser Dungeon mit Grafikunterstützung          *\r\n"+
"*  Adresse: rb.mud.de Port: 4711 Admins: mud@rb.mud.de                     *\r\n"+
"*  Hier ist was los Leute! Kommt. Lest. Schaut. Spielt. Und Redet.         *\r\n"+
"*                                                                          *\r\n"+
"* Client: von Foobar basierend auf Arbeiten von Riwa und Hate@Morgengrauen *\r\n"+
"*                                                                          *\r\n"+
"*****************************************************************Java*rulez*\r\n"
                            );
        else warn("no User to display promotion available");
        resetMachine();
      }
    // display SCEne
    else if (lastCommand.equals("@SCE"))
      {
        s=readIdentifier(s);
        if (lastState==1) return "";
        if (bsxWindow!=null)
          if (!bsxWindow.showScene(strIdentifier))
            if (parent!=null)
              parent.writeToSocket("#RQS "+strIdentifier+"\r\n");
            else warn("no Socket to send to");
          else
            if (debug>0)
              System.out.println("Query BSX-Scene for "+strIdentifier);
            else ;
        else warn("no BSX Output available");
        resetMachine();
      }
    // DeFine Object
    else if (lastCommand.equals("@DFO"))
      {
        s=readIdentifier(s);
        if (lastState==1) return "";
        s=readBSXData(s);
        if (lastState==2) return "";
        BSXInputStream bis=new BSXInputStream(new StringBufferInputStream(BSXData));
        if (bsxWindow!=null)
          try {
            bsxWindow.addObject(strIdentifier,bis.readBSXGraphic());
          } catch (IOException e) { e.printStackTrace(); resetMachine(); return s; }
        else warn("no BSX Output available");
        resetMachine();
      }
    else if (lastCommand.equals("@DFS"))
      {
        s=readIdentifier(s);
        if (lastState==1) return "";
        s=readBSXData(s);
        if (lastState==2) return "";
        BSXInputStream bis=new BSXInputStream(new StringBufferInputStream(BSXData));
        if (bsxWindow!=null)
          try {
            bsxWindow.addScene(strIdentifier,bis.readBSXGraphic());
          } catch (IOException e) { e.printStackTrace(); resetMachine(); return s; }
        else warn("no BSX Output available");
        resetMachine();
      }
    else if (lastCommand.equals("@PUR"))
      {
        warn("@PUR not implemented yet");
        resetMachine();
      }
    else if (lastCommand.equals("@TXT"))
      {
        warn("@TXT not implemented (and no plan for doing that)");
        resetMachine();
      }
    else if (lastCommand.equals("@BOM"))
      {
        warn("@BOM not implemented (and no plan for doing that)");
        resetMachine();
      }
    else if (lastCommand.equals("@EOM"))
      {
        warn("@EOM not implemented (and no plan for doing that)");
        resetMachine();
      }
    else if (lastCommand.equals("@LON"))
      {
        warn("@LON not implemented yet");
        resetMachine();
      }
    else if (lastCommand.equals("@LOF"))
      {
        warn("@LOF not implemented yet");
        resetMachine();
      }
    else if (lastCommand.equals("@TMS"))
      {
        if (parent!=null)
        {
          parent.writeToUser("Received @TMS to end this session.\r\n");
        }
        else
          warn("@TMS received! Session not terminated. Close connection manually.");
        resetMachine();
      }
    s=lastCommand+s;
    resetMachine();
    return s;
  }
}



