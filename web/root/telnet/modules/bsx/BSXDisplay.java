/*
 * BSXDisplay 	-- a graphics component for drawing BSX Objects on
 * --
 * $Id$
 * $timestamp: Thu Feb 15 00:47:34 1996 by Matthias L. Jugel :$ 
 */
package modules.bsx;

import java.awt.*;
import java.util.Hashtable;
import java.util.Enumeration;
import java.util.Vector;

public class BSXDisplay extends Frame
{
private int Scale = 100;		/* display scale */
  
private Hashtable scenes;		/* scene store */
private Hashtable objects;		/* object store */
  
private String curScene = "";		/* current scene */
  
private Image imageBuffer;
private Graphics gBuffer;
public BSXDisplay() { this("BSXModule - Visit Regenbogen rb.mud.de:4780"); }
public BSXDisplay(String title) {
  super(title);
  addNotify();
  imageBuffer = createImage(511,255);
  if(imageBuffer==null) {
    System.out.println("Couldn't create an offscreen-image :(");
    System.exit(1);
    }
  gBuffer = imageBuffer.getGraphics();
  scenes = new Hashtable(); 
  objects = new Hashtable();
  setScale(100);
  resize(510,255);
  show();
}
  
public void setScale(int scale) {
  Scale = scale;
  resize(511 * Scale / 100, 255 * Scale / 100);
}
  
  /* add a scene to the store, do not yet display */
public void addScene(String id, BSXGraphic picture)
  {
    BSXScene scene = new BSXScene(picture);
    BSXScene sc = (BSXScene)scenes.get(id);
    if(sc != null)
      {
	scene.objects = sc.objects;
	scenes.remove(id);
      }
    scenes.put(id, scene);
    if(curScene.equals(id)) redraw();
  }
  
  /* show current scene */
public boolean showScene() { return showScene(curScene); }
  
  /* show scene given as argument */
public boolean showScene(String id)
  {
    curScene = id;
    if(!scenes.containsKey(id)) 
      {
	addScene(id, new BSXGraphic());
	return false;
      }
    redraw();
    return true;
  }
  
  /* add object to store */
public void addObject(String id, BSXGraphic o)
  {
    objects.put(id, o);
    BSXScene scene = (BSXScene)scenes.get(curScene);
    if(scene != null && scene.objects.containsKey(id)) redraw();
  }
  
  /* show object at position 0, layer 0 */
public boolean showObject(String id) { return showObject(id, 0, 0); }
  
  /* show object at position in layer */
public boolean showObject(String id, int position, int layer)
  {
    BSXScene scene = (BSXScene)scenes.get(curScene);
    
    /* add object to scene database */
    if((scene = (BSXScene)scenes.get(curScene)) == null) 
      {
	addScene(curScene, new BSXGraphic());
	scene = (BSXScene)scenes.get(curScene);
      }
    
    BSXObject o = new BSXObject(position, layer);
    scene.objects.put(id, o);
    
    /* check if object is in our database */
    if(!objects.containsKey(id)) return false;
    
    redraw();
    return true;
  }
  
  /* remove object from scene, if scene exists */
public boolean removeObject(String id)
  {
    BSXScene scene = (BSXScene)scenes.get(curScene);
    if(scene != null)
      {
	BSXObject o = (BSXObject)scene.objects.get(id);
	scene.objects.remove(id);
	if(o != null) 
	  redraw();
      }
    return true;
  }
  
  /* redraw image buffer */
public void redraw()
  {
    BSXScene scene = (BSXScene)scenes.get(curScene);
    if(scene == null) return;
    
    Vector[] layer = new Vector[8];
    /* create layers */
    for(Enumeration e = scene.objects.keys(); e.hasMoreElements();) 
      {
	String key; key = (String)e.nextElement();
	BSXObject o = (BSXObject)scene.objects.get(key);
	if(layer[o.layer] == null) layer[o.layer] = new Vector();
	layer[o.layer].addElement(key);
      }
    
    /* draw background graphic */
    if(scene.background != null && scene.background.size() > 0)
      drawPicture(scene.background);
    else
      return;
    
    /* display layers from back to front */
    for(int l = 7; l >= 0; l--)
      if(layer[l] != null)
	for(int o = layer[l].size()-1; o >= 0; o--)
	  {
	    BSXObject obj = (BSXObject)
	      scene.objects.get(layer[l].elementAt(o));
	    drawPicture((BSXGraphic)objects.get(layer[l].elementAt(o)),
			16 * obj.position, 4 * obj.layer);
	  }
    paint(getGraphics());
  }
  
public Point Translate(int x, int y, 
		       int centreX, int centreY, int scale)
  {
    return new Point((       x - 127 + centreX)  * 2 * scale / 100,
                     (255 - (y - 127 + centreY))     * scale / 100);
  }
  
public void drawPicture(BSXGraphic pic) { drawPicture(pic, 127, 127); }
  
public void drawPicture(BSXGraphic pic, int position, int layer)
  {
    if(pic == null) return;
    int ap = pic.size();
    for(int i = 0; i < ap; i++)
      {
	BSXPolygon poly = (BSXPolygon)pic.elementAt(i);
	Polygon p = new Polygon();
	gBuffer.setColor(poly.getColor());
	for(int j = poly.npoints - 1; j >= 0; j--)
	  {
	    Point pt = Translate(poly.xpoints[j], poly.ypoints[j], 
				 position, layer, Scale);
	    p.addPoint(pt.x, pt.y);
	  }
	gBuffer.fillPolygon(p);
      }
  }
  
public boolean mouseEnter(Event evt, int x, int y)
  {
    //System.out.println("MOUSE ENTERS");
    requestFocus();
    return true;
  }
  
  
public void update(Graphics g)
  {
    paint(g);
  }
  
  
public void paint(Graphics g)
  {
    g.drawImage(imageBuffer, 0, 0, this);
  }
}








