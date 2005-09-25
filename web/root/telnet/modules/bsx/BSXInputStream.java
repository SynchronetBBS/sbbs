/*
 * BSXInputStream	-- extends InputStream for BSX Polygons
 * --
 * $Id$
 * $timestamp: Wed Feb 14 21:36:56 1996 by Matthias L. Jugel :$
 */
package modules.bsx;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;

import java.awt.Polygon;
import java.awt.Rectangle;

public class BSXInputStream extends DataInputStream
{
  public BSXInputStream(InputStream in) { super(in); }
  
  public final BSXGraphic readBSXGraphic()	/* read a BSX graphic */
    throws IOException
    {
      int ap = readASCIIHex();
      BSXGraphic picture = new BSXGraphic(ap);
      for(; ap > 0; ap--) picture.addPolygon(readBSXPolygon());
      return picture;
    }
  
  public final BSXPolygon readBSXPolygon()	/* read a BSX polygon */
    throws IOException
    {
      BSXPolygon p = new BSXPolygon();		/* create new BSX polygon */
      int points = readASCIIHex();		/* get amount of edges */
      int color = readASCIIHex();			/* get color */
      
      p.setColor(color);
      for(; points > 0 ; points--)
	{
	  int x, y;
	  x = readASCIIHex();
	  y = readASCIIHex();
	  p.addPoint(x, y);
	}
      return p;
    }
  
  public int readASCIIHex()			/* read 2 byte ASCII hex val */
    {
      int h, l;
      try {
	h = (int)readByte();
	l = (int)readByte();
      } catch(Exception e) { return 0; }
      h = h-'0'-(h>'9'?'A'-'9'-1:0);
      l = l-'0'-(l>'9'?'A'-'9'-1:0);
      return 16*h+l;
    }
}


