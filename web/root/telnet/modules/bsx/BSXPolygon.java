/*
 * BSXPolygon	-- anhanced Polygon (to meet BSX standard)
 *		   includes color for each polygon
 * --
 * $Id$
 */
package modules.bsx;

import java.awt.Polygon;
import java.awt.Color;

public class BSXPolygon extends Polygon
{
  private Color colTable[] = {
                              new Color(  0,   0,   0),
			      new Color(  0,   0, 255),
			      new Color( 34, 139,  34),
			      new Color(135, 206, 235),
			      new Color(205,  92,  92),
			      new Color(255, 105, 180),
			      new Color(165,  42,  42),
			      new Color(211, 211, 211),
			      new Color(105, 105, 105),
			      new Color(  0, 191, 255),
			      new Color(  0, 255,   0),
			      new Color(  0, 255, 255),
			      new Color(255,  99,  71),
			      new Color(255,   0, 255),
			      new Color(255, 255,   0),
			      new Color(255, 255, 255)
			     };

  private Color Pcolor = Color.black;

  public BSXPolygon() { super(); }
  public BSXPolygon(int color) { super(); Pcolor = new Color(color); }
  public BSXPolygon(int xpoints[], int ypoints[], int npoints, int color)
         {
           super(xpoints, ypoints, npoints);
           Pcolor = colTable[color];
         }

  public void setColor(int color) { Pcolor = colTable[color]; }
  public Color getColor() { return Pcolor; }
}
