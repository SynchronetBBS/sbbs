/**
 * CharDisplayTest
 * --
 * $Id$
 * $timestamp: Mon Feb 17 20:11:20 1997 by Matthias L. Jugel :$
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
import java.awt.Button;
import java.awt.Panel;
import java.awt.Event;
import java.awt.FlowLayout;
import java.awt.BorderLayout;
import java.awt.Choice;
import java.awt.TextField;
import java.awt.Font;

import display.CharDisplay;

/**
 * CharDisplayTest -- a test applet to show the display/CharDisplay features
 * --
 * @version	$Id$
 * @author	Matthias L. Jugel, Marcus Meiﬂner
 */
public class CharDisplayTest extends Applet
{
  CharDisplay display = new CharDisplay(80, 24, "Courier", 14);

  Panel buttons = new Panel();
  Button info = new Button("Information");
  Button chars = new Button("Character Table");
  Button attr = new Button("Attributes");
  Choice fonts = new Choice();
  TextField from = new TextField("0", 4);

  public void init()
  {
    setLayout(new BorderLayout());
    fonts.addItem("Helvetica");
    fonts.addItem("TimesRoman");
    fonts.addItem("Courier");
    fonts.addItem("Dialog");
    fonts.addItem("DialogInput");
    fonts.addItem("ZapfDingBats");
    fonts.addItem("default");
    buttons.add(info);
    buttons.add(chars);
    buttons.add(attr);
    buttons.add(fonts);
    buttons.add(from);
    add("North", buttons);
    display.setResizeStrategy(CharDisplay.RESIZE_FONT);
    add("Center", display);
    Info();
  }
  
  public boolean handleEvent(Event evt)
  {
    if(evt.target == info) { Info(); return true; }
    if(evt.target == chars) { CharacterTable(); return true; }
    if(evt.target == attr) { Attributes(); return true; }
    if(evt.id == Event.ACTION_EVENT && 
       (evt.target == fonts || evt.target == from))
    {
      remove(display);
      display = new CharDisplay(80, 24, fonts.getSelectedItem(), 12);
      add("Center", display);
      CharacterTable();
      layout();
      return true;
    }
    return false;
  }

  private void Clear()
  {
    display.deleteArea(0, 0, 80, 24);
  }
    
  private void Info()
  {
    Clear();
    display.putString(4, 1, "CharDisplay.class Information", CharDisplay.INVERT);
    display.putString(4, 3, "Version: "+display.version, CharDisplay.BOLD);
    display.putString(4, 5, "This class implements several hardware features needed to implement");
    display.putString(4, 6, "a video terminal.");
    display.putString(4, 7, "This includes simple operations, such as putting and inserting single");
    display.putString(4, 8, "characters or strings on the screen, character attributes and colors.");
    display.putString(4, 9, "Special features like inserting lines, scrolling text up or down and");
    display.putString(4,10, "defining scrollareas help implementing terminal emulations.");
    display.redraw();
  }
    
  private void CharacterTable()
  {
    int ch = (new Integer(from.getText())).intValue();
    
    Clear();
    display.putString( 4, 1, "Character Table", CharDisplay.INVERT);
    for(int c = 1; c < 80; c += 6)
      for(int l = 3; l < 23; l++)
      {
	display.putString(c, l, ""+ch, CharDisplay.INVERT);
	display.putChar(c+4, l, (char)ch++);
      }
    display.markLine(3, 20);
    display.redraw();
  }

  private void Attributes()
  {
    int c = 4, l = 8;
    
    Clear();
    display.putString( 4, 1, "Character attributes", CharDisplay.INVERT);
    display.putString( 4, 3, "Normal", CharDisplay.NORMAL);
    display.putString(22, 3, "Bold", CharDisplay.BOLD);
    display.putString(40, 3, "Underline", CharDisplay.UNDERLINE);
    display.putString(58, 3, "Invert", CharDisplay.INVERT);

    display.putString( 4, 5, "Black", 1 << 3 | 8 << 7);
    display.putString(13, 5, "Red", 2 << 3);
    display.putString(22, 5, "Green", 3 << 3);
    display.putString(31, 5, "Yellow", 4 << 3);
    display.putString(40, 5, "Blue", 5 << 3);
    display.putString(49, 5, "Magenta", 6 << 3);
    display.putString(58, 5, "Cyan", 7 << 3);
    display.putString(67, 5, "LightGray", 8 << 3);
    
    for(int bg = 1; bg <= 8; bg++)
    {
      for(int fg = 1; fg <= 8; fg++)
      {
	for(int a = 0; a <= 7; a++)
	{
	  display.putChar(c++, l, '@', (fg << 3) | (bg << 7) | a);
	  display.redraw();
	}
	c++;
      }
      l += 2; c = 4;
    }
    
  }
}
