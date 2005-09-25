/*
 * BSXScene	-- BSX Scene
 * --
 * $Id$
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
