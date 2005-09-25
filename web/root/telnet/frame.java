/**
 * frame -- a frame subclass for handling frame events
 * --
 * $Id$
 * $timestamp: Tue Jul  8 10:02:36 1997 by Matthias L. Jugel :$
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

import java.awt.Frame;
import java.awt.Component;
import java.awt.Event;
import java.applet.Applet;

public class frame extends Frame 
{
  public frame(String title) { super(title); }
  
  public boolean handleEvent(Event evt) {
    if(evt.target == this && evt.id == Event.WINDOW_DESTROY) {
      Component comp[] = getComponents();
      for(int i = comp.length - 1; i >= 0; i--)
        if(comp[i] instanceof Applet) {
          ((Applet)comp[i]).stop();
          this.dispose();
          return true;
        }
    }
    return false;
  }
}

    
    
