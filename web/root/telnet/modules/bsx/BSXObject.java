/*
 * BSXObject	-- a simple BSX Object
 * --
 * $Id: BSXObject.java,v 1.1.1.1 2005/09/25 22:40:20 rswindell Exp $
 */
package modules.bsx;

public class BSXObject
{
  public int position = 0, layer = 0;
  public boolean visible = true;

  public BSXObject(int p, int l) { position = p; layer = l; }
}
