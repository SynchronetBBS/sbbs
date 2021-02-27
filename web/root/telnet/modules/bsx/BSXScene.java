/*
 * BSXScene	-- BSX Scene
 * --
 * $Id: BSXScene.java,v 1.1.1.1 2005/09/25 22:40:20 rswindell Exp $
 */
package modules.bsx;

import java.util.Hashtable;

public class BSXScene
{
  public BSXGraphic background;
  public Hashtable objects = new Hashtable();

  public BSXScene() { background = null; }
	public BSXScene(BSXGraphic pic) { background = pic; }
}
